// Restricted Android Vulkan instance/WSI provider. All native handles stay on the window thread.
#include "guest_vulkan.h"
#include "guest_graphics.h"
#include "bridge.h"
#include "embed.h"

#define VK_USE_PLATFORM_WIN32_KHR
#define VK_USE_PLATFORM_ANDROID_KHR
#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstring>
#include <new>
#include <type_traits>
#include <utility>

namespace winapk::bridge {
namespace {

static_assert(sizeof(VkInstance) == 8 && sizeof(VkPhysicalDevice) == 8 && sizeof(VkSurfaceKHR) == 8);
static_assert(sizeof(VkApplicationInfo) == 48 && sizeof(VkInstanceCreateInfo) == 64 &&
              sizeof(VkAndroidSurfaceCreateInfoKHR) == 32 && offsetof(VkAndroidSurfaceCreateInfoKHR, window) == 24);
static_assert(sizeof(VkExtensionProperties) == 260 && sizeof(VkQueueFamilyProperties) == 24 &&
              sizeof(VkSurfaceCapabilitiesKHR) == 52 && sizeof(VkSurfaceFormatKHR) == 8);
static_assert(sizeof(VkDeviceQueueCreateInfo) == 40 && sizeof(VkDeviceCreateInfo) == 72 &&
              sizeof(VkSwapchainCreateInfoKHR) == 104 && sizeof(VkImageMemoryBarrier) == 72 &&
              sizeof(VkSubmitInfo) == 72 && sizeof(VkPresentInfoKHR) == 64);
static_assert(sizeof(VkBufferCreateInfo) == 56 && sizeof(VkMemoryAllocateInfo) == 32 &&
              sizeof(VkMemoryRequirements) == 24 && sizeof(VkPhysicalDeviceMemoryProperties) == 520 &&
              sizeof(VkBufferMemoryBarrier) == 56);

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
    bool presented = false;
    struct Device { VkPhysicalDevice host, guest; };
    std::array<Device, 32> devices{};
    uint32_t deviceCount = 0;
    VkDevice device = VK_NULL_HANDLE, guestDevice = VK_NULL_HANDLE;
    PFN_vkGetDeviceProcAddr deviceProc = nullptr;
    VkPhysicalDevice devicePhysical = VK_NULL_HANDLE;
    VkQueue queue = VK_NULL_HANDLE, guestQueue = VK_NULL_HANDLE;
    uint32_t family = 0;
    VkSwapchainKHR swapchain = VK_NULL_HANDLE, guestSwapchain = VK_NULL_HANDLE;
    VkExtent2D extent{};
    VkResult swapchainError = VK_SUCCESS;
    struct Image { VkImage host, guest; };
    std::array<Image, 16> images{};
    uint32_t imageCount = 0, imageIndex = UINT32_MAX;
    VkCommandPool pool = VK_NULL_HANDLE, guestPool = VK_NULL_HANDLE;
    VkCommandBuffer command = VK_NULL_HANDLE, guestCommand = VK_NULL_HANDLE;
    VkFence fence = VK_NULL_HANDLE, guestFence = VK_NULL_HANDLE;
    VkBuffer buffer = VK_NULL_HANDLE, guestBuffer = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE, guestMemory = VK_NULL_HANDLE;
    VkDeviceSize bufferSize = 0;
    VkMemoryRequirements memoryRequirements{};
    bool bound = false, mapped = false, bufferReadable = false;
    enum class Frame { Idle, Acquired, Ready, Recording, Executable, Submitted, Complete, Invalid };
    Frame frame = Frame::Idle;
    unsigned commands = 0;
} vk;
uintptr_t nextHandle = 1;
using Frame = Vulkan::Frame;
constexpr uint64_t kWaitNs = 1000000000; // at most one second per guest-requested wait on the HWND thread
constexpr VkDeviceSize kMaxBufferSize = 64 * 1024;

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

/// Resolves a device-side driver entry privately against the owned device.
template<class P> P DeviceDriver(const char* name) {
    return reinterpret_cast<P>(vk.deviceProc(vk.device, name));
}

/// Releases the one transfer buffer after the queue is drained or before any work is recorded.
void DropBuffer() {
    if (!vk.buffer) return;
    DeviceDriver<PFN_vkDestroyBuffer>("vkDestroyBuffer")(vk.device, vk.buffer, nullptr);
    vk.buffer = vk.guestBuffer = VK_NULL_HANDLE;
    vk.bufferSize = 0;
    vk.memoryRequirements = {};
    vk.bound = vk.bufferReadable = false;
}

/// Unmaps and releases the allocation once its buffer has been released.
void DropMemory() {
    if (!vk.memory) return;
    if (vk.mapped) DeviceDriver<PFN_vkUnmapMemory>("vkUnmapMemory")(vk.device, vk.memory);
    DeviceDriver<PFN_vkFreeMemory>("vkFreeMemory")(vk.device, vk.memory, nullptr);
    vk.memory = vk.guestMemory = VK_NULL_HANDLE;
    vk.mapped = false;
}

/// Drains the one queue before destroying the backing swapchain and invalidating its tokens.
void DropSwapchain() {
    if (!vk.swapchain) return;
    DeviceDriver<PFN_vkQueueWaitIdle>("vkQueueWaitIdle")(vk.queue);
    DeviceDriver<PFN_vkDestroySwapchainKHR>("vkDestroySwapchainKHR")(vk.device, vk.swapchain, nullptr);
    vk.swapchain = vk.guestSwapchain = VK_NULL_HANDLE;
    vk.imageCount = 0;
    vk.imageIndex = UINT32_MAX;
    vk.frame = Vulkan::Frame::Idle;
    vk.presented = false;
    vk.swapchainError = VK_SUCCESS;
}

/// Destroys device children in dependency order, including on unexpected window close.
bool DropDevice() {
    if (!vk.device) return true;
    if (vk.frame == Frame::Acquired) {
        const auto result = DeviceDriver<PFN_vkWaitForFences>("vkWaitForFences")
            (vk.device, 1, &vk.fence, VK_TRUE, kWaitNs);
        if (result != VK_SUCCESS && result != VK_ERROR_DEVICE_LOST) {
            LogLine("GRAPHICS_RENDER_FAILED: guest Vulkan acquire fence still pending at close; leaving driver objects to process exit");
            return false;
        }
    }
    DropSwapchain();
    if (vk.pool) DeviceDriver<PFN_vkDestroyCommandPool>("vkDestroyCommandPool")(vk.device, vk.pool, nullptr);
    if (vk.fence) DeviceDriver<PFN_vkDestroyFence>("vkDestroyFence")(vk.device, vk.fence, nullptr);
    DropBuffer();
    DropMemory();
    DeviceDriver<PFN_vkDestroyDevice>("vkDestroyDevice")(vk.device, nullptr);
    vk.device = vk.guestDevice = VK_NULL_HANDLE;
    vk.deviceProc = nullptr;
    vk.queue = vk.guestQueue = VK_NULL_HANDLE;
    vk.pool = vk.guestPool = VK_NULL_HANDLE;
    vk.command = vk.guestCommand = VK_NULL_HANDLE;
    vk.fence = vk.guestFence = VK_NULL_HANDLE;
    return true;
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
    if (!DropDevice()) return;
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
        if (!allocator && i == vk.guestInstance && s == vk.guestSurface && !vk.swapchain) { DropSurface(); return VK_SUCCESS; }
        LogLine("GRAPHICS_CAPABILITY_MISSING: guest Vulkan invalid surface destruction");
        return VK_ERROR_INITIALIZATION_FAILED;
    });
}

