// run.cpp -- `winapk run`: runs one APK's Java code in this process (PLAN.md §8, G1).
//
//   winapk run <apk> [--native=off|restricted] [--graphics=off|auto|vulkan|d3d11] [...]
//
// Steps, each logged as "[winapk] stage …": inspect the APK (no code runs), convert its DEX
// to classes.jar with com.macapk.dex in a child `winapk dex` (cached by content), create the JVM here
// with upstream's options, register winapk-bridge.dll's natives and call
// com.macapk.loader.Boot.run on this thread, which becomes the app's UI thread.
//
// Data: <data-dir> (default %LOCALAPPDATA%\Windows-ARM64.apk\Apps) holds android\ (the
// app-visible Android root: data\data\<package>, sdcard, ...), code\<package>\ (the APK's
// converted code and its cache stamp) and logs\. Host-domain code throughout.
//
// Paths: winapk.exe runs with the UTF-8 active code page (winapk.manifest), so the JVM option
// strings, which HotSpot reads in the ANSI code page, carry any Unicode path; arguments reach
// Java code only as UTF-16 strings.
#include "apk.h"
#include "doctor.h"
#include "run.h"

#include <windows.h>
#include <bcrypt.h>
#include <jni.h>
#include <shellscalingapi.h>
#include <shlobj.h>

#include <cmath>
#include <algorithm>
#include <cstdio>
#include <fstream>
#include <memory>
#include <set>
#include <sstream>
#include <vector>

