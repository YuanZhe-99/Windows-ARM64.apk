// Restricted Android Vulkan instance/WSI provider. All native handles stay on the window thread.
#include "guest_vulkan.h"
#include "guest_graphics.h"
#include "surface.h"
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
static_assert(sizeof(VkAllocationCallbacks) == 48 && sizeof(VkShaderModuleCreateInfo) == 40 &&
              sizeof(VkImageCreateInfo) == 88 && sizeof(VkImageViewCreateInfo) == 80 &&
              sizeof(VkRenderPassCreateInfo) == 64 && sizeof(VkFramebufferCreateInfo) == 64 &&
              sizeof(VkPipelineLayoutCreateInfo) == 48 && sizeof(VkGraphicsPipelineCreateInfo) == 144 &&
              sizeof(VkRenderPassBeginInfo) == 64 && sizeof(VkBufferImageCopy) == 56);

guest::ModuleSet* g_guestModules = nullptr;

/// Calls a screened guest allocation callback without CFG rejecting the guest ELF address.
__declspec(guard(nocf)) void* InvokeGuestAlloc(PFN_vkAllocationFunction fn, void* ud, size_t sz,
                                               size_t align, VkSystemAllocationScope scope) {
    return fn(ud, sz, align, scope);
}

/// Calls a screened guest free callback without CFG rejecting the guest ELF address.
__declspec(guard(nocf)) void InvokeGuestFree(PFN_vkFreeFunction fn, void* ud, void* mem) {
    fn(ud, mem);
}

/// Calls a screened guest VK_EXT_debug_report callback on the calling guest thread.
__declspec(guard(nocf)) VkBool32 InvokeGuestDebugReport(
    PFN_vkDebugReportCallbackEXT fn, VkDebugReportFlagsEXT flags, VkDebugReportObjectTypeEXT objType,
    uint64_t obj, size_t loc, int32_t code, const char* prefix, const char* msg, void* ud) {
    return fn(flags, objType, obj, loc, code, prefix, msg, ud);
}

bool ValidGuestCode(const void* fn) {
    return fn && g_guestModules && g_guestModules->Executable(reinterpret_cast<uintptr_t>(fn));
}

bool ValidAllocator(const VkAllocationCallbacks* a) {
    if (!a || !ValidGuestCode(reinterpret_cast<const void*>(a->pfnAllocation)) ||
        !ValidGuestCode(reinterpret_cast<const void*>(a->pfnReallocation)) ||
        !ValidGuestCode(reinterpret_cast<const void*>(a->pfnFree)))
        return false;
    if (static_cast<bool>(a->pfnInternalAllocation) != static_cast<bool>(a->pfnInternalFree))
        return false;
    if (a->pfnInternalAllocation &&
        (!ValidGuestCode(reinterpret_cast<const void*>(a->pfnInternalAllocation)) ||
         !ValidGuestCode(reinterpret_cast<const void*>(a->pfnInternalFree))))
        return false;
    return true;
}

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
    bool debugReportExtension = false;
    VkDebugReportCallbackEXT guestDebugCallback = VK_NULL_HANDLE;
    VkDebugReportFlagsEXT debugFlags = 0;
    PFN_vkDebugReportCallbackEXT debugCallback = nullptr;
    void* debugUserData = nullptr;
    bool retired = false;
    bool presented = false;
    bool deviceLost = false;
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
    VkImage offscreenImage = VK_NULL_HANDLE, guestOffscreenImage = VK_NULL_HANDLE;
    VkDeviceMemory imageMemory = VK_NULL_HANDLE, guestImageMemory = VK_NULL_HANDLE;
    VkMemoryRequirements imageRequirements{};
    VkExtent2D offscreenExtent{};
    bool imageBound = false;
    VkImageView imageView = VK_NULL_HANDLE, guestImageView = VK_NULL_HANDLE;
    struct ShaderSlot {
        VkShaderModule host = VK_NULL_HANDLE, guest = VK_NULL_HANDLE;
        void* guestAlloc = nullptr;
        bool usedAllocator = false;
    };
    std::array<ShaderSlot, 2> shaders{};
    VkRenderPass renderPass = VK_NULL_HANDLE, guestRenderPass = VK_NULL_HANDLE;
    VkFramebuffer framebuffer = VK_NULL_HANDLE, guestFramebuffer = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE, guestPipelineLayout = VK_NULL_HANDLE;
    VkPipeline graphicsPipeline = VK_NULL_HANDLE, guestGraphicsPipeline = VK_NULL_HANDLE;
    enum class Frame {
        Idle, Acquired, Ready, Recording, Executable, Submitted, Complete, Invalid,
        OffscreenRecording, OffscreenExecutable, OffscreenSubmitted
    };
    Frame frame = Frame::Idle;
    unsigned commands = 0;
    unsigned offscreenStep = 0;
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
    if (!vk.deviceLost) DeviceDriver<PFN_vkQueueWaitIdle>("vkQueueWaitIdle")(vk.queue);
    DeviceDriver<PFN_vkDestroySwapchainKHR>("vkDestroySwapchainKHR")(vk.device, vk.swapchain, nullptr);
    vk.swapchain = vk.guestSwapchain = VK_NULL_HANDLE;
    vk.imageCount = 0;
    vk.imageIndex = UINT32_MAX;
    vk.frame = Vulkan::Frame::Idle;
    vk.presented = false;
    vk.swapchainError = VK_SUCCESS;
}

