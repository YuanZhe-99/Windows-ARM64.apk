// angle_runtime.cpp -- shared Windows ARM64 ANGLE backend selector, blob cache and Pbuffer probe.
#include "angle_runtime.h"
#include "bridge.h"

#include <shlobj.h>
#include <vulkan/vulkan_core.h>

#ifdef WINAPK_HAS_ANGLE_HEADERS
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <EGL/eglext_angle.h>
#include <GLES2/gl2.h>
#endif

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <mutex>

namespace winapk::bridge {
namespace {

bool EnvHasLoaderOverride() {
    for (const wchar_t* k : {L"VK_DRIVER_FILES", L"VK_ICD_FILENAMES", L"VK_LOADER_DRIVERS_SELECT"}) {
        wchar_t buf[8]{};
        if (GetEnvironmentVariableW(k, buf, 8) > 0) return true;
    }
    return false;
}

bool ProbeNativeVendorVulkanUncached() {
    wchar_t sysDir[MAX_PATH]{};
    const UINT len = GetSystemDirectoryW(sysDir, MAX_PATH);
    if (!len || len >= MAX_PATH) return false;
    const std::wstring path = std::wstring(sysDir) + L"\\vulkan-1.dll";
    HMODULE mod = LoadLibraryExW(path.c_str(), nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (!mod) return false;

    auto gipa = reinterpret_cast<PFN_vkGetInstanceProcAddr>(GetProcAddress(mod, "vkGetInstanceProcAddr"));
    if (!gipa) { FreeLibrary(mod); return false; }
    auto createInst = reinterpret_cast<PFN_vkCreateInstance>(gipa(VK_NULL_HANDLE, "vkCreateInstance"));
    if (!createInst) { FreeLibrary(mod); return false; }

    VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO};
    app.apiVersion = VK_API_VERSION_1_2;
    VkInstanceCreateInfo ci{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    ci.pApplicationInfo = &app;
    VkInstance inst = VK_NULL_HANDLE;
    if (createInst(&ci, nullptr, &inst) != VK_SUCCESS || !inst) {
        FreeLibrary(mod);
        return false;
    }

    auto destroyInst = reinterpret_cast<PFN_vkDestroyInstance>(gipa(inst, "vkDestroyInstance"));
    auto enumDevs = reinterpret_cast<PFN_vkEnumeratePhysicalDevices>(gipa(inst, "vkEnumeratePhysicalDevices"));
    auto getProps2 = reinterpret_cast<PFN_vkGetPhysicalDeviceProperties2>(gipa(inst, "vkGetPhysicalDeviceProperties2"));
    auto getProps = reinterpret_cast<PFN_vkGetPhysicalDeviceProperties>(gipa(inst, "vkGetPhysicalDeviceProperties"));

    bool hasNative = false;
    uint32_t count = 0;
    if (enumDevs && enumDevs(inst, &count, nullptr) == VK_SUCCESS && count > 0 && count <= 32) {
        std::vector<VkPhysicalDevice> devs(count);
        if (enumDevs(inst, &count, devs.data()) == VK_SUCCESS) {
            for (VkPhysicalDevice pd : devs) {
                if (getProps2) {
                    VkPhysicalDeviceDriverProperties drv{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES};
                    VkPhysicalDeviceProperties2 p2{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2, &drv};
                    getProps2(pd, &p2);
                    if (p2.properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_CPU) continue;
                    const bool translated = drv.driverID == VK_DRIVER_ID_MESA_DOZEN ||
                        std::strstr(drv.driverName, "Dozen") != nullptr ||
                        std::strstr(p2.properties.deviceName, "Direct3D12") != nullptr;
                    if (!translated) { hasNative = true; break; }
                } else if (getProps) {
                    VkPhysicalDeviceProperties p{};
                    getProps(pd, &p);
                    if (p.deviceType != VK_PHYSICAL_DEVICE_TYPE_CPU &&
                        std::strstr(p.deviceName, "Direct3D12") == nullptr) {
                        hasNative = true;
                        break;
                    }
                }
            }
        }
    }
    if (destroyInst) destroyInst(inst, nullptr);
    FreeLibrary(mod);
    return hasNative;
}

#ifdef WINAPK_HAS_ANGLE_HEADERS
// ---- Atomic on-disk EGL_ANDROID_blob_cache ----
// Two separate directories (d3d11 vs vulkan) so concurrent displays or backend switches never mix.
struct BlobCacheDir {
    std::wstring dir;
    std::atomic<uint64_t> bytes{0};
    std::atomic<uint64_t> seq{0};
};

constexpr uint64_t kMaxBlobCacheBytes = 256ull * 1024ull * 1024ull; // 256 MiB per backend
std::once_flag g_blobInitFlag;
BlobCacheDir g_blobD3d11;
BlobCacheDir g_blobVulkan;

void InitBlobDirsOnce() {
    wchar_t envOff[8]{};
    if (GetEnvironmentVariableW(L"WINAPK_ANGLE_BLOB_CACHE", envOff, 8) > 0 && envOff[0] == L'0') return;
    PWSTR local = nullptr;
    if (FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &local)) || !local) return;
    const std::filesystem::path base = std::filesystem::path(local) / L"Windows-ARM64.apk" / L"Caches" / L"angle-blobs";
    CoTaskMemFree(local);

