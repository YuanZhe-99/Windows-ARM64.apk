// threads.cpp -- explicit Android LP64 pthread tokens, errno and LLVM emutls descriptors.
// Nothing here changes x18 or TPIDR_EL0, shares a CRT object, or generates executable thunks.
#include "threads.h"

#include <windows.h>
#include <process.h>
#include <malloc.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <condition_variable>
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
constexpr int kBusy = 16, kPermission = 1, kNoMemory = 12;
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

/// Owns host-only state for a guest mutex; every field is protected by ThreadState::mutex.
struct MutexState {
    uint64_t generation;
    int kind;  // Android NORMAL=0, RECURSIVE=1, ERRORCHECK=2.
    GuestThread owner = 0;
    uint32_t depth = 0;
    size_t contenders = 0, conditionWaiters = 0;
    std::condition_variable available;

    /// Creates a fresh unlocked mutex record with no host object in guest memory.
    MutexState(uint64_t id, int type) : generation(id), kind(type) {}
};

/// Owns a guest condition's wait queue and dynamic mutex binding, outside guest storage.
struct ConditionState {
    uint64_t generation;
    size_t waiters = 0;
    MutexState* binding = nullptr;
    std::condition_variable changed;

    /// Creates an untimed process-private condition record.
    explicit ConditionState(uint64_t id) : generation(id) {}
};

}  // namespace

