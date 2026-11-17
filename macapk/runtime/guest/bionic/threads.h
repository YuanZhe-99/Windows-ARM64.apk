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
    /// Creates an empty thread/key registry with an optional caller-owned guest module namespace.
    explicit ThreadRuntime(ModuleSet* modules = nullptr);
    /// Releases the registry; terminates for unjoined workers, active contexts or busy synchronization.
    ~ThreadRuntime();
    ThreadRuntime(const ThreadRuntime&) = delete;
    ThreadRuntime& operator=(const ThreadRuntime&) = delete;

    /// Returns audited thread/errno/emutls, dl API and compiler-required byte imports, not arbitrary CRT exports.
    static std::span<const HostSymbol> Imports();

    /// Calls a guest entry with a fresh context, then runs key destructors and frees emulated TLS.
    /// Inputs: entry -- the proven-compatible signature void*(void*).
    /// Notes: synchronous, not nested; modules stay mapped through cleanup. No guest exception
    /// may unwind into the host. Guest execution faults are not recovered here.
    void* Call(void* (*entry)(void*), void* argument);

    /// Reports whether all contexts, TLS blocks, keys, workers and synchronization records are released.
    bool Idle() const;

private:
    std::unique_ptr<ThreadState> state_;
};

}  // namespace winapk::guest
