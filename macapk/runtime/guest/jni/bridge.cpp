// Host-domain JNI adapters. Only fixed, ABI-audited signatures enter here from Android code.
#include "bridge.h"
#include "../bionic/threads.h"

#include <windows.h>
#include <array>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <mutex>
#include <utility>

namespace winapk::guest {
namespace {

constexpr size_t kSlots = 233;  // JNI 1.6 through GetObjectRefType, matching the NDK table.
static_assert(offsetof(JNINativeInterface_, GetObjectRefType) == 232 * sizeof(void*));
static_assert(sizeof(jlong) == 8 && sizeof(jint) == 4 && sizeof(jvalue) == 8 &&
              sizeof(jfloat) == 4 && sizeof(jdouble) == 8 && sizeof(jchar) == 2 &&
              sizeof(jobjectRefType) == 4);

/// Stops at a JNI contract violation, never unwinding a host C++ exception into guest code.
[[noreturn]] void Fatal(const char* reason, size_t slot = 0) {
    std::fprintf(stderr, "GUEST_ABI_UNSUPPORTED JNI: %s (slot %zu)\n", reason, slot);
    std::fflush(stderr);
    TerminateProcess(GetCurrentProcess(), 3);
    std::abort();
}

/// Refuses one unimplemented table slot without reading parameters of an unknown signature.
template<size_t Slot> [[noreturn]] void Unsupported() { Fatal("unsupported entry", Slot); }

/// Creates explicit refusal entries rather than copying any host function table.
template<size_t... I> std::array<void*, sizeof...(I)> Refusals(std::index_sequence<I...>) {
    return {reinterpret_cast<void*>(&Unsupported<I>)...};
}

/// Records a validated method descriptor with its JVM lookup category.
struct Method {
    std::string signature;
    bool isStatic;
    bool constructor;
};

/// Describes only the JNI invocation table pointer and its host owner, never a host JavaVM.
struct VmProxy {
    void** functions;
    JniState* state;
};

/// Shares one thread's attachment ownership across ClassLoader-specific JNI environments.
struct Attachment {
    JavaVM* vm;
    bool owned = false;
    /// Detaches only after the last proxy is gone, never detaching a borrowed Java thread.
    ~Attachment() {
        if (owned && vm->DetachCurrentThread() != JNI_OK) Fatal("worker detach failed");
    }
};

/// Retains a host environment privately, tied to exactly one host thread.
struct EnvProxy {
    void** functions;
    JniState* state;
    JNIEnv* host;
    std::shared_ptr<Attachment> attachment;
    std::shared_ptr<ThreadBinding> binding;
    /// Detaches only bridge-owned worker attachments, after guest context/TLS destruction.
    ~EnvProxy();
};

/// Closes bindings while their environment identities remain visible to guest key/JNI callbacks.
struct ThreadEnvironments {
    std::vector<std::unique_ptr<EnvProxy>> entries;
    bool closing = false;
    /// Closes remaining proxies before thread-local storage and JVM attachment disappear.
    ~ThreadEnvironments();
    /// Removes one proxy only after the last binding has completed key/TLS destruction.
    void Remove(EnvProxy* proxy);
};
thread_local ThreadEnvironments environments;
std::mutex vmsMutex;
std::map<void*, JniState*> vms;

/// Validates environment identity against the calling thread's live proxies before dereferencing.
EnvProxy& Checked(void* pointer) {
    for (const auto& entry : environments.entries) if (entry.get() == pointer) return *entry;
    Fatal("foreign, detached or cross-thread JNIEnv");
}

/// Validates a guest VM identity without trusting the guest's object fields.
JniState& CheckedVm(void* pointer) {
    std::lock_guard lock(vmsMutex);
    const auto found = vms.find(pointer);
    if (found == vms.end()) Fatal("unknown JavaVM");
    return *found->second;
}

/// Throws an explicit JNI profile error without replacing an already pending Java exception.
void Error(JNIEnv* env, const char* reason) {
    if (env->ExceptionCheck()) return;
    jclass type = env->FindClass("java/lang/UnsatisfiedLinkError");
    if (type) {
        env->ThrowNew(type, reason);
        env->DeleteLocalRef(type);
    }
}

/// Validates one bounded JNI type and advances past its complete descriptor.
bool Type(const char*& at, bool allowVoid) {
    bool array = false;
    size_t dimensions = 0;
    while (*at == '[') { array = true; at++; if (++dimensions > 255) return false; }
    if (*at == 'L') {
        const char* start = ++at;
        while (*at && *at != ';') {
            if (*at == '.' || *at == '[' || *at == '(' || *at == ')') return false;
            at++;
        }
        if (at == start || *at != ';') return false;
        at++;
        return true;
    }
    const char value = *at;
    if (!value) return false;
    at++;
    return value == 'Z' || value == 'B' || value == 'C' || value == 'S' ||
           value == 'I' || value == 'J' || value == 'F' || value == 'D' ||
           (value == 'V' && allowVoid && !array);
}

/// Checks a JVM method signature before caching it or decoding bounded guest arguments.
bool Descriptor(const char* signature) {
    if (!signature || strnlen(signature, 4097) > 4096 || *signature++ != '(') return false;
    size_t count = 0;
    while (*signature != ')') if (++count > 64 || !Type(signature, false)) return false;
    signature++;
    return Type(signature, true) && !*signature;
}

/// Converts a method's reference/array return to the common JNI object-call category.
char Result(const std::string& descriptor) {
    const char type = descriptor[descriptor.find(')') + 1];
    return type == '[' ? 'L' : type;
}

}  // namespace

/// Owns one loader's tables, stable method metadata and screened guest helper mapping.
struct JniState {
    JavaVM* vm = nullptr;
    ThreadRuntime* threads = nullptr;
    jobject loader = nullptr;
    std::array<void*, kSlots> table = Refusals(std::make_index_sequence<kSlots>{});
    std::array<void*, 8> invocation = Refusals(std::make_index_sequence<8>{});
    VmProxy proxy{invocation.data(), this};
    std::unique_ptr<Module> helper;
    std::mutex mutex;
    std::map<jmethodID, Method> methods;
    std::atomic<size_t> proxies = 0;
};

namespace {

EnvProxy::~EnvProxy() {
    state->proxies--;
}

ThreadEnvironments::~ThreadEnvironments() {
    closing = true;
    while (!entries.empty()) Remove(entries.back().get());
}

void ThreadEnvironments::Remove(EnvProxy* proxy) {
    if (proxy->binding && proxy->binding.use_count() == 1) proxy->binding->Close();
    proxy->binding.reset();
    std::erase_if(entries, [proxy](const auto& entry) { return entry.get() == proxy; });
}

/// Finds or allocates one thread-local proxy, returning JNI_ENOMEM without an escaping exception.
EnvProxy* Environment(JniState& state, JNIEnv* host, bool attached) {
    for (const auto& entry : environments.entries) {
        if (entry->state != &state) continue;
        if (entry->host != host) Fatal("JNIEnv changed on an attached thread");
        return entry.get();
    }
    if (environments.closing) return nullptr;
    try {
        std::shared_ptr<Attachment> attachment;
        for (const auto& entry : environments.entries)
            if (entry->state->vm == state.vm) { attachment = entry->attachment; break; }
        if (!attachment) {
            attachment = std::make_shared<Attachment>();
            attachment->vm = state.vm;
        }
        auto binding = state.threads->Bind();
        auto entry = std::make_unique<EnvProxy>();
        entry->functions = state.table.data();
        entry->state = &state;
        entry->host = host;
        entry->attachment = attachment;
        entry->binding = std::move(binding);
        state.proxies++;
        auto* result = entry.get();
        environments.entries.push_back(std::move(entry));
        if (attached) attachment->owned = true;
        return result;
    } catch (const std::bad_alloc&) { return nullptr; }
}

/// Adapts one explicitly selected fixed JNI entry, substituting only the private host environment.
template<auto Member> struct Forward;
template<class R, class... A, R (JNICALL *JNINativeInterface_::*Member)(JNIEnv*, A...)>
struct Forward<Member> {
    /// Calls the host's fixed-signature entry after verifying the proxy's thread ownership.
    static R Call(void* guest, A... arguments) {
        auto* host = Checked(guest).host;
        return (host->functions->*Member)(host, arguments...);
    }
};

/// Reports only the guest table's JNI contract version, not the host JDK's newer slots.
jint Version(void* guest) { Checked(guest); return JNI_VERSION_1_6; }

/// Resolves app classes through the configured loader even on newly attached guest workers.
jclass FindClass(void* guest, const char* name) {
    auto& proxy = Checked(guest);
    if (!name || strnlen(name, 4097) > 4096) { Error(proxy.host, "GUEST_ABI_UNSUPPORTED JNI class name"); return nullptr; }
    if (!proxy.state->loader) return proxy.host->FindClass(name);
    try {
        std::string binary = name;
        for (char& c : binary) if (c == '/') c = '.';
        JNIEnv* env = proxy.host;
        jclass type = env->FindClass("java/lang/Class");
        jmethodID lookup = type ? env->GetStaticMethodID(type, "forName",
            "(Ljava/lang/String;ZLjava/lang/ClassLoader;)Ljava/lang/Class;") : nullptr;
        jstring text = lookup ? env->NewStringUTF(binary.c_str()) : nullptr;
        jvalue args[3]{};
        args[0].l = text;
        args[1].z = JNI_TRUE;  // JNI FindClass loads, links and initializes the named class.
        args[2].l = proxy.state->loader;
        auto result = text ? static_cast<jclass>(env->CallStaticObjectMethodA(type, lookup, args)) : nullptr;
        if (env->ExceptionCheck()) {
            // Class.forName reports CNFE, while JNI FindClass requires NoClassDefFoundError.
            jthrowable original = env->ExceptionOccurred();
            env->ExceptionClear();
            jclass missing = env->FindClass("java/lang/ClassNotFoundException");
            const bool notFound = missing && env->IsInstanceOf(original, missing);
            if (notFound) {
                jclass error = env->FindClass("java/lang/NoClassDefFoundError");
                if (error) {
                    env->ThrowNew(error, name);
                    env->DeleteLocalRef(error);
                }
            } else if (!env->ExceptionCheck()) {
                env->Throw(original);
            }
            if (missing) env->DeleteLocalRef(missing);
            env->DeleteLocalRef(original);
        }
        if (text) env->DeleteLocalRef(text);
        if (type) env->DeleteLocalRef(type);
        return result;
    } catch (const std::bad_alloc&) { Error(proxy.host, "GUEST_ABI_UNSUPPORTED JNI metadata allocation"); return nullptr; }
}

/// Records the exact signature for later A calls and guest-side va_arg decoding.
template<bool Static> jmethodID Lookup(void* guest, jclass type, const char* name, const char* signature) {
    auto& proxy = Checked(guest);
    if (!name || !Descriptor(signature)) { Error(proxy.host, "GUEST_ABI_UNSUPPORTED JNI method descriptor"); return nullptr; }
    auto method = Static ? proxy.host->GetStaticMethodID(type, name, signature) :
                           proxy.host->GetMethodID(type, name, signature);
    if (!method) return nullptr;  // Preserve the JVM's NoSuchMethodError.
    try {
        std::lock_guard lock(proxy.state->mutex);
        proxy.state->methods.emplace(method, Method{signature, Static, !std::strcmp(name, "<init>")});
        return method;
    } catch (const std::bad_alloc&) { Error(proxy.host, "GUEST_ABI_UNSUPPORTED JNI metadata allocation"); return nullptr; }
}

/// Returns stable descriptor bytes only for a known method, call category and result kind.
const char* Signature(void* guest, jmethodID method, jint mode, jint result) {
    auto& proxy = Checked(guest);
    std::lock_guard lock(proxy.state->mutex);
    const auto found = proxy.state->methods.find(method);
    if (found == proxy.state->methods.end() || found->second.isStatic != (mode == 2) ||
        mode < 0 || mode > 3 || Result(found->second.signature) != result ||
        found->second.constructor != (mode == 3) || (mode == 3 && result != 'V')) {
        Error(proxy.host, "GUEST_ABI_UNSUPPORTED JNI unknown method or call category");
        return nullptr;
    }
    return found->second.signature.c_str();
}

/// Returns the guest VM through a fixed signature, without exposing the JVM's host invocation table.
jint GetVm(void* guest, void** result) {
    if (!result) return JNI_ERR;
    *result = &Checked(guest).state->proxy;
    return JNI_OK;
}

/// Checks a supported guest invocation version before touching the host JVM.
bool VmVersion(jint version) {
    return version == JNI_VERSION_1_1 || version == JNI_VERSION_1_2 ||
           version == JNI_VERSION_1_4 || version == JNI_VERSION_1_6;
}

/// Returns a per-thread guest environment only when this host thread is already attached.
jint GetEnv(void* guestVm, void** output, jint version) {
    auto& state = CheckedVm(guestVm);
    if (!output) return JNI_ERR;
    *output = nullptr;
    if (!VmVersion(version)) return JNI_EVERSION;
    JNIEnv* env = nullptr;
    const jint status = state.vm->GetEnv(reinterpret_cast<void**>(&env), version);
    if (status != JNI_OK) return status;
    *output = Environment(state, env, false);
    return *output ? JNI_OK : JNI_ENOMEM;
}

/// Attaches a worker with explicit guest argument layout; already-attached threads retain ownership.
template<bool Daemon> jint Attach(void* guestVm, void** output, const void* arguments) {
    auto& state = CheckedVm(guestVm);
    if (!output) return JNI_ERR;
    *output = nullptr;
    JavaVMAttachArgs hostArgs{};
    if (arguments) {
        /// Describes the NDK's LP64 attachment fields; no Windows long or va_list is shared.
        struct GuestArgs { int32_t version; uint32_t padding; char* name; jobject group; };
        static_assert(sizeof(GuestArgs) == 24);
        const auto& guestArgs = *static_cast<const GuestArgs*>(arguments);
        if (!VmVersion(guestArgs.version)) return JNI_EVERSION;
        hostArgs = {guestArgs.version, guestArgs.name, guestArgs.group};
    }
    JNIEnv* env = nullptr;
    const jint existing = state.vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6);
    if (existing != JNI_OK && existing != JNI_EDETACHED) return existing;
    const bool owned = existing == JNI_EDETACHED;
    if (owned) {
        const jint status = Daemon ?
            state.vm->AttachCurrentThreadAsDaemon(reinterpret_cast<void**>(&env), arguments ? &hostArgs : nullptr) :
            state.vm->AttachCurrentThread(reinterpret_cast<void**>(&env), arguments ? &hostArgs : nullptr);
        if (status != JNI_OK) return status;
    }
    *output = Environment(state, env, owned);
    if (!*output && owned) state.vm->DetachCurrentThread();
    return *output ? JNI_OK : JNI_ENOMEM;
}

/// Detaches only a bridge-owned attachment, refusing borrowed Java threads and preserving their VM.
jint Detach(void* guestVm) {
    auto& state = CheckedVm(guestVm);
    for (const auto& entry : environments.entries) {
        if (entry->state->vm != state.vm) continue;
        if (!entry->attachment->owned) return JNI_ERR;
        jint status = state.vm->DetachCurrentThread();
        if (status != JNI_OK) return status;
        entry->attachment->owned = false;
        for (size_t i = 0; i < environments.entries.size();) {
            auto* proxy = environments.entries[i].get();
            if (proxy->state->vm == state.vm) environments.Remove(proxy);
            else i++;
        }
        return JNI_OK;
    }
    return JNI_OK;
}

/// Invokes an object constructor through the JVM A entry after validating its recorded signature.
jobject NewObject(void* guest, jclass type, jmethodID method, const jvalue* arguments) {
    if (!Signature(guest, method, 3, 'V')) return nullptr;
    return Checked(guest).host->NewObjectA(type, method, arguments);
}

// Every A entry below is a fixed signature. The Android helper supplies the ... and V entries.
#define CALL(Name, CType, Code) \
    /** Calls one checked instance method through its host A entry. */ \
    CType Call##Name(void* guest, jobject object, jmethodID method, const jvalue* args) { \
        if (!Signature(guest, method, 0, Code)) return CType{}; \
        return Checked(guest).host->Call##Name##MethodA(object, method, args); \
    } \
    /** Calls one checked nonvirtual method with an explicit declaring class. */ \
    CType Nonvirtual##Name(void* guest, jobject object, jclass type, jmethodID method, const jvalue* args) { \
        if (!Signature(guest, method, 1, Code)) return CType{}; \
        return Checked(guest).host->CallNonvirtual##Name##MethodA(object, type, method, args); \
    } \
    /** Calls one checked static method through its host A entry. */ \
    CType Static##Name(void* guest, jclass type, jmethodID method, const jvalue* args) { \
        if (!Signature(guest, method, 2, Code)) return CType{}; \
        return Checked(guest).host->CallStatic##Name##MethodA(type, method, args); \
    }
