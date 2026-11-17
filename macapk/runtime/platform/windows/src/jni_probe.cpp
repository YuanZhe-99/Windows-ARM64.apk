// P4 JNI foundation with a real ARM64 JVM, not APK admission or G3.
#include "doctor.h"
#include "../../../guest/jni/bridge.h"
#include "../../../guest/bionic/threads.h"
#include "../../../guest/bionic/libraries.h"

#include <windows.h>
#include <cstdio>
#include <filesystem>

namespace {
JavaVM* hostVm = nullptr;
jobject expectedLoader = nullptr;
winapk::guest::JniBridge* activeBridge = nullptr;
int (*guestCase)(void*, void*, int) = nullptr;
int contextExits = 0;
std::vector<int> loadEvents;

/// Records actual dependency constructors and JNI hooks, not inferred library state.
void LoadEvent(int event) { loadEvents.push_back(event); }

/// Owns the probe's initial explicit ELF reference independently of the JNI library registry.
struct Fixture {
    winapk::guest::ModuleSet& modules;
    std::string name;
    uint64_t handle = 0;
    winapk::guest::LoadError error;
};

/// Opens the test entry image under a managed guest context, without invoking any JNI hook.
void* OpenFixture(void* argument) {
    auto& fixture = *static_cast<Fixture*>(argument);
    fixture.handle = fixture.modules.Open(fixture.name, &fixture.error);
    return nullptr;
}

/// Checks x18/sp read-only and the actual isolated loader reached through guest FindClass.
int HostState(jclass type) {
    uintptr_t x18, sp;
    __asm__ volatile("mov %0, x18\n mov %1, sp" : "=r"(x18), "=r"(sp));
    if (x18 != reinterpret_cast<uintptr_t>(NtCurrentTeb()) || (sp & 15)) return 0;
    JNIEnv* env = nullptr;
    if (hostVm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK) return 0;
    jclass cls = env->FindClass("java/lang/Class");
    jmethodID getLoader = cls ? env->GetMethodID(cls, "getClassLoader", "()Ljava/lang/ClassLoader;") : nullptr;
    jobject loader = getLoader ? env->CallObjectMethodA(type, getLoader, nullptr) : nullptr;
    bool ok = !env->ExceptionCheck() && loader && env->IsSameObject(loader, expectedLoader);
    if (loader) env->DeleteLocalRef(loader);
    if (cls) env->DeleteLocalRef(cls);
    return ok;
}

/// Calls one guest case under a Bionic context without exposing the host JNIEnv.
struct Case {
    int (*entry)(void*, void*, int);
    void* env;
    void* vm;
    int number;
    int result = -1;
};

/// Adapts the fixed guest case signature to the managed thread runtime.
void* Run(void* argument) {
    auto& test = *static_cast<Case*>(argument);
    test.result = test.entry(test.env, test.vm, test.number);
    return nullptr;
}

/// Reenters the same retained binding through a host-controlled callback, without replacing guest state.
int Nested(int closing) {
    if (closing != 0 && closing != 1) return 1;
    JNIEnv* env = nullptr;
    if (hostVm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK) return 1;
    Case test{guestCase, activeBridge->Environment(env), activeBridge->Vm(), closing ? 11 : 10};
    activeBridge->Call(env, Run, &test);
    return test.result;
}

/// Records an actually executed key destructor after it has checked its JNI/TLS context.
void ContextExit(int ok) { contextExits += ok ? 1 : 100; }
}  // namespace