/// Destroys the named instance after all children have been released.
void VKAPI_CALL DestroyInstance(VkInstance i, const VkAllocationCallbacks* allocator) {
    if (!i) return;
    Run([=] {
        if (!allocator && i == vk.guestInstance && !vk.surface && !vk.device) { Shutdown(); return VK_SUCCESS; }
        LogLine("GRAPHICS_CAPABILITY_MISSING: guest Vulkan invalid instance destruction (live surface or foreign handle)");
        return VK_ERROR_INITIALIZATION_FAILED;
    });
}

/// Lists only the swapchain extension, and only on devices whose ICD supports it.
VkResult VKAPI_CALL DeviceExtensions(VkPhysicalDevice d, const char* layer, uint32_t* count, VkExtensionProperties* out) {
    if (layer) return VK_ERROR_LAYER_NOT_PRESENT;
    return Run([=] {
        auto physical = Physical(d);
        if (!physical) return VK_ERROR_INITIALIZATION_FAILED;
        std::array<VkExtensionProperties, 512> all{};
        uint32_t n = static_cast<uint32_t>(all.size());
        const auto status = Driver<PFN_vkEnumerateDeviceExtensionProperties>("vkEnumerateDeviceExtensionProperties", vk.instance)
            (physical, nullptr, &n, all.data());
        if (status != VK_SUCCESS) return status;
        for (uint32_t i = 0; i < n; ++i)
            if (!std::strcmp(all[i].extensionName, VK_KHR_SWAPCHAIN_EXTENSION_NAME))
                return Enumerate(1, &all[i], count, out);
        return Enumerate<VkExtensionProperties>(0, nullptr, count, out);
    });
}

/// Creates the sole device, with no feature chain and exactly one graphics/present queue.
VkResult VKAPI_CALL CreateDevice(VkPhysicalDevice d, const VkDeviceCreateInfo* info,
                                const VkAllocationCallbacks* allocator, VkDevice* out) {
    if (!out) return VK_ERROR_INITIALIZATION_FAILED;
    *out = VK_NULL_HANDLE;
    if (!info || allocator || info->sType != VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO || info->pNext ||
        info->flags || info->pEnabledFeatures || info->queueCreateInfoCount != 1 || !info->pQueueCreateInfos)
        return VK_ERROR_FEATURE_NOT_PRESENT;
    if (info->enabledLayerCount) return VK_ERROR_LAYER_NOT_PRESENT;
    if (info->enabledExtensionCount != 1 || !info->ppEnabledExtensionNames ||
        !info->ppEnabledExtensionNames[0] || std::strcmp(info->ppEnabledExtensionNames[0], VK_KHR_SWAPCHAIN_EXTENSION_NAME))
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    const auto& q = info->pQueueCreateInfos[0];
    if (q.sType != VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO || q.pNext || q.flags ||
        q.queueCount != 1 || !q.pQueuePriorities || !(q.pQueuePriorities[0] >= 0.f && q.pQueuePriorities[0] <= 1.f))
        return VK_ERROR_FEATURE_NOT_PRESENT;
    return Run([&] {
        const auto physical = Physical(d);
        HWND window;
        int w, h;
        if (!physical) return VK_ERROR_INITIALIZATION_FAILED;
        if (!vk.surface || vk.retired || !ResolveSurface(vk.native, window, w, h) || window != vk.window)
            return VK_ERROR_SURFACE_LOST_KHR;
        if (vk.device) return VK_ERROR_TOO_MANY_OBJECTS;
        std::array<VkQueueFamilyProperties, 64> families{};
        uint32_t count = static_cast<uint32_t>(families.size());
        auto properties = Driver<PFN_vkGetPhysicalDeviceQueueFamilyProperties>("vkGetPhysicalDeviceQueueFamilyProperties", vk.instance);
        properties(physical, &count, families.data());
        if (q.queueFamilyIndex >= count || !(families[q.queueFamilyIndex].queueFlags & VK_QUEUE_GRAPHICS_BIT))
            return VK_ERROR_FEATURE_NOT_PRESENT;
        VkBool32 support = VK_FALSE;
        auto result = Driver<PFN_vkGetPhysicalDeviceSurfaceSupportKHR>("vkGetPhysicalDeviceSurfaceSupportKHR", vk.instance)
            (physical, q.queueFamilyIndex, vk.surface, &support);
        if (result != VK_SUCCESS || !support) return result != VK_SUCCESS ? result : VK_ERROR_FEATURE_NOT_PRESENT;
        auto proc = Driver<PFN_vkGetDeviceProcAddr>("vkGetDeviceProcAddr", vk.instance);
        if (!proc) return VK_ERROR_INITIALIZATION_FAILED;
        result = Driver<PFN_vkCreateDevice>("vkCreateDevice", vk.instance)(physical, info, nullptr, &vk.device);
        if (result != VK_SUCCESS) return result;
        vk.deviceProc = proc;
        vk.family = q.queueFamilyIndex;
        vk.devicePhysical = physical;
        DeviceDriver<PFN_vkGetDeviceQueue>("vkGetDeviceQueue")(vk.device, vk.family, 0, &vk.queue);
        vk.guestQueue = reinterpret_cast<VkQueue>(nextHandle++);
        *out = vk.guestDevice = reinterpret_cast<VkDevice>(nextHandle++);
        return VK_SUCCESS;
    });
}

