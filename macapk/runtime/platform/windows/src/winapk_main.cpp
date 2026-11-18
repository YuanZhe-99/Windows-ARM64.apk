// winapk_main.cpp -- the winapk CLI on Windows ARM64 (PLAN.md §18).
//
//   winapk doctor [--json] [--present] [--no-jvm] [--java-home <dir>]
//   winapk inspect <apk> [--json]
//   winapk run <apk> [--native=off|restricted] [--graphics=off|auto|vulkan|d3d11] [...]
//   winapk dex <apk> <out.jar> [--java-home <dir>]   (the conversion step of run, in its own process)
//   winapk --version
#include "doctor.h"
#include "run.h"

#include <windows.h>

#include <cstdio>
#include <string>

#ifndef WINAPK_VERSION
#error "WINAPK_VERSION comes from internal/version/version.go (windows.cmake)"
#endif

namespace winapk {
namespace {

/// Escapes a string for JSON.
std::string Json(const std::string& s) {
    std::string out = "\"";
    for (unsigned char c : s) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (c < 0x20) {
                    char b[8];
                    snprintf(b, sizeof b, "\\u%04x", c);
                    out += b;
                } else {
                    out += (char)c;
                }
        }
    }
    return out + "\"";
}

std::string Bool(bool b) { return b ? "true" : "false"; }

std::string Hex(uint32_t v) {
    char b[16];
    snprintf(b, sizeof b, "0x%04x", v);
    return b;
}

/// Says what this build cannot do yet, and what this PC lacks, each with a stable ID.
void Limitations(Report& r) {
    auto add = [&](const char* id, const std::string& why) { r.limitations.emplace_back(id, why); };
    if (r.host.processArch != "arm64") add("HOST_ARCH_UNSUPPORTED", "winapk runs as " + r.host.processArch + ", not native arm64");
    if (r.jvm.started != "ok") add("DEPENDENCY_MISSING", "JVM: " + r.jvm.started);
    add("GUEST_ABI_UNSUPPORTED", "guest native defaults off; restricted C/JNI profile only, not arbitrary NDK/C++/ELF TLS or graphics compatibility");
    if (!r.angleFilesArm64)
        add("ANGLE_DLLS_MISSING", "private ARM64 libEGL.dll/libGLESv2.dll not found; the Java bitmap path is independent");
    if (r.vulkan.loader != "available") {
        add("GRAPHICS_CAPABILITY_MISSING", "Vulkan loader: " + r.vulkan.loader);
    } else if (r.vulkan.devices.empty()) {
        add("GRAPHICS_CAPABILITY_MISSING", "the Vulkan loader lists no device: no driver installs a Vulkan ICD for this GPU");
    } else {
        bool vendor = false;
        for (const VulkanDevice& d : r.vulkan.devices) vendor |= !d.translated && d.type != "cpu";
        if (!vendor) {
            add("VULKAN_DRIVER_TRANSLATED",
                "no GPU vendor Vulkan driver: every Vulkan device translates to another API (Mesa Dozen on Direct3D 12)");
        }
    }
    add("GUEST_VULKAN_NOT_IMPLEMENTED", "guest Vulkan supports only an experimental WSI/transfer-clear profile, not general rendering");
}

