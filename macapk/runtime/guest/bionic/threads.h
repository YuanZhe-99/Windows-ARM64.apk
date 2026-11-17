// threads.h -- P3's managed guest thread foundation on Windows ARM64.
#pragma once

#include "../loader/elf_loader.h"

namespace winapk::guest {

struct ThreadState;

/// Owns guest threads, TLS keys and synchronization tables without exposing host locks/handles.
/// Notes: host code only; all guest modules must outlive this runtime and every Call.
/// Only default attributes and normal entry-point return are supported. No JNI attachment yet.
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

    /// Calls a guest entry with a fresh context, then runs key destructors and frees emulated TLS.
    /// Inputs: entry -- the proven-compatible signature void*(void*).
    /// Notes: synchronous, not nested; modules stay mapped through cleanup. No guest exception
    /// may unwind into the host. Guest execution faults are not recovered here.
    void* Call(void* (*entry)(void*), void* argument);

    /// Reports whether contexts, TLS, keys, workers, allocations, fds and synchronization records are released.
    /// Notes: process-lifetime exit/atfork registrations may remain until finalization/teardown.
    bool Idle() const;

private:
    std::unique_ptr<ThreadState> state_;
};

}  // namespace winapk::guest
