// log.cpp -- com.macapk.loader.NativeLog on Windows: the app log, the boot clock and the
// thread priority hint.
#include "bridge.h"

#include <windows.h>

#include <cstdlib>

namespace winapk::bridge {
namespace {

/// Returns the lowest android.util.Log level printed (MACAPK_LOG_LEVEL, as upstream).
int MinLevel() {
    static const int level = [] {
        char e[16];
        const DWORD n = GetEnvironmentVariableA("MACAPK_LOG_LEVEL", e, sizeof e);
        return n > 0 && n < sizeof e ? std::atoi(e) : 0;
    }();
    return level;
}

/// Prints one android.util.Log line as "[I] tag: message", upstream's format.
void JNICALL Print(JNIEnv* env, jclass, jint level, jstring tag, jstring msg) {
    if (level < MinLevel()) return;
    const char* lv = level <= 2 ? "V" : level == 3 ? "D" : level == 4 ? "I" : level == 5 ? "W" : "E";
    LogLine(std::string("[") + lv + "] " + Utf8(env, tag) + ": " + Utf8(env, msg));
}

/// Raises the calling thread's scheduling priority (upstream raises its QoS class to
/// user-interactive for the UI threads).
void JNICALL BoostCurrentThread(JNIEnv*, jclass) {
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);
}

/// Returns SystemClock.elapsedRealtimeNanos(): the performance counter in nanoseconds.
/// Notes: the same counter and conversion as HotSpot's System.nanoTime on Windows, so
/// elapsedRealtimeNanos() >= nanoTime() holds as AOSP code assumes; the counter keeps
/// running while the PC sleeps, as CLOCK_BOOTTIME does.
jlong JNICALL BootTimeNanos(JNIEnv*, jclass) {
    static const long long freq = [] {
        LARGE_INTEGER f;
        QueryPerformanceFrequency(&f);
        return f.QuadPart;
    }();
    LARGE_INTEGER c;
    QueryPerformanceCounter(&c);
    const long long whole = c.QuadPart / freq, rest = c.QuadPart % freq;
    return static_cast<jlong>(whole * 1000000000LL + rest * 1000000000LL / freq);
}

const JNINativeMethod kMethods[] = {
    {const_cast<char*>("print"), const_cast<char*>("(ILjava/lang/String;Ljava/lang/String;)V"), reinterpret_cast<void*>(Print)},
    {const_cast<char*>("nativeBoostCurrentThreadQos"), const_cast<char*>("()V"), reinterpret_cast<void*>(BoostCurrentThread)},
    {const_cast<char*>("nativeBootTimeNanos"), const_cast<char*>("()J"), reinterpret_cast<void*>(BootTimeNanos)},
};

}  // namespace

bool RegisterLog(JNIEnv* env) {
    return Bind(env, "com/macapk/loader/NativeLog", kMethods, static_cast<int>(std::size(kMethods)));
}

}  // namespace winapk::bridge