/// Starts one native JVM and runs a compact descriptor/varargs/reference/thread contract matrix.
int wmain(int argc, wchar_t** argv) {
    using namespace winapk;
    using namespace winapk::guest;
    if (argc != 4 && argc != 5) return 1;
    if (argc == 5 && (argv[4][0] < L'0' || argv[4][0] > L'8' || argv[4][1])) return 1;
    const int selected = argc == 5 ? argv[4][0] - L'0' : -1;
    SetDefaultDllDirectories(LOAD_LIBRARY_SEARCH_DEFAULT_DIRS | LOAD_LIBRARY_SEARCH_USER_DIRS);
    if (PeArch(std::wstring(argv[1]) + L"\\bin\\server\\jvm.dll") != "arm64") return 2;
    const auto jar = std::filesystem::path(argv[0]).parent_path() / L"winapk-probe.jar";
    JavaVM* vm = nullptr;
    std::string reason;
    const auto errorLog = std::filesystem::absolute(jar.parent_path() / L".." / L"test-data" / L"logs" / L"hs_err_pid%p.log");
    std::vector<std::string> options = {"-Djava.class.path=" + Utf8(jar.wstring()), "-Xcheck:jni",
        "-XX:ErrorFile=" + Utf8(errorLog.wstring()), "-XX:-CreateCoredumpOnCrash",
        "-Xrs", "-Djava.awt.headless=true"};
    if (argc == 4) {
        options.insert(options.end(), {"-Xbatch", "-XX:+UnlockDiagnosticVMOptions", "-XX:+LogCompilation",
            "-XX:LogFile=" + Utf8((errorLog.parent_path() / L"T09-compilation.xml").wstring())});
    }
    JNIEnv* env = CreateJvm(argv[1], std::move(options), &vm, reason);
    if (!env) { std::printf("reject %s\n", reason.c_str()); return 2; }
    hostVm = vm;
    loadEvents.reserve(32);  // Guest callbacks append bounded fixture events without host allocation.
    int result = 0;
    {
        jclass type = env->FindClass("com/winapk/probe/GuestJniProbe");
        jmethodID getLoader = type ? env->GetStaticMethodID(type, "isolatedLoader", "()Ljava/lang/ClassLoader;") : nullptr;
        jobject loader = getLoader ? env->CallStaticObjectMethodA(type, getLoader, nullptr) : nullptr;
        if (env->ExceptionCheck() || !loader) return 2;
        expectedLoader = env->NewGlobalRef(loader);
        LoadError error;
        auto support = BionicLibraries::Load((std::filesystem::path(argv[3]).parent_path() / L"libwinapk-bionic.so").wstring(), &error);
        if (!support) { std::printf("reject %s %s\n", CategoryName(error.category), error.reason.c_str()); return 2; }
        std::vector<HostSymbol> imports(support->Imports().begin(), support->Imports().end());
        imports.push_back({"t09_host_state", reinterpret_cast<void*>(&HostState)});
        imports.push_back({"t09_nested", reinterpret_cast<void*>(&Nested)});
        imports.push_back({"t09_context_exit", reinterpret_cast<void*>(&ContextExit)});
        imports.push_back({"t09_load_event", reinterpret_cast<void*>(&LoadEvent)});
        ModuleSet modules(std::filesystem::path(argv[2]).parent_path().wstring(), imports, support->Libraries());
        ThreadRuntime threads(&modules);
        Fixture fixture{modules, Utf8(std::filesystem::path(argv[2]).filename().wstring())};
        threads.Call(OpenFixture, &fixture);
        if (!fixture.handle) { std::printf("reject %s %s\n", CategoryName(fixture.error.category), fixture.error.reason.c_str()); return 2; }
        auto bridge = JniBridge::Load(env, loader, threads, modules, argv[3], &error);
        if (!bridge) { std::printf("reject %s %s\n", CategoryName(error.category), error.reason.c_str()); return 2; }
        if (!bridge->OpenLibrary(env, fixture.name, &error)) return 2;
        auto entry = reinterpret_cast<int (*)(void*, void*, int)>(modules.Find(fixture.handle, "t09_case"));
        if (!entry) return 2;
        activeBridge = bridge.get();
        guestCase = entry;
        void* proxy = bridge->Environment(env);
        if (!proxy || proxy == env || bridge->Vm() == vm) return 3;
        if (selected == 8) {
            if (bridge->OpenLibrary(env, "libjni-version.so", &error) || env->ExceptionCheck()) return 2;
            jclass cls = env->FindClass("java/lang/ClassLoader");
            jmethodID load = env->GetMethodID(cls, "loadClass", "(Ljava/lang/String;)Ljava/lang/Class;");
            jstring name = env->NewStringUTF("com.winapk.probe.GuestJniProbe");
            jvalue arg{}; arg.l = name;
            auto guest = static_cast<jclass>(env->CallObjectMethodA(loader, load, &arg));
            if (env->ExceptionCheck() || !guest) return 2;
            jmethodID method = env->GetStaticMethodID(guest, "badVersionRegistered", "()I");
            env->CallStaticIntMethodA(guest, method, nullptr);  // Must terminate before entering failed guest code.
            return 3;
        }
        const char* names[] = {"calls", "references", "exceptions", "workers"};
        const int first = argc == 5 ? selected : 0;
        const int last = argc == 5 ? first + 1 : 4;
        jclass nativeType = nullptr;
        if (argc == 4 && !result) {
            Case registration{entry, proxy, bridge->Vm(), 12};
            bridge->Call(env, Run, &registration);
            jclass loaderClass = env->FindClass("java/lang/ClassLoader");
            jmethodID loadClass = loaderClass ? env->GetMethodID(loaderClass, "loadClass",
                "(Ljava/lang/String;)Ljava/lang/Class;") : nullptr;
            jstring name = env->NewStringUTF("com.winapk.probe.GuestJniProbe");
            jvalue arg{}; arg.l = name;
            auto guestType = loadClass ? static_cast<jclass>(env->CallObjectMethodA(loader, loadClass, &arg)) : nullptr;
            const bool loaded = !env->ExceptionCheck() && guestType;
            if (loaded) nativeType = static_cast<jclass>(env->NewLocalRef(guestType));
            jmethodID runNatives = loaded ? env->GetStaticMethodID(guestType, "runNatives", "()Z") : nullptr;
            jboolean called = runNatives ? env->CallStaticBooleanMethodA(guestType, runNatives, nullptr) : JNI_FALSE;
            const bool ok = !env->ExceptionCheck() && !registration.result && called == JNI_TRUE;
            std::printf("case T09.native %s registration=%d\n", ok ? "PASS" : "FAIL", registration.result);
            if (!ok) result = 3;
            if (guestType) env->DeleteLocalRef(guestType);
            if (name) env->DeleteLocalRef(name);
            if (loaderClass) env->DeleteLocalRef(loaderClass);
        }
        for (int number = first; number < last && !result; number++) {
            Case test{entry, proxy, bridge->Vm(), number};
            bridge->Call(env, Run, &test);
            const bool exception = env->ExceptionCheck();
            const bool ok = test.result == 0 && !exception && bridge->Environments() == 1;
            std::printf("case T09.%s %s value=%d proxies=%zu\n", number < 4 ? names[number] : "negative",
                        ok ? "PASS" : "FAIL", test.result, bridge->Environments());
            if (exception) { env->ExceptionDescribe(); env->ExceptionClear(); }
            if (!ok) { result = 3; break; }
        }
        if (argc == 4 && !result) {
            bool ok = bridge->OpenLibrary(env, "libjni-load.so", &error);
            ok = ok && loadEvents == std::vector<int>({2, 1, 3}) &&
                 bridge->OpenLibrary(env, "libjni-load.so", &error) &&
                 bridge->OpenLibrary(env, "libjni-noinit.so", &error) &&
                 loadEvents == std::vector<int>({2, 1, 3, 1});
            ok = ok && bridge->BindExports(env, nativeType) && bridge->BindExports(env, nativeType);
            jmethodID exports = ok ? env->GetStaticMethodID(nativeType, "runExports", "()Z") : nullptr;
            jboolean value = exports ? env->CallStaticBooleanMethodA(nativeType, exports, nullptr) : JNI_FALSE;
            ok = !env->ExceptionCheck() && ok && value;
            std::printf("case T09.libraries %s\n", ok ? "PASS" : "FAIL");
            if (!ok) {
                std::printf("diagnostic %s %s events:", CategoryName(error.category), error.reason.c_str());
                for (int event : loadEvents) std::printf(" %d", event);
                std::printf("\n");
                if (env->ExceptionCheck()) { env->ExceptionDescribe(); env->ExceptionClear(); }
                result = 3;
            }
        }
        if (argc == 4 && !result) {
            bool ok = !bridge->OpenLibrary(env, "libjni-version.so", &error) &&
                      error.category == Category::AbiUnsupported && error.reason.find("version") != std::string::npos &&
                      !env->ExceptionCheck();
            const auto afterVersion = loadEvents;
            ok = ok && !bridge->OpenLibrary(env, "libjni-version.so", &error) && loadEvents == afterVersion;
            ok = ok && !bridge->OpenLibrary(env, "libjni-pending.so", &error) && env->ExceptionCheck();
            jthrowable pending = env->ExceptionOccurred();
            env->ExceptionClear();
            jclass illegal = env->FindClass("java/lang/IllegalStateException");
            ok = ok && pending && illegal && env->IsInstanceOf(pending, illegal);
            if (pending) env->DeleteLocalRef(pending);
            if (illegal) env->DeleteLocalRef(illegal);
            const auto afterPending = loadEvents;
            ok = ok && !bridge->OpenLibrary(env, "libjni-pending.so", &error) && loadEvents == afterPending &&
                 !env->ExceptionCheck();
            ok = ok && !bridge->OpenLibrary(env, "libjni-datahook.so", &error) &&
                 error.category == Category::AbiUnsupported &&
                 error.reason.find("not a guest executable entry") != std::string::npos && !env->ExceptionCheck();
            std::printf("case T09.onload-refusal %s\n", ok ? "PASS" : "FAIL");
            if (!ok) {
                std::printf("diagnostic %s %s\n", CategoryName(error.category), error.reason.c_str());
                if (env->ExceptionCheck()) { env->ExceptionDescribe(); env->ExceptionClear(); }
                result = 3;
            }
        }
        if (argc == 4 && !result) {
            Case begin{entry, proxy, bridge->Vm(), 8}, verify{entry, proxy, bridge->Vm(), 9};
            bridge->Call(env, Run, &begin);
            bridge->Call(env, Run, &verify);
            const bool ok = !env->ExceptionCheck() && !begin.result && !verify.result && !contextExits;
            std::printf("case T09.context %s begin=%d verify=%d\n", ok ? "PASS" : "FAIL", begin.result, verify.result);
            if (!ok) result = 3;
        }
        bridge.reset();  // Key/TLS callbacks must still see a live proxy, loader and guest mappings.
        if (argc == 4) {
            jmethodID after = nativeType ? env->GetStaticMethodID(nativeType, "afterUnregister", "()Z") : nullptr;
            jboolean unregistered = after ? env->CallStaticBooleanMethodA(nativeType, after, nullptr) : JNI_FALSE;
            const bool ok = !env->ExceptionCheck() && threads.Idle() && contextExits == 1 && unregistered;
            std::printf("case T09.teardown %s destructors=%d\n", ok ? "PASS" : "FAIL", contextExits);
            if (!ok) result = 3;
        }
        activeBridge = nullptr;
        env->DeleteLocalRef(loader);
        env->DeleteLocalRef(type);
        env->DeleteGlobalRef(expectedLoader);
        if (nativeType) env->DeleteLocalRef(nativeType);
    }
    vm->DestroyJavaVM();
    if (!result) std::printf("result T09.foundation PASS (not G3)\n");
    return result;
}
