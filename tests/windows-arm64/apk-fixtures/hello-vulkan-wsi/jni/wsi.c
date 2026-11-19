#include <android/native_window_jni.h>
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#define VK_USE_PLATFORM_ANDROID_KHR
#include <vulkan/vulkan.h>

static const uint32_t tri_vert_spv[] =
#include "tri.vert.inc"
;
static const uint32_t tri_frag_spv[] =
#include "tri.frag.inc"
;

_Static_assert(sizeof(VkInstanceCreateInfo) == 64 && sizeof(VkApplicationInfo) == 48, "instance ABI");
_Static_assert(sizeof(VkAndroidSurfaceCreateInfoKHR) == 32 && offsetof(VkAndroidSurfaceCreateInfoKHR, window) == 24, "Android surface ABI");
_Static_assert(sizeof(VkSurfaceCapabilitiesKHR) == 52 && sizeof(VkQueueFamilyProperties) == 24, "query ABI");

static VkInstance instance;
static VkPhysicalDevice physical;
static VkSurfaceKHR surface;
static VkDevice device;
static VkQueue queue;
static VkSwapchainKHR swapchain;
static VkCommandPool pool;
static VkCommandBuffer command;
static VkFence acquired;
static VkBuffer readback;
static VkDeviceMemory storage;
enum { readback_words = 16 };
static uint32_t family;
static ANativeWindow* native;
static PFN_vkCreateAndroidSurfaceKHR create_surface;
static PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR caps;
static PFN_vkQueuePresentKHR present_frame;
static PFN_vkDestroyDebugReportCallbackEXT destroy_debug;
static VkDebugReportCallbackEXT debug_handle;
static PFN_vkDestroyDebugUtilsMessengerEXT destroy_messenger;
static VkDebugUtilsMessengerEXT messenger_handle;

typedef struct {
    uint32_t magic;
    uint32_t allocs;
    uint32_t frees;
    uint32_t fail_next;
} guest_alloc_state;

static void* VKAPI_PTR guest_alloc(void* user, size_t size, size_t align, VkSystemAllocationScope scope) {
    guest_alloc_state* st = (guest_alloc_state*)user;
    (void)scope;
    if (!st || st->magic != 0xA110CA7Eu) return NULL;
    if (st->fail_next) {
        st->fail_next = 0;
        return NULL;
    }
    (void)align;
    void* mem = malloc(size ? size : 1);
    if (mem) st->allocs++;
    return mem;
}

static void* VKAPI_PTR guest_realloc(void* user, void* orig, size_t size, size_t align, VkSystemAllocationScope scope) {
    guest_alloc_state* st = (guest_alloc_state*)user;
    (void)scope;
    if (!st || st->magic != 0xA110CA7Eu) return NULL;
    if (!orig) return guest_alloc(user, size, align, scope);
    if (!size) {
        st->frees++;
        free(orig);
        return NULL;
    }
    return realloc(orig, size);
}

static void VKAPI_PTR guest_free(void* user, void* mem) {
    guest_alloc_state* st = (guest_alloc_state*)user;
    if (!st || st->magic != 0xA110CA7Eu || !mem) return;
    st->frees++;
    free(mem);
}

typedef struct {
    uint32_t seen;
    int32_t code;
    uint32_t utils_seen;
} guest_debug_state;

static VkBool32 VKAPI_PTR guest_debug_cb(VkDebugReportFlagsEXT flags, VkDebugReportObjectTypeEXT obj_type,
                                         uint64_t obj, size_t loc, int32_t code,
                                         const char* prefix, const char* msg, void* user) {
    guest_debug_state* st = (guest_debug_state*)user;
    if (st && (flags & VK_DEBUG_REPORT_WARNING_BIT_EXT) && obj_type == VK_DEBUG_REPORT_OBJECT_TYPE_INSTANCE_EXT &&
        obj == 0x42u && loc == 7u && prefix && strcmp(prefix, "t13") == 0 && msg && strcmp(msg, "guest-cb-ok") == 0) {
        st->seen++;
        st->code = code;
    }
    return VK_FALSE;
}

static VkBool32 VKAPI_PTR guest_utils_cb(VkDebugUtilsMessageSeverityFlagBitsEXT sev,
                                         VkDebugUtilsMessageTypeFlagsEXT types,
                                         const VkDebugUtilsMessengerCallbackDataEXT* data,
                                         void* user) {
    guest_debug_state* st = (guest_debug_state*)user;
    if (st && (sev & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) &&
        (types & VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT) &&
        data && data->pMessage && strcmp(data->pMessage, "guest-utils-ok") == 0) {
        st->utils_seen++;
    }
    return VK_FALSE;
}

static guest_debug_state debug_state;

/** Checks the fixed device, synchronization and command buffer layouts on Android ARM64. */
_Static_assert(sizeof(VkDeviceQueueCreateInfo) == 40 && sizeof(VkDeviceCreateInfo) == 72 &&
    sizeof(VkSwapchainCreateInfoKHR) == 104 && sizeof(VkImageMemoryBarrier) == 72 &&
    sizeof(VkSubmitInfo) == 72 && sizeof(VkPresentInfoKHR) == 64, "render ABI");
_Static_assert(sizeof(VkBufferCreateInfo) == 56 && sizeof(VkMemoryAllocateInfo) == 32 &&
    sizeof(VkMemoryRequirements) == 24 && sizeof(VkPhysicalDeviceMemoryProperties) == 520 &&
    sizeof(VkBufferMemoryBarrier) == 56, "transfer ABI");