    auto setup = [&](BlobCacheDir& slot, const wchar_t* tag) {
        std::error_code ec;
        const auto dir = base / tag;
        std::filesystem::create_directories(dir, ec);
        if (ec) return;
        uint64_t total = 0;
        for (const auto& entry : std::filesystem::directory_iterator(dir, ec)) {
            if (ec) break;
            if (entry.is_regular_file(ec)) {
                if (entry.path().extension() == L".tmp") {
                    std::filesystem::remove(entry.path(), ec);
                } else {
                    total += entry.file_size(ec);
                }
            }
        }
        slot.dir = dir.wstring();
        slot.bytes.store(total, std::memory_order_relaxed);
    };
    setup(g_blobD3d11, L"801a1a5-d3d11");
    setup(g_blobVulkan, L"801a1a5-vulkan");
}

bool BlobFilePath(const BlobCacheDir& slot, const void* key, EGLsizeiANDROID keySize, std::wstring& out) {
    if (slot.dir.empty() || !key || keySize <= 0 || keySize > 128) return false;
    static constexpr wchar_t kHex[] = L"0123456789abcdef";
    const auto* bytes = static_cast<const uint8_t*>(key);
    std::wstring name;
    name.reserve(static_cast<size_t>(keySize) * 2 + 5);
    for (EGLsizeiANDROID i = 0; i < keySize; ++i) {
        name.push_back(kHex[bytes[i] >> 4]);
        name.push_back(kHex[bytes[i] & 0x0f]);
    }
    name += L".blob";
    out = slot.dir + L"\\" + name;
    return true;
}

void StoreBlob(BlobCacheDir& slot, const void* key, EGLsizeiANDROID keySize, const void* value, EGLsizeiANDROID valueSize) {
    if (!value || valueSize <= 0 || static_cast<uint64_t>(valueSize) > 16ull * 1024ull * 1024ull) return;
    if (slot.bytes.load(std::memory_order_relaxed) + static_cast<uint64_t>(valueSize) > kMaxBlobCacheBytes) return;
    std::wstring finalPath;
    if (!BlobFilePath(slot, key, keySize, finalPath)) return;
    const uint64_t n = slot.seq.fetch_add(1, std::memory_order_relaxed);
    const std::wstring tmpPath = finalPath + L"." + std::to_wstring(GetCurrentProcessId()) + L"." + std::to_wstring(n) + L".tmp";

    HANDLE h = CreateFileW(tmpPath.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return;
    const auto* p = static_cast<const uint8_t*>(value);
    DWORD remaining = static_cast<DWORD>(valueSize);
    bool ok = true;
    while (remaining > 0) {
        DWORD written = 0;
        if (!WriteFile(h, p, remaining, &written, nullptr) || written == 0) { ok = false; break; }
        p += written;
        remaining -= written;
    }
    CloseHandle(h);
    if (!ok || !MoveFileExW(tmpPath.c_str(), finalPath.c_str(), MOVEFILE_REPLACE_EXISTING)) {
        DeleteFileW(tmpPath.c_str());
        return;
    }
    slot.bytes.fetch_add(static_cast<uint64_t>(valueSize), std::memory_order_relaxed);
}

EGLsizeiANDROID LoadBlob(const BlobCacheDir& slot, const void* key, EGLsizeiANDROID keySize, void* value, EGLsizeiANDROID valueSize) {
    std::wstring path;
    if (!BlobFilePath(slot, key, keySize, path)) return 0;
    WIN32_FILE_ATTRIBUTE_DATA fad{};
    if (!GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &fad)) return 0;
    const uint64_t fullSize = (static_cast<uint64_t>(fad.nFileSizeHigh) << 32) | fad.nFileSizeLow;
    if (fullSize == 0 || fullSize > 64ull * 1024ull * 1024ull) return 0;
    const auto expected = static_cast<EGLsizeiANDROID>(fullSize);
    // ANGLE probes first with value == nullptr / valueSize < expected and requires the full stored size.
    if (!value || valueSize < expected) return expected;

    HANDLE h = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return 0;
    auto* dst = static_cast<uint8_t*>(value);
    DWORD remaining = static_cast<DWORD>(expected);
    while (remaining > 0) {
        DWORD got = 0;
        if (!ReadFile(h, dst, remaining, &got, nullptr) || got == 0) { CloseHandle(h); return 0; }
        dst += got;
        remaining -= got;
    }
    CloseHandle(h);
    return expected;
}

void EGLAPIENTRY BlobSetD3d11(const void* k, EGLsizeiANDROID ks, const void* v, EGLsizeiANDROID vs) { StoreBlob(g_blobD3d11, k, ks, v, vs); }
EGLsizeiANDROID EGLAPIENTRY BlobGetD3d11(const void* k, EGLsizeiANDROID ks, void* v, EGLsizeiANDROID vs) { return LoadBlob(g_blobD3d11, k, ks, v, vs); }
void EGLAPIENTRY BlobSetVulkan(const void* k, EGLsizeiANDROID ks, const void* v, EGLsizeiANDROID vs) { StoreBlob(g_blobVulkan, k, ks, v, vs); }
EGLsizeiANDROID EGLAPIENTRY BlobGetVulkan(const void* k, EGLsizeiANDROID ks, void* v, EGLsizeiANDROID vs) { return LoadBlob(g_blobVulkan, k, ks, v, vs); }
#endif

}  // namespace

