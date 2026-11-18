// guest.cpp -- process-private P4 APK namespace and host-owned guest JNI service.
#include "bridge.h"
#include "surface.h"
#include "guest_graphics.h"
#include "guest_vulkan.h"
#include "../../../../guest/bionic/libraries.h"
#include "../../../../guest/jni/bridge.h"

#include <windows.h>
#include <filesystem>
#include <memory>
#include <mutex>
#include <set>
#include <sstream>

namespace winapk::bridge {
namespace {

/// Retains the host-selected directories and profile without accepting app-supplied host paths.
struct GuestConfig {
    std::wstring libraries, helpers, android;
    std::string profile = "N2";
    std::set<std::string> admitted;
};
GuestConfig config;
std::mutex guestMutex;

/// Owns one app's providers, namespace, thread state and JNI bridge in teardown order.
struct GuestRuntime {
    std::unique_ptr<guest::BionicLibraries> support;
    std::unique_ptr<guest::ModuleSet> modules;
    std::unique_ptr<guest::ThreadRuntime> threads;
    std::unique_ptr<guest::JniBridge> jni;
    jobject loader = nullptr;
};
// Process lifetime: never run a JNI destructor after HotSpot or while Java workers remain.
GuestRuntime* runtime = nullptr;

/// Reports a stable loading category without clearing a hook's pending Java exception.
void Reject(JNIEnv* env, const guest::LoadError& error) {
    if (env->ExceptionCheck()) return;
    const std::string text = std::string(guest::CategoryName(error.category)) + ": " + error.reason;
    jclass type = env->FindClass("java/lang/UnsatisfiedLinkError");
    if (type) { env->ThrowNew(type, text.c_str()); env->DeleteLocalRef(type); }
}

/// Creates the bridge once for the actual app loader, then initializes an explicit ELF root.
void Open(JNIEnv* env, jclass, jobject loader, jstring name) {
    try {
        const auto basename = Utf8(env, name);
        if (env->ExceptionCheck()) return;
        GuestRuntime* current;
        {
            std::lock_guard lock(guestMutex);
            if (config.libraries.empty() || !loader) {
                Reject(env, {guest::Category::AbiUnsupported, "", "guest native is not configured"});
                return;
            }
            if (!config.admitted.contains(basename)) {
                Reject(env, {guest::Category::AbiUnsupported, "", "library not present in APK arm64-v8a set"});
                return;
            }
            if (!runtime) {
                auto value = std::make_unique<GuestRuntime>();
                guest::LoadError error;
                const bool n0 = config.profile == "N0";
                const bool n2 = config.profile == "N2";
                std::span<const guest::HostSymbol> imports{};
                std::vector<guest::BuiltinLibrary> libraries;
                if (!n0) {
                    value->support = guest::BionicLibraries::Load(config.helpers + L"\\libwinapk-bionic.so", &error);
                    if (!value->support) { Reject(env, error); return; }
                    imports = value->support->Imports();
                    const auto baseLibraries = value->support->Libraries();
                    libraries.assign(baseLibraries.begin(), baseLibraries.end());
                    if (n2) {
                        libraries.push_back(NativeWindowLibrary());
                        AddGuestGraphicsLibraries(libraries);
                        AddGuestVulkanLibrary(libraries);
                    }
                }
                value->modules = std::make_unique<guest::ModuleSet>(config.libraries, imports, libraries);
                if (n2) SetGuestVulkanModules(value->modules.get());
                value->threads = std::make_unique<guest::ThreadRuntime>(value->modules.get(), config.android);
                value->jni = guest::JniBridge::Load(env, loader, *value->threads, *value->modules,
                    config.helpers + L"\\libwinapk-jni.so", &error, !n0);
                if (!value->jni) { Reject(env, error); return; }
                value->loader = env->NewGlobalRef(loader);
                if (!value->loader) return;
                runtime = value.release();
            }
            current = runtime;
            if (!env->IsSameObject(loader, current->loader)) {
                Reject(env, {guest::Category::AbiUnsupported, "", "additional guest ClassLoader is not admitted"});
                return;
            }
        }
        guest::LoadError error;
        if (!current->jni->OpenLibrary(env, basename, &error)) Reject(env, error);
    } catch (const std::bad_alloc&) {
        Reject(env, {guest::Category::HostMemory, "", "guest runtime allocation failed"});
    }
}

/// Binds native declarations only after an explicit root succeeded in this app namespace.
void BindExports(JNIEnv* env, jclass, jclass type) {
    GuestRuntime* current;
    { std::lock_guard lock(guestMutex); current = runtime; }
    if (!current || !current->jni->BindExports(env, type))
        Reject(env, {guest::Category::AbiUnsupported, "", "guest export binding failed"});
}

}  // namespace

bool RegisterGuest(JNIEnv* env) {
    JNINativeMethod methods[] = {
        {const_cast<char*>("openNative"), const_cast<char*>("(Ljava/lang/ClassLoader;Ljava/lang/String;)V"), reinterpret_cast<void*>(&Open)},
        {const_cast<char*>("bindNative"), const_cast<char*>("(Ljava/lang/Class;)V"), reinterpret_cast<void*>(&BindExports)},
    };
    return Bind(env, "com/macapk/loader/GuestNativeLoader", methods, 2);
}

}  // namespace winapk::bridge

/// Configures the one APK namespace before its JVM starts, without running guest code.
extern "C" __declspec(dllexport) void winapk_bridge_configure_guest(
        const wchar_t* libraries, const wchar_t* helpers, const wchar_t* android,
        const char* admitted, const char* profile) {
    winapk::bridge::config = {libraries, helpers, android, profile && *profile ? profile : "N2"};
    std::istringstream names(admitted);
    for (std::string name; std::getline(names, name);) winapk::bridge::config.admitted.insert(name);
}
