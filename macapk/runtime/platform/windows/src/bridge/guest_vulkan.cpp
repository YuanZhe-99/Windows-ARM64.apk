// guest_vulkan.cpp -- Full multi-instance, multi-device Android guest Vulkan 1.0–1.3 bridge
// with `vk.xml`-generated dispatch, standard `pNext` chains, safe CFG-exempt guest callback
// thunks (`VkAllocationCallbacks`, `VK_EXT_debug_report`, `VK_EXT_debug_utils`,
// `VK_EXT_device_memory_report`), and `--embed` WSI shared-texture / transfer compositing
// (GPU-P4, ADR-0004, ADR-0005).
#include "guest_vulkan.h"
#include "angle_runtime.h"
#include "bridge.h"
#include "compositor.h"
#include "embed.h"
#include "guest_graphics.h"
#include "surface.h"

#include <windows.h>
#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.h>

// Android surface declarations (`<vulkan/vulkan_android.h>` without Android headers).
typedef VkFlags VkAndroidSurfaceCreateFlagsKHR;
struct VkAndroidSurfaceCreateInfoKHR {
    VkStructureType sType;
    const void* pNext;
    VkAndroidSurfaceCreateFlagsKHR flags;
    void* window; // ANativeWindow*
};
typedef VkResult (VKAPI_PTR *PFN_vkCreateAndroidSurfaceKHR)(
    VkInstance instance, const VkAndroidSurfaceCreateInfoKHR* pCreateInfo,
    const VkAllocationCallbacks* pAllocator, VkSurfaceKHR* pSurface);
constexpr VkStructureType kStructTypeAndroidSurfaceCreateInfo = static_cast<VkStructureType>(1000008000);
constexpr const char* kAndroidSurfaceExtName = "VK_KHR_android_surface";

#include <algorithm>
#include <array>
#include <atomic>
#include <cstring>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <shared_mutex>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <vector>

namespace winapk::bridge {
namespace {

static_assert(sizeof(VkInstanceCreateInfo) == 64 && sizeof(VkApplicationInfo) == 48 &&
              sizeof(VkAndroidSurfaceCreateInfoKHR) == 32 &&
              offsetof(VkAndroidSurfaceCreateInfoKHR, window) == 24 &&
              sizeof(VkSurfaceCapabilitiesKHR) == 52 && sizeof(VkQueueFamilyProperties) == 24 &&
              sizeof(VkDeviceQueueCreateInfo) == 40 && sizeof(VkDeviceCreateInfo) == 72 &&
              sizeof(VkSwapchainCreateInfoKHR) == 104 && sizeof(VkImageMemoryBarrier) == 72 &&
              sizeof(VkSubmitInfo) == 72 && sizeof(VkPresentInfoKHR) == 64 &&
              sizeof(VkAllocationCallbacks) == 48 && sizeof(VkGraphicsPipelineCreateInfo) == 144 &&
              sizeof(VkPhysicalDeviceProperties2) == 840 && sizeof(VkPhysicalDeviceFeatures2) == 240 &&
              sizeof(VkWriteDescriptorSet) == 64 && sizeof(VkDescriptorSetLayoutBinding) == 24);

guest::ModuleSet* g_guestModules = nullptr;
std::mutex g_modulesMu;

bool IsGuestExecutable(const void* fn) {
    if (!fn) return false;
    const auto addr = reinterpret_cast<uintptr_t>(fn);
    if (addr < 0x10000u || (addr & 3u) != 0) return false;
    std::lock_guard lock(g_modulesMu);
    if (g_guestModules) return g_guestModules->Executable(addr);
    return true;
}

// ---- CFG-exempt guest callback invokers (`__declspec(guard(nocf))`) ----

__declspec(guard(nocf)) void* CallGuestAlloc(PFN_vkAllocationFunction fn, void* ud, size_t sz,
                                             size_t align, VkSystemAllocationScope scope) {
    return fn(ud, sz, align, scope);
}
__declspec(guard(nocf)) void* CallGuestRealloc(PFN_vkReallocationFunction fn, void* ud, void* orig,
                                               size_t sz, size_t align, VkSystemAllocationScope scope) {
    return fn(ud, orig, sz, align, scope);
}
__declspec(guard(nocf)) void CallGuestFree(PFN_vkFreeFunction fn, void* ud, void* mem) {
    fn(ud, mem);
}
__declspec(guard(nocf)) void CallGuestInternalAlloc(PFN_vkInternalAllocationNotification fn, void* ud,
                                                    size_t sz, VkInternalAllocationType t, VkSystemAllocationScope s) {
    fn(ud, sz, t, s);
}
__declspec(guard(nocf)) void CallGuestInternalFree(PFN_vkInternalFreeNotification fn, void* ud,
                                                   size_t sz, VkInternalAllocationType t, VkSystemAllocationScope s) {
    fn(ud, sz, t, s);
}
__declspec(guard(nocf)) VkBool32 CallGuestDebugReport(
        PFN_vkDebugReportCallbackEXT fn, VkDebugReportFlagsEXT flags, VkDebugReportObjectTypeEXT objType,
        uint64_t obj, size_t loc, int32_t code, const char* prefix, const char* msg, void* ud) {
    return fn(flags, objType, obj, loc, code, prefix, msg, ud);
}
__declspec(guard(nocf)) VkBool32 CallGuestDebugUtils(
        PFN_vkDebugUtilsMessengerCallbackEXT fn, VkDebugUtilsMessageSeverityFlagBitsEXT sev,
        VkDebugUtilsMessageTypeFlagsEXT types, const VkDebugUtilsMessengerCallbackDataEXT* data, void* ud) {
    return fn(sev, types, data, ud);
}
__declspec(guard(nocf)) void CallGuestDeviceMemoryReport(
        PFN_vkDeviceMemoryReportCallbackEXT fn, const VkDeviceMemoryReportCallbackDataEXT* data, void* ud) {
    fn(data, ud);
}

// Validates guest `pNext` chains: admits standard Vulkan 1.0–1.3 core & KHR/EXT structs while
// rejecting root create-info structs misused in `pNext` and unbridged Android/FD external handles.
VkResult ValidateGuestPNext(const void* pNext) {
    int depth = 0;
    for (const auto* cur = static_cast<const VkBaseInStructure*>(pNext); cur != nullptr; cur = cur->pNext) {
        if (++depth > 64) return VK_ERROR_FEATURE_NOT_PRESENT;
        switch (static_cast<int32_t>(cur->sType)) {
            case VK_STRUCTURE_TYPE_APPLICATION_INFO:
            case VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO:
            case VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO:
            case VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO:
                return VK_ERROR_FEATURE_NOT_PRESENT;

            case VK_STRUCTURE_TYPE_DEVICE_DEVICE_MEMORY_REPORT_CREATE_INFO_EXT: {
                const auto* mr = reinterpret_cast<const VkDeviceDeviceMemoryReportCreateInfoEXT*>(cur);
                if (!IsGuestExecutable(reinterpret_cast<const void*>(mr->pfnUserCallback))) {
                    return VK_ERROR_FEATURE_NOT_PRESENT;
                }
                break;
            }

            case 1000129000: // VK_STRUCTURE_TYPE_ANDROID_HARDWARE_BUFFER_USAGE_ANDROID
            case 1000129001: // VK_STRUCTURE_TYPE_ANDROID_HARDWARE_BUFFER_PROPERTIES_ANDROID
            case 1000129002: // VK_STRUCTURE_TYPE_ANDROID_HARDWARE_BUFFER_FORMAT_PROPERTIES_ANDROID
            case 1000129003: // VK_STRUCTURE_TYPE_IMPORT_ANDROID_HARDWARE_BUFFER_INFO_ANDROID
            case 1000129004: // VK_STRUCTURE_TYPE_MEMORY_GET_ANDROID_HARDWARE_BUFFER_INFO_ANDROID
            case 1000129005: // VK_STRUCTURE_TYPE_EXTERNAL_FORMAT_ANDROID
            case VK_STRUCTURE_TYPE_IMPORT_MEMORY_FD_INFO_KHR:
            case VK_STRUCTURE_TYPE_MEMORY_FD_PROPERTIES_KHR:
            case VK_STRUCTURE_TYPE_MEMORY_GET_FD_INFO_KHR:
            case VK_STRUCTURE_TYPE_IMPORT_SEMAPHORE_FD_INFO_KHR:
            case VK_STRUCTURE_TYPE_SEMAPHORE_GET_FD_INFO_KHR:
            case VK_STRUCTURE_TYPE_IMPORT_FENCE_FD_INFO_KHR:
            case VK_STRUCTURE_TYPE_FENCE_GET_FD_INFO_KHR:
                return VK_ERROR_EXTENSION_NOT_PRESENT;
            default:
                break;
        }
    }
    return VK_SUCCESS;
}

// Heap-interned guest `VkAllocationCallbacks` record so ICDs that copy `*pAllocator` into
// `instance->alloc` / `device->alloc` never see a dangling stack `pUserData` pointer.
struct GuestAllocatorRecord {
    void* guestUser = nullptr;
    PFN_vkAllocationFunction alloc = nullptr;
    PFN_vkReallocationFunction realloc = nullptr;
    PFN_vkFreeFunction free = nullptr;
    PFN_vkInternalAllocationNotification internalAlloc = nullptr;
    PFN_vkInternalFreeNotification internalFree = nullptr;
    VkAllocationCallbacks host{};

    static void* VKAPI_PTR ThunkAlloc(void* ud, size_t sz, size_t align, VkSystemAllocationScope s) {
        const auto* r = static_cast<const GuestAllocatorRecord*>(ud);
        return CallGuestAlloc(r->alloc, r->guestUser, sz, align, s);
    }
    static void* VKAPI_PTR ThunkRealloc(void* ud, void* orig, size_t sz, size_t align, VkSystemAllocationScope s) {
        const auto* r = static_cast<const GuestAllocatorRecord*>(ud);
        return CallGuestRealloc(r->realloc, r->guestUser, orig, sz, align, s);
    }
    static void VKAPI_PTR ThunkFree(void* ud, void* mem) {
        const auto* r = static_cast<const GuestAllocatorRecord*>(ud);
        CallGuestFree(r->free, r->guestUser, mem);
    }
    static void VKAPI_PTR ThunkInternalAlloc(void* ud, size_t sz, VkInternalAllocationType t, VkSystemAllocationScope s) {
        const auto* r = static_cast<const GuestAllocatorRecord*>(ud);
        if (r->internalAlloc) CallGuestInternalAlloc(r->internalAlloc, r->guestUser, sz, t, s);
    }
    static void VKAPI_PTR ThunkInternalFree(void* ud, size_t sz, VkInternalAllocationType t, VkSystemAllocationScope s) {
        const auto* r = static_cast<const GuestAllocatorRecord*>(ud);
        if (r->internalFree) CallGuestInternalFree(r->internalFree, r->guestUser, sz, t, s);
    }
};

using AllocatorKey = std::array<uintptr_t, 6>;
std::mutex g_allocMu;
std::map<AllocatorKey, std::unique_ptr<GuestAllocatorRecord>> g_allocators;

const VkAllocationCallbacks* InternGuestAllocator(const VkAllocationCallbacks* cb, bool& okOut) {
    okOut = true;
    if (!cb) return nullptr;
    if (!IsGuestExecutable(reinterpret_cast<const void*>(cb->pfnAllocation)) ||
        !IsGuestExecutable(reinterpret_cast<const void*>(cb->pfnReallocation)) ||
        !IsGuestExecutable(reinterpret_cast<const void*>(cb->pfnFree)) ||
        ((cb->pfnInternalAllocation != nullptr) != (cb->pfnInternalFree != nullptr)) ||
        (cb->pfnInternalAllocation && !IsGuestExecutable(reinterpret_cast<const void*>(cb->pfnInternalAllocation))) ||
        (cb->pfnInternalFree && !IsGuestExecutable(reinterpret_cast<const void*>(cb->pfnInternalFree)))) {
        okOut = false;
        return nullptr;
    }
    const AllocatorKey key{
        reinterpret_cast<uintptr_t>(cb->pUserData),
        reinterpret_cast<uintptr_t>(cb->pfnAllocation),
        reinterpret_cast<uintptr_t>(cb->pfnReallocation),
        reinterpret_cast<uintptr_t>(cb->pfnFree),
        reinterpret_cast<uintptr_t>(cb->pfnInternalAllocation),
        reinterpret_cast<uintptr_t>(cb->pfnInternalFree),
    };
    std::lock_guard lock(g_allocMu);
    auto& ptr = g_allocators[key];
    if (!ptr) {
        ptr = std::make_unique<GuestAllocatorRecord>();
        ptr->guestUser = cb->pUserData;
        ptr->alloc = cb->pfnAllocation;
        ptr->realloc = cb->pfnReallocation;
        ptr->free = cb->pfnFree;
        ptr->internalAlloc = cb->pfnInternalAllocation;
        ptr->internalFree = cb->pfnInternalFree;
        ptr->host.pUserData = ptr.get();
        ptr->host.pfnAllocation = &GuestAllocatorRecord::ThunkAlloc;
        ptr->host.pfnReallocation = &GuestAllocatorRecord::ThunkRealloc;
        ptr->host.pfnFree = &GuestAllocatorRecord::ThunkFree;
        if (cb->pfnInternalAllocation) {
            ptr->host.pfnInternalAllocation = &GuestAllocatorRecord::ThunkInternalAlloc;
            ptr->host.pfnInternalFree = &GuestAllocatorRecord::ThunkInternalFree;
        }
    }
    return &ptr->host;
}

struct DebugReportThunk {
    VkInstance instance = VK_NULL_HANDLE;
    VkDebugReportCallbackEXT hostHandle = VK_NULL_HANDLE;
    VkDebugReportFlagsEXT flags = 0;
    PFN_vkDebugReportCallbackEXT guestCb = nullptr;
    void* guestUser = nullptr;
};

struct DebugUtilsThunk {
    VkInstance instance = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT hostHandle = VK_NULL_HANDLE;
    VkDebugUtilsMessageSeverityFlagsEXT severityMask = 0;
    VkDebugUtilsMessageTypeFlagsEXT typeMask = 0;
    PFN_vkDebugUtilsMessengerCallbackEXT guestCb = nullptr;
    void* guestUser = nullptr;
};

struct DeviceMemoryReportThunk {
    PFN_vkDeviceMemoryReportCallbackEXT guestCb = nullptr;
    void* guestUser = nullptr;
};

struct InstanceRecord {
    VkInstance hostInstance = VK_NULL_HANDLE;
    PFN_vkGetInstanceProcAddr gipa = nullptr;
    std::set<std::string> enabledExtensions;
    bool hostHasExtMemCaps = false;
};

struct DeviceRecord;

#define GUEST_VULKAN_STAGE_TABLE
#include "guest_vulkan_autogen.inc"
#undef GUEST_VULKAN_STAGE_TABLE

struct DeviceRecord {
    VkDevice hostDevice = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    InstanceRecord* instance = nullptr;
    PFN_vkGetDeviceProcAddr gdpa = nullptr;
    DeviceDispatchTable dt{};
    std::set<std::string> enabledExtensions;
    std::vector<std::unique_ptr<DeviceMemoryReportThunk>> memReportThunks;
    bool hostHasExtMemWin32 = false;
    bool lost = false;
};

struct SurfaceRecord {
    VkInstance instance = VK_NULL_HANDLE;
    VkSurfaceKHR hostSurface = VK_NULL_HANDLE;
    void* anwToken = nullptr;
    HWND hwnd = nullptr;
};

struct SwapchainRecord {
    VkDevice device = VK_NULL_HANDLE;
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    VkSwapchainKHR hostSwapchain = VK_NULL_HANDLE;
    VkFormat format = VK_FORMAT_B8G8R8A8_UNORM;
    VkExtent2D extent{0, 0};
    uint32_t queueFamily = 0;
    bool presented = false;
    bool outOfDate = false;

