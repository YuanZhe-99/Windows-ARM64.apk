// T08's host harness: no JVM/window, only fixed-signature calls into the audited guest fixture.
#include "threads.h"
#include "../../../../guest/bionic/threads.h"

#include <windows.h>

#include <atomic>
#include <cerrno>
#include <cstdio>
#include <filesystem>
#include <vector>

namespace {

std::atomic<size_t> callbacks = 0, violations = 0;

/// Checks host state on the calling worker without changing either platform-reserved register.
int HostState(const int32_t* guestErrno) {
    uintptr_t x18, sp;
    __asm__ volatile("mov %0, x18\n mov %1, sp" : "=r"(x18), "=r"(sp));
    const bool ok = x18 == reinterpret_cast<uintptr_t>(NtCurrentTeb()) && !(sp & 15) && guestErrno != &errno;
    callbacks++;
    if (!ok) violations++;
    return ok;
}

/// Checks the guest wall clock's Unix epoch and nanosecond units against the actual host clock.
int TimeCheck(int64_t seconds, int64_t nanoseconds) {
    FILETIME time{};
    GetSystemTimePreciseAsFileTime(&time);
    const int64_t now = static_cast<int64_t>((static_cast<uint64_t>(time.dwHighDateTime) << 32) | time.dwLowDateTime);
    const int64_t unixSeconds = (now - 116444736000000000LL) / 10000000;
    return nanoseconds >= 0 && nanoseconds < 1000000000 && nanoseconds % 100 == 0 &&
           seconds >= unixSeconds - 2 && seconds <= unixSeconds + 2;
}

}  // namespace

int ProbeBionic(const std::wstring& path, BionicSuite suite, const std::wstring& androidRoot) {
    using winapk::guest::ThreadRuntime;
    auto imports = ThreadRuntime::Imports();
    std::vector<winapk::guest::HostSymbol> symbols(imports.begin(), imports.end());
    symbols.push_back({"t08_host_state", reinterpret_cast<void*>(&HostState)});
    symbols.push_back({"t08_time_check", reinterpret_cast<void*>(&TimeCheck)});
    winapk::guest::LoadError error;
    auto module = winapk::guest::Load(path, symbols, &error);
    if (!module) {
        const auto name = std::filesystem::path(path).filename().u8string();
        std::printf("reject %s %s%s%s: %s\n", winapk::guest::CategoryName(error.category),
                    reinterpret_cast<const char*>(name.c_str()), error.where.empty() ? "" : " ",
                    error.where.c_str(), error.reason.c_str());
        return 2;
    }
    auto entry = reinterpret_cast<void* (*)(void*)>(module->Find("t08_case"));
    if (!entry) {
        std::puts("case T08.entry FAIL: missing t08_case");
        return 3;
    }
    // Registry destruction precedes module destruction, including every key callback.
    ThreadRuntime runtime(nullptr, androidRoot);
    const char* threadCases[] = {"threads8", "threads32", "destructor-limit", "deleted-keys", "key-exhaustion", "errors", "memset"};
    const char* syncCases[] = {"mutex-contention8", "mutex-kinds", "mutex-lifecycle", "cond-signal", "cond-broadcast", "cond-errors"};
    const char* memoryCases[] = {"memory-allocation", "memory-bytes", "memory-strings", "memory-sharing"};
    const char* ioCases[] = {"io-roundtrip", "io-flags", "io-refusals", "io-clocks"};
    const auto cases = suite == BionicSuite::Io ? std::span(ioCases) :
                       suite == BionicSuite::Memory ? std::span(memoryCases) :
                       suite == BionicSuite::Sync ? std::span(syncCases) : std::span<const char* const>(threadCases);
    int failed = 0;
    for (uintptr_t i = 0; i < std::size(cases); i++) {
        void* result = runtime.Call(entry, reinterpret_cast<void*>(i));
        const bool ok = !result && runtime.Idle();
        std::printf("case T08.%s %s: assertion %llu, registry %s\n", cases[i], ok ? "PASS" : "FAIL",
                    static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(result)), runtime.Idle() ? "empty" : "not empty");
        // Do not run more guest cases on a failed lifecycle; runtime destruction refuses
        // to release the registry if this entry left unjoined executable code behind.
        if (!ok) return 3;
    }

    const bool sync = suite == BionicSuite::Sync;
    // Repeated fresh contexts at one module address must reinitialize every emutls variable.
    // Memory/strings have bounded semantic cases, not another redundant stress loop.
    if (suite == BionicSuite::Threads || suite == BionicSuite::Sync) {
        DWORD before = 0, after = 0;
        for (int i = 0; i < 10; i++)
            if (runtime.Call(entry, nullptr) || !runtime.Idle()) failed++;
        GetProcessHandleCount(GetCurrentProcess(), &before);
        bool cycles = true;
        for (int i = 0; i < 200; i++)
            if (runtime.Call(entry, nullptr) || !runtime.Idle()) cycles = false;
        GetProcessHandleCount(GetCurrentProcess(), &after);
        const bool clean = cycles && after <= before;
        std::printf("case T08.%sstress %s: 200 cycles, 1600 workers, handle delta %+lld, registry %s\n",
                    sync ? "sync-" : "", clean ? "PASS" : "FAIL", static_cast<long long>(after) - before, runtime.Idle() ? "empty" : "not empty");
        failed += !clean;
    }
    const bool host = callbacks > 0 && violations == 0;
    std::printf("case T08.%shost-state %s: %zu callbacks, %zu x18/sp/errno violations\n",
                suite == BionicSuite::Io ? "io-" : suite == BionicSuite::Memory ? "memory-" : sync ? "sync-" : "",
                host ? "PASS" : "FAIL", callbacks.load(), violations.load());
    failed += !host;
    std::printf("result T08 %s\n", failed ? "FAIL" : "PASS");
    return failed ? 3 : 0;
}
