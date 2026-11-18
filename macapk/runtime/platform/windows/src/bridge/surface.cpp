// Android native window identities, independent of graphics drivers.
#include "surface.h"
#include "../../../../guest/jni/bridge.h"

#include <cerrno>
#include <cstdint>
#include <limits>
#include <map>
#include <mutex>
#include <new>

namespace winapk::bridge {
namespace {

/// Stores the one currently attached Surface and its token, if held by guest code.
struct Attachment {
    HWND window = nullptr;
    jobject surface = nullptr;  ///< borrowed global reference, withdrawn before its owner drops it
    jfieldID released = nullptr; ///< Surface.mReleased, not a virtual Java call
    int width = 0, height = 0;
    uintptr_t token = 0;
};

std::mutex surfaceMutex;
Attachment attached;
bool surfaceBridgeRegistered = false;
std::map<uintptr_t, unsigned> references; // includes retired tokens until the last release
uintptr_t nextToken = 1;
constexpr size_t kMaxTokens = 1024;

/// Converts only the actual attached holder Surface, acquiring a guest reference on success.
/// ABI: guest JNIEnv proxy and jobject are distinct from the opaque native window result.
void* FromSurface(void* guestEnv, jobject surface) {
    JNIEnv* env = guest::HostEnvironment(guestEnv);
    if (!env || !surface || env->ExceptionCheck()) return nullptr;
    std::lock_guard lock(surfaceMutex);
    if (!attached.surface || !env->IsSameObject(surface, attached.surface) ||
        env->GetBooleanField(surface, attached.released)) return nullptr;
    if (attached.token) {
        auto& count = references.find(attached.token)->second;
        if (count == std::numeric_limits<unsigned>::max()) return nullptr;
        ++count;
    } else {
        if (references.size() >= kMaxTokens || !nextToken) return nullptr;
        try {
            references.emplace(nextToken, 1);
        } catch (const std::bad_alloc&) {
            return nullptr;
        }
        attached.token = nextToken++;
    }
    return reinterpret_cast<void*>(attached.token);
}

/// Retains an existing reference, including a detached one; never resurrects a spent token.
void Acquire(void* window) {
    std::lock_guard lock(surfaceMutex);
    auto it = references.find(reinterpret_cast<uintptr_t>(window));
    if (it != references.end() && it->second != std::numeric_limits<unsigned>::max()) ++it->second;
}

/// Drops one reference; detach and Java release do not implicitly call this guest API.
void Release(void* window) {
    std::lock_guard lock(surfaceMutex);
    auto it = references.find(reinterpret_cast<uintptr_t>(window));
    if (it != references.end() && --it->second == 0) {
        if (attached.token == it->first) attached.token = 0;
        references.erase(it);
    }
}

/// Returns one client dimension from the currently attached identity or -EINVAL.
template<bool Width> int32_t Dimension(void* window) {
    std::lock_guard lock(surfaceMutex);
    if (!window || reinterpret_cast<uintptr_t>(window) != attached.token) return -EINVAL;
    return Width ? attached.width : attached.height;
}

} // namespace

void PublishSurface(HWND window, jobject surface, jfieldID released, int width, int height) {
    std::lock_guard lock(surfaceMutex);
    surfaceBridgeRegistered = true;
    if (attached.window != window || attached.surface != surface) attached.token = 0;
    attached = {window, surface, released, width, height, attached.token};
}

void ResizeSurface(HWND window, int width, int height) {
    std::lock_guard lock(surfaceMutex);
    if (attached.window != window) return;
    attached.width = width;
    attached.height = height;
}

void RetireSurface(HWND window) {
    std::lock_guard lock(surfaceMutex);
    if (!window || attached.window == window) attached = {};
}

bool ResolveSurface(void* token, HWND& window, int& width, int& height) {
    std::lock_guard lock(surfaceMutex);
    if (!token || reinterpret_cast<uintptr_t>(token) != attached.token) return false;
    window = attached.window;
    width = attached.width;
    height = attached.height;
    return true;
}

bool RetainSurface(void* token) {
    std::lock_guard lock(surfaceMutex);
    const auto id = reinterpret_cast<uintptr_t>(token);
    if (!id || id != attached.token) return false;
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
    }, ""};
}

} // namespace winapk::bridge
