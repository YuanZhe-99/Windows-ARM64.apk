// ndk_platform.cpp -- Profile N2 Android NDK platform libraries (`libjnigraphics.so`,
// `libandroid.so` `ALooper`/`AChoreographer`/`AAssetManager`, `libOpenSLES.so`, `libaaudio.so`).
#include "ndk_platform.h"
#include "bridge.h"
#include "../../../../guest/bionic/bionic_n2.h"
#include "../../../../guest/bionic/threads.h"
#include "../../../../guest/jni/bridge.h"

#include <windows.h>
#include <audioclient.h>
#include <mmdeviceapi.h>
#include <zlib.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstring>
#include <deque>
#include <fstream>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace winapk::bridge {
namespace {

uint64_t MonoNowNs() {
    ULONGLONG ticks = 0;
    QueryUnbiasedInterruptTimePrecise(&ticks);
    return static_cast<uint64_t>(ticks) * 100ull;
}

uint64_t RefreshPeriodNs() {
    DEVMODEW dm{};
    dm.dmSize = sizeof(dm);
    if (EnumDisplaySettingsW(nullptr, ENUM_CURRENT_SETTINGS, &dm) && dm.dmDisplayFrequency >= 30 && dm.dmDisplayFrequency <= 360) {
        return 1000000000ull / dm.dmDisplayFrequency;
    }
    return 16666667ull; // 60 Hz default
}

// =============================================================================
// 1. `libjnigraphics.so` (`AndroidBitmap_*`)
// =============================================================================

struct GuestAndroidBitmapInfo {
    uint32_t width;
    uint32_t height;
    uint32_t stride;
    int32_t format; // RGBA_8888=1, RGB_565=4, RGBA_4444=7, A_8=8
    uint32_t flags;
};
static_assert(sizeof(GuestAndroidBitmapInfo) == 20);

struct LockedBitmap {
    jobject globalRef = nullptr;
    int width = 0, height = 0;
    int32_t format = 1;
    bool mutableBmp = false;
    std::vector<uint8_t> pixels;
};

std::mutex g_bmpMu;
std::vector<std::unique_ptr<LockedBitmap>> g_lockedBitmaps;

bool InspectJavaBitmap(JNIEnv* env, jobject bmp, GuestAndroidBitmapInfo& out, bool& mutableOut) {
    if (!env || !bmp || env->ExceptionCheck()) return false;
    jclass cls = env->GetObjectClass(bmp);
    if (!cls) { env->ExceptionClear(); return false; }

    jmethodID isRecMid = env->GetMethodID(cls, "isRecycled", "()Z");
    jmethodID isMutMid = env->GetMethodID(cls, "isMutable", "()Z");
    jmethodID getWMid = env->GetMethodID(cls, "getWidth", "()I");
    jmethodID getHMid = env->GetMethodID(cls, "getHeight", "()I");
    jmethodID getCfgMid = env->GetMethodID(cls, "getConfig", "()Landroid/graphics/Bitmap$Config;");

    if (!isRecMid || !getWMid || !getHMid || !getCfgMid) {
        env->ExceptionClear();
        env->DeleteLocalRef(cls);
        return false;
    }
    if (env->CallBooleanMethod(bmp, isRecMid) == JNI_TRUE) {
        env->DeleteLocalRef(cls);
        return false;
    }
    mutableOut = isMutMid && env->CallBooleanMethod(bmp, isMutMid) == JNI_TRUE;
    const jint w = env->CallIntMethod(bmp, getWMid);
    const jint h = env->CallIntMethod(bmp, getHMid);
    jobject cfgObj = env->CallObjectMethod(bmp, getCfgMid);
    int ordinal = 3; // ARGB_8888 default
    if (cfgObj) {
        jclass cfgCls = env->GetObjectClass(cfgObj);
        jmethodID ordMid = cfgCls ? env->GetMethodID(cfgCls, "ordinal", "()I") : nullptr;
        if (ordMid) ordinal = env->CallIntMethod(cfgObj, ordMid);
        if (cfgCls) env->DeleteLocalRef(cfgCls);
        env->DeleteLocalRef(cfgObj);
    }
    env->ExceptionClear();
    env->DeleteLocalRef(cls);
    if (w <= 0 || h <= 0) return false;

    // Bitmap.Config ordinals: ALPHA_8=0, RGB_565=1, ARGB_4444=2, ARGB_8888=3
    out.width = static_cast<uint32_t>(w);
    out.height = static_cast<uint32_t>(h);
    out.flags = 0;
    if (ordinal == 0) {
        out.format = 8; // ANDROID_BITMAP_FORMAT_A_8
        out.stride = static_cast<uint32_t>(w);
    } else if (ordinal == 1) {
        out.format = 4; // ANDROID_BITMAP_FORMAT_RGB_565
        out.stride = static_cast<uint32_t>(w) * 2u;
    } else {
        out.format = 1; // ANDROID_BITMAP_FORMAT_RGBA_8888
        out.stride = static_cast<uint32_t>(w) * 4u;
    }
    return true;
}

int JniBitmapGetInfo(void* guestEnv, jobject jbitmap, GuestAndroidBitmapInfo* info) {
    JNIEnv* env = guest::HostEnvironment(guestEnv);
    GuestAndroidBitmapInfo tmp{};
    bool mut = false;
    if (!InspectJavaBitmap(env, jbitmap, tmp, mut)) return -1; // ANDROID_BITMAP_RESULT_BAD_PARAMETER
    if (info) *info = tmp;
    return 0; // ANDROID_BITMAP_RESULT_SUCCESS
}

int JniBitmapLockPixels(void* guestEnv, jobject jbitmap, void** addrPtr) {
    if (!addrPtr) return -1;
    *addrPtr = nullptr;
    JNIEnv* env = guest::HostEnvironment(guestEnv);
    GuestAndroidBitmapInfo info{};
    bool mut = false;
    if (!InspectJavaBitmap(env, jbitmap, info, mut)) return -1;

    const int w = static_cast<int>(info.width);
    const int h = static_cast<int>(info.height);
    const jsize totalPx = static_cast<jsize>(w * h);

    jclass cls = env->GetObjectClass(jbitmap);
    jmethodID getPxMid = cls ? env->GetMethodID(cls, "getPixels", "([IIIIIII)V") : nullptr;
    jintArray argbArr = getPxMid ? env->NewIntArray(totalPx) : nullptr;
    if (!argbArr) {
        if (cls) env->DeleteLocalRef(cls);
        env->ExceptionClear();
        return -3; // ANDROID_BITMAP_RESULT_ALLOCATION_FAILED
    }
    env->CallVoidMethod(jbitmap, getPxMid, argbArr, 0, w, 0, 0, w, h);
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        env->DeleteLocalRef(argbArr);
        env->DeleteLocalRef(cls);
        return -2;
    }

    std::vector<jint> argb(static_cast<size_t>(totalPx));
    env->GetIntArrayRegion(argbArr, 0, totalPx, argb.data());
    env->DeleteLocalRef(argbArr);
    env->DeleteLocalRef(cls);

    auto entry = std::make_unique<LockedBitmap>();
    entry->globalRef = env->NewGlobalRef(jbitmap);
    entry->width = w;
    entry->height = h;
    entry->format = info.format;
    entry->mutableBmp = mut;
    entry->pixels.resize(static_cast<size_t>(info.stride) * static_cast<size_t>(h));

    if (info.format == 8) {
        for (size_t i = 0; i < argb.size(); ++i) {
            entry->pixels[i] = static_cast<uint8_t>((static_cast<uint32_t>(argb[i]) >> 24) & 0xffu);
        }
    } else if (info.format == 4) {
        auto* dst16 = reinterpret_cast<uint16_t*>(entry->pixels.data());
        for (size_t i = 0; i < argb.size(); ++i) {
            const uint32_t c = static_cast<uint32_t>(argb[i]);
            const uint16_t r = static_cast<uint16_t>(((c >> 16) & 0xffu) >> 3);
            const uint16_t g = static_cast<uint16_t>(((c >> 8) & 0xffu) >> 2);
            const uint16_t b = static_cast<uint16_t>((c & 0xffu) >> 3);
            dst16[i] = static_cast<uint16_t>((r << 11) | (g << 5) | b);
        }
    } else {
        for (size_t i = 0; i < argb.size(); ++i) {
            const uint32_t c = static_cast<uint32_t>(argb[i]);
            entry->pixels[i * 4 + 0] = static_cast<uint8_t>((c >> 16) & 0xffu); // R
            entry->pixels[i * 4 + 1] = static_cast<uint8_t>((c >> 8) & 0xffu);  // G
            entry->pixels[i * 4 + 2] = static_cast<uint8_t>(c & 0xffu);         // B
            entry->pixels[i * 4 + 3] = static_cast<uint8_t>((c >> 24) & 0xffu); // A
        }
    }