_Static_assert(sizeof(VkAllocationCallbacks) == 48 && sizeof(VkShaderModuleCreateInfo) == 40 &&
    sizeof(VkImageCreateInfo) == 88 && sizeof(VkImageViewCreateInfo) == 80 &&
    sizeof(VkRenderPassCreateInfo) == 64 && sizeof(VkFramebufferCreateInfo) == 64 &&
    sizeof(VkPipelineLayoutCreateInfo) == 48 && sizeof(VkGraphicsPipelineCreateInfo) == 144 &&
    sizeof(VkRenderPassBeginInfo) == 64 && sizeof(VkBufferImageCopy) == 56, "graphics pipeline ABI");

/** Creates real WSI and checks that the bridge, rather than the guest, retains its native window. */
static int attach(JNIEnv* env, jobject holder) {
    native = ANativeWindow_fromSurface(env, holder);
    if (!native) return 0;
    VkAndroidSurfaceCreateInfoKHR ci = { .sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR, .window = native };
    const VkResult result = create_surface(instance, &ci, NULL, &surface);
    ANativeWindow_release(native);
    return result == VK_SUCCESS && ANativeWindow_getWidth(native) > 0;
}

static int has_ext(const VkExtensionProperties* list, uint32_t n, const char* want) {
    for (uint32_t i = 0; i < n; ++i) {
        if (strcmp(list[i].extensionName, want) == 0) return 1;
    }
    return 0;
}

