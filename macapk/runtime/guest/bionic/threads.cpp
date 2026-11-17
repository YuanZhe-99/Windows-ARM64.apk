// threads.cpp -- explicit Android LP64 pthread tokens, errno and LLVM emutls descriptors.
// Nothing here changes x18 or TPIDR_EL0, shares a CRT object, or generates executable thunks.
#include "threads.h"

#include <windows.h>
#include <process.h>
#include <malloc.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <map>
#include <mutex>
#include <new>
#include <vector>

namespace winapk::guest {
namespace {

// Linux/Bionic values, deliberately not the host's errno macros.
constexpr int kAgain = 11, kInvalid = 22, kDeadlock = 35, kNoThread = 3, kNotSupported = 95;
constexpr size_t kKeys = 128;
using GuestThread = int64_t;  // Android pthread_t is long, not Windows long.
using GuestKey = int32_t;
using Entry = void* (*)(void*);
using Destructor = void (*)(void*);

/// Terminates on a broken guest execution contract instead of unwinding through ELF code.
[[noreturn]] void Fatal(const char* reason) {
    std::fprintf(stderr, "GUEST_ABI_UNSUPPORTED thread runtime: %s\n", reason);
    std::fflush(stderr);
    TerminateProcess(GetCurrentProcess(), 3);
    std::abort();
}

/// Describes one LLVM AArch64 emulated TLS variable, using guest-sized fields only.
struct EmutlsControl {
    uint64_t size, alignment, object;
    const void* initializer;
};
static_assert(sizeof(EmutlsControl) == 32 && offsetof(EmutlsControl, initializer) == 24);

/// Records an allocated key's generation token and guest destructor.
struct Key {
    GuestKey token = 0;
    Destructor destructor = nullptr;
};

/// Keeps a thread-specific value associated with the key generation that set it.
struct Value {
    GuestKey token = 0;
    void* pointer = nullptr;
};

/// Owns one aligned emutls allocation in a single guest context.
struct TlsBlock {
    const EmutlsControl* control;
    void* pointer;
};

/// Keeps the host thread handle and its full pointer-sized return value outside guest memory.
struct Worker {
    ThreadState* runtime;
    GuestThread token;
    Entry entry;
    void* argument;
    HANDLE handle = nullptr;
    void* result = nullptr;
    bool joining = false;
    /// Closes the thread handle only after its owner has waited for exit.
    ~Worker() { if (handle) CloseHandle(handle); }
};

}  // namespace

/// Holds one runtime's synchronized token tables and exact cleanup counters.
struct ThreadState {
    std::mutex mutex;
    std::map<GuestThread, std::unique_ptr<Worker>> workers;
    std::array<Key, kKeys> keys{};
    GuestThread nextThread = 1;
    GuestKey nextKey = 1;
    std::atomic<size_t> contexts = 0, tlsBlocks = 0;
};

namespace {

/// Holds guest-only state for one host thread, never in a guest hardware thread pointer.
struct GuestThreadContext {
    ThreadState& runtime;
    GuestThread token;
    int32_t error = 0;
    std::array<Value, kKeys> values{};
    std::vector<TlsBlock> tls;

