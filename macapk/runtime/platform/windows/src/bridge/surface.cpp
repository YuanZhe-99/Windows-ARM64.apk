// surface.cpp -- multi-surface Android ANativeWindow identities and CPU lock/unlockAndPost.
#include "surface.h"
#include "embed.h"
#include "../../../../guest/jni/bridge.h"

#include <algorithm>
#include <cerrno>
#include <cstdint>
#include <cstring>
#include <limits>
#include <map>
#include <mutex>
#include <new>
#include <vector>

namespace winapk::bridge {
namespace {

constexpr int32_t kWindowFormatRgba8888 = 1;
constexpr int32_t kWindowFormatRgbx8888 = 2;
constexpr int32_t kWindowFormatRgb565 = 4;

// Android NDK LP64 `ANativeWindow_Buffer` layout (`<android/native_window.h>`).
struct GuestNativeWindowBuffer {
    int32_t width;
    int32_t height;
    int32_t stride;
    int32_t format;
    void* bits;
    uint32_t reserved[6];
};
static_assert(sizeof(GuestNativeWindowBuffer) == 48);

// Android NDK `ARect` (`<android/rect.h>`).
struct GuestARect {
    int32_t left, top, right, bottom;
};
static_assert(sizeof(GuestARect) == 16);

struct Attachment {
    HWND window = nullptr;
    jobject surface = nullptr;   ///< borrowed global ref owned by `View` until retired
    jfieldID released = nullptr; ///< `Surface.mReleased`
    int width = 0, height = 0;
    int bufWidth = 0, bufHeight = 0; ///< 0 = follow `width` / `height`
    int32_t format = kWindowFormatRgba8888;
    bool isRoot = true;
    bool locked = false;
    SurfaceLayerPlacement placement{};
    uintptr_t token = 0;
    std::vector<uint8_t> cpuBuffer;
};

std::mutex surfaceMutex;
std::vector<Attachment> attachments;
bool surfaceBridgeRegistered = false;
std::map<uintptr_t, unsigned> references; // includes retired tokens until last release
uintptr_t nextToken = 1;
constexpr size_t kMaxTokens = 1024;

Attachment* FindByTokenLocked(uintptr_t id) {
    if (!id) return nullptr;
    for (auto& a : attachments) {
        if (a.token == id) return &a;
    }
    return nullptr;
}

void NotifyJavaConsumer(JNIEnv* env, jobject surface) {
    jclass cls = env->GetObjectClass(surface);
    if (!cls) { env->ExceptionClear(); return; }
    jmethodID mid = env->GetMethodID(cls, "macapkNoteNativeWindowConsumer", "()V");
    if (mid) {
        env->CallVoidMethod(surface, mid);
    }
    env->ExceptionClear();
    env->DeleteLocalRef(cls);
}

/// Converts a published live `Surface` into an opaque guest `ANativeWindow*` token.
void* FromSurface(void* guestEnv, jobject surface) {
    JNIEnv* env = guest::HostEnvironment(guestEnv);
    if (!env || !surface || env->ExceptionCheck()) return nullptr;
    uintptr_t resultToken = 0;
    bool firstAcquire = false;
    {
        std::lock_guard lock(surfaceMutex);
        Attachment* match = nullptr;
        for (auto& a : attachments) {
            if (a.surface && env->IsSameObject(surface, a.surface)) {
                match = &a;
                break;
            }
        }
        if (!match || env->GetBooleanField(surface, match->released)) return nullptr;
        if (match->token) {
            auto& count = references.find(match->token)->second;
            if (count == std::numeric_limits<unsigned>::max()) return nullptr;
            ++count;
        } else {
            if (references.size() >= kMaxTokens || !nextToken) return nullptr;
            try {
                references.emplace(nextToken, 1);
            } catch (const std::bad_alloc&) {
                return nullptr;
            }
            match->token = nextToken++;
            firstAcquire = true;
        }
        resultToken = match->token;
    }
    if (firstAcquire) NotifyJavaConsumer(env, surface);
    return reinterpret_cast<void*>(resultToken);
}

void Acquire(void* window) {
    std::lock_guard lock(surfaceMutex);
    auto it = references.find(reinterpret_cast<uintptr_t>(window));
    if (it != references.end() && it->second != std::numeric_limits<unsigned>::max()) ++it->second;
}

void Release(void* window) {
    HWND dropHwnd = nullptr;
    int dropKey = 0;
    {
        std::lock_guard lock(surfaceMutex);
        auto it = references.find(reinterpret_cast<uintptr_t>(window));
        if (it != references.end() && --it->second == 0) {
            if (Attachment* a = FindByTokenLocked(it->first)) {
                dropHwnd = a->window;
                dropKey = -static_cast<int>(a->token);
                a->token = 0;
                a->locked = false;
                a->cpuBuffer.clear();
            }
            references.erase(it);
        }
    }
    if (dropHwnd && dropKey != 0) {
        CompositorUpdateSurfacePlacement(dropHwnd, dropKey, {}, true);
    }
}

template<bool Width> int32_t Dimension(void* window) {
    std::lock_guard lock(surfaceMutex);
    Attachment* a = FindByTokenLocked(reinterpret_cast<uintptr_t>(window));
    if (!a) return -EINVAL;
    if constexpr (Width) return a->bufWidth > 0 ? a->bufWidth : a->width;
    else return a->bufHeight > 0 ? a->bufHeight : a->height;
}

int32_t GetFormat(void* window) {
    std::lock_guard lock(surfaceMutex);
    Attachment* a = FindByTokenLocked(reinterpret_cast<uintptr_t>(window));
    if (!a) return -EINVAL;
    return a->format;
}

int32_t SetBuffersGeometry(void* window, int32_t w, int32_t h, int32_t format) {
    if (w < 0 || h < 0) return -EINVAL;
    if (format != 0 && format != kWindowFormatRgba8888 && format != kWindowFormatRgbx8888 && format != kWindowFormatRgb565) {
        return -EINVAL;
    }
    std::lock_guard lock(surfaceMutex);
    Attachment* a = FindByTokenLocked(reinterpret_cast<uintptr_t>(window));
    if (!a || a->locked) return -EINVAL;
    a->bufWidth = w;
    a->bufHeight = h;
    if (format != 0) a->format = format;
    return 0;
}

int32_t LockWindow(void* window, GuestNativeWindowBuffer* outBuffer, GuestARect* inOutDirtyBounds) {
    if (!outBuffer) return -EINVAL;
    std::lock_guard lock(surfaceMutex);
    Attachment* a = FindByTokenLocked(reinterpret_cast<uintptr_t>(window));
    if (!a || a->locked) return -EINVAL;
    const int w = a->bufWidth > 0 ? a->bufWidth : a->width;
    const int h = a->bufHeight > 0 ? a->bufHeight : a->height;
    if (w <= 0 || h <= 0) return -EINVAL;
    const size_t bpp = a->format == kWindowFormatRgb565 ? 2u : 4u;
    const size_t needed = static_cast<size_t>(w) * static_cast<size_t>(h) * bpp;
    try {
        if (a->cpuBuffer.size() != needed) a->cpuBuffer.assign(needed, 0);
    } catch (const std::bad_alloc&) {
        return -ENOMEM;
    }
    a->locked = true;
    outBuffer->width = w;
    outBuffer->height = h;
    outBuffer->stride = w;
    outBuffer->format = a->format;
    outBuffer->bits = a->cpuBuffer.data();
    std::memset(outBuffer->reserved, 0, sizeof(outBuffer->reserved));
    if (inOutDirtyBounds) {
        inOutDirtyBounds->left = 0;
        inOutDirtyBounds->top = 0;
        inOutDirtyBounds->right = w;
        inOutDirtyBounds->bottom = h;
    }
    return 0;
}

int32_t UnlockAndPostWindow(void* window) {
    HWND hwnd = nullptr;
    int layerKey = 0;
    int w = 0, h = 0;
    int32_t fmt = kWindowFormatRgba8888;
    SurfaceLayerPlacement placement{};
    std::vector<uint8_t> rgbaCopy;
    {
        std::lock_guard lock(surfaceMutex);
        Attachment* a = FindByTokenLocked(reinterpret_cast<uintptr_t>(window));
        if (!a || !a->locked) return -EINVAL;
        a->locked = false;
        w = a->bufWidth > 0 ? a->bufWidth : a->width;
        h = a->bufHeight > 0 ? a->bufHeight : a->height;
        if (w <= 0 || h <= 0) return 0;
        hwnd = a->window;
        layerKey = -static_cast<int>(a->token);
        fmt = a->format;
        placement = a->placement;
        if (placement.width <= 0 || placement.height <= 0) {
            placement.width = a->width;
            placement.height = a->height;
            placement.visible = true;
        }
        try {
            if (fmt == kWindowFormatRgb565) {
                rgbaCopy.resize(static_cast<size_t>(w) * static_cast<size_t>(h) * 4);
                const auto* src = reinterpret_cast<const uint16_t*>(a->cpuBuffer.data());
                const size_t n = static_cast<size_t>(w) * static_cast<size_t>(h);
                for (size_t i = 0; i < n; ++i) {
                    const uint16_t p = src[i];
                    const uint8_t r5 = static_cast<uint8_t>((p >> 11) & 0x1f);
                    const uint8_t g6 = static_cast<uint8_t>((p >> 5) & 0x3f);
                    const uint8_t b5 = static_cast<uint8_t>(p & 0x1f);
                    rgbaCopy[i * 4 + 0] = static_cast<uint8_t>((r5 << 3) | (r5 >> 2));
                    rgbaCopy[i * 4 + 1] = static_cast<uint8_t>((g6 << 2) | (g6 >> 4));
                    rgbaCopy[i * 4 + 2] = static_cast<uint8_t>((b5 << 3) | (b5 >> 2));
                    rgbaCopy[i * 4 + 3] = 0xff;
                }
            } else {
                rgbaCopy = a->cpuBuffer;
            }
        } catch (const std::bad_alloc&) {
            return -ENOMEM;
        }
    }
    const LayerAlpha alpha = fmt == kWindowFormatRgbx8888 ? LayerAlpha::Opaque : LayerAlpha::Straight;
    if (IsEmbedded()) SendEmbedEvent("backend native-window");
    CompositorSubmitSurfaceBytes(hwnd, layerKey, rgbaCopy.data(), static_cast<size_t>(w) * 4,
                                 w, h, LayerPixels::Rgba8, alpha, false, placement);
    InvalidateRect(hwnd, nullptr, FALSE);
    return 0;
}

} // namespace

void PublishSurface(HWND window, jobject surface, jfieldID released, int width, int height, bool replaceRoot) {
    if (!window || !surface) return;
    std::lock_guard lock(surfaceMutex);
    surfaceBridgeRegistered = true;
    for (auto& a : attachments) {
        if (a.window == window && a.surface == surface) {
            a.width = width;
            a.height = height;
            if (a.placement.width <= 0 || a.isRoot) {
                a.placement.width = width;
                a.placement.height = height;
                a.placement.visible = width > 0 && height > 0;
            }
            if (replaceRoot) a.isRoot = true;
            return;
        }
    }
    if (replaceRoot) {
        attachments.erase(
            std::remove_if(attachments.begin(), attachments.end(),
                           [window](const Attachment& a) { return a.window == window && a.isRoot; }),
            attachments.end());
    }
    Attachment fresh{};
    fresh.window = window;
    fresh.surface = surface;
    fresh.released = released;
    fresh.width = width;
    fresh.height = height;
    fresh.isRoot = replaceRoot;
    fresh.placement = {0, 0, width, height, width > 0 && height > 0, false};
    attachments.push_back(std::move(fresh));
}

void ResizeSurface(HWND window, int width, int height) {
    std::lock_guard lock(surfaceMutex);
    for (auto& a : attachments) {
        if (a.window != window) continue;
        if (a.isRoot) {
            a.width = width;
            a.height = height;
            a.placement.width = width;
            a.placement.height = height;
            a.placement.visible = width > 0 && height > 0;
        }
    }
}

void UpdateSurfaceBounds(HWND window, jobject surface, const SurfaceLayerPlacement& placement) {
    int layerKey = 0;
    {
        std::lock_guard lock(surfaceMutex);
        for (auto& a : attachments) {
            if (a.window == window && a.surface == surface) {
                a.placement = placement;
                if (!a.isRoot && placement.width > 0 && placement.height > 0) {
                    a.width = placement.width;
                    a.height = placement.height;
                } else if (!placement.visible) {
                    if (!a.isRoot) a.width = a.height = 0;
                }
                if (a.token) layerKey = -static_cast<int>(a.token);
                break;
            }
        }
    }
    if (layerKey != 0) {
        CompositorUpdateSurfacePlacement(window, layerKey, placement, false);
    }
}

void RetireSurfaceObject(JNIEnv* env, jobject surface) {
    if (!env || !surface) return;
    HWND dropHwnd = nullptr;
    int dropKey = 0;
    {
        std::lock_guard lock(surfaceMutex);
        for (auto it = attachments.begin(); it != attachments.end(); ++it) {
            if (it->surface && env->IsSameObject(surface, it->surface)) {
                if (it->token) {
                    dropHwnd = it->window;
                    dropKey = -static_cast<int>(it->token);
                }
                attachments.erase(it);
                break;
            }
        }
    }
    if (dropHwnd && dropKey != 0) {
        CompositorUpdateSurfacePlacement(dropHwnd, dropKey, {}, true);
    }
}

void RetireSurface(HWND window) {
    std::lock_guard lock(surfaceMutex);
    if (!window) {
        attachments.clear();
        return;
    }
    attachments.erase(
        std::remove_if(attachments.begin(), attachments.end(),
                       [window](const Attachment& a) { return a.window == window; }),
        attachments.end());
}

bool ResolveSurface(void* token, HWND& window, int& width, int& height, SurfaceLayerPlacement* placementOut) {
    std::lock_guard lock(surfaceMutex);
    Attachment* a = FindByTokenLocked(reinterpret_cast<uintptr_t>(token));
    if (!a) return false;
    window = a->window;
    width = a->bufWidth > 0 ? a->bufWidth : a->width;
    height = a->bufHeight > 0 ? a->bufHeight : a->height;
    if (placementOut) {
        *placementOut = a->placement;
        if (placementOut->width <= 0 && a->width > 0) {
            placementOut->width = a->width;
            placementOut->height = a->height;
            placementOut->visible = true;
        }
    }
    return true;
}

bool RetainSurface(void* token) {
    std::lock_guard lock(surfaceMutex);
    const auto id = reinterpret_cast<uintptr_t>(token);
    Attachment* a = FindByTokenLocked(id);
    if (!a) return false;
    auto& count = references.find(id)->second;
    if (count == std::numeric_limits<unsigned>::max()) return false;
    ++count;
    return true;
}

void ReleaseSurface(void* token) {
    Release(token);
}

bool SurfaceBridgeReady() {
    std::lock_guard lock(surfaceMutex);
    return surfaceBridgeRegistered;
}

guest::BuiltinLibrary NativeWindowLibrary() {
    {
        std::lock_guard lock(surfaceMutex);
        surfaceBridgeRegistered = true;
    }
    return {"libandroid.so", {
        {"ANativeWindow_fromSurface", reinterpret_cast<void*>(&FromSurface)},
        {"ANativeWindow_acquire", reinterpret_cast<void*>(&Acquire)},
        {"ANativeWindow_release", reinterpret_cast<void*>(&Release)},
        {"ANativeWindow_getWidth", reinterpret_cast<void*>(&Dimension<true>)},
        {"ANativeWindow_getHeight", reinterpret_cast<void*>(&Dimension<false>)},
        {"ANativeWindow_getFormat", reinterpret_cast<void*>(&GetFormat)},
        {"ANativeWindow_setBuffersGeometry", reinterpret_cast<void*>(&SetBuffersGeometry)},
        {"ANativeWindow_lock", reinterpret_cast<void*>(&LockWindow)},
        {"ANativeWindow_unlockAndPost", reinterpret_cast<void*>(&UnlockAndPostWindow)},
    }, ""};
}

} // namespace winapk::bridge