/** Checks Vulkan 1.2/1.3 version, extension filtering, pNext properties2, callbacks and WSI. */
JNIEXPORT jint JNICALL Java_org_winapk_fixture_vulkanwsi_MainActivity_open(JNIEnv* env, jclass type, jobject holder) {
    (void)type;
    uint32_t version = 0;
    PFN_vkEnumerateInstanceVersion get_version = (PFN_vkEnumerateInstanceVersion)vkGetInstanceProcAddr(NULL, "vkEnumerateInstanceVersion");
    if (!get_version || get_version(&version) || version < VK_API_VERSION_1_2 ||
        vkGetInstanceProcAddr(NULL, "vkCreateAndroidSurfaceKHR") || vkGetInstanceProcAddr(NULL, "vkInvalidFunction")) return 1;

    uint32_t count = 0;
    VkResult result = vkEnumerateInstanceExtensionProperties(NULL, &count, NULL);
    if (result == VK_ERROR_INITIALIZATION_FAILED || (!result && !count)) return -1;
    if (result || count < 4 || count > 128) return 2;
    VkExtensionProperties exts[128];
    uint32_t one = 1;
    if (vkEnumerateInstanceExtensionProperties(NULL, &one, exts) != VK_INCOMPLETE || one != 1) return 3;
    if (vkEnumerateInstanceExtensionProperties(NULL, &count, exts) ||
        !has_ext(exts, count, VK_KHR_SURFACE_EXTENSION_NAME) ||
        !has_ext(exts, count, VK_KHR_ANDROID_SURFACE_EXTENSION_NAME) ||
        !has_ext(exts, count, VK_EXT_DEBUG_REPORT_EXTENSION_NAME) ||
        !has_ext(exts, count, VK_EXT_DEBUG_UTILS_EXTENSION_NAME) ||
        has_ext(exts, count, "VK_KHR_win32_surface") ||
        has_ext(exts, count, "VK_KHR_external_memory_win32") ||
        has_ext(exts, count, "VK_ANDROID_external_memory_android_hardware_buffer")) return 4;

    VkApplicationInfo app = { .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO, .apiVersion = VK_API_VERSION_1_2 };
    VkInstanceCreateInfo ci = { .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO, .pApplicationInfo = &app };
    const char* unsupported[] = {"VK_ANDROID_external_memory_android_hardware_buffer"};
    ci.enabledExtensionCount = 1;
    ci.ppEnabledExtensionNames = unsupported;
    if (vkCreateInstance(&ci, NULL, &instance) != VK_ERROR_EXTENSION_NOT_PRESENT || instance) return 5;
    const char* want[] = {
        VK_KHR_SURFACE_EXTENSION_NAME,
        VK_KHR_ANDROID_SURFACE_EXTENSION_NAME,
        VK_EXT_DEBUG_REPORT_EXTENSION_NAME,
        VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
    };
    ci.enabledExtensionCount = 4;
    ci.ppEnabledExtensionNames = want;
    VkAllocationCallbacks allocator = {0};
    if (vkCreateInstance(&ci, &allocator, &instance) != VK_ERROR_FEATURE_NOT_PRESENT || instance) return 6;
    ci.pNext = &app;
    if (vkCreateInstance(&ci, NULL, &instance) != VK_ERROR_FEATURE_NOT_PRESENT || instance) return 7;
    ci.pNext = NULL;
    ci.enabledExtensionCount = 1;
    result = vkCreateInstance(&ci, NULL, &instance);
    if (result == VK_ERROR_INCOMPATIBLE_DRIVER) return -1;
    if (result || !instance || !vkGetInstanceProcAddr(instance, "vkDestroySurfaceKHR") ||
        vkGetInstanceProcAddr(instance, "vkCreateAndroidSurfaceKHR") ||
        vkGetInstanceProcAddr(instance, "vkCreateDebugReportCallbackEXT") ||
        vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT")) return 15;
    vkDestroyInstance(instance, NULL);
    instance = VK_NULL_HANDLE;

    ci.enabledExtensionCount = 4;
    result = vkCreateInstance(&ci, NULL, &instance);
    if (result == VK_ERROR_INCOMPATIBLE_DRIVER) return -1;
    if (result || !instance) return 8;

    /* Verify VK_EXT_debug_report callback screening and invocation. */
    PFN_vkCreateDebugReportCallbackEXT create_debug =
        (PFN_vkCreateDebugReportCallbackEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugReportCallbackEXT");
    destroy_debug =
        (PFN_vkDestroyDebugReportCallbackEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugReportCallbackEXT");
    PFN_vkDebugReportMessageEXT send_debug =
        (PFN_vkDebugReportMessageEXT)vkGetInstanceProcAddr(instance, "vkDebugReportMessageEXT");
    if (!create_debug || !destroy_debug || !send_debug) return 16;
    VkDebugReportCallbackCreateInfoEXT bad_dbg = {
        .sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT,
        .flags = VK_DEBUG_REPORT_WARNING_BIT_EXT,
        .pfnCallback = (PFN_vkDebugReportCallbackEXT)(uintptr_t)4
    };
    if (create_debug(instance, &bad_dbg, NULL, &debug_handle) != VK_ERROR_FEATURE_NOT_PRESENT || debug_handle) return 17;
    debug_state.seen = 0;
    debug_state.code = 0;
    debug_state.utils_seen = 0;
    VkDebugReportCallbackCreateInfoEXT dbg_ci = {
        .sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT,
        .flags = VK_DEBUG_REPORT_WARNING_BIT_EXT,
        .pfnCallback = &guest_debug_cb,
        .pUserData = &debug_state
    };
    if (create_debug(instance, &dbg_ci, NULL, &debug_handle) != VK_SUCCESS || !debug_handle) return 18;
    send_debug(instance, VK_DEBUG_REPORT_WARNING_BIT_EXT, VK_DEBUG_REPORT_OBJECT_TYPE_INSTANCE_EXT,
               0x42u, 7u, 1234, "t13", "guest-cb-ok");
    if (debug_state.seen < 1 || debug_state.code != 1234) return 19;

    /* Verify VK_EXT_debug_utils messenger screening and invocation. */
    PFN_vkCreateDebugUtilsMessengerEXT create_utils =
        (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
    destroy_messenger =
        (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
    PFN_vkSubmitDebugUtilsMessageEXT send_utils =
        (PFN_vkSubmitDebugUtilsMessageEXT)vkGetInstanceProcAddr(instance, "vkSubmitDebugUtilsMessageEXT");
    if (!create_utils || !destroy_messenger || !send_utils) return 16;
    VkDebugUtilsMessengerCreateInfoEXT bad_utils = {
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
        .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT,
        .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT,
        .pfnUserCallback = (PFN_vkDebugUtilsMessengerCallbackEXT)(uintptr_t)8
    };
    if (create_utils(instance, &bad_utils, NULL, &messenger_handle) != VK_ERROR_FEATURE_NOT_PRESENT || messenger_handle) return 17;
    VkDebugUtilsMessengerCreateInfoEXT utils_ci = {
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
        .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT,
        .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT,
        .pfnUserCallback = &guest_utils_cb,
        .pUserData = &debug_state
    };
    if (create_utils(instance, &utils_ci, NULL, &messenger_handle) != VK_SUCCESS || !messenger_handle) return 18;
    VkDebugUtilsMessengerCallbackDataEXT utils_data = {
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CALLBACK_DATA_EXT,
        .pMessage = "guest-utils-ok"
    };
    send_utils(instance, VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT,
               VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT, &utils_data);
    if (debug_state.utils_seen < 1) return 19;

    create_surface = (PFN_vkCreateAndroidSurfaceKHR)vkGetInstanceProcAddr(instance, "vkCreateAndroidSurfaceKHR");
    caps = (PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR)vkGetInstanceProcAddr(instance, "vkGetPhysicalDeviceSurfaceCapabilitiesKHR");
    if (!create_surface || create_surface != &vkCreateAndroidSurfaceKHR || !caps ||
        vkGetInstanceProcAddr(instance, "vkCreateWin32SurfaceKHR") ||
        !vkGetInstanceProcAddr(instance, "vkCreateShaderModule") ||
        vkGetInstanceProcAddr((VkInstance)(uintptr_t)UINTPTR_MAX, "vkDestroyInstance")) return 9;
    VkAndroidSurfaceCreateInfoKHR invalid = {.sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR,
        .window = (ANativeWindow*)(uintptr_t)UINTPTR_MAX};
    if (create_surface(instance, &invalid, NULL, &surface) != VK_ERROR_SURFACE_LOST_KHR || surface || !attach(env, holder)) return 10;

    count = 0;
    if (vkEnumeratePhysicalDevices(instance, &count, NULL) || !count) return 11;
    count = 1;
    result = vkEnumeratePhysicalDevices(instance, &count, &physical);
    if ((result && result != VK_INCOMPLETE) || count != 1 || !physical) return 12;

    /* Verify Vulkan 1.1/1.2 pNext chain (`VkPhysicalDeviceProperties2` + `VkPhysicalDeviceDriverProperties`). */
    VkPhysicalDeviceDriverProperties drv_props = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES
    };
    VkPhysicalDeviceProperties2 props2 = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
        .pNext = &drv_props
    };
    vkGetPhysicalDeviceProperties2(physical, &props2);
    if (props2.properties.apiVersion < VK_API_VERSION_1_2 || !props2.properties.deviceName[0]) return 12;

    vkGetPhysicalDeviceQueueFamilyProperties(physical, &count, NULL);
    VkBool32 present = VK_FALSE;
    VkQueueFamilyProperties properties[32];
    if (count > 32) return 13;
    vkGetPhysicalDeviceQueueFamilyProperties(physical, &count, properties);
    for (uint32_t i = 0; i < count && !present; ++i) {
        if (!(properties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)) continue;
        if (vkGetPhysicalDeviceSurfaceSupportKHR(physical, i, surface, &present)) return 13;
        if (present) family = i;
    }
    if (!present || vkGetPhysicalDeviceSurfaceFormatsKHR(physical, surface, &count, NULL) || !count ||
        vkGetPhysicalDeviceSurfacePresentModesKHR(physical, surface, &count, NULL) || !count) return 14;
    return 0;
}

/** Allocates the device, queue, pool, primary buffer and acquire fence, checking extension filtering. */
static int start_device(void) {
    uint32_t count = 0;
    if (vkEnumerateDeviceExtensionProperties(physical, NULL, &count, NULL) || !count || count > 512) return 20;
    VkExtensionProperties* dev_exts = (VkExtensionProperties*)calloc(count, sizeof(VkExtensionProperties));
    if (!dev_exts) return 20;
    VkResult er = vkEnumerateDeviceExtensionProperties(physical, NULL, &count, dev_exts);
    int ext_ok = (er == VK_SUCCESS) &&
                 has_ext(dev_exts, count, VK_KHR_SWAPCHAIN_EXTENSION_NAME) &&
                 !has_ext(dev_exts, count, "VK_KHR_external_memory_win32") &&
                 !has_ext(dev_exts, count, "VK_KHR_external_memory_fd") &&
                 !has_ext(dev_exts, count, "VK_ANDROID_external_memory_android_hardware_buffer");
    free(dev_exts);
    if (!ext_ok) return 20;

    const float priority = 1.f;
    const char* bad_exts[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME, "VK_ANDROID_external_memory_android_hardware_buffer"};
    const char* extensions[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
    const VkDeviceQueueCreateInfo queue_info = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .queueFamilyIndex = family, .queueCount = 1, .pQueuePriorities = &priority
    };
    VkDevice bad_device = VK_NULL_HANDLE;
    const VkDeviceCreateInfo bad_device_info = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .queueCreateInfoCount = 1, .pQueueCreateInfos = &queue_info,
        .enabledExtensionCount = 2, .ppEnabledExtensionNames = bad_exts
    };
    if (vkCreateDevice(physical, &bad_device_info, NULL, &bad_device) != VK_ERROR_EXTENSION_NOT_PRESENT || bad_device) return 20;

    /* Enable Vulkan 1.1/1.2 features2 via pNext chain on vkCreateDevice. */
    VkPhysicalDeviceFeatures2 feat2 = { .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2 };
    vkGetPhysicalDeviceFeatures2(physical, &feat2);
    const VkDeviceCreateInfo device_info = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext = &feat2,
        .queueCreateInfoCount = 1, .pQueueCreateInfos = &queue_info,
        .enabledExtensionCount = 1, .ppEnabledExtensionNames = extensions
    };
    if (vkCreateDevice(physical, &device_info, NULL, &device)) return 21;
    vkGetDeviceQueue(device, family, 0, &queue);
    present_frame = (PFN_vkQueuePresentKHR)vkGetDeviceProcAddr(device, "vkQueuePresentKHR");
    if (!queue || !present_frame || present_frame != &vkQueuePresentKHR ||
        vkGetDeviceProcAddr(device, "vkCreateAndroidSurfaceKHR") ||
        vkGetDeviceProcAddr((VkDevice)(uintptr_t)UINTPTR_MAX, "vkQueuePresentKHR")) return 22;
    const VkCommandPoolCreateInfo pool_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, .queueFamilyIndex = family
    };
    if (vkCreateCommandPool(device, &pool_info, NULL, &pool)) return 23;
    const VkCommandBufferAllocateInfo alloc = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, .commandPool = pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY, .commandBufferCount = 1
    };
    if (vkAllocateCommandBuffers(device, &alloc, &command)) return 24;
    const VkFenceCreateInfo fence_info = {.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    return vkCreateFence(device, &fence_info, NULL, &acquired) == VK_SUCCESS ? 0 : 25;
}

