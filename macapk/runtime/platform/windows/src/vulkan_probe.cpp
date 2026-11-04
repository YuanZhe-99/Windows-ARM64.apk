// vulkan_probe.cpp -- the Windows Vulkan loader, its devices and Win32 presentation (PLAN.md §7).
//
// This reports what the driver offers to a Windows program. It says nothing about guest
// Vulkan, which needs the bridge of P6: doctor keeps the two apart (runtime.guestVulkan).
#include "doctor.h"

#include <windows.h>
#define VK_USE_PLATFORM_WIN32_KHR
#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>

#include <cstdio>
#include <cstring>

namespace winapk {

namespace {

std::string Version(uint32_t v) {
    char b[32];
    snprintf(b, sizeof b, "%u.%u.%u", VK_API_VERSION_MAJOR(v), VK_API_VERSION_MINOR(v), VK_API_VERSION_PATCH(v));
    return b;
}

const char* TypeName(VkPhysicalDeviceType t) {
    switch (t) {
        case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: return "integrated";
        case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU: return "discrete";
        case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU: return "virtual";
        case VK_PHYSICAL_DEVICE_TYPE_CPU: return "cpu";
        default: return "other";
    }
}

/// Creates a small visible window labelled as a test, for the surface check.
HWND TestWindow() {
    WNDCLASSW wc{};
    wc.lpfnWndProc = DefWindowProcW;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = L"WinapkPresentTest";
    RegisterClassW(&wc);
    HWND w = CreateWindowExW(0, wc.lpszClassName, L"Windows-ARM64.apk - Vulkan present test", WS_OVERLAPPEDWINDOW,
                             CW_USEDEFAULT, CW_USEDEFAULT, 320, 200, nullptr, nullptr, wc.hInstance, nullptr);
    if (w) ShowWindow(w, SW_SHOWNOACTIVATE);
    return w;
}

}  // namespace

void ProbeVulkan(Report& r, bool present) {
    VulkanInfo& v = r.vulkan;
    v.win32Present = present ? "not-run: no Vulkan device" : "not-run: pass --present (shows a test window)";
    // Only the system's loader: never one next to the program or on PATH.
    HMODULE lib = LoadLibraryExW(L"vulkan-1.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (!lib) {
        v.loader = "not-found";
        return;
    }
    wchar_t path[MAX_PATH];
    GetModuleFileNameW(lib, path, MAX_PATH);
    v.loaderPath = Utf8(path);
    v.loaderArch = PeArch(path);
    auto gipa = reinterpret_cast<PFN_vkGetInstanceProcAddr>(GetProcAddress(lib, "vkGetInstanceProcAddr"));
    if (!gipa) {
        v.loader = "no-entry-point";
        return;
    }
    uint32_t iv = VK_API_VERSION_1_0;
    if (auto ev = reinterpret_cast<PFN_vkEnumerateInstanceVersion>(gipa(nullptr, "vkEnumerateInstanceVersion"))) ev(&iv);
    v.instanceVersion = Version(iv);

    auto eie = reinterpret_cast<PFN_vkEnumerateInstanceExtensionProperties>(gipa(nullptr, "vkEnumerateInstanceExtensionProperties"));
    uint32_t n = 0;
    std::vector<VkExtensionProperties> exts;
    if (eie && eie(nullptr, &n, nullptr) == VK_SUCCESS) {
        exts.resize(n);
        eie(nullptr, &n, exts.data());
    }
    bool surface = false;
    for (auto& e : exts) {
        surface |= strcmp(e.extensionName, VK_KHR_SURFACE_EXTENSION_NAME) == 0;
        v.win32SurfaceExtension |= strcmp(e.extensionName, VK_KHR_WIN32_SURFACE_EXTENSION_NAME) == 0;
    }
    std::vector<const char*> want;
    if (surface && v.win32SurfaceExtension) want = {VK_KHR_SURFACE_EXTENSION_NAME, VK_KHR_WIN32_SURFACE_EXTENSION_NAME};

    VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO};
    app.pApplicationName = "winapk doctor";
    app.apiVersion = iv >= VK_API_VERSION_1_1 ? VK_API_VERSION_1_1 : VK_API_VERSION_1_0;
    VkInstanceCreateInfo ci{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    ci.pApplicationInfo = &app;
    ci.enabledExtensionCount = (uint32_t)want.size();
    ci.ppEnabledExtensionNames = want.data();
    auto createInstance = reinterpret_cast<PFN_vkCreateInstance>(gipa(nullptr, "vkCreateInstance"));
    VkInstance inst = VK_NULL_HANDLE;
    VkResult res = createInstance ? createInstance(&ci, nullptr, &inst) : VK_ERROR_INITIALIZATION_FAILED;
    if (res != VK_SUCCESS) {
        v.loader = "instance-failed: " + std::to_string((int)res);
        return;
    }
    v.loader = "available";
#define LOAD(name) auto name = reinterpret_cast<PFN_##name>(gipa(inst, #name))
    LOAD(vkDestroyInstance);
    LOAD(vkEnumeratePhysicalDevices);
    LOAD(vkGetPhysicalDeviceProperties);
    LOAD(vkGetPhysicalDeviceProperties2);
    LOAD(vkGetPhysicalDeviceQueueFamilyProperties);
    LOAD(vkGetPhysicalDeviceWin32PresentationSupportKHR);
    LOAD(vkCreateWin32SurfaceKHR);
    LOAD(vkDestroySurfaceKHR);
    LOAD(vkGetPhysicalDeviceSurfaceSupportKHR);
    LOAD(vkGetPhysicalDeviceSurfaceCapabilitiesKHR);
#undef LOAD
    uint32_t count = 0;
    std::vector<VkPhysicalDevice> pds;
    if (vkEnumeratePhysicalDevices(inst, &count, nullptr) == VK_SUCCESS && count > 0) {
        pds.resize(count);
        vkEnumeratePhysicalDevices(inst, &count, pds.data());
    }
    for (VkPhysicalDevice pd : pds) {
        VkPhysicalDeviceProperties p{};
        vkGetPhysicalDeviceProperties(pd, &p);
        VulkanDevice d;
        d.name = p.deviceName;
        d.vendorId = p.vendorID;
        d.deviceId = p.deviceID;
        char dv[16];
        snprintf(dv, sizeof dv, "0x%08x", p.driverVersion);
        d.driverVersion = dv;
        d.apiVersion = Version(p.apiVersion);
        d.type = TypeName(p.deviceType);
        if (vkGetPhysicalDeviceProperties2 && p.apiVersion >= VK_API_VERSION_1_2) {
            VkPhysicalDeviceDriverProperties drv{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES};
            VkPhysicalDeviceProperties2 p2{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2, &drv};
            vkGetPhysicalDeviceProperties2(pd, &p2);
            d.driverName = drv.driverName;
            d.driverInfo = drv.driverInfo;
            d.translated = drv.driverID == VK_DRIVER_ID_MESA_DOZEN;
        }
        vkGetPhysicalDeviceQueueFamilyProperties(pd, &d.queueFamilies, nullptr);
        for (uint32_t q = 0; vkGetPhysicalDeviceWin32PresentationSupportKHR && q < d.queueFamilies; ++q) {
            d.win32Presentation |= vkGetPhysicalDeviceWin32PresentationSupportKHR(pd, q) == VK_TRUE;
        }
        v.devices.push_back(d);
    }
    if (present && !pds.empty()) {
        if (!vkCreateWin32SurfaceKHR) {
            v.win32Present = "failed: VK_KHR_win32_surface is not offered";
        } else if (HWND w = TestWindow()) {
            VkWin32SurfaceCreateInfoKHR sci{VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR};
            sci.hinstance = GetModuleHandleW(nullptr);
            sci.hwnd = w;
            VkSurfaceKHR s = VK_NULL_HANDLE;
            VkResult sr = vkCreateWin32SurfaceKHR(inst, &sci, nullptr, &s);
            if (sr != VK_SUCCESS) {
                v.win32Present = "failed: vkCreateWin32SurfaceKHR " + std::to_string((int)sr);
            } else {
                VkBool32 ok = VK_FALSE;
                for (uint32_t q = 0; q < v.devices[0].queueFamilies && !ok; ++q) {
                    vkGetPhysicalDeviceSurfaceSupportKHR(pds[0], q, s, &ok);
                }
                VkSurfaceCapabilitiesKHR caps{};
                VkResult cr = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(pds[0], s, &caps);
                v.win32Present = ok && cr == VK_SUCCESS ? "passed" : "failed: no queue family presents to the surface";
                vkDestroySurfaceKHR(inst, s, nullptr);
            }
            DestroyWindow(w);
        } else {
            v.win32Present = "failed: no test window";
        }
    }
    vkDestroyInstance(inst, nullptr);
}

}  // namespace winapk