    std::lock_guard lk(g_bmpMu);
    for (const auto& existing : g_lockedBitmaps) {
        if (env->IsSameObject(existing->globalRef, jbitmap)) {
            env->DeleteGlobalRef(entry->globalRef);
            *addrPtr = existing->pixels.data();
            return 0;
        }
    }
    *addrPtr = entry->pixels.data();
    g_lockedBitmaps.push_back(std::move(entry));
    return 0;
}

int JniBitmapUnlockPixels(void* guestEnv, jobject jbitmap) {
    JNIEnv* env = guest::HostEnvironment(guestEnv);
    if (!env || !jbitmap) return -1;
    std::unique_ptr<LockedBitmap> locked;
    {
        std::lock_guard lk(g_bmpMu);
        for (auto it = g_lockedBitmaps.begin(); it != g_lockedBitmaps.end(); ++it) {
            if (env->IsSameObject((*it)->globalRef, jbitmap)) {
                locked = std::move(*it);
                g_lockedBitmaps.erase(it);
                break;
            }
        }
    }
    if (!locked) return -2;

    if (locked->mutableBmp) {
        const size_t n = static_cast<size_t>(locked->width) * static_cast<size_t>(locked->height);
        std::vector<jint> argb(n);
        if (locked->format == 8) {
            for (size_t i = 0; i < n; ++i) argb[i] = static_cast<jint>(static_cast<uint32_t>(locked->pixels[i]) << 24);
        } else if (locked->format == 4) {
            const auto* src16 = reinterpret_cast<const uint16_t*>(locked->pixels.data());
            for (size_t i = 0; i < n; ++i) {
                const uint16_t p = src16[i];
                const uint32_t r5 = (p >> 11) & 0x1fu, g6 = (p >> 5) & 0x3fu, b5 = p & 0x1fu;
                const uint32_t r = (r5 << 3) | (r5 >> 2);
                const uint32_t g = (g6 << 2) | (g6 >> 4);
                const uint32_t b = (b5 << 3) | (b5 >> 2);
                argb[i] = static_cast<jint>(0xff000000u | (r << 16) | (g << 8) | b);
            }
        } else {
            for (size_t i = 0; i < n; ++i) {
                const uint32_t r = locked->pixels[i * 4 + 0];
                const uint32_t g = locked->pixels[i * 4 + 1];
                const uint32_t b = locked->pixels[i * 4 + 2];
                const uint32_t a = locked->pixels[i * 4 + 3];
                argb[i] = static_cast<jint>((a << 24) | (r << 16) | (g << 8) | b);
            }
        }
        jclass cls = env->GetObjectClass(jbitmap);
        jmethodID setPxMid = cls ? env->GetMethodID(cls, "setPixels", "([IIIIIII)V") : nullptr;
        jintArray arr = setPxMid ? env->NewIntArray(static_cast<jsize>(n)) : nullptr;
        if (arr) {
            env->SetIntArrayRegion(arr, 0, static_cast<jsize>(n), argb.data());
            env->CallVoidMethod(jbitmap, setPxMid, arr, 0, locked->width, 0, 0, locked->width, locked->height);
            env->ExceptionClear();
            env->DeleteLocalRef(arr);
        }
        if (cls) env->DeleteLocalRef(cls);
    }
    env->DeleteGlobalRef(locked->globalRef);
    return 0;
}

// =============================================================================
// 2. `ALooper` & `AChoreographer` (`libandroid.so`)
// =============================================================================

using GuestLooperCb = int (*)(int fd, int events, void* data);
using GuestChoreoCb32 = void (*)(int64_t frameTimeNanos, void* data); // LP64 `long` is 64-bit!
using GuestChoreoCb64 = void (*)(int64_t frameTimeNanos, void* data);
using GuestRefreshCb = void (*)(int64_t vsyncPeriodNanos, void* data);

__declspec(guard(nocf)) int CallLooperFdCb(GuestLooperCb cb, int fd, int events, void* data) {
    return cb(fd, events, data);
}
__declspec(guard(nocf)) void CallChoreoCb(GuestChoreoCb64 cb, int64_t ns, void* data) {
    cb(ns, data);
}
__declspec(guard(nocf)) void CallRefreshCb(GuestRefreshCb cb, int64_t ns, void* data) {
    cb(ns, data);
}

struct LooperFdReg {
    int fd = -1;
    int ident = -2;
    int events = 0;
    GuestLooperCb callback = nullptr;
    void* data = nullptr;
};

struct ChoreoPending {
    uint64_t dueMonoNs = 0;
    GuestChoreoCb64 callback = nullptr;
    void* data = nullptr;
};

struct GuestLooper {
    std::atomic<int> refs{1};
    bool allowNonCallbacks = false;
    std::atomic<bool> vsyncRequested{false};
    std::atomic<int64_t> pendingVsyncNano{-1};
    std::mutex mu;
    std::condition_variable cv;
    bool woken = false;
    uint64_t n2ListenerId = 0;
    std::map<int, LooperFdReg> fds;
    std::vector<ChoreoPending> frameCallbacks;

    GuestLooper();
    ~GuestLooper();
};

std::mutex g_allLoopersMu;
std::vector<GuestLooper*> g_allLoopers;

GuestLooper::GuestLooper() {
    n2ListenerId = guest::RegisterN2WakeListener([this] {
        {
            std::lock_guard lk(mu);
            woken = true;
        }
        cv.notify_all();
    });
    std::lock_guard lk(g_allLoopersMu);
    g_allLoopers.push_back(this);
}

GuestLooper::~GuestLooper() {
    {
        std::lock_guard lk(g_allLoopersMu);
        std::erase(g_allLoopers, this);
    }
    if (n2ListenerId) guest::UnregisterN2WakeListener(n2ListenerId);
}

thread_local GuestLooper* t_looper = nullptr;

void* NdkLooperForThread() { return t_looper; }

void NdkLooperAcquire(void* ptr) {
    if (ptr) static_cast<GuestLooper*>(ptr)->refs.fetch_add(1, std::memory_order_relaxed);
}

void NdkLooperRelease(void* ptr) {
    auto* l = static_cast<GuestLooper*>(ptr);
    if (l && l->refs.fetch_sub(1, std::memory_order_acq_rel) == 1) {
        if (t_looper == l) t_looper = nullptr;
        delete l;
    }
}

void* NdkLooperPrepare(int opts) {
    if (!t_looper) {
        t_looper = new GuestLooper();
        t_looper->allowNonCallbacks = (opts & 1 /* ALOOPER_PREPARE_ALLOW_NON_CALLBACKS */) != 0;
    }
    return t_looper;
}

void NdkLooperWake(void* ptr) {
    auto* l = static_cast<GuestLooper*>(ptr);
    if (!l) return;
    {
        std::lock_guard lk(l->mu);
        l->woken = true;
    }
    l->cv.notify_all();
}

int NdkLooperAddFd(void* ptr, int fd, int ident, int events, GuestLooperCb cb, void* data) {
    auto* l = static_cast<GuestLooper*>(ptr);
    if (!l || fd < 0) return -1;
    if (!cb && !l->allowNonCallbacks) return -1;
    {
        std::lock_guard lk(l->mu);
        l->fds[fd] = {fd, cb ? -2 /* ALOOPER_POLL_CALLBACK */ : ident, events, cb, data};
    }
    NdkLooperWake(l);
    return 1;
}