/// Holds one runtime's synchronized token tables and exact cleanup counters.
struct ThreadState {
    std::mutex mutex;
    std::map<GuestThread, std::unique_ptr<Worker>> workers;
    std::array<Key, kKeys> keys{};
    std::map<void*, std::unique_ptr<MutexState>> mutexes;
    std::map<void*, std::unique_ptr<ConditionState>> conditions;
    uint64_t nextSync = 1;
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

// Android LP64 pthread_mutex_t / pthread_cond_t: 10 / 12 int32 words, alignment 4.
// Guest bytes contain only these private markers, never a host lock or an address.
constexpr uint32_t kMutexMarker = 0x57414d58, kConditionMarker = 0x57414356, kDestroyed = 0xffffffff;

/// Builds a guest-sized generation marker, with every reserved word explicitly zero.
template <size_t N>
std::array<uint32_t, N> Marker(uint32_t tag, uint64_t generation = 0) {
    std::array<uint32_t, N> words{};
    words[0] = tag;
    words[1] = static_cast<uint32_t>(generation);
    words[2] = static_cast<uint32_t>(generation >> 32);
    return words;
}

/// Reads opaque guest bytes without treating Android storage as a C++ host lock object.
template <size_t N>
std::array<uint32_t, N> Words(const void* object) {
    std::array<uint32_t, N> words;
    std::memcpy(words.data(), object, sizeof words);
    return words;
}

/// Writes only the guest-sized private marker, using its four-byte-aligned byte representation.
template <size_t N>
void Stamp(void* object, uint32_t tag, uint64_t generation = 0) {
    const auto words = Marker<N>(tag, generation);
    std::memcpy(object, words.data(), sizeof words);
}

/// Decodes NDK static initializer words, refusing unknown storage and unsupported features.
template <size_t N>
int StaticKind(const std::array<uint32_t, N>& words, int* kind) {
    if (std::any_of(words.begin() + 1, words.end(), [](uint32_t word) { return word != 0; })) return kInvalid;
    if constexpr (N == 10) {
        if (words[0] == 0 || words[0] == 0x4000 || words[0] == 0x8000) {
            *kind = static_cast<int>(words[0] >> 14);
            return 0;
        }
        if (words[0] == 0x2000 || words[0] == 0x6000 || words[0] == 0xa000) return kNotSupported;
    } else {
        if (words[0] == 0) { *kind = 0; return 0; }
        if (words[0] <= 3) return kNotSupported;  // process-shared and/or monotonic
    }
    return kInvalid;
}

/// Looks up or lazily creates a mutex record while the caller holds the registry lock.
MutexState* GetMutex(ThreadState& runtime, void* object, int* error) {
    if (!object) { *error = kInvalid; return nullptr; }
    const auto words = Words<10>(object);
    auto it = runtime.mutexes.find(object);
    if (it != runtime.mutexes.end()) {
        if (words == Marker<10>(kMutexMarker, it->second->generation)) return it->second.get();
        *error = kInvalid;
        return nullptr;
    }
    int kind;
    *error = StaticKind(words, &kind);
    if (*error) return nullptr;
    if (runtime.nextSync == UINT64_MAX) { *error = kAgain; return nullptr; }
    const uint64_t generation = runtime.nextSync++;
    auto mutex = std::make_unique<MutexState>(generation, kind);
    auto* result = mutex.get();
    runtime.mutexes.emplace(object, std::move(mutex));
    Stamp<10>(object, kMutexMarker, generation);
    return result;
}

/// Looks up or lazily creates an untimed condition while the registry lock is held.
ConditionState* GetCondition(ThreadState& runtime, void* object, int* error) {
    if (!object) { *error = kInvalid; return nullptr; }
    const auto words = Words<12>(object);
    auto it = runtime.conditions.find(object);
    if (it != runtime.conditions.end()) {
        if (words == Marker<12>(kConditionMarker, it->second->generation)) return it->second.get();
        *error = kInvalid;
        return nullptr;
    }
    int kind;
    *error = StaticKind(words, &kind);
    if (*error) return nullptr;
    if (runtime.nextSync == UINT64_MAX) { *error = kAgain; return nullptr; }
    const uint64_t generation = runtime.nextSync++;
    auto condition = std::make_unique<ConditionState>(generation);
    auto* result = condition.get();
    runtime.conditions.emplace(object, std::move(condition));
    Stamp<12>(object, kConditionMarker, generation);
    return result;
}

/// Initializes a normal guest mutex, preserving storage on unsupported attributes or allocation failure.
int MutexInit(void* object, const void* attributes) {
    auto& runtime = Context().runtime;
    if (!object) return kInvalid;
    if (attributes) return kNotSupported;
    try {
        std::lock_guard lock(runtime.mutex);
        if (runtime.mutexes.contains(object)) return kBusy;
        if (runtime.nextSync == UINT64_MAX) return kAgain;
        const uint64_t generation = runtime.nextSync++;
        auto mutex = std::make_unique<MutexState>(generation, 0);
        runtime.mutexes.emplace(object, std::move(mutex));
        Stamp<10>(object, kMutexMarker, generation);
        return 0;
    } catch (const std::bad_alloc&) { return kNoMemory; }
}

/// Acquires logical mutex ownership; pins contended records until their waits end.
int Acquire(void* object, bool attempt) {
    auto& context = Context();
    try {
        std::unique_lock lock(context.runtime.mutex);
        int error = 0;
        auto* mutex = GetMutex(context.runtime, object, &error);
        if (!mutex) return error;
        if (mutex->owner == context.token && mutex->kind == 1) {
            if (mutex->depth == 2048) return kAgain;  // Bionic non-PI recursion counter: 11 bits, depth minus one.
            mutex->depth++;
            return 0;
        }
        if (attempt && mutex->owner) return kBusy;
        if (mutex->owner == context.token && mutex->kind == 2) return kDeadlock;
        mutex->contenders++;
        while (mutex->owner) mutex->available.wait(lock);
        mutex->contenders--;
        mutex->owner = context.token;
        mutex->depth = 1;
        return 0;
    } catch (const std::bad_alloc&) { return kNoMemory; }
      catch (const std::system_error&) { Fatal("host mutex wait failed"); }
}

/// Blocks for a guest mutex, preserving normal/recursive/error-checking relock semantics.
int MutexLock(void* object) { return Acquire(object, false); }

/// Attempts a guest mutex acquisition without waiting, returning Linux EBUSY when owned.
int MutexTryLock(void* object) { return Acquire(object, true); }

/// Releases one level of guest ownership and wakes a contender only on the final unlock.
int MutexUnlock(void* object) {
    auto& context = Context();
    try {
        std::lock_guard lock(context.runtime.mutex);
        int error = 0;
        auto* mutex = GetMutex(context.runtime, object, &error);
        if (!mutex) return error;
        if (mutex->owner != context.token) return kPermission;
        if (--mutex->depth == 0) {
            mutex->owner = 0;
            mutex->available.notify_one();
        }
        return 0;
    } catch (const std::bad_alloc&) { return kNoMemory; }
}

/// Destroys only an unowned mutex with no lock/condition waiters; leaves a tombstone in guest storage.
int MutexDestroy(void* object) {
    auto& runtime = Context().runtime;
    if (!object) return kInvalid;
    std::lock_guard lock(runtime.mutex);
    auto it = runtime.mutexes.find(object);
    if (it == runtime.mutexes.end()) {
        int kind;
        if (int error = StaticKind(Words<10>(object), &kind)) return error;
    } else {
        auto& mutex = *it->second;
        if (Words<10>(object) != Marker<10>(kMutexMarker, mutex.generation)) return kInvalid;
        if (mutex.owner || mutex.contenders || mutex.conditionWaiters) return kBusy;
        runtime.mutexes.erase(it);
    }
    Stamp<10>(object, kDestroyed);
    return 0;
}

/// Initializes an untimed process-private condition without sharing host lock bytes.
int ConditionInit(void* object, const void* attributes) {
    auto& runtime = Context().runtime;
    if (!object) return kInvalid;
    if (attributes) return kNotSupported;
    try {
        std::lock_guard lock(runtime.mutex);
        if (runtime.conditions.contains(object)) return kBusy;
        if (runtime.nextSync == UINT64_MAX) return kAgain;
        const uint64_t generation = runtime.nextSync++;
        auto condition = std::make_unique<ConditionState>(generation);
        runtime.conditions.emplace(object, std::move(condition));
        Stamp<12>(object, kConditionMarker, generation);
        return 0;
    } catch (const std::bad_alloc&) { return kNoMemory; }
}

/// Atomically releases a guest mutex for a condition wait and reacquires it before returning.
int ConditionWait(void* object, void* mutexObject) {
    auto& context = Context();
    try {
        std::unique_lock lock(context.runtime.mutex);
        int error = 0;
        auto* mutex = GetMutex(context.runtime, mutexObject, &error);
        if (!mutex) return error;
        if (mutex->owner != context.token) return kPermission;
        if (mutex->depth != 1) return kNotSupported;
        auto* condition = GetCondition(context.runtime, object, &error);
        if (!condition) return error;
        if (condition->waiters && condition->binding != mutex) return kInvalid;
        condition->binding = mutex;
        condition->waiters++;
        mutex->conditionWaiters++;
        mutex->owner = 0;
        mutex->depth = 0;
        mutex->available.notify_one();
        condition->changed.wait(lock);  // Predicate belongs to the guest; spurious wakes are legal.
        while (mutex->owner) mutex->available.wait(lock);
        mutex->owner = context.token;
        mutex->depth = 1;
        mutex->conditionWaiters--;
        if (--condition->waiters == 0) condition->binding = nullptr;
        return 0;
    } catch (const std::bad_alloc&) { return kNoMemory; }
      catch (const std::system_error&) { Fatal("host condition wait failed"); }
}

/// Wakes one or all current condition waiters without retaining a notification for future waits.
int Wake(void* object, bool all) {
    auto& runtime = Context().runtime;
    try {
        std::lock_guard lock(runtime.mutex);
        int error = 0;
        auto* condition = GetCondition(runtime, object, &error);
        if (!condition) return error;
        if (all) condition->changed.notify_all();
        else condition->changed.notify_one();
        return 0;
    } catch (const std::bad_alloc&) { return kNoMemory; }
}

/// Signals one guest condition waiter, if any.
int ConditionSignal(void* object) { return Wake(object, false); }

/// Broadcasts to every current guest condition waiter.
int ConditionBroadcast(void* object) { return Wake(object, true); }

/// Destroys only a condition with no waiter still referencing its host record.
int ConditionDestroy(void* object) {
    auto& runtime = Context().runtime;
    if (!object) return kInvalid;
    std::lock_guard lock(runtime.mutex);
    auto it = runtime.conditions.find(object);
    if (it == runtime.conditions.end()) {
        int kind;
        if (int error = StaticKind(Words<12>(object), &kind)) return error;
    } else {
        auto& condition = *it->second;
        if (Words<12>(object) != Marker<12>(kConditionMarker, condition.generation)) return kInvalid;
        if (condition.waiters) return kBusy;
        runtime.conditions.erase(it);
    }
    Stamp<12>(object, kDestroyed);
    return 0;
}

}  // namespace