/// Destroys any remaining offscreen graphics/image resources on device teardown.
void DropGraphics() {
    if (vk.graphicsPipeline) DeviceDriver<PFN_vkDestroyPipeline>("vkDestroyPipeline")(vk.device, vk.graphicsPipeline, nullptr);
    if (vk.pipelineLayout) DeviceDriver<PFN_vkDestroyPipelineLayout>("vkDestroyPipelineLayout")(vk.device, vk.pipelineLayout, nullptr);
    if (vk.framebuffer) DeviceDriver<PFN_vkDestroyFramebuffer>("vkDestroyFramebuffer")(vk.device, vk.framebuffer, nullptr);
    if (vk.renderPass) DeviceDriver<PFN_vkDestroyRenderPass>("vkDestroyRenderPass")(vk.device, vk.renderPass, nullptr);
    if (vk.imageView) DeviceDriver<PFN_vkDestroyImageView>("vkDestroyImageView")(vk.device, vk.imageView, nullptr);
    for (auto& s : vk.shaders) {
        if (s.host) DeviceDriver<PFN_vkDestroyShaderModule>("vkDestroyShaderModule")(vk.device, s.host, nullptr);
        s = {};
    }
    if (vk.offscreenImage) DeviceDriver<PFN_vkDestroyImage>("vkDestroyImage")(vk.device, vk.offscreenImage, nullptr);
    if (vk.imageMemory) DeviceDriver<PFN_vkFreeMemory>("vkFreeMemory")(vk.device, vk.imageMemory, nullptr);
    vk.graphicsPipeline = vk.guestGraphicsPipeline = VK_NULL_HANDLE;
    vk.pipelineLayout = vk.guestPipelineLayout = VK_NULL_HANDLE;
    vk.framebuffer = vk.guestFramebuffer = VK_NULL_HANDLE;
    vk.renderPass = vk.guestRenderPass = VK_NULL_HANDLE;
    vk.imageView = vk.guestImageView = VK_NULL_HANDLE;
    vk.offscreenImage = vk.guestOffscreenImage = VK_NULL_HANDLE;
    vk.imageMemory = vk.guestImageMemory = VK_NULL_HANDLE;
    vk.imageRequirements = {};
    vk.offscreenExtent = {};
    vk.imageBound = false;
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
    DropGraphics();
    if (vk.pool) DeviceDriver<PFN_vkDestroyCommandPool>("vkDestroyCommandPool")(vk.device, vk.pool, nullptr);
    if (vk.fence) DeviceDriver<PFN_vkDestroyFence>("vkDestroyFence")(vk.device, vk.fence, nullptr);
    DropBuffer();
    DropMemory();
    DeviceDriver<PFN_vkDestroyDevice>("vkDestroyDevice")(vk.device, nullptr);
    vk.device = vk.guestDevice = VK_NULL_HANDLE;
    vk.deviceProc = nullptr;
    vk.deviceLost = false;
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

/// Explicit extension tables separated by role (PLAN.md §13.4):
/// 1) Translated guest→host WSI instance extensions, 2) Guest-implemented instance extensions,
/// 3) Translated device extensions. Android external memory / AHardwareBuffer is never exposed.
struct ExtensionTranslation { VkExtensionProperties guest; const char* host; };
constexpr ExtensionTranslation kTranslatedInstanceExtensions[] = {
    {{VK_KHR_SURFACE_EXTENSION_NAME, VK_KHR_SURFACE_SPEC_VERSION}, VK_KHR_SURFACE_EXTENSION_NAME},
    {{VK_KHR_ANDROID_SURFACE_EXTENSION_NAME, VK_KHR_ANDROID_SURFACE_SPEC_VERSION}, VK_KHR_WIN32_SURFACE_EXTENSION_NAME},
};
constexpr VkExtensionProperties kGuestImplementedInstanceExtensions[] = {
    {VK_EXT_DEBUG_REPORT_EXTENSION_NAME, VK_EXT_DEBUG_REPORT_SPEC_VERSION},
};

/// Walks a guest pNext chain and refuses any unhandled or Android-external structure instead of truncating it.
VkResult RejectUnhandledPNext(const void* pNext) {
    if (!pNext) return VK_SUCCESS;
    const auto* node = static_cast<const VkBaseInStructure*>(pNext);
    switch (node->sType) {
        case VK_STRUCTURE_TYPE_IMPORT_ANDROID_HARDWARE_BUFFER_INFO_ANDROID:
        case VK_STRUCTURE_TYPE_MEMORY_GET_ANDROID_HARDWARE_BUFFER_INFO_ANDROID:
        case VK_STRUCTURE_TYPE_ANDROID_HARDWARE_BUFFER_USAGE_ANDROID:
        case VK_STRUCTURE_TYPE_ANDROID_HARDWARE_BUFFER_PROPERTIES_ANDROID:
        case VK_STRUCTURE_TYPE_ANDROID_HARDWARE_BUFFER_FORMAT_PROPERTIES_ANDROID:
        case VK_STRUCTURE_TYPE_EXTERNAL_FORMAT_ANDROID:
            return VK_ERROR_EXTENSION_NOT_PRESENT;
        default:
            return VK_ERROR_FEATURE_NOT_PRESENT;
    }
}

/// Filters extensions instead of forwarding host Win32 or interop entries; Android Surface requires a ready Surface bridge.
VkResult VKAPI_CALL Extensions(const char* layer, uint32_t* count, VkExtensionProperties* out) {
    if (layer) return VK_ERROR_LAYER_NOT_PRESENT;
    return Run([=] {
        if (!Loader()) return VK_ERROR_INITIALIZATION_FAILED;
        const bool wsi = WsiAvailable() && SurfaceBridgeReady();
        if (!WsiAvailable()) LogLine("GRAPHICS_CAPABILITY_MISSING: guest Vulkan system loader does not offer Win32 WSI");
        if (!wsi) return Enumerate<VkExtensionProperties>(0, nullptr, count, out);
        const VkExtensionProperties supported[] = {
            kTranslatedInstanceExtensions[0].guest,
            kTranslatedInstanceExtensions[1].guest,
            kGuestImplementedInstanceExtensions[0],
        };
        return Enumerate(3u, supported, count, out);
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
    if (!info || info->sType != VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO || allocator || info->flags)
        return VK_ERROR_FEATURE_NOT_PRESENT;
    if (const auto rc = RejectUnhandledPNext(info->pNext); rc != VK_SUCCESS) return rc;
    if (info->enabledLayerCount) return VK_ERROR_LAYER_NOT_PRESENT;
    if (info->enabledExtensionCount > 3 || (info->enabledExtensionCount && !info->ppEnabledExtensionNames))
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    bool surface = false, android = false, debugReport = false;
    for (uint32_t i = 0; i < info->enabledExtensionCount; ++i) {
        const char* name = info->ppEnabledExtensionNames[i];
        if (!name || strnlen(name, VK_MAX_EXTENSION_NAME_SIZE) == VK_MAX_EXTENSION_NAME_SIZE)
            return VK_ERROR_EXTENSION_NOT_PRESENT;
        if (!surface && std::strcmp(name, VK_KHR_SURFACE_EXTENSION_NAME) == 0) surface = true;
        else if (!android && std::strcmp(name, VK_KHR_ANDROID_SURFACE_EXTENSION_NAME) == 0) android = true;
        else if (!debugReport && std::strcmp(name, VK_EXT_DEBUG_REPORT_EXTENSION_NAME) == 0) debugReport = true;
        else return VK_ERROR_EXTENSION_NOT_PRESENT;
    }
    if (android && !surface) return VK_ERROR_EXTENSION_NOT_PRESENT;
    if (info->pApplicationInfo) {
        const auto& app = *info->pApplicationInfo;
        if (app.sType != VK_STRUCTURE_TYPE_APPLICATION_INFO) return VK_ERROR_FEATURE_NOT_PRESENT;
        if (const auto rc = RejectUnhandledPNext(app.pNext); rc != VK_SUCCESS) return rc;
        if (app.apiVersion > VK_API_VERSION_1_0) return VK_ERROR_INCOMPATIBLE_DRIVER;
    }
    return Run([&] {
        if (vk.instance || !Loader()) return VK_ERROR_INITIALIZATION_FAILED;
        if (surface && !WsiAvailable()) return VK_ERROR_EXTENSION_NOT_PRESENT;
        if (android && !SurfaceBridgeReady()) return VK_ERROR_EXTENSION_NOT_PRESENT;
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
        vk.debugReportExtension = debugReport;
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

/// Registers one screened guest VK_EXT_debug_report callback on the active instance.
VkResult VKAPI_CALL CreateDebugReportCallback(VkInstance i, const VkDebugReportCallbackCreateInfoEXT* info,
                                              const VkAllocationCallbacks* allocator, VkDebugReportCallbackEXT* out) {
    if (!out) return VK_ERROR_INITIALIZATION_FAILED;
    *out = VK_NULL_HANDLE;
    if (!info || info->sType != VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT ||
        info->pNext || allocator || !ValidGuestCode(reinterpret_cast<const void*>(info->pfnCallback)))
        return VK_ERROR_FEATURE_NOT_PRESENT;
    return Run([=] {
        if (!i || i != vk.guestInstance || !vk.debugReportExtension || vk.guestDebugCallback)
            return VK_ERROR_INITIALIZATION_FAILED;
        vk.debugFlags = info->flags;
        vk.debugCallback = info->pfnCallback;
        vk.debugUserData = info->pUserData;
        *out = vk.guestDebugCallback = reinterpret_cast<VkDebugReportCallbackEXT>(nextHandle++);
        return VK_SUCCESS;
    });
}

/// Retires the active VK_EXT_debug_report callback before instance teardown.
void VKAPI_CALL DestroyDebugReportCallback(VkInstance i, VkDebugReportCallbackEXT cb, const VkAllocationCallbacks* allocator) {
    if (!cb) return;
    Run([=] {
        if (!allocator && i == vk.guestInstance && cb == vk.guestDebugCallback) {
            vk.guestDebugCallback = VK_NULL_HANDLE;
            vk.debugFlags = 0;
            vk.debugCallback = nullptr;
            vk.debugUserData = nullptr;
            return VK_SUCCESS;
        }
        LogLine("GRAPHICS_CAPABILITY_MISSING: guest Vulkan invalid debug callback destruction");
        return VK_ERROR_INITIALIZATION_FAILED;
    });
}

/// Dispatches an application debug report to the registered guest callback on the calling guest thread.
void VKAPI_CALL DebugReportMessage(VkInstance i, VkDebugReportFlagsEXT flags, VkDebugReportObjectTypeEXT objType,
                                   uint64_t obj, size_t loc, int32_t code, const char* prefix, const char* msg) {
    if (!prefix || !msg || strnlen(prefix, 256) == 256 || strnlen(msg, 4096) == 4096) return;
    PFN_vkDebugReportCallbackEXT cb = nullptr;
    void* ud = nullptr;
    if (Run([&] {
            if (!i || i != vk.guestInstance || !vk.debugReportExtension || !vk.guestDebugCallback)
                return VK_ERROR_INITIALIZATION_FAILED;
            if (vk.debugFlags & flags) {
                cb = vk.debugCallback;
                ud = vk.debugUserData;
            }
            return VK_SUCCESS;
        }) == VK_SUCCESS && cb) {
        InvokeGuestDebugReport(cb, flags, objType, obj, loc, code, prefix, msg, ud);
    }
}

/// Destroys the named instance after all children have been released.
void VKAPI_CALL DestroyInstance(VkInstance i, const VkAllocationCallbacks* allocator) {
    if (!i) return;
    Run([=] {
        if (!allocator && i == vk.guestInstance && !vk.surface && !vk.device && !vk.guestDebugCallback) { Shutdown(); return VK_SUCCESS; }
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
        if (!allocator && d && d == vk.guestDevice && !vk.swapchain && !vk.pool && !vk.fence && !vk.buffer && !vk.memory &&
            !vk.graphicsPipeline && !vk.pipelineLayout && !vk.framebuffer && !vk.renderPass && !vk.imageView &&
            !vk.shaders[0].host && !vk.shaders[1].host && !vk.offscreenImage && !vk.imageMemory)
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

/// Allocates memory for either the coherent transfer buffer or the offscreen 2D color image.
VkResult VKAPI_CALL AllocateMemory(VkDevice d, const VkMemoryAllocateInfo* info, const VkAllocationCallbacks* allocator, VkDeviceMemory* out) {
    if (!out) return VK_ERROR_INITIALIZATION_FAILED;
    *out = VK_NULL_HANDLE;
    if (!info || info->sType != VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO || allocator)
        return VK_ERROR_FEATURE_NOT_PRESENT;
    if (const auto rc = RejectUnhandledPNext(info->pNext); rc != VK_SUCCESS) return rc;
    return Run([=] {
        if (!d || d != vk.guestDevice || vk.frame != Frame::Idle)
            return VK_ERROR_INITIALIZATION_FAILED;
        VkPhysicalDeviceMemoryProperties props{};
        Driver<PFN_vkGetPhysicalDeviceMemoryProperties>("vkGetPhysicalDeviceMemoryProperties", vk.instance)
            (vk.devicePhysical, &props);
        const auto i = info->memoryTypeIndex;
        if (i >= props.memoryTypeCount || i >= 32) return VK_ERROR_FEATURE_NOT_PRESENT;
        constexpr auto coherent = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        if (vk.buffer && !vk.memory && info->allocationSize == vk.memoryRequirements.size &&
            (vk.memoryRequirements.memoryTypeBits & (uint32_t{1} << i)) &&
            (props.memoryTypes[i].propertyFlags & coherent) == coherent) {
            const auto result = DeviceDriver<PFN_vkAllocateMemory>("vkAllocateMemory")(vk.device, info, nullptr, &vk.memory);
            if (result == VK_SUCCESS) *out = vk.guestMemory = reinterpret_cast<VkDeviceMemory>(nextHandle++);
            return result;
        }
        if (vk.offscreenImage && !vk.imageMemory && info->allocationSize == vk.imageRequirements.size &&
            (vk.imageRequirements.memoryTypeBits & (uint32_t{1} << i))) {
            const auto result = DeviceDriver<PFN_vkAllocateMemory>("vkAllocateMemory")(vk.device, info, nullptr, &vk.imageMemory);
            if (result == VK_SUCCESS) *out = vk.guestImageMemory = reinterpret_cast<VkDeviceMemory>(nextHandle++);
            return result;
        }
        return VK_ERROR_FEATURE_NOT_PRESENT;
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

/// Frees a device allocation only after its bound buffer or image has been destroyed.
void VKAPI_CALL FreeMemory(VkDevice d, VkDeviceMemory memory, const VkAllocationCallbacks* allocator) {
    Run([=] {
        if (!d || d != vk.guestDevice || allocator || vk.frame != Frame::Idle) return VK_SUCCESS;
        if (memory && memory == vk.guestMemory && !vk.bound && !vk.mapped) DropMemory();
        else if (memory && memory == vk.guestImageMemory && !vk.imageBound) {
            DeviceDriver<PFN_vkFreeMemory>("vkFreeMemory")(vk.device, vk.imageMemory, nullptr);
            vk.imageMemory = vk.guestImageMemory = VK_NULL_HANDLE;
        }
        return VK_SUCCESS;
    });
}

/// Creates one bounded 2D RGBA8 color-attachment/transfer-source image.
VkResult VKAPI_CALL CreateImage(VkDevice d, const VkImageCreateInfo* info, const VkAllocationCallbacks* allocator, VkImage* out) {
    if (!out) return VK_ERROR_INITIALIZATION_FAILED;
    *out = VK_NULL_HANDLE;
    constexpr auto wantUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    if (!info || info->sType != VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO || info->pNext || info->flags || allocator ||
        info->imageType != VK_IMAGE_TYPE_2D || info->format != VK_FORMAT_R8G8B8A8_UNORM ||
        !info->extent.width || info->extent.width > 256 || !info->extent.height || info->extent.height > 256 ||
        info->extent.depth != 1 || info->mipLevels != 1 || info->arrayLayers != 1 ||
        info->samples != VK_SAMPLE_COUNT_1_BIT || info->tiling != VK_IMAGE_TILING_OPTIMAL ||
        info->usage != wantUsage || info->sharingMode != VK_SHARING_MODE_EXCLUSIVE ||
        info->queueFamilyIndexCount || info->initialLayout != VK_IMAGE_LAYOUT_UNDEFINED)
        return VK_ERROR_FEATURE_NOT_PRESENT;
    return Run([=] {
        if (!d || d != vk.guestDevice || vk.offscreenImage || vk.imageMemory || vk.frame != Frame::Idle)
            return VK_ERROR_INITIALIZATION_FAILED;
        const auto result = DeviceDriver<PFN_vkCreateImage>("vkCreateImage")(vk.device, info, nullptr, &vk.offscreenImage);
        if (result != VK_SUCCESS) return result;
        DeviceDriver<PFN_vkGetImageMemoryRequirements>("vkGetImageMemoryRequirements")(vk.device, vk.offscreenImage, &vk.imageRequirements);
        vk.offscreenExtent = {info->extent.width, info->extent.height};
        *out = vk.guestOffscreenImage = reinterpret_cast<VkImage>(nextHandle++);
        return VK_SUCCESS;
    });
}

/// Queries memory requirements of the offscreen 2D image; returns zero for stale tokens.
void VKAPI_CALL ImageRequirements(VkDevice d, VkImage image, VkMemoryRequirements* out) {
    if (!out) return;
    *out = {};
    Run([=] {
        if (d && d == vk.guestDevice && image && image == vk.guestOffscreenImage) *out = vk.imageRequirements;
        return VK_SUCCESS;
    });
}

/// Binds the offscreen 2D image to its dedicated allocation at offset zero.
VkResult VKAPI_CALL BindImage(VkDevice d, VkImage image, VkDeviceMemory memory, VkDeviceSize offset) {
    return Run([=] {
        if (!d || d != vk.guestDevice || !image || image != vk.guestOffscreenImage ||
            !memory || memory != vk.guestImageMemory || vk.imageBound || offset || vk.frame != Frame::Idle)
            return VK_ERROR_FEATURE_NOT_PRESENT;
        const auto result = DeviceDriver<PFN_vkBindImageMemory>("vkBindImageMemory")(vk.device, vk.offscreenImage, vk.imageMemory, 0);
        vk.imageBound = result == VK_SUCCESS;
        return result;
    });
}

/// Destroys the offscreen 2D image once its view is gone and the queue is idle.
void VKAPI_CALL DestroyImage(VkDevice d, VkImage image, const VkAllocationCallbacks* allocator) {
    Run([=] {
        if (d && d == vk.guestDevice && image && image == vk.guestOffscreenImage && !allocator &&
            !vk.imageView && vk.frame == Frame::Idle) {
            DeviceDriver<PFN_vkDestroyImage>("vkDestroyImage")(vk.device, vk.offscreenImage, nullptr);
            vk.offscreenImage = vk.guestOffscreenImage = VK_NULL_HANDLE;
            vk.imageRequirements = {};
            vk.offscreenExtent = {};
            vk.imageBound = false;
        }
        return VK_SUCCESS;
    });
}

/// Creates a 2D RGBA8 color view over the bound offscreen image.
VkResult VKAPI_CALL CreateImageView(VkDevice d, const VkImageViewCreateInfo* info, const VkAllocationCallbacks* allocator, VkImageView* out) {
    if (!out) return VK_ERROR_INITIALIZATION_FAILED;
    *out = VK_NULL_HANDLE;
    if (!info || info->sType != VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO || info->pNext || info->flags || allocator ||
        info->viewType != VK_IMAGE_VIEW_TYPE_2D || info->format != VK_FORMAT_R8G8B8A8_UNORM ||
        info->subresourceRange.aspectMask != VK_IMAGE_ASPECT_COLOR_BIT || info->subresourceRange.baseMipLevel ||
        info->subresourceRange.levelCount != 1 || info->subresourceRange.baseArrayLayer || info->subresourceRange.layerCount != 1)
        return VK_ERROR_FEATURE_NOT_PRESENT;
    return Run([=] {
        if (!d || d != vk.guestDevice || !vk.imageBound || info->image != vk.guestOffscreenImage || vk.imageView || vk.frame != Frame::Idle)
            return VK_ERROR_INITIALIZATION_FAILED;
        auto host = *info;
        host.image = vk.offscreenImage;
        const auto result = DeviceDriver<PFN_vkCreateImageView>("vkCreateImageView")(vk.device, &host, nullptr, &vk.imageView);
        if (result == VK_SUCCESS) *out = vk.guestImageView = reinterpret_cast<VkImageView>(nextHandle++);
        return result;
    });
}

/// Destroys the offscreen image view once no framebuffer references it.
void VKAPI_CALL DestroyImageView(VkDevice d, VkImageView view, const VkAllocationCallbacks* allocator) {
    Run([=] {
        if (d && d == vk.guestDevice && view && view == vk.guestImageView && !allocator && !vk.framebuffer && vk.frame == Frame::Idle) {
            DeviceDriver<PFN_vkDestroyImageView>("vkDestroyImageView")(vk.device, vk.imageView, nullptr);
            vk.imageView = vk.guestImageView = VK_NULL_HANDLE;
        }
        return VK_SUCCESS;
    });
}

/// Creates a SPIR-V shader module, bridging optional guest VkAllocationCallbacks on the calling guest thread.
VkResult VKAPI_CALL CreateShaderModule(VkDevice d, const VkShaderModuleCreateInfo* info,
                                       const VkAllocationCallbacks* allocator, VkShaderModule* out) {
    if (!out) return VK_ERROR_INITIALIZATION_FAILED;
    *out = VK_NULL_HANDLE;
    if (!info || info->sType != VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO || info->pNext || info->flags ||
        !info->pCode || info->codeSize < 20 || info->codeSize > 65536 || (info->codeSize % 4) != 0 ||
        info->pCode[0] != 0x07230203u) return VK_ERROR_FEATURE_NOT_PRESENT;
    if (allocator && !ValidAllocator(allocator)) return VK_ERROR_FEATURE_NOT_PRESENT;
    void* guestMem = nullptr;
    if (allocator) {
        guestMem = InvokeGuestAlloc(allocator->pfnAllocation, allocator->pUserData,
                                    info->codeSize, alignof(uint64_t), VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);
        if (!guestMem) return VK_ERROR_OUT_OF_HOST_MEMORY;
    }
    const auto result = Run([&] {
        if (!d || d != vk.guestDevice || vk.frame != Frame::Idle) return VK_ERROR_INITIALIZATION_FAILED;
        size_t slot = vk.shaders.size();
        for (size_t i = 0; i < vk.shaders.size(); ++i) if (!vk.shaders[i].host) { slot = i; break; }
        if (slot == vk.shaders.size()) return VK_ERROR_TOO_MANY_OBJECTS;
        VkShaderModule mod = VK_NULL_HANDLE;
        const auto rc = DeviceDriver<PFN_vkCreateShaderModule>("vkCreateShaderModule")(vk.device, info, nullptr, &mod);
        if (rc != VK_SUCCESS) return rc;
        vk.shaders[slot] = {mod, reinterpret_cast<VkShaderModule>(nextHandle++), guestMem, allocator != nullptr};
        *out = vk.shaders[slot].guest;
        return VK_SUCCESS;
    });
    if (result != VK_SUCCESS && allocator && guestMem)
        InvokeGuestFree(allocator->pfnFree, allocator->pUserData, guestMem);
    return result;
}

/// Destroys a shader module and invokes the matching guest free callback when created with one.
void VKAPI_CALL DestroyShaderModule(VkDevice d, VkShaderModule module, const VkAllocationCallbacks* allocator) {
    if (!module) return;
    if (allocator && !ValidAllocator(allocator)) {
        LogLine("GRAPHICS_CAPABILITY_MISSING: guest Vulkan invalid shader allocator on destroy");
        return;
    }
    void* guestMem = nullptr;
    bool freeGuest = false;
    Run([&] {
        if (!d || d != vk.guestDevice || vk.frame != Frame::Idle) return VK_ERROR_INITIALIZATION_FAILED;
        for (auto& s : vk.shaders) {
            if (s.guest == module) {
                if (s.usedAllocator != (allocator != nullptr)) {
                    LogLine("GRAPHICS_CAPABILITY_MISSING: guest Vulkan mismatched shader allocator");
                    return VK_ERROR_INITIALIZATION_FAILED;
                }
                DeviceDriver<PFN_vkDestroyShaderModule>("vkDestroyShaderModule")(vk.device, s.host, nullptr);
                guestMem = s.guestAlloc;
                freeGuest = s.usedAllocator;
                s = {};
                return VK_SUCCESS;
            }
        }
        return VK_ERROR_INITIALIZATION_FAILED;
    });
    if (freeGuest && allocator) InvokeGuestFree(allocator->pfnFree, allocator->pUserData, guestMem);
}

/// Creates a single-subpass RGBA8 color render pass transitioning to transfer-source optimal.
VkResult VKAPI_CALL CreateRenderPass(VkDevice d, const VkRenderPassCreateInfo* info,
                                     const VkAllocationCallbacks* allocator, VkRenderPass* out) {
    if (!out) return VK_ERROR_INITIALIZATION_FAILED;
    *out = VK_NULL_HANDLE;
    if (!info || info->sType != VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO || info->pNext || info->flags || allocator ||
        info->attachmentCount != 1 || !info->pAttachments || info->subpassCount != 1 || !info->pSubpasses || info->dependencyCount)
        return VK_ERROR_FEATURE_NOT_PRESENT;
    const auto& att = info->pAttachments[0];
    const auto& sub = info->pSubpasses[0];
    if (att.flags || att.format != VK_FORMAT_R8G8B8A8_UNORM || att.samples != VK_SAMPLE_COUNT_1_BIT ||
        att.loadOp != VK_ATTACHMENT_LOAD_OP_CLEAR || att.storeOp != VK_ATTACHMENT_STORE_OP_STORE ||
        att.initialLayout != VK_IMAGE_LAYOUT_UNDEFINED || att.finalLayout != VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL ||
        sub.flags || sub.pipelineBindPoint != VK_PIPELINE_BIND_POINT_GRAPHICS || sub.inputAttachmentCount ||
        sub.colorAttachmentCount != 1 || !sub.pColorAttachments || sub.pResolveAttachments || sub.pDepthStencilAttachment ||
        sub.preserveAttachmentCount || sub.pColorAttachments[0].attachment != 0 ||
        sub.pColorAttachments[0].layout != VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
        return VK_ERROR_FEATURE_NOT_PRESENT;
    return Run([=] {
        if (!d || d != vk.guestDevice || vk.renderPass || vk.frame != Frame::Idle) return VK_ERROR_INITIALIZATION_FAILED;
        const auto result = DeviceDriver<PFN_vkCreateRenderPass>("vkCreateRenderPass")(vk.device, info, nullptr, &vk.renderPass);
        if (result == VK_SUCCESS) *out = vk.guestRenderPass = reinterpret_cast<VkRenderPass>(nextHandle++);
        return result;
    });
}

/// Destroys the render pass once framebuffers and pipelines using it are gone.
void VKAPI_CALL DestroyRenderPass(VkDevice d, VkRenderPass rp, const VkAllocationCallbacks* allocator) {
    Run([=] {
        if (d && d == vk.guestDevice && rp && rp == vk.guestRenderPass && !allocator &&
            !vk.framebuffer && !vk.graphicsPipeline && vk.frame == Frame::Idle) {
            DeviceDriver<PFN_vkDestroyRenderPass>("vkDestroyRenderPass")(vk.device, vk.renderPass, nullptr);
            vk.renderPass = vk.guestRenderPass = VK_NULL_HANDLE;
        }
        return VK_SUCCESS;
    });
}

/// Creates a framebuffer binding the offscreen RGBA8 image view to the render pass.
VkResult VKAPI_CALL CreateFramebuffer(VkDevice d, const VkFramebufferCreateInfo* info,
                                      const VkAllocationCallbacks* allocator, VkFramebuffer* out) {
    if (!out) return VK_ERROR_INITIALIZATION_FAILED;
    *out = VK_NULL_HANDLE;
    if (!info || info->sType != VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO || info->pNext || info->flags || allocator ||
        info->attachmentCount != 1 || !info->pAttachments || info->layers != 1)
        return VK_ERROR_FEATURE_NOT_PRESENT;
    return Run([=] {
        if (!d || d != vk.guestDevice || vk.framebuffer || vk.frame != Frame::Idle ||
            info->renderPass != vk.guestRenderPass || info->pAttachments[0] != vk.guestImageView ||
            info->width != vk.offscreenExtent.width || info->height != vk.offscreenExtent.height)
            return VK_ERROR_INITIALIZATION_FAILED;
        auto host = *info;
        host.renderPass = vk.renderPass;
        host.pAttachments = &vk.imageView;
        const auto result = DeviceDriver<PFN_vkCreateFramebuffer>("vkCreateFramebuffer")(vk.device, &host, nullptr, &vk.framebuffer);
        if (result == VK_SUCCESS) *out = vk.guestFramebuffer = reinterpret_cast<VkFramebuffer>(nextHandle++);
        return result;
    });
}

/// Destroys the offscreen framebuffer when idle.
void VKAPI_CALL DestroyFramebuffer(VkDevice d, VkFramebuffer fb, const VkAllocationCallbacks* allocator) {
    Run([=] {
        if (d && d == vk.guestDevice && fb && fb == vk.guestFramebuffer && !allocator && vk.frame == Frame::Idle) {
            DeviceDriver<PFN_vkDestroyFramebuffer>("vkDestroyFramebuffer")(vk.device, vk.framebuffer, nullptr);
            vk.framebuffer = vk.guestFramebuffer = VK_NULL_HANDLE;
        }
        return VK_SUCCESS;
    });
}

/// Creates an empty pipeline layout for the bounded graphics shader profile.
VkResult VKAPI_CALL CreatePipelineLayout(VkDevice d, const VkPipelineLayoutCreateInfo* info,
                                         const VkAllocationCallbacks* allocator, VkPipelineLayout* out) {
    if (!out) return VK_ERROR_INITIALIZATION_FAILED;
    *out = VK_NULL_HANDLE;
    if (!info || info->sType != VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO || info->pNext || info->flags || allocator ||
        info->setLayoutCount || info->pushConstantRangeCount) return VK_ERROR_FEATURE_NOT_PRESENT;
    return Run([=] {
        if (!d || d != vk.guestDevice || vk.pipelineLayout || vk.frame != Frame::Idle) return VK_ERROR_INITIALIZATION_FAILED;
        const auto result = DeviceDriver<PFN_vkCreatePipelineLayout>("vkCreatePipelineLayout")(vk.device, info, nullptr, &vk.pipelineLayout);
        if (result == VK_SUCCESS) *out = vk.guestPipelineLayout = reinterpret_cast<VkPipelineLayout>(nextHandle++);
        return result;
    });
}

/// Destroys the pipeline layout once the graphics pipeline is gone.
void VKAPI_CALL DestroyPipelineLayout(VkDevice d, VkPipelineLayout layout, const VkAllocationCallbacks* allocator) {
    Run([=] {
        if (d && d == vk.guestDevice && layout && layout == vk.guestPipelineLayout && !allocator &&
            !vk.graphicsPipeline && vk.frame == Frame::Idle) {
            DeviceDriver<PFN_vkDestroyPipelineLayout>("vkDestroyPipelineLayout")(vk.device, vk.pipelineLayout, nullptr);
            vk.pipelineLayout = vk.guestPipelineLayout = VK_NULL_HANDLE;
        }
        return VK_SUCCESS;
    });
}

/// Compiles one vertex+fragment graphics pipeline against the owned render pass and layout.
VkResult VKAPI_CALL CreateGraphicsPipelines(VkDevice d, VkPipelineCache cache, uint32_t count,
                                            const VkGraphicsPipelineCreateInfo* infos,
                                            const VkAllocationCallbacks* allocator, VkPipeline* out) {
    if (!out) return VK_ERROR_INITIALIZATION_FAILED;
    if (count) *out = VK_NULL_HANDLE;
    if (cache || count != 1 || !infos || allocator) return VK_ERROR_FEATURE_NOT_PRESENT;
    const auto& ci = infos[0];
    if (ci.sType != VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO || ci.pNext || ci.flags ||
        ci.stageCount != 2 || !ci.pStages || !ci.pVertexInputState || !ci.pInputAssemblyState ||
        ci.pTessellationState || !ci.pViewportState || !ci.pRasterizationState || !ci.pMultisampleState ||
        ci.pDepthStencilState || !ci.pColorBlendState || ci.pDynamicState || ci.subpass != 0 || ci.basePipelineHandle)
        return VK_ERROR_FEATURE_NOT_PRESENT;
    if (ci.pVertexInputState->sType != VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO || ci.pVertexInputState->pNext ||
        ci.pVertexInputState->vertexBindingDescriptionCount || ci.pVertexInputState->vertexAttributeDescriptionCount ||
        ci.pInputAssemblyState->sType != VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO || ci.pInputAssemblyState->pNext ||
        ci.pInputAssemblyState->topology != VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST || ci.pInputAssemblyState->primitiveRestartEnable ||
        ci.pViewportState->sType != VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO || ci.pViewportState->pNext ||
        ci.pViewportState->viewportCount != 1 || !ci.pViewportState->pViewports ||
        ci.pViewportState->scissorCount != 1 || !ci.pViewportState->pScissors ||
        ci.pRasterizationState->sType != VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO || ci.pRasterizationState->pNext ||
        ci.pMultisampleState->sType != VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO || ci.pMultisampleState->pNext ||
        ci.pMultisampleState->rasterizationSamples != VK_SAMPLE_COUNT_1_BIT ||
        ci.pColorBlendState->sType != VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO || ci.pColorBlendState->pNext ||
        ci.pColorBlendState->attachmentCount != 1 || !ci.pColorBlendState->pAttachments)
        return VK_ERROR_FEATURE_NOT_PRESENT;
    return Run([=] {
        if (!d || d != vk.guestDevice || vk.graphicsPipeline || vk.frame != Frame::Idle ||
            ci.layout != vk.guestPipelineLayout || ci.renderPass != vk.guestRenderPass)
            return VK_ERROR_INITIALIZATION_FAILED;
        VkPipelineShaderStageCreateInfo stages[2]{ci.pStages[0], ci.pStages[1]};
        bool sawVert = false, sawFrag = false;
        for (int i = 0; i < 2; ++i) {
            if (stages[i].sType != VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO || stages[i].pNext || stages[i].flags ||
                stages[i].pSpecializationInfo || !stages[i].pName || std::strcmp(stages[i].pName, "main") != 0)
                return VK_ERROR_FEATURE_NOT_PRESENT;
            VkShaderModule hostMod = VK_NULL_HANDLE;
            for (const auto& s : vk.shaders) if (s.guest && s.guest == stages[i].module) hostMod = s.host;
            if (!hostMod) return VK_ERROR_INITIALIZATION_FAILED;
            stages[i].module = hostMod;
            if (stages[i].stage == VK_SHADER_STAGE_VERTEX_BIT) sawVert = true;
            else if (stages[i].stage == VK_SHADER_STAGE_FRAGMENT_BIT) sawFrag = true;
        }
        if (!sawVert || !sawFrag) return VK_ERROR_FEATURE_NOT_PRESENT;
        auto host = ci;
        host.pStages = stages;
        host.layout = vk.pipelineLayout;
        host.renderPass = vk.renderPass;
        const auto result = DeviceDriver<PFN_vkCreateGraphicsPipelines>("vkCreateGraphicsPipelines")
            (vk.device, VK_NULL_HANDLE, 1, &host, nullptr, &vk.graphicsPipeline);
        if (result == VK_SUCCESS) *out = vk.guestGraphicsPipeline = reinterpret_cast<VkPipeline>(nextHandle++);
        return result;
    });
}

/// Destroys the graphics pipeline when idle.
void VKAPI_CALL DestroyPipeline(VkDevice d, VkPipeline pipeline, const VkAllocationCallbacks* allocator) {
    Run([=] {
        if (d && d == vk.guestDevice && pipeline && pipeline == vk.guestGraphicsPipeline && !allocator && vk.frame == Frame::Idle) {
            DeviceDriver<PFN_vkDestroyPipeline>("vkDestroyPipeline")(vk.device, vk.graphicsPipeline, nullptr);
            vk.graphicsPipeline = vk.guestGraphicsPipeline = VK_NULL_HANDLE;
        }
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
        if (vk.deviceLost) return VK_ERROR_DEVICE_LOST;
        if (auto result = CheckSwapchain(); result != VK_SUCCESS) return result;
        if (DeviceDriver<PFN_vkGetFenceStatus>("vkGetFenceStatus")(vk.device, vk.fence) != VK_NOT_READY)
            return VK_ERROR_INITIALIZATION_FAILED;
        const auto result = DeviceDriver<PFN_vkAcquireNextImageKHR>("vkAcquireNextImageKHR")
            (vk.device, vk.swapchain, std::min(timeout, kWaitNs), VK_NULL_HANDLE, vk.fence, index);
        if (result == VK_ERROR_DEVICE_LOST) vk.deviceLost = true;
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

/// Begins recording either a swapchain frame (after acquire fence) or an offscreen graphics pass (when Idle).
VkResult VKAPI_CALL BeginCommand(VkCommandBuffer cmd, const VkCommandBufferBeginInfo* info) {
    if (!info || info->sType != VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO || info->pNext || info->pInheritanceInfo ||
        info->flags != VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT) return VK_ERROR_FEATURE_NOT_PRESENT;
    return Run([=] {
        if (!cmd || cmd != vk.guestCommand || (vk.frame != Frame::Ready && vk.frame != Frame::Idle))
            return VK_ERROR_INITIALIZATION_FAILED;
        const auto result = DeviceDriver<PFN_vkBeginCommandBuffer>("vkBeginCommandBuffer")(vk.command, info);
        if (result == VK_SUCCESS) {
            if (vk.frame == Frame::Ready) {
                vk.frame = Frame::Recording;
                vk.commands = 0;
            } else {
                vk.frame = Frame::OffscreenRecording;
                vk.offscreenStep = 0;
            }
        }
        return result;
    });
}

/// Begins the single offscreen render pass.
void VKAPI_CALL BeginRenderPass(VkCommandBuffer cmd, const VkRenderPassBeginInfo* info, VkSubpassContents contents) {
    Run([=] {
        if (!cmd || cmd != vk.guestCommand || vk.frame != Frame::OffscreenRecording) return VK_ERROR_INITIALIZATION_FAILED;
        if (vk.offscreenStep != 0 || contents != VK_SUBPASS_CONTENTS_INLINE || !info ||
            info->sType != VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO || info->pNext ||
            info->renderPass != vk.guestRenderPass || info->framebuffer != vk.guestFramebuffer ||
            info->renderArea.offset.x || info->renderArea.offset.y ||
            info->renderArea.extent.width != vk.offscreenExtent.width ||
            info->renderArea.extent.height != vk.offscreenExtent.height ||
            info->clearValueCount != 1 || !info->pClearValues) {
            vk.frame = Frame::Invalid;
            return VK_ERROR_FEATURE_NOT_PRESENT;
        }
        auto host = *info;
        host.renderPass = vk.renderPass;
        host.framebuffer = vk.framebuffer;
        DeviceDriver<PFN_vkCmdBeginRenderPass>("vkCmdBeginRenderPass")(vk.command, &host, contents);
        vk.offscreenStep = 1;
        return VK_SUCCESS;
    });
}

/// Binds the graphics pipeline inside the offscreen render pass.
void VKAPI_CALL BindPipeline(VkCommandBuffer cmd, VkPipelineBindPoint point, VkPipeline pipeline) {
    Run([=] {
        if (!cmd || cmd != vk.guestCommand || vk.frame != Frame::OffscreenRecording) return VK_ERROR_INITIALIZATION_FAILED;
        if (vk.offscreenStep != 1 || point != VK_PIPELINE_BIND_POINT_GRAPHICS || pipeline != vk.guestGraphicsPipeline) {
            vk.frame = Frame::Invalid;
            return VK_ERROR_FEATURE_NOT_PRESENT;
        }
        DeviceDriver<PFN_vkCmdBindPipeline>("vkCmdBindPipeline")(vk.command, point, vk.graphicsPipeline);
        vk.offscreenStep = 2;
        return VK_SUCCESS;
    });
}

/// Draws a non-indexed triangle list inside the bound offscreen graphics pass.
void VKAPI_CALL CmdDraw(VkCommandBuffer cmd, uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance) {
    Run([=] {
        if (!cmd || cmd != vk.guestCommand || vk.frame != Frame::OffscreenRecording) return VK_ERROR_INITIALIZATION_FAILED;
        if (vk.offscreenStep != 2 || vertexCount == 0 || vertexCount > 64 || instanceCount != 1 || firstVertex || firstInstance) {
            vk.frame = Frame::Invalid;
            return VK_ERROR_FEATURE_NOT_PRESENT;
        }
        DeviceDriver<PFN_vkCmdDraw>("vkCmdDraw")(vk.command, vertexCount, instanceCount, firstVertex, firstInstance);
        vk.offscreenStep = 3;
        return VK_SUCCESS;
    });
}

/// Ends the offscreen render pass, transitioning the attachment to VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL.
void VKAPI_CALL EndRenderPass(VkCommandBuffer cmd) {
    Run([=] {
        if (!cmd || cmd != vk.guestCommand || vk.frame != Frame::OffscreenRecording) return VK_ERROR_INITIALIZATION_FAILED;
        if (vk.offscreenStep != 3) {
            vk.frame = Frame::Invalid;
            return VK_ERROR_FEATURE_NOT_PRESENT;
        }
        DeviceDriver<PFN_vkCmdEndRenderPass>("vkCmdEndRenderPass")(vk.command);
        vk.offscreenStep = 4;
        return VK_SUCCESS;
    });
}

/// Copies the rendered offscreen 2D image into the bound coherent readback buffer.
void VKAPI_CALL CopyImageToBuffer(VkCommandBuffer cmd, VkImage srcImage, VkImageLayout srcLayout,
                                  VkBuffer dstBuffer, uint32_t regionCount, const VkBufferImageCopy* regions) {
    Run([=] {
        if (!cmd || cmd != vk.guestCommand || vk.frame != Frame::OffscreenRecording) return VK_ERROR_INITIALIZATION_FAILED;
        const VkDeviceSize needed = VkDeviceSize{vk.offscreenExtent.width} * vk.offscreenExtent.height * 4u;
        if (vk.offscreenStep != 4 || srcImage != vk.guestOffscreenImage || srcLayout != VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL ||
            dstBuffer != vk.guestBuffer || !vk.bound || vk.mapped || regionCount != 1 || !regions || needed != vk.bufferSize) {
            vk.frame = Frame::Invalid;
            return VK_ERROR_FEATURE_NOT_PRESENT;
        }
        const auto& r = regions[0];
        if (r.bufferOffset || r.bufferRowLength || r.bufferImageHeight ||
            r.imageSubresource.aspectMask != VK_IMAGE_ASPECT_COLOR_BIT || r.imageSubresource.mipLevel ||
            r.imageSubresource.baseArrayLayer || r.imageSubresource.layerCount != 1 ||
            r.imageOffset.x || r.imageOffset.y || r.imageOffset.z ||
            r.imageExtent.width != vk.offscreenExtent.width || r.imageExtent.height != vk.offscreenExtent.height || r.imageExtent.depth != 1) {
            vk.frame = Frame::Invalid;
            return VK_ERROR_FEATURE_NOT_PRESENT;
        }
        DeviceDriver<PFN_vkCmdCopyImageToBuffer>("vkCmdCopyImageToBuffer")
            (vk.command, vk.offscreenImage, srcLayout, vk.buffer, 1, regions);
        vk.offscreenStep = 5;
        return VK_SUCCESS;
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
        if (!cmd || cmd != vk.guestCommand || (vk.frame != Frame::Recording && vk.frame != Frame::OffscreenRecording))
            return VK_ERROR_INITIALIZATION_FAILED;
        if (((vk.frame == Frame::Recording && vk.commands == 4) ||
             (vk.frame == Frame::OffscreenRecording && vk.offscreenStep == 5)) &&
            !flags && !memoryCount && !count && bufferCount == 1 && buffers &&
            src == VK_PIPELINE_STAGE_TRANSFER_BIT && dst == VK_PIPELINE_STAGE_HOST_BIT) {
            auto host = buffers[0];
            if (host.sType == VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER && !host.pNext && host.buffer == vk.guestBuffer &&
                host.srcAccessMask == VK_ACCESS_TRANSFER_WRITE_BIT && host.dstAccessMask == VK_ACCESS_HOST_READ_BIT &&
                host.srcQueueFamilyIndex == VK_QUEUE_FAMILY_IGNORED && host.dstQueueFamilyIndex == VK_QUEUE_FAMILY_IGNORED &&
                !host.offset && host.size == vk.bufferSize) {
                host.buffer = vk.buffer;
                DeviceDriver<PFN_vkCmdPipelineBarrier>("vkCmdPipelineBarrier")(vk.command, src, dst, 0, 0, nullptr, 1, &host, 0, nullptr);
                if (vk.frame == Frame::Recording) ++vk.commands;
                else ++vk.offscreenStep;
                return VK_SUCCESS;
            }
        }
        if (vk.frame != Frame::Recording) {
            vk.frame = Frame::Invalid;
            return VK_ERROR_FEATURE_NOT_PRESENT;
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

/// Ends a transfer clear or offscreen graphics pass with its required barrier.
VkResult VKAPI_CALL EndCommand(VkCommandBuffer cmd) {
    return Run([=] {
        if (!cmd || cmd != vk.guestCommand ||
            (vk.frame != Frame::Recording && vk.frame != Frame::OffscreenRecording && vk.frame != Frame::Invalid))
            return VK_ERROR_INITIALIZATION_FAILED;
        const bool wasOffscreen = vk.frame == Frame::OffscreenRecording;
        const bool complete = (vk.frame == Frame::Recording && (vk.commands == 3 || vk.commands == 5)) ||
                              (wasOffscreen && vk.offscreenStep == 6);
        const auto result = DeviceDriver<PFN_vkEndCommandBuffer>("vkEndCommandBuffer")(vk.command);
        if (complete && result == VK_SUCCESS) {
            vk.frame = wasOffscreen ? Frame::OffscreenExecutable : Frame::Executable;
        } else {
            vk.frame = wasOffscreen ? Frame::Idle : Frame::Ready;
        }
        return complete ? result : VK_ERROR_FEATURE_NOT_PRESENT;
    });
}

/// Submits one audited command buffer; no semaphores or foreign objects are accepted.
VkResult VKAPI_CALL Submit(VkQueue q, uint32_t n, const VkSubmitInfo* info, VkFence fence) {
    return Run([=] {
        if (!q || q != vk.guestQueue || n != 1 || !info || info->sType != VK_STRUCTURE_TYPE_SUBMIT_INFO ||
            info->pNext || info->waitSemaphoreCount || info->signalSemaphoreCount || fence ||
            info->commandBufferCount != 1 || !info->pCommandBuffers || *info->pCommandBuffers != vk.guestCommand ||
            (vk.frame != Frame::Executable && vk.frame != Frame::OffscreenExecutable)) return VK_ERROR_INITIALIZATION_FAILED;
        if (vk.deviceLost) return VK_ERROR_DEVICE_LOST;
        static bool injectedDeviceLoss = false;
        if (!injectedDeviceLoss && GetEnvironmentVariableW(L"WINAPK_TEST_VK_DEVICE_LOST", nullptr, 0) > 0) {
            injectedDeviceLoss = true;
            vk.deviceLost = true;
            vk.frame = Frame::Idle;
            LogLine("GRAPHICS_RENDER_FAILED: guest Vulkan vkQueueSubmit error=-4 (injected device loss)");
            return VK_ERROR_DEVICE_LOST;
        }
        const bool offscreen = vk.frame == Frame::OffscreenExecutable;
        auto host = *info;
        host.pCommandBuffers = &vk.command;
        if (offscreen || vk.commands == 5) vk.bufferReadable = false;
        const auto result = DeviceDriver<PFN_vkQueueSubmit>("vkQueueSubmit")(vk.queue, 1, &host, VK_NULL_HANDLE);
        if (result == VK_ERROR_DEVICE_LOST) vk.deviceLost = true;
        if (offscreen) vk.frame = result == VK_SUCCESS ? Frame::OffscreenSubmitted : Frame::Idle;
        else vk.frame = result == VK_SUCCESS ? Frame::Submitted : Frame::Ready;
        return result;
    });
}

/// Synchronizes the one queue without holding JVM or surface registry locks.
VkResult VKAPI_CALL QueueIdle(VkQueue q) {
    return Run([=] {
        if (!q || q != vk.guestQueue) return VK_ERROR_INITIALIZATION_FAILED;
        if (vk.deviceLost) return VK_ERROR_DEVICE_LOST;
        const auto result = DeviceDriver<PFN_vkQueueWaitIdle>("vkQueueWaitIdle")(vk.queue);
        if (result == VK_ERROR_DEVICE_LOST) vk.deviceLost = true;
        if (vk.frame == Frame::Submitted) {
            vk.frame = result == VK_SUCCESS ? Frame::Complete : Frame::Ready;
            if (result == VK_SUCCESS && vk.commands == 5) vk.bufferReadable = true;
        } else if (vk.frame == Frame::OffscreenSubmitted) {
            vk.frame = Frame::Idle;
            if (result == VK_SUCCESS) vk.bufferReadable = true;
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
enum class Scope { Global, Instance, Surface, AndroidSurface, DebugReport, Device };
struct Entry { guest::HostSymbol symbol; Scope scope; bool exported = true; };
/// Type-checks each registered wrapper against the locked Vulkan Headers PFN.
template<class P, class F> Entry CheckedEntry(const char* name, F fn, Scope scope, bool exported = true) {
    static_assert(std::is_same_v<P, F>);
    return {{name, reinterpret_cast<void*>(fn)}, scope, exported};
}
#define ENTRY(name, fn, scope) CheckedEntry<PFN_##name>(#name, &fn, Scope::scope)
#define EXT_ENTRY(name, fn, scope) CheckedEntry<PFN_##name>(#name, &fn, Scope::scope, false)
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
    EXT_ENTRY(vkCreateDebugReportCallbackEXT, CreateDebugReportCallback, DebugReport),
    EXT_ENTRY(vkDestroyDebugReportCallbackEXT, DestroyDebugReportCallback, DebugReport),
    EXT_ENTRY(vkDebugReportMessageEXT, DebugReportMessage, DebugReport),
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
    ENTRY(vkCreateImage, CreateImage, Device),
    ENTRY(vkGetImageMemoryRequirements, ImageRequirements, Device),
    ENTRY(vkBindImageMemory, BindImage, Device),
    ENTRY(vkDestroyImage, DestroyImage, Device),
    ENTRY(vkCreateImageView, CreateImageView, Device),
    ENTRY(vkDestroyImageView, DestroyImageView, Device),
    ENTRY(vkCreateShaderModule, CreateShaderModule, Device),
    ENTRY(vkDestroyShaderModule, DestroyShaderModule, Device),
    ENTRY(vkCreateRenderPass, CreateRenderPass, Device),
    ENTRY(vkDestroyRenderPass, DestroyRenderPass, Device),
    ENTRY(vkCreateFramebuffer, CreateFramebuffer, Device),
    ENTRY(vkDestroyFramebuffer, DestroyFramebuffer, Device),
    ENTRY(vkCreatePipelineLayout, CreatePipelineLayout, Device),
    ENTRY(vkDestroyPipelineLayout, DestroyPipelineLayout, Device),
    ENTRY(vkCreateGraphicsPipelines, CreateGraphicsPipelines, Device),
    ENTRY(vkDestroyPipeline, DestroyPipeline, Device),
    ENTRY(vkCmdBeginRenderPass, BeginRenderPass, Device),
    ENTRY(vkCmdBindPipeline, BindPipeline, Device),
    ENTRY(vkCmdDraw, CmdDraw, Device),
    ENTRY(vkCmdEndRenderPass, EndRenderPass, Device),
    ENTRY(vkCmdCopyImageToBuffer, CopyImageToBuffer, Device),
    ENTRY(vkEndCommandBuffer, EndCommand, Device),
    ENTRY(vkQueueSubmit, Submit, Device),
    ENTRY(vkQueueWaitIdle, QueueIdle, Device),
    ENTRY(vkQueuePresentKHR, Present, Device),
};
#undef EXT_ENTRY
#undef ENTRY

/// Resolves only a declared bridge entry, never a driver pointer.
PFN_vkVoidFunction VKAPI_CALL Proc(VkInstance instance, const char* name) {
    if (!name || strnlen(name, 128) == 128) return nullptr;
    for (const auto& entry : entries) {
        if (std::strcmp(entry.symbol.name, name) != 0) continue;
        if (entry.scope == Scope::Global && !instance) return reinterpret_cast<PFN_vkVoidFunction>(entry.symbol.address);
        if (!instance || entry.scope == Scope::Device) return nullptr;
        return Run([&] {
            return instance == vk.guestInstance &&
                (entry.scope != Scope::Surface || vk.surfaceExtension) &&
                (entry.scope != Scope::AndroidSurface || vk.androidSurfaceExtension) &&
                (entry.scope != Scope::DebugReport || vk.debugReportExtension)
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
    for (const auto& entry : entries) if (entry.exported) library.symbols.push_back(entry.symbol);
    libraries.push_back(std::move(library));
}

void SetGuestVulkanModules(guest::ModuleSet* modules) { g_guestModules = modules; }

bool GuestVulkanOwnsWindow() { return vk.surface && !vk.retired; }

/// Allows Win32 to validate paint without overwriting the active swapchain's contents.
bool GuestVulkanPresents(HWND window) { return vk.window == window && !vk.retired && vk.presented; }

void RetireGuestVulkan(HWND window, bool closing) {
    if (closing && (!vk.window || vk.window == window)) Shutdown();
    else if (vk.window == window) vk.retired = true;
}

} // namespace winapk::bridge