namespace winapk {
namespace {

/// Logs one runtime line to stderr.
void Log(const std::string& s) {
    std::fprintf(stderr, "[winapk] %s\n", s.c_str());
    std::fflush(stderr);
}

bool Exists(const std::wstring& p) { return GetFileAttributesW(p.c_str()) != INVALID_FILE_ATTRIBUTES; }

/// Returns the SHA-256, as lowercase hex, of what `feed` passes to its argument, "" on failure.
/// Inputs: `feed` calls `add(bytes, n)` for each piece and returns false on a read error.
template <typename Feed>
std::string Sha256(Feed feed) {
    BCRYPT_HASH_HANDLE h = nullptr;
    std::string hex;
    if (!BCRYPT_SUCCESS(BCryptCreateHash(BCRYPT_SHA256_ALG_HANDLE, &h, nullptr, 0, nullptr, 0, 0))) return hex;
    bool ok = true;
    const bool fed = feed([&](const void* p, ULONG n) { ok = ok && BCRYPT_SUCCESS(BCryptHashData(h, (PUCHAR)p, n, 0)); });
    uint8_t digest[32];
    if (ok && fed && BCRYPT_SUCCESS(BCryptFinishHash(h, digest, sizeof digest, 0))) {
        static const char* kHex = "0123456789abcdef";
        for (uint8_t b : digest) { hex += kHex[b >> 4]; hex += kHex[b & 15]; }
    }
    BCryptDestroyHash(h);
    return hex;
}

/// Returns the SHA-256 of a file as lowercase hex, "" when it cannot be read.
std::string Sha256File(const std::wstring& path) {
    HANDLE f = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
    if (f == INVALID_HANDLE_VALUE) return {};
    const std::string hex = Sha256([&](auto add) {
        std::vector<uint8_t> buf(1 << 20);
        DWORD n = 0;
        while (ReadFile(f, buf.data(), static_cast<DWORD>(buf.size()), &n, nullptr)) {
            if (n == 0) return true;
            add(buf.data(), n);
        }
        return false;
    });
    CloseHandle(f);
    return hex;
}

/// The build identity of the converted code: the converter and framework jars' digests.
struct BuildId {
    std::string converter, framework;
    /// Returns upstream's MACAPK_BUILD_JAVA_ID: 16 hex digits of a digest of both jars.
    /// Notes: upstream's build_ids.py narrows the basis to the converter's bytecode and the
    /// framework's class shapes, so a framework method-body edit keeps old caches; this uses
    /// both jars whole (upstream's former basis, its JARS_ID), which re-converts more often but
    /// never keeps a stale jar (ADR-0003).
    std::string JavaId() const {
        const std::string s = "converter=" + converter + "\nframework=" + framework + "\n";
        return Sha256([&](auto add) { add(s.data(), static_cast<ULONG>(s.size())); return true; }).substr(0, 16);
    }
};

/// Quotes one argument for a Windows command line (CommandLineToArgvW rules).
std::wstring Quote(const std::wstring& a) {
    std::wstring q = L"\"";
    size_t slashes = 0;
    for (wchar_t c : a) {
        if (c == L'\\') { ++slashes; continue; }
        if (c == L'"') q.append(slashes * 2 + 1, L'\\');
        else q.append(slashes, L'\\');
        slashes = 0;
        q += c;
    }
    q.append(slashes * 2, L'\\');
    return q + L"\"";
}

/// Starts the DEX converter inside a kill-on-close job and waits for its exit.
/// Notes: assigns the job atomically at creation; the non-inherited job handle belongs to the launcher.
/// Returns: the child exit code, or -1 on a Win32 launch or wait failure.
int Spawn(const std::wstring& exe, const std::vector<std::wstring>& args) {
    const auto failed = [](const char* operation, DWORD error) {
        Log(std::string("DEX_CONVERSION_FAILED: ") + operation + " win32=" + std::to_string(error));
        return -1;
    };
    std::wstring cmd = Quote(exe);
    for (const auto& a : args) cmd += L" " + Quote(a);

    const std::unique_ptr<void, decltype(&CloseHandle)> job(CreateJobObjectW(nullptr, nullptr), CloseHandle);
    if (!job) return failed("CreateJobObjectW", GetLastError());
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
    limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    if (!SetInformationJobObject(job.get(), JobObjectExtendedLimitInformation, &limits, sizeof limits))
        return failed("SetInformationJobObject", GetLastError());

    SIZE_T size = 0;
    InitializeProcThreadAttributeList(nullptr, 1, 0, &size);
    if (!size) return failed("InitializeProcThreadAttributeList (size)", GetLastError());
    std::vector<unsigned char> storage(size);
    STARTUPINFOEXW si{};
    si.StartupInfo.cb = sizeof si;
    si.lpAttributeList = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(storage.data());
    if (!InitializeProcThreadAttributeList(si.lpAttributeList, 1, 0, &size))
        return failed("InitializeProcThreadAttributeList", GetLastError());
    HANDLE childJob = job.get();
    if (!UpdateProcThreadAttribute(si.lpAttributeList, 0, PROC_THREAD_ATTRIBUTE_JOB_LIST,
                                  &childJob, sizeof childJob, nullptr, nullptr)) {
        const auto error = GetLastError();
        DeleteProcThreadAttributeList(si.lpAttributeList);
        return failed("UpdateProcThreadAttribute (job list)", error);
    }

    PROCESS_INFORMATION pi{};
    const auto created = CreateProcessW(exe.c_str(), cmd.data(), nullptr, nullptr, TRUE,
                                       EXTENDED_STARTUPINFO_PRESENT, nullptr, nullptr, &si.StartupInfo, &pi);
    const auto creationError = created ? ERROR_SUCCESS : GetLastError();
    DeleteProcThreadAttributeList(si.lpAttributeList);
    if (!created) return failed("CreateProcessW (DEX job)", creationError);

    CloseHandle(pi.hThread);
    const std::unique_ptr<void, decltype(&CloseHandle)> process(pi.hProcess, CloseHandle);
    Log("stage dex-child pid=" + std::to_string(pi.dwProcessId));
    if (WaitForSingleObject(process.get(), INFINITE) != WAIT_OBJECT_0)
        return failed("WaitForSingleObject (DEX child)", GetLastError());
    DWORD code = 0;
    if (!GetExitCodeProcess(process.get(), &code)) return failed("GetExitCodeProcess (DEX child)", GetLastError());
    return static_cast<int>(code);
}

/// Writes `text` to `path` through a temporary file and an atomic replace (PLAN.md §8.5).
bool WriteAtomic(const std::wstring& path, const std::string& text) {
    const std::wstring tmp = path + L".tmp" + std::to_wstring(GetCurrentProcessId());
    {
        std::ofstream o(tmp, std::ios::binary | std::ios::trunc);
        if (!(o << text)) return false;
    }
    return MoveFileExW(tmp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
}

/// Extracts a bounded ARM64 APK library set by content identity, never by arbitrary ZIP paths.
/// Returns: a stable category on unsafe names, ABI-only mismatch, corrupt input or write failure.
std::string StageLibraries(const RunArgs& a, const ApkInfo& apk, const std::wstring& appDir,
                           const std::wstring& androidRoot, std::wstring& directory, std::string& admitted) {
    directory = appDir + L"\\.macapk-native-" + Wide(Sha256File(a.apk));
    std::vector<ZipEntry> entries;
    if (auto error = ReadZip(a.apk, entries); !error.empty()) return "INSTALL_FAILED_INVALID_APK: " + error;
    std::vector<ZipEntry> libraries;
    std::set<std::string> names;
    uint64_t total = 0;
    constexpr std::string_view prefix = "lib/arm64-v8a/";
    for (const auto& entry : entries) {
        if (!entry.name.starts_with(prefix) || entry.name.ends_with('/')) continue;
        const std::string name = entry.name.substr(prefix.size());
        if (name.empty() || name.size() > 128 || !name.ends_with(".so") || name.find("..") != std::string::npos)
            return "GUEST_ABI_UNSUPPORTED: portable ARM64 .so basename required";
        std::string folded = name;
        for (char& c : folded) {
            if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                  (c >= '0' && c <= '9') || c == '_' || c == '+' || c == '.' || c == '-'))
                return "GUEST_ABI_UNSUPPORTED: portable ARM64 .so basename required";
            if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
        }
        if (!names.insert(folded).second ||
            std::find(apk.unsafeNames.begin(), apk.unsafeNames.end(), entry.name) != apk.unsafeNames.end())
            return "INSTALL_FAILED_INVALID_APK: unsafe or colliding ARM64 library entry";
        if (entry.size > (64ull << 20) || (total += entry.size) > (256ull << 20) || libraries.size() == 128)
            return "GUEST_ABI_UNSUPPORTED: APK library size/count limit";
        libraries.push_back(entry);
    }
    if (!apk.nativeLibs.empty() && libraries.empty())
        return "GUEST_ABI_UNSUPPORTED: APK has no arm64-v8a libraries";
    const int rc = SHCreateDirectoryExW(nullptr, directory.c_str(), nullptr);
    if (rc != ERROR_SUCCESS && rc != ERROR_ALREADY_EXISTS) return "IO_ERROR: cannot create APK library directory";
    const std::wstring appLibDir = androidRoot + L"\\data\\data\\" + Wide(apk.packageName) + L"\\app_lib";
    SHCreateDirectoryExW(nullptr, appLibDir.c_str(), nullptr);
    for (const auto& entry : libraries) {
        std::vector<uint8_t> bytes;
        if (auto error = ReadEntry(a.apk, entry, bytes); !error.empty()) return "INSTALL_FAILED_INVALID_APK: " + error;
        const std::wstring baseW = Wide(entry.name.substr(prefix.size()));
        const std::string blob(bytes.begin(), bytes.end());
        if (!WriteAtomic(directory + L"\\" + baseW, blob))
            return "IO_ERROR: cannot write APK library";
        WriteAtomic(appLibDir + L"\\" + baseW, blob);
        admitted += entry.name.substr(prefix.size()) + "\n";
    }
    Log("stage guest-libraries (" + std::to_string(libraries.size()) + " ARM64 candidates; screened on load)");
    return {};
}

std::string ReadText(const std::wstring& path) {
    std::ifstream i(path, std::ios::binary);
    std::stringstream s;
    s << i.rdbuf();
    return s.str();
}

/// Says whether a manifest package name is usable as a folder name: Java package syntax.
bool SafePackage(const std::string& p) {
    if (p.empty() || p.size() > 200 || p.front() == '.' || p.back() == '.') return false;
    for (char c : p) if (!(isalnum(static_cast<unsigned char>(c)) || c == '.' || c == '_')) return false;
    return p.find("..") == std::string::npos;
}

/// Returns this executable's path.
std::wstring ExePath() {
    std::wstring self(32768, L'\0');
    self.resize(GetModuleFileNameW(nullptr, self.data(), static_cast<DWORD>(self.size())));
    return self;
}

/// Returns this executable's folder.
std::wstring ExeDir() {
    const std::wstring self = ExePath();
    return self.substr(0, self.find_last_of(L'\\'));
}

/// Returns the parent of a folder.
std::wstring Parent(const std::wstring& p) { return p.substr(0, p.find_last_of(L'\\')); }

/// Converts the APK's DEX to `appDir`\classes.jar unless the cache already holds it.
/// Returns: 0 when classes.jar is ready (or the APK has no DEX), else an exit code.
int StageCode(const RunArgs& a, const JvmInfo& jvm, const BuildId& id, const std::wstring& appDir, bool& hasCode) {
    // The stamp's name starts with .macapk so Boot's code-dir migration keeps it here
    // (Boot.isCodeDirArtifact); upstream's own stamp is .source-mtime.
    const std::wstring jar = appDir + L"\\classes.jar", stampPath = appDir + L"\\.macapk-winapk-stamp";
    // The cache key: the APK's content, the converter and framework builds, the platform.
    const std::string stamp = "apk=" + Sha256File(a.apk) + "\nconverter=" + id.converter + "\nframework=" + id.framework +
                              "\nplatform=windows-arm64\n";
    const std::string old = ReadText(stampPath);
    if (old == stamp + "dex=none\n") { hasCode = false; Log("stage dex-converted (cached: no DEX)"); return 0; }
    if (old == stamp && Exists(jar)) { hasCode = true; Log("stage dex-converted (cached)"); return 0; }

    const std::wstring tmp = appDir + L"\\classes.jar.tmp" + std::to_wstring(GetCurrentProcessId());
    Log("converting DEX to JVM bytecode (com.macapk.dex)");
    const ULONGLONG t0 = GetTickCount64();
    const int rc = Spawn(ExePath(), {L"dex", a.apk, tmp, L"--java-home", Wide(jvm.home)});
    if (rc == 3) {  // com.macapk.dex: no DEX in the input
        DeleteFileW(tmp.c_str());
        DeleteFileW(jar.c_str());
        hasCode = false;
        WriteAtomic(stampPath, stamp + "dex=none\n");
        Log("stage dex-converted (the APK has no DEX)");
        return 0;
    }
    if (rc != 0 || !Exists(tmp)) {
        DeleteFileW(tmp.c_str());
        if (rc == 69) { Log("DEPENDENCY_MISSING: the converter could not start (see above)"); return 69; }
        Log(rc == 1 ? "INSTALL_FAILED_INVALID_APK: the APK's DEX cannot be parsed (see the converter's output above)"
                    : "DEX_CONVERSION_FAILED: the converter exited with " + std::to_string(rc));
        return rc == 1 ? 65 : 70;
    }
    if (!MoveFileExW(tmp.c_str(), jar.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) || !WriteAtomic(stampPath, stamp)) {
        Log("IO_ERROR: cannot store classes.jar in " + Utf8(appDir));
        return 74;
    }
    hasCode = true;
    Log("stage dex-converted (" + std::to_string(GetTickCount64() - t0) + " ms)");
    return 0;
}

/// Returns the scale of the primary display, where the app's window opens: 1.0 at 96 DPI.
/// Notes: the display's current DPI (the window's), not GetDpiForSystem, which keeps the
/// scale of sign-in until the person signs out.
float DisplayScale() {
    UINT x = 96, y = 96;
    if (FAILED(GetDpiForMonitor(MonitorFromPoint(POINT{0, 0}, MONITOR_DEFAULTTOPRIMARY), MDT_EFFECTIVE_DPI, &x, &y))) return 1.0f;
    return x / 96.0f;
}

/// Picks the app's size in points: the request, else a phone shape that fits the screen.
void PhoneSize(const RunArgs& a, int& w, int& h) {
    if (a.width > 0 && a.height > 0) { w = a.width; h = a.height; return; }
    RECT work{};
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0);
    const float s = DisplayScale();
    const int room = static_cast<int>((work.bottom - work.top) / s * 0.85f);
    h = room < 800 ? room : 800;
    w = h / 2;
}

}  // namespace