void PrintJson(const Report& r) {
    std::string o = "{\n  \"schemaVersion\": 1,\n  \"version\": " + Json(r.version) + ",\n";
    o += "  \"host\": { \"osBuild\": " + Json(r.host.osBuild) + ", \"osArch\": " + Json(r.host.osArch) +
         ", \"processArch\": " + Json(r.host.processArch) + ", \"cpu\": " + Json(r.host.cpu) +
         ", \"memoryMB\": " + std::to_string(r.host.memoryMB) + " },\n";
    const JvmInfo& j = r.jvm;
    o += "  \"jvm\": { \"home\": " + Json(j.home) + ", \"source\": " + Json(j.source) + ", \"vendor\": " + Json(j.vendor) +
         ", \"version\": " + Json(j.version) + ", \"arch\": " + Json(j.arch) + ", \"javaDesktop\": " + Bool(j.javaDesktop) +
         ", \"jniHeaders\": " + Bool(j.jniHeaders) + ", \"started\": " + Json(j.started) + ", \"probe\": {";
    for (size_t i = 0; i < j.probe.size(); ++i) o += (i ? ", " : " ") + Json(j.probe[i].first) + ": " + Json(j.probe[i].second);
    o += j.probe.empty() ? "} },\n" : " } },\n";
    o += "  \"graphics\": {\n    \"adapters\": [";
    for (size_t i = 0; i < r.adapters.size(); ++i) {
        const Adapter& a = r.adapters[i];
        o += std::string(i ? "," : "") + "\n      { \"name\": " + Json(a.name) + ", \"vendorId\": " + Json(Hex(a.vendorId)) +
             ", \"deviceId\": " + Json(Hex(a.deviceId)) + ", \"driverVersion\": " + Json(a.driverVersion) +
             ", \"software\": " + Bool(a.software) + " }";
    }
    const VulkanInfo& v = r.vulkan;
    o += std::string(r.adapters.empty() ? "" : "\n    ") + "],\n";
    o += "    \"vulkanLoader\": " + Json(v.loader) + ", \"vulkanLoaderPath\": " + Json(v.loaderPath) +
         ", \"vulkanLoaderArch\": " + Json(v.loaderArch) + ",\n    \"vulkanInstanceVersion\": " + Json(v.instanceVersion) +
         ", \"win32SurfaceExtension\": " + Bool(v.win32SurfaceExtension) + ",\n    \"devices\": [";
    for (size_t i = 0; i < v.devices.size(); ++i) {
        const VulkanDevice& d = v.devices[i];
        o += std::string(i ? "," : "") + "\n      { \"name\": " + Json(d.name) + ", \"vendorId\": " + Json(Hex(d.vendorId)) +
             ", \"deviceId\": " + Json(Hex(d.deviceId)) + ", \"driverVersion\": " + Json(d.driverVersion) +
             ", \"apiVersion\": " + Json(d.apiVersion) + ", \"type\": " + Json(d.type) +
             ", \"driverName\": " + Json(d.driverName) + ", \"driverInfo\": " + Json(d.driverInfo) +
             ", \"translated\": " + Bool(d.translated) +
             ", \"queueFamilies\": " + std::to_string(d.queueFamilies) + ", \"win32Presentation\": " + Bool(d.win32Presentation) + " }";
    }
    o += std::string(v.devices.empty() ? "" : "\n    ") + "],\n";
    o += "    \"win32Present\": " + Json(v.win32Present) + ",\n    \"angleFilesArm64\": " + Bool(r.angleFilesArm64) +
         ",\n    \"angleBackends\": []\n  },\n";
    o += "  \"runtime\": { \"guestNativeProfiles\": [";
    for (size_t i = 0; i < r.guestNativeProfiles.size(); ++i)
        o += (i ? ", " : "") + Json(r.guestNativeProfiles[i]);
    o += "], \"guestVulkan\": false, \"frameworkCommit\": " + Json(r.frameworkCommit) + " },\n  \"limitations\": [";
    for (size_t i = 0; i < r.limitations.size(); ++i) {
        o += std::string(i ? "," : "") + "\n    { \"id\": " + Json(r.limitations[i].first) + ", \"reason\": " + Json(r.limitations[i].second) + " }";
    }
    o += "\n  ]\n}\n";
    fputs(o.c_str(), stdout);
}

void PrintText(const Report& r) {
    printf("winapk %s\n", r.version.c_str());
    printf("Windows   %s, %s (this process: %s)\n", r.host.osBuild.c_str(), r.host.osArch.c_str(), r.host.processArch.c_str());
    printf("CPU       %s, %llu MB\n", r.host.cpu.c_str(), (unsigned long long)r.host.memoryMB);
    for (const Adapter& a : r.adapters) {
        if (!a.software) printf("GPU       %s (driver %s)\n", a.name.c_str(), a.driverVersion.c_str());
    }
    const JvmInfo& j = r.jvm;
    if (j.home.empty()) {
        printf("JVM       none found\n");
    } else {
        printf("JVM       %s %s, %s, from %s (%s)\n", j.vendor.c_str(), j.version.c_str(), j.arch.c_str(), j.source.c_str(), j.home.c_str());
    }
    printf("          started: %s\n", j.started.c_str());
    for (const auto& [k, v] : j.probe) printf("          %s: %s\n", k.c_str(), v.c_str());
    printf("Vulkan    loader %s %s (%s); %zu device(s); Win32 present: %s\n", r.vulkan.loader.c_str(),
           r.vulkan.instanceVersion.c_str(), r.vulkan.loaderArch.c_str(), r.vulkan.devices.size(), r.vulkan.win32Present.c_str());
    for (const VulkanDevice& d : r.vulkan.devices) {
        printf("          %s, API %s, driver %s%s, %u queue families, Win32 presentation %s\n", d.name.c_str(),
               d.apiVersion.c_str(), d.driverName.empty() ? "?" : d.driverName.c_str(), d.translated ? " (translated)" : "",
               d.queueFamilies, d.win32Presentation ? "yes" : "no");
    }
    printf("ANGLE     private ARM64 DLLs %s; renderer not tested by doctor\n", r.angleFilesArm64 ? "found" : "not found");
    printf("Not yet:\n");
    for (const auto& [id, why] : r.limitations) printf("  %s: %s\n", id.c_str(), why.c_str());
}

