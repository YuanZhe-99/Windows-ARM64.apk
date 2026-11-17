// P4 JNI foundation with a real ARM64 JVM, not APK admission or G3.
#include "doctor.h"
#include "../../../guest/jni/bridge.h"
#include "../../../guest/bionic/threads.h"

#include <windows.h>
#include <cstdio>
#include <filesystem>

namespace {
JavaVM* hostVm = nullptr;
jobject expectedLoader = nullptr;
winapk::guest::JniBridge* activeBridge = nullptr;
int (*guestCase)(void*, void*, int) = nullptr;
int contextExits = 0;

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
    if (argc == 5 && (argv[4][0] < L'0' || argv[4][0] > L'7' || argv[4][1])) return 1;
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
    int result = 0;
    {
        jclass type = env->FindClass("com/winapk/probe/GuestJniProbe");
        jmethodID getLoader = type ? env->GetStaticMethodID(type, "isolatedLoader", "()Ljava/lang/ClassLoader;") : nullptr;
        jobject loader = getLoader ? env->CallStaticObjectMethodA(type, getLoader, nullptr) : nullptr;
        if (env->ExceptionCheck() || !loader) return 2;
        expectedLoader = env->NewGlobalRef(loader);
        LoadError error;
        auto fixed = ThreadRuntime::Imports();
        std::vector<HostSymbol> imports(fixed.begin(), fixed.end());
        imports.push_back({"t09_host_state", reinterpret_cast<void*>(&HostState)});
        imports.push_back({"t09_nested", reinterpret_cast<void*>(&Nested)});
        imports.push_back({"t09_context_exit", reinterpret_cast<void*>(&ContextExit)});
        auto module = guest::Load(argv[2], imports, &error);
        if (!module) { std::printf("reject %s %s\n", CategoryName(error.category), error.reason.c_str()); return 2; }
        ThreadRuntime threads;
        auto bridge = JniBridge::Load(env, loader, threads, argv[3], &error);
        if (!bridge) { std::printf("reject %s %s\n", CategoryName(error.category), error.reason.c_str()); return 2; }
        auto entry = reinterpret_cast<int (*)(void*, void*, int)>(module->Find("t09_case"));
        if (!entry) return 2;
        activeBridge = bridge.get();
        guestCase = entry;
        void* proxy = bridge->Environment(env);
        if (!proxy || proxy == env || bridge->Vm() == vm) return 3;
        const char* names[] = {"calls", "references", "exceptions", "workers"};
        const int first = argc == 5 ? selected : 0;
        const int last = argc == 5 ? first + 1 : 4;
        for (int number = first; number < last; number++) {
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
            Case begin{entry, proxy, bridge->Vm(), 8}, verify{entry, proxy, bridge->Vm(), 9};
            bridge->Call(env, Run, &begin);
            bridge->Call(env, Run, &verify);
            const bool ok = !env->ExceptionCheck() && !begin.result && !verify.result && !contextExits;
            std::printf("case T09.context %s begin=%d verify=%d\n", ok ? "PASS" : "FAIL", begin.result, verify.result);
            if (!ok) result = 3;
        }
        bridge.reset();  // Key/TLS callbacks must still see a live proxy, loader and guest mappings.
        if (argc == 4) {
            const bool ok = threads.Idle() && contextExits == 1 && !env->ExceptionCheck();
            std::printf("case T09.teardown %s destructors=%d\n", ok ? "PASS" : "FAIL", contextExits);
            if (!ok) result = 3;
        }
        activeBridge = nullptr;
        env->DeleteLocalRef(loader);
        env->DeleteLocalRef(type);
        env->DeleteGlobalRef(expectedLoader);
    }
    vm->DestroyJavaVM();
    if (!result) std::printf("result T09.foundation PASS (not G3)\n");
    return result;
}