/// Returns only the queue owned by the one configured device.
void VKAPI_CALL GetQueue(VkDevice d, uint32_t family, uint32_t index, VkQueue* out) {
    if (!out) return;
    *out = VK_NULL_HANDLE;
    Run([=] {
        if (d && d == vk.guestDevice && family == vk.family && index == 0) *out = vk.guestQueue;
        return VK_SUCCESS;
    });
}

/// Destroys the device after its guest-visible children have been retired.
void VKAPI_CALL DestroyDevice(VkDevice d, const VkAllocationCallbacks* allocator) {
    Run([=] {
        if (!allocator && d && d == vk.guestDevice && !vk.swapchain && !vk.pool && !vk.fence && !vk.buffer && !vk.memory)
            DropDevice();
        else LogLine("GRAPHICS_CAPABILITY_MISSING: guest Vulkan invalid device destruction");
        return VK_SUCCESS;
    });
}

/// Reports the ICD's memory types with their original indices for the transfer allocation.
void VKAPI_CALL MemoryProperties(VkPhysicalDevice d, VkPhysicalDeviceMemoryProperties* out) {
    if (!out) return;
    *out = {};
    Run([=] {
        const auto physical = Physical(d);
        if (!physical) return VK_ERROR_INITIALIZATION_FAILED;
        Driver<PFN_vkGetPhysicalDeviceMemoryProperties>("vkGetPhysicalDeviceMemoryProperties", vk.instance)(physical, out);
        return VK_SUCCESS;
    });
}

/// Creates one exclusive bounded transfer-destination buffer and captures its requirements.
VkResult VKAPI_CALL CreateBuffer(VkDevice d, const VkBufferCreateInfo* info, const VkAllocationCallbacks* allocator, VkBuffer* out) {
    if (!out) return VK_ERROR_INITIALIZATION_FAILED;
    *out = VK_NULL_HANDLE;
    if (!info || info->sType != VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO || info->pNext || info->flags || allocator ||
        info->size == 0 || info->size > kMaxBufferSize || info->size % 4 || info->usage != VK_BUFFER_USAGE_TRANSFER_DST_BIT ||
        info->sharingMode != VK_SHARING_MODE_EXCLUSIVE || info->queueFamilyIndexCount) return VK_ERROR_FEATURE_NOT_PRESENT;
    return Run([=] {
        if (!d || d != vk.guestDevice || vk.buffer || vk.memory || vk.frame != Frame::Idle)
            return VK_ERROR_INITIALIZATION_FAILED;
        auto result = DeviceDriver<PFN_vkCreateBuffer>("vkCreateBuffer")(vk.device, info, nullptr, &vk.buffer);
        if (result != VK_SUCCESS) return result;
        DeviceDriver<PFN_vkGetBufferMemoryRequirements>("vkGetBufferMemoryRequirements")(vk.device, vk.buffer, &vk.memoryRequirements);
        vk.bufferSize = info->size;
        *out = vk.guestBuffer = reinterpret_cast<VkBuffer>(nextHandle++);
        return VK_SUCCESS;
    });
}

/// Copies the real requirements for the named buffer; returns zero for a stale token.
void VKAPI_CALL BufferRequirements(VkDevice d, VkBuffer buffer, VkMemoryRequirements* out) {
    if (!out) return;
    *out = {};
    Run([=] {
        if (d && d == vk.guestDevice && buffer && buffer == vk.guestBuffer) *out = vk.memoryRequirements;
        return VK_SUCCESS;
    });
}

/// Allocates only the buffer's required size from a compatible host-visible coherent memory type.
VkResult VKAPI_CALL AllocateMemory(VkDevice d, const VkMemoryAllocateInfo* info, const VkAllocationCallbacks* allocator, VkDeviceMemory* out) {
    if (!out) return VK_ERROR_INITIALIZATION_FAILED;
    *out = VK_NULL_HANDLE;
    if (!info || info->sType != VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO || info->pNext || allocator)
        return VK_ERROR_FEATURE_NOT_PRESENT;
    return Run([=] {
        if (!d || d != vk.guestDevice || !vk.buffer || vk.memory || vk.frame != Frame::Idle)
            return VK_ERROR_INITIALIZATION_FAILED;
        VkPhysicalDeviceMemoryProperties props{};
        Driver<PFN_vkGetPhysicalDeviceMemoryProperties>("vkGetPhysicalDeviceMemoryProperties", vk.instance)
            (vk.devicePhysical, &props);
        const auto i = info->memoryTypeIndex;
        constexpr auto required = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        if (info->allocationSize != vk.memoryRequirements.size || i >= props.memoryTypeCount || i >= 32 ||
            !(vk.memoryRequirements.memoryTypeBits & (uint32_t{1} << i)) ||
            (props.memoryTypes[i].propertyFlags & required) != required) return VK_ERROR_FEATURE_NOT_PRESENT;
        const auto result = DeviceDriver<PFN_vkAllocateMemory>("vkAllocateMemory")(vk.device, info, nullptr, &vk.memory);
        if (result == VK_SUCCESS) *out = vk.guestMemory = reinterpret_cast<VkDeviceMemory>(nextHandle++);
        return result;
    });
}

/// Binds the exact compatible allocation at offset zero.
VkResult VKAPI_CALL BindBuffer(VkDevice d, VkBuffer buffer, VkDeviceMemory memory, VkDeviceSize offset) {
    return Run([=] {
        if (!d || d != vk.guestDevice || !buffer || buffer != vk.guestBuffer || !memory || memory != vk.guestMemory ||
            vk.bound || offset || vk.frame != Frame::Idle) return VK_ERROR_FEATURE_NOT_PRESENT;
        const auto result = DeviceDriver<PFN_vkBindBufferMemory>("vkBindBufferMemory")(vk.device, vk.buffer, vk.memory, 0);
        vk.bound = result == VK_SUCCESS;
        return result;
    });
}

