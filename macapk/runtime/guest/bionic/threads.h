// threads.h -- P3's managed guest thread foundation on Windows ARM64.
#pragma once

#include "../loader/elf_loader.h"

namespace winapk::guest {

struct ThreadState;
struct BindingState;

/// Retains one host thread's guest state across native entries and nested callbacks.
/// Notes: host-only; runtime/mappings outlive every binding; never move a binding across threads.
class ThreadBinding {
public:
    /// Closes an owned context, running key destructors and freeing TLS on its original thread.
    ~ThreadBinding();
    /// Enters the retained context or reuses its active instance for a nested fixed-signature call.
    void* Call(void* (*entry)(void*), void* argument);
    /// Closes key/TLS state while external JNI proxies are still valid; idempotent after closure.
    /// Notes: close only the last shared owner; an active ordinary Call cannot close its binding.
    void Close();

private:
    friend class ThreadRuntime;
    /// Borrows the current runtime context or allocates a persistent, initially inactive one.
    explicit ThreadBinding(ThreadState& state);
    std::unique_ptr<BindingState> state_;
};

/// Owns guest threads, TLS keys and synchronization tables without exposing host locks/handles.
/// Notes: host code only; all guest modules must outlive this runtime and every Call.
/// Only default attributes and normal entry-point return are supported; JNI owns attachment itself.
class ThreadRuntime {
public:
    /// Creates an empty registry with optional modules and an existing Android filesystem root.
    /// Notes: an absent root disables file opens; the supplied root is canonicalized, never created.
    explicit ThreadRuntime(ModuleSet* modules = nullptr, const std::wstring& androidRoot = {});
    /// Finalizes registered guest exit callbacks, then releases registries and private storage.
    /// Notes: modules outlive teardown; unjoined workers, active contexts or busy synchronization terminate.
    ~ThreadRuntime();
    ThreadRuntime(const ThreadRuntime&) = delete;
    ThreadRuntime& operator=(const ThreadRuntime&) = delete;

    /// Returns audited thread/errno/emutls, memory/string, file/time and dl imports, not arbitrary CRT exports.
    static std::span<const HostSymbol> Imports();

    /// Installs the host-owned mutable environment lookup backing guest getenv(3).
    static void SetEnvLookup(const char* (*lookup)(const char*));

    /// Calls a guest entry with a fresh context, then runs key destructors and frees emulated TLS.
    /// Inputs: entry -- the proven-compatible signature void*(void*).
    /// Notes: synchronous, not nested; modules stay mapped through cleanup. No guest exception
    /// may unwind into the host. Guest execution faults are not recovered here.
    void* Call(void* (*entry)(void*), void* argument);

    /// Returns this host thread's shared persistent binding, borrowing an existing Bionic worker.
    /// Notes: Bind may allocate; callers keep the binding until their host thread/proxy closes.
    std::shared_ptr<ThreadBinding> Bind();

    /// Reports whether contexts, TLS, keys, workers, allocations, fds and synchronization records are released.
    /// Notes: process-lifetime exit/atfork registrations may remain until finalization/teardown.
    bool Idle() const;

    /// Enables N2 process-lifetime thread auto-binding for host driver/audio callbacks entering guest code.
    void EnableN2AutoBind();

    /// Returns N2-only pthread/heap/dl/clock symbol overrides (`pthread_detach`, `pthread_cond_timedwait`,
    /// `posix_memalign`, `aligned_alloc`, `malloc_usable_size`, `dladdr`, `dl_iterate_phdr`, etc.).
    static std::span<const HostSymbol> N2Overrides();

private:
    std::unique_ptr<ThreadState> state_;
};

// Internal host-side helpers for N2 Bionic / NDK platform modules (`bionic_n2.cpp`, `ndk_platform.cpp`).
int32_t* GuestErrnoPtr();
void* GuestHeapMalloc(uint64_t size);
void* GuestHeapCalloc(uint64_t count, uint64_t size);
void* GuestHeapRealloc(void* ptr, uint64_t size);
void GuestHeapFree(void* ptr);
int GuestPosixMemalign(void** memptr, uint64_t alignment, uint64_t size);
void* GuestAlignedAlloc(uint64_t alignment, uint64_t size);
uint64_t GuestMallocUsableSize(const void* ptr);
ModuleSet* GuestActiveModules();
const std::wstring& GuestActiveAndroidRoot();
int32_t GuestLogWriteRaw(int32_t priority, const char* tag, const char* message);

}  // namespace winapk::guest