int NdkLooperRemoveFd(void* ptr, int fd) {
    auto* l = static_cast<GuestLooper*>(ptr);
    if (!l) return -1;
    std::lock_guard lk(l->mu);
    return l->fds.erase(fd) > 0 ? 1 : 0;
}

int NdkLooperPollOnce(int timeoutMillis, int* outFd, int* outEvents, void** outData) {
    if (outFd) *outFd = -1;
    if (outEvents) *outEvents = 0;
    if (outData) *outData = nullptr;

    GuestLooper* l = t_looper;
    if (!l) return -4; // ALOOPER_POLL_ERROR

    const uint64_t startNs = MonoNowNs();
    const uint64_t callDeadlineNs = timeoutMillis < 0 ? UINT64_MAX
        : startNs + static_cast<uint64_t>(timeoutMillis) * 1000000ull;

    for (;;) {
        const uint64_t nowNs = MonoNowNs();
        uint64_t nextWakeNs = callDeadlineNs;
        bool hadWake = false;
        std::vector<ChoreoPending> dueChoreo;
        std::vector<LooperFdReg> fdSnapshot;

        {
            std::lock_guard lk(l->mu);
            if (l->woken) {
                hadWake = true;
                l->woken = false;
            }
            for (auto it = l->frameCallbacks.begin(); it != l->frameCallbacks.end();) {
                if (nowNs >= it->dueMonoNs) {
                    dueChoreo.push_back(*it);
                    it = l->frameCallbacks.erase(it);
                } else {
                    if (it->dueMonoNs < nextWakeNs) nextWakeNs = it->dueMonoNs;
                    ++it;
                }
            }
            for (const auto& [fd, reg] : l->fds) fdSnapshot.push_back(reg);
        }

        if (!dueChoreo.empty()) {
            const int64_t frameTime = static_cast<int64_t>(nowNs);
            for (const auto& item : dueChoreo) {
                if (item.callback) CallChoreoCb(item.callback, frameTime, item.data);
            }
            return -2; // ALOOPER_POLL_CALLBACK
        }

        for (const auto& reg : fdSnapshot) {
            const uint32_t ready = guest::QueryN2FdReady(reg.fd, &nextWakeNs);
            const uint32_t matched = ready & (static_cast<uint32_t>(reg.events) | 4u | 8u);
            if (matched != 0) {
                if (reg.callback) {
                    const int keep = CallLooperFdCb(reg.callback, reg.fd, static_cast<int>(matched), reg.data);
                    if (keep == 0) NdkLooperRemoveFd(l, reg.fd);
                    return -2; // ALOOPER_POLL_CALLBACK
                }
                if (outFd) *outFd = reg.fd;
                if (outEvents) *outEvents = static_cast<int>(matched);
                if (outData) *outData = reg.data;
                return reg.ident;
            }
        }

        if (hadWake) return -1; // ALOOPER_POLL_WAKE
        if (timeoutMillis == 0 || nowNs >= callDeadlineNs) return -3; // ALOOPER_POLL_TIMEOUT

        std::unique_lock lk(l->mu);
        if (l->woken) continue;
        const uint64_t curNs = MonoNowNs();
        if (nextWakeNs == UINT64_MAX) {
            l->cv.wait_for(lk, std::chrono::milliseconds(500));
        } else if (nextWakeNs > curNs) {
            l->cv.wait_for(lk, std::chrono::nanoseconds(nextWakeNs - curNs));
        }
    }
}

int NdkLooperPollAll(int timeoutMillis, int* outFd, int* outEvents, void** outData) {
    return NdkLooperPollOnce(timeoutMillis, outFd, outEvents, outData);
}

struct GuestChoreographer {
    GuestLooper* looper = nullptr;
};
thread_local std::unique_ptr<GuestChoreographer> t_choreo;

void* NdkChoreoGetInstance() {
    GuestLooper* l = static_cast<GuestLooper*>(NdkLooperPrepare(0));
    if (!t_choreo) t_choreo = std::make_unique<GuestChoreographer>();
    t_choreo->looper = l;
    return t_choreo.get();
}

void NdkChoreoPostDelayed64(void* ptr, GuestChoreoCb64 cb, void* data, uint32_t delayMillis) {
    auto* ch = static_cast<GuestChoreographer*>(ptr);
    if (!ch || !ch->looper || !cb) return;
    const uint64_t period = RefreshPeriodNs();
    const uint64_t base = MonoNowNs() + static_cast<uint64_t>(delayMillis) * 1000000ull;
    const uint64_t nextVsync = ((base / period) + 1ull) * period;
    {
        std::lock_guard lk(ch->looper->mu);
        ch->looper->frameCallbacks.push_back({nextVsync, cb, data});
    }
    ch->looper->cv.notify_all();
}

void NdkChoreoPost64(void* ptr, GuestChoreoCb64 cb, void* data) {
    NdkChoreoPostDelayed64(ptr, cb, data, 0);
}
void NdkChoreoPostDelayed32(void* ptr, GuestChoreoCb32 cb, void* data, int64_t delayMillis) {
    NdkChoreoPostDelayed64(ptr, cb, data, delayMillis > 0 ? static_cast<uint32_t>(delayMillis) : 0);
}
void NdkChoreoPost32(void* ptr, GuestChoreoCb32 cb, void* data) {
    NdkChoreoPostDelayed64(ptr, cb, data, 0);
}
void NdkChoreoRegRefresh(void*, GuestRefreshCb cb, void* data) {
    if (cb) CallRefreshCb(cb, static_cast<int64_t>(RefreshPeriodNs()), data);
}
void NdkChoreoUnregRefresh(void*, GuestRefreshCb, void*) {}

// =============================================================================
// 3. `AAssetManager_*` (`libandroid.so`, direct APK `assets/` ZIP reader)
// =============================================================================

struct ApkAssetMeta {
    uint16_t method = 0;
    uint64_t compSize = 0;
    uint64_t uncompSize = 0;
    uint64_t dataOffset = 0; // exact byte offset past local header inside APK
};

struct ApkAssetArchive {
    std::once_flag once;
    std::wstring apkPath;
    std::map<std::string, ApkAssetMeta> assets;
};
ApkAssetArchive g_apkArchive;

void EnsureApkArchiveIndexed() {
    std::call_once(g_apkArchive.once, [] {
        wchar_t buf[32768]{};
        const DWORD len = GetEnvironmentVariableW(L"WINAPK_APK_PATH", buf, 32768);
        if (!len || len >= 32768) return;
        g_apkArchive.apkPath.assign(buf, len);

        std::ifstream f(g_apkArchive.apkPath, std::ios::binary | std::ios::ate);
        if (!f) return;
        const uint64_t fileSize = static_cast<uint64_t>(f.tellg());
        if (fileSize < 22) return;

        const uint64_t tailSize = std::min<uint64_t>(fileSize, 65557);
        std::vector<uint8_t> tail(static_cast<size_t>(tailSize));
        f.seekg(static_cast<std::streamoff>(fileSize - tailSize));
        f.read(reinterpret_cast<char*>(tail.data()), static_cast<std::streamsize>(tailSize));

        int64_t eocd = -1;
        for (int64_t i = static_cast<int64_t>(tailSize) - 22; i >= 0; --i) {
            if (tail[i] == 0x50 && tail[i + 1] == 0x4b && tail[i + 2] == 0x05 && tail[i + 3] == 0x06) {
                eocd = i;
                break;
            }
        }
        if (eocd < 0) return;

        uint32_t cdSize = 0, cdOff = 0;
        std::memcpy(&cdSize, &tail[eocd + 12], 4);
        std::memcpy(&cdOff, &tail[eocd + 16], 4);
        if (uint64_t(cdOff) + uint64_t(cdSize) > fileSize) return;

        std::vector<uint8_t> cd(cdSize);
        f.seekg(static_cast<std::streamoff>(cdOff));
        f.read(reinterpret_cast<char*>(cd.data()), static_cast<std::streamsize>(cdSize));

        size_t p = 0;
        while (p + 46 <= cd.size()) {
            if (cd[p] != 0x50 || cd[p + 1] != 0x4b || cd[p + 2] != 0x01 || cd[p + 3] != 0x02) break;
            uint16_t method = 0, nameLen = 0, extraLen = 0, commentLen = 0;
            uint32_t compSz = 0, uncompSz = 0, localOff = 0;
            std::memcpy(&method, &cd[p + 10], 2);
            std::memcpy(&compSz, &cd[p + 20], 4);
            std::memcpy(&uncompSz, &cd[p + 24], 4);
            std::memcpy(&nameLen, &cd[p + 28], 2);
            std::memcpy(&extraLen, &cd[p + 30], 2);
            std::memcpy(&commentLen, &cd[p + 32], 2);
            std::memcpy(&localOff, &cd[p + 42], 4);
            if (p + 46 + nameLen > cd.size()) break;

            std::string entryName(reinterpret_cast<const char*>(&cd[p + 46]), nameLen);
            p += 46 + nameLen + extraLen + commentLen;
            if (!entryName.starts_with("assets/") || entryName.ends_with('/')) continue;

            uint8_t lh[30]{};
            f.seekg(static_cast<std::streamoff>(localOff));
            if (!f.read(reinterpret_cast<char*>(lh), 30)) continue;
            uint16_t lhName = 0, lhExtra = 0;
            std::memcpy(&lhName, &lh[26], 2);
            std::memcpy(&lhExtra, &lh[28], 2);

            const std::string rel = entryName.substr(7); // strip "assets/"
            g_apkArchive.assets[rel] = {method, compSz, uncompSz, uint64_t(localOff) + 30ull + lhName + lhExtra};
        }
    });
}