CALL(Object, jobject, 'L')
CALL(Boolean, jboolean, 'Z')
CALL(Byte, jbyte, 'B')
CALL(Char, jchar, 'C')
CALL(Short, jshort, 'S')
CALL(Int, jint, 'I')
CALL(Long, jlong, 'J')
CALL(Float, jfloat, 'F')
CALL(Double, jdouble, 'D')
// void needs no value-initialized failure expression.
#define VOID_CALL(Name, Parameters, Arguments, Mode) \
    /** Calls a checked void method without consuming or clearing a pending exception. */ \
    void Name(void* guest, Parameters, jmethodID method, const jvalue* args) { \
        if (Signature(guest, method, Mode, 'V')) Checked(guest).host->Name##MethodA(Arguments, method, args); \
    }
VOID_CALL(CallVoid, jobject object, object, 0)
// Nonvirtual's extra comma is handled explicitly.
/// Calls a checked nonvirtual void method through the host A entry.
void CallNonvirtualVoid(void* guest, jobject object, jclass type, jmethodID method, const jvalue* args) {
    if (Signature(guest, method, 1, 'V')) Checked(guest).host->CallNonvirtualVoidMethodA(object, type, method, args);
}
VOID_CALL(CallStaticVoid, jclass type, type, 2)
#undef VOID_CALL
#undef CALL