/// Maps the logical written range only after the GPU-to-host barrier and queue completion.
VkResult VKAPI_CALL MapMemory(VkDevice d, VkDeviceMemory memory, VkDeviceSize offset, VkDeviceSize size, VkMemoryMapFlags flags, void** out) {
    if (!out) return VK_ERROR_MEMORY_MAP_FAILED;
    *out = nullptr;
    return Run([=] {
        if (!d || d != vk.guestDevice || !memory || memory != vk.guestMemory || !vk.bufferReadable || vk.mapped ||
            (vk.frame != Frame::Idle && vk.frame != Frame::Complete) || offset || size != vk.bufferSize || flags)
            return VK_ERROR_MEMORY_MAP_FAILED;
        const auto result = DeviceDriver<PFN_vkMapMemory>("vkMapMemory")(vk.device, vk.memory, 0, size, 0, out);
        vk.mapped = result == VK_SUCCESS;
        return result;
    });
}

/// Ends access to the named mapped allocation before GPU writes may resume.
void VKAPI_CALL UnmapMemory(VkDevice d, VkDeviceMemory memory) {
    Run([=] {
        if (d && d == vk.guestDevice && memory && memory == vk.guestMemory && vk.mapped) {
            DeviceDriver<PFN_vkUnmapMemory>("vkUnmapMemory")(vk.device, vk.memory);
            vk.mapped = false;
        }
        return VK_SUCCESS;
    });
}

/// Destroys the transfer buffer only when no recording or pending submit can use it.
void VKAPI_CALL DestroyBuffer(VkDevice d, VkBuffer buffer, const VkAllocationCallbacks* allocator) {
    Run([=] {
        if (d && d == vk.guestDevice && buffer && buffer == vk.guestBuffer && !allocator && !vk.mapped &&
            vk.frame == Frame::Idle) DropBuffer();
        return VK_SUCCESS;
    });
}

/// Frees the allocation only after the linked buffer has been destroyed.
void VKAPI_CALL FreeMemory(VkDevice d, VkDeviceMemory memory, const VkAllocationCallbacks* allocator) {
    Run([=] {
        if (d && d == vk.guestDevice && memory && memory == vk.guestMemory && !allocator && !vk.bound && !vk.mapped &&
            vk.frame == Frame::Idle) DropMemory();
        return VK_SUCCESS;
    });
}

/// Resolves the live managed extent without allowing a stale or detached window into the ICD.
VkResult SurfaceExtent(VkExtent2D& extent) {
    HWND window;
    int w, h;
    if (!vk.surface || vk.retired || !ResolveSurface(vk.native, window, w, h) || window != vk.window)
        return VK_ERROR_SURFACE_LOST_KHR;
    extent = {static_cast<uint32_t>(w), static_cast<uint32_t>(h)};
    return VK_SUCCESS;
}

/// Detects resize/minimize without replacing the caller's swapchain silently.
VkResult CheckSwapchain() {
    VkExtent2D extent;
    const auto result = SurfaceExtent(extent);
    if (result != VK_SUCCESS) return result;
    if (vk.swapchainError != VK_SUCCESS) return vk.swapchainError;
    if (extent.width != vk.extent.width || extent.height != vk.extent.height)
        vk.swapchainError = VK_ERROR_OUT_OF_DATE_KHR;
    return vk.swapchainError;
}

/// Creates a FIFO transfer-destination swapchain on the exact current client extent.
VkResult VKAPI_CALL CreateSwapchain(VkDevice d, const VkSwapchainCreateInfoKHR* info,
                                   const VkAllocationCallbacks* allocator, VkSwapchainKHR* out) {
    if (!out) return VK_ERROR_INITIALIZATION_FAILED;
    *out = VK_NULL_HANDLE;
    if (!info || allocator || info->sType != VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR || info->pNext ||
        info->flags || info->imageArrayLayers != 1 || info->imageUsage != VK_IMAGE_USAGE_TRANSFER_DST_BIT ||
        info->imageSharingMode != VK_SHARING_MODE_EXCLUSIVE || info->queueFamilyIndexCount || info->oldSwapchain ||
        info->presentMode != VK_PRESENT_MODE_FIFO_KHR) return VK_ERROR_FEATURE_NOT_PRESENT;
    if (info->imageFormat != VK_FORMAT_B8G8R8A8_UNORM && info->imageFormat != VK_FORMAT_R8G8B8A8_UNORM)
        return VK_ERROR_FORMAT_NOT_SUPPORTED;
    return Run([=] {
        if (!d || d != vk.guestDevice || vk.swapchain) return VK_ERROR_INITIALIZATION_FAILED;
        if (!info->surface || info->surface != vk.guestSurface) return VK_ERROR_SURFACE_LOST_KHR;
        VkExtent2D extent;
        auto result = SurfaceExtent(extent);
        if (result != VK_SUCCESS) return result;
        if (!extent.width || !extent.height || info->imageExtent.width != extent.width || info->imageExtent.height != extent.height)
            return VK_ERROR_OUT_OF_DATE_KHR;
        VkSurfaceCapabilitiesKHR caps{};
        result = Driver<PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR>("vkGetPhysicalDeviceSurfaceCapabilitiesKHR", vk.instance)
            (vk.devicePhysical, vk.surface, &caps);
        if (result != VK_SUCCESS) return result;
        if (!(caps.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT) ||
            !(caps.supportedTransforms & info->preTransform) || !(caps.supportedCompositeAlpha & info->compositeAlpha) ||
            info->minImageCount < caps.minImageCount || (caps.maxImageCount && info->minImageCount > caps.maxImageCount))
            return VK_ERROR_FEATURE_NOT_PRESENT;
        uint32_t count = 64;
        VkSurfaceFormatKHR formats[64];
        result = Driver<PFN_vkGetPhysicalDeviceSurfaceFormatsKHR>("vkGetPhysicalDeviceSurfaceFormatsKHR", vk.instance)
            (vk.devicePhysical, vk.surface, &count, formats);
        if (result != VK_SUCCESS) return result;
        bool match = false;
        for (uint32_t i = 0; i < count; ++i)
            match |= formats[i].format == info->imageFormat && formats[i].colorSpace == info->imageColorSpace;
        if (!match) return VK_ERROR_FORMAT_NOT_SUPPORTED;
        auto host = *info;
        host.surface = vk.surface;
        result = DeviceDriver<PFN_vkCreateSwapchainKHR>("vkCreateSwapchainKHR")(vk.device, &host, nullptr, &vk.swapchain);
        if (result != VK_SUCCESS) return result;
        std::array<VkImage, 16> images{};
        vk.imageCount = static_cast<uint32_t>(images.size());
        result = DeviceDriver<PFN_vkGetSwapchainImagesKHR>("vkGetSwapchainImagesKHR")
            (vk.device, vk.swapchain, &vk.imageCount, images.data());
        if (result != VK_SUCCESS || !vk.imageCount) {
            DropSwapchain();
            return VK_ERROR_INITIALIZATION_FAILED;
        }
        for (uint32_t i = 0; i < vk.imageCount; ++i) vk.images[i] = {images[i], reinterpret_cast<VkImage>(nextHandle++)};
        vk.extent = extent;
        *out = vk.guestSwapchain = reinterpret_cast<VkSwapchainKHR>(nextHandle++);
        return VK_SUCCESS;
    });
}