int Usage() {
    fputs("usage: winapk doctor [--json] [--present] [--no-jvm] [--java-home <dir>]\n"
          "       winapk inspect <apk> [--json]\n"
          "       winapk run <apk> [--native=off|restricted] [--profile=N0|N1|N2] [--graphics=off|auto|vulkan|d3d11]\n"
          "                        [--data-dir <dir>] [--java-home <dir>] [--size <w>x<h>]\n"
          "       winapk dex <apk> <out.jar> [--java-home <dir>]\n"
          "       winapk --version\n", stderr);
    return 2;
}

}  // namespace
}  // namespace winapk

int wmain(int argc, wchar_t** argv) {
    using namespace winapk;
    if (argc < 2) return Usage();
    std::wstring cmd = argv[1];
    if (cmd == L"--version" || cmd == L"version") {
        printf("winapk %s\n", WINAPK_VERSION);
        return 0;
    }
    auto full = [](const std::wstring& p) {
        wchar_t buf[32768];
        DWORD n = GetFullPathNameW(p.c_str(), ARRAYSIZE(buf), buf, nullptr);
        return n > 0 && n < ARRAYSIZE(buf) ? std::wstring(buf, n) : p;
    };
    if (cmd == L"inspect") {
        if (argc < 3) return Usage();
        bool json = argc > 3 && std::wstring(argv[3]) == L"--json";
        return Inspect(full(argv[2]), json);
    }
    if (cmd == L"run") {
        if (argc < 3) return Usage();
        RunArgs ra;
        ra.apk = full(argv[2]);
        for (int i = 3; i < argc; ++i) {
            std::wstring a = argv[i];
            if (a.rfind(L"--native=", 0) == 0) ra.native = a.substr(9);
            else if (a.rfind(L"--profile=", 0) == 0) ra.profile = a.substr(10);
            else if (a.rfind(L"--graphics=", 0) == 0) ra.graphics = a.substr(11);
            else if (a == L"--data-dir" && i + 1 < argc) ra.dataDir = full(argv[++i]);
            else if (a == L"--java-home" && i + 1 < argc) ra.javaHome = argv[++i];
            else if (a == L"--embed" && i + 1 < argc) ra.embed = argv[++i];
            else if (a.rfind(L"--embed=", 0) == 0) ra.embed = a.substr(8);
            else if (a == L"--size" && i + 1 < argc && swscanf_s(argv[++i], L"%dx%d", &ra.width, &ra.height) == 2) {}
            else return Usage();
        }
        return Run(ra);
    }
    if (cmd == L"dex") {
        if (argc != 4 && !(argc == 6 && std::wstring(argv[4]) == L"--java-home")) return Usage();
        return ConvertDex(full(argv[2]), full(argv[3]), argc == 6 ? argv[5] : L"");
    }
    if (cmd != L"doctor") return Usage();
    bool json = false, present = false, noJvm = false;
    std::wstring javaHome;
    for (int i = 2; i < argc; ++i) {
        std::wstring a = argv[i];
        if (a == L"--json") json = true;
        else if (a == L"--present") present = true;
        else if (a == L"--no-jvm") noJvm = true;
        else if (a == L"--java-home" && i + 1 < argc) javaHome = argv[++i];
        else return Usage();
    }
    wchar_t self[MAX_PATH];
    GetModuleFileNameW(nullptr, self, MAX_PATH);
    std::wstring dir = self;
    dir = dir.substr(0, dir.find_last_of(L'\\'));

    Report r;
    r.version = WINAPK_VERSION;
    r.frameworkCommit = WINAPK_MACAPK_COMMIT;
    ProbeHost(r);
    ProbeJvm(r, javaHome, dir + L"\\winapk-probe.jar", noJvm);
    ProbeVulkan(r, present);
    r.angleFilesArm64 = PeArch(dir + L"\\angle\\libEGL.dll") == "arm64" &&
                        PeArch(dir + L"\\angle\\libGLESv2.dll") == "arm64";
    if (r.host.processArch == "arm64" && PeArch(dir + L"\\winapk-bridge.dll") == "arm64") {
        r.guestNativeProfiles.push_back("N0");
        if (GetFileAttributesW((dir + L"\\guest\\libwinapk-bionic.so").c_str()) != INVALID_FILE_ATTRIBUTES &&
            GetFileAttributesW((dir + L"\\guest\\libwinapk-jni.so").c_str()) != INVALID_FILE_ATTRIBUTES) {
            r.guestNativeProfiles.push_back("N1");
            if (r.angleFilesArm64 || r.vulkan.loader == "available")
                r.guestNativeProfiles.push_back("N2");
        }
    }
    Limitations(r);
    if (json) PrintJson(r);
    else PrintText(r);
    return 0;
}