struct GuestAsset {
    ApkAssetMeta meta{};
    std::vector<uint8_t> bytes;
    size_t pos = 0;
};

struct GuestAssetDir {
    std::vector<std::string> files;
    size_t idx = 0;
};

int g_assetMgrSingleton = 1;

void* NdkAssetMgrFromJava(void*, jobject) {
    EnsureApkArchiveIndexed();
    return &g_assetMgrSingleton;
}

GuestAsset* NdkAssetOpen(void*, const char* filename, int) {
    if (!filename) return nullptr;
    EnsureApkArchiveIndexed();
    while (*filename == '/') ++filename;
    auto it = g_apkArchive.assets.find(filename);
    if (it == g_apkArchive.assets.end()) return nullptr;

    const ApkAssetMeta& m = it->second;
    std::ifstream f(g_apkArchive.apkPath, std::ios::binary);
    if (!f) return nullptr;
    f.seekg(static_cast<std::streamoff>(m.dataOffset));

    auto asset = std::make_unique<GuestAsset>();
    asset->meta = m;
    if (m.method == 0) {
        asset->bytes.resize(static_cast<size_t>(m.uncompSize));
        if (m.uncompSize > 0 && !f.read(reinterpret_cast<char*>(asset->bytes.data()), static_cast<std::streamsize>(m.uncompSize))) {
            return nullptr;
        }
    } else if (m.method == 8) {
        std::vector<uint8_t> comp(static_cast<size_t>(m.compSize));
        if (m.compSize > 0 && !f.read(reinterpret_cast<char*>(comp.data()), static_cast<std::streamsize>(m.compSize))) {
            return nullptr;
        }
        asset->bytes.resize(static_cast<size_t>(m.uncompSize));
        z_stream zs{};
        zs.next_in = comp.data();
        zs.avail_in = static_cast<uInt>(comp.size());
        zs.next_out = asset->bytes.data();
        zs.avail_out = static_cast<uInt>(asset->bytes.size());
        if (inflateInit2(&zs, -MAX_WBITS) != Z_OK) return nullptr;
        const int rc = inflate(&zs, Z_FINISH);
        inflateEnd(&zs);
        if (rc != Z_STREAM_END && m.uncompSize != 0) return nullptr;
    } else {
        return nullptr;
    }
    return asset.release();
}

GuestAssetDir* NdkAssetOpenDir(void*, const char* dirName) {
    EnsureApkArchiveIndexed();
    std::string prefix = dirName ? dirName : "";
    while (!prefix.empty() && prefix.front() == '/') prefix.erase(prefix.begin());
    while (!prefix.empty() && prefix.back() == '/') prefix.pop_back();
    if (!prefix.empty()) prefix += "/";

    auto dir = std::make_unique<GuestAssetDir>();
    for (const auto& [rel, meta] : g_apkArchive.assets) {
        (void)meta;
        if (!rel.starts_with(prefix)) continue;
        const std::string rest = rel.substr(prefix.size());
        if (!rest.empty() && rest.find('/') == std::string::npos) {
            dir->files.push_back(rest);
        }
    }
    return dir.release();
}

const char* NdkAssetDirNext(GuestAssetDir* d) {
    if (!d || d->idx >= d->files.size()) return nullptr;
    return d->files[d->idx++].c_str();
}
void NdkAssetDirRewind(GuestAssetDir* d) { if (d) d->idx = 0; }
void NdkAssetDirClose(GuestAssetDir* d) { delete d; }

int NdkAssetRead(GuestAsset* a, void* buf, uint64_t count) {
    if (!a || (!buf && count > 0)) return -1;
    const size_t rem = a->pos < a->bytes.size() ? a->bytes.size() - a->pos : 0;
    const size_t n = std::min<size_t>(rem, static_cast<size_t>(count));
    if (n > 0) {
        std::memcpy(buf, a->bytes.data() + a->pos, n);
        a->pos += n;
    }
    return static_cast<int>(n);
}

int64_t NdkAssetSeek64(GuestAsset* a, int64_t offset, int whence) {
    if (!a) return -1;
    const int64_t base = whence == 0 ? 0 : (whence == 1 ? static_cast<int64_t>(a->pos) : static_cast<int64_t>(a->bytes.size()));
    const int64_t next = base + offset;
    if (next < 0 || static_cast<size_t>(next) > a->bytes.size()) return -1;
    a->pos = static_cast<size_t>(next);
    return next;
}
int64_t NdkAssetSeek(GuestAsset* a, int64_t offset, int whence) { return NdkAssetSeek64(a, offset, whence); }
void NdkAssetClose(GuestAsset* a) { delete a; }
const void* NdkAssetGetBuffer(GuestAsset* a) { return a ? a->bytes.data() : nullptr; }
int64_t NdkAssetGetLength64(GuestAsset* a) { return a ? static_cast<int64_t>(a->bytes.size()) : 0; }
int64_t NdkAssetGetLength(GuestAsset* a) { return NdkAssetGetLength64(a); }
int64_t NdkAssetGetRemaining64(GuestAsset* a) { return a ? static_cast<int64_t>(a->bytes.size() - a->pos) : 0; }
int64_t NdkAssetGetRemaining(GuestAsset* a) { return NdkAssetGetRemaining64(a); }
int NdkAssetIsAllocated(GuestAsset* a) { return a ? 1 : 0; }
int NdkAssetOpenFd64(GuestAsset* a, int64_t* outStart, int64_t* outLength) {
    if (!a || a->meta.method != 0) return -1; // Compressed assets return < 0 per NDK contract!
    if (outStart) *outStart = static_cast<int64_t>(a->meta.dataOffset);
    if (outLength) *outLength = static_cast<int64_t>(a->meta.uncompSize);
    const std::string utf8Apk = [&] {
        const auto& w = g_apkArchive.apkPath;
        int n = WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()), nullptr, 0, nullptr, nullptr);
        std::string s(n, '\0');
        WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()), s.data(), n, nullptr, nullptr);
        return s;
    }();
    // Open via N2 `open` if present in guest namespace:
    if (auto* mods = guest::GuestActiveModules()) {
        if (auto openFn = reinterpret_cast<int (*)(const char*, int)>(mods->FindDefault("__open_2"))) {
            return openFn(utf8Apk.c_str(), 0);
        }
    }
    return -1;
}
int NdkAssetOpenFd(GuestAsset* a, int64_t* outStart, int64_t* outLength) { return NdkAssetOpenFd64(a, outStart, outLength); }

// =============================================================================
// 4. `libOpenSLES.so` & `libaaudio.so` (WASAPI PCM Buffer-Queue Output)
// =============================================================================