/// Lists only the opaque images of the named swapchain.
VkResult VKAPI_CALL Images(VkDevice d, VkSwapchainKHR s, uint32_t* count, VkImage* out) {
    return Run([=] {
        if (!d || d != vk.guestDevice || !s || s != vk.guestSwapchain) return VK_ERROR_INITIALIZATION_FAILED;
        std::array<VkImage, 16> tokens{};
        for (uint32_t i = 0; i < vk.imageCount; ++i) tokens[i] = vk.images[i].guest;
        return Enumerate(vk.imageCount, tokens.data(), count, out);
    });
}

/// Destroys the named swapchain; driver commands have been drained before tokens are invalidated.
void VKAPI_CALL DestroySwapchain(VkDevice d, VkSwapchainKHR s, const VkAllocationCallbacks* allocator) {
    Run([=] {
        if (!allocator && d && d == vk.guestDevice && s && s == vk.guestSwapchain &&
            (vk.frame == Frame::Idle || vk.frame == Frame::Ready || vk.frame == Frame::Complete)) DropSwapchain();
        else LogLine("GRAPHICS_CAPABILITY_MISSING: guest Vulkan invalid swapchain destruction");
        return VK_SUCCESS;
    });
}

/// Creates a real unsignaled fence for the one acquired image.
VkResult VKAPI_CALL CreateFence(VkDevice d, const VkFenceCreateInfo* info, const VkAllocationCallbacks* allocator, VkFence* out) {
    if (!out) return VK_ERROR_INITIALIZATION_FAILED;
    *out = VK_NULL_HANDLE;
    if (!info || allocator || info->sType != VK_STRUCTURE_TYPE_FENCE_CREATE_INFO || info->pNext || info->flags)
        return VK_ERROR_FEATURE_NOT_PRESENT;
    return Run([=] {
        if (!d || d != vk.guestDevice || vk.fence) return VK_ERROR_INITIALIZATION_FAILED;
        const auto result = DeviceDriver<PFN_vkCreateFence>("vkCreateFence")(vk.device, info, nullptr, &vk.fence);
        if (result == VK_SUCCESS) *out = vk.guestFence = reinterpret_cast<VkFence>(nextHandle++);
        return result;
    });
}

/// Waits for the real acquire fence and makes the image eligible for recording.
VkResult VKAPI_CALL WaitFence(VkDevice d, uint32_t n, const VkFence* fences, VkBool32 all, uint64_t timeout) {
    return Run([=] {
        if (!d || d != vk.guestDevice || n != 1 || !fences || !*fences || *fences != vk.guestFence || vk.frame != Frame::Acquired)
            return VK_ERROR_INITIALIZATION_FAILED;
        const auto result = DeviceDriver<PFN_vkWaitForFences>("vkWaitForFences")(vk.device, 1, &vk.fence, all, std::min(timeout, kWaitNs));
        if (result == VK_SUCCESS) vk.frame = Frame::Ready;
        return result;
    });
}

/// Resets the acquire fence only after the previous frame completed.
VkResult VKAPI_CALL ResetFence(VkDevice d, uint32_t n, const VkFence* fences) {
    return Run([=] {
        if (!d || d != vk.guestDevice || n != 1 || !fences || !*fences || *fences != vk.guestFence || vk.frame != Frame::Idle)
            return VK_ERROR_INITIALIZATION_FAILED;
        return DeviceDriver<PFN_vkResetFences>("vkResetFences")(vk.device, 1, &vk.fence);
    });
}

/// Destroys the acquire fence only when no acquire operation can still signal it.
void VKAPI_CALL DestroyFence(VkDevice d, VkFence fence, const VkAllocationCallbacks* allocator) {
    Run([=] {
        if (!allocator && d && d == vk.guestDevice && fence && fence == vk.guestFence && vk.frame != Frame::Acquired) {
            DeviceDriver<PFN_vkDestroyFence>("vkDestroyFence")(vk.device, vk.fence, nullptr);
            vk.fence = vk.guestFence = VK_NULL_HANDLE;
        }
        return VK_SUCCESS;
    });
}

/// Acquires one real image with a bounded wait and the caller's own fence token.
VkResult VKAPI_CALL AcquireImage(VkDevice d, VkSwapchainKHR s, uint64_t timeout, VkSemaphore semaphore, VkFence fence, uint32_t* index) {
    return Run([=] {
        if (!d || d != vk.guestDevice || !s || s != vk.guestSwapchain || !fence || fence != vk.guestFence ||
            !index || semaphore || vk.frame != Frame::Idle) return VK_ERROR_INITIALIZATION_FAILED;
        if (auto result = CheckSwapchain(); result != VK_SUCCESS) return result;
        if (DeviceDriver<PFN_vkGetFenceStatus>("vkGetFenceStatus")(vk.device, vk.fence) != VK_NOT_READY)
            return VK_ERROR_INITIALIZATION_FAILED;
        const auto result = DeviceDriver<PFN_vkAcquireNextImageKHR>("vkAcquireNextImageKHR")
            (vk.device, vk.swapchain, std::min(timeout, kWaitNs), VK_NULL_HANDLE, vk.fence, index);
        if (result == VK_SUCCESS || result == VK_SUBOPTIMAL_KHR) {
            vk.imageIndex = *index;
            vk.frame = Frame::Acquired;
        } else if (result < 0) vk.swapchainError = result;
        return result;
    });
}

