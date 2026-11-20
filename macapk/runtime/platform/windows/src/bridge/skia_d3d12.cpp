// skia_d3d12.cpp -- Offscreen Direct3D 12 hardware device and command queue backing
// com.macapk.graphics.NativeSkiaD3D12 for Skia's Ganesh D3D12 backend (PLAN.md §15, T15).
//
// Deliberately independent of the app window, ANGLE and guest Vulkan: AGSL RuntimeShader and
// RenderEffect evaluate into offscreen Skia GPU surfaces and read back straight-alpha pixels.
#include "skia_d3d12.h"
#include "bridge.h"
#include "compositor.h"
#include "graphics.h"

#include <windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>

#include <algorithm>
#include <cstring>
#include <map>
#include <mutex>
#include <string>
#include <vector>

namespace winapk::bridge {
namespace {

using Microsoft::WRL::ComPtr;

void ThrowIllegalState(JNIEnv* env, const char* msg) {
    if (env->ExceptionCheck()) return;
    if (jclass cls = env->FindClass("java/lang/IllegalStateException")) {
        env->ThrowNew(cls, msg);
        env->DeleteLocalRef(cls);
    }
}

jlongArray JNICALL CreateAdapterDeviceQueue(JNIEnv* env, jclass) {
    if (GetEnvironmentVariableW(L"WINAPK_DISABLE_SKIA", nullptr, 0) > 0) {
        ThrowIllegalState(env, "GRAPHICS_CAPABILITY_MISSING: Skia Direct3D12 disabled by WINAPK_DISABLE_SKIA");
        return nullptr;
    }
    const bool allowWarp = GetEnvironmentVariableW(L"WINAPK_SKIA_ALLOW_WARP", nullptr, 0) > 0;

    ComPtr<IDXGIFactory4> factory;
    if (FAILED(CreateDXGIFactory1(IID_PPV_ARGS(&factory))) || !factory) {
        ThrowIllegalState(env, "GRAPHICS_CAPABILITY_MISSING: CreateDXGIFactory1 failed for Skia D3D12");
        return nullptr;
    }

    ComPtr<IDXGIAdapter1> chosenAdapter;
    ComPtr<ID3D12Device> chosenDevice;
    for (UINT idx = 0;; ++idx) {
        ComPtr<IDXGIAdapter1> adapter;
        if (factory->EnumAdapters1(idx, &adapter) == DXGI_ERROR_NOT_FOUND) break;
        if (!adapter) continue;
        DXGI_ADAPTER_DESC1 desc{};
        if (FAILED(adapter->GetDesc1(&desc))) continue;
        if ((desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) && !allowWarp) continue;

        ComPtr<ID3D12Device> dev;
        if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&dev))) && dev) {
            chosenAdapter = adapter;
            chosenDevice = dev;
            break;
        }
    }
    if (!chosenAdapter || !chosenDevice) {
        ThrowIllegalState(env, "GRAPHICS_CAPABILITY_MISSING: no hardware Direct3D 12 device available for Skia AGSL");
        return nullptr;
    }

    D3D12_COMMAND_QUEUE_DESC qdesc{};
    qdesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    qdesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
    qdesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    ComPtr<ID3D12CommandQueue> queue;
    if (FAILED(chosenDevice->CreateCommandQueue(&qdesc, IID_PPV_ARGS(&queue))) || !queue) {
        ThrowIllegalState(env, "GRAPHICS_CAPABILITY_MISSING: cannot create Skia Direct3D 12 command queue");
        return nullptr;
    }

    jlongArray res = env->NewLongArray(3);
    if (!res) return nullptr;
    // Transfer one COM reference per handle to Java; releaseAdapterDeviceQueue balances them.
    jlong handles[3] = {
        static_cast<jlong>(reinterpret_cast<uintptr_t>(chosenAdapter.Detach())),
        static_cast<jlong>(reinterpret_cast<uintptr_t>(chosenDevice.Detach())),
        static_cast<jlong>(reinterpret_cast<uintptr_t>(queue.Detach())),
    };
    env->SetLongArrayRegion(res, 0, 3, handles);
    if (env->ExceptionCheck()) {
        reinterpret_cast<ID3D12CommandQueue*>(static_cast<uintptr_t>(handles[2]))->Release();
        reinterpret_cast<ID3D12Device*>(static_cast<uintptr_t>(handles[1]))->Release();
        reinterpret_cast<IDXGIAdapter1*>(static_cast<uintptr_t>(handles[0]))->Release();
        return nullptr;
    }
    return res;
}