    /// Creates a context before guest entry, making it visible to audited shims.
    GuestThreadContext(ThreadState& state, GuestThread id);
    /// Runs key destructors while TLS is alive, then frees TLS before ending the context.
    ~GuestThreadContext();
};

thread_local GuestThreadContext* current = nullptr;

/// Returns the calling managed context; an unmanaged import is a fatal contract violation.
GuestThreadContext& Context() {
    if (!current) Fatal("shim called without a managed guest context");
    return *current;
}

GuestThreadContext::GuestThreadContext(ThreadState& state, GuestThread id) : runtime(state), token(id) {
    if (current) Fatal("nested fresh guest context");
    current = this;
    runtime.contexts++;
}

GuestThreadContext::~GuestThreadContext() {
    // Snapshot tokens, not table iterators: callbacks may create/delete keys or set values.
    for (int round = 0; round < 4; round++) {
        const auto pending = values;
        for (size_t i = 0; i < kKeys; i++) {
            const Value v = pending[i];
            if (!v.pointer) continue;
            Destructor destructor = nullptr;
            {
                std::lock_guard lock(runtime.mutex);
                if (runtime.keys[i].token != v.token || values[i].token != v.token ||
                    values[i].pointer != v.pointer) continue;
                destructor = runtime.keys[i].destructor;
                values[i].pointer = nullptr;
            }
            if (destructor) destructor(v.pointer);
        }
    }
    for (const TlsBlock& block : tls) {
        _aligned_free(block.pointer);
        runtime.tlsBlocks--;
    }
    runtime.contexts--;
    current = nullptr;
}

/// Returns this context's guest errno address, not the host CRT's errno address.
int32_t* Errno() { return &Context().error; }

/// Returns the guest's opaque 64-bit thread token.
GuestThread Self() { return Context().token; }

/// Compares two Android thread tokens with a fixed integer signature.
int Equal(GuestThread a, GuestThread b) { return a == b; }

/// Fills guest bytes through an audited fixed signature (Android size_t is 64-bit).
void* BytesSet(void* destination, int32_t value, uint64_t size) {
    return std::memset(destination, value, static_cast<size_t>(size));
}

/// Enters a new guest context on a CRT-initialized host worker and publishes its return value.
unsigned __stdcall Start(void* pointer) {
    auto& worker = *static_cast<Worker*>(pointer);
    {
        GuestThreadContext context(*worker.runtime, worker.token);
        worker.result = worker.entry(worker.argument);
    }
    return 0;
}

/// Creates a default joinable worker, returning Linux errors without changing guest errno.
int Create(GuestThread* token, const void* attributes, Entry entry, void* argument) {
    auto& runtime = Context().runtime;
    if (!token || !entry) return kInvalid;
    if (attributes) return kNotSupported;
    try {
        std::lock_guard lock(runtime.mutex);
        if (runtime.nextThread == std::numeric_limits<GuestThread>::max()) return kAgain;
        const GuestThread id = runtime.nextThread++;
        auto worker = std::make_unique<Worker>();
        worker->runtime = &runtime;
        worker->token = id;
        worker->entry = entry;
        worker->argument = argument;
        auto [it, inserted] = runtime.workers.emplace(id, std::move(worker));
        (void)inserted;
        // The table owns the worker before it can start, and no throwing operation follows.
        it->second->handle = reinterpret_cast<HANDLE>(_beginthreadex(nullptr, 0, Start, it->second.get(), 0, nullptr));
        if (!it->second->handle) {
            runtime.workers.erase(it);
            return kAgain;
        }
        *token = id;
        return 0;
    } catch (const std::bad_alloc&) {
        return kAgain;
    }
}

/// Joins one worker without holding the registry lock across guest execution or destruction.
int Join(GuestThread token, void** result) {
    auto& context = Context();
    if (token == context.token) return kDeadlock;
    Worker* worker;
    {
        std::lock_guard lock(context.runtime.mutex);
        auto it = context.runtime.workers.find(token);
        if (it == context.runtime.workers.end()) return kNoThread;
        worker = it->second.get();
        if (worker->joining) return kInvalid;
        worker->joining = true;
    }
    if (WaitForSingleObject(worker->handle, INFINITE) != WAIT_OBJECT_0)
        Fatal("waiting for a managed worker failed");
    if (result) *result = worker->result;
    std::lock_guard lock(context.runtime.mutex);
    context.runtime.workers.erase(token);
    return 0;
}

/// Allocates a generation-tagged key, with the Android limit of 128 simultaneous keys.
int KeyCreate(GuestKey* token, Destructor destructor) {
    if (!token) return kInvalid;
    auto& runtime = Context().runtime;
    std::lock_guard lock(runtime.mutex);
    if (runtime.nextKey == std::numeric_limits<GuestKey>::max()) return kAgain;
    for (auto& key : runtime.keys) {
        if (key.token) continue;
        key = {runtime.nextKey++, destructor};
        *token = key.token;
        return 0;
    }
    return kAgain;
}

/// Deletes a key without calling its destructor or reviving stale per-thread values.
int KeyDelete(GuestKey token) {
    auto& runtime = Context().runtime;
    std::lock_guard lock(runtime.mutex);
    for (auto& key : runtime.keys) {
        if (key.token != token || !token) continue;
        key = {};
        return 0;
    }
    return kInvalid;
}

/// Sets one thread's value only when its generation token still names a live key.
int SetSpecific(GuestKey token, const void* pointer) {
    auto& context = Context();
    std::lock_guard lock(context.runtime.mutex);
    for (size_t i = 0; i < kKeys; i++) {
        if (context.runtime.keys[i].token != token || !token) continue;
        context.values[i] = {token, const_cast<void*>(pointer)};
        return 0;
    }
    return kInvalid;
}

/// Gets one thread's value; invalid/deleted/unset keys have no associated value.
void* GetSpecific(GuestKey token) {
    auto& context = Context();
    std::lock_guard lock(context.runtime.mutex);
    for (size_t i = 0; i < kKeys; i++) {
        if (context.runtime.keys[i].token == token && token && context.values[i].token == token)
            return context.values[i].pointer;
    }
    return nullptr;
}

/// Resolves C emulated TLS from an explicit LLVM descriptor without touching hardware TLS.
void* EmutlsAddress(const EmutlsControl* control) {
    auto& context = Context();
    if (!control || !control->size || !control->alignment ||
        (control->alignment & (control->alignment - 1)) ||
        control->size > SIZE_MAX - control->alignment)
        Fatal("invalid emulated TLS descriptor");
    for (const auto& block : context.tls)
        if (block.control == control) return block.pointer;
    void* pointer = _aligned_malloc(static_cast<size_t>(control->size),
                                   static_cast<size_t>(std::max<uint64_t>(control->alignment, sizeof(void*))));
    if (!pointer) Fatal("emulated TLS allocation failed");
    if (control->initializer) std::memcpy(pointer, control->initializer, static_cast<size_t>(control->size));
    else std::memset(pointer, 0, static_cast<size_t>(control->size));
    try {
        context.tls.push_back({control, pointer});
    } catch (const std::bad_alloc&) {
        _aligned_free(pointer);
        Fatal("emulated TLS registry allocation failed");
    }
    context.runtime.tlsBlocks++;
    return pointer;
}

}  // namespace

ThreadRuntime::ThreadRuntime() : state_(std::make_unique<ThreadState>()) {}

ThreadRuntime::~ThreadRuntime() {
    std::lock_guard lock(state_->mutex);
    if (!state_->workers.empty() || state_->contexts != 0)
        Fatal("runtime destroyed with unjoined workers or active contexts");
}

std::span<const HostSymbol> ThreadRuntime::Imports() {
    static const HostSymbol imports[] = {
        {"__errno", reinterpret_cast<void*>(&Errno)},
        {"pthread_create", reinterpret_cast<void*>(&Create)},
        {"pthread_join", reinterpret_cast<void*>(&Join)},
        {"pthread_self", reinterpret_cast<void*>(&Self)},
        {"pthread_equal", reinterpret_cast<void*>(&Equal)},
        {"pthread_key_create", reinterpret_cast<void*>(&KeyCreate)},
        {"pthread_key_delete", reinterpret_cast<void*>(&KeyDelete)},
        {"pthread_setspecific", reinterpret_cast<void*>(&SetSpecific)},
        {"pthread_getspecific", reinterpret_cast<void*>(&GetSpecific)},
        {"__emutls_get_address", reinterpret_cast<void*>(&EmutlsAddress)},
        {"memset", reinterpret_cast<void*>(&BytesSet)},
    };
    return imports;
}

void* ThreadRuntime::Call(Entry entry, void* argument) {
    GuestThread token;
    {
        std::lock_guard lock(state_->mutex);
        if (state_->nextThread == std::numeric_limits<GuestThread>::max()) Fatal("thread token space exhausted");
        token = state_->nextThread++;
    }
    GuestThreadContext context(*state_, token);
    return entry(argument);
}

bool ThreadRuntime::Idle() const {
    std::lock_guard lock(state_->mutex);
    return state_->workers.empty() && state_->contexts == 0 && state_->tlsBlocks == 0 &&
           std::all_of(state_->keys.begin(), state_->keys.end(), [](const Key& key) { return key.token == 0; });
}

}  // namespace winapk::guest