ThreadRuntime::ThreadRuntime() : state_(std::make_unique<ThreadState>()) {}

ThreadRuntime::~ThreadRuntime() {
    std::lock_guard lock(state_->mutex);
    if (!state_->workers.empty() || state_->contexts != 0)
        Fatal("runtime destroyed with unjoined workers or active contexts");
    for (const auto& [object, mutex] : state_->mutexes) {
        (void)object;
        if (mutex->owner || mutex->contenders || mutex->conditionWaiters)
            Fatal("runtime destroyed with owned or waited-on synchronization objects");
    }
    for (const auto& [object, condition] : state_->conditions) {
        (void)object;
        if (condition->waiters) Fatal("runtime destroyed with owned or waited-on synchronization objects");
    }
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
        {"pthread_mutex_init", reinterpret_cast<void*>(&MutexInit)},
        {"pthread_mutex_lock", reinterpret_cast<void*>(&MutexLock)},
        {"pthread_mutex_trylock", reinterpret_cast<void*>(&MutexTryLock)},
        {"pthread_mutex_unlock", reinterpret_cast<void*>(&MutexUnlock)},
        {"pthread_mutex_destroy", reinterpret_cast<void*>(&MutexDestroy)},
        {"pthread_cond_init", reinterpret_cast<void*>(&ConditionInit)},
        {"pthread_cond_wait", reinterpret_cast<void*>(&ConditionWait)},
        {"pthread_cond_signal", reinterpret_cast<void*>(&ConditionSignal)},
        {"pthread_cond_broadcast", reinterpret_cast<void*>(&ConditionBroadcast)},
        {"pthread_cond_destroy", reinterpret_cast<void*>(&ConditionDestroy)},
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
           state_->mutexes.empty() && state_->conditions.empty() &&
           std::all_of(state_->keys.begin(), state_->keys.end(), [](const Key& key) { return key.token == 0; });
}

}  // namespace winapk::guest
