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
static ANativeWindow* native;
static PFN_vkCreateAndroidSurfaceKHR create_surface;
static PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR caps;

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
        vkGetInstanceProcAddr(instance, "vkCreateWin32SurfaceKHR") || vkGetInstanceProcAddr(instance, "vkCreateDevice") ||
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
    for (uint32_t i = 0; i < count && !present; ++i)
        if (vkGetPhysicalDeviceSurfaceSupportKHR(physical, i, surface, &present)) return 13;
    if (!present || vkGetPhysicalDeviceSurfaceFormatsKHR(physical, surface, &count, NULL) || !count ||
        vkGetPhysicalDeviceSurfacePresentModesKHR(physical, surface, &count, NULL) || !count) return 14;
    return 0;
}

/** Returns the real ICD extent and checks it against the retained Android token. */
JNIEXPORT jlong JNICALL Java_org_winapk_fixture_vulkanwsi_MainActivity_extent(JNIEnv* env, jclass type) {
    (void)env;
    (void)type;
    VkSurfaceCapabilitiesKHR value;
    if (caps(physical, surface, &value)) return -2;
    if (value.currentExtent.width != (uint32_t)ANativeWindow_getWidth(native) ||
        value.currentExtent.height != (uint32_t)ANativeWindow_getHeight(native)) return -3;
    return ((jlong)value.currentExtent.width << 32) | value.currentExtent.height;
}

/** Checks detached and stale WSI, recreates it, then destroys the exact owner. */
JNIEXPORT jboolean JNICALL Java_org_winapk_fixture_vulkanwsi_MainActivity_replace(JNIEnv* env, jclass type, jobject holder) {
    (void)type;
    VkSurfaceCapabilitiesKHR value;
    VkSurfaceKHR old = surface;
    if (caps(physical, old, &value) != VK_ERROR_SURFACE_LOST_KHR || ANativeWindow_getWidth(native) >= 0) return JNI_FALSE;
    vkDestroySurfaceKHR(instance, old, NULL);
    if (!attach(env, holder) || old == surface || caps(physical, old, &value) != VK_ERROR_SURFACE_LOST_KHR ||
        caps(physical, surface, &value)) return JNI_FALSE;
    vkDestroySurfaceKHR(instance, surface, NULL);
    vkDestroyInstance(instance, NULL);
    return vkGetInstanceProcAddr(instance, "vkDestroyInstance") == NULL;
}