struct SLInterfaceID_ {
    uint32_t time_low;
    uint16_t time_mid;
    uint16_t time_hi_and_version;
    uint16_t clock_seq;
    uint8_t node[6];
};
using SLInterfaceID = const SLInterfaceID_*;

constexpr SLInterfaceID_ kGuidEngine   = {0x8d97c260, 0xddd4, 0x11db, 0x958f, {0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x1b}};
constexpr SLInterfaceID_ kGuidPlay     = {0xef0bd9c0, 0xddd7, 0x11db, 0xbf49, {0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x1b}};
constexpr SLInterfaceID_ kGuidBufQueue = {0x2bc9f160, 0xddd6, 0x11db, 0x8d99, {0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x1b}};
constexpr SLInterfaceID_ kGuidAndSimple= {0x198e4940, 0xc5d7, 0x11df, 0xa2a6, {0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x1b}};
constexpr SLInterfaceID_ kGuidAndCfg   = {0x2dfbb6a0, 0x2579, 0x11df, 0x8c85, {0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x1b}};
constexpr SLInterfaceID_ kGuidVolume   = {0x09e8ede0, 0xddde, 0x11db, 0xb4f6, {0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x1b}};

// Exported `STT_OBJECT` variables (`extern const SLInterfaceID SL_IID_*;`):
SLInterfaceID g_iidEngine = &kGuidEngine;
SLInterfaceID g_iidPlay = &kGuidPlay;
SLInterfaceID g_iidBufQueue = &kGuidBufQueue;
SLInterfaceID g_iidAndSimple = &kGuidAndSimple;
SLInterfaceID g_iidAndCfg = &kGuidAndCfg;
SLInterfaceID g_iidVolume = &kGuidVolume;

bool SameGuid(SLInterfaceID a, const SLInterfaceID_& b) {
    return a && std::memcmp(a, &b, sizeof(SLInterfaceID_)) == 0;
}

using SlBufQueueCb = void (*)(const void* callerItf, void* context);
__declspec(guard(nocf)) void CallSlBufQueueCb(SlBufQueueCb cb, const void* callerItf, void* ctx) {
    cb(callerItf, ctx);
}

struct SLDataFormat_PCM_Guest {
    uint32_t formatType;
    uint32_t numChannels;
    uint32_t samplesPerSec; // millihertz!
    uint32_t bitsPerSample;
    uint32_t containerSize;
    uint32_t channelMask;
    uint32_t endianness;
    uint32_t representation; // optional PCM_EX: 3 == FLOAT
};
struct SLDataSource_Guest { void* pLocator; SLDataFormat_PCM_Guest* pFormat; };

enum class SlKind { Engine, OutputMix, AudioPlayer };

struct SlItfSlot {
    const void* vt = nullptr;
    struct SlObject* owner = nullptr;
};

struct SlObject {
    SlItfSlot objItf{};
    SlItfSlot engineItf{};
    SlItfSlot playItf{};
    SlItfSlot bqItf{};
    SlItfSlot cfgItf{};
    SlItfSlot volItf{};
    SlKind kind = SlKind::Engine;

    // AudioPlayer state:
    uint32_t channels = 2;
    uint32_t sampleRate = 48000;
    uint32_t bitsPerSample = 16;
    bool isFloat = false;
    std::atomic<uint32_t> playState{1}; // 1=STOPPED, 2=PAUSED, 3=PLAYING
    std::atomic<bool> stopWorker{false};
    std::mutex qMu;
    std::condition_variable qCv;
    std::deque<std::vector<uint8_t>> queue;
    uint32_t playIndex = 0;
    SlBufQueueCb bqCallback = nullptr;
    void* bqContext = nullptr;
    std::thread worker;

    ~SlObject() {
        stopWorker.store(true);
        qCv.notify_all();
        if (worker.joinable()) worker.join();
    }
};

void AudioPlayerThread(SlObject* p) {
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    IMMDeviceEnumerator* enumerator = nullptr;
    IMMDevice* device = nullptr;
    IAudioClient* client = nullptr;
    IAudioRenderClient* render = nullptr;
    UINT32 bufferFrames = 0;
    const uint32_t bytesPerFrame = std::max<uint32_t>(1, p->channels * (p->bitsPerSample / 8));

    WAVEFORMATEXTENSIBLE wfx{};
    wfx.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
    wfx.Format.nChannels = static_cast<WORD>(p->channels);
    wfx.Format.nSamplesPerSec = p->sampleRate;
    wfx.Format.wBitsPerSample = static_cast<WORD>(p->bitsPerSample);
    wfx.Format.nBlockAlign = static_cast<WORD>(bytesPerFrame);
    wfx.Format.nAvgBytesPerSec = p->sampleRate * bytesPerFrame;
    wfx.Format.cbSize = 22;
    wfx.Samples.wValidBitsPerSample = static_cast<WORD>(p->bitsPerSample);
    wfx.dwChannelMask = p->channels == 1 ? SPEAKER_FRONT_CENTER : (SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT);
    static constexpr GUID kSubPcm   = {0x00000001, 0x0000, 0x0010, {0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}};
    static constexpr GUID kSubFloat = {0x00000003, 0x0000, 0x0010, {0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}};
    wfx.SubFormat = p->isFloat ? kSubFloat : kSubPcm;

    bool hasWasapi = false;
    if (SUCCEEDED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                                   __uuidof(IMMDeviceEnumerator), reinterpret_cast<void**>(&enumerator))) &&
        SUCCEEDED(enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &device)) &&
        SUCCEEDED(device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, reinterpret_cast<void**>(&client))) &&
        SUCCEEDED(client->Initialize(AUDCLNT_SHAREMODE_SHARED,
                                     AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM | AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY,
                                     400000 /* 40ms */, 0, &wfx.Format, nullptr)) &&
        SUCCEEDED(client->GetBufferSize(&bufferFrames)) &&
        SUCCEEDED(client->GetService(__uuidof(IAudioRenderClient), reinterpret_cast<void**>(&render)))) {
        client->Start();
        hasWasapi = true;
    }

    while (!p->stopWorker.load(std::memory_order_relaxed)) {
        std::vector<uint8_t> chunk;
        SlBufQueueCb cb = nullptr;
        void* cbCtx = nullptr;
        {
            std::unique_lock lk(p->qMu);
            p->qCv.wait_for(lk, std::chrono::milliseconds(10), [&] {
                return p->stopWorker.load() || (p->playState.load() == 3 && !p->queue.empty());
            });
            if (p->stopWorker.load()) break;
            if (p->playState.load() != 3 || p->queue.empty()) continue;
            chunk = std::move(p->queue.front());
        }

        const uint32_t framesTotal = static_cast<uint32_t>(chunk.size() / bytesPerFrame);
        if (hasWasapi && framesTotal > 0) {
            uint32_t writtenFrames = 0;
            while (writtenFrames < framesTotal && !p->stopWorker.load()) {
                UINT32 padding = 0;
                if (FAILED(client->GetCurrentPadding(&padding))) break;
                const UINT32 avail = bufferFrames > padding ? bufferFrames - padding : 0;
                if (avail == 0) {
                    Sleep(2);
                    continue;
                }
                const UINT32 step = std::min<UINT32>(avail, framesTotal - writtenFrames);
                BYTE* dst = nullptr;
                if (SUCCEEDED(render->GetBuffer(step, &dst)) && dst) {
                    std::memcpy(dst, chunk.data() + static_cast<size_t>(writtenFrames) * bytesPerFrame,
                                static_cast<size_t>(step) * bytesPerFrame);
                    render->ReleaseBuffer(step, 0);
                }
                writtenFrames += step;
            }
        } else if (framesTotal > 0) {
            const uint64_t durMs = (static_cast<uint64_t>(framesTotal) * 1000ull) / std::max<uint32_t>(1, p->sampleRate);
            Sleep(static_cast<DWORD>(std::clamp<uint64_t>(durMs, 1, 100)));
        }

        {
            std::lock_guard lk(p->qMu);
            if (!p->queue.empty()) p->queue.pop_front();
            ++p->playIndex;
            cb = p->bqCallback;
            cbCtx = p->bqContext;
        }
        if (cb && !p->stopWorker.load()) {
            CallSlBufQueueCb(cb, &p->bqItf, cbCtx);
        }
    }

    if (client) { client->Stop(); client->Release(); }
    if (render) render->Release();
    if (device) device->Release();
    if (enumerator) enumerator->Release();
    CoUninitialize();
}