/** Creates a FIFO swapchain for actual native surface pixels. */
static int start_swapchain(void) {
    VkSurfaceCapabilitiesKHR capabilities;
    if (caps(physical, surface, &capabilities) || !(capabilities.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT)) return 26;
    VkSurfaceFormatKHR formats[64];
    uint32_t count = 64;
    if (vkGetPhysicalDeviceSurfaceFormatsKHR(physical, surface, &count, formats) || !count) return 27;
    uint32_t format = 0;
    for (uint32_t i = 0; i < count; ++i)
        if (formats[i].format == VK_FORMAT_B8G8R8A8_UNORM || formats[i].format == VK_FORMAT_R8G8B8A8_UNORM) {
            format = i;
            break;
        }
    uint32_t alpha = capabilities.supportedCompositeAlpha;
    const VkSwapchainCreateInfoKHR info = {
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR, .surface = surface,
        .minImageCount = capabilities.minImageCount, .imageFormat = formats[format].format,
        .imageColorSpace = formats[format].colorSpace, .imageExtent = capabilities.currentExtent,
        .imageArrayLayers = 1, .imageUsage = VK_IMAGE_USAGE_TRANSFER_DST_BIT, .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .preTransform = capabilities.currentTransform, .compositeAlpha = alpha & (0u - alpha),
        .presentMode = VK_PRESENT_MODE_FIFO_KHR, .clipped = VK_TRUE
    };
    return vkCreateSwapchainKHR(device, &info, NULL, &swapchain) == VK_SUCCESS ? 0 : 28;
}