/// Allocates the one command pool on the admitted queue family.
VkResult VKAPI_CALL CreatePool(VkDevice d, const VkCommandPoolCreateInfo* info, const VkAllocationCallbacks* allocator, VkCommandPool* out) {
    if (!out) return VK_ERROR_INITIALIZATION_FAILED;
    *out = VK_NULL_HANDLE;
    if (!info || allocator || info->sType != VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO || info->pNext ||
        info->flags != VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT) return VK_ERROR_FEATURE_NOT_PRESENT;
    return Run([=] {
        if (!d || d != vk.guestDevice || vk.pool || info->queueFamilyIndex != vk.family) return VK_ERROR_INITIALIZATION_FAILED;
        const auto result = DeviceDriver<PFN_vkCreateCommandPool>("vkCreateCommandPool")(vk.device, info, nullptr, &vk.pool);
        if (result == VK_SUCCESS) *out = vk.guestPool = reinterpret_cast<VkCommandPool>(nextHandle++);
        return result;
    });
}

/// Releases the command pool and its one buffer, never while a submission is pending.
void VKAPI_CALL DestroyPool(VkDevice d, VkCommandPool pool, const VkAllocationCallbacks* allocator) {
    Run([=] {
        if (!allocator && d && d == vk.guestDevice && pool && pool == vk.guestPool && vk.frame == Frame::Idle) {
            DeviceDriver<PFN_vkDestroyCommandPool>("vkDestroyCommandPool")(vk.device, vk.pool, nullptr);
            vk.pool = vk.guestPool = VK_NULL_HANDLE;
            vk.command = vk.guestCommand = VK_NULL_HANDLE;
        }
        return VK_SUCCESS;
    });
}

/// Allocates one primary command buffer without exposing a host pointer.
VkResult VKAPI_CALL AllocateCommand(VkDevice d, const VkCommandBufferAllocateInfo* info, VkCommandBuffer* out) {
    if (!out) return VK_ERROR_INITIALIZATION_FAILED;
    *out = VK_NULL_HANDLE;
    if (!info || info->sType != VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO || info->pNext ||
        info->level != VK_COMMAND_BUFFER_LEVEL_PRIMARY || info->commandBufferCount != 1) return VK_ERROR_FEATURE_NOT_PRESENT;
    return Run([=] {
        if (!d || d != vk.guestDevice || !info->commandPool || info->commandPool != vk.guestPool || vk.command)
            return VK_ERROR_INITIALIZATION_FAILED;
        auto host = *info;
        host.commandPool = vk.pool;
        const auto result = DeviceDriver<PFN_vkAllocateCommandBuffers>("vkAllocateCommandBuffers")(vk.device, &host, &vk.command);
        if (result == VK_SUCCESS) *out = vk.guestCommand = reinterpret_cast<VkCommandBuffer>(nextHandle++);
        return result;
    });
}

/// Begins recording after the image's acquire fence has signaled.
VkResult VKAPI_CALL BeginCommand(VkCommandBuffer cmd, const VkCommandBufferBeginInfo* info) {
    if (!info || info->sType != VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO || info->pNext || info->pInheritanceInfo ||
        info->flags != VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT) return VK_ERROR_FEATURE_NOT_PRESENT;
    return Run([=] {
        if (!cmd || cmd != vk.guestCommand || vk.frame != Frame::Ready) return VK_ERROR_INITIALIZATION_FAILED;
        const auto result = DeviceDriver<PFN_vkBeginCommandBuffer>("vkBeginCommandBuffer")(vk.command, info);
        if (result == VK_SUCCESS) { vk.frame = Frame::Recording; vk.commands = 0; }
        return result;
    });
}

/// Accepts exactly the single color subresource of the swapchain image.
bool ColorRange(const VkImageSubresourceRange& range) {
    return range.aspectMask == VK_IMAGE_ASPECT_COLOR_BIT && !range.baseMipLevel && range.levelCount == 1 &&
           !range.baseArrayLayer && range.layerCount == 1;
}

/// Records the admitted image transitions and the optional transfer-to-host buffer barrier.
void VKAPI_CALL Barrier(VkCommandBuffer cmd, VkPipelineStageFlags src, VkPipelineStageFlags dst, VkDependencyFlags flags,
    uint32_t memoryCount, const VkMemoryBarrier*, uint32_t bufferCount, const VkBufferMemoryBarrier* buffers,
    uint32_t count, const VkImageMemoryBarrier* barriers) {
    Run([=] {
        if (!cmd || cmd != vk.guestCommand || vk.frame != Frame::Recording) return VK_ERROR_INITIALIZATION_FAILED;
        if (vk.commands == 4 && !flags && !memoryCount && !count && bufferCount == 1 && buffers &&
            src == VK_PIPELINE_STAGE_TRANSFER_BIT && dst == VK_PIPELINE_STAGE_HOST_BIT) {
            auto host = buffers[0];
            if (host.sType == VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER && !host.pNext && host.buffer == vk.guestBuffer &&
                host.srcAccessMask == VK_ACCESS_TRANSFER_WRITE_BIT && host.dstAccessMask == VK_ACCESS_HOST_READ_BIT &&
                host.srcQueueFamilyIndex == VK_QUEUE_FAMILY_IGNORED && host.dstQueueFamilyIndex == VK_QUEUE_FAMILY_IGNORED &&
                !host.offset && host.size == vk.bufferSize) {
                host.buffer = vk.buffer;
                DeviceDriver<PFN_vkCmdPipelineBarrier>("vkCmdPipelineBarrier")(vk.command, src, dst, 0, 0, nullptr, 1, &host, 0, nullptr);
                ++vk.commands;
                return VK_SUCCESS;
            }
        }
        const auto& image = vk.images[vk.imageIndex];
        const bool before = vk.commands == 0 && src == VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT && dst == VK_PIPELINE_STAGE_TRANSFER_BIT;
        const bool after = vk.commands == 2 && src == VK_PIPELINE_STAGE_TRANSFER_BIT && dst == VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
        if (!flags && !memoryCount && !bufferCount && count == 1 && barriers && (before || after)) {
            auto host = barriers[0];
            if (host.sType == VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER && !host.pNext && host.image == image.guest &&
                ColorRange(host.subresourceRange) && host.srcQueueFamilyIndex == VK_QUEUE_FAMILY_IGNORED &&
                host.dstQueueFamilyIndex == VK_QUEUE_FAMILY_IGNORED &&
                (before ? (host.oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && host.newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL &&
                           !host.srcAccessMask && host.dstAccessMask == VK_ACCESS_TRANSFER_WRITE_BIT) :
                          (host.oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && host.newLayout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR &&
                           host.srcAccessMask == VK_ACCESS_TRANSFER_WRITE_BIT && !host.dstAccessMask))) {
                host.image = image.host;
                DeviceDriver<PFN_vkCmdPipelineBarrier>("vkCmdPipelineBarrier")(vk.command, src, dst, 0, 0, nullptr, 0, nullptr, 1, &host);
                ++vk.commands;
                return VK_SUCCESS;
            }
        }
        vk.frame = Frame::Invalid;
        return VK_ERROR_FEATURE_NOT_PRESENT;
    });
}