    // Offscreen `--embed` WSI resources (DXGI shared texture fast path + transfer buffer fallback)
    std::vector<VkImage> images;
    VkCommandPool embedPool = VK_NULL_HANDLE;
    VkCommandBuffer embedCmd = VK_NULL_HANDLE;
    VkFence embedFence = VK_NULL_HANDLE;

    VkImage embedSharedImg = VK_NULL_HANDLE;
    VkDeviceMemory embedSharedMem = VK_NULL_HANDLE;
    HANDLE embedSharedHandle = nullptr;

    VkBuffer embedBuf = VK_NULL_HANDLE;
    VkDeviceMemory embedMem = VK_NULL_HANDLE;
    void* embedMapped = nullptr;
    size_t embedBytes = 0;
};

std::shared_mutex g_vkMu;
HMODULE g_vulkanLoader = nullptr;
PFN_vkGetInstanceProcAddr g_globalGipa = nullptr;
bool g_injectDeviceLostChecked = false;
bool g_injectDeviceLost = false;
std::atomic<bool> g_deviceLossInjected{false};
std::atomic<uint64_t> g_nextSyntheticHandle{0x77000001ull};

std::unordered_map<VkInstance, std::unique_ptr<InstanceRecord>> g_instances;
std::unordered_map<VkPhysicalDevice, InstanceRecord*> g_physToInst;
std::unordered_map<VkDevice, std::unique_ptr<DeviceRecord>> g_devices;
std::unordered_map<VkQueue, DeviceRecord*> g_queueToDevice;
std::unordered_map<VkCommandBuffer, DeviceRecord*> g_cmdToDevice;
std::unordered_map<VkCommandPool, std::vector<VkCommandBuffer>> g_poolCmds;
std::unordered_map<VkSurfaceKHR, SurfaceRecord> g_surfaces;
std::unordered_map<VkSwapchainKHR, SwapchainRecord> g_swapchains;
std::unordered_map<VkDebugReportCallbackEXT, std::unique_ptr<DebugReportThunk>> g_debugReports;
std::unordered_map<VkDebugUtilsMessengerEXT, std::unique_ptr<DebugUtilsThunk>> g_debugMessengers;

thread_local VkDevice t_cachedDevHandle = VK_NULL_HANDLE;
thread_local DeviceRecord* t_cachedDevRec = nullptr;
thread_local VkCommandBuffer t_cachedCmdHandle = VK_NULL_HANDLE;
thread_local DeviceRecord* t_cachedCmdRec = nullptr;

InstanceRecord* LookupInstanceRecord(VkInstance inst) {
    if (!inst) return nullptr;
    std::shared_lock lock(g_vkMu);
    auto it = g_instances.find(inst);
    return it != g_instances.end() ? it->second.get() : nullptr;
}

InstanceRecord* LookupPhysicalDeviceInstance(VkPhysicalDevice pd) {
    if (!pd) return nullptr;
    std::shared_lock lock(g_vkMu);
    auto it = g_physToInst.find(pd);
    return it != g_physToInst.end() ? it->second : nullptr;
}

DeviceRecord* LookupDeviceRecord(VkDevice dev) {
    if (!dev) return nullptr;
    if (dev == t_cachedDevHandle && t_cachedDevRec) return t_cachedDevRec;
    std::shared_lock lock(g_vkMu);
    auto it = g_devices.find(dev);
    if (it == g_devices.end()) return nullptr;
    t_cachedDevHandle = dev;
    t_cachedDevRec = it->second.get();
    return t_cachedDevRec;
}

DeviceRecord* LookupQueueDevice(VkQueue q) {
    if (!q) return nullptr;
    std::shared_lock lock(g_vkMu);
    auto it = g_queueToDevice.find(q);
    return it != g_queueToDevice.end() ? it->second : nullptr;
}

DeviceRecord* LookupCmdBufDevice(VkCommandBuffer cmd) {
    if (!cmd) return nullptr;
    if (cmd == t_cachedCmdHandle && t_cachedCmdRec) return t_cachedCmdRec;
    std::shared_lock lock(g_vkMu);
    auto it = g_cmdToDevice.find(cmd);
    if (it == g_cmdToDevice.end()) return nullptr;
    t_cachedCmdHandle = cmd;
    t_cachedCmdRec = it->second;
    return t_cachedCmdRec;
}

void InvalidateTlsCaches() {
    t_cachedDevHandle = VK_NULL_HANDLE;
    t_cachedDevRec = nullptr;
    t_cachedCmdHandle = VK_NULL_HANDLE;
    t_cachedCmdRec = nullptr;
}

#define GUEST_VULKAN_STAGE_IMPL
#include "guest_vulkan_autogen.inc"
#undef GUEST_VULKAN_STAGE_IMPL

bool IsGuestVulkanPermitted() {
    std::string req = "off";
    if (!DispatchGuestGraphics([&] { req = GuestGraphicsRequest(); })) {
        wchar_t buf[32]{};
        if (GetEnvironmentVariableW(L"WINAPK_GRAPHICS_MODE", buf, 32) > 0) {
            req.clear();
            for (int i = 0; buf[i]; ++i) req.push_back(static_cast<char>(buf[i]));
        } else {
            req = "auto";
        }
    }
    if (req == "vulkan") return true;
    if (req == "d3d11" || req == "off") return false;
    if (HasNativeVendorVulkan()) return true;
    wchar_t reqVk[8]{};
    return GetEnvironmentVariableW(L"WINAPK_APK_REQUIRES_VULKAN", reqVk, 8) > 0 && reqVk[0] == L'1';
}

bool EnsureVulkanLoader() {
    std::unique_lock lock(g_vkMu);
    if (!g_injectDeviceLostChecked) {
        g_injectDeviceLostChecked = true;
        wchar_t env[8]{};
        g_injectDeviceLost = GetEnvironmentVariableW(L"WINAPK_TEST_VK_DEVICE_LOST", env, 8) > 0 && env[0] == L'1';
    }
    if (g_vulkanLoader && g_globalGipa) return true;
    wchar_t sysDir[MAX_PATH]{};
    const UINT len = GetSystemDirectoryW(sysDir, MAX_PATH);
    if (!len || len >= MAX_PATH) return false;
    const std::wstring path = std::wstring(sysDir) + L"\\vulkan-1.dll";
    g_vulkanLoader = LoadLibraryExW(path.c_str(), nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (!g_vulkanLoader) return false;
    g_globalGipa = reinterpret_cast<PFN_vkGetInstanceProcAddr>(GetProcAddress(g_vulkanLoader, "vkGetInstanceProcAddr"));
    if (!g_globalGipa) {
        FreeLibrary(g_vulkanLoader);
        g_vulkanLoader = nullptr;
        return false;
    }
    return true;
}

bool IsForbiddenExtension(const char* ext) {
    if (!ext) return true;
    return std::strcmp(ext, "VK_KHR_win32_surface") == 0 ||
           std::strcmp(ext, "VK_KHR_external_memory_win32") == 0 ||
           std::strcmp(ext, "VK_KHR_external_semaphore_win32") == 0 ||
           std::strcmp(ext, "VK_KHR_external_fence_win32") == 0 ||
           std::strcmp(ext, "VK_KHR_win32_keyed_mutex") == 0 ||
           std::strcmp(ext, "VK_ANDROID_external_memory_android_hardware_buffer") == 0 ||
           std::strcmp(ext, "VK_KHR_external_memory_fd") == 0 ||
           std::strcmp(ext, "VK_KHR_external_semaphore_fd") == 0 ||
           std::strcmp(ext, "VK_KHR_external_fence_fd") == 0 ||
           std::strncmp(ext, "VK_ANDROID_", 11) == 0;
}

// ---- Global & Instance Commands ----

VKAPI_ATTR VkResult VKAPI_CALL V_vkEnumerateInstanceVersion(uint32_t* pApiVersion) {
    if (!pApiVersion) return VK_ERROR_INITIALIZATION_FAILED;
    if (!IsGuestVulkanPermitted() || !EnsureVulkanLoader()) return VK_ERROR_INITIALIZATION_FAILED;
    auto fn = reinterpret_cast<PFN_vkEnumerateInstanceVersion>(g_globalGipa(VK_NULL_HANDLE, "vkEnumerateInstanceVersion"));
    if (fn) {
        uint32_t v = VK_API_VERSION_1_0;
        const VkResult rc = fn(&v);
        if (rc == VK_SUCCESS) *pApiVersion = std::min<uint32_t>(v, VK_API_VERSION_1_3);
        return rc;
    }
    *pApiVersion = VK_API_VERSION_1_0;
    return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL V_vkEnumerateInstanceLayerProperties(uint32_t* pPropertyCount, VkLayerProperties*) {
    if (!pPropertyCount) return VK_ERROR_INITIALIZATION_FAILED;
    *pPropertyCount = 0;
    return VK_SUCCESS;
}

std::vector<VkExtensionProperties> QueryRawHostInstanceExtensions(bool& hasLiveDevices) {
    hasLiveDevices = false;
    std::vector<VkExtensionProperties> hostExts;
    if (!IsGuestVulkanPermitted() || !EnsureVulkanLoader()) return hostExts;
    auto enumFn = reinterpret_cast<PFN_vkEnumerateInstanceExtensionProperties>(
        g_globalGipa(VK_NULL_HANDLE, "vkEnumerateInstanceExtensionProperties"));
    auto createFn = reinterpret_cast<PFN_vkCreateInstance>(g_globalGipa(VK_NULL_HANDLE, "vkCreateInstance"));
    if (!enumFn || !createFn) return hostExts;

    VkInstance probe = VK_NULL_HANDLE;
    VkInstanceCreateInfo ci{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    if (createFn(&ci, nullptr, &probe) != VK_SUCCESS || !probe) return hostExts;
    auto enumDevs = reinterpret_cast<PFN_vkEnumeratePhysicalDevices>(g_globalGipa(probe, "vkEnumeratePhysicalDevices"));
    auto destroyFn = reinterpret_cast<PFN_vkDestroyInstance>(g_globalGipa(probe, "vkDestroyInstance"));
    uint32_t devCount = 0;
    hasLiveDevices = enumDevs && enumDevs(probe, &devCount, nullptr) == VK_SUCCESS && devCount > 0;
    if (destroyFn) destroyFn(probe, nullptr);
    if (!hasLiveDevices) return hostExts;

    uint32_t n = 0;
    if (enumFn(nullptr, &n, nullptr) != VK_SUCCESS || n == 0) return hostExts;
    hostExts.resize(n);
    if (enumFn(nullptr, &n, hostExts.data()) != VK_SUCCESS) hostExts.clear();
    return hostExts;
}

std::vector<VkExtensionProperties> FilteredInstanceExtensions(std::set<std::string>* rawHostNamesOut = nullptr) {
    bool hasDevs = false;
    const auto hostExts = QueryRawHostInstanceExtensions(hasDevs);
    std::vector<VkExtensionProperties> out;
    if (!hasDevs || hostExts.empty()) return out;

    bool hasWin32Surface = false;
    bool hasDebugReport = false;
    bool hasDebugUtils = false;
    for (const auto& e : hostExts) {
        if (rawHostNamesOut) rawHostNamesOut->insert(e.extensionName);
        if (std::strcmp(e.extensionName, VK_KHR_WIN32_SURFACE_EXTENSION_NAME) == 0) {
            hasWin32Surface = true;
            continue;
        }
        if (IsForbiddenExtension(e.extensionName)) continue;
        if (std::strcmp(e.extensionName, VK_EXT_DEBUG_REPORT_EXTENSION_NAME) == 0) hasDebugReport = true;
        if (std::strcmp(e.extensionName, VK_EXT_DEBUG_UTILS_EXTENSION_NAME) == 0) hasDebugUtils = true;
        out.push_back(e);
    }
    if (hasWin32Surface && SurfaceBridgeReady()) {
        VkExtensionProperties androidSurf{};
        std::snprintf(androidSurf.extensionName, sizeof(androidSurf.extensionName), "%s", kAndroidSurfaceExtName);
        androidSurf.specVersion = 6;
        out.push_back(androidSurf);
    }
    if (!hasDebugReport) {
        VkExtensionProperties dr{};
        std::snprintf(dr.extensionName, sizeof(dr.extensionName), "%s", VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
        dr.specVersion = VK_EXT_DEBUG_REPORT_SPEC_VERSION;
        out.push_back(dr);
    }
    if (!hasDebugUtils) {
        VkExtensionProperties du{};
        std::snprintf(du.extensionName, sizeof(du.extensionName), "%s", VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        du.specVersion = VK_EXT_DEBUG_UTILS_SPEC_VERSION;
        out.push_back(du);
    }
    return out;
}

VKAPI_ATTR VkResult VKAPI_CALL V_vkEnumerateInstanceExtensionProperties(
        const char* pLayerName, uint32_t* pPropertyCount, VkExtensionProperties* pProperties) {
    if (!pPropertyCount) return VK_ERROR_INITIALIZATION_FAILED;
    if (pLayerName && *pLayerName) return VK_ERROR_LAYER_NOT_PRESENT;
    if (!IsGuestVulkanPermitted() || !EnsureVulkanLoader()) {
        *pPropertyCount = 0;
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    const auto list = FilteredInstanceExtensions();
    if (!pProperties) {
        *pPropertyCount = static_cast<uint32_t>(list.size());
        return VK_SUCCESS;
    }
    const uint32_t copyN = std::min<uint32_t>(*pPropertyCount, static_cast<uint32_t>(list.size()));
    for (uint32_t i = 0; i < copyN; ++i) pProperties[i] = list[i];
    const bool incomplete = *pPropertyCount < list.size();
    *pPropertyCount = copyN;
    return incomplete ? VK_INCOMPLETE : VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL V_vkCreateInstance(
        const VkInstanceCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkInstance* pInstance) {
    if (!pCreateInfo || !pInstance || pCreateInfo->sType != VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO) {
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    *pInstance = VK_NULL_HANDLE;
    if (!IsGuestVulkanPermitted() || !EnsureVulkanLoader()) return VK_ERROR_INCOMPATIBLE_DRIVER;

    VkResult pnextErr = ValidateGuestPNext(pCreateInfo->pNext);
    if (pnextErr != VK_SUCCESS) return pnextErr;
    if (pCreateInfo->pApplicationInfo) {
        if (pCreateInfo->pApplicationInfo->sType != VK_STRUCTURE_TYPE_APPLICATION_INFO) {
            return VK_ERROR_FEATURE_NOT_PRESENT;
        }
        VkResult appNextErr = ValidateGuestPNext(pCreateInfo->pApplicationInfo->pNext);
        if (appNextErr != VK_SUCCESS) return appNextErr;
    }
    bool allocOk = true;
    const VkAllocationCallbacks* hostAlloc = InternGuestAllocator(pAllocator, allocOk);
    if (!allocOk) return VK_ERROR_FEATURE_NOT_PRESENT;
    if (pCreateInfo->enabledLayerCount > 0) return VK_ERROR_LAYER_NOT_PRESENT;

    std::set<std::string> rawHostNames;
    const auto available = FilteredInstanceExtensions(&rawHostNames);
    if (available.empty()) return VK_ERROR_INCOMPATIBLE_DRIVER;

    std::vector<const char*> hostExts;
    std::set<std::string> enabledSet;
    for (uint32_t i = 0; i < pCreateInfo->enabledExtensionCount; ++i) {
        const char* req = pCreateInfo->ppEnabledExtensionNames ? pCreateInfo->ppEnabledExtensionNames[i] : nullptr;
        if (!req || IsForbiddenExtension(req)) return VK_ERROR_EXTENSION_NOT_PRESENT;
        const bool found = std::any_of(available.begin(), available.end(),
            [req](const VkExtensionProperties& p) { return std::strcmp(p.extensionName, req) == 0; });
        if (!found) return VK_ERROR_EXTENSION_NOT_PRESENT;
        enabledSet.insert(req);
        if (std::strcmp(req, kAndroidSurfaceExtName) == 0) {
            if (!enabledSet.contains(VK_KHR_SURFACE_EXTENSION_NAME) &&
                std::none_of(pCreateInfo->ppEnabledExtensionNames,
                             pCreateInfo->ppEnabledExtensionNames + pCreateInfo->enabledExtensionCount,
                             [](const char* s) { return s && std::strcmp(s, VK_KHR_SURFACE_EXTENSION_NAME) == 0; })) {
                return VK_ERROR_EXTENSION_NOT_PRESENT;
            }
            hostExts.push_back(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
        } else if (rawHostNames.contains(req)) {
            hostExts.push_back(req);
        }
    }

    const bool wantHostExtMemCaps = IsEmbedded() &&
        rawHostNames.contains(VK_KHR_EXTERNAL_MEMORY_CAPABILITIES_EXTENSION_NAME) &&
        !enabledSet.contains(VK_KHR_EXTERNAL_MEMORY_CAPABILITIES_EXTENSION_NAME);
    if (wantHostExtMemCaps) {
        hostExts.push_back(VK_KHR_EXTERNAL_MEMORY_CAPABILITIES_EXTENSION_NAME);
    }

    VkInstanceCreateInfo hostCi = *pCreateInfo;
    hostCi.enabledExtensionCount = static_cast<uint32_t>(hostExts.size());
    hostCi.ppEnabledExtensionNames = hostExts.empty() ? nullptr : hostExts.data();

    auto createFn = reinterpret_cast<PFN_vkCreateInstance>(g_globalGipa(VK_NULL_HANDLE, "vkCreateInstance"));
    if (!createFn) return VK_ERROR_INITIALIZATION_FAILED;
    VkInstance hostInst = VK_NULL_HANDLE;
    const VkResult rc = createFn(&hostCi, hostAlloc, &hostInst);
    if (rc != VK_SUCCESS || !hostInst) return rc;

    auto rec = std::make_unique<InstanceRecord>();
    rec->hostInstance = hostInst;
    rec->gipa = g_globalGipa;
    rec->enabledExtensions = std::move(enabledSet);
    rec->hostHasExtMemCaps = wantHostExtMemCaps || rec->enabledExtensions.contains(VK_KHR_EXTERNAL_MEMORY_CAPABILITIES_EXTENSION_NAME);

    std::unique_lock lock(g_vkMu);
    g_instances[hostInst] = std::move(rec);
    *pInstance = hostInst;
    return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL V_vkDestroyInstance(VkInstance instance, const VkAllocationCallbacks* pAllocator) {
    if (!instance) return;
    bool allocOk = true;
    const VkAllocationCallbacks* hostAlloc = InternGuestAllocator(pAllocator, allocOk);
    if (!allocOk) return;
    PFN_vkDestroyInstance destroyFn = nullptr;
    {
        std::unique_lock lock(g_vkMu);
        auto it = g_instances.find(instance);
        if (it == g_instances.end()) return;
        destroyFn = reinterpret_cast<PFN_vkDestroyInstance>(it->second->gipa(instance, "vkDestroyInstance"));
        for (auto pit = g_physToInst.begin(); pit != g_physToInst.end();) {
            if (pit->second == it->second.get()) pit = g_physToInst.erase(pit);
            else ++pit;
        }
        for (auto sit = g_surfaces.begin(); sit != g_surfaces.end();) {
            if (sit->second.instance == instance) {
                if (sit->second.anwToken) ReleaseSurface(sit->second.anwToken);
                sit = g_surfaces.erase(sit);
            } else ++sit;
        }
        for (auto dit = g_debugReports.begin(); dit != g_debugReports.end();) {
            if (dit->second->instance == instance) dit = g_debugReports.erase(dit);
            else ++dit;
        }
        for (auto mit = g_debugMessengers.begin(); mit != g_debugMessengers.end();) {
            if (mit->second->instance == instance) mit = g_debugMessengers.erase(mit);
            else ++mit;
        }
        g_instances.erase(it);
    }
    if (destroyFn) {
        destroyFn(instance, hostAlloc);
        LogLine("[winapk] guest Vulkan instance teardown complete");
    }
}

VKAPI_ATTR VkResult VKAPI_CALL V_vkEnumeratePhysicalDevices(
        VkInstance instance, uint32_t* pPhysicalDeviceCount, VkPhysicalDevice* pPhysicalDevices) {
    if (!pPhysicalDeviceCount) return VK_ERROR_INITIALIZATION_FAILED;
    InstanceRecord* rec = LookupInstanceRecord(instance);
    if (!rec) return VK_ERROR_INITIALIZATION_FAILED;
    auto enumFn = reinterpret_cast<PFN_vkEnumeratePhysicalDevices>(rec->gipa(instance, "vkEnumeratePhysicalDevices"));
    auto propsFn = reinterpret_cast<PFN_vkGetPhysicalDeviceProperties>(rec->gipa(instance, "vkGetPhysicalDeviceProperties"));
    if (!enumFn) return VK_ERROR_INITIALIZATION_FAILED;

    uint32_t rawCount = 0;
    VkResult rc = enumFn(instance, &rawCount, nullptr);
    if (rc != VK_SUCCESS || rawCount == 0) { *pPhysicalDeviceCount = 0; return rc; }
    std::vector<VkPhysicalDevice> allDevs(rawCount);
    rc = enumFn(instance, &rawCount, allDevs.data());
    if (rc != VK_SUCCESS) return rc;

    std::vector<VkPhysicalDevice> gpuDevs;
    for (VkPhysicalDevice pd : allDevs) {
        if (propsFn) {
            VkPhysicalDeviceProperties p{};
            propsFn(pd, &p);
            if (p.deviceType == VK_PHYSICAL_DEVICE_TYPE_CPU) continue;
        }
        gpuDevs.push_back(pd);
    }
    if (gpuDevs.empty()) gpuDevs = allDevs;

    {
        std::unique_lock lock(g_vkMu);
        for (VkPhysicalDevice pd : gpuDevs) g_physToInst[pd] = rec;
    }

    if (!pPhysicalDevices) {
        *pPhysicalDeviceCount = static_cast<uint32_t>(gpuDevs.size());
        return VK_SUCCESS;
    }
    const uint32_t copyN = std::min<uint32_t>(*pPhysicalDeviceCount, static_cast<uint32_t>(gpuDevs.size()));
    for (uint32_t i = 0; i < copyN; ++i) pPhysicalDevices[i] = gpuDevs[i];
    const bool incomplete = *pPhysicalDeviceCount < gpuDevs.size();
    *pPhysicalDeviceCount = copyN;
    return incomplete ? VK_INCOMPLETE : VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL V_vkEnumerateDeviceLayerProperties(VkPhysicalDevice, uint32_t* pPropertyCount, VkLayerProperties*) {
    if (!pPropertyCount) return VK_ERROR_INITIALIZATION_FAILED;
    *pPropertyCount = 0;
    return VK_SUCCESS;
}

std::vector<VkExtensionProperties> FilteredDeviceExtensions(
        VkPhysicalDevice pd, InstanceRecord* instRec, std::set<std::string>* rawHostNamesOut = nullptr) {
    std::vector<VkExtensionProperties> out;
    auto enumFn = reinterpret_cast<PFN_vkEnumerateDeviceExtensionProperties>(
        instRec->gipa(instRec->hostInstance, "vkEnumerateDeviceExtensionProperties"));
    if (!enumFn) return out;
    uint32_t n = 0;
    if (enumFn(pd, nullptr, &n, nullptr) != VK_SUCCESS || n == 0) return out;
    std::vector<VkExtensionProperties> hostExts(n);
    if (enumFn(pd, nullptr, &n, hostExts.data()) != VK_SUCCESS) return out;
    for (const auto& e : hostExts) {
        if (rawHostNamesOut) rawHostNamesOut->insert(e.extensionName);
        if (IsForbiddenExtension(e.extensionName)) continue;
        out.push_back(e);
    }
    return out;
}

VKAPI_ATTR VkResult VKAPI_CALL V_vkEnumerateDeviceExtensionProperties(
        VkPhysicalDevice physicalDevice, const char* pLayerName,
        uint32_t* pPropertyCount, VkExtensionProperties* pProperties) {
    if (!pPropertyCount) return VK_ERROR_INITIALIZATION_FAILED;
    if (pLayerName && *pLayerName) return VK_ERROR_LAYER_NOT_PRESENT;
    InstanceRecord* instRec = LookupPhysicalDeviceInstance(physicalDevice);
    if (!instRec) return VK_ERROR_INITIALIZATION_FAILED;

    const auto list = FilteredDeviceExtensions(physicalDevice, instRec);
    if (!pProperties) {
        *pPropertyCount = static_cast<uint32_t>(list.size());
        return VK_SUCCESS;
    }
    const uint32_t copyN = std::min<uint32_t>(*pPropertyCount, static_cast<uint32_t>(list.size()));
    for (uint32_t i = 0; i < copyN; ++i) pProperties[i] = list[i];
    const bool incomplete = *pPropertyCount < list.size();
    *pPropertyCount = copyN;
    return incomplete ? VK_INCOMPLETE : VK_SUCCESS;
}

void VKAPI_PTR HostDeviceMemoryReportTrampoline(const VkDeviceMemoryReportCallbackDataEXT* data, void* userData) {
    const auto* thunk = static_cast<const DeviceMemoryReportThunk*>(userData);
    if (thunk && thunk->guestCb) CallGuestDeviceMemoryReport(thunk->guestCb, data, thunk->guestUser);
}

VKAPI_ATTR VkResult VKAPI_CALL V_vkCreateDevice(
        VkPhysicalDevice physicalDevice, const VkDeviceCreateInfo* pCreateInfo,
        const VkAllocationCallbacks* pAllocator, VkDevice* pDevice) {
    if (!pCreateInfo || !pDevice || pCreateInfo->sType != VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO) {
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    *pDevice = VK_NULL_HANDLE;
    InstanceRecord* instRec = LookupPhysicalDeviceInstance(physicalDevice);
    if (!instRec) return VK_ERROR_INITIALIZATION_FAILED;

    VkResult pnextErr = ValidateGuestPNext(pCreateInfo->pNext);
    if (pnextErr != VK_SUCCESS) return pnextErr;
    bool allocOk = true;
    const VkAllocationCallbacks* hostAlloc = InternGuestAllocator(pAllocator, allocOk);
    if (!allocOk) return VK_ERROR_FEATURE_NOT_PRESENT;

    std::set<std::string> rawDevExts;
    const auto available = FilteredDeviceExtensions(physicalDevice, instRec, &rawDevExts);
    std::set<std::string> enabledSet;
    std::vector<const char*> hostExts;
    for (uint32_t i = 0; i < pCreateInfo->enabledExtensionCount; ++i) {
        const char* req = pCreateInfo->ppEnabledExtensionNames ? pCreateInfo->ppEnabledExtensionNames[i] : nullptr;
        if (!req || IsForbiddenExtension(req)) return VK_ERROR_EXTENSION_NOT_PRESENT;
        const bool found = std::any_of(available.begin(), available.end(),
            [req](const VkExtensionProperties& p) { return std::strcmp(p.extensionName, req) == 0; });
        if (!found) return VK_ERROR_EXTENSION_NOT_PRESENT;
        enabledSet.insert(req);
        hostExts.push_back(req);
    }

    // Privately enable `VK_KHR_external_memory_win32` on the host device for zero-copy `--embed` WSI
    // when supported by a native GPU ICD, without exposing it in `enabledSet`.
    bool hostExtMemWin32 = false;
    if (IsEmbedded() && instRec->hostHasExtMemCaps &&
        rawDevExts.contains(VK_KHR_EXTERNAL_MEMORY_EXTENSION_NAME) &&
        rawDevExts.contains(VK_KHR_EXTERNAL_MEMORY_WIN32_EXTENSION_NAME)) {
        if (!enabledSet.contains(VK_KHR_EXTERNAL_MEMORY_EXTENSION_NAME)) {
            hostExts.push_back(VK_KHR_EXTERNAL_MEMORY_EXTENSION_NAME);
        }
        hostExts.push_back(VK_KHR_EXTERNAL_MEMORY_WIN32_EXTENSION_NAME);
        hostExtMemWin32 = true;
    }

    // Wrap any `VkDeviceDeviceMemoryReportCreateInfoEXT` nodes in `pNext` with CFG-exempt thunks.
    std::vector<std::unique_ptr<DeviceMemoryReportThunk>> memThunks;
    std::vector<VkDeviceDeviceMemoryReportCreateInfoEXT> clonedMemNodes;
    const void* finalPNext = pCreateInfo->pNext;
    for (const auto* cur = static_cast<const VkBaseInStructure*>(pCreateInfo->pNext); cur; cur = cur->pNext) {
        if (cur->sType == VK_STRUCTURE_TYPE_DEVICE_DEVICE_MEMORY_REPORT_CREATE_INFO_EXT) {
            const auto* orig = reinterpret_cast<const VkDeviceDeviceMemoryReportCreateInfoEXT*>(cur);
            auto thunk = std::make_unique<DeviceMemoryReportThunk>();
            thunk->guestCb = orig->pfnUserCallback;
            thunk->guestUser = orig->pUserData;
            VkDeviceDeviceMemoryReportCreateInfoEXT copy = *orig;
            copy.pfnUserCallback = &HostDeviceMemoryReportTrampoline;
            copy.pUserData = thunk.get();
            memThunks.push_back(std::move(thunk));
            clonedMemNodes.push_back(copy);
        }
    }
    if (!clonedMemNodes.empty() && pCreateInfo->pNext ==
            reinterpret_cast<const void*>(pCreateInfo->pNext)) {
        // If only a single memory-report node was passed, chain it directly.
        if (clonedMemNodes.size() == 1 &&
            static_cast<const VkBaseInStructure*>(pCreateInfo->pNext)->sType ==
                VK_STRUCTURE_TYPE_DEVICE_DEVICE_MEMORY_REPORT_CREATE_INFO_EXT) {
            finalPNext = &clonedMemNodes[0];
        }
    }

    VkDeviceCreateInfo hostCi = *pCreateInfo;
    hostCi.pNext = finalPNext;
    hostCi.enabledExtensionCount = static_cast<uint32_t>(hostExts.size());
    hostCi.ppEnabledExtensionNames = hostExts.empty() ? nullptr : hostExts.data();

    auto createFn = reinterpret_cast<PFN_vkCreateDevice>(instRec->gipa(instRec->hostInstance, "vkCreateDevice"));
    auto gdpa = reinterpret_cast<PFN_vkGetDeviceProcAddr>(instRec->gipa(instRec->hostInstance, "vkGetDeviceProcAddr"));
    if (!createFn || !gdpa) return VK_ERROR_INITIALIZATION_FAILED;

    VkDevice hostDev = VK_NULL_HANDLE;
    const VkResult rc = createFn(physicalDevice, &hostCi, hostAlloc, &hostDev);
    if (rc != VK_SUCCESS || !hostDev) return rc;

    auto devRec = std::make_unique<DeviceRecord>();
    devRec->hostDevice = hostDev;
    devRec->physicalDevice = physicalDevice;
    devRec->instance = instRec;
    devRec->gdpa = gdpa;
    devRec->enabledExtensions = std::move(enabledSet);
    devRec->memReportThunks = std::move(memThunks);
    devRec->hostHasExtMemWin32 = hostExtMemWin32;
    PopulateDeviceDispatchTable(hostDev, gdpa, devRec->dt);

    auto getQueueFn = reinterpret_cast<PFN_vkGetDeviceQueue>(gdpa(hostDev, "vkGetDeviceQueue"));
    std::vector<VkQueue> queues;
    if (getQueueFn) {
        for (uint32_t i = 0; i < pCreateInfo->queueCreateInfoCount; ++i) {
            const auto& qci = pCreateInfo->pQueueCreateInfos[i];
            for (uint32_t qi = 0; qi < qci.queueCount; ++qi) {
                VkQueue q = VK_NULL_HANDLE;
                getQueueFn(hostDev, qci.queueFamilyIndex, qi, &q);
                if (q) queues.push_back(q);
            }
        }
    }

    std::unique_lock lock(g_vkMu);
    DeviceRecord* rawPtr = devRec.get();
    g_devices[hostDev] = std::move(devRec);
    for (VkQueue q : queues) g_queueToDevice[q] = rawPtr;
    *pDevice = hostDev;
    return VK_SUCCESS;
}

void CleanupSwapchainEmbedResources(DeviceRecord* devRec, SwapchainRecord& sc) {
    if (!devRec) return;
    if (sc.embedMapped && devRec->dt.vkUnmapMemory) devRec->dt.vkUnmapMemory(devRec->hostDevice, sc.embedMem);
    if (sc.embedFence && devRec->dt.vkDestroyFence) devRec->dt.vkDestroyFence(devRec->hostDevice, sc.embedFence, nullptr);
    if (sc.embedSharedImg && devRec->dt.vkDestroyImage) devRec->dt.vkDestroyImage(devRec->hostDevice, sc.embedSharedImg, nullptr);
    if (sc.embedSharedMem && devRec->dt.vkFreeMemory) devRec->dt.vkFreeMemory(devRec->hostDevice, sc.embedSharedMem, nullptr);
    if (sc.embedBuf && devRec->dt.vkDestroyBuffer) devRec->dt.vkDestroyBuffer(devRec->hostDevice, sc.embedBuf, nullptr);
    if (sc.embedMem && devRec->dt.vkFreeMemory) devRec->dt.vkFreeMemory(devRec->hostDevice, sc.embedMem, nullptr);
    if (sc.embedPool) {
        auto destroyPool = reinterpret_cast<PFN_vkDestroyCommandPool>(devRec->gdpa(devRec->hostDevice, "vkDestroyCommandPool"));
        if (destroyPool) destroyPool(devRec->hostDevice, sc.embedPool, nullptr);
    }
    sc.embedMapped = nullptr;
    sc.embedSharedHandle = nullptr;
    sc.embedSharedImg = VK_NULL_HANDLE;
    sc.embedSharedMem = VK_NULL_HANDLE;
    sc.embedFence = VK_NULL_HANDLE;
    sc.embedBuf = VK_NULL_HANDLE;
    sc.embedMem = VK_NULL_HANDLE;
    sc.embedPool = VK_NULL_HANDLE;
    sc.embedCmd = VK_NULL_HANDLE;
}

VKAPI_ATTR void VKAPI_CALL V_vkDestroyDevice(VkDevice device, const VkAllocationCallbacks* pAllocator) {
    if (!device) return;
    bool allocOk = true;
    const VkAllocationCallbacks* hostAlloc = InternGuestAllocator(pAllocator, allocOk);
    if (!allocOk) return;
    PFN_vkDestroyDevice destroyFn = nullptr;
    {
        std::unique_lock lock(g_vkMu);
        auto it = g_devices.find(device);
        if (it == g_devices.end()) return;
        DeviceRecord* rec = it->second.get();
        destroyFn = reinterpret_cast<PFN_vkDestroyDevice>(rec->gdpa(device, "vkDestroyDevice"));
        for (auto scIt = g_swapchains.begin(); scIt != g_swapchains.end();) {
            if (scIt->second.device == device) {
                CleanupSwapchainEmbedResources(rec, scIt->second);
                scIt = g_swapchains.erase(scIt);
            } else ++scIt;
        }
        for (auto qIt = g_queueToDevice.begin(); qIt != g_queueToDevice.end();) {
            if (qIt->second == rec) qIt = g_queueToDevice.erase(qIt);
            else ++qIt;
        }
        for (auto cIt = g_cmdToDevice.begin(); cIt != g_cmdToDevice.end();) {
            if (cIt->second == rec) cIt = g_cmdToDevice.erase(cIt);
            else ++cIt;
        }
        InvalidateTlsCaches();
        g_devices.erase(it);
    }
    if (destroyFn) destroyFn(device, hostAlloc);
}

VKAPI_ATTR void VKAPI_CALL V_vkGetDeviceQueue(VkDevice device, uint32_t queueFamilyIndex, uint32_t queueIndex, VkQueue* pQueue) {
    if (!pQueue) return;
    *pQueue = VK_NULL_HANDLE;
    DeviceRecord* rec = LookupDeviceRecord(device);
    if (!rec) return;
    auto fn = reinterpret_cast<PFN_vkGetDeviceQueue>(rec->gdpa(device, "vkGetDeviceQueue"));
    if (!fn) return;
    VkQueue q = VK_NULL_HANDLE;
    fn(device, queueFamilyIndex, queueIndex, &q);
    if (q) {
        std::unique_lock lock(g_vkMu);
        g_queueToDevice[q] = rec;
    }
    *pQueue = q;
}

VKAPI_ATTR void VKAPI_CALL V_vkGetDeviceQueue2(VkDevice device, const VkDeviceQueueInfo2* pQueueInfo, VkQueue* pQueue) {
    if (!pQueue || !pQueueInfo) return;
    *pQueue = VK_NULL_HANDLE;
    DeviceRecord* rec = LookupDeviceRecord(device);
    if (!rec) return;
    auto fn = reinterpret_cast<PFN_vkGetDeviceQueue2>(rec->gdpa(device, "vkGetDeviceQueue2"));
    if (!fn) return;
    VkQueue q = VK_NULL_HANDLE;
    fn(device, pQueueInfo, &q);
    if (q) {
        std::unique_lock lock(g_vkMu);
        g_queueToDevice[q] = rec;
    }
    *pQueue = q;
}

VKAPI_ATTR VkResult VKAPI_CALL V_vkAllocateCommandBuffers(
        VkDevice device, const VkCommandBufferAllocateInfo* pAllocateInfo, VkCommandBuffer* pCommandBuffers) {
    if (!pAllocateInfo || !pCommandBuffers) return VK_ERROR_INITIALIZATION_FAILED;
    VkResult pnextErr = ValidateGuestPNext(pAllocateInfo->pNext);
    if (pnextErr != VK_SUCCESS) return pnextErr;
    DeviceRecord* rec = LookupDeviceRecord(device);
    if (!rec) return VK_ERROR_INITIALIZATION_FAILED;
    auto fn = reinterpret_cast<PFN_vkAllocateCommandBuffers>(rec->gdpa(device, "vkAllocateCommandBuffers"));
    if (!fn) return VK_ERROR_INITIALIZATION_FAILED;
    const VkResult rc = fn(device, pAllocateInfo, pCommandBuffers);
    if (rc == VK_SUCCESS) {
        std::unique_lock lock(g_vkMu);
        auto& vec = g_poolCmds[pAllocateInfo->commandPool];
        for (uint32_t i = 0; i < pAllocateInfo->commandBufferCount; ++i) {
            if (pCommandBuffers[i]) {
                g_cmdToDevice[pCommandBuffers[i]] = rec;
                vec.push_back(pCommandBuffers[i]);
            }
        }
    }
    return rc;
}

VKAPI_ATTR void VKAPI_CALL V_vkFreeCommandBuffers(
        VkDevice device, VkCommandPool commandPool, uint32_t commandBufferCount, const VkCommandBuffer* pCommandBuffers) {
    DeviceRecord* rec = LookupDeviceRecord(device);
    if (!rec || !pCommandBuffers || commandBufferCount == 0) return;
    auto fn = reinterpret_cast<PFN_vkFreeCommandBuffers>(rec->gdpa(device, "vkFreeCommandBuffers"));
    {
        std::unique_lock lock(g_vkMu);
        for (uint32_t i = 0; i < commandBufferCount; ++i) {
            g_cmdToDevice.erase(pCommandBuffers[i]);
        }
        InvalidateTlsCaches();
    }
    if (fn) fn(device, commandPool, commandBufferCount, pCommandBuffers);
}

VKAPI_ATTR void VKAPI_CALL V_vkDestroyCommandPool(
        VkDevice device, VkCommandPool commandPool, const VkAllocationCallbacks* pAllocator) {
    DeviceRecord* rec = LookupDeviceRecord(device);
    if (!rec) return;
    bool allocOk = true;
    const VkAllocationCallbacks* hostAlloc = InternGuestAllocator(pAllocator, allocOk);
    if (!allocOk) return;
    if (commandPool) {
        std::unique_lock lock(g_vkMu);
        auto it = g_poolCmds.find(commandPool);
        if (it != g_poolCmds.end()) {
            for (VkCommandBuffer cb : it->second) g_cmdToDevice.erase(cb);
            g_poolCmds.erase(it);
        }
        InvalidateTlsCaches();
    }
    auto fn = reinterpret_cast<PFN_vkDestroyCommandPool>(rec->gdpa(device, "vkDestroyCommandPool"));
    if (fn) fn(device, commandPool, hostAlloc);
}

// ---- Android Surface & WSI Translation ----

VKAPI_ATTR VkResult VKAPI_CALL V_vkCreateAndroidSurfaceKHR(
        VkInstance instance, const VkAndroidSurfaceCreateInfoKHR* pCreateInfo,
        const VkAllocationCallbacks* pAllocator, VkSurfaceKHR* pSurface) {
    if (!pCreateInfo || !pSurface || pCreateInfo->sType != kStructTypeAndroidSurfaceCreateInfo) {
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    *pSurface = VK_NULL_HANDLE;
    VkResult pnextErr = ValidateGuestPNext(pCreateInfo->pNext);
    if (pnextErr != VK_SUCCESS) return pnextErr;
    bool allocOk = true;
    const VkAllocationCallbacks* hostAlloc = InternGuestAllocator(pAllocator, allocOk);
    if (!allocOk) return VK_ERROR_FEATURE_NOT_PRESENT;

    InstanceRecord* instRec = LookupInstanceRecord(instance);
    if (!instRec || !instRec->enabledExtensions.contains(kAndroidSurfaceExtName)) {
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }

    HWND hwnd = nullptr;
    int w = 0, h = 0;
    if (!ResolveSurface(pCreateInfo->window, hwnd, w, h) || !RetainSurface(pCreateInfo->window)) {
        return VK_ERROR_SURFACE_LOST_KHR;
    }

    auto createWin32 = reinterpret_cast<PFN_vkCreateWin32SurfaceKHR>(
        instRec->gipa(instance, "vkCreateWin32SurfaceKHR"));
    if (!createWin32) {
        ReleaseSurface(pCreateInfo->window);
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }
    VkWin32SurfaceCreateInfoKHR winCi{VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR};
    winCi.hinstance = GetModuleHandleW(nullptr);
    winCi.hwnd = hwnd;
    VkSurfaceKHR hostSurf = VK_NULL_HANDLE;
    const VkResult rc = createWin32(instance, &winCi, hostAlloc, &hostSurf);
    if (rc != VK_SUCCESS || !hostSurf) {
        ReleaseSurface(pCreateInfo->window);
        return rc;
    }

    std::unique_lock lock(g_vkMu);
    g_surfaces[hostSurf] = {instance, hostSurf, pCreateInfo->window, hwnd};
    *pSurface = hostSurf;
    return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL V_vkDestroySurfaceKHR(
        VkInstance instance, VkSurfaceKHR surface, const VkAllocationCallbacks* pAllocator) {
    if (!instance || !surface) return;
    bool allocOk = true;
    const VkAllocationCallbacks* hostAlloc = InternGuestAllocator(pAllocator, allocOk);
    if (!allocOk) return;
    InstanceRecord* instRec = LookupInstanceRecord(instance);
    if (!instRec) return;
    void* releaseAnw = nullptr;
    HWND clearHwnd = nullptr;
    {
        std::unique_lock lock(g_vkMu);
        auto it = g_surfaces.find(surface);
        if (it != g_surfaces.end()) {
            releaseAnw = it->second.anwToken;
            clearHwnd = it->second.hwnd;
            g_surfaces.erase(it);
        }
    }
    if (releaseAnw) {
        if (clearHwnd) {
            const int key = -static_cast<int>(reinterpret_cast<uintptr_t>(releaseAnw));
            CompositorUpdateSurfacePlacement(clearHwnd, key, {}, true);
        }
        ReleaseSurface(releaseAnw);
    }
    auto destroyFn = reinterpret_cast<PFN_vkDestroySurfaceKHR>(instRec->gipa(instance, "vkDestroySurfaceKHR"));
    if (destroyFn) destroyFn(instance, surface, hostAlloc);
}

bool CheckSurfaceAlive(VkSurfaceKHR surface, HWND& hwndOut, int& wOut, int& hOut, SurfaceLayerPlacement* placeOut = nullptr) {
    void* token = nullptr;
    HWND expectedHwnd = nullptr;
    {
        std::shared_lock lock(g_vkMu);
        auto it = g_surfaces.find(surface);
        if (it == g_surfaces.end() || !it->second.anwToken) return false;
        token = it->second.anwToken;
        expectedHwnd = it->second.hwnd;
    }
    if (!ResolveSurface(token, hwndOut, wOut, hOut, placeOut) || hwndOut != expectedHwnd) return false;
    return true;
}

VKAPI_ATTR VkResult VKAPI_CALL V_vkGetPhysicalDeviceSurfaceSupportKHR(
        VkPhysicalDevice physicalDevice, uint32_t queueFamilyIndex, VkSurfaceKHR surface, VkBool32* pSupported) {
    if (!pSupported) return VK_ERROR_INITIALIZATION_FAILED;
    InstanceRecord* instRec = LookupPhysicalDeviceInstance(physicalDevice);
    if (!instRec) return VK_ERROR_INITIALIZATION_FAILED;
    HWND hwnd = nullptr; int w = 0, h = 0;
    if (!CheckSurfaceAlive(surface, hwnd, w, h)) return VK_ERROR_SURFACE_LOST_KHR;
    auto fn = reinterpret_cast<PFN_vkGetPhysicalDeviceSurfaceSupportKHR>(
        instRec->gipa(instRec->hostInstance, "vkGetPhysicalDeviceSurfaceSupportKHR"));
    return fn ? fn(physicalDevice, queueFamilyIndex, surface, pSupported) : VK_ERROR_EXTENSION_NOT_PRESENT;
}

VKAPI_ATTR VkResult VKAPI_CALL V_vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
        VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, VkSurfaceCapabilitiesKHR* pSurfaceCapabilities) {
    if (!pSurfaceCapabilities) return VK_ERROR_INITIALIZATION_FAILED;
    InstanceRecord* instRec = LookupPhysicalDeviceInstance(physicalDevice);
    if (!instRec) return VK_ERROR_INITIALIZATION_FAILED;
    HWND hwnd = nullptr; int w = 0, h = 0;
    if (!CheckSurfaceAlive(surface, hwnd, w, h)) return VK_ERROR_SURFACE_LOST_KHR;
    auto fn = reinterpret_cast<PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR>(
        instRec->gipa(instRec->hostInstance, "vkGetPhysicalDeviceSurfaceCapabilitiesKHR"));
    if (!fn) return VK_ERROR_EXTENSION_NOT_PRESENT;
    const VkResult rc = fn(physicalDevice, surface, pSurfaceCapabilities);
    if (rc == VK_SUCCESS) {
        const VkExtent2D ext{static_cast<uint32_t>(std::max(0, w)), static_cast<uint32_t>(std::max(0, h))};
        pSurfaceCapabilities->currentExtent = ext;
        pSurfaceCapabilities->minImageExtent = ext;
        pSurfaceCapabilities->maxImageExtent = ext;
    }
    return rc;
}

VKAPI_ATTR VkResult VKAPI_CALL V_vkGetPhysicalDeviceSurfaceCapabilities2KHR(
        VkPhysicalDevice physicalDevice, const VkPhysicalDeviceSurfaceInfo2KHR* pSurfaceInfo,
        VkSurfaceCapabilities2KHR* pSurfaceCapabilities) {
    if (!pSurfaceInfo || !pSurfaceCapabilities) return VK_ERROR_INITIALIZATION_FAILED;
    VkResult pnextErr = ValidateGuestPNext(pSurfaceInfo->pNext);
    if (pnextErr != VK_SUCCESS) return pnextErr;
    return V_vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
        physicalDevice, pSurfaceInfo->surface, &pSurfaceCapabilities->surfaceCapabilities);
}

VKAPI_ATTR VkResult VKAPI_CALL V_vkGetPhysicalDeviceSurfaceFormatsKHR(
        VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, uint32_t* pSurfaceFormatCount, VkSurfaceFormatKHR* pSurfaceFormats) {
    if (!pSurfaceFormatCount) return VK_ERROR_INITIALIZATION_FAILED;
    InstanceRecord* instRec = LookupPhysicalDeviceInstance(physicalDevice);
    if (!instRec) return VK_ERROR_INITIALIZATION_FAILED;
    HWND hwnd = nullptr; int w = 0, h = 0;
    if (!CheckSurfaceAlive(surface, hwnd, w, h)) return VK_ERROR_SURFACE_LOST_KHR;
    auto fn = reinterpret_cast<PFN_vkGetPhysicalDeviceSurfaceFormatsKHR>(
        instRec->gipa(instRec->hostInstance, "vkGetPhysicalDeviceSurfaceFormatsKHR"));
    return fn ? fn(physicalDevice, surface, pSurfaceFormatCount, pSurfaceFormats) : VK_ERROR_EXTENSION_NOT_PRESENT;
}

VKAPI_ATTR VkResult VKAPI_CALL V_vkGetPhysicalDeviceSurfacePresentModesKHR(
        VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, uint32_t* pPresentModeCount, VkPresentModeKHR* pPresentModes) {
    if (!pPresentModeCount) return VK_ERROR_INITIALIZATION_FAILED;
    InstanceRecord* instRec = LookupPhysicalDeviceInstance(physicalDevice);
    if (!instRec) return VK_ERROR_INITIALIZATION_FAILED;
    HWND hwnd = nullptr; int w = 0, h = 0;
    if (!CheckSurfaceAlive(surface, hwnd, w, h)) return VK_ERROR_SURFACE_LOST_KHR;
    auto fn = reinterpret_cast<PFN_vkGetPhysicalDeviceSurfacePresentModesKHR>(
        instRec->gipa(instRec->hostInstance, "vkGetPhysicalDeviceSurfacePresentModesKHR"));
    return fn ? fn(physicalDevice, surface, pPresentModeCount, pPresentModes) : VK_ERROR_EXTENSION_NOT_PRESENT;
}

bool TryInitDxgiSharedTextureEmbed(DeviceRecord* devRec, SwapchainRecord& sc) {
    if (!devRec->hostHasExtMemWin32) return false;
    auto getImgFmt2 = reinterpret_cast<PFN_vkGetPhysicalDeviceImageFormatProperties2>(
        devRec->instance->gipa(devRec->instance->hostInstance, "vkGetPhysicalDeviceImageFormatProperties2"));
    auto getWin32Handle = reinterpret_cast<PFN_vkGetMemoryWin32HandleKHR>(
        devRec->gdpa(devRec->hostDevice, "vkGetMemoryWin32HandleKHR"));
    if (!getImgFmt2 || !getWin32Handle || !devRec->dt.vkCreateImage || !devRec->dt.vkAllocateMemory ||
        !devRec->dt.vkBindImageMemory) {
        return false;
    }

    VkPhysicalDeviceExternalImageFormatInfo extInfo{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_IMAGE_FORMAT_INFO};
    extInfo.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D11_TEXTURE_KMT_BIT;
    VkPhysicalDeviceImageFormatInfo2 fmtInfo{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_FORMAT_INFO_2, &extInfo};
    fmtInfo.format = sc.format;
    fmtInfo.type = VK_IMAGE_TYPE_2D;
    fmtInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    fmtInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    VkExternalImageFormatProperties extProps{VK_STRUCTURE_TYPE_EXTERNAL_IMAGE_FORMAT_PROPERTIES};
    VkImageFormatProperties2 fmtProps{VK_STRUCTURE_TYPE_IMAGE_FORMAT_PROPERTIES_2, &extProps};
    if (getImgFmt2(devRec->physicalDevice, &fmtInfo, &fmtProps) != VK_SUCCESS ||
        !(extProps.externalMemoryProperties.externalMemoryFeatures & VK_EXTERNAL_MEMORY_FEATURE_EXPORTABLE_BIT)) {
        return false;
    }

    VkExternalMemoryImageCreateInfo extImgCi{VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO};
    extImgCi.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D11_TEXTURE_KMT_BIT;
    VkImageCreateInfo ici{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, &extImgCi};
    ici.imageType = VK_IMAGE_TYPE_2D;
    ici.format = sc.format;
    ici.extent = {sc.extent.width, sc.extent.height, 1};
    ici.mipLevels = 1;
    ici.arrayLayers = 1;
    ici.samples = VK_SAMPLE_COUNT_1_BIT;
    ici.tiling = VK_IMAGE_TILING_OPTIMAL;
    ici.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    ici.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    ici.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    if (devRec->dt.vkCreateImage(devRec->hostDevice, &ici, nullptr, &sc.embedSharedImg) != VK_SUCCESS) return false;

    VkMemoryRequirements req{};
    devRec->dt.vkGetImageMemoryRequirements(devRec->hostDevice, sc.embedSharedImg, &req);
    auto getMemProps = reinterpret_cast<PFN_vkGetPhysicalDeviceMemoryProperties>(
        devRec->instance->gipa(devRec->instance->hostInstance, "vkGetPhysicalDeviceMemoryProperties"));
    VkPhysicalDeviceMemoryProperties mp{};
    if (getMemProps) getMemProps(devRec->physicalDevice, &mp);
    uint32_t memType = UINT32_MAX;
    for (uint32_t i = 0; i < mp.memoryTypeCount; ++i) {
        if ((req.memoryTypeBits & (1u << i)) && (mp.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)) {
            memType = i;
            break;
        }
    }
    if (memType == UINT32_MAX) return false;

    VkExportMemoryAllocateInfo expAi{VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO};
    expAi.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D11_TEXTURE_KMT_BIT;
    VkMemoryAllocateInfo mai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, &expAi};
    mai.allocationSize = req.size;
    mai.memoryTypeIndex = memType;
    if (devRec->dt.vkAllocateMemory(devRec->hostDevice, &mai, nullptr, &sc.embedSharedMem) != VK_SUCCESS ||
        devRec->dt.vkBindImageMemory(devRec->hostDevice, sc.embedSharedImg, sc.embedSharedMem, 0) != VK_SUCCESS) {
        return false;
    }

    VkMemoryGetWin32HandleInfoKHR ghi{VK_STRUCTURE_TYPE_MEMORY_GET_WIN32_HANDLE_INFO_KHR};
    ghi.memory = sc.embedSharedMem;
    ghi.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D11_TEXTURE_KMT_BIT;
    return getWin32Handle(devRec->hostDevice, &ghi, &sc.embedSharedHandle) == VK_SUCCESS && sc.embedSharedHandle != nullptr;
}

void InitSwapchainEmbedStaging(DeviceRecord* devRec, SwapchainRecord& sc) {
    if (!IsEmbedded() || sc.extent.width == 0 || sc.extent.height == 0) return;
    auto getImages = reinterpret_cast<PFN_vkGetSwapchainImagesKHR>(devRec->gdpa(devRec->hostDevice, "vkGetSwapchainImagesKHR"));
    if (!getImages) return;
    uint32_t imgCount = 0;
    if (getImages(devRec->hostDevice, sc.hostSwapchain, &imgCount, nullptr) != VK_SUCCESS || imgCount == 0) return;
    sc.images.resize(imgCount);
    if (getImages(devRec->hostDevice, sc.hostSwapchain, &imgCount, sc.images.data()) != VK_SUCCESS) {
        sc.images.clear();
        return;
    }

    VkCommandPoolCreateInfo cpci{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    cpci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    cpci.queueFamilyIndex = sc.queueFamily;
    if (devRec->dt.vkCreateCommandPool(devRec->hostDevice, &cpci, nullptr, &sc.embedPool) != VK_SUCCESS) return;
    auto allocCmd = reinterpret_cast<PFN_vkAllocateCommandBuffers>(devRec->gdpa(devRec->hostDevice, "vkAllocateCommandBuffers"));
    VkCommandBufferAllocateInfo cbai{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    cbai.commandPool = sc.embedPool;
    cbai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cbai.commandBufferCount = 1;
    if (!allocCmd || allocCmd(devRec->hostDevice, &cbai, &sc.embedCmd) != VK_SUCCESS) {
        CleanupSwapchainEmbedResources(devRec, sc);
        return;
    }
    VkFenceCreateInfo fci{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    if (devRec->dt.vkCreateFence(devRec->hostDevice, &fci, nullptr, &sc.embedFence) != VK_SUCCESS) {
        CleanupSwapchainEmbedResources(devRec, sc);
        return;
    }

    // 1. Prefer zero-copy `VK_KHR_external_memory_win32` DXGI shared texture when supported by the ICD.
    if (TryInitDxgiSharedTextureEmbed(devRec, sc)) return;
    if (sc.embedSharedImg) { devRec->dt.vkDestroyImage(devRec->hostDevice, sc.embedSharedImg, nullptr); sc.embedSharedImg = VK_NULL_HANDLE; }
    if (sc.embedSharedMem) { devRec->dt.vkFreeMemory(devRec->hostDevice, sc.embedSharedMem, nullptr); sc.embedSharedMem = VK_NULL_HANDLE; }

    // 2. Fallback: host-coherent `VkBuffer` transfer readback (works on Mesa Dozen and all ICDs).
    const size_t bytes = static_cast<size_t>(sc.extent.width) * static_cast<size_t>(sc.extent.height) * 4;
    VkBufferCreateInfo bci{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bci.size = bytes;
    bci.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (!devRec->dt.vkCreateBuffer || devRec->dt.vkCreateBuffer(devRec->hostDevice, &bci, nullptr, &sc.embedBuf) != VK_SUCCESS) {
        CleanupSwapchainEmbedResources(devRec, sc);
        return;
    }

    VkMemoryRequirements req{};
    devRec->dt.vkGetBufferMemoryRequirements(devRec->hostDevice, sc.embedBuf, &req);
    auto getMemProps = reinterpret_cast<PFN_vkGetPhysicalDeviceMemoryProperties>(
        devRec->instance->gipa(devRec->instance->hostInstance, "vkGetPhysicalDeviceMemoryProperties"));
    VkPhysicalDeviceMemoryProperties mp{};
    if (getMemProps) getMemProps(devRec->physicalDevice, &mp);
    constexpr VkMemoryPropertyFlags kWant = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    uint32_t memType = UINT32_MAX;
    for (uint32_t i = 0; i < mp.memoryTypeCount; ++i) {
        if ((req.memoryTypeBits & (1u << i)) && (mp.memoryTypes[i].propertyFlags & kWant) == kWant) {
            memType = i;
            break;
        }
    }
    if (memType == UINT32_MAX) { CleanupSwapchainEmbedResources(devRec, sc); return; }

    VkMemoryAllocateInfo mai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    mai.allocationSize = req.size;
    mai.memoryTypeIndex = memType;
    if (devRec->dt.vkAllocateMemory(devRec->hostDevice, &mai, nullptr, &sc.embedMem) != VK_SUCCESS ||
        devRec->dt.vkBindBufferMemory(devRec->hostDevice, sc.embedBuf, sc.embedMem, 0) != VK_SUCCESS ||
        devRec->dt.vkMapMemory(devRec->hostDevice, sc.embedMem, 0, bytes, 0, &sc.embedMapped) != VK_SUCCESS) {
        CleanupSwapchainEmbedResources(devRec, sc);
        return;
    }
    sc.embedBytes = bytes;
}

VKAPI_ATTR VkResult VKAPI_CALL V_vkCreateSwapchainKHR(
        VkDevice device, const VkSwapchainCreateInfoKHR* pCreateInfo,
        const VkAllocationCallbacks* pAllocator, VkSwapchainKHR* pSwapchain) {
    if (!pCreateInfo || !pSwapchain || pCreateInfo->sType != VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR) {
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    *pSwapchain = VK_NULL_HANDLE;
    VkResult pnextErr = ValidateGuestPNext(pCreateInfo->pNext);
    if (pnextErr != VK_SUCCESS) return pnextErr;
    bool allocOk = true;
    const VkAllocationCallbacks* hostAlloc = InternGuestAllocator(pAllocator, allocOk);
    if (!allocOk) return VK_ERROR_FEATURE_NOT_PRESENT;

    DeviceRecord* devRec = LookupDeviceRecord(device);
    if (!devRec || !devRec->enabledExtensions.contains(VK_KHR_SWAPCHAIN_EXTENSION_NAME)) {
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }
    if (devRec->lost) return VK_ERROR_DEVICE_LOST;

    HWND hwnd = nullptr; int w = 0, h = 0;
    if (!CheckSurfaceAlive(pCreateInfo->surface, hwnd, w, h)) return VK_ERROR_SURFACE_LOST_KHR;
    if (w <= 0 || h <= 0 || pCreateInfo->imageExtent.width == 0 || pCreateInfo->imageExtent.height == 0) {
        return VK_ERROR_OUT_OF_DATE_KHR;
    }

    VkSwapchainCreateInfoKHR hostCi = *pCreateInfo;
    if (IsEmbedded()) {
        auto capsFn = reinterpret_cast<PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR>(
            devRec->instance->gipa(devRec->instance->hostInstance, "vkGetPhysicalDeviceSurfaceCapabilitiesKHR"));
        VkSurfaceCapabilitiesKHR hostCaps{};
        if (capsFn && capsFn(devRec->physicalDevice, pCreateInfo->surface, &hostCaps) == VK_SUCCESS &&
            (hostCaps.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_SRC_BIT)) {
            hostCi.imageUsage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        }
    }

    auto createFn = reinterpret_cast<PFN_vkCreateSwapchainKHR>(devRec->gdpa(device, "vkCreateSwapchainKHR"));
    if (!createFn) return VK_ERROR_EXTENSION_NOT_PRESENT;
    VkSwapchainKHR hostSc = VK_NULL_HANDLE;
    const VkResult rc = createFn(device, &hostCi, hostAlloc, &hostSc);
    if (rc != VK_SUCCESS || !hostSc) return rc;

    SwapchainRecord sc{};
    sc.device = device;
    sc.surface = pCreateInfo->surface;
    sc.hostSwapchain = hostSc;
    sc.format = pCreateInfo->imageFormat;
    sc.extent = pCreateInfo->imageExtent;
    sc.queueFamily = pCreateInfo->queueFamilyIndexCount > 0 && pCreateInfo->pQueueFamilyIndices
        ? pCreateInfo->pQueueFamilyIndices[0] : 0;
    InitSwapchainEmbedStaging(devRec, sc);

    std::unique_lock lock(g_vkMu);
    g_swapchains[hostSc] = std::move(sc);
    *pSwapchain = hostSc;
    return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL V_vkDestroySwapchainKHR(
        VkDevice device, VkSwapchainKHR swapchain, const VkAllocationCallbacks* pAllocator) {
    if (!device || !swapchain) return;
    bool allocOk = true;
    const VkAllocationCallbacks* hostAlloc = InternGuestAllocator(pAllocator, allocOk);
    if (!allocOk) return;
    DeviceRecord* devRec = LookupDeviceRecord(device);
    if (!devRec) return;
    {
        std::unique_lock lock(g_vkMu);
        auto it = g_swapchains.find(swapchain);
        if (it != g_swapchains.end()) {
            CleanupSwapchainEmbedResources(devRec, it->second);
            g_swapchains.erase(it);
        }
    }
    auto destroyFn = reinterpret_cast<PFN_vkDestroySwapchainKHR>(devRec->gdpa(device, "vkDestroySwapchainKHR"));
    if (destroyFn) destroyFn(device, swapchain, hostAlloc);
}

VKAPI_ATTR VkResult VKAPI_CALL V_vkGetSwapchainImagesKHR(
        VkDevice device, VkSwapchainKHR swapchain, uint32_t* pSwapchainImageCount, VkImage* pSwapchainImages) {
    if (!pSwapchainImageCount) return VK_ERROR_INITIALIZATION_FAILED;
    DeviceRecord* devRec = LookupDeviceRecord(device);
    if (!devRec) return VK_ERROR_INITIALIZATION_FAILED;
    auto fn = reinterpret_cast<PFN_vkGetSwapchainImagesKHR>(devRec->gdpa(device, "vkGetSwapchainImagesKHR"));
    return fn ? fn(device, swapchain, pSwapchainImageCount, pSwapchainImages) : VK_ERROR_EXTENSION_NOT_PRESENT;
}

VKAPI_ATTR VkResult VKAPI_CALL V_vkAcquireNextImageKHR(
        VkDevice device, VkSwapchainKHR swapchain, uint64_t timeout,
        VkSemaphore semaphore, VkFence fence, uint32_t* pImageIndex) {
    if (!pImageIndex) return VK_ERROR_INITIALIZATION_FAILED;
    DeviceRecord* devRec = LookupDeviceRecord(device);
    if (!devRec) return VK_ERROR_INITIALIZATION_FAILED;
    if (devRec->lost) return VK_ERROR_DEVICE_LOST;

    VkSurfaceKHR surf = VK_NULL_HANDLE;
    VkExtent2D scExtent{};
    bool alreadyOutOfDate = false;
    {
        std::shared_lock lock(g_vkMu);
        auto it = g_swapchains.find(swapchain);
        if (it == g_swapchains.end()) return VK_ERROR_SURFACE_LOST_KHR;
        surf = it->second.surface;
        scExtent = it->second.extent;
        alreadyOutOfDate = it->second.outOfDate;
    }
    HWND hwnd = nullptr; int w = 0, h = 0;
    if (!CheckSurfaceAlive(surf, hwnd, w, h)) return VK_ERROR_SURFACE_LOST_KHR;
    if (alreadyOutOfDate || w <= 0 || h <= 0 ||
        static_cast<uint32_t>(w) != scExtent.width || static_cast<uint32_t>(h) != scExtent.height) {
        if (!alreadyOutOfDate) {
            std::unique_lock lock(g_vkMu);
            auto it = g_swapchains.find(swapchain);
            if (it != g_swapchains.end()) it->second.outOfDate = true;
        }
        return VK_ERROR_OUT_OF_DATE_KHR;
    }
    auto fn = reinterpret_cast<PFN_vkAcquireNextImageKHR>(devRec->gdpa(device, "vkAcquireNextImageKHR"));
    return fn ? fn(device, swapchain, timeout, semaphore, fence, pImageIndex) : VK_ERROR_EXTENSION_NOT_PRESENT;
}

VKAPI_ATTR VkResult VKAPI_CALL V_vkAcquireNextImage2KHR(
        VkDevice device, const VkAcquireNextImageInfoKHR* pAcquireInfo, uint32_t* pImageIndex) {
    if (!pAcquireInfo || !pImageIndex) return VK_ERROR_INITIALIZATION_FAILED;
    VkResult pnextErr = ValidateGuestPNext(pAcquireInfo->pNext);
    if (pnextErr != VK_SUCCESS) return pnextErr;
    return V_vkAcquireNextImageKHR(device, pAcquireInfo->swapchain, pAcquireInfo->timeout,
                                   pAcquireInfo->semaphore, pAcquireInfo->fence, pImageIndex);
}

void PublishVulkanSwapchainToEmbed(DeviceRecord* devRec, VkQueue queue, SwapchainRecord& sc,
                                   void* anwToken, uint32_t imgIdx) {
    if (!IsEmbedded() || !anwToken || !sc.embedCmd || !sc.embedFence || imgIdx >= sc.images.size()) return;
    if (!sc.embedSharedHandle && !sc.embedMapped) return;

    HWND hwnd = nullptr; int w = 0, h = 0;
    SurfaceLayerPlacement placement{};
    if (!ResolveSurface(anwToken, hwnd, w, h, &placement) || !hwnd) return;

    devRec->dt.vkResetFences(devRec->hostDevice, 1, &sc.embedFence);
    devRec->dt.vkResetCommandBuffer(sc.embedCmd, 0);
    VkCommandBufferBeginInfo bi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    if (devRec->dt.vkBeginCommandBuffer(sc.embedCmd, &bi) != VK_SUCCESS) return;

    VkImageMemoryBarrier toSrc{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    toSrc.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
    toSrc.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    toSrc.oldLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    toSrc.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    toSrc.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toSrc.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toSrc.image = sc.images[imgIdx];
    toSrc.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

    if (sc.embedSharedHandle && sc.embedSharedImg) {
        VkImageMemoryBarrier dstToCopy = toSrc;
        dstToCopy.srcAccessMask = 0;
        dstToCopy.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        dstToCopy.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        dstToCopy.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        dstToCopy.image = sc.embedSharedImg;
        const VkImageMemoryBarrier pre[2] = {toSrc, dstToCopy};
        devRec->dt.vkCmdPipelineBarrier(sc.embedCmd, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                        0, 0, nullptr, 0, nullptr, 2, pre);

        VkImageCopy copyRegion{};
        copyRegion.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
        copyRegion.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
        copyRegion.extent = {sc.extent.width, sc.extent.height, 1};
        devRec->dt.vkCmdCopyImage(sc.embedCmd, sc.images[imgIdx], VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                  sc.embedSharedImg, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);
    } else {
        devRec->dt.vkCmdPipelineBarrier(sc.embedCmd, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                        0, 0, nullptr, 0, nullptr, 1, &toSrc);
        VkBufferImageCopy region{};
        region.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
        region.imageExtent = {sc.extent.width, sc.extent.height, 1};
        devRec->dt.vkCmdCopyImageToBuffer(sc.embedCmd, sc.images[imgIdx], VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                          sc.embedBuf, 1, &region);
    }

    VkImageMemoryBarrier toPresent = toSrc;
    toPresent.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    toPresent.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    toPresent.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    toPresent.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    devRec->dt.vkCmdPipelineBarrier(sc.embedCmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                                    0, 0, nullptr, 0, nullptr, 1, &toPresent);

    if (devRec->dt.vkEndCommandBuffer(sc.embedCmd) != VK_SUCCESS) return;
    VkSubmitInfo si{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    si.commandBufferCount = 1;
    si.pCommandBuffers = &sc.embedCmd;
    if (devRec->dt.vkQueueSubmit(queue, 1, &si, sc.embedFence) != VK_SUCCESS) return;
    if (devRec->dt.vkWaitForFences(devRec->hostDevice, 1, &sc.embedFence, VK_TRUE, 1000000000ull) != VK_SUCCESS) return;

    const int layerKey = -static_cast<int>(reinterpret_cast<uintptr_t>(anwToken));
    SendEmbedEvent("backend vulkan");
    if (sc.embedSharedHandle &&
        CompositorSubmitSurfaceSharedTexture(hwnd, layerKey, sc.embedSharedHandle,
                                             static_cast<int>(sc.extent.width), static_cast<int>(sc.extent.height),
                                             LayerAlpha::Opaque, false, placement)) {
        return;
    }
    if (sc.embedMapped) {
        const bool isBgra = sc.format == VK_FORMAT_B8G8R8A8_UNORM || sc.format == VK_FORMAT_B8G8R8A8_SRGB;
        CompositorSubmitSurfaceBytes(hwnd, layerKey, sc.embedMapped, static_cast<size_t>(sc.extent.width) * 4,
                                     static_cast<int>(sc.extent.width), static_cast<int>(sc.extent.height),
                                     isBgra ? LayerPixels::Bgra8 : LayerPixels::Rgba8,
                                     LayerAlpha::Opaque, false, placement);
    }
}

VKAPI_ATTR VkResult VKAPI_CALL V_vkQueuePresentKHR(VkQueue queue, const VkPresentInfoKHR* pPresentInfo) {
    if (!queue || !pPresentInfo || pPresentInfo->sType != VK_STRUCTURE_TYPE_PRESENT_INFO_KHR ||
        pPresentInfo->swapchainCount == 0 || !pPresentInfo->pSwapchains || !pPresentInfo->pImageIndices) {
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    VkResult pnextErr = ValidateGuestPNext(pPresentInfo->pNext);
    if (pnextErr != VK_SUCCESS) return pnextErr;

    DeviceRecord* devRec = LookupQueueDevice(queue);
    if (!devRec) return VK_ERROR_INITIALIZATION_FAILED;

    if (g_injectDeviceLost && !g_deviceLossInjected.exchange(true)) {
        devRec->lost = true;
        LogLine("GRAPHICS_RENDER_FAILED: guest Vulkan VK_ERROR_DEVICE_LOST (injected device loss)");
        if (pPresentInfo->pResults) {
            for (uint32_t i = 0; i < pPresentInfo->swapchainCount; ++i) pPresentInfo->pResults[i] = VK_ERROR_DEVICE_LOST;
        }
        return VK_ERROR_DEVICE_LOST;
    }
    if (devRec->lost) return VK_ERROR_DEVICE_LOST;

    for (uint32_t i = 0; i < pPresentInfo->swapchainCount; ++i) {
        VkSwapchainKHR scHandle = pPresentInfo->pSwapchains[i];
        VkSurfaceKHR surf = VK_NULL_HANDLE;
        VkExtent2D scExt{};
        bool alreadyOutOfDate = false;
        {
            std::shared_lock lock(g_vkMu);
            auto it = g_swapchains.find(scHandle);
            if (it == g_swapchains.end()) return VK_ERROR_SURFACE_LOST_KHR;
            surf = it->second.surface;
            scExt = it->second.extent;
            alreadyOutOfDate = it->second.outOfDate;
        }
        HWND hwnd = nullptr; int w = 0, h = 0;
        if (!CheckSurfaceAlive(surf, hwnd, w, h)) {
            if (pPresentInfo->pResults) pPresentInfo->pResults[i] = VK_ERROR_SURFACE_LOST_KHR;
            return VK_ERROR_SURFACE_LOST_KHR;
        }
        if (alreadyOutOfDate || w <= 0 || h <= 0 ||
            static_cast<uint32_t>(w) != scExt.width || static_cast<uint32_t>(h) != scExt.height) {
            {
                std::unique_lock lock(g_vkMu);
                auto it = g_swapchains.find(scHandle);
                if (it != g_swapchains.end()) it->second.outOfDate = true;
            }
            if (pPresentInfo->pResults) pPresentInfo->pResults[i] = VK_ERROR_OUT_OF_DATE_KHR;
            return VK_ERROR_OUT_OF_DATE_KHR;
        }
        if (IsEmbedded()) {
            SwapchainRecord* scPtr = nullptr;
            void* anwToken = nullptr;
            {
                std::shared_lock lock(g_vkMu);
                auto it = g_swapchains.find(scHandle);
                if (it != g_swapchains.end()) {
                    scPtr = &it->second;
                    auto sit = g_surfaces.find(it->second.surface);
                    if (sit != g_surfaces.end()) anwToken = sit->second.anwToken;
                }
            }
            if (scPtr && anwToken) {
                PublishVulkanSwapchainToEmbed(devRec, queue, *scPtr, anwToken, pPresentInfo->pImageIndices[i]);
            }
        }
    }

    auto presentFn = reinterpret_cast<PFN_vkQueuePresentKHR>(devRec->gdpa(devRec->hostDevice, "vkQueuePresentKHR"));
    if (!presentFn) return VK_ERROR_EXTENSION_NOT_PRESENT;
    const VkResult rc = presentFn(queue, pPresentInfo);
    if (rc == VK_SUCCESS || rc == VK_SUBOPTIMAL_KHR) {
        std::unique_lock lock(g_vkMu);
        for (uint32_t i = 0; i < pPresentInfo->swapchainCount; ++i) {
            auto it = g_swapchains.find(pPresentInfo->pSwapchains[i]);
            if (it != g_swapchains.end()) it->second.presented = true;
        }
    } else if (rc == VK_ERROR_DEVICE_LOST) {
        devRec->lost = true;
        LogLine("GRAPHICS_RENDER_FAILED: guest Vulkan VK_ERROR_DEVICE_LOST");
    }
    return rc;
}

// ---- Debug Callbacks (`VK_EXT_debug_report` & `VK_EXT_debug_utils`, dual host + bridge) ----

VkBool32 VKAPI_PTR HostDebugReportTrampoline(
        VkDebugReportFlagsEXT flags, VkDebugReportObjectTypeEXT objType, uint64_t obj,
        size_t location, int32_t msgCode, const char* layerPrefix, const char* msg, void* userData) {
    const auto* thunk = static_cast<const DebugReportThunk*>(userData);
    if (!thunk || !thunk->guestCb) return VK_FALSE;
    return CallGuestDebugReport(thunk->guestCb, flags, objType, obj, location, msgCode, layerPrefix, msg, thunk->guestUser);
}

VKAPI_ATTR VkResult VKAPI_CALL V_vkCreateDebugReportCallbackEXT(
        VkInstance instance, const VkDebugReportCallbackCreateInfoEXT* pCreateInfo,
        const VkAllocationCallbacks* pAllocator, VkDebugReportCallbackEXT* pCallback) {
    if (!pCreateInfo || !pCallback || pCreateInfo->sType != VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT) {
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    *pCallback = VK_NULL_HANDLE;
    VkResult pnextErr = ValidateGuestPNext(pCreateInfo->pNext);
    if (pnextErr != VK_SUCCESS) return pnextErr;
    if (!IsGuestExecutable(reinterpret_cast<const void*>(pCreateInfo->pfnCallback))) {
        return VK_ERROR_FEATURE_NOT_PRESENT;
    }
    bool allocOk = true;
    const VkAllocationCallbacks* hostAlloc = InternGuestAllocator(pAllocator, allocOk);
    if (!allocOk) return VK_ERROR_FEATURE_NOT_PRESENT;

    InstanceRecord* instRec = LookupInstanceRecord(instance);
    if (!instRec || !instRec->enabledExtensions.contains(VK_EXT_DEBUG_REPORT_EXTENSION_NAME)) {
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }

    auto thunk = std::make_unique<DebugReportThunk>();
    thunk->instance = instance;
    thunk->flags = pCreateInfo->flags;
    thunk->guestCb = pCreateInfo->pfnCallback;
    thunk->guestUser = pCreateInfo->pUserData;

    VkDebugReportCallbackEXT handle = VK_NULL_HANDLE;
    auto createFn = reinterpret_cast<PFN_vkCreateDebugReportCallbackEXT>(
        instRec->gipa(instance, "vkCreateDebugReportCallbackEXT"));
    if (createFn) {
        VkDebugReportCallbackCreateInfoEXT hostCi = *pCreateInfo;
        hostCi.pfnCallback = &HostDebugReportTrampoline;
        hostCi.pUserData = thunk.get();
        const VkResult rc = createFn(instance, &hostCi, hostAlloc, &handle);
        if (rc == VK_SUCCESS && handle) {
            thunk->hostHandle = handle;
        } else if (rc != VK_ERROR_EXTENSION_NOT_PRESENT) {
            return rc;
        }
    }
    if (!handle) {
        handle = reinterpret_cast<VkDebugReportCallbackEXT>(g_nextSyntheticHandle.fetch_add(1, std::memory_order_relaxed));
    }

    std::unique_lock lock(g_vkMu);
    g_debugReports[handle] = std::move(thunk);
    *pCallback = handle;
    return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL V_vkDestroyDebugReportCallbackEXT(
        VkInstance instance, VkDebugReportCallbackEXT callback, const VkAllocationCallbacks* pAllocator) {
    if (!instance || !callback) return;
    bool allocOk = true;
    const VkAllocationCallbacks* hostAlloc = InternGuestAllocator(pAllocator, allocOk);
    if (!allocOk) return;
    InstanceRecord* instRec = LookupInstanceRecord(instance);
    if (!instRec) return;

    VkDebugReportCallbackEXT hostHandle = VK_NULL_HANDLE;
    {
        std::unique_lock lock(g_vkMu);
        auto it = g_debugReports.find(callback);
        if (it == g_debugReports.end()) return;
        hostHandle = it->second->hostHandle;
        g_debugReports.erase(it);
    }
    if (hostHandle) {
        auto destroyFn = reinterpret_cast<PFN_vkDestroyDebugReportCallbackEXT>(
            instRec->gipa(instance, "vkDestroyDebugReportCallbackEXT"));
        if (destroyFn) destroyFn(instance, hostHandle, hostAlloc);
    }
}

VKAPI_ATTR void VKAPI_CALL V_vkDebugReportMessageEXT(
        VkInstance instance, VkDebugReportFlagsEXT flags, VkDebugReportObjectTypeEXT objectType,
        uint64_t object, size_t location, int32_t messageCode, const char* pLayerPrefix, const char* pMessage) {
    InstanceRecord* instRec = LookupInstanceRecord(instance);
    if (!instRec) return;
    auto msgFn = reinterpret_cast<PFN_vkDebugReportMessageEXT>(instRec->gipa(instance, "vkDebugReportMessageEXT"));
    if (msgFn) {
        msgFn(instance, flags, objectType, object, location, messageCode, pLayerPrefix, pMessage);
    }
    // Deliver directly to bridge-synthesized callbacks whose `hostHandle == VK_NULL_HANDLE`.
    std::vector<DebugReportThunk> pending;
    {
        std::shared_lock lock(g_vkMu);
        for (const auto& [h, t] : g_debugReports) {
            if (t->instance == instance && t->hostHandle == VK_NULL_HANDLE && (t->flags & flags) != 0) {
                pending.push_back(*t);
            }
        }
    }
    for (const auto& t : pending) {
        CallGuestDebugReport(t.guestCb, flags, objectType, object, location,
                             messageCode, pLayerPrefix ? pLayerPrefix : "", pMessage ? pMessage : "", t.guestUser);
    }
}

VkBool32 VKAPI_PTR HostDebugUtilsTrampoline(
        VkDebugUtilsMessageSeverityFlagBitsEXT severity, VkDebugUtilsMessageTypeFlagsEXT types,
        const VkDebugUtilsMessengerCallbackDataEXT* data, void* userData) {
    const auto* thunk = static_cast<const DebugUtilsThunk*>(userData);
    if (!thunk || !thunk->guestCb) return VK_FALSE;
    return CallGuestDebugUtils(thunk->guestCb, severity, types, data, thunk->guestUser);
}

VKAPI_ATTR VkResult VKAPI_CALL V_vkCreateDebugUtilsMessengerEXT(
        VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
        const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pMessenger) {
    if (!pCreateInfo || !pMessenger || pCreateInfo->sType != VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT) {
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    *pMessenger = VK_NULL_HANDLE;
    VkResult pnextErr = ValidateGuestPNext(pCreateInfo->pNext);
    if (pnextErr != VK_SUCCESS) return pnextErr;
    if (!IsGuestExecutable(reinterpret_cast<const void*>(pCreateInfo->pfnUserCallback))) {
        return VK_ERROR_FEATURE_NOT_PRESENT;
    }
    bool allocOk = true;
    const VkAllocationCallbacks* hostAlloc = InternGuestAllocator(pAllocator, allocOk);
    if (!allocOk) return VK_ERROR_FEATURE_NOT_PRESENT;

    InstanceRecord* instRec = LookupInstanceRecord(instance);
    if (!instRec || !instRec->enabledExtensions.contains(VK_EXT_DEBUG_UTILS_EXTENSION_NAME)) {
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }

    auto thunk = std::make_unique<DebugUtilsThunk>();
    thunk->instance = instance;
    thunk->severityMask = pCreateInfo->messageSeverity;
    thunk->typeMask = pCreateInfo->messageType;
    thunk->guestCb = pCreateInfo->pfnUserCallback;
    thunk->guestUser = pCreateInfo->pUserData;

    VkDebugUtilsMessengerEXT handle = VK_NULL_HANDLE;
    auto createFn = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
        instRec->gipa(instance, "vkCreateDebugUtilsMessengerEXT"));
    if (createFn) {
        VkDebugUtilsMessengerCreateInfoEXT hostCi = *pCreateInfo;
        hostCi.pfnUserCallback = &HostDebugUtilsTrampoline;
        hostCi.pUserData = thunk.get();
        const VkResult rc = createFn(instance, &hostCi, hostAlloc, &handle);
        if (rc == VK_SUCCESS && handle) {
            thunk->hostHandle = handle;
        } else if (rc != VK_ERROR_EXTENSION_NOT_PRESENT) {
            return rc;
        }
    }
    if (!handle) {
        handle = reinterpret_cast<VkDebugUtilsMessengerEXT>(g_nextSyntheticHandle.fetch_add(1, std::memory_order_relaxed));
    }

    std::unique_lock lock(g_vkMu);
    g_debugMessengers[handle] = std::move(thunk);
    *pMessenger = handle;
    return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL V_vkDestroyDebugUtilsMessengerEXT(
        VkInstance instance, VkDebugUtilsMessengerEXT messenger, const VkAllocationCallbacks* pAllocator) {
    if (!instance || !messenger) return;
    bool allocOk = true;
    const VkAllocationCallbacks* hostAlloc = InternGuestAllocator(pAllocator, allocOk);
    if (!allocOk) return;
    InstanceRecord* instRec = LookupInstanceRecord(instance);
    if (!instRec) return;

    VkDebugUtilsMessengerEXT hostHandle = VK_NULL_HANDLE;
    {
        std::unique_lock lock(g_vkMu);
        auto it = g_debugMessengers.find(messenger);
        if (it == g_debugMessengers.end()) return;
        hostHandle = it->second->hostHandle;
        g_debugMessengers.erase(it);
    }
    if (hostHandle) {
        auto destroyFn = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
            instRec->gipa(instance, "vkDestroyDebugUtilsMessengerEXT"));
        if (destroyFn) destroyFn(instance, hostHandle, hostAlloc);
    }
}

VKAPI_ATTR void VKAPI_CALL V_vkSubmitDebugUtilsMessageEXT(
        VkInstance instance, VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
        VkDebugUtilsMessageTypeFlagsEXT messageTypes, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData) {
    InstanceRecord* instRec = LookupInstanceRecord(instance);
    if (!instRec || !pCallbackData) return;
    auto fn = reinterpret_cast<PFN_vkSubmitDebugUtilsMessageEXT>(instRec->gipa(instance, "vkSubmitDebugUtilsMessageEXT"));
    if (fn) fn(instance, messageSeverity, messageTypes, pCallbackData);

    std::vector<DebugUtilsThunk> pending;
    {
        std::shared_lock lock(g_vkMu);
        for (const auto& [h, t] : g_debugMessengers) {
            if (t->instance == instance && t->hostHandle == VK_NULL_HANDLE &&
                (t->severityMask & messageSeverity) != 0 && (t->typeMask & messageTypes) != 0) {
                pending.push_back(*t);
            }
        }
    }
    for (const auto& t : pending) {
        CallGuestDebugUtils(t.guestCb, messageSeverity, messageTypes, pCallbackData, t.guestUser);
    }
}

// ---- `vkGetInstanceProcAddr` & `vkGetDeviceProcAddr` ----

VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL V_vkGetInstanceProcAddr(VkInstance instance, const char* pName);
VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL V_vkGetDeviceProcAddr(VkDevice device, const char* pName);

enum class ManualScope { Global, Instance, Surface, AndroidSurface, DebugReport, DebugUtils, Device, Swapchain };
struct ManualEntry { const char* name; void* fn; ManualScope scope; bool exported; };

const ManualEntry kManualEntries[] = {
    {"vkGetInstanceProcAddr", reinterpret_cast<void*>(&V_vkGetInstanceProcAddr), ManualScope::Global, true},
    {"vkEnumerateInstanceVersion", reinterpret_cast<void*>(&V_vkEnumerateInstanceVersion), ManualScope::Global, true},
    {"vkEnumerateInstanceLayerProperties", reinterpret_cast<void*>(&V_vkEnumerateInstanceLayerProperties), ManualScope::Global, true},
    {"vkEnumerateInstanceExtensionProperties", reinterpret_cast<void*>(&V_vkEnumerateInstanceExtensionProperties), ManualScope::Global, true},
    {"vkCreateInstance", reinterpret_cast<void*>(&V_vkCreateInstance), ManualScope::Global, true},

    {"vkDestroyInstance", reinterpret_cast<void*>(&V_vkDestroyInstance), ManualScope::Instance, true},
    {"vkEnumeratePhysicalDevices", reinterpret_cast<void*>(&V_vkEnumeratePhysicalDevices), ManualScope::Instance, true},
    {"vkEnumerateDeviceLayerProperties", reinterpret_cast<void*>(&V_vkEnumerateDeviceLayerProperties), ManualScope::Instance, true},
    {"vkEnumerateDeviceExtensionProperties", reinterpret_cast<void*>(&V_vkEnumerateDeviceExtensionProperties), ManualScope::Instance, true},
    {"vkCreateDevice", reinterpret_cast<void*>(&V_vkCreateDevice), ManualScope::Instance, true},
    {"vkGetDeviceProcAddr", reinterpret_cast<void*>(&V_vkGetDeviceProcAddr), ManualScope::Device, true},

    {"vkDestroySurfaceKHR", reinterpret_cast<void*>(&V_vkDestroySurfaceKHR), ManualScope::Surface, true},
    {"vkGetPhysicalDeviceSurfaceSupportKHR", reinterpret_cast<void*>(&V_vkGetPhysicalDeviceSurfaceSupportKHR), ManualScope::Surface, true},
    {"vkGetPhysicalDeviceSurfaceCapabilitiesKHR", reinterpret_cast<void*>(&V_vkGetPhysicalDeviceSurfaceCapabilitiesKHR), ManualScope::Surface, true},
    {"vkGetPhysicalDeviceSurfaceCapabilities2KHR", reinterpret_cast<void*>(&V_vkGetPhysicalDeviceSurfaceCapabilities2KHR), ManualScope::Surface, false},
    {"vkGetPhysicalDeviceSurfaceFormatsKHR", reinterpret_cast<void*>(&V_vkGetPhysicalDeviceSurfaceFormatsKHR), ManualScope::Surface, true},
    {"vkGetPhysicalDeviceSurfacePresentModesKHR", reinterpret_cast<void*>(&V_vkGetPhysicalDeviceSurfacePresentModesKHR), ManualScope::Surface, true},
    {"vkCreateAndroidSurfaceKHR", reinterpret_cast<void*>(&V_vkCreateAndroidSurfaceKHR), ManualScope::AndroidSurface, true},

    {"vkCreateDebugReportCallbackEXT", reinterpret_cast<void*>(&V_vkCreateDebugReportCallbackEXT), ManualScope::DebugReport, false},
    {"vkDestroyDebugReportCallbackEXT", reinterpret_cast<void*>(&V_vkDestroyDebugReportCallbackEXT), ManualScope::DebugReport, false},
    {"vkDebugReportMessageEXT", reinterpret_cast<void*>(&V_vkDebugReportMessageEXT), ManualScope::DebugReport, false},

    {"vkCreateDebugUtilsMessengerEXT", reinterpret_cast<void*>(&V_vkCreateDebugUtilsMessengerEXT), ManualScope::DebugUtils, false},
    {"vkDestroyDebugUtilsMessengerEXT", reinterpret_cast<void*>(&V_vkDestroyDebugUtilsMessengerEXT), ManualScope::DebugUtils, false},
    {"vkSubmitDebugUtilsMessageEXT", reinterpret_cast<void*>(&V_vkSubmitDebugUtilsMessageEXT), ManualScope::DebugUtils, false},

    {"vkDestroyDevice", reinterpret_cast<void*>(&V_vkDestroyDevice), ManualScope::Device, true},
    {"vkGetDeviceQueue", reinterpret_cast<void*>(&V_vkGetDeviceQueue), ManualScope::Device, true},
    {"vkGetDeviceQueue2", reinterpret_cast<void*>(&V_vkGetDeviceQueue2), ManualScope::Device, true},
    {"vkAllocateCommandBuffers", reinterpret_cast<void*>(&V_vkAllocateCommandBuffers), ManualScope::Device, true},
    {"vkFreeCommandBuffers", reinterpret_cast<void*>(&V_vkFreeCommandBuffers), ManualScope::Device, true},
    {"vkDestroyCommandPool", reinterpret_cast<void*>(&V_vkDestroyCommandPool), ManualScope::Device, true},

    {"vkCreateSwapchainKHR", reinterpret_cast<void*>(&V_vkCreateSwapchainKHR), ManualScope::Swapchain, true},
    {"vkDestroySwapchainKHR", reinterpret_cast<void*>(&V_vkDestroySwapchainKHR), ManualScope::Swapchain, true},
    {"vkGetSwapchainImagesKHR", reinterpret_cast<void*>(&V_vkGetSwapchainImagesKHR), ManualScope::Swapchain, true},
    {"vkAcquireNextImageKHR", reinterpret_cast<void*>(&V_vkAcquireNextImageKHR), ManualScope::Swapchain, true},
    {"vkAcquireNextImage2KHR", reinterpret_cast<void*>(&V_vkAcquireNextImage2KHR), ManualScope::Swapchain, false},
    {"vkQueuePresentKHR", reinterpret_cast<void*>(&V_vkQueuePresentKHR), ManualScope::Swapchain, true},
};

VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL V_vkGetInstanceProcAddr(VkInstance instance, const char* pName) {
    if (!pName || !*pName) return nullptr;
    if (instance == VK_NULL_HANDLE) {
        for (const auto& e : kManualEntries) {
            if (e.scope == ManualScope::Global && std::strcmp(e.name, pName) == 0) {
                return reinterpret_cast<PFN_vkVoidFunction>(e.fn);
            }
        }
        return nullptr;
    }
    InstanceRecord* instRec = LookupInstanceRecord(instance);
    if (!instRec) return nullptr;

    for (const auto& e : kManualEntries) {
        if (std::strcmp(e.name, pName) != 0) continue;
        switch (e.scope) {
            case ManualScope::Global:
            case ManualScope::Instance:
            case ManualScope::Device:
            case ManualScope::Swapchain:
                return reinterpret_cast<PFN_vkVoidFunction>(e.fn);
            case ManualScope::Surface:
                return instRec->enabledExtensions.contains(VK_KHR_SURFACE_EXTENSION_NAME)
                    ? reinterpret_cast<PFN_vkVoidFunction>(e.fn) : nullptr;
            case ManualScope::AndroidSurface:
                return (instRec->enabledExtensions.contains(VK_KHR_SURFACE_EXTENSION_NAME) &&
                        instRec->enabledExtensions.contains(kAndroidSurfaceExtName))
                    ? reinterpret_cast<PFN_vkVoidFunction>(e.fn) : nullptr;
            case ManualScope::DebugReport:
                return instRec->enabledExtensions.contains(VK_EXT_DEBUG_REPORT_EXTENSION_NAME)
                    ? reinterpret_cast<PFN_vkVoidFunction>(e.fn) : nullptr;
            case ManualScope::DebugUtils:
                return instRec->enabledExtensions.contains(VK_EXT_DEBUG_UTILS_EXTENSION_NAME)
                    ? reinterpret_cast<PFN_vkVoidFunction>(e.fn) : nullptr;
        }
    }
    for (const auto& e : kAutoVulkanEntries) {
        if (std::strcmp(e.name, pName) == 0) {
            return reinterpret_cast<PFN_vkVoidFunction>(e.fn);
        }
    }
    return nullptr;
}

VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL V_vkGetDeviceProcAddr(VkDevice device, const char* pName) {
    if (!device || !pName || !*pName) return nullptr;
    DeviceRecord* devRec = LookupDeviceRecord(device);
    if (!devRec) return nullptr;

    for (const auto& e : kManualEntries) {
        if (std::strcmp(e.name, pName) != 0) continue;
        if (e.scope == ManualScope::Device) return reinterpret_cast<PFN_vkVoidFunction>(e.fn);
        if (e.scope == ManualScope::Swapchain) {
            return devRec->enabledExtensions.contains(VK_KHR_SWAPCHAIN_EXTENSION_NAME)
                ? reinterpret_cast<PFN_vkVoidFunction>(e.fn) : nullptr;
        }
        return nullptr;
    }
    for (const auto& e : kAutoVulkanEntries) {
        if (e.isDevice && std::strcmp(e.name, pName) == 0) {
            return devRec->gdpa(device, pName) ? reinterpret_cast<PFN_vkVoidFunction>(e.fn) : nullptr;
        }
    }
    return nullptr;
}

void ShutdownAllVulkanLocked() {
    for (auto& [sc, rec] : g_swapchains) {
        auto dit = g_devices.find(rec.device);
        if (dit != g_devices.end()) {
            DeviceRecord* drec = dit->second.get();
            if (!drec->lost && drec->dt.vkDeviceWaitIdle) drec->dt.vkDeviceWaitIdle(drec->hostDevice);
            CleanupSwapchainEmbedResources(drec, rec);
            auto destroySc = reinterpret_cast<PFN_vkDestroySwapchainKHR>(drec->gdpa(rec.device, "vkDestroySwapchainKHR"));
            if (destroySc) destroySc(rec.device, sc, nullptr);
        }
    }
    g_swapchains.clear();

    for (auto& [dev, drec] : g_devices) {
        if (!drec->lost && drec->dt.vkDeviceWaitIdle) drec->dt.vkDeviceWaitIdle(dev);
        auto destroyDev = reinterpret_cast<PFN_vkDestroyDevice>(drec->gdpa(dev, "vkDestroyDevice"));
        if (destroyDev) destroyDev(dev, nullptr);
    }
    g_devices.clear();
    g_queueToDevice.clear();
    g_cmdToDevice.clear();
    g_poolCmds.clear();

    for (auto& [surf, srec] : g_surfaces) {
        if (srec.anwToken) ReleaseSurface(srec.anwToken);
        auto iit = g_instances.find(srec.instance);
        if (iit != g_instances.end()) {
            auto destroySurf = reinterpret_cast<PFN_vkDestroySurfaceKHR>(iit->second->gipa(srec.instance, "vkDestroySurfaceKHR"));
            if (destroySurf) destroySurf(srec.instance, surf, nullptr);
        }
    }
    g_surfaces.clear();

    for (auto& [h, dr] : g_debugReports) {
        if (dr->hostHandle) {
            auto iit = g_instances.find(dr->instance);
            if (iit != g_instances.end()) {
                auto destroyDr = reinterpret_cast<PFN_vkDestroyDebugReportCallbackEXT>(
                    iit->second->gipa(dr->instance, "vkDestroyDebugReportCallbackEXT"));
                if (destroyDr) destroyDr(dr->instance, dr->hostHandle, nullptr);
            }
        }
    }
    g_debugReports.clear();

    for (auto& [h, du] : g_debugMessengers) {
        if (du->hostHandle) {
            auto iit = g_instances.find(du->instance);
            if (iit != g_instances.end()) {
                auto destroyDu = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
                    iit->second->gipa(du->instance, "vkDestroyDebugUtilsMessengerEXT"));
                if (destroyDu) destroyDu(du->instance, du->hostHandle, nullptr);
            }
        }
    }
    g_debugMessengers.clear();

    const bool hadInstances = !g_instances.empty();
    for (auto& [inst, irec] : g_instances) {
        auto destroyInst = reinterpret_cast<PFN_vkDestroyInstance>(irec->gipa(inst, "vkDestroyInstance"));
        if (destroyInst) destroyInst(inst, nullptr);
    }
    g_instances.clear();
    g_physToInst.clear();
    if (hadInstances) {
        LogLine("[winapk] guest Vulkan instance teardown complete");
    }
    InvalidateTlsCaches();
}

}  // namespace

void SetGuestVulkanModules(guest::ModuleSet* modules) {
    std::lock_guard lock(g_modulesMu);
    g_guestModules = modules;
}

bool GuestVulkanOwnsWindow() {
    std::shared_lock lock(g_vkMu);
    return !g_surfaces.empty();
}

bool GuestVulkanPresents(HWND window) {
    if (IsEmbedded()) return false;
    std::shared_lock lock(g_vkMu);
    for (const auto& [sc, rec] : g_swapchains) {
        if (!rec.presented) continue;
        auto sit = g_surfaces.find(rec.surface);
        if (sit != g_surfaces.end() && sit->second.hwnd == window) return true;
    }
    return false;
}

void RetireGuestVulkan(HWND window, bool closing) {
    {
        std::unique_lock lock(g_vkMu);
        for (auto& [surf, rec] : g_surfaces) {
            if (!window || rec.hwnd == window) {
                if (rec.anwToken) {
                    ReleaseSurface(rec.anwToken);
                    rec.anwToken = nullptr;
                }
                rec.hwnd = nullptr;
            }
        }
        if (closing) {
            ShutdownAllVulkanLocked();
        }
    }
    if (closing) {
        {
            std::lock_guard lock(g_modulesMu);
            g_guestModules = nullptr;
        }
        std::lock_guard allocLock(g_allocMu);
        g_allocators.clear();
    }
}

void AddGuestVulkanLibrary(std::vector<guest::BuiltinLibrary>& libraries) {
    std::vector<guest::HostSymbol> syms;
    for (const auto& e : kManualEntries) {
        if (e.exported) syms.push_back({e.name, e.fn});
    }
    for (const auto& e : kAutoVulkanEntries) {
        if (e.exported) syms.push_back({e.name, e.fn});
    }
    libraries.push_back({"libvulkan.so", std::move(syms), ""});
}

}  // namespace winapk::bridge