/** Reserves a coherent host-visible buffer and checks refusal of Android external memory pNext. */
static int start_readback(void) {
    const VkBufferCreateInfo info = { .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = readback_words * sizeof(uint32_t),
        .usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE };
    if (vkCreateBuffer(device, &info, NULL, &readback) || !readback) return 40;
    VkMemoryRequirements req;
    vkGetBufferMemoryRequirements(device, readback, &req);
    VkPhysicalDeviceMemoryProperties props;
    vkGetPhysicalDeviceMemoryProperties(physical, &props);
    const VkMemoryPropertyFlags want = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    uint32_t type = 0;
    for (; type < props.memoryTypeCount && type < 32; ++type)
        if ((req.memoryTypeBits & (1u << type)) && (props.memoryTypes[type].propertyFlags & want) == want) break;
    if (type >= props.memoryTypeCount || type >= 32 || req.size < info.size) return 41;
    const VkMemoryAllocateInfo allocation = { .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = req.size, .memoryTypeIndex = type };
    const VkImportAndroidHardwareBufferInfoANDROID ahb_next = {
        .sType = VK_STRUCTURE_TYPE_IMPORT_ANDROID_HARDWARE_BUFFER_INFO_ANDROID
    };
    const VkMemoryAllocateInfo bad_alloc = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, .pNext = &ahb_next,
        .allocationSize = req.size, .memoryTypeIndex = type
    };
    VkDeviceMemory bad_mem = VK_NULL_HANDLE;
    if (vkAllocateMemory(device, &bad_alloc, NULL, &bad_mem) != VK_ERROR_EXTENSION_NOT_PRESENT || bad_mem) return 42;
    const VkImportMemoryFdInfoKHR fd_next = {
        .sType = VK_STRUCTURE_TYPE_IMPORT_MEMORY_FD_INFO_KHR, .handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT, .fd = 3
    };
    const VkMemoryAllocateInfo bad_fd_alloc = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, .pNext = &fd_next,
        .allocationSize = req.size, .memoryTypeIndex = type
    };
    if (vkAllocateMemory(device, &bad_fd_alloc, NULL, &bad_mem) != VK_ERROR_EXTENSION_NOT_PRESENT || bad_mem) return 42;
    if (vkAllocateMemory(device, &allocation, NULL, &storage) || !storage) return 42;
    if (vkBindBufferMemory(device, readback, storage, 0)) return 43;
    void* pre_map = NULL;
    if (vkMapMemory(device, storage, 0, info.size, 0, &pre_map) || !pre_map) return 44;
    memset(pre_map, 0, info.size);
    vkUnmapMemory(device, storage);
    return 0;
}

/** Records layout transitions, color and optional transfer readback, then submits and presents. */
static int show_frame(float red, float blue) {
    VkImage images[16];
    uint32_t count = 16, index;
    if (vkGetSwapchainImagesKHR(device, swapchain, &count, images) || !count) return 29;
    if (vkResetFences(device, 1, &acquired)) return 30;
    VkResult result = vkAcquireNextImageKHR(device, swapchain, 1000000000u, VK_NULL_HANDLE, acquired, &index);
    if ((result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) || index >= count) return 31;
    if (vkWaitForFences(device, 1, &acquired, VK_TRUE, 1000000000u)) return 32;
    const VkCommandBufferBeginInfo begin = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
    };
    if (vkBeginCommandBuffer(command, &begin)) return 33;
    VkImageMemoryBarrier barrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED, .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED, .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = images[index], .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}
    };
    vkCmdPipelineBarrier(command, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
        0, 0, NULL, 0, NULL, 1, &barrier);
    const VkClearColorValue color = {.float32 = {red, 0.25f, blue, 1.f}};
    vkCmdClearColorImage(command, images[index], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &color, 1, &barrier.subresourceRange);
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = 0;
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    vkCmdPipelineBarrier(command, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
        0, 0, NULL, 0, NULL, 1, &barrier);
    const uint32_t pattern = UINT32_C(0x13579bdf);
    if (readback) {
        const VkBufferMemoryBarrier host_read = {
            .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
            .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT, .dstAccessMask = VK_ACCESS_HOST_READ_BIT,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED, .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .buffer = readback, .size = readback_words * sizeof(uint32_t)
        };
        vkCmdFillBuffer(command, readback, 0, host_read.size, pattern);
        vkCmdPipelineBarrier(command, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT,
            0, 0, NULL, 1, &host_read, 0, NULL);
    }
    if (vkEndCommandBuffer(command)) return 34;
    const VkSubmitInfo submit = {.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO, .commandBufferCount = 1, .pCommandBuffers = &command};
    if (vkQueueSubmit(queue, 1, &submit, VK_NULL_HANDLE) || vkQueueWaitIdle(queue)) return 35;
    if (readback) {
        void* mapped = NULL;
        if (vkMapMemory(device, storage, 0, readback_words * sizeof(uint32_t), 0, &mapped) || !mapped) return 45;
        const uint32_t* words = mapped;
        int matches = 1;
        for (uint32_t i = 0; i < readback_words; ++i)
            if (words[i] != pattern) matches = 0;
        vkUnmapMemory(device, storage);
        if (!matches) return 46;
    }
    VkResult single = VK_ERROR_INITIALIZATION_FAILED;
    const VkPresentInfoKHR present = {
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR, .swapchainCount = 1, .pSwapchains = &swapchain,
        .pImageIndices = &index, .pResults = &single
    };
    result = present_frame(queue, &present);
    if ((result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) ||
        (single != VK_SUCCESS && single != VK_SUBOPTIMAL_KHR) || vkQueueWaitIdle(queue)) return 36;
    return 0;
}