/// Installs audited fixed JNI entry wrappers and the explicit invocation proxies.
void Install(JniState& state) {
    for (size_t slot = 0; slot < 4; slot++) state.table[slot] = nullptr;
    for (size_t slot = 0; slot < 3; slot++) state.invocation[slot] = nullptr;
#define SET(Name, Entry) state.table[offsetof(JNINativeInterface_, Name) / sizeof(void*)] = reinterpret_cast<void*>(Entry)
#define FIXED(Name) SET(Name, &Forward<&JNINativeInterface_::Name>::Call)
    SET(GetVersion, &Version);
    SET(FindClass, &FindClass);
    SET(GetMethodID, &Lookup<false>);
    SET(GetStaticMethodID, &Lookup<true>);
    SET(GetJavaVM, &GetVm);
    SET(NewObjectA, &NewObject);
    FIXED(GetSuperclass); FIXED(IsAssignableFrom);
    FIXED(Throw); FIXED(ThrowNew); FIXED(ExceptionOccurred); FIXED(ExceptionDescribe);
    FIXED(ExceptionClear); FIXED(ExceptionCheck); FIXED(FatalError);
    FIXED(PushLocalFrame); FIXED(PopLocalFrame); FIXED(EnsureLocalCapacity);
    FIXED(NewGlobalRef); FIXED(DeleteGlobalRef); FIXED(DeleteLocalRef);
    FIXED(IsSameObject); FIXED(NewLocalRef); FIXED(GetObjectClass); FIXED(IsInstanceOf);
    FIXED(NewWeakGlobalRef); FIXED(DeleteWeakGlobalRef); FIXED(GetObjectRefType);
    FIXED(GetFieldID); FIXED(GetStaticFieldID);
    FIXED(NewString); FIXED(GetStringLength); FIXED(GetStringChars); FIXED(ReleaseStringChars);
    FIXED(NewStringUTF); FIXED(GetStringUTFLength); FIXED(GetStringUTFChars); FIXED(ReleaseStringUTFChars);
    FIXED(GetStringRegion); FIXED(GetStringUTFRegion);
    FIXED(GetArrayLength); FIXED(NewObjectArray); FIXED(GetObjectArrayElement); FIXED(SetObjectArrayElement);
#define FAMILY(Name) \
    SET(Call##Name##MethodA, &Call##Name); \
    SET(CallNonvirtual##Name##MethodA, &Nonvirtual##Name); \
    SET(CallStatic##Name##MethodA, &Static##Name); \
    FIXED(Get##Name##Field); FIXED(Set##Name##Field); \
    FIXED(GetStatic##Name##Field); FIXED(SetStatic##Name##Field)
    FAMILY(Object); FAMILY(Boolean); FAMILY(Byte); FAMILY(Char); FAMILY(Short);
    FAMILY(Int); FAMILY(Long); FAMILY(Float); FAMILY(Double);
#undef FAMILY
    SET(CallVoidMethodA, &CallVoid);
    SET(CallNonvirtualVoidMethodA, &CallNonvirtualVoid);
    SET(CallStaticVoidMethodA, &CallStaticVoid);
#define ARRAY(Name) \
    FIXED(New##Name##Array); FIXED(Get##Name##ArrayElements); FIXED(Release##Name##ArrayElements); \
    FIXED(Get##Name##ArrayRegion); FIXED(Set##Name##ArrayRegion)
    ARRAY(Boolean); ARRAY(Byte); ARRAY(Char); ARRAY(Short);
    ARRAY(Int); ARRAY(Long); ARRAY(Float); ARRAY(Double);
#undef ARRAY
#undef FIXED
#undef SET
    // DestroyJavaVM stays an explicit refusal; guests never own the process JVM.
    state.invocation[4] = reinterpret_cast<void*>(&Attach<false>);
    state.invocation[5] = reinterpret_cast<void*>(&Detach);
    state.invocation[6] = reinterpret_cast<void*>(&GetEnv);
    state.invocation[7] = reinterpret_cast<void*>(&Attach<true>);
}

}  // namespace

JniBridge::JniBridge() : state_(std::make_unique<JniState>()) {}

std::unique_ptr<JniBridge> JniBridge::Load(JNIEnv* env, jobject loader, ThreadRuntime& threads,
                                           const std::wstring& helper, LoadError* error) {
    try {
        auto bridge = std::unique_ptr<JniBridge>(new JniBridge);
        auto& state = *bridge->state_;
        state.threads = &threads;
        if (env->GetJavaVM(&state.vm) != JNI_OK) return nullptr;
        state.loader = loader ? env->NewGlobalRef(loader) : nullptr;
        if (loader && !state.loader) return nullptr;
        Install(state);
        auto fixed = ThreadRuntime::Imports();
        std::vector<HostSymbol> imports(fixed.begin(), fixed.end());
        imports.push_back({"winapk_jni_signature", reinterpret_cast<void*>(&Signature)});
        state.helper = guest::Load(helper, imports, error);
        if (!state.helper) return nullptr;
        auto install = reinterpret_cast<void (*)(void**)>(state.helper->Find("winapk_jni_install"));
        if (!install) {
            *error = {Category::SymbolUnresolved, "JNI helper", "missing winapk_jni_install"};
            return nullptr;
        }
        install(state.table.data());
        std::lock_guard lock(vmsMutex);
        vms.emplace(&state.proxy, &state);
        return bridge;
    } catch (const std::bad_alloc&) {
        *error = {Category::HostMemory, "JNI bridge", "proxy allocation failed"};
        return nullptr;
    }
}

JniBridge::~JniBridge() {
    for (size_t i = 0; i < environments.entries.size();) {
        auto* proxy = environments.entries[i].get();
        if (proxy->state == state_.get()) environments.Remove(proxy);
        else i++;
    }
    if (state_->proxies) Fatal("bridge destroyed with live worker environments");
    if (state_->loader) {
        JNIEnv* env = nullptr;
        if (state_->vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK)
            Fatal("bridge destruction requires an attached owner");
        env->DeleteGlobalRef(state_->loader);
    }
    std::lock_guard lock(vmsMutex);
    vms.erase(&state_->proxy);
}

void* JniBridge::Environment(JNIEnv* env) {
    JNIEnv* actual = nullptr;
    if (state_->vm->GetEnv(reinterpret_cast<void**>(&actual), JNI_VERSION_1_6) != JNI_OK || actual != env)
        Fatal("foreign host JNIEnv");
    return guest::Environment(*state_, env, false);
}

void* JniBridge::Call(JNIEnv* env, void* (*entry)(void*), void* argument) {
    void* proxy = Environment(env);
    if (!proxy) { Error(env, "GUEST_ABI_UNSUPPORTED JNI thread binding allocation"); return nullptr; }
    return Checked(proxy).binding->Call(entry, argument);
}

void* JniBridge::Vm() const { return &state_->proxy; }
size_t JniBridge::Environments() const { return state_->proxies; }

}  // namespace winapk::guest