int Inspect(const std::wstring& path, bool json) {
    ApkInfo i;
    const std::string err = InspectApk(path, i);
    auto q = [](const std::string& s) {
        std::string o = "\"";
        for (unsigned char c : s) {
            if (c == '"' || c == '\\') { o += '\\'; o += static_cast<char>(c); }
            else if (c < 0x20) { char b[8]; std::snprintf(b, sizeof b, "\\u%04x", c); o += b; }
            else o += static_cast<char>(c);
        }
        return o + "\"";
    };
    auto list = [&](const std::vector<std::string>& v) {
        std::string o = "[";
        for (size_t k = 0; k < v.size(); ++k) o += (k ? ", " : "") + q(v[k]);
        return o + "]";
    };
    auto b64 = [](const std::vector<uint8_t>& b) {
        static constexpr char kTab[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        std::string o;
        o.reserve(((b.size() + 2) / 3) * 4);
        for (size_t k = 0; k < b.size(); k += 3) {
            const uint32_t v = (uint32_t(b[k]) << 16) |
                               (k + 1 < b.size() ? uint32_t(b[k + 1]) << 8 : 0) |
                               (k + 2 < b.size() ? uint32_t(b[k + 2]) : 0);
            o += kTab[(v >> 18) & 63];
            o += kTab[(v >> 12) & 63];
            o += k + 1 < b.size() ? kTab[(v >> 6) & 63] : '=';
            o += k + 2 < b.size() ? kTab[v & 63] : '=';
        }
        return o;
    };
    if (json) {
        std::printf("{\n  \"schemaVersion\": 1,\n  \"ok\": %s,\n  \"error\": %s,\n  \"package\": %s,\n  \"label\": %s,\n"
                    "  \"versionName\": %s,\n  \"versionCode\": %lld,\n  \"size\": %llu,\n  \"minSdk\": %d,\n  \"targetSdk\": %d,\n"
                    "  \"dexCount\": %d,\n  \"glesVersion\": %s,\n  \"vulkan\": %s,\n  \"profile\": %s,\n  \"verdict\": %s,\n"
                    "  \"permissions\": %s,\n  \"nativeAbis\": %s,\n  \"nativeLibs\": %s,\n  \"iconPng\": %s,\n  \"unsafeEntryNames\": %s\n}\n",
                    err.empty() ? "true" : "false", q(err).c_str(), q(i.packageName).c_str(), q(i.label).c_str(),
                    q(i.versionName).c_str(), static_cast<long long>(i.versionCode), static_cast<unsigned long long>(i.fileSize),
                    i.minSdk, i.targetSdk, i.dexCount, q(i.glesVersion).c_str(), i.vulkan ? "true" : "false",
                    q(i.profile).c_str(), q(i.verdict).c_str(), list(i.permissions).c_str(), list(i.nativeAbis).c_str(),
                    list(i.nativeLibs).c_str(), q(b64(i.iconPng)).c_str(), list(i.unsafeNames).c_str());
    } else if (err.empty()) {
        std::printf("%s (%s) %s (%lld), %llu bytes, minSdk %d, targetSdk %d, %d DEX, %zu native libraries, verdict %s%s\n",
                    i.label.c_str(), i.packageName.c_str(), i.versionName.c_str(), static_cast<long long>(i.versionCode),
                    static_cast<unsigned long long>(i.fileSize), i.minSdk, i.targetSdk, i.dexCount,
                    i.nativeLibs.size(), i.verdict.c_str(), i.unsafeNames.empty() ? "" : ", entry names unsafe on Windows");
    } else {
        std::fprintf(stderr, "not a readable APK: %s\n", err.c_str());
    }
    return err.empty() ? 0 : 65;
}

int ConvertDex(const std::wstring& apk, const std::wstring& out, const std::wstring& javaHome) {
    const JvmInfo jvm = FindJvm(javaHome);
    const std::wstring lib = Parent(ExeDir()) + L"\\lib";
    if (jvm.home.empty() || jvm.arch != "arm64" || !Exists(lib + L"\\macapk-loader.jar")) {
        Log("DEPENDENCY_MISSING: the DEX converter needs an arm64 JVM and " + Utf8(lib) + "\\macapk-loader.jar");
        return 69;
    }
    const std::string L = Utf8(lib);
    std::string err;
    JavaVM* vm = nullptr;
    // java.exe's options for the converter before: the throughput collector, the framework
    // as the converter's type universe (ClassHierarchyOracle).
    JNIEnv* env = CreateJvm(Wide(jvm.home), {"-XX:+UseParallelGC",
        "-Ddex2jvm.redirectNativeLoad=com/macapk/loader/GuestNativeLoader",
        "-Djava.class.path=" + L + "\\android-framework.jar;" + L + "\\macapk-loader.jar;" + L + "\\bcprov-jdk18on-1.79.jar"}, &vm, err);
    if (!env) { Log("DEPENDENCY_MISSING: " + err); return 69; }
    jclass main = env->FindClass("com/macapk/dex/Main");
    jmethodID mid = main ? env->GetStaticMethodID(main, "main", "([Ljava/lang/String;)V") : nullptr;
    if (!mid) { env->ExceptionDescribe(); Log("DEPENDENCY_MISSING: com.macapk.dex.Main not found"); return 69; }
    auto js = [&](const std::wstring& s) { return env->NewString(reinterpret_cast<const jchar*>(s.data()), static_cast<jsize>(s.size())); };
    jobjectArray args = env->NewObjectArray(2, env->FindClass("java/lang/String"), nullptr);
    env->SetObjectArrayElement(args, 0, js(apk));
    env->SetObjectArrayElement(args, 1, js(out));
    // Main ends with System.exit on every outcome but success, which ends this process.
    env->CallStaticVoidMethod(main, mid, args);
    int code = 0;
    if (env->ExceptionCheck()) { env->ExceptionDescribe(); env->ExceptionClear(); code = 4; }
    vm->DestroyJavaVM();
    return code;
}

int Run(const RunArgs& a) {
    if (a.native != L"off" && a.native != L"restricted") {
        Log("GUEST_ABI_UNSUPPORTED: --native must be off or restricted");
        return 64;
    }
    std::wstring profile = a.profile;
    if (profile == L"n0") profile = L"N0";
    else if (profile == L"n1") profile = L"N1";
    else if (profile == L"n2") profile = L"N2";
    if (profile != L"N0" && profile != L"N1" && profile != L"N2") {
        Log("GUEST_ABI_UNSUPPORTED: --profile must be N0, N1 or N2");
        return 64;
    }
    if (a.graphics != L"off" && a.graphics != L"auto" && a.graphics != L"vulkan" && a.graphics != L"d3d11") {
        Log("GRAPHICS_CAPABILITY_MISSING: --graphics must be off, auto, vulkan or d3d11");
        return 64;
    }
    Log("stage inspect " + Utf8(a.apk));
    ApkInfo apk;
    if (std::string err = InspectApk(a.apk, apk); !err.empty()) {
        Log("INSTALL_FAILED_INVALID_APK: " + err);
        return 65;
    }
    if (!SafePackage(apk.packageName)) {
        Log("INSTALL_FAILED_INVALID_APK: package name \"" + apk.packageName + "\" is not a valid Java package name");
        return 65;
    }
    Log("package " + apk.packageName + " " + apk.versionName + " (" + std::to_string(apk.versionCode) + "), " +
        std::to_string(apk.dexCount) + " DEX, minSdk " + std::to_string(apk.minSdk) + ", targetSdk " + std::to_string(apk.targetSdk));
    if (a.native == L"off" && !apk.nativeLibs.empty()) {
        std::string abis;
        for (const auto& x : apk.nativeAbis) abis += (abis.empty() ? "" : ", ") + x;
        Log("guest native is off: the APK's " + std::to_string(apk.nativeLibs.size()) + " native libraries (" + abis +
            ") will not load; System.loadLibrary fails with GUEST_NATIVE_DISABLED");
    }

    JvmInfo jvm = FindJvm(a.javaHome);
    if (jvm.home.empty() || jvm.arch != "arm64") {
        Log("DEPENDENCY_MISSING: " + (jvm.home.empty() ? std::string("no JVM found (Windows-ARM64.apk's own runtime, WINAPK_JAVA_HOME or JAVA_HOME)")
                                                       : "jvm.dll at " + jvm.home + " is " + jvm.arch + ", not arm64"));
        return 69;
    }
    const std::wstring bin = ExeDir(), lib = Parent(bin) + L"\\lib", fwres = bin + L"\\tools\\framework-res.apk";
    for (const std::wstring& need : {lib + L"\\android-framework.jar", lib + L"\\macapk-loader.jar", fwres, bin + L"\\winapk-bridge.dll"}) {
        if (!Exists(need)) { Log("DEPENDENCY_MISSING: " + Utf8(need)); return 69; }
    }

    std::wstring root = a.dataDir;
    if (root.empty()) {
        PWSTR local = nullptr;
        if (FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &local))) { Log("IO_ERROR: no LocalAppData folder"); return 74; }
        root = std::wstring(local) + L"\\Windows-ARM64.apk\\Apps";
        CoTaskMemFree(local);
    }
    const std::wstring android = root + L"\\android", appDir = root + L"\\code\\" + Wide(apk.packageName), logs = root + L"\\logs";
    for (const std::wstring& d : {android, appDir, logs}) {
        const int rc = SHCreateDirectoryExW(nullptr, d.c_str(), nullptr);
        if (rc != ERROR_SUCCESS && rc != ERROR_ALREADY_EXISTS) { Log("IO_ERROR: cannot create " + Utf8(d)); return 74; }
    }

    const BuildId id{Sha256File(lib + L"\\macapk-loader.jar"), Sha256File(lib + L"\\android-framework.jar")};
    bool hasCode = false;
    std::wstring guestLibraries;
    std::string admitted;
    if (a.native == L"restricted") {
        if (profile != L"N0") {
            for (const auto& name : {L"libwinapk-bionic.so", L"libwinapk-jni.so"}) {
                if (!Exists(bin + L"\\guest\\" + name)) {
                    Log("DEPENDENCY_MISSING: guest JNI/Bionic helper");
                    return 69;
                }
            }
            if (profile == L"N2" && !Exists(bin + L"\\guest\\libwinapk-bionic-n2.so")) {
                Log("DEPENDENCY_MISSING: guest N2 Bionic helper");
                return 69;
            }
        }
        if (auto error = StageLibraries(a, apk, appDir, android, guestLibraries, admitted); !error.empty()) {
            Log(error);
            return error.starts_with("IO_ERROR") ? 74 : 65;
        }
    }
    if (int rc = StageCode(a, jvm, id, appDir, hasCode)) return rc;

    // The bridge sets the app's size before Java asks for it; load only from bin + System32, never PATH or CWD.
    HMODULE bridge = LoadLibraryExW((bin + L"\\winapk-bridge.dll").c_str(), nullptr, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);
    auto reg = bridge ? reinterpret_cast<jboolean (*)(JNIEnv*)>(GetProcAddress(bridge, "winapk_bridge_register")) : nullptr;
    auto setSize = bridge ? reinterpret_cast<void (*)(int, int)>(GetProcAddress(bridge, "winapk_bridge_set_phone_size")) : nullptr;
    if (!reg || !setSize) { Log("DEPENDENCY_MISSING: winapk-bridge.dll is missing its entry points"); return 69; }
    if (!a.embed.empty()) {
        auto connectEmbed = reinterpret_cast<jboolean (*)(const wchar_t*)>(GetProcAddress(bridge, "winapk_bridge_connect_embed"));
        if (!connectEmbed || !connectEmbed(a.embed.c_str())) {
            Log("IO_ERROR: cannot connect to front-end embed channel");
            return 74;
        }
    }
    if (a.native == L"restricted") {
        auto configure = reinterpret_cast<void (*)(const wchar_t*, const wchar_t*, const wchar_t*, const char*, const char*)>(
            GetProcAddress(bridge, "winapk_bridge_configure_guest"));
        if (!configure) { Log("DEPENDENCY_MISSING: guest configuration entry"); return 69; }
        configure(guestLibraries.c_str(), (bin + L"\\guest").c_str(), android.c_str(), admitted.c_str(), Utf8(profile).c_str());
    }
    int w = 0, h = 0;
    PhoneSize(a, w, h);
    setSize(w, h);
    const float scale = DisplayScale();
    const int densityDpi = static_cast<int>(std::lround(160 * scale));  // a point (DIP) is a dp
    Log("window " + std::to_string(w) + "x" + std::to_string(h) + " points, scale " + std::to_string(scale) + ", density " + std::to_string(densityDpi) + " dpi");

    // This process only. MACAPK_BUILD_JAVA_ID keys the app's own dynamic-DEX cache
    // (dalvik.system.MacApkDexConverter), as upstream's main.cpp exports it from build-id.txt.
    SetEnvironmentVariableW(L"MACAPK_FW_RES_APK", fwres.c_str());
    SetEnvironmentVariableW(L"WINAPK_APK_PATH", a.apk.c_str());
    SetEnvironmentVariableW(L"MACAPK_BUILD_JAVA_ID", Wide(id.JavaId()).c_str());
    SetEnvironmentVariableW(L"WINAPK_GRAPHICS_MODE", a.graphics.c_str());
    SetEnvironmentVariableW(L"WINAPK_APK_REQUIRES_VULKAN", apk.vulkanRequired ? L"1" : L"0");
    const std::string L = Utf8(lib);
    std::vector<std::string> opts = {
        "-Djava.class.path=" + L + "\\android-framework.jar;" + L + "\\macapk-loader.jar;" + L + "\\bcprov-jdk18on-1.79.jar",
        "--patch-module=java.base=" + L + "\\java_base_patch",
        "--patch-module=jdk.unsupported=" + L + "\\jdk_unsupported_patch",
        // upstream's jvm_loader.cpp, less what is macOS-only (its hosts-file path, AOT cache)
        "-Xrs", "-Djava.library.path=", "-XX:+UseSerialGC", "-XX:+TieredCompilation",
        "-Xms128m", "-Xmx1g", "-XX:NewSize=64m", "-XX:MaxNewSize=512m", "-XX:MinHeapFreeRatio=40", "-XX:MaxHeapFreeRatio=70",
        "-Xss16m", "-XX:+UnlockDiagnosticVMOptions", "-XX:-BytecodeVerificationRemote", "-XX:-BytecodeVerificationLocal",
        "--enable-native-access=ALL-UNNAMED",
        "--add-opens=java.base/java.io=ALL-UNNAMED", "--add-opens=java.base/java.util=ALL-UNNAMED",
        "--add-opens=java.base/java.lang=ALL-UNNAMED", "--add-opens=java.base/jdk.internal.loader=ALL-UNNAMED",
        "--add-exports=java.base/sun.net.www.protocol.http=ALL-UNNAMED", "--add-exports=java.base/sun.net.www.protocol.https=ALL-UNNAMED",
        "-Djava.awt.headless=true",
        "-XX:ErrorFile=" + Utf8(logs) + "\\hs_err_pid%p.log",
        "-Dmacapk.android.root=" + Utf8(android),
        std::string("-Dmacapk.runtime.version=") + WINAPK_VERSION,
        "-Dmacapk.display.density=" + std::to_string(densityDpi),
        "-Dmacapk.bundle.apk=" + Utf8(a.apk),
        "-Ddex2jvm.redirectNativeLoad=com/macapk/loader/GuestNativeLoader",
        "-Dwinapk.guestNative=" + Utf8(a.native),
        "-Dwinapk.graphics=" + Utf8(a.graphics),
        "-Dwinapk.guestLibraryDir=" + Utf8(guestLibraries),
    };
    std::string err;
    JavaVM* vm = nullptr;
    JNIEnv* env = CreateJvm(Wide(jvm.home), opts, &vm, err);
    if (!env) { Log("DEPENDENCY_MISSING: " + err); return 69; }
    Log("stage jvm-created (" + jvm.vendor + " " + jvm.version + ", " + jvm.home + ")");
    if (!reg(env)) Log("some host natives did not bind (see above)");

    jclass boot = env->FindClass("com/macapk/loader/Boot");
    jmethodID run = boot ? env->GetStaticMethodID(boot, "run", "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)V") : nullptr;
    if (!run) { env->ExceptionDescribe(); Log("DEPENDENCY_MISSING: com.macapk.loader.Boot.run not found"); return 69; }
    auto js = [&](const std::wstring& s) { return env->NewString(reinterpret_cast<const jchar*>(s.data()), static_cast<jsize>(s.size())); };
    Log("stage boot");
    env->CallStaticVoidMethod(boot, run, js(a.apk), js(hasCode ? appDir + L"\\classes.jar" : L""), js(appDir), js(L""));
    int code = 0;
    if (env->ExceptionCheck()) {
        env->ExceptionDescribe();
        env->ExceptionClear();
        Log("APP_CRASHED: Boot.run threw");
        code = 1;
    }
    Log("stage exit");
    vm->DestroyJavaVM();
    return code;
}

}  // namespace winapk