/**
 * Exercises offscreen VkImage, SPIR-V graphics pipeline, VkPipelineCache, descriptor set +
 * sampler + push constants, image-to-buffer readback and balanced guest VkAllocationCallbacks.
 */
static int run_graphics_shader_check(void) {
    guest_alloc_state ast = { .magic = 0xA110CA7Eu, .allocs = 0, .frees = 0, .fail_next = 1 };
    const VkAllocationCallbacks cb = {
        .pUserData = &ast,
        .pfnAllocation = &guest_alloc,
        .pfnReallocation = &guest_realloc,
        .pfnFree = &guest_free,
    };
    VkShaderModule vert = VK_NULL_HANDLE, frag = VK_NULL_HANDLE;
    const VkShaderModuleCreateInfo vci = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = sizeof(tri_vert_spv), .pCode = tri_vert_spv
    };
    const VkShaderModuleCreateInfo fci = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = sizeof(tri_frag_spv), .pCode = tri_frag_spv
    };
    if (vkCreateShaderModule(device, &vci, &cb, &vert) != VK_ERROR_OUT_OF_HOST_MEMORY || vert || ast.fail_next != 0)
        return 50;
    if (vkCreateShaderModule(device, &vci, &cb, &vert) || !vert ||
        vkCreateShaderModule(device, &fci, &cb, &frag) || !frag || ast.allocs < 2)
        return 51;

    /* Verify descriptor set layout, descriptor pool, descriptor set update & sampler creation. */
    VkSampler sampler = VK_NULL_HANDLE;
    const VkSamplerCreateInfo sci = {
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter = VK_FILTER_NEAREST, .minFilter = VK_FILTER_NEAREST,
        .mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST,
        .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
    };
    if (vkCreateSampler(device, &sci, NULL, &sampler) || !sampler) return 52;

    const VkDescriptorSetLayoutBinding ubo_bind = {
        .binding = 0, .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT
    };
    const VkDescriptorSetLayoutCreateInfo dslci = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = 1, .pBindings = &ubo_bind
    };
    VkDescriptorSetLayout dsl = VK_NULL_HANDLE;
    if (vkCreateDescriptorSetLayout(device, &dslci, NULL, &dsl) || !dsl) return 52;

    const VkDescriptorPoolSize pool_sz = { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1 };
    const VkDescriptorPoolCreateInfo dpci = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .maxSets = 1, .poolSizeCount = 1, .pPoolSizes = &pool_sz
    };
    VkDescriptorPool dpool = VK_NULL_HANDLE;
    if (vkCreateDescriptorPool(device, &dpci, NULL, &dpool) || !dpool) return 52;

    const VkDescriptorSetAllocateInfo dsai = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = dpool, .descriptorSetCount = 1, .pSetLayouts = &dsl
    };
    VkDescriptorSet dset = VK_NULL_HANDLE;
    if (vkAllocateDescriptorSets(device, &dsai, &dset) || !dset) return 52;

    const VkDescriptorBufferInfo dbi = { .buffer = readback, .offset = 0, .range = 16 };
    const VkWriteDescriptorSet wds = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = dset, .dstBinding = 0, .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .pBufferInfo = &dbi
    };
    vkUpdateDescriptorSets(device, 1, &wds, 0, NULL);

    VkImage img = VK_NULL_HANDLE;
    const VkImageCreateInfo ici = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D, .format = VK_FORMAT_R8G8B8A8_UNORM,
        .extent = {4, 4, 1}, .mipLevels = 1, .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT, .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE, .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
    };
    if (vkCreateImage(device, &ici, NULL, &img) || !img) return 52;
    VkMemoryRequirements ireq;
    vkGetImageMemoryRequirements(device, img, &ireq);
    VkPhysicalDeviceMemoryProperties mprops;
    vkGetPhysicalDeviceMemoryProperties(physical, &mprops);
    uint32_t itype = 32;
    for (uint32_t i = 0; i < mprops.memoryTypeCount && i < 32; ++i) {
        if (ireq.memoryTypeBits & (1u << i)) {
            itype = i;
            if (mprops.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) break;
        }
    }
    if (itype >= 32 || !ireq.size) return 53;
    VkDeviceMemory imem = VK_NULL_HANDLE;
    const VkMemoryAllocateInfo iai = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, .allocationSize = ireq.size, .memoryTypeIndex = itype
    };
    if (vkAllocateMemory(device, &iai, NULL, &imem) || !imem || vkBindImageMemory(device, img, imem, 0)) return 54;

    VkImageView view = VK_NULL_HANDLE;
    const VkImageViewCreateInfo ivci = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, .image = img, .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = VK_FORMAT_R8G8B8A8_UNORM, .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}
    };
    if (vkCreateImageView(device, &ivci, NULL, &view) || !view) return 55;

    VkRenderPass rp = VK_NULL_HANDLE;
    const VkAttachmentDescription att = {
        .format = VK_FORMAT_R8G8B8A8_UNORM, .samples = VK_SAMPLE_COUNT_1_BIT,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR, .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE, .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED, .finalLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL
    };
    const VkAttachmentReference cref = {0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    const VkSubpassDescription sub = {
        .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS, .colorAttachmentCount = 1, .pColorAttachments = &cref
    };
    const VkRenderPassCreateInfo rpci = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount = 1, .pAttachments = &att, .subpassCount = 1, .pSubpasses = &sub
    };
    if (vkCreateRenderPass(device, &rpci, NULL, &rp) || !rp) return 56;

    VkFramebuffer fb = VK_NULL_HANDLE;
    const VkFramebufferCreateInfo fbci = {
        .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO, .renderPass = rp,
        .attachmentCount = 1, .pAttachments = &view, .width = 4, .height = 4, .layers = 1
    };
    if (vkCreateFramebuffer(device, &fbci, NULL, &fb) || !fb) return 57;

    const VkPushConstantRange pcr = { .stageFlags = VK_SHADER_STAGE_VERTEX_BIT, .offset = 0, .size = 16 };
    VkPipelineLayout layout = VK_NULL_HANDLE;
    const VkPipelineLayoutCreateInfo plci = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1, .pSetLayouts = &dsl,
        .pushConstantRangeCount = 1, .pPushConstantRanges = &pcr
    };
    if (vkCreatePipelineLayout(device, &plci, NULL, &layout) || !layout) return 58;

    VkPipelineCache pcache = VK_NULL_HANDLE;
    const VkPipelineCacheCreateInfo pcci = { .sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO };
    if (vkCreatePipelineCache(device, &pcci, NULL, &pcache) || !pcache) return 58;

    const VkPipelineShaderStageCreateInfo stages[2] = {
        { .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .stage = VK_SHADER_STAGE_VERTEX_BIT, .module = vert, .pName = "main" },
        { .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .stage = VK_SHADER_STAGE_FRAGMENT_BIT, .module = frag, .pName = "main" },
    };
    const VkPipelineVertexInputStateCreateInfo vi = { .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
    const VkPipelineInputAssemblyStateCreateInfo ia = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO, .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST
    };
    const VkViewport vp = {0.f, 0.f, 4.f, 4.f, 0.f, 1.f};
    const VkRect2D sc = {{0, 0}, {4, 4}};
    const VkPipelineViewportStateCreateInfo vs = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1, .pViewports = &vp, .scissorCount = 1, .pScissors = &sc
    };
    const VkPipelineRasterizationStateCreateInfo rs = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .polygonMode = VK_POLYGON_MODE_FILL, .cullMode = VK_CULL_MODE_NONE, .lineWidth = 1.f
    };
    const VkPipelineMultisampleStateCreateInfo ms = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO, .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT
    };
    const VkPipelineColorBlendAttachmentState cba = { .colorWriteMask = 0xFu };
    const VkPipelineColorBlendStateCreateInfo cb_state = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO, .attachmentCount = 1, .pAttachments = &cba
    };
    const VkGraphicsPipelineCreateInfo gpci = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .stageCount = 2, .pStages = stages, .pVertexInputState = &vi, .pInputAssemblyState = &ia,
        .pViewportState = &vs, .pRasterizationState = &rs, .pMultisampleState = &ms,
        .pColorBlendState = &cb_state, .layout = layout, .renderPass = rp, .basePipelineIndex = -1
    };
    VkPipeline pipe = VK_NULL_HANDLE;
    if (vkCreateGraphicsPipelines(device, pcache, 1, &gpci, NULL, &pipe) || !pipe) return 59;

    size_t cache_bytes = 0;
    if (vkGetPipelineCacheData(device, pcache, &cache_bytes, NULL)) return 59;

    const VkCommandBufferBeginInfo begin = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
    };
    if (vkBeginCommandBuffer(command, &begin)) return 60;
    const VkClearValue clear = { .color = { .float32 = {0.f, 0.f, 0.f, 1.f} } };
    const VkRenderPassBeginInfo rpbi = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO, .renderPass = rp, .framebuffer = fb,
        .renderArea = {{0, 0}, {4, 4}}, .clearValueCount = 1, .pClearValues = &clear
    };
    const float pc_vals[4] = {1.f, 2.f, 3.f, 4.f};
    vkCmdBeginRenderPass(command, &rpbi, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(command, VK_PIPELINE_BIND_POINT_GRAPHICS, pipe);
    vkCmdBindDescriptorSets(command, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 0, 1, &dset, 0, NULL);
    vkCmdPushConstants(command, layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(pc_vals), pc_vals);
    vkCmdDraw(command, 3, 1, 0, 0);
    vkCmdEndRenderPass(command);
    const VkBufferImageCopy copy = {
        .imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},
        .imageExtent = {4, 4, 1}
    };
    vkCmdCopyImageToBuffer(command, img, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, readback, 1, &copy);
    const VkBufferMemoryBarrier host_read = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
        .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT, .dstAccessMask = VK_ACCESS_HOST_READ_BIT,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED, .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .buffer = readback, .size = readback_words * sizeof(uint32_t)
    };
    vkCmdPipelineBarrier(command, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT,
                         0, 0, NULL, 1, &host_read, 0, NULL);
    if (vkEndCommandBuffer(command)) return 61;
    const VkSubmitInfo sub_info = { .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO, .commandBufferCount = 1, .pCommandBuffers = &command };
    if (vkQueueSubmit(queue, 1, &sub_info, VK_NULL_HANDLE) || vkQueueWaitIdle(queue)) return 62;

    void* mapped = NULL;
    if (vkMapMemory(device, storage, 0, readback_words * sizeof(uint32_t), 0, &mapped) || !mapped) return 63;
    const uint32_t* pixels = (const uint32_t*)mapped;
    int shader_ok = 1;
    for (uint32_t i = 0; i < readback_words; ++i)
        if (pixels[i] != UINT32_C(0xFFCC8822)) shader_ok = 0;
    vkUnmapMemory(device, storage);
    if (!shader_ok) return 64;

    vkDestroyPipeline(device, pipe, NULL);
    vkDestroyPipelineCache(device, pcache, NULL);
    vkDestroyPipelineLayout(device, layout, NULL);
    vkDestroyDescriptorPool(device, dpool, NULL);
    vkDestroyDescriptorSetLayout(device, dsl, NULL);
    vkDestroySampler(device, sampler, NULL);
    vkDestroyShaderModule(device, frag, &cb);
    vkDestroyShaderModule(device, vert, &cb);
    if (ast.frees != ast.allocs) return 65;
    vkDestroyFramebuffer(device, fb, NULL);
    vkDestroyRenderPass(device, rp, NULL);
    vkDestroyImageView(device, view, NULL);
    vkDestroyImage(device, img, NULL);
    vkFreeMemory(device, imem, NULL);
    return 0;
}