bool HasNativeVendorVulkan() {
    if (EnvHasLoaderOverride()) return ProbeNativeVendorVulkanUncached();
    static const bool cached = ProbeNativeVendorVulkanUncached();
    return cached;
}

std::vector<const char*> SelectAngleBackends(const std::string& request, bool* nativeVulkanOut) {
    if (request == "d3d11") {
        if (nativeVulkanOut) *nativeVulkanOut = false;
        return {"d3d11"};
    }
    if (request == "vulkan") {
        if (nativeVulkanOut) *nativeVulkanOut = false;
        return {"vulkan"};
    }
    if (request == "auto") {
        const bool nativeVk = HasNativeVendorVulkan();
        if (nativeVulkanOut) *nativeVulkanOut = nativeVk;
        if (nativeVk) return {"vulkan", "d3d11"};
        return {"d3d11", "vulkan"};
    }
    if (nativeVulkanOut) *nativeVulkanOut = false;
    return {};
}

void InstallAngleBlobCache(void* display, void* (*getProc)(const char*), const char* backend) {
#ifdef WINAPK_HAS_ANGLE_HEADERS
    if (!display || !getProc || !backend) return;
    std::call_once(g_blobInitFlag, InitBlobDirsOnce);
    auto setFuncs = reinterpret_cast<PFNEGLSETBLOBCACHEFUNCSANDROIDPROC>(getProc("eglSetBlobCacheFuncsANDROID"));
    if (!setFuncs) return;
    if (std::strcmp(backend, "d3d11") == 0 && !g_blobD3d11.dir.empty()) {
        setFuncs(static_cast<EGLDisplay>(display), &BlobSetD3d11, &BlobGetD3d11);
    } else if (std::strcmp(backend, "vulkan") == 0 && !g_blobVulkan.dir.empty()) {
        setFuncs(static_cast<EGLDisplay>(display), &BlobSetVulkan, &BlobGetVulkan);
    }
#else
    (void)display; (void)getProc; (void)backend;
#endif
}

