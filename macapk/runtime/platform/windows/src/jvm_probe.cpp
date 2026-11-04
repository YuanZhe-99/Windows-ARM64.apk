// jvm_probe.cpp -- finds the JVM the runtime would embed and starts it once (PLAN.md §7, T01).
//
// Host-domain code: the JVM, jvm.dll and every JNI type here are Windows ARM64 host objects.
#include "doctor.h"

#include <windows.h>
#include <jni.h>

#include <fstream>
#include <sstream>

namespace winapk {

namespace {

/// Returns an environment variable, "" when unset.
std::wstring Env(const wchar_t* name) {
    wchar_t buf[MAX_PATH * 2];
    DWORD n = GetEnvironmentVariableW(name, buf, ARRAYSIZE(buf));
    return n > 0 && n < ARRAYSIZE(buf) ? std::wstring(buf, n) : std::wstring();
}

bool Exists(const std::wstring& p) { return GetFileAttributesW(p.c_str()) != INVALID_FILE_ATTRIBUTES; }

std::wstring Trim(std::wstring p) {
    while (!p.empty() && (p.back() == L'\\' || p.back() == L'/')) p.pop_back();
    return p;
}

/// Reads KEY="value" lines of a JDK's release file.
std::string ReleaseValue(const std::wstring& home, const std::string& key) {
    std::ifstream f(home + L"\\release");
    std::string line;
    while (std::getline(f, line)) {
        if (line.rfind(key + "=", 0) == 0) {
            std::string v = line.substr(key.size() + 1);
            if (v.size() >= 2 && v.front() == '"') v = v.substr(1, v.size() - 2);
            return v;
        }
    }
    return {};
}

/// HostProbe.hostEcho: proves a Java → host JNI call with a checked value.
jint JNICALL HostEcho(JNIEnv*, jclass, jint value) { return value * 2 + 1; }

/// The JVM options doctor starts with: upstream's module openings
/// (macapk/runtime/src/jvm_loader.cpp), so their acceptance by this JDK is checked too.
std::vector<std::string> JvmOptions(const std::wstring& probeJar) {
    return {
        "-Djava.class.path=" + Utf8(probeJar),
        "-Xrs",
        "-Djava.awt.headless=true",
        "-Xss16m",
        "--enable-native-access=ALL-UNNAMED",
        "--add-opens=java.base/java.io=ALL-UNNAMED",
        "--add-opens=java.base/java.util=ALL-UNNAMED",
        "--add-opens=java.base/java.lang=ALL-UNNAMED",
        "--add-opens=java.base/jdk.internal.loader=ALL-UNNAMED",
        "--add-exports=java.base/sun.net.www.protocol.http=ALL-UNNAMED",
        "--add-exports=java.base/sun.net.www.protocol.https=ALL-UNNAMED",
    };
}

/// Creates the JVM from jvm.dll, registers HostProbe's native and runs it.
/// Returns: "ok" or "failed: <reason>"; findings go to probe.
std::string StartJvm(const std::wstring& home, const std::wstring& probeJar,
                     std::vector<std::pair<std::string, std::string>>& probe) {
    if (!Exists(probeJar)) return "failed: " + Utf8(probeJar) + " is missing (build the Host component)";
    std::string err;
    JavaVM* vm = nullptr;
    JNIEnv* env = CreateJvm(home, JvmOptions(probeJar), &vm, err);
    if (!env) return "failed: " + err;
    std::string result = "ok";
    jclass cls = env->FindClass("com/winapk/probe/HostProbe");
    JNINativeMethod natives[] = {{const_cast<char*>("hostEcho"), const_cast<char*>("(I)I"), reinterpret_cast<void*>(&HostEcho)}};
    jmethodID run = cls ? env->GetStaticMethodID(cls, "run", "()Ljava/lang/String;") : nullptr;
    if (!cls || !run || env->RegisterNatives(cls, natives, 1) != JNI_OK) {
        env->ExceptionClear();
        result = "failed: HostProbe not found in " + Utf8(probeJar);
    } else if (auto s = static_cast<jstring>(env->CallStaticObjectMethod(cls, run)); env->ExceptionCheck() || !s) {
        env->ExceptionDescribe();
        env->ExceptionClear();
        result = "failed: HostProbe.run threw";
    } else {
        const char* chars = env->GetStringUTFChars(s, nullptr);
        std::istringstream lines(chars);
        env->ReleaseStringUTFChars(s, chars);
        std::string line;
        while (std::getline(lines, line)) {
            size_t eq = line.find('=');
            if (eq != std::string::npos) probe.emplace_back(line.substr(0, eq), line.substr(eq + 1));
        }
    }
    vm->DestroyJavaVM();
    return result;
}

}  // namespace

JvmInfo FindJvm(const std::wstring& javaHome) {
    std::wstring own = Env(L"LOCALAPPDATA") + L"\\Windows-ARM64.apk\\Runtime";
    const std::pair<std::wstring, const char*> candidates[] = {
        {javaHome, "argument"},
        {own, "own"},
        {Env(L"WINAPK_JAVA_HOME"), "WINAPK_JAVA_HOME"},
        {Env(L"JAVA_HOME"), "JAVA_HOME"},
    };
    JvmInfo j;
    for (const auto& [dir, source] : candidates) {
        std::wstring home = Trim(dir);
        if (home.empty() || !Exists(home + L"\\bin\\server\\jvm.dll")) continue;
        j.home = Utf8(home);
        j.source = source;
        j.vendor = ReleaseValue(home, "IMPLEMENTOR");
        j.version = ReleaseValue(home, "JAVA_VERSION");
        j.arch = PeArch(home + L"\\bin\\server\\jvm.dll");
        j.javaDesktop = (" " + ReleaseValue(home, "MODULES") + " ").find(" java.desktop ") != std::string::npos;
        j.jniHeaders = Exists(home + L"\\include\\jni.h") && Exists(home + L"\\include\\win32\\jni_md.h");
        break;
    }
    return j;
}

JNIEnv* CreateJvm(const std::wstring& home, std::vector<std::string> opts, JavaVM** vm, std::string& err) {
    // jvm.dll's own imports (vcruntime) live in the JDK's bin folder.
    AddDllDirectory((home + L"\\bin").c_str());
    HMODULE jvm = LoadLibraryExW((home + L"\\bin\\server\\jvm.dll").c_str(), nullptr,
                                 LOAD_LIBRARY_SEARCH_DEFAULT_DIRS | LOAD_LIBRARY_SEARCH_USER_DIRS);
    if (!jvm) { err = "LoadLibrary(jvm.dll) error " + std::to_string(GetLastError()); return nullptr; }
    using CreateFn = jint(JNICALL*)(JavaVM**, void**, void*);
    auto create = reinterpret_cast<CreateFn>(GetProcAddress(jvm, "JNI_CreateJavaVM"));
    if (!create) { err = "jvm.dll has no JNI_CreateJavaVM"; return nullptr; }
    std::vector<JavaVMOption> jopts(opts.size());
    for (size_t i = 0; i < opts.size(); ++i) jopts[i].optionString = opts[i].data();
    JavaVMInitArgs args{};
    args.version = JNI_VERSION_21;
    args.nOptions = (jint)jopts.size();
    args.options = jopts.data();
    args.ignoreUnrecognized = JNI_FALSE;  // an option this JDK rejects is a finding, never ignored
    JNIEnv* env = nullptr;
    jint rc = create(vm, reinterpret_cast<void**>(&env), &args);
    if (rc != JNI_OK) { err = "JNI_CreateJavaVM returned " + std::to_string(rc); return nullptr; }
    return env;
}

void ProbeJvm(Report& r, const std::wstring& javaHome, const std::wstring& probeJar, bool skipStart) {
    r.jvm = FindJvm(javaHome);
    JvmInfo& j = r.jvm;    if (j.home.empty()) {
        j.started = "not-run: no JVM found (Windows-ARM64.apk's own runtime, WINAPK_JAVA_HOME or JAVA_HOME)";
    } else if (j.arch != "arm64") {
        // An emulated JVM is never a native run (AGENTS.md, "Native means native").
        j.started = "not-run: jvm.dll is " + j.arch + ", not arm64";
    } else if (skipStart) {
        j.started = "not-run: --no-jvm";
    } else {
        j.started = StartJvm(Wide(j.home), probeJar, j.probe);
    }
}

}  // namespace winapk
