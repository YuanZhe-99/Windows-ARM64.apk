// doctor.h -- what `winapk doctor` finds on this PC (PLAN.md §7, "doctor 最低字段").
//
// Host-domain code only (Windows ARM64): nothing here touches guest Android ABI types.
#pragma once

#include <jni.h>

#include <cstdint>
#include <string>
#include <vector>

namespace winapk {

/// Describes Windows and this process as Windows reports them.
struct HostInfo {
    std::string osBuild;      ///< "10.0.26300.9711": major.minor.build.UBR
    std::string osArch;       ///< the machine's architecture: "arm64", "x64", ...
    std::string processArch;  ///< this process's: "arm64" unless it runs emulated
    std::string cpu;          ///< the processor's name, as the registry has it
    uint64_t memoryMB = 0;    ///< physical memory
};

/// Describes a display adapter as DXGI lists it (the driver Windows uses for it).
struct Adapter {
    std::string name;
    uint32_t vendorId = 0;
    uint32_t deviceId = 0;
    std::string driverVersion;  ///< the user-mode driver's version, "a.b.c.d"
    bool software = false;      ///< Microsoft's software adapters (Basic Render, Remote Display)
};

/// Describes the JVM the runtime would embed and what starting it showed.
struct JvmInfo {
    std::string home;        ///< the JDK or runtime folder, "" when none was found
    std::string source;      ///< where it was found: "argument", "own", "WINAPK_JAVA_HOME", "JAVA_HOME"
    std::string vendor;      ///< from the release file
    std::string version;     ///< from the release file
    std::string arch;        ///< of jvm.dll's PE image: "arm64", "x64", ...
    bool javaDesktop = false;  ///< the release file lists java.desktop (Java2D, AWT)
    bool jniHeaders = false;   ///< include/jni.h and include/win32/jni_md.h exist (a JDK, not a JRE)
    /// The result of creating the JVM in this process and running HostProbe:
    /// "ok", "not-run: <reason>" or "failed: <reason>".
    std::string started;
    /// HostProbe's findings by key (jni, java2d, fontFamilies, osArch, ...), in order.
    std::vector<std::pair<std::string, std::string>> probe;
};

/// Describes a Vulkan physical device as the Windows loader enumerates it.
struct VulkanDevice {
    std::string name;
    uint32_t vendorId = 0;
    uint32_t deviceId = 0;
    std::string driverVersion;  ///< raw driverVersion, "0x%08x": its encoding is the vendor's
    std::string apiVersion;     ///< "1.3.295"
    std::string type;           ///< "integrated", "discrete", "virtual", "cpu", "other"
    std::string driverName;  ///< VkPhysicalDeviceDriverProperties (Vulkan 1.2), "" when not reported
    std::string driverInfo;
    /// The driver translates Vulkan to another API (Mesa Dozen: Direct3D 12) rather than
    /// being the GPU vendor's own Vulkan driver.
    bool translated = false;
    uint32_t queueFamilies = 0;
    bool win32Presentation = false;  ///< some queue family can present to a Win32 surface
};

/// Describes the Vulkan loader, its devices and the Win32 present test.
struct VulkanInfo {
    /// "available", or why not: "not-found", "no-entry-point", "instance-failed: <VkResult>".
    std::string loader;
    std::string loaderPath;
    std::string loaderArch;     ///< of vulkan-1.dll's PE image
    std::string instanceVersion;  ///< "1.3.300"
    bool win32SurfaceExtension = false;
    std::vector<VulkanDevice> devices;
    /// "passed", "failed: <reason>" or "not-run: <reason>". A window shows only with --present.
    std::string win32Present;
};

/// Describes one probed private ANGLE backend (`d3d11` or `vulkan`).
struct AngleBackend {
    std::string backend;
    std::string gles;
    std::string renderer;
    bool translated = false;
};

/// Everything doctor reports.
struct Report {
    std::string version;          ///< winapk's own version (internal/version)
    std::string frameworkCommit;  ///< merged Mac.apk commit recorded in build-lock.json
    HostInfo host;
    std::vector<Adapter> adapters;
    JvmInfo jvm;
    VulkanInfo vulkan;
    bool angleFilesArm64 = false; ///< private EGL/GLES PE files found
    std::vector<AngleBackend> angleBackends; ///< hardware backends verified via headless 1x1 Pbuffer
    std::vector<std::string> guestNativeProfiles; ///< restricted profiles whose helpers are present
    /// Stable identifiers of what this build cannot do yet (PLAN.md §14.2 style), each with
    /// a reason; doctor reports them rather than letting a missing capability look present.
    std::vector<std::pair<std::string, std::string>> limitations;
};

/// Fills host and adapters.
void ProbeHost(Report& r);

/// Finds the JVM (javaHome, else Windows-ARM64.apk's own runtime, else WINAPK_JAVA_HOME,
/// else JAVA_HOME) and, unless skipStart, creates it in this process and runs HostProbe.
/// Inputs: probeJar is winapk-probe.jar, next to winapk.exe.
/// Side effects: loads jvm.dll; a JVM cannot be created twice in one process, so this runs
/// once per doctor run.
void ProbeJvm(Report& r, const std::wstring& javaHome, const std::wstring& probeJar, bool skipStart);

/// Finds the JVM the runtime embeds: javaHome, else Windows-ARM64.apk's own runtime
/// (%LOCALAPPDATA%\Windows-ARM64.apk\Runtime), else WINAPK_JAVA_HOME, else JAVA_HOME.
/// Returns: its description; home is "" when none was found. Nothing is started.
JvmInfo FindJvm(const std::wstring& javaHome);

/// Loads `home`'s jvm.dll and creates the JVM with `opts`, refusing unknown options.
/// Returns: the main thread's JNIEnv and `vm`, or null with `err` set.
/// Side effects: a process can create one JVM only.
JNIEnv* CreateJvm(const std::wstring& home, std::vector<std::string> opts, JavaVM** vm, std::string& err);

/// Fills vulkan: loader, instance, devices, and with present the Win32 surface test.
/// Side effects: with present, shows a small window titled as a test for under a second.
void ProbeVulkan(Report& r, bool present);

/// Returns the machine type of a PE file as "arm64", "arm64ec", "x64", "x86" or "unknown".
std::string PeArch(const std::wstring& path);

/// Converts UTF-16 to UTF-8.
std::string Utf8(const std::wstring& s);

/// Converts UTF-8 to UTF-16.
std::wstring Wide(const std::string& s);

}  // namespace winapk