std::vector<AngleBackendInfo> ProbeAngleBackends(const std::wstring& angleDir, bool vulkanTranslated) {
    std::vector<AngleBackendInfo> out;
#ifdef WINAPK_HAS_ANGLE_HEADERS
    if (angleDir.empty()) return out;
    constexpr DWORD flags = LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32;
    HMODULE gles = LoadLibraryExW((angleDir + L"\\libGLESv2.dll").c_str(), nullptr, flags);
    HMODULE egl = gles ? LoadLibraryExW((angleDir + L"\\libEGL.dll").c_str(), nullptr, flags) : nullptr;
    if (!egl || !gles) {
        if (egl) FreeLibrary(egl);
        if (gles) FreeLibrary(gles);
        return out;
    }

    auto eglGetProc = reinterpret_cast<PFNEGLGETPROCADDRESSPROC>(GetProcAddress(egl, "eglGetProcAddress"));
    auto eglInit = reinterpret_cast<PFNEGLINITIALIZEPROC>(GetProcAddress(egl, "eglInitialize"));
    auto eglTerm = reinterpret_cast<PFNEGLTERMINATEPROC>(GetProcAddress(egl, "eglTerminate"));
    auto eglBind = reinterpret_cast<PFNEGLBINDAPIPROC>(GetProcAddress(egl, "eglBindAPI"));
    auto eglChoose = reinterpret_cast<PFNEGLCHOOSECONFIGPROC>(GetProcAddress(egl, "eglChooseConfig"));
    auto eglCreatePb = reinterpret_cast<PFNEGLCREATEPBUFFERSURFACEPROC>(GetProcAddress(egl, "eglCreatePbufferSurface"));
    auto eglDestroySurf = reinterpret_cast<PFNEGLDESTROYSURFACEPROC>(GetProcAddress(egl, "eglDestroySurface"));
    auto eglCreateCtx = reinterpret_cast<PFNEGLCREATECONTEXTPROC>(GetProcAddress(egl, "eglCreateContext"));
    auto eglDestroyCtx = reinterpret_cast<PFNEGLDESTROYCONTEXTPROC>(GetProcAddress(egl, "eglDestroyContext"));
    auto eglMakeCur = reinterpret_cast<PFNEGLMAKECURRENTPROC>(GetProcAddress(egl, "eglMakeCurrent"));
    auto glGetStr = reinterpret_cast<PFNGLGETSTRINGPROC>(GetProcAddress(gles, "glGetString"));

    auto getPlat = eglGetProc ? reinterpret_cast<PFNEGLGETPLATFORMDISPLAYEXTPROC>(eglGetProc("eglGetPlatformDisplayEXT")) : nullptr;
    if (getPlat && eglInit && eglTerm && eglBind && eglChoose && eglCreatePb && eglDestroySurf && eglCreateCtx && eglDestroyCtx && eglMakeCur && glGetStr) {
        for (const char* name : {"d3d11", "vulkan"}) {
            const bool isVk = std::strcmp(name, "vulkan") == 0;
            const EGLint dispAttrs[] = {
                EGL_PLATFORM_ANGLE_TYPE_ANGLE, isVk ? EGL_PLATFORM_ANGLE_TYPE_VULKAN_ANGLE : EGL_PLATFORM_ANGLE_TYPE_D3D11_ANGLE,
                EGL_PLATFORM_ANGLE_DEVICE_TYPE_ANGLE, EGL_PLATFORM_ANGLE_DEVICE_TYPE_HARDWARE_ANGLE,
                EGL_NONE
            };
            EGLDisplay dpy = getPlat(EGL_PLATFORM_ANGLE_ANGLE, EGL_DEFAULT_DISPLAY, dispAttrs);
            if (dpy == EGL_NO_DISPLAY || !eglInit(dpy, nullptr, nullptr)) continue;
            if (!eglBind(EGL_OPENGL_ES_API)) { eglTerm(dpy); continue; }

            EGLConfig cfg = nullptr;
            EGLint numCfg = 0;
            constexpr EGLint cfgAttrs[] = {
                EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
                EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
                EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8,
                EGL_NONE
            };
            if (!eglChoose(dpy, cfgAttrs, &cfg, 1, &numCfg) || numCfg < 1) { eglTerm(dpy); continue; }

            constexpr EGLint pbAttrs[] = {EGL_WIDTH, 1, EGL_HEIGHT, 1, EGL_NONE};
            EGLSurface surf = eglCreatePb(dpy, cfg, pbAttrs);
            if (surf == EGL_NO_SURFACE) { eglTerm(dpy); continue; }

            // Try ES 3 first so doctor reports the true ceiling (e.g. ES 3.0 on D3D11, ES 3.1 on native Vulkan, ES 2.0 on Dozen).
            EGLContext ctx = EGL_NO_CONTEXT;
            for (int major : {3, 2}) {
                const EGLint ctxAttrs[] = {EGL_CONTEXT_CLIENT_VERSION, major, EGL_NONE};
                ctx = eglCreateCtx(dpy, cfg, EGL_NO_CONTEXT, ctxAttrs);
                if (ctx != EGL_NO_CONTEXT) break;
            }
            if (ctx != EGL_NO_CONTEXT && eglMakeCur(dpy, surf, surf, ctx)) {
                const char* rend = reinterpret_cast<const char*>(glGetStr(GL_RENDERER));
                const char* ver = reinterpret_cast<const char*>(glGetStr(GL_VERSION));
                const bool okMatch = rend && ver && (isVk
                    ? (std::strstr(rend, "Vulkan") != nullptr)
                    : (std::strstr(rend, "Direct3D11") != nullptr || std::strstr(rend, "D3D11") != nullptr));
                // Reject Microsoft Basic Render Driver masquerading as hardware.
                const bool software = rend && std::strstr(rend, "Microsoft Basic Render Driver") != nullptr;
                if (okMatch && !software) {
                    out.push_back({name, ver, rend, isVk && vulkanTranslated});
                }
                eglMakeCur(dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
            }
            if (ctx != EGL_NO_CONTEXT) eglDestroyCtx(dpy, ctx);
            eglDestroySurf(dpy, surf);
            eglTerm(dpy);
        }
    }
    FreeLibrary(egl);
    FreeLibrary(gles);
#else
    (void)angleDir; (void)vulkanTranslated;
#endif
    return out;
}

}  // namespace winapk::bridge

extern "C" __declspec(dllexport) int winapk_bridge_probe_angle(
        const wchar_t* angleDir, int vulkanTranslated, char* outBuf, int outCap) {
    if (!angleDir || !outBuf || outCap <= 0) return 0;
    outBuf[0] = '\0';
    const auto list = winapk::bridge::ProbeAngleBackends(angleDir, vulkanTranslated != 0);
    std::string text;
    for (const auto& b : list) {
        text += b.backend + "\t" + b.gles + "\t" + b.renderer + "\t" + (b.translated ? "1" : "0") + "\n";
    }
    if (static_cast<int>(text.size()) >= outCap) return 0;
    std::memcpy(outBuf, text.data(), text.size() + 1);
    return static_cast<int>(list.size());
}