/** Presents at launch, restore or resize, requiring the previous swapchain to be stale when rebuilding. */
JNIEXPORT jint JNICALL Java_org_winapk_fixture_vulkanwsi_MainActivity_present(JNIEnv* env, jclass type, jboolean resized) {
    (void)env;
    (void)type;
    if (resized) {
        uint32_t index;
        if (vkResetFences(device, 1, &acquired) ||
            vkAcquireNextImageKHR(device, swapchain, 0, VK_NULL_HANDLE, acquired, &index) != VK_ERROR_OUT_OF_DATE_KHR) return 37;
        vkDestroySwapchainKHR(device, swapchain, NULL);
    } else {
        int status = start_device();
        if (status) return status;
        status = start_readback();
        if (status) return status;
        status = run_graphics_shader_check();
        if (status) return status;
    }
    int status = start_swapchain();
    if (status) return status;
    status = resized ? show_frame(0.125f, 0.75f) : show_frame(0.75f, 0.125f);
    if (!resized && !status) {
        vkDestroyBuffer(device, readback, NULL);
        vkFreeMemory(device, storage, NULL);
        readback = VK_NULL_HANDLE;
        storage = VK_NULL_HANDLE;
    }
    return status;
}

/** Validates the retained surface's extent and that minimize invalidates acquisition. */
JNIEXPORT jlong JNICALL Java_org_winapk_fixture_vulkanwsi_MainActivity_extent(JNIEnv* env, jclass type) {
    (void)env;
    (void)type;
    VkSurfaceCapabilitiesKHR value;
    if (caps(physical, surface, &value)) return -2;
    if (value.currentExtent.width != (uint32_t)ANativeWindow_getWidth(native) ||
        value.currentExtent.height != (uint32_t)ANativeWindow_getHeight(native)) return -3;
    if (!value.currentExtent.width && !value.currentExtent.height) {
        uint32_t index;
        if (!swapchain || vkAcquireNextImageKHR(device, swapchain, 0, VK_NULL_HANDLE, acquired, &index) !=
                          VK_ERROR_OUT_OF_DATE_KHR) return -4;
    }
    return ((jlong)value.currentExtent.width << 32) | value.currentExtent.height;
}

