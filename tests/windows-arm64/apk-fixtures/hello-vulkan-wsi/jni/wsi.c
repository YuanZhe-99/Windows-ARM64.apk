#include <android/native_window_jni.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>

#define VK_USE_PLATFORM_ANDROID_KHR
#include <vulkan/vulkan.h>

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
static uint32_t family;
static ANativeWindow* native;
static PFN_vkCreateAndroidSurfaceKHR create_surface;
static PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR caps;
static PFN_vkQueuePresentKHR present_frame;

/** Checks the fixed device, synchronization and command buffer layouts on Android ARM64. */
_Static_assert(sizeof(VkDeviceQueueCreateInfo) == 40 && sizeof(VkDeviceCreateInfo) == 72 &&
    sizeof(VkSwapchainCreateInfoKHR) == 104 && sizeof(VkImageMemoryBarrier) == 72 &&
    sizeof(VkSubmitInfo) == 72 && sizeof(VkPresentInfoKHR) == 64, "render ABI");

/** Creates real WSI and checks that the bridge, rather than the guest, retains its native window. */
static int attach(JNIEnv* env, jobject holder) {
    native = ANativeWindow_fromSurface(env, holder);
    if (!native) return 0;
    VkAndroidSurfaceCreateInfoKHR ci = { .sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR, .window = native };
    const VkResult result = create_surface(instance, &ci, NULL, &surface);
    ANativeWindow_release(native);
    return result == VK_SUCCESS && ANativeWindow_getWidth(native) > 0;
}

/** Checks version, extension filtering, scope, callback refusal and real WSI properties. */
JNIEXPORT jint JNICALL Java_org_winapk_fixture_vulkanwsi_MainActivity_open(JNIEnv* env, jclass type, jobject holder) {
    (void)type;
    uint32_t version = 0;
    PFN_vkEnumerateInstanceVersion get_version = (PFN_vkEnumerateInstanceVersion)vkGetInstanceProcAddr(NULL, "vkEnumerateInstanceVersion");
    if (!get_version || get_version(&version) || version != VK_API_VERSION_1_0 ||
        vkGetInstanceProcAddr(NULL, "vkCreateAndroidSurfaceKHR") || vkGetInstanceProcAddr(NULL, "vkInvalidFunction")) return 1;

    uint32_t count = 0;
    VkResult result = vkEnumerateInstanceExtensionProperties(NULL, &count, NULL);
    if (result == VK_ERROR_INITIALIZATION_FAILED || (!result && !count)) return -1;
    if (result || count != 2) return 2;
    VkExtensionProperties exts[2];
    count = 1;
    if (vkEnumerateInstanceExtensionProperties(NULL, &count, exts) != VK_INCOMPLETE || count != 1) return 3;
    count = 2;
    if (vkEnumerateInstanceExtensionProperties(NULL, &count, exts) || count != 2 ||
        strcmp(exts[0].extensionName, VK_KHR_SURFACE_EXTENSION_NAME) ||
        strcmp(exts[1].extensionName, VK_KHR_ANDROID_SURFACE_EXTENSION_NAME)) return 4;

    VkApplicationInfo app = { .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO, .apiVersion = VK_API_VERSION_1_0 };
    VkInstanceCreateInfo ci = { .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO, .pApplicationInfo = &app };
    const char* unsupported[] = {"VK_ANDROID_external_memory_android_hardware_buffer"};
    ci.enabledExtensionCount = 1;
    ci.ppEnabledExtensionNames = unsupported;
    if (vkCreateInstance(&ci, NULL, &instance) != VK_ERROR_EXTENSION_NOT_PRESENT || instance) return 5;
    const char* want[] = {VK_KHR_SURFACE_EXTENSION_NAME, VK_KHR_ANDROID_SURFACE_EXTENSION_NAME};
    ci.enabledExtensionCount = 2;
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
        vkGetInstanceProcAddr(instance, "vkCreateAndroidSurfaceKHR")) return 15;
    vkDestroyInstance(instance, NULL);
    ci.enabledExtensionCount = 2;
    result = vkCreateInstance(&ci, NULL, &instance);
    if (result == VK_ERROR_INCOMPATIBLE_DRIVER) return -1;
    if (result || !instance) return 8;
    create_surface = (PFN_vkCreateAndroidSurfaceKHR)vkGetInstanceProcAddr(instance, "vkCreateAndroidSurfaceKHR");
    caps = (PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR)vkGetInstanceProcAddr(instance, "vkGetPhysicalDeviceSurfaceCapabilitiesKHR");
    if (!create_surface || create_surface != &vkCreateAndroidSurfaceKHR || !caps ||
        vkGetInstanceProcAddr(instance, "vkCreateWin32SurfaceKHR") || vkGetInstanceProcAddr(instance, "vkCreateShaderModule") ||
        vkGetInstanceProcAddr((VkInstance)(uintptr_t)UINTPTR_MAX, "vkDestroyInstance")) return 9;
    VkAndroidSurfaceCreateInfoKHR invalid = {.sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR,
        .window = (ANativeWindow*)(uintptr_t)UINTPTR_MAX};
    if (create_surface(instance, &invalid, NULL, &surface) != VK_ERROR_SURFACE_LOST_KHR || surface || !attach(env, holder)) return 10;

    count = 0;
    if (vkEnumeratePhysicalDevices(instance, &count, NULL) || !count) return 11;
    count = 1;
    result = vkEnumeratePhysicalDevices(instance, &count, &physical);
    if ((result && result != VK_INCOMPLETE) || count != 1 || !physical) return 12;
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

/** Allocates the one device, queue, pool, primary buffer and acquire fence. */
static int start_device(void) {
    VkExtensionProperties properties;
    uint32_t count = 1;
    if (vkEnumerateDeviceExtensionProperties(physical, NULL, &count, &properties) ||
        count != 1 || strcmp(properties.extensionName, VK_KHR_SWAPCHAIN_EXTENSION_NAME)) return 20;
    const float priority = 1.f;
    const char* extensions[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
    const VkDeviceQueueCreateInfo queue_info = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .queueFamilyIndex = family, .queueCount = 1, .pQueuePriorities = &priority
    };
    const VkDeviceCreateInfo device_info = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
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

/** Records the guest's layout transitions and color, then submits and presents the acquired image. */
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
    if (vkEndCommandBuffer(command)) return 34;
    const VkSubmitInfo submit = {.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO, .commandBufferCount = 1, .pCommandBuffers = &command};
    if (vkQueueSubmit(queue, 1, &submit, VK_NULL_HANDLE) || vkQueueWaitIdle(queue)) return 35;
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
    }
    int status = start_swapchain();
    if (status) return status;
    return resized ? show_frame(0.125f, 0.75f) : show_frame(0.75f, 0.125f);
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
    vkDestroyInstance(instance, NULL);
    return vkGetInstanceProcAddr(instance, "vkDestroyInstance") == NULL;
}
