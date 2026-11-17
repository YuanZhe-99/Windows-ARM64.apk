// Guest-PC-only fatal diagnostics; no exception recovery, JVM interception or ELF unwinding.
#include "faults.h"
#include "elf_loader.h"

#include <windows.h>

#include <algorithm>
#include <charconv>
#include <cstring>
#include <vector>

namespace winapk::guest {
namespace {

/// Retains stable image metadata while an exception handler reads it.
/// Notes: writers run only in host PE code and never hold this lock across a guest callback.
struct FaultRegistry {
    SRWLOCK lock = SRWLOCK_INIT;
    std::vector<const Module*> modules;
    PVOID handler = nullptr;

    /// Removes the Windows callback after all ordinary module/runtime lifetimes have ended.
    ~FaultRegistry() { if (handler) RemoveVectoredExceptionHandler(handler); }
};

/// Returns the process registry, initialized before the first Windows callback is installed.
FaultRegistry& Registry() {
    static FaultRegistry registry;
    return registry;
}

/// Diagnoses only recognized fatal exceptions whose PC is inside a live guest executable segment.
LONG CALLBACK GuestFault(EXCEPTION_POINTERS* exception) {
    const DWORD code = exception->ExceptionRecord->ExceptionCode;
    if (code != EXCEPTION_ACCESS_VIOLATION && code != EXCEPTION_ILLEGAL_INSTRUCTION &&
        code != EXCEPTION_PRIV_INSTRUCTION && code != EXCEPTION_BREAKPOINT) return EXCEPTION_CONTINUE_SEARCH;
    const uintptr_t pc = exception->ContextRecord->Pc;
    MEMORY_BASIC_INFORMATION information{};
    // A writer's PC is MEM_IMAGE. Never wait recursively for its registry lock on a host fault;
    // guest ELF code is MEM_PRIVATE, and writers never execute it while holding the lock.
    if (!VirtualQuery(reinterpret_cast<void*>(pc), &information, sizeof information) ||
        information.Type != MEM_PRIVATE) return EXCEPTION_CONTINUE_SEARCH;
    auto& registry = Registry();
    char name[256]{};
    uint64_t address = 0;
    bool found = false;
    AcquireSRWLockShared(&registry.lock);
    for (const auto* module : registry.modules) {
        if (!module->FaultAddress(pc, &address)) continue;
        const auto& moduleName = module->Name();
        std::memcpy(name, moduleName.data(), std::min(moduleName.size(), sizeof name - 1));
        found = true;
        break;
    }
    ReleaseSRWLockShared(&registry.lock);
    if (!found) return EXCEPTION_CONTINUE_SEARCH;

    char message[512];
    char* cursor = message;
    char* const end = message + sizeof message;
    // Stack-only diagnostic construction: no CRT formatting, heap allocation or guest callback.
    auto append = [&](std::string_view text) {
        const size_t size = std::min(text.size(), static_cast<size_t>(end - cursor));
        std::memcpy(cursor, text.data(), size);
        cursor += size;
    };
    auto hex = [&](uint64_t value) {
        const auto result = std::to_chars(cursor, end, value, 16);
        if (result.ec == std::errc{}) cursor = result.ptr;
    };
    append("GUEST_EXECUTION_FAULT ");
    append(name);
    append(" vaddr 0x");
    hex(address);
    append(": Windows exception 0x");
    hex(code);
    append("; terminating guest process\n");
    DWORD written = 0;
    WriteFile(GetStdHandle(STD_ERROR_HANDLE), message, static_cast<DWORD>(cursor - message), &written, nullptr);
    TerminateProcess(GetCurrentProcess(), 3);
    return EXCEPTION_CONTINUE_SEARCH;  // Only if Windows refuses to terminate its own process.
}

}  // namespace

bool TrackModule(const Module* module) {
    auto& registry = Registry();
    AcquireSRWLockExclusive(&registry.lock);
    if (!registry.handler) {
        // A JVM may terminate even for foreign native PCs before a tail handler is reached.
        // Only registered guest executable PCs are ours; all JVM/host/unknown PCs continue unchanged.
        registry.handler = AddVectoredExceptionHandler(1, GuestFault);
        if (!registry.handler) {
            ReleaseSRWLockExclusive(&registry.lock);
            return false;
        }
    }
    try {
        registry.modules.push_back(module);
    } catch (...) {
        ReleaseSRWLockExclusive(&registry.lock);
        throw;  // Host-only transaction: Load/ModuleSet catches allocation failure before execution.
    }
    ReleaseSRWLockExclusive(&registry.lock);
    return true;
}

void UntrackModule(const Module* module) {
    auto& registry = Registry();
    AcquireSRWLockExclusive(&registry.lock);
    const auto found = std::find(registry.modules.begin(), registry.modules.end(), module);
    if (found != registry.modules.end()) registry.modules.erase(found);
    ReleaseSRWLockExclusive(&registry.lock);
}

}  // namespace winapk::guest