struct SharedD3D12Slot {
    int width = 0;
    int height = 0;
    UINT rowPitch = 0;
    ComPtr<ID3D12Resource> sharedTex;
    ComPtr<ID3D12Resource> uploadBuf;
    ComPtr<ID3D12CommandAllocator> alloc;
    ComPtr<ID3D12GraphicsCommandList> cmd;
    ComPtr<ID3D12Fence> fence;
    HANDLE fenceEvent = nullptr;
    HANDLE ntSharedHandle = nullptr;
    UINT64 fenceVal = 0;

    ~SharedD3D12Slot() {
        if (ntSharedHandle) CloseHandle(ntSharedHandle);
        if (fenceEvent) CloseHandle(fenceEvent);
    }
};

std::mutex g_sharedMu;
std::map<int, std::unique_ptr<SharedD3D12Slot>> g_sharedSlots;

void ClearSharedSlotsLocked() {
    g_sharedSlots.clear();
}

bool UploadAndShareD3D12Locked(ID3D12Device* device, ID3D12CommandQueue* queue, int slotKey,
                               const uint8_t* bgraBytes, int w, int h, HANDLE& ntHandleOut) {
    ntHandleOut = nullptr;
    if (!device || !queue || !bgraBytes || w <= 0 || h <= 0) return false;

    const UINT alignedPitch = (static_cast<UINT>(w) * 4u + 255u) & ~255u;
    const UINT64 uploadBytes = static_cast<UINT64>(alignedPitch) * static_cast<UINT64>(h);

    auto& ptr = g_sharedSlots[slotKey];
    if (!ptr || ptr->width != w || ptr->height != h || !ptr->sharedTex || !ptr->ntSharedHandle) {
        auto next = std::make_unique<SharedD3D12Slot>();
        next->width = w;
        next->height = h;
        next->rowPitch = alignedPitch;

        D3D12_HEAP_PROPERTIES defaultHeap{};
        defaultHeap.Type = D3D12_HEAP_TYPE_DEFAULT;
        D3D12_RESOURCE_DESC texDesc{};
        texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        texDesc.Width = static_cast<UINT64>(w);
        texDesc.Height = static_cast<UINT>(h);
        texDesc.DepthOrArraySize = 1;
        texDesc.MipLevels = 1;
        texDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        texDesc.SampleDesc = {1, 0};
        texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
        texDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET | D3D12_RESOURCE_FLAG_ALLOW_SIMULTANEOUS_ACCESS;

        if (FAILED(device->CreateCommittedResource(&defaultHeap, D3D12_HEAP_FLAG_SHARED, &texDesc,
                                                   D3D12_RESOURCE_STATE_COMMON, nullptr,
                                                   IID_PPV_ARGS(&next->sharedTex)))) {
            return false;
        }
        if (FAILED(device->CreateSharedHandle(next->sharedTex.Get(), nullptr, GENERIC_ALL, nullptr, &next->ntSharedHandle)) ||
            !next->ntSharedHandle) {
            return false;
        }

        D3D12_HEAP_PROPERTIES uploadHeap{};
        uploadHeap.Type = D3D12_HEAP_TYPE_UPLOAD;
        D3D12_RESOURCE_DESC bufDesc{};
        bufDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        bufDesc.Width = uploadBytes;
        bufDesc.Height = 1;
        bufDesc.DepthOrArraySize = 1;
        bufDesc.MipLevels = 1;
        bufDesc.Format = DXGI_FORMAT_UNKNOWN;
        bufDesc.SampleDesc = {1, 0};
        bufDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

        if (FAILED(device->CreateCommittedResource(&uploadHeap, D3D12_HEAP_FLAG_NONE, &bufDesc,
                                                   D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
                                                   IID_PPV_ARGS(&next->uploadBuf)))) {
            return false;
        }
        if (FAILED(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&next->alloc))) ||
            FAILED(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, next->alloc.Get(), nullptr,
                                             IID_PPV_ARGS(&next->cmd))) ||
            FAILED(next->cmd->Close()) ||
            FAILED(device->CreateFence(0, D3D12_FENCE_FLAG_SHARED, IID_PPV_ARGS(&next->fence)))) {
            return false;
        }
        next->fenceEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
        if (!next->fenceEvent) return false;
        ptr = std::move(next);
    }

    void* mapped = nullptr;
    D3D12_RANGE readNone{0, 0};
    if (FAILED(ptr->uploadBuf->Map(0, &readNone, &mapped)) || !mapped) return false;
    auto* dst = static_cast<uint8_t*>(mapped);
    for (int y = 0; y < h; ++y) {
        std::memcpy(dst + static_cast<size_t>(y) * ptr->rowPitch,
                    bgraBytes + static_cast<size_t>(y) * static_cast<size_t>(w) * 4,
                    static_cast<size_t>(w) * 4);
    }
    ptr->uploadBuf->Unmap(0, nullptr);

    if (FAILED(ptr->alloc->Reset()) || FAILED(ptr->cmd->Reset(ptr->alloc.Get(), nullptr))) return false;

    D3D12_RESOURCE_BARRIER toCopy{};
    toCopy.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    toCopy.Transition.pResource = ptr->sharedTex.Get();
    toCopy.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    toCopy.Transition.StateBefore = D3D12_RESOURCE_STATE_COMMON;
    toCopy.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
    ptr->cmd->ResourceBarrier(1, &toCopy);

    D3D12_TEXTURE_COPY_LOCATION dstLoc{};
    dstLoc.pResource = ptr->sharedTex.Get();
    dstLoc.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    dstLoc.SubresourceIndex = 0;

    D3D12_TEXTURE_COPY_LOCATION srcLoc{};
    srcLoc.pResource = ptr->uploadBuf.Get();
    srcLoc.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    srcLoc.PlacedFootprint.Offset = 0;
    srcLoc.PlacedFootprint.Footprint.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    srcLoc.PlacedFootprint.Footprint.Width = static_cast<UINT>(w);
    srcLoc.PlacedFootprint.Footprint.Height = static_cast<UINT>(h);
    srcLoc.PlacedFootprint.Footprint.Depth = 1;
    srcLoc.PlacedFootprint.Footprint.RowPitch = ptr->rowPitch;

    ptr->cmd->CopyTextureRegion(&dstLoc, 0, 0, 0, &srcLoc, nullptr);

    D3D12_RESOURCE_BARRIER toCommon{};
    toCommon.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    toCommon.Transition.pResource = ptr->sharedTex.Get();
    toCommon.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    toCommon.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
    toCommon.Transition.StateAfter = D3D12_RESOURCE_STATE_COMMON;
    ptr->cmd->ResourceBarrier(1, &toCommon);

    if (FAILED(ptr->cmd->Close())) return false;
    ID3D12CommandList* lists[] = {ptr->cmd.Get()};
    queue->ExecuteCommandLists(1, lists);

    const UINT64 fv = ++ptr->fenceVal;
    if (FAILED(queue->Signal(ptr->fence.Get(), fv))) return false;
    if (ptr->fence->GetCompletedValue() < fv) {
        if (FAILED(ptr->fence->SetEventOnCompletion(fv, ptr->fenceEvent))) return false;
        WaitForSingleObject(ptr->fenceEvent, 2000);
    }
    ntHandleOut = ptr->ntSharedHandle;
    return true;
}