/// Records a clear of only the acquired image, with its transfer layout established.
void VKAPI_CALL ClearImage(VkCommandBuffer cmd, VkImage image, VkImageLayout layout, const VkClearColorValue* color,
                           uint32_t count, const VkImageSubresourceRange* range) {
    Run([=] {
        if (!cmd || cmd != vk.guestCommand || vk.frame != Frame::Recording) return VK_ERROR_INITIALIZATION_FAILED;
        const auto& expected = vk.images[vk.imageIndex];
        if (vk.commands != 1 || image != expected.guest || layout != VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL ||
            !color || count != 1 || !range || !ColorRange(*range)) {
            vk.frame = Frame::Invalid;
            return VK_ERROR_FEATURE_NOT_PRESENT;
        }
        DeviceDriver<PFN_vkCmdClearColorImage>("vkCmdClearColorImage")(vk.command, expected.host, layout, color, 1, range);
        ++vk.commands;
        return VK_SUCCESS;
    });
}

/// Records a whole-buffer fill after the swapchain image transitions to present.
void VKAPI_CALL FillBuffer(VkCommandBuffer cmd, VkBuffer buffer, VkDeviceSize offset, VkDeviceSize size, uint32_t value) {
    Run([=] {
        if (!cmd || cmd != vk.guestCommand || vk.frame != Frame::Recording) return VK_ERROR_INITIALIZATION_FAILED;
        if (vk.commands != 3 || !buffer || buffer != vk.guestBuffer || !vk.bound || vk.mapped || offset || size != vk.bufferSize) {
            vk.frame = Frame::Invalid;
            return VK_ERROR_FEATURE_NOT_PRESENT;
        }
        DeviceDriver<PFN_vkCmdFillBuffer>("vkCmdFillBuffer")(vk.command, vk.buffer, 0, size, value);
        ++vk.commands;
        return VK_SUCCESS;
    });
}

/// Ends a transfer clear with or without the optional fill and its required barrier.
VkResult VKAPI_CALL EndCommand(VkCommandBuffer cmd) {
    return Run([=] {
        if (!cmd || cmd != vk.guestCommand ||
            (vk.frame != Frame::Recording && vk.frame != Frame::Invalid)) return VK_ERROR_INITIALIZATION_FAILED;
        const bool complete = vk.frame == Frame::Recording && (vk.commands == 3 || vk.commands == 5);
        const auto result = DeviceDriver<PFN_vkEndCommandBuffer>("vkEndCommandBuffer")(vk.command);
        vk.frame = complete && result == VK_SUCCESS ? Frame::Executable : Frame::Ready;
        return complete ? result : VK_ERROR_FEATURE_NOT_PRESENT;
    });
}

/// Submits one audited command buffer; no semaphores or foreign objects are accepted.
VkResult VKAPI_CALL Submit(VkQueue q, uint32_t n, const VkSubmitInfo* info, VkFence fence) {
    return Run([=] {
        if (!q || q != vk.guestQueue || n != 1 || !info || info->sType != VK_STRUCTURE_TYPE_SUBMIT_INFO ||
            info->pNext || info->waitSemaphoreCount || info->signalSemaphoreCount || fence ||
            info->commandBufferCount != 1 || !info->pCommandBuffers || *info->pCommandBuffers != vk.guestCommand ||
            vk.frame != Frame::Executable) return VK_ERROR_INITIALIZATION_FAILED;
        auto host = *info;
        host.pCommandBuffers = &vk.command;
        if (vk.commands == 5) vk.bufferReadable = false;
        const auto result = DeviceDriver<PFN_vkQueueSubmit>("vkQueueSubmit")(vk.queue, 1, &host, VK_NULL_HANDLE);
        vk.frame = result == VK_SUCCESS ? Frame::Submitted : Frame::Ready;
        return result;
    });
}

/// Synchronizes the one queue without holding JVM or surface registry locks.
VkResult VKAPI_CALL QueueIdle(VkQueue q) {
    return Run([=] {
        if (!q || q != vk.guestQueue) return VK_ERROR_INITIALIZATION_FAILED;
        const auto result = DeviceDriver<PFN_vkQueueWaitIdle>("vkQueueWaitIdle")(vk.queue);
        if (vk.frame == Frame::Submitted) {
            vk.frame = result == VK_SUCCESS ? Frame::Complete : Frame::Ready;
            if (result == VK_SUCCESS && vk.commands == 5) vk.bufferReadable = true;
        }
        return result;
    });
}

/// Presents only the acquired and completed image; returns raw WSI status without silent fallback.
VkResult VKAPI_CALL Present(VkQueue q, const VkPresentInfoKHR* info) {
    return Run([=] {
        if (!q || q != vk.guestQueue || !info || info->sType != VK_STRUCTURE_TYPE_PRESENT_INFO_KHR ||
            info->pNext || info->waitSemaphoreCount || info->swapchainCount != 1 || !info->pSwapchains ||
            *info->pSwapchains != vk.guestSwapchain || !info->pImageIndices || *info->pImageIndices != vk.imageIndex ||
            vk.frame != Frame::Complete) return VK_ERROR_INITIALIZATION_FAILED;
        auto result = CheckSwapchain();
        if (result == VK_SUCCESS) {
            auto host = *info;
            host.pSwapchains = &vk.swapchain;
            result = DeviceDriver<PFN_vkQueuePresentKHR>("vkQueuePresentKHR")(vk.queue, &host);
        } else if (info->pResults) *info->pResults = result;
        vk.frame = Frame::Idle;
        vk.imageIndex = UINT32_MAX;
        if (result == VK_SUCCESS || result == VK_SUBOPTIMAL_KHR) {
            vk.presented = true;
            if (IsEmbedded()) {
                SendEmbedEvent("backend vulkan");
                PublishEmbedHwnd(vk.window);
            }
        }
        else if (result < 0) vk.swapchainError = result;
        return result;
    });
}

