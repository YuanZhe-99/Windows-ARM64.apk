// Host-domain JNI adapters. Only fixed, ABI-audited signatures enter here from Android code.
#include "bridge.h"
#include "entries.h"
#include "../bionic/threads.h"

#include <windows.h>
#include <array>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <mutex>
#include <optional>
#include <set>
#include <tuple>
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

struct JniLibrary;
/// Retains one process-lifetime PE slot's screened target and descriptor-derived stack extent.
struct NativeSlot {
    JniState* state = nullptr;
    void* target = nullptr;
    uint64_t stackSlots = 0;
    JniLibrary* initialization = nullptr;
};
std::mutex nativesMutex;
std::array<NativeSlot, 64> natives;
size_t nextNative = 0;

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

/// Copies modified UTF-8 bytes while releasing JVM-owned string storage even on host allocation failure.
std::string Text(JNIEnv* env, jstring value) {
    if (!value) return {};
    const char* bytes = env->GetStringUTFChars(value, nullptr);
    if (!bytes) return {};
    try {
        std::string result(bytes);
        env->ReleaseStringUTFChars(value, bytes);
        return result;
    } catch (...) { env->ReleaseStringUTFChars(value, bytes); throw; }
}

/// Bounds reflection's local references without clearing a pending Java exception.
struct LocalFrame {
    JNIEnv* env;
    bool active;
    /// Opens a host JNI frame whose references never become guest persistent state.
    explicit LocalFrame(JNIEnv* value) : env(value), active(env->PushLocalFrame(64) == JNI_OK) {}
    /// Releases reflection temporaries while preserving pending exceptions.
    ~LocalFrame() { if (active) env->PopLocalFrame(nullptr); }
};

/// Reads a Class object's binary name without initializing that represented class.
std::string ClassName(JNIEnv* env, jclass type) {
    LocalFrame frame(env);
    if (!frame.active) return {};
    jclass cls = env->FindClass("java/lang/Class");
    jmethodID name = cls ? env->GetMethodID(cls, "getName", "()Ljava/lang/String;") : nullptr;
    auto text = name ? static_cast<jstring>(env->CallObjectMethodA(type, name, nullptr)) : nullptr;
    return env->ExceptionCheck() ? std::string{} : Text(env, text);
}

/// Identifies one declared native method within this bridge's unique ClassLoader.
using NativeKey = std::tuple<std::string, std::string, std::string>;

/// Separates explicit Java loading/JNI initialization from the ELF graph's constructor state.
struct JniLibrary {
    enum class Phase { Loading, Loaded, Failed };
    std::atomic<Phase> phase = Phase::Loading;
    DWORD owner = GetCurrentThreadId();
    uint64_t handle = 0;
    LoadError error;
};
thread_local std::pair<JniState*, JniLibrary*> initializingLibrary;

}  // namespace

