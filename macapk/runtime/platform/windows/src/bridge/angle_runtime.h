// angle_runtime.h -- shared Windows ARM64 ANGLE backend selection, Pbuffer capability probe,
// and atomic on-disk EGL_ANDROID_blob_cache (ADR-0005, GPU-P1).
//
// Host-domain code only. Keeps D3D11 and Vulkan backend reporting strictly separate and never
// silently switches an explicit backend request.
#pragma once

#include <windows.h>

#include <string>
#include <vector>

namespace winapk::bridge {

/// Describes one verified ANGLE hardware backend probed via a 1x1 offscreen Pbuffer.
struct AngleBackendInfo {
    std::string backend;   ///< "d3d11" or "vulkan"
    std::string gles;      ///< e.g. "OpenGL ES 3.0 ..."
    std::string renderer;  ///< ANGLE GL_RENDERER string
    bool translated = false; ///< true when ANGLE's Vulkan backend sits over a translated Vulkan ICD
};

/// Returns true when System32's Vulkan loader exposes at least one non-CPU, non-translated
/// GPU-vendor physical device (false when Vulkan is absent or every GPU device is Mesa Dozen).
/// Notes: cached per process unless Vulkan loader environment overrides are active.
bool HasNativeVendorVulkan();

/// Computes the ordered ANGLE backend candidates for `request` ("d3d11", "vulkan", or "auto").
/// Inputs: `request` is the CLI/UI graphics mode.
/// Returns: 1 backend for explicit `d3d11`/`vulkan` (no fallback), 2 for `auto` (`vulkan` first
///          on native-ICD hosts, `d3d11` first on translated-Vulkan/Dozen hosts per ADR-0005),
///          or empty when `request == "off"` or unrecognized.
std::vector<const char*> SelectAngleBackends(const std::string& request, bool* nativeVulkanOut = nullptr);

/// Installs an atomic on-disk `EGL_ANDROID_blob_cache` on `display` if ANGLE exposes
/// `eglSetBlobCacheFuncsANDROID`.
/// Inputs: `display` is an initialized `EGLDisplay`; `getProc` is `eglGetProcAddress`; `backend`
///         is `"d3d11"` or `"vulkan"`.
/// Side effects: creates `%LOCALAPPDATA%\Windows-ARM64.apk\Caches\angle-blobs\<commit>-<backend>`
///               on demand and persists compiled DXBC / SPIR-V blobs up to 256 MiB.
void InstallAngleBlobCache(void* display, void* (*getProc)(const char*), const char* backend);

/// Probes `angleDir`'s ARM64 `libEGL.dll` and `libGLESv2.dll` headlessly using 1x1 Pbuffers
/// on hardware D3D11 and hardware Vulkan without creating a window.
/// Inputs: `angleDir` is `<bin>\angle`; `vulkanTranslated` marks whether host Vulkan is translated.
/// Returns: every backend that initialized a hardware context and matched its expected renderer.
std::vector<AngleBackendInfo> ProbeAngleBackends(const std::wstring& angleDir, bool vulkanTranslated);

}  // namespace winapk::bridge

/// Exported entry point for `winapk doctor` to probe private ANGLE backends without linking EGL.
/// Writes newline-separated `<backend>\t<gles>\t<renderer>\t<translated:0|1>` records into `outBuf`.
extern "C" __declspec(dllexport) int winapk_bridge_probe_angle(
    const wchar_t* angleDir, int vulkanTranslated, char* outBuf, int outCap);