jboolean JNICALL PresentSharedLayer(JNIEnv*, jclass, jlong devicePtr, jlong queuePtr,
                                    jint layerKey, jlong bgraPixelsPtr,
                                    jint width, jint height, jint x, jint y, jboolean zTop) {
    auto* device = reinterpret_cast<ID3D12Device*>(static_cast<uintptr_t>(devicePtr));
    auto* queue = reinterpret_cast<ID3D12CommandQueue*>(static_cast<uintptr_t>(queuePtr));
    const auto* pixels = reinterpret_cast<const uint8_t*>(static_cast<uintptr_t>(bgraPixelsPtr));
    if (!device || !queue || !pixels || width <= 0 || height <= 0) return JNI_FALSE;

    int winW = 0, winH = 0;
    HWND hwnd = static_cast<HWND>(EnsureJavaSurfaceViewWindow(&winW, &winH));
    if (!hwnd) return JNI_FALSE;

    HANDLE ntHandle = nullptr;
    {
        std::lock_guard lock(g_sharedMu);
        if (!UploadAndShareD3D12Locked(device, queue, layerKey, pixels, width, height, ntHandle)) {
            return JNI_FALSE;
        }
    }
    const SurfaceLayerPlacement placement{x, y, width, height, true, zTop == JNI_TRUE};
    const bool ok = CompositorSubmitSurfaceSharedNtHandle(hwnd, layerKey, ntHandle, width, height,
                                                          LayerAlpha::Straight, false, placement);
    if (ok) InvalidateRect(hwnd, nullptr, FALSE);
    return ok ? JNI_TRUE : JNI_FALSE;
}