/// Owns one loader's tables, stable method metadata and screened guest helper mapping.
struct JniState {
    JavaVM* vm = nullptr;
    ThreadRuntime* threads = nullptr;
    ModuleSet* modules = nullptr;
    jobject loader = nullptr;
    std::array<void*, kSlots> table = Refusals(std::make_index_sequence<kSlots>{});
    std::array<void*, 8> invocation = Refusals(std::make_index_sequence<8>{});
    VmProxy proxy{invocation.data(), this};
    std::unique_ptr<Module> helper;
    std::mutex mutex;
    std::map<jmethodID, Method> methods;
    std::atomic<size_t> proxies = 0;
    std::set<NativeKey> nativeKeys;
    std::map<std::string, JniLibrary> libraries;
    std::vector<uint64_t> roots;
    std::vector<size_t> nativeSlots;
    std::vector<jclass> nativeClasses;
    std::atomic<size_t> nativeCalls = 0;
    bool nativeClosing = false;
    DWORD closingThread = 0;
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

/// Counts only arguments beyond the fixed JNI integer/FP register banks, not JVM or C varargs.
uint64_t StackSlots(const char* descriptor) {
    unsigned integers = 2, floating = 0;
    uint64_t slots = 0;
    const char* at = descriptor + 1;
    while (*at != ')') {
        const bool fp = *at == 'F' || *at == 'D';
        Type(at, false);  // Descriptor was fully validated before this call.
        if (fp ? floating++ >= 8 : integers++ >= 8) slots++;
    }
    return slots;
}

/// Admits only classes actually defined by the bridge's configured app loader.
bool NativeClass(EnvProxy& proxy, jclass type) {
    JNIEnv* env = proxy.host;
    if (!proxy.state->loader) {
        Error(env, "GUEST_ABI_UNSUPPORTED JNI registration requires an app loader");
        return false;
    }
    jclass cls = env->FindClass("java/lang/Class");
    jmethodID lookup = cls ? env->GetMethodID(cls, "getClassLoader", "()Ljava/lang/ClassLoader;") : nullptr;
    jobject loader = lookup ? env->CallObjectMethodA(type, lookup, nullptr) : nullptr;
    const bool ok = !env->ExceptionCheck() && env->IsSameObject(loader, proxy.state->loader);
    if (loader) env->DeleteLocalRef(loader);
    if (cls) env->DeleteLocalRef(cls);
    if (!ok) Error(env, "GUEST_ABI_UNSUPPORTED JNI native class loader");
    return ok;
}

/// Describes Android's native registration fields explicitly, without reusing a host CRT object.
struct GuestNativeMethod { const char* name; const char* signature; void* target; };
static_assert(sizeof(GuestNativeMethod) == 24 && sizeof(JNINativeMethod) == 24);

/// Registers PE adapters for audited fixed JNI signatures; the JVM validates Java declarations.
jint Register(void* guest, jclass type, const GuestNativeMethod* methods, jint count) {
    auto& proxy = Checked(guest);
    auto& state = *proxy.state;
    JNIEnv* env = proxy.host;
    if (count < 0 || count > 64 || (count && !methods)) {
        Error(env, "GUEST_ABI_UNSUPPORTED JNI native registration count");
        return JNI_ERR;
    }
    if (!NativeClass(proxy, type)) return JNI_ERR;
    if (!count) return env->RegisterNatives(type, nullptr, 0);
    try {
        std::vector<JNINativeMethod> host(static_cast<size_t>(count));
        std::vector<uint64_t> stackSlots(static_cast<size_t>(count));
        for (jint i = 0; i < count; i++) {
            const auto& method = methods[i];
            bool executable = false;
            executable = state.modules->Executable(reinterpret_cast<uintptr_t>(method.target));
            if (!executable || !method.name || strnlen(method.name, 4097) > 4096 ||
                method.name[0] == '<' || !Descriptor(method.signature)) {
                Error(env, "GUEST_ABI_UNSUPPORTED JNI native entry or descriptor");
                return JNI_ERR;
            }
            stackSlots[i] = StackSlots(method.signature);
            host[i] = {const_cast<char*>(method.name), const_cast<char*>(method.signature), nullptr};
        }
        auto pinned = static_cast<jclass>(env->NewGlobalRef(type));
        if (!pinned) return JNI_ERR;
        bool reserved = false;
        try {
            std::lock_guard lock(nativesMutex);
            if (!state.nativeClosing && static_cast<size_t>(count) <= natives.size() - nextNative) {
                state.nativeSlots.reserve(state.nativeSlots.size() + count);
                state.nativeClasses.reserve(state.nativeClasses.size() + 1);
                for (jint i = 0; i < count; i++) {
                    const size_t slot = nextNative++;
                    natives[slot] = {&state, methods[i].target, stackSlots[i],
                        initializingLibrary.first == &state ? initializingLibrary.second : nullptr};
                    state.nativeSlots.push_back(slot);
                    host[i].fnPtr = winapk_jni_entries[slot];
                }
                state.nativeClasses.push_back(pinned);
                reserved = true;
            }
        } catch (...) { env->DeleteGlobalRef(pinned); throw; }
        if (!reserved) {
            env->DeleteGlobalRef(pinned);
            Error(env, "GUEST_ABI_UNSUPPORTED JNI native slot capacity or shutdown");
            return JNI_ERR;
        }
        const auto className = ClassName(env, type);
        if (env->ExceptionCheck()) return JNI_ERR;
        {
            std::lock_guard lock(state.mutex);
            for (jint i = 0; i < count; i++)
                state.nativeKeys.emplace(className, methods[i].name, methods[i].signature);
        }
        // The JVM validates existence/native modifiers without extra GetMethodID initialization.
        // Keep slots/classes pinned even on JVM failure: JNI may have bound part of the batch.
        return env->RegisterNatives(type, host.data(), count);
    } catch (const std::bad_alloc&) {
        Error(env, "GUEST_ABI_UNSUPPORTED JNI native registration allocation");
        return JNI_ERR;
    }
}

/// Holds a single fixed-signature guest invocation while its thread binding is active.
struct NativeCall { NativeSlot slot; EntryFrame* frame; };

/// Invokes the assembly's proven fixed JNI ABI, never a variadic host prototype.
void* InvokeNative(void* argument) {
    auto& call = *static_cast<NativeCall*>(argument);
    winapk_jni_invoke(call.slot.target, call.frame, call.slot.stackSlots);
    return nullptr;
}

/// Holds one explicit library request while the JNI bridge activates its persistent guest context.
struct LibraryCall {
    JniState& state;
    JNIEnv* env;
    std::string name;
    LoadError* error;
    bool ok = false;
};

/// Opens the ELF graph, then calls only the root's own JNI hook without holding any state lock.
void* InitializeLibrary(void* argument) {
    auto& call = *static_cast<LibraryCall*>(argument);
    auto& state = call.state;
    {
        std::lock_guard lock(state.mutex);
        auto found = state.libraries.find(call.name);
        if (found != state.libraries.end()) {
            if (found->second.phase == JniLibrary::Phase::Loaded) { call.ok = true; return nullptr; }
            *call.error = found->second.phase == JniLibrary::Phase::Failed ? found->second.error :
                LoadError{Category::AbiUnsupported, call.name, "recursive or competing JNI library initialization"};
            return nullptr;
        }
        state.libraries.try_emplace(call.name);
    }
    LoadError error;
    const uint64_t handle = state.modules->Open(call.name, &error);
    bool ok = handle != 0;
    if (ok) {
        {
            std::lock_guard lock(state.mutex);
            state.libraries.at(call.name).handle = handle;
        }
        auto onLoad = reinterpret_cast<jint (*)(void*, void*)>(state.modules->FindOwn(handle, "JNI_OnLoad"));
        if (onLoad && !state.modules->Executable(reinterpret_cast<uintptr_t>(onLoad))) {
            ok = false;
            error = {Category::AbiUnsupported, call.name, "JNI_OnLoad is not a guest executable entry"};
        } else if (onLoad) {
            /// Restores the outer explicit-load context after nested JNI hooks or callbacks.
            struct Initialization {
                std::pair<JniState*, JniLibrary*> previous = initializingLibrary;
                /// Associates registrations with this hook's final success/failure state.
                Initialization(JniState& state, const std::string& name) {
                    std::lock_guard lock(state.mutex);
                    initializingLibrary = {&state, &state.libraries.at(name)};
                }
                /// Restores any enclosing library's registration ownership.
                ~Initialization() { initializingLibrary = previous; }
            } initialization(state, call.name);
            jint version = onLoad(&state.proxy, nullptr);
            if (call.env->ExceptionCheck()) {
                ok = false;
                error = {Category::AbiUnsupported, call.name, "JNI_OnLoad left a pending Java exception"};
            } else if (!VmVersion(version)) {
                ok = false;
                error = {Category::AbiUnsupported, call.name, "unsupported JNI_OnLoad version " + std::to_string(version)};
            }
        }
    }
    {
        std::lock_guard lock(state.mutex);
        auto& library = state.libraries.at(call.name);
        library.phase = ok ? JniLibrary::Phase::Loaded : JniLibrary::Phase::Failed;
        library.error = error;
        if (ok) state.roots.push_back(handle);
    }
    call.ok = ok;
    if (!ok) *call.error = std::move(error);
    return nullptr;
}

/// Converts a reflected type name into a JNI descriptor without causing class initialization.
std::string TypeDescriptor(std::string name) {
    for (const auto& [primitive, code] : {
            std::pair{"void", "V"}, {"boolean", "Z"}, {"byte", "B"}, {"char", "C"},
            {"short", "S"}, {"int", "I"}, {"long", "J"}, {"float", "F"}, {"double", "D"}})
        if (name == primitive) return code;
    for (char& c : name) if (c == '.') c = '/';
    return name.starts_with('[') ? name : "L" + name + ";";
}

/// Escapes UTF-16 units exactly as JNI symbol names require, including individual surrogate units.
std::optional<std::string> Mangle(JNIEnv* env, jstring value, bool className = false) {
    const jsize length = env->GetStringLength(value);
    const jchar* text = env->GetStringChars(value, nullptr);
    if (!text) return std::nullopt;
    try {
        std::string result;
        constexpr char hex[] = "0123456789abcdef";
        for (jsize i = 0; i < length; i++) {
            jchar c = text[i];
            if (className && c == '.') c = '/';
            if (c >= '0' && c <= '3' && (result.empty() || result.back() == '_')) {
                env->ReleaseStringChars(value, text);
                return std::nullopt;  // JNI escape failure: do not search or bind an ambiguous name.
            }
            if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9'))
                result += static_cast<char>(c);
            else if (c == '/') result += '_';
            else if (c == '_') result += "_1";
            else if (c == ';') result += "_2";
            else if (c == '[') result += "_3";
            else {
                result += "_0";
                for (int shift : {12, 8, 4, 0}) result += hex[(c >> shift) & 15];
            }
        }
        env->ReleaseStringChars(value, text);
        return result;
    } catch (...) { env->ReleaseStringChars(value, text); throw; }
}

/// Retains native export metadata until one fixed-signature registration batch consumes it.
struct ExportMethod { std::string name, signature; void* target; };

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
    SET(RegisterNatives, &Register);
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
                                           ModuleSet& modules, const std::wstring& helper, LoadError* error) {
    try {
        auto bridge = std::unique_ptr<JniBridge>(new JniBridge);
        auto& state = *bridge->state_;
        state.threads = &threads;
        state.modules = &modules;
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
    {
        std::lock_guard lock(nativesMutex);
        state_->nativeClosing = true;
        state_->closingThread = GetCurrentThreadId();
        if (state_->nativeCalls) Fatal("bridge destroyed during a native call");
    }
    for (size_t i = 0; i < environments.entries.size();) {
        auto* proxy = environments.entries[i].get();
        if (proxy->state == state_.get()) environments.Remove(proxy);
        else i++;
    }
    if (state_->proxies) Fatal("bridge destroyed with live worker environments");
    if (!state_->nativeClasses.empty()) {
        JNIEnv* env = nullptr;
        if (state_->vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK)
            Fatal("native teardown requires an attached owner");
        for (jclass type : state_->nativeClasses) {
            if (env->UnregisterNatives(type) != JNI_OK) Fatal("native teardown unregister failed");
            env->DeleteGlobalRef(type);
        }
        std::lock_guard lock(nativesMutex);
        for (size_t slot : state_->nativeSlots) natives[slot].state = nullptr;
    }
    if (state_->loader) {
        JNIEnv* env = nullptr;
        if (state_->vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK)
            Fatal("bridge destruction requires an attached owner");
        env->DeleteGlobalRef(state_->loader);
    }
    for (const auto& [name, library] : state_->libraries) {
        (void)name;
        if (library.handle) state_->modules->Close(library.handle);
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

bool JniBridge::OpenLibrary(JNIEnv* env, std::string_view name, LoadError* error) {
    try {
        LibraryCall call{*state_, env, std::string(name), error};
        Call(env, InitializeLibrary, &call);
        return call.ok;
    } catch (const std::bad_alloc&) {
        std::lock_guard lock(state_->mutex);
        for (auto& [libraryName, library] : state_->libraries) {
            if (libraryName != name) continue;
            library.error = {Category::HostMemory, "", "out of memory"};
            library.phase = JniLibrary::Phase::Failed;
        }
        *error = {Category::HostMemory, "JNI library", "library lifecycle allocation failed"};
        return false;
    }
}

bool JniBridge::BindExports(JNIEnv* env, jclass type) {
    try {
        auto* proxy = static_cast<EnvProxy*>(Environment(env));
        if (!proxy || !NativeClass(*proxy, type)) return false;
        LocalFrame frame(env);
        if (!frame.active) return false;
        jclass cls = env->FindClass("java/lang/Class"), methodType = env->FindClass("java/lang/reflect/Method");
        if (!cls || !methodType) return false;
        jmethodID className = env->GetMethodID(cls, "getName", "()Ljava/lang/String;");
        jmethodID declared = env->GetMethodID(cls, "getDeclaredMethods", "()[Ljava/lang/reflect/Method;");
        jmethodID modifiers = env->GetMethodID(methodType, "getModifiers", "()I");
        jmethodID methodName = env->GetMethodID(methodType, "getName", "()Ljava/lang/String;");
        jmethodID parameters = env->GetMethodID(methodType, "getParameterTypes", "()[Ljava/lang/Class;");
        jmethodID resultType = env->GetMethodID(methodType, "getReturnType", "()Ljava/lang/Class;");
        if (env->ExceptionCheck()) return false;
        auto javaName = static_cast<jstring>(env->CallObjectMethodA(type, className, nullptr));
        if (env->ExceptionCheck() || !javaName) return false;
        const auto binaryName = Text(env, javaName);
        const auto encodedClass = Mangle(env, javaName, true);
        if (env->ExceptionCheck()) return false;
        if (!encodedClass) return true;  // Explicit registrations remain valid; unresolved methods stay unbound.
        const auto prefix = "Java_" + *encodedClass + "_";
        auto methods = static_cast<jobjectArray>(env->CallObjectMethodA(type, declared, nullptr));
        if (env->ExceptionCheck() || !methods) return false;
        std::vector<uint64_t> roots;
        { std::lock_guard lock(state_->mutex); roots = state_->roots; }
        std::vector<ExportMethod> exports;
        const jsize count = env->GetArrayLength(methods);
        for (jsize i = 0; i < count; i++) {
            LocalFrame item(env);
            if (!item.active) return false;
            jobject method = env->GetObjectArrayElement(methods, i);
            if (env->ExceptionCheck() || !method) return false;
            jint flags = env->CallIntMethodA(method, modifiers, nullptr);
            if (env->ExceptionCheck()) return false;
            if (!(flags & 0x0100)) continue;
            auto name = static_cast<jstring>(env->CallObjectMethodA(method, methodName, nullptr));
            if (env->ExceptionCheck()) return false;
            auto args = static_cast<jobjectArray>(env->CallObjectMethodA(method, parameters, nullptr));
            if (env->ExceptionCheck()) return false;
            auto result = static_cast<jclass>(env->CallObjectMethodA(method, resultType, nullptr));
            if (env->ExceptionCheck()) return false;
            std::string signature = "(";
            for (jsize n = 0, end = env->GetArrayLength(args); n < end; n++) {
                auto argumentType = static_cast<jclass>(env->GetObjectArrayElement(args, n));
                if (env->ExceptionCheck() || !argumentType) return false;
                signature += TypeDescriptor(ClassName(env, argumentType));
                env->DeleteLocalRef(argumentType);
                if (env->ExceptionCheck()) return false;
            }
            const std::string arguments = signature.substr(1);
            signature += ")" + TypeDescriptor(ClassName(env, result));
            const auto plainName = Text(env, name);
            if (env->ExceptionCheck()) return false;
            {
                std::lock_guard lock(state_->mutex);
                if (state_->nativeKeys.contains({binaryName, plainName, signature})) continue;
            }
            const auto encodedMethod = Mangle(env, name);
            if (env->ExceptionCheck()) return false;
            if (!encodedMethod) continue;
            const auto shortName = prefix + *encodedMethod;
            jstring descriptor = env->NewStringUTF(arguments.c_str());
            if (!descriptor) return false;
            const auto encodedArguments = Mangle(env, descriptor);
            if (env->ExceptionCheck()) return false;
            if (!encodedArguments) continue;
            const auto longName = shortName + "__" + *encodedArguments;
            void* target = nullptr;
            for (const auto& symbol : {shortName, longName}) {
                for (uint64_t root : roots)
                    if ((target = state_->modules->Find(root, symbol))) break;
                if (target) break;
            }
            if (env->ExceptionCheck()) return false;
            if (target) exports.push_back({plainName, std::move(signature), target});
        }
        std::vector<GuestNativeMethod> natives;
        for (const auto& method : exports) natives.push_back({method.name.c_str(), method.signature.c_str(), method.target});
        return natives.empty() || Register(proxy, type, natives.data(), static_cast<jint>(natives.size())) == JNI_OK;
    } catch (const std::bad_alloc&) {
        Error(env, "GUEST_ABI_UNSUPPORTED JNI export metadata allocation");
        return false;
    }
}

void* JniBridge::Vm() const { return &state_->proxy; }
size_t JniBridge::Environments() const { return state_->proxies; }

extern "C" void winapk_jni_dispatch(uint32_t slot, EntryFrame* frame) {
    NativeSlot target;
    {
        std::lock_guard lock(nativesMutex);
        if (slot >= nextNative || !natives[slot].state) Fatal("closed or invalid native slot", slot);
        target = natives[slot];
        if (target.state->nativeClosing &&
            (target.state->closingThread != GetCurrentThreadId() || !target.state->proxies))
            Fatal("native call during shutdown", slot);
        if (target.initialization &&
            (target.initialization->phase == JniLibrary::Phase::Failed ||
             (target.initialization->phase == JniLibrary::Phase::Loading &&
              target.initialization->owner != GetCurrentThreadId())))
            Fatal("native entry from failed or competing JNI_OnLoad", slot);
        target.state->nativeCalls++;
    }
    /// Keeps a dispatcher reservation alive until all nested guest and JNI calls have returned.
    struct ActiveCall {
        JniState& state;
        /// Releases the native-entry lifetime reservation, including a host allocation refusal.
        ~ActiveCall() { state.nativeCalls--; }
    } active{*target.state};
    auto* host = reinterpret_cast<JNIEnv*>(frame->integers[0]);
    JNIEnv* actual = nullptr;
    if (target.state->vm->GetEnv(reinterpret_cast<void**>(&actual), JNI_VERSION_1_6) != JNI_OK || actual != host)
        Fatal("native entry received a foreign host environment", slot);
    auto* proxy = Environment(*target.state, host, false);
    frame->integerResult = frame->floatingResult = 0;
    if (!proxy) { Error(host, "GUEST_ABI_UNSUPPORTED JNI native thread binding allocation"); return; }
    frame->integers[0] = reinterpret_cast<uintptr_t>(proxy);
    NativeCall call{target, frame};
    proxy->binding->Call(InvokeNative, &call);
}

}  // namespace winapk::guest
