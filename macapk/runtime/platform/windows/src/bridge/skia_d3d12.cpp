// skia_d3d12.cpp -- Offscreen Direct3D 12 hardware device and command queue backing
// com.macapk.graphics.NativeSkiaD3D12 for Skia's Ganesh D3D12 backend (PLAN.md §15, T15).
//
// Deliberately independent of the app window, ANGLE and guest Vulkan: AGSL RuntimeShader and
// RenderEffect evaluate into offscreen Skia GPU surfaces and read back straight-alpha pixels.
#include "skia_d3d12.h"
#include "bridge.h"

#include <windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>

#include <cstring>
#include <string>

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

void JNICALL ReleaseAdapterDeviceQueue(JNIEnv*, jclass, jlong adapterPtr, jlong devicePtr, jlong queuePtr) {
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
};
#undef M

}  // namespace

bool RegisterSkia(JNIEnv* env) {
    return Bind(env, "com/macapk/graphics/NativeSkiaD3D12", kSkiaMethods, static_cast<int>(std::size(kSkiaMethods)));
}

}  // namespace winapk::bridge