SlObject* FromItf(void* self) {
    return self ? static_cast<SlItfSlot*>(self)->owner : nullptr;
}

uint32_t ObjRealize(void*, uint32_t) { return 0; }
uint32_t ObjResume(void*, uint32_t) { return 0; }
uint32_t ObjGetState(void*, uint32_t* st) { if (st) *st = 2 /* SL_OBJECT_STATE_REALIZED */; return 0; }
uint32_t ObjGetInterface(void* self, SLInterfaceID iid, void* pItf) {
    if (!self || !iid || !pItf) return 2; // SL_RESULT_PARAMETER_INVALID
    auto* o = FromItf(self);
    auto* out = static_cast<void**>(pItf);
    if (o->kind == SlKind::Engine && SameGuid(iid, kGuidEngine)) { *out = &o->engineItf; return 0; }
    if (o->kind == SlKind::AudioPlayer) {
        if (SameGuid(iid, kGuidPlay)) { *out = &o->playItf; return 0; }
        if (SameGuid(iid, kGuidBufQueue) || SameGuid(iid, kGuidAndSimple)) { *out = &o->bqItf; return 0; }
        if (SameGuid(iid, kGuidAndCfg)) { *out = &o->cfgItf; return 0; }
        if (SameGuid(iid, kGuidVolume)) { *out = &o->volItf; return 0; }
    }
    return 12; // SL_RESULT_FEATURE_UNSUPPORTED
}
uint32_t ObjRegCb(void*, void*, void*) { return 0; }
void ObjAbort(void*) {}
void ObjDestroy(void* self) { delete FromItf(self); }
uint32_t ObjNoop2(void*, int32_t, uint32_t) { return 0; }
uint32_t ObjNoopGet(void*, int32_t* a, uint32_t* b) { if (a) *a = 0; if (b) *b = 0; return 0; }
uint32_t ObjLoss(void*, int16_t, void*, uint32_t) { return 0; }

uint32_t PlaySetState(void* self, uint32_t state) {
    auto* o = FromItf(self);
    o->playState.store(state);
    o->qCv.notify_all();
    return 0;
}
uint32_t PlayGetState(void* self, uint32_t* state) {
    auto* o = FromItf(self);
    if (state) *state = o->playState.load();
    return 0;
}
uint32_t PlayGetMs(void*, uint32_t* ms) { if (ms) *ms = 0; return 0; }
uint32_t PlayNoop(void*, void*, void*) { return 0; }
uint32_t PlayMaskSet(void*, uint32_t) { return 0; }
uint32_t PlayMaskGet(void*, uint32_t* m) { if (m) *m = 0; return 0; }
uint32_t PlayClearPos(void*) { return 0; }

struct SlBufQueueState { uint32_t count; uint32_t index; };
uint32_t BqEnqueue(void* self, const void* pBuffer, uint32_t size) {
    if (!pBuffer || size == 0) return 2;
    auto* o = FromItf(self);
    {
        std::lock_guard lk(o->qMu);
        const auto* b = static_cast<const uint8_t*>(pBuffer);
        o->queue.emplace_back(b, b + size);
    }
    o->qCv.notify_all();
    return 0;
}
uint32_t BqClear(void* self) {
    auto* o = FromItf(self);
    std::lock_guard lk(o->qMu);
    o->queue.clear();
    return 0;
}
uint32_t BqGetState(void* self, SlBufQueueState* st) {
    if (!st) return 2;
    auto* o = FromItf(self);
    std::lock_guard lk(o->qMu);
    st->count = static_cast<uint32_t>(o->queue.size());
    st->index = o->playIndex;
    return 0;
}
uint32_t BqRegCb(void* self, SlBufQueueCb cb, void* ctx) {
    auto* o = FromItf(self);
    std::lock_guard lk(o->qMu);
    o->bqCallback = cb;
    o->bqContext = ctx;
    return 0;
}

uint32_t CfgSet(void*, const char*, const void*, uint32_t) { return 0; }
uint32_t CfgGet(void*, const char*, uint32_t* sz, void* val) {
    if (sz && val && *sz >= 4) *static_cast<uint32_t*>(val) = 0;
    if (sz) *sz = 4;
    return 0;
}
uint32_t CfgProxy(void*, uint32_t, void*) { return 12; }
uint32_t CfgRelProxy(void*, uint32_t) { return 0; }

uint32_t VolNoopSet(void*, int16_t) { return 0; }
uint32_t VolNoopGet(void*, int16_t* v) { if (v) *v = 0; return 0; }
uint32_t VolMuteSet(void*, uint32_t) { return 0; }
uint32_t VolMuteGet(void*, uint32_t* m) { if (m) *m = 0; return 0; }

SlObject* MakeSlObject(SlKind kind);

uint32_t EngUnsupported() { return 12; }
uint32_t EngCreateOutputMix(void*, void** pMix, uint32_t, const SLInterfaceID*, const uint32_t*) {
    if (!pMix) return 2;
    *pMix = &MakeSlObject(SlKind::OutputMix)->objItf;
    return 0;
}
uint32_t EngCreateAudioPlayer(void*, void** pPlayer, SLDataSource_Guest* src, void*,
                              uint32_t, const SLInterfaceID*, const uint32_t*) {
    if (!pPlayer) return 2;
    auto* obj = MakeSlObject(SlKind::AudioPlayer);
    if (src && src->pFormat) {
        const auto* fmt = src->pFormat;
        if (fmt->numChannels >= 1 && fmt->numChannels <= 8) obj->channels = fmt->numChannels;
        if (fmt->samplesPerSec >= 8000000) obj->sampleRate = fmt->samplesPerSec / 1000u;
        else if (fmt->samplesPerSec >= 8000) obj->sampleRate = fmt->samplesPerSec;
        if (fmt->bitsPerSample == 16 || fmt->bitsPerSample == 32) obj->bitsPerSample = fmt->bitsPerSample;
        if (fmt->formatType == 4 /* SL_ANDROID_DATAFORMAT_PCM_EX */ && fmt->representation == 3) {
            obj->isFloat = true;
            obj->bitsPerSample = 32;
        }
    }
    obj->worker = std::thread(AudioPlayerThread, obj);
    *pPlayer = &obj->objItf;
    return 0;
}

const void* const kObjVtable[] = {
    reinterpret_cast<const void*>(&ObjRealize),
    reinterpret_cast<const void*>(&ObjResume),
    reinterpret_cast<const void*>(&ObjGetState),
    reinterpret_cast<const void*>(&ObjGetInterface),
    reinterpret_cast<const void*>(&ObjRegCb),
    reinterpret_cast<const void*>(&ObjAbort),
    reinterpret_cast<const void*>(&ObjDestroy),
    reinterpret_cast<const void*>(&ObjNoop2),
    reinterpret_cast<const void*>(&ObjNoopGet),
    reinterpret_cast<const void*>(&ObjLoss),
};

const void* const kEngineVtable[] = {
    reinterpret_cast<const void*>(&EngUnsupported),      // 0: CreateLEDDevice
    reinterpret_cast<const void*>(&EngUnsupported),      // 1: CreateVibraDevice
    reinterpret_cast<const void*>(&EngCreateAudioPlayer),// 2: CreateAudioPlayer
    reinterpret_cast<const void*>(&EngUnsupported),      // 3: CreateAudioRecorder
    reinterpret_cast<const void*>(&EngUnsupported),      // 4: CreateMidiPlayer
    reinterpret_cast<const void*>(&EngUnsupported),      // 5: CreateListener
    reinterpret_cast<const void*>(&EngUnsupported),      // 6: Create3DGroup
    reinterpret_cast<const void*>(&EngCreateOutputMix),  // 7: CreateOutputMix
    reinterpret_cast<const void*>(&EngUnsupported),      // 8: CreateMetadataExtractor
    reinterpret_cast<const void*>(&EngUnsupported),      // 9: CreateExtensionObject
    reinterpret_cast<const void*>(&EngUnsupported),
    reinterpret_cast<const void*>(&EngUnsupported),
    reinterpret_cast<const void*>(&EngUnsupported),
    reinterpret_cast<const void*>(&EngUnsupported),
    reinterpret_cast<const void*>(&EngUnsupported),
};