jboolean JNICALL VerifySharedCompositorRoundtrip(JNIEnv* env, jclass, jlong devicePtr, jlong queuePtr,
                                                 jlong bgraPixelsPtr, jint width, jint height, jintArray outArgb) {
    auto* device = reinterpret_cast<ID3D12Device*>(static_cast<uintptr_t>(devicePtr));
    auto* queue = reinterpret_cast<ID3D12CommandQueue*>(static_cast<uintptr_t>(queuePtr));
    const auto* pixels = reinterpret_cast<const uint8_t*>(static_cast<uintptr_t>(bgraPixelsPtr));
    if (!device || !queue || !pixels || !outArgb || width <= 0 || height <= 0 ||
        env->GetArrayLength(outArgb) < static_cast<jsize>(width) * height) {
        return JNI_FALSE;
    }

    HWND dummyHwnd = CreateWindowExW(0, L"STATIC", L"winapk-skia-d3d12-interop", WS_POPUP,
                                     0, 0, width, height, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
    if (!dummyHwnd) return JNI_FALSE;

    bool ok = false;
    HANDLE ntHandle = nullptr;
    {
        std::lock_guard lock(g_sharedMu);
        ok = UploadAndShareD3D12Locked(device, queue, -999999, pixels, width, height, ntHandle);
    }
    if (ok) {
        const SurfaceLayerPlacement placement{0, 0, width, height, true, true};
        ok = CompositorSubmitSurfaceSharedNtHandle(dummyHwnd, -999999, ntHandle, width, height,
                                                   LayerAlpha::Straight, false, placement);
    }
    if (ok) {
        std::vector<uint8_t> rgba(static_cast<size_t>(width) * static_cast<size_t>(height) * 4);
        ok = CompositorReadbackRgba(dummyHwnd, rgba.data(), width, height);
        if (ok) {
            std::vector<jint> argb(static_cast<size_t>(width) * static_cast<size_t>(height));
            for (size_t i = 0; i < argb.size(); ++i) {
                const uint32_t r = rgba[i * 4 + 0];
                const uint32_t g = rgba[i * 4 + 1];
                const uint32_t b = rgba[i * 4 + 2];
                const uint32_t a = rgba[i * 4 + 3];
                argb[i] = static_cast<jint>((a << 24) | (r << 16) | (g << 8) | b);
            }
            env->SetIntArrayRegion(outArgb, 0, static_cast<jsize>(argb.size()), argb.data());
        }
    }
    CompositorDestroyWindow(dummyHwnd);
    DestroyWindow(dummyHwnd);
    {
        std::lock_guard lock(g_sharedMu);
        g_sharedSlots.erase(-999999);
    }
    return ok ? JNI_TRUE : JNI_FALSE;
}

void JNICALL ReleaseAdapterDeviceQueue(JNIEnv*, jclass, jlong adapterPtr, jlong devicePtr, jlong queuePtr) {
    {
        std::lock_guard lock(g_sharedMu);
        ClearSharedSlotsLocked();
    }
    auto* adapter = reinterpret_cast<IDXGIAdapter1*>(static_cast<uintptr_t>(adapterPtr));
    auto* device = reinterpret_cast<ID3D12Device*>(static_cast<uintptr_t>(devicePtr));
    auto* queue = reinterpret_cast<ID3D12CommandQueue*>(static_cast<uintptr_t>(queuePtr));

    if (device && queue) {
        ComPtr<ID3D12Fence> fence;
        if (SUCCEEDED(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence))) && fence) {
            HANDLE ev = CreateEventW(nullptr, FALSE, FALSE, nullptr);
            if (ev) {
                if (SUCCEEDED(queue->Signal(fence.Get(), 1)) &&
                    SUCCEEDED(fence->SetEventOnCompletion(1, ev))) {
                    WaitForSingleObject(ev, 2000);
                }
                CloseHandle(ev);
            }
        }
    }
    if (queue) queue->Release();
    if (device) device->Release();
    if (adapter) adapter->Release();
}

jstring JNICALL AdapterName(JNIEnv* env, jclass, jlong adapterPtr) {
    auto* adapter = reinterpret_cast<IDXGIAdapter1*>(static_cast<uintptr_t>(adapterPtr));
    if (!adapter) return env->NewStringUTF("unknown");
    DXGI_ADAPTER_DESC1 desc{};
    if (FAILED(adapter->GetDesc1(&desc))) return env->NewStringUTF("unknown");
    const size_t len = wcsnlen(desc.Description, ARRAYSIZE(desc.Description));
    return env->NewString(reinterpret_cast<const jchar*>(desc.Description), static_cast<jsize>(len));
}

#define M(name, sig, fn) {const_cast<char*>(name), const_cast<char*>(sig), reinterpret_cast<void*>(fn)}
const JNINativeMethod kSkiaMethods[] = {
    M("createAdapterDeviceQueue", "()[J", CreateAdapterDeviceQueue),
    M("releaseAdapterDeviceQueue", "(JJJ)V", ReleaseAdapterDeviceQueue),
    M("adapterName", "(J)Ljava/lang/String;", AdapterName),
    M("presentSharedLayer", "(JJIJIIIIZ)Z", PresentSharedLayer),
    M("verifySharedCompositorRoundtrip", "(JJJII[I)Z", VerifySharedCompositorRoundtrip),
};
#undef M

}  // namespace

bool RegisterSkia(JNIEnv* env) {
    return Bind(env, "com/macapk/graphics/NativeSkiaD3D12", kSkiaMethods, static_cast<int>(std::size(kSkiaMethods)));
}

}  // namespace winapk::bridge