PFN_vkVoidFunction VKAPI_CALL Proc(VkInstance instance, const char* name);
PFN_vkVoidFunction VKAPI_CALL DeviceProc(VkDevice device, const char* name);

/// Describes one audited fixed-signature import and its resolution scope.
enum class Scope { Global, Instance, Surface, AndroidSurface, Device };
struct Entry { guest::HostSymbol symbol; Scope scope; };
/// Type-checks each registered wrapper against the locked Vulkan Headers PFN.
template<class P, class F> Entry CheckedEntry(const char* name, F fn, Scope scope) {
    static_assert(std::is_same_v<P, F>);
    return {{name, reinterpret_cast<void*>(fn)}, scope};
}
#define ENTRY(name, fn, scope) CheckedEntry<PFN_##name>(#name, &fn, Scope::scope)
const Entry entries[] = {
    ENTRY(vkGetInstanceProcAddr, Proc, Global),
    ENTRY(vkEnumerateInstanceVersion, Version, Global),
    ENTRY(vkEnumerateInstanceLayerProperties, Layers, Global),
    ENTRY(vkEnumerateInstanceExtensionProperties, Extensions, Global),
    ENTRY(vkCreateInstance, CreateInstance, Global),
    ENTRY(vkDestroyInstance, DestroyInstance, Instance),
    ENTRY(vkEnumeratePhysicalDevices, PhysicalDevices, Instance),
    ENTRY(vkGetPhysicalDeviceQueueFamilyProperties, Queues, Instance),
    ENTRY(vkGetPhysicalDeviceMemoryProperties, MemoryProperties, Instance),
    ENTRY(vkCreateAndroidSurfaceKHR, CreateSurface, AndroidSurface),
    ENTRY(vkDestroySurfaceKHR, DestroySurface, Surface),
    ENTRY(vkGetPhysicalDeviceSurfaceSupportKHR, PresentSupport, Surface),
    ENTRY(vkGetPhysicalDeviceSurfaceCapabilitiesKHR, Capabilities, Surface),
    ENTRY(vkGetPhysicalDeviceSurfaceFormatsKHR, Formats, Surface),
    ENTRY(vkGetPhysicalDeviceSurfacePresentModesKHR, PresentModes, Surface),
    ENTRY(vkEnumerateDeviceExtensionProperties, DeviceExtensions, Instance),
    ENTRY(vkCreateDevice, CreateDevice, Instance),
    ENTRY(vkGetDeviceProcAddr, DeviceProc, Instance),
    ENTRY(vkGetDeviceQueue, GetQueue, Device),
    ENTRY(vkDestroyDevice, DestroyDevice, Device),
    ENTRY(vkCreateSwapchainKHR, CreateSwapchain, Device),
    ENTRY(vkGetSwapchainImagesKHR, Images, Device),
    ENTRY(vkDestroySwapchainKHR, DestroySwapchain, Device),
    ENTRY(vkAcquireNextImageKHR, AcquireImage, Device),
    ENTRY(vkCreateFence, CreateFence, Device),
    ENTRY(vkWaitForFences, WaitFence, Device),
    ENTRY(vkResetFences, ResetFence, Device),
    ENTRY(vkDestroyFence, DestroyFence, Device),
    ENTRY(vkCreateCommandPool, CreatePool, Device),
    ENTRY(vkDestroyCommandPool, DestroyPool, Device),
    ENTRY(vkAllocateCommandBuffers, AllocateCommand, Device),
    ENTRY(vkBeginCommandBuffer, BeginCommand, Device),
    ENTRY(vkCmdPipelineBarrier, Barrier, Device),
    ENTRY(vkCmdClearColorImage, ClearImage, Device),
    ENTRY(vkCreateBuffer, CreateBuffer, Device),
    ENTRY(vkGetBufferMemoryRequirements, BufferRequirements, Device),
    ENTRY(vkAllocateMemory, AllocateMemory, Device),
    ENTRY(vkBindBufferMemory, BindBuffer, Device),
    ENTRY(vkMapMemory, MapMemory, Device),
    ENTRY(vkUnmapMemory, UnmapMemory, Device),
    ENTRY(vkDestroyBuffer, DestroyBuffer, Device),
    ENTRY(vkFreeMemory, FreeMemory, Device),
    ENTRY(vkCmdFillBuffer, FillBuffer, Device),
    ENTRY(vkEndCommandBuffer, EndCommand, Device),
    ENTRY(vkQueueSubmit, Submit, Device),
    ENTRY(vkQueueWaitIdle, QueueIdle, Device),
    ENTRY(vkQueuePresentKHR, Present, Device),
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

/// Resolves only device commands on the live device, never an ICD function pointer.
PFN_vkVoidFunction VKAPI_CALL DeviceProc(VkDevice device, const char* name) {
    if (!device || !name || strnlen(name, 128) == 128) return nullptr;
    for (const auto& entry : entries)
        if (entry.scope == Scope::Device && !std::strcmp(entry.symbol.name, name))
            return Run([=] { return device == vk.guestDevice ? VK_SUCCESS : VK_ERROR_INITIALIZATION_FAILED; }) == VK_SUCCESS
                ? reinterpret_cast<PFN_vkVoidFunction>(entry.symbol.address) : nullptr;
    return nullptr;
}

} // namespace

void AddGuestVulkanLibrary(std::vector<guest::BuiltinLibrary>& libraries) {
    guest::BuiltinLibrary library{"libvulkan.so", {}, ""};
    for (const auto& entry : entries) library.symbols.push_back(entry.symbol);
    libraries.push_back(std::move(library));
}

bool GuestVulkanOwnsWindow() { return vk.surface && !vk.retired; }

/// Allows Win32 to validate paint without overwriting the active swapchain's contents.
bool GuestVulkanPresents(HWND window) { return vk.window == window && !vk.retired && vk.presented; }

void RetireGuestVulkan(HWND window, bool closing) {
    if (closing && (!vk.window || vk.window == window)) Shutdown();
    else if (vk.window == window) vk.retired = true;
}

} // namespace winapk::bridge
