// bridge.cpp -- the registration entry points of winapk-bridge.dll.
#include "bridge.h"
#include "embed.h"
#include "graphics.h"

#include <windows.h>

#include <cstdio>
#include <mutex>

// upstream native_sqlite.cpp, built into this DLL: com.macapk.loader.NativeSqlite.
namespace macapk::sqlite { void RegisterNativeSqliteMethods(JNIEnv* env); }

namespace winapk::bridge {

bool Bind(JNIEnv* env, const char* className, const JNINativeMethod* methods, int count) {
    jclass cls = env->FindClass(className);
    if (!cls) {
        env->ExceptionClear();
        LogLine(std::string("[winapk] bridge: class ") + className + " not found");
        return false;
    }
    bool ok = true;
    // One by one, so a method missing from this Java build names itself and the rest bind.
    for (int i = 0; i < count; ++i) {
        if (env->RegisterNatives(cls, &methods[i], 1) != JNI_OK) {
            env->ExceptionClear();
            LogLine(std::string("[winapk] bridge: ") + className + "." + methods[i].name +
                    methods[i].signature + " not bound");
            ok = false;
        }
    }
    env->DeleteLocalRef(cls);
    return ok;
}

std::string Utf8(JNIEnv* env, jstring s) {
    if (!s) return {};
    // GetStringUTFChars is modified UTF-8; go through UTF-16 for real UTF-8.
    const jsize n = env->GetStringLength(s);
    const jchar* w = env->GetStringChars(s, nullptr);
    std::string out;
    if (w && n > 0) {
        const int len = WideCharToMultiByte(CP_UTF8, 0, reinterpret_cast<const wchar_t*>(w), n, nullptr, 0, nullptr, nullptr);
        out.resize(len);
        WideCharToMultiByte(CP_UTF8, 0, reinterpret_cast<const wchar_t*>(w), n, out.data(), len, nullptr, nullptr);
    }
    if (w) env->ReleaseStringChars(s, w);
    return out;
}

void LogLine(const std::string& line) {
    {
        static std::mutex mu;
        std::lock_guard<std::mutex> lock(mu);
        std::fwrite(line.data(), 1, line.size(), stderr);
        std::fputc('\n', stderr);
        std::fflush(stderr);
    }
    SendEmbedLog(line);
}

}  // namespace winapk::bridge

extern "C" __declspec(dllexport) jboolean winapk_bridge_register(JNIEnv* env) {
    using namespace winapk::bridge;
    bool ok = RegisterLog(env);
    ok = RegisterWindow(env) && ok;
    ok = RegisterGuest(env) && ok;
    ok = RegisterJavaGraphics(env) && ok;
    // Host services (NativeOs / Clipboard) are optional on minimal HostProbe classpaths that lack macapk-loader.jar.
    if (jclass osCls = env->FindClass("com/macapk/loader/NativeOs")) {
        env->DeleteLocalRef(osCls);
        ok = RegisterHostServices(env) && ok;
    } else {
        env->ExceptionClear();
    }
    if (jclass audioCls = env->FindClass("com/macapk/loader/NativeAudio")) {
        env->DeleteLocalRef(audioCls);
        ok = RegisterAudio(env) && ok;
    } else {
        env->ExceptionClear();
    }
    macapk::sqlite::RegisterNativeSqliteMethods(env);  // logs its own result, as upstream
    return ok ? JNI_TRUE : JNI_FALSE;
}

/// Registers the natives when a java.exe JVM loads this DLL with System.load.
extern "C" JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void*) {
    JNIEnv* env = nullptr;
    if (vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_21) != JNI_OK) return JNI_ERR;
    winapk_bridge_register(env);
    return JNI_VERSION_21;
}