/** Checks detached and stale WSI, recreates it, then destroys the exact owner. */
JNIEXPORT jboolean JNICALL Java_org_winapk_fixture_vulkanwsi_MainActivity_replace(JNIEnv* env, jclass type, jobject holder) {
    (void)type;
    VkSurfaceCapabilitiesKHR value;
    VkSurfaceKHR old = surface;
    if (caps(physical, old, &value) != VK_ERROR_SURFACE_LOST_KHR || ANativeWindow_getWidth(native) >= 0) return JNI_FALSE;
    vkDestroySwapchainKHR(device, swapchain, NULL);
    vkDestroyFence(device, acquired, NULL);
    vkDestroyCommandPool(device, pool, NULL);
    vkDestroyDevice(device, NULL);
    if (vkGetDeviceProcAddr(device, "vkQueuePresentKHR")) return JNI_FALSE;
    vkDestroySurfaceKHR(instance, old, NULL);
    if (!attach(env, holder) || old == surface || caps(physical, old, &value) != VK_ERROR_SURFACE_LOST_KHR ||
        caps(physical, surface, &value)) return JNI_FALSE;
    vkDestroySurfaceKHR(instance, surface, NULL);
    if (destroy_debug && debug_handle) {
        destroy_debug(instance, debug_handle, NULL);
        debug_handle = VK_NULL_HANDLE;
    }
    if (destroy_messenger && messenger_handle) {
        destroy_messenger(instance, messenger_handle, NULL);
        messenger_handle = VK_NULL_HANDLE;
    }
    vkDestroyInstance(instance, NULL);
    return vkGetInstanceProcAddr(instance, "vkDestroyInstance") == NULL;
}