const void* const kPlayVtable[] = {
    reinterpret_cast<const void*>(&PlaySetState),
    reinterpret_cast<const void*>(&PlayGetState),
    reinterpret_cast<const void*>(&PlayGetMs),
    reinterpret_cast<const void*>(&PlayGetMs),
    reinterpret_cast<const void*>(&PlayNoop),
    reinterpret_cast<const void*>(&PlayMaskSet),
    reinterpret_cast<const void*>(&PlayMaskGet),
    reinterpret_cast<const void*>(&PlayMaskSet),
    reinterpret_cast<const void*>(&PlayClearPos),
    reinterpret_cast<const void*>(&PlayGetMs),
    reinterpret_cast<const void*>(&PlayMaskSet),
    reinterpret_cast<const void*>(&PlayClearPos),
    reinterpret_cast<const void*>(&PlayGetMs),
};

const void* const kBqVtable[] = {
    reinterpret_cast<const void*>(&BqEnqueue),
    reinterpret_cast<const void*>(&BqClear),
    reinterpret_cast<const void*>(&BqGetState),
    reinterpret_cast<const void*>(&BqRegCb),
};

const void* const kCfgVtable[] = {
    reinterpret_cast<const void*>(&CfgSet),
    reinterpret_cast<const void*>(&CfgGet),
    reinterpret_cast<const void*>(&CfgProxy),
    reinterpret_cast<const void*>(&CfgRelProxy),
};

const void* const kVolVtable[] = {
    reinterpret_cast<const void*>(&VolNoopSet),
    reinterpret_cast<const void*>(&VolNoopGet),
    reinterpret_cast<const void*>(&VolNoopGet),
    reinterpret_cast<const void*>(&VolMuteSet),
    reinterpret_cast<const void*>(&VolMuteGet),
    reinterpret_cast<const void*>(&VolMuteSet),
    reinterpret_cast<const void*>(&VolMuteGet),
    reinterpret_cast<const void*>(&VolNoopSet),
    reinterpret_cast<const void*>(&VolNoopGet),
};

SlObject* MakeSlObject(SlKind kind) {
    auto* o = new SlObject();
    o->objItf = {kObjVtable, o};
    o->engineItf = {kEngineVtable, o};
    o->playItf = {kPlayVtable, o};
    o->bqItf = {kBqVtable, o};
    o->cfgItf = {kCfgVtable, o};
    o->volItf = {kVolVtable, o};
    o->kind = kind;
    return o;
}

uint32_t SlCreateEngine(void** pEngine, uint32_t, const void*, uint32_t, const SLInterfaceID*, const uint32_t*) {
    if (!pEngine) return 2;
    *pEngine = &MakeSlObject(SlKind::Engine)->objItf;
    return 0;
}

// Minimal AAudio builder refusal (`AAUDIO_ERROR_UNAVAILABLE = -890`) so dual AAudio/OpenSLES
// engines cleanly select the OpenSLES WASAPI backend without fake silent success:
int AAudioCreateBuilder(void** out) {
    if (out) *out = nullptr;
    return -890; // AAUDIO_ERROR_UNAVAILABLE
}
int AAudioStubErr() { return -890; }
void AAudioStubVoid() {}

int64_t g_jvmNanoBase = 0;
int64_t g_qpcBase = 0;
int64_t g_qpcFreq = 1;

int64_t CurrentJvmNanoTime() {
    LARGE_INTEGER qpc{};
    QueryPerformanceCounter(&qpc);
    const int64_t delta = qpc.QuadPart - g_qpcBase;
    const int64_t sec = delta / g_qpcFreq;
    const int64_t rem = delta % g_qpcFreq;
    return g_jvmNanoBase + sec * 1000000000LL + (rem * 1000000000LL) / g_qpcFreq;
}

jlong JNICALL Java_NativeLooper_forCurrentThread(JNIEnv*, jclass) {
    return reinterpret_cast<jlong>(NdkLooperPrepare(0));
}

void JNICALL Java_NativeLooper_pollOnce(JNIEnv*, jclass, jlong nl, jint timeoutMs) {
    if (!nl) return;
    if (!t_looper) t_looper = reinterpret_cast<GuestLooper*>(nl);
    const int rc = NdkLooperPollOnce(timeoutMs, nullptr, nullptr, nullptr);
    if (rc == -2) {
        for (int spin = 0; spin < 16; ++spin) {
            if (NdkLooperPollOnce(0, nullptr, nullptr, nullptr) != -2) break;
        }
    }
}

void JNICALL Java_NativeLooper_pollPlainFds(JNIEnv*, jclass, jlong nl) {
    if (!nl) return;
    if (!t_looper) t_looper = reinterpret_cast<GuestLooper*>(nl);
    for (int spin = 0; spin < 16; ++spin) {
        if (NdkLooperPollOnce(0, nullptr, nullptr, nullptr) != -2) break;
    }
}

void JNICALL Java_NativeLooper_wake(JNIEnv*, jclass, jlong nl) {
    if (nl) NdkLooperWake(reinterpret_cast<void*>(nl));
}

jint JNICALL Java_NativeLooper_runMainPoll(JNIEnv*, jclass, jlong nl) {
    auto* l = reinterpret_cast<GuestLooper*>(nl);
    if (!l) return 0;
    {
        std::lock_guard lk(l->mu);
        if (l->fds.empty()) return 0;
    }
    if (!t_looper) t_looper = l;
    for (;;) {
        NdkLooperPollOnce(16, nullptr, nullptr, nullptr);
    }
}

void JNICALL Java_NativeLooper_scheduleVsync(JNIEnv*, jclass, jlong nl) {
    if (!nl) return;
    auto* l = reinterpret_cast<GuestLooper*>(nl);
    l->vsyncRequested.store(true, std::memory_order_release);
}

jlong JNICALL Java_NativeLooper_takeVsync(JNIEnv*, jclass, jlong nl) {
    if (!nl) return -1;
    auto* l = reinterpret_cast<GuestLooper*>(nl);
    return l->pendingVsyncNano.exchange(-1, std::memory_order_acq_rel);
}

}  // namespace

bool RegisterNativeLooper(JNIEnv* env) {
    LARGE_INTEGER qpc{}, freq{};
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&qpc);
    g_qpcFreq = std::max<int64_t>(1, freq.QuadPart);
    g_qpcBase = qpc.QuadPart;
    if (jclass sysCls = env->FindClass("java/lang/System")) {
        if (jmethodID nanoMid = env->GetStaticMethodID(sysCls, "nanoTime", "()J")) {
            g_jvmNanoBase = env->CallStaticLongMethod(sysCls, nanoMid);
        }
        env->ExceptionClear();
        env->DeleteLocalRef(sysCls);
    }
    static const JNINativeMethod kMethods[] = {
        {const_cast<char*>("forCurrentThread"), const_cast<char*>("()J"), reinterpret_cast<void*>(&Java_NativeLooper_forCurrentThread)},
        {const_cast<char*>("pollOnce"), const_cast<char*>("(JI)V"), reinterpret_cast<void*>(&Java_NativeLooper_pollOnce)},
        {const_cast<char*>("pollPlainFds"), const_cast<char*>("(J)V"), reinterpret_cast<void*>(&Java_NativeLooper_pollPlainFds)},
        {const_cast<char*>("wake"), const_cast<char*>("(J)V"), reinterpret_cast<void*>(&Java_NativeLooper_wake)},
        {const_cast<char*>("runMainPoll"), const_cast<char*>("(J)I"), reinterpret_cast<void*>(&Java_NativeLooper_runMainPoll)},
        {const_cast<char*>("scheduleVsync"), const_cast<char*>("(J)V"), reinterpret_cast<void*>(&Java_NativeLooper_scheduleVsync)},
        {const_cast<char*>("takeVsync"), const_cast<char*>("(J)J"), reinterpret_cast<void*>(&Java_NativeLooper_takeVsync)},
    };
    return Bind(env, "com/macapk/loader/NativeLooper", kMethods, static_cast<int>(std::size(kMethods)));
}

