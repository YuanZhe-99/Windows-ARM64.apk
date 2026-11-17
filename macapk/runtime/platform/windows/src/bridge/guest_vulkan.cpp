// Restricted Android Vulkan instance/WSI provider. All native handles stay on the window thread.
#include "guest_vulkan.h"
#include "guest_graphics.h"
#include "bridge.h"

#define VK_USE_PLATFORM_WIN32_KHR
#define VK_USE_PLATFORM_ANDROID_KHR
#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstring>
#include <new>
#include <utility>

namespace winapk::bridge {
namespace {

static_assert(sizeof(VkInstance) == 8 && sizeof(VkPhysicalDevice) == 8 && sizeof(VkSurfaceKHR) == 8);
static_assert(sizeof(VkApplicationInfo) == 48 && sizeof(VkInstanceCreateInfo) == 64 &&
              sizeof(VkAndroidSurfaceCreateInfoKHR) == 32 && offsetof(VkAndroidSurfaceCreateInfoKHR, window) == 24);
static_assert(sizeof(VkExtensionProperties) == 260 && sizeof(VkQueueFamilyProperties) == 24 &&
              sizeof(VkSurfaceCapabilitiesKHR) == 52 && sizeof(VkSurfaceFormatKHR) == 8);

/// Owns the bridge's single instance and window surface; unrelated to ANGLE, even under Dozen.
struct Vulkan {
    HMODULE loader = nullptr;
    PFN_vkGetInstanceProcAddr proc = nullptr;
    PFN_vkEnumerateInstanceExtensionProperties extensions = nullptr;
    VkInstance instance = VK_NULL_HANDLE, guestInstance = VK_NULL_HANDLE;
    VkSurfaceKHR surface = VK_NULL_HANDLE, guestSurface = VK_NULL_HANDLE;
    HWND window = nullptr;
    ANativeWindow* native = nullptr;
    bool surfaceExtension = false;
    bool androidSurfaceExtension = false;
    bool retired = false;
    struct Device { VkPhysicalDevice host, guest; };
    std::array<Device, 32> devices{};
    uint32_t deviceCount = 0;
} vk;
uintptr_t nextHandle = 1;

/// Runs a synchronous command on the HWND owner, without holding a registry/JVM lock while waiting.
template<class F> VkResult Run(F&& fn) {
    VkResult result = VK_ERROR_INITIALIZATION_FAILED;
    try {
        if (DispatchGuestGraphics([&] { result = fn(); })) return result;
    } catch (const std::bad_alloc&) {
        return VK_ERROR_OUT_OF_HOST_MEMORY;
    }
    return result;
}

/// Resolves a driver entry privately against the owned instance (or bootstrap scope).
template<class P> P Driver(const char* name, VkInstance instance = VK_NULL_HANDLE) {
    return reinterpret_cast<P>(vk.proc(instance, name));
}

/// Opens the native system loader only, never a DLL chosen by an APK or on PATH.
bool Loader() {
    if (vk.proc && vk.extensions) return true;
    if (GuestGraphicsRequest() != "vulkan") return false;
    vk.loader = LoadLibraryExW(L"vulkan-1.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (!vk.loader) return false;
    vk.proc = reinterpret_cast<PFN_vkGetInstanceProcAddr>(GetProcAddress(vk.loader, "vkGetInstanceProcAddr"));
    if (vk.proc) vk.extensions = Driver<PFN_vkEnumerateInstanceExtensionProperties>("vkEnumerateInstanceExtensionProperties");
    if (vk.extensions) return true;
    FreeLibrary(vk.loader);
    vk = {};
    return false;
}

/// Counts only the WSI extensions needed for a working Android-to-Win32 translation.
bool WsiAvailable() {
    std::array<VkExtensionProperties, 128> exts{};
    uint32_t count = static_cast<uint32_t>(exts.size());
    if (vk.extensions(nullptr, &count, exts.data()) != VK_SUCCESS) return false;
    bool surface = false, win32 = false;
    for (uint32_t i = 0; i < count; ++i) {
        surface |= std::strcmp(exts[i].extensionName, VK_KHR_SURFACE_EXTENSION_NAME) == 0;
        win32 |= std::strcmp(exts[i].extensionName, VK_KHR_WIN32_SURFACE_EXTENSION_NAME) == 0;
    }
    return surface && win32;
}

/// Copies a Vulkan enumeration with count-only, bounded output and VK_INCOMPLETE semantics.
template<class T> VkResult Enumerate(uint32_t count, const T* values, uint32_t* size, T* out) {
    if (!size) return VK_ERROR_INITIALIZATION_FAILED;
    if (!out) { *size = count; return VK_SUCCESS; }
    const auto written = (std::min)(*size, count);
    for (uint32_t i = 0; i < written; ++i) out[i] = values[i];
    *size = written;
    return written < count ? VK_INCOMPLETE : VK_SUCCESS;
}

/// Reports the bootstrap API level supported by the restricted profile, not the ICD maximum.
VkResult VKAPI_CALL Version(uint32_t* version) {
    if (!version) return VK_ERROR_INITIALIZATION_FAILED;
    *version = VK_API_VERSION_1_0;
    return VK_SUCCESS;
}

/// Reports no guest layers; requesting a layer during creation is also refused.
VkResult VKAPI_CALL Layers(uint32_t* count, VkLayerProperties* out) {
    return Enumerate<VkLayerProperties>(0, nullptr, count, out);
}

/// Filters extensions instead of forwarding host Win32, interop or debug entries.
VkResult VKAPI_CALL Extensions(const char* layer, uint32_t* count, VkExtensionProperties* out) {
    if (layer) return VK_ERROR_LAYER_NOT_PRESENT;
    return Run([=] {
        if (!Loader()) return VK_ERROR_INITIALIZATION_FAILED;
        const VkExtensionProperties supported[] = {
            {VK_KHR_SURFACE_EXTENSION_NAME, VK_KHR_SURFACE_SPEC_VERSION},
            {VK_KHR_ANDROID_SURFACE_EXTENSION_NAME, VK_KHR_ANDROID_SURFACE_SPEC_VERSION},
        };
        const bool available = WsiAvailable();
        if (!available) LogLine("GRAPHICS_CAPABILITY_MISSING: guest Vulkan system loader does not offer Win32 WSI");
        return Enumerate(available ? 2u : 0u, supported, count, out);
    });
}

/// Releases the WSI object separately from the instance and the original Java Surface.
void DropSurface() {
    if (vk.surface) {
        Driver<PFN_vkDestroySurfaceKHR>("vkDestroySurfaceKHR", vk.instance)(vk.instance, vk.surface, nullptr);
        ReleaseSurface(vk.native);
    }
    vk.surface = vk.guestSurface = VK_NULL_HANDLE;
    vk.native = nullptr;
    vk.window = nullptr;
    vk.retired = false;
}

/// Tears down all driver state before the runtime closes the HWND.
void Shutdown() {
    DropSurface();
    if (vk.instance) {
        Driver<PFN_vkDestroyInstance>("vkDestroyInstance", vk.instance)(vk.instance, nullptr);
        LogLine("[winapk] guest Vulkan instance teardown complete");
    }
    if (vk.loader) FreeLibrary(vk.loader);
    vk = {};
}

/// Validates the fixed ABI, translates requested Android WSI extensions and owns the real instance.
VkResult VKAPI_CALL CreateInstance(const VkInstanceCreateInfo* info, const VkAllocationCallbacks* allocator, VkInstance* out) {
    if (!out) return VK_ERROR_INITIALIZATION_FAILED;
    *out = VK_NULL_HANDLE;
    if (!info || info->sType != VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO || info->pNext || allocator || info->flags)
        return VK_ERROR_FEATURE_NOT_PRESENT;
    if (info->enabledLayerCount) return VK_ERROR_LAYER_NOT_PRESENT;
    if (info->enabledExtensionCount > 2 || (info->enabledExtensionCount && !info->ppEnabledExtensionNames))
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    bool surface = false, android = false;
    for (uint32_t i = 0; i < info->enabledExtensionCount; ++i) {
        const char* name = info->ppEnabledExtensionNames[i];
        if (!name || strnlen(name, VK_MAX_EXTENSION_NAME_SIZE) == VK_MAX_EXTENSION_NAME_SIZE)
            return VK_ERROR_EXTENSION_NOT_PRESENT;
        if (!surface && std::strcmp(name, VK_KHR_SURFACE_EXTENSION_NAME) == 0) surface = true;
        else if (!android && std::strcmp(name, VK_KHR_ANDROID_SURFACE_EXTENSION_NAME) == 0) android = true;
        else return VK_ERROR_EXTENSION_NOT_PRESENT;
    }
    if (android && !surface) return VK_ERROR_EXTENSION_NOT_PRESENT;
    if (info->pApplicationInfo) {
        const auto& app = *info->pApplicationInfo;
        if (app.sType != VK_STRUCTURE_TYPE_APPLICATION_INFO || app.pNext) return VK_ERROR_FEATURE_NOT_PRESENT;
        if (app.apiVersion > VK_API_VERSION_1_0) return VK_ERROR_INCOMPATIBLE_DRIVER;
    }
    return Run([&] {
        if (vk.instance || !Loader()) return VK_ERROR_INITIALIZATION_FAILED;
        if (surface && !WsiAvailable()) return VK_ERROR_EXTENSION_NOT_PRESENT;
        const char* names[] = {VK_KHR_SURFACE_EXTENSION_NAME, VK_KHR_WIN32_SURFACE_EXTENSION_NAME};
        VkInstanceCreateInfo host = *info;
        host.enabledExtensionCount = android ? 2 : surface ? 1 : 0;
        host.ppEnabledExtensionNames = names;
        auto create = Driver<PFN_vkCreateInstance>("vkCreateInstance");
        if (!create) return VK_ERROR_INITIALIZATION_FAILED;
        const auto result = create(&host, nullptr, &vk.instance);
        if (result != VK_SUCCESS) {
            LogLine("GRAPHICS_CAPABILITY_MISSING: guest Vulkan vkCreateInstance result=" + std::to_string(result));
            vk.instance = VK_NULL_HANDLE;
            return result;
        }
        vk.surfaceExtension = surface;
        vk.androidSurfaceExtension = android;
        *out = vk.guestInstance = reinterpret_cast<VkInstance>(nextHandle++);
        return VK_SUCCESS;
    });
}

/// Resolves only physical-device tokens minted by this instance.
VkPhysicalDevice Physical(VkPhysicalDevice guest) {
    if (!guest) return VK_NULL_HANDLE;
    for (uint32_t i = 0; i < vk.deviceCount; ++i) if (vk.devices[i].guest == guest) return vk.devices[i].host;
    return VK_NULL_HANDLE;
}

/// Enumerates real devices, returning stable bridge identities until instance destruction.
VkResult VKAPI_CALL PhysicalDevices(VkInstance instance, uint32_t* count, VkPhysicalDevice* out) {
    return Run([=] {
        if (!instance || instance != vk.guestInstance) return VK_ERROR_INITIALIZATION_FAILED;
        auto enumerate = Driver<PFN_vkEnumeratePhysicalDevices>("vkEnumeratePhysicalDevices", vk.instance);
        if (!enumerate) return VK_ERROR_INITIALIZATION_FAILED;
        if (!vk.deviceCount) {
            std::array<VkPhysicalDevice, 32> host{};
            uint32_t size = static_cast<uint32_t>(host.size());
            const auto result = enumerate(vk.instance, &size, host.data());
            if (result != VK_SUCCESS) return result == VK_INCOMPLETE ? VK_ERROR_INITIALIZATION_FAILED : result;
            for (uint32_t i = 0; i < size; ++i)
                vk.devices[i] = {host[i], reinterpret_cast<VkPhysicalDevice>(nextHandle++)};
            vk.deviceCount = size;
        }
        std::array<VkPhysicalDevice, 32> tokens{};
        for (uint32_t i = 0; i < vk.deviceCount; ++i) tokens[i] = vk.devices[i].guest;
        return Enumerate(vk.deviceCount, tokens.data(), count, out);
    });
}

/// Queries the real physical-device queue family table.
void VKAPI_CALL Queues(VkPhysicalDevice device, uint32_t* count, VkQueueFamilyProperties* out) {
    Run([=] {
        if (!count) return VK_ERROR_INITIALIZATION_FAILED;
        auto physical = Physical(device);
        if (!physical) { *count = 0; return VK_ERROR_INITIALIZATION_FAILED; }
        Driver<PFN_vkGetPhysicalDeviceQueueFamilyProperties>("vkGetPhysicalDeviceQueueFamilyProperties", vk.instance)(physical, count, out);
        return VK_SUCCESS;
    });
}

/// Creates a native Win32 surface from one live, retained Android native window.
VkResult VKAPI_CALL CreateSurface(VkInstance instance, const VkAndroidSurfaceCreateInfoKHR* info,
                                  const VkAllocationCallbacks* allocator, VkSurfaceKHR* out) {
    if (!out) return VK_ERROR_INITIALIZATION_FAILED;
    *out = VK_NULL_HANDLE;
    if (!info || info->sType != VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR ||
        info->pNext || info->flags || allocator) return VK_ERROR_FEATURE_NOT_PRESENT;
    return Run([&] {
        if (!instance || instance != vk.guestInstance) return VK_ERROR_INITIALIZATION_FAILED;
        if (!vk.androidSurfaceExtension) return VK_ERROR_EXTENSION_NOT_PRESENT;
        if (vk.surface || GuestGraphicsActive() || GuestGraphicsRequest() != "vulkan") return VK_ERROR_NATIVE_WINDOW_IN_USE_KHR;
        HWND window;
        int width, height;
        if (!ResolveSurface(info->window, window, width, height) || !RetainSurface(info->window))
            return VK_ERROR_SURFACE_LOST_KHR;
        VkWin32SurfaceCreateInfoKHR win{VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR};
        win.hwnd = window;
        win.hinstance = GetModuleHandleW(nullptr);
        auto create = Driver<PFN_vkCreateWin32SurfaceKHR>("vkCreateWin32SurfaceKHR", vk.instance);
        const auto result = create ? create(vk.instance, &win, nullptr, &vk.surface) : VK_ERROR_EXTENSION_NOT_PRESENT;
        if (result != VK_SUCCESS) {
            ReleaseSurface(info->window);
            return result;
        }
        vk.window = window;
        vk.native = info->window;
        *out = vk.guestSurface = reinterpret_cast<VkSurfaceKHR>(nextHandle++);
        return VK_SUCCESS;
    });
}

/// Rejects results from a wrong or retired surface before any native driver query.
template<class F> VkResult WithSurface(VkPhysicalDevice d, VkSurfaceKHR s, F&& fn) {
    return Run([&] {
        auto physical = Physical(d);
        if (!physical) return VK_ERROR_INITIALIZATION_FAILED;
        HWND w;
        int width, height;
        if (!s || s != vk.guestSurface || vk.retired || !ResolveSurface(vk.native, w, width, height) || w != vk.window)
            return VK_ERROR_SURFACE_LOST_KHR;
        return fn(physical, width, height);
    });
}

/// Returns actual present support for a checked physical device and queue family.
VkResult VKAPI_CALL PresentSupport(VkPhysicalDevice d, uint32_t queue, VkSurfaceKHR s, VkBool32* out) {
    if (!out) return VK_ERROR_INITIALIZATION_FAILED;
    return WithSurface(d, s, [&](VkPhysicalDevice physical, int, int) {
        uint32_t count = 0;
        Driver<PFN_vkGetPhysicalDeviceQueueFamilyProperties>("vkGetPhysicalDeviceQueueFamilyProperties", vk.instance)(physical, &count, nullptr);
        if (queue >= count) return VK_ERROR_INITIALIZATION_FAILED;
        return Driver<PFN_vkGetPhysicalDeviceSurfaceSupportKHR>("vkGetPhysicalDeviceSurfaceSupportKHR", vk.instance)
            (physical, queue, vk.surface, out);
    });
}

/// Queries native capabilities, using the managed extent for minimized windows.
VkResult VKAPI_CALL Capabilities(VkPhysicalDevice d, VkSurfaceKHR s, VkSurfaceCapabilitiesKHR* out) {
    if (!out) return VK_ERROR_INITIALIZATION_FAILED;
    return WithSurface(d, s, [&](VkPhysicalDevice physical, int width, int height) {
        const auto result = Driver<PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR>("vkGetPhysicalDeviceSurfaceCapabilitiesKHR", vk.instance)
            (physical, vk.surface, out);
        if (result == VK_SUCCESS && (!width || !height)) out->currentExtent = {0,0};
        return result;
    });
}

/// Enumerates formats for the exact native surface.
VkResult VKAPI_CALL Formats(VkPhysicalDevice d, VkSurfaceKHR s, uint32_t* count, VkSurfaceFormatKHR* out) {
    if (!count) return VK_ERROR_INITIALIZATION_FAILED;
    return WithSurface(d, s, [&](VkPhysicalDevice physical, int, int) {
        return Driver<PFN_vkGetPhysicalDeviceSurfaceFormatsKHR>("vkGetPhysicalDeviceSurfaceFormatsKHR", vk.instance)
            (physical, vk.surface, count, out);
    });
}

/// Enumerates presentation modes for the exact native surface.
VkResult VKAPI_CALL PresentModes(VkPhysicalDevice d, VkSurfaceKHR s, uint32_t* count, VkPresentModeKHR* out) {
    if (!count) return VK_ERROR_INITIALIZATION_FAILED;
    return WithSurface(d, s, [&](VkPhysicalDevice physical, int, int) {
        return Driver<PFN_vkGetPhysicalDeviceSurfacePresentModesKHR>("vkGetPhysicalDeviceSurfacePresentModesKHR", vk.instance)
            (physical, vk.surface, count, out);
    });
}

/// Destroys only the named surface owned by the named instance, including an invalidated surface.
void VKAPI_CALL DestroySurface(VkInstance i, VkSurfaceKHR s, const VkAllocationCallbacks* allocator) {
    if (!s) return;
    Run([=] {
        if (!allocator && i == vk.guestInstance && s == vk.guestSurface) { DropSurface(); return VK_SUCCESS; }
        LogLine("GRAPHICS_CAPABILITY_MISSING: guest Vulkan invalid surface destruction");
        return VK_ERROR_INITIALIZATION_FAILED;
    });
}

/// Destroys the named instance after all children have been released.
void VKAPI_CALL DestroyInstance(VkInstance i, const VkAllocationCallbacks* allocator) {
    if (!i) return;
    Run([=] {
        if (!allocator && i == vk.guestInstance && !vk.surface) { Shutdown(); return VK_SUCCESS; }
        LogLine("GRAPHICS_CAPABILITY_MISSING: guest Vulkan invalid instance destruction (live surface or foreign handle)");
        return VK_ERROR_INITIALIZATION_FAILED;
    });
}

PFN_vkVoidFunction VKAPI_CALL Proc(VkInstance instance, const char* name);

/// Describes one audited fixed-signature import and its resolution scope.
enum class Scope { Global, Instance, Surface, AndroidSurface };
struct Entry { guest::HostSymbol symbol; Scope scope; };
#define ENTRY(name, fn, scope) {{#name, reinterpret_cast<void*>(&fn)}, Scope::scope}
const Entry entries[] = {
    ENTRY(vkGetInstanceProcAddr, Proc, Global),
    ENTRY(vkEnumerateInstanceVersion, Version, Global),
    ENTRY(vkEnumerateInstanceLayerProperties, Layers, Global),
    ENTRY(vkEnumerateInstanceExtensionProperties, Extensions, Global),
    ENTRY(vkCreateInstance, CreateInstance, Global),
    ENTRY(vkDestroyInstance, DestroyInstance, Instance),
    ENTRY(vkEnumeratePhysicalDevices, PhysicalDevices, Instance),
    ENTRY(vkGetPhysicalDeviceQueueFamilyProperties, Queues, Instance),
    ENTRY(vkCreateAndroidSurfaceKHR, CreateSurface, AndroidSurface),
    ENTRY(vkDestroySurfaceKHR, DestroySurface, Surface),
    ENTRY(vkGetPhysicalDeviceSurfaceSupportKHR, PresentSupport, Surface),
    ENTRY(vkGetPhysicalDeviceSurfaceCapabilitiesKHR, Capabilities, Surface),
    ENTRY(vkGetPhysicalDeviceSurfaceFormatsKHR, Formats, Surface),
    ENTRY(vkGetPhysicalDeviceSurfacePresentModesKHR, PresentModes, Surface),
};
#undef ENTRY

/// Resolves only a declared bridge entry, never a driver pointer.
PFN_vkVoidFunction VKAPI_CALL Proc(VkInstance instance, const char* name) {
    if (!name || strnlen(name, 128) == 128) return nullptr;
    for (const auto& entry : entries) {
        if (std::strcmp(entry.symbol.name, name) != 0) continue;
        if (entry.scope == Scope::Global && !instance) return reinterpret_cast<PFN_vkVoidFunction>(entry.symbol.address);
        if (!instance) return nullptr;
        return Run([&] {
            return instance == vk.guestInstance &&
                (entry.scope != Scope::Surface || vk.surfaceExtension) &&
                (entry.scope != Scope::AndroidSurface || vk.androidSurfaceExtension)
                ? VK_SUCCESS : VK_ERROR_INITIALIZATION_FAILED;
        }) == VK_SUCCESS ? reinterpret_cast<PFN_vkVoidFunction>(entry.symbol.address) : nullptr;
    }
    return nullptr;
}

} // namespace

void AddGuestVulkanLibrary(std::vector<guest::BuiltinLibrary>& libraries) {
    guest::BuiltinLibrary library{"libvulkan.so", {}, ""};
    for (const auto& entry : entries) library.symbols.push_back(entry.symbol);
    libraries.push_back(std::move(library));
}

bool GuestVulkanOwnsWindow() { return vk.surface && !vk.retired; }

void RetireGuestVulkan(HWND window, bool closing) {
    if (closing && (!vk.window || vk.window == window)) Shutdown();
    else if (vk.window == window) vk.retired = true;
}

} // namespace winapk::bridge