void DeliverNativeLooperVsync() {
    std::vector<GuestLooper*> ready;
    {
        std::lock_guard lk(g_allLoopersMu);
        for (GuestLooper* l : g_allLoopers) {
            if (l->vsyncRequested.exchange(false, std::memory_order_acq_rel)) {
                ready.push_back(l);
            }
        }
    }
    if (ready.empty()) return;
    const int64_t ts = CurrentJvmNanoTime();
    for (GuestLooper* l : ready) {
        l->pendingVsyncNano.store(ts, std::memory_order_release);
        NdkLooperWake(l);
    }
}

void AddNdkPlatformLibraries(guest::BuiltinLibrary& libandroid,
                             std::vector<guest::BuiltinLibrary>& libraries) {
    // 1. Append `ALooper_*`, `AChoreographer_*`, `AAssetManager_*` to `libandroid.so`:
    const guest::HostSymbol androidExtras[] = {
        {"ALooper_forThread", reinterpret_cast<void*>(&NdkLooperForThread)},
        {"ALooper_prepare", reinterpret_cast<void*>(&NdkLooperPrepare)},
        {"ALooper_acquire", reinterpret_cast<void*>(&NdkLooperAcquire)},
        {"ALooper_release", reinterpret_cast<void*>(&NdkLooperRelease)},
        {"ALooper_wake", reinterpret_cast<void*>(&NdkLooperWake)},
        {"ALooper_addFd", reinterpret_cast<void*>(&NdkLooperAddFd)},
        {"ALooper_removeFd", reinterpret_cast<void*>(&NdkLooperRemoveFd)},
        {"ALooper_pollOnce", reinterpret_cast<void*>(&NdkLooperPollOnce)},
        {"ALooper_pollAll", reinterpret_cast<void*>(&NdkLooperPollAll)},

        {"AChoreographer_getInstance", reinterpret_cast<void*>(&NdkChoreoGetInstance)},
        {"AChoreographer_postFrameCallback", reinterpret_cast<void*>(&NdkChoreoPost32)},
        {"AChoreographer_postFrameCallbackDelayed", reinterpret_cast<void*>(&NdkChoreoPostDelayed32)},
        {"AChoreographer_postFrameCallback64", reinterpret_cast<void*>(&NdkChoreoPost64)},
        {"AChoreographer_postFrameCallbackDelayed64", reinterpret_cast<void*>(&NdkChoreoPostDelayed64)},
        {"AChoreographer_registerRefreshRateCallback", reinterpret_cast<void*>(&NdkChoreoRegRefresh)},
        {"AChoreographer_unregisterRefreshRateCallback", reinterpret_cast<void*>(&NdkChoreoUnregRefresh)},

        {"AAssetManager_fromJava", reinterpret_cast<void*>(&NdkAssetMgrFromJava)},
        {"AAssetManager_open", reinterpret_cast<void*>(&NdkAssetOpen)},
        {"AAssetManager_openDir", reinterpret_cast<void*>(&NdkAssetOpenDir)},
        {"AAssetDir_getNextFileName", reinterpret_cast<void*>(&NdkAssetDirNext)},
        {"AAssetDir_rewind", reinterpret_cast<void*>(&NdkAssetDirRewind)},
        {"AAssetDir_close", reinterpret_cast<void*>(&NdkAssetDirClose)},
        {"AAsset_read", reinterpret_cast<void*>(&NdkAssetRead)},
        {"AAsset_seek", reinterpret_cast<void*>(&NdkAssetSeek)},
        {"AAsset_seek64", reinterpret_cast<void*>(&NdkAssetSeek64)},
        {"AAsset_close", reinterpret_cast<void*>(&NdkAssetClose)},
        {"AAsset_getBuffer", reinterpret_cast<void*>(&NdkAssetGetBuffer)},
        {"AAsset_getLength", reinterpret_cast<void*>(&NdkAssetGetLength)},
        {"AAsset_getLength64", reinterpret_cast<void*>(&NdkAssetGetLength64)},
        {"AAsset_getRemainingLength", reinterpret_cast<void*>(&NdkAssetGetRemaining)},
        {"AAsset_getRemainingLength64", reinterpret_cast<void*>(&NdkAssetGetRemaining64)},
        {"AAsset_openFileDescriptor", reinterpret_cast<void*>(&NdkAssetOpenFd)},
        {"AAsset_openFileDescriptor64", reinterpret_cast<void*>(&NdkAssetOpenFd64)},
        {"AAsset_isAllocated", reinterpret_cast<void*>(&NdkAssetIsAllocated)},
    };
    libandroid.symbols.insert(libandroid.symbols.end(), std::begin(androidExtras), std::end(androidExtras));

    // 2. `libjnigraphics.so`:
    libraries.push_back({"libjnigraphics.so", {
        {"AndroidBitmap_getInfo", reinterpret_cast<void*>(&JniBitmapGetInfo)},
        {"AndroidBitmap_lockPixels", reinterpret_cast<void*>(&JniBitmapLockPixels)},
        {"AndroidBitmap_unlockPixels", reinterpret_cast<void*>(&JniBitmapUnlockPixels)},
    }, ""});

    // 3. `libOpenSLES.so`:
    libraries.push_back({"libOpenSLES.so", {
        {"slCreateEngine", reinterpret_cast<void*>(&SlCreateEngine)},
        {"SL_IID_ENGINE", &g_iidEngine},
        {"SL_IID_PLAY", &g_iidPlay},
        {"SL_IID_BUFFERQUEUE", &g_iidBufQueue},
        {"SL_IID_ANDROIDSIMPLEBUFFERQUEUE", &g_iidAndSimple},
        {"SL_IID_ANDROIDCONFIGURATION", &g_iidAndCfg},
        {"SL_IID_VOLUME", &g_iidVolume},
    }, ""});

    // 4. `libaaudio.so`:
    libraries.push_back({"libaaudio.so", {
        {"AAudio_createStreamBuilder", reinterpret_cast<void*>(&AAudioCreateBuilder)},
        {"AAudioStreamBuilder_delete", reinterpret_cast<void*>(&AAudioStubErr)},
        {"AAudioStreamBuilder_openStream", reinterpret_cast<void*>(&AAudioStubErr)},
        {"AAudioStreamBuilder_setDeviceId", reinterpret_cast<void*>(&AAudioStubVoid)},
        {"AAudioStreamBuilder_setSampleRate", reinterpret_cast<void*>(&AAudioStubVoid)},
        {"AAudioStreamBuilder_setChannelCount", reinterpret_cast<void*>(&AAudioStubVoid)},
        {"AAudioStreamBuilder_setFormat", reinterpret_cast<void*>(&AAudioStubVoid)},
        {"AAudioStreamBuilder_setSharingMode", reinterpret_cast<void*>(&AAudioStubVoid)},
        {"AAudioStreamBuilder_setDirection", reinterpret_cast<void*>(&AAudioStubVoid)},
        {"AAudioStreamBuilder_setPerformanceMode", reinterpret_cast<void*>(&AAudioStubVoid)},
        {"AAudioStreamBuilder_setDataCallback", reinterpret_cast<void*>(&AAudioStubVoid)},
        {"AAudioStreamBuilder_setErrorCallback", reinterpret_cast<void*>(&AAudioStubVoid)},
        {"AAudioStream_close", reinterpret_cast<void*>(&AAudioStubErr)},
        {"AAudioStream_requestStart", reinterpret_cast<void*>(&AAudioStubErr)},
        {"AAudioStream_requestPause", reinterpret_cast<void*>(&AAudioStubErr)},
        {"AAudioStream_requestStop", reinterpret_cast<void*>(&AAudioStubErr)},
        {"AAudioStream_write", reinterpret_cast<void*>(&AAudioStubErr)},
    }, ""});
}

}  // namespace winapk::bridge
