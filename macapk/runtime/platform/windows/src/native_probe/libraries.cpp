#include "libraries.h"
#include "../../../../guest/bionic/libraries.h"
#include "../../../../guest/bionic/threads.h"

#include <windows.h>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <tuple>

namespace {
int trace = 0, violations = 0, checked = 0;

/// Records constructor/shutdown values and checks that each callback preserves host thread state.
void Trace(int value, const int32_t* guestErrno) {
    uintptr_t x18, sp;
    __asm__ volatile("mov %0, x18\n mov %1, sp" : "=r"(x18), "=r"(sp));
    if (x18 != reinterpret_cast<uintptr_t>(NtCurrentTeb()) || (sp & 15) ||
        !guestErrno || guestErrno == &errno || (value == 9 && *guestErrno != 0)) violations++;
    trace = trace * 10 + value;
}

/// Stops on a broken lifecycle instead of running further callbacks through potentially rolled-back code.
void Check(const char* name, bool ok) {
    checked++;
    std::printf("case T07.n1-%s %s\n", name, ok ? "PASS" : "FAIL");
    if (!ok) {
        std::fflush(stdout);
        TerminateProcess(GetCurrentProcess(), 3);
        std::abort();
    }
}

/// Keeps the namespace owners alive through every constructor, explicit finalizer and shutdown.
struct Probe {
    const std::wstring& cases;
    const winapk::guest::BionicLibraries& libraries;
    winapk::guest::ModuleSet& modules;
    std::span<const winapk::guest::HostSymbol> imports;
};

/// Refuses invalid transactions first, then exercises ordinary NDK imports and real lifecycle callbacks.
void* Cases(void* argument) {
    using namespace winapk::guest;
    auto& probe = *static_cast<Probe*>(argument);
    LoadError error;
    bool refused = true;
    for (const auto& [directory, category, reason] : {
            std::tuple{L"missing", Category::SymbolUnresolved, "fork@LIBC"},
            std::tuple{L"version", Category::AbiUnsupported, "NOPE"},
            std::tuple{L"chain", Category::ElfMalformed, "DT_VERNEED"}}) {
        ModuleSet negative((std::filesystem::path(probe.cases) / directory).wstring(),
                           probe.imports, probe.libraries.Libraries());
        refused &= !negative.Open("libbionic.so", &error) && error.category == category &&
                   error.reason.find(reason) != std::string::npos && !negative.Mapped() && trace == 0;
        if (!refused) std::printf("diagnostic %s: %s\n", CategoryName(error.category), error.reason.c_str());
    }
    Check("refusal-rollback", refused);
    const uint64_t root = probe.modules.Open("libbionic.so", &error);
    auto value = reinterpret_cast<int (*)()>(probe.modules.Find(root, "bionic_value"));
    Check("ndk-imports", root && value && value() == 0 && trace == 1 && probe.modules.Mapped() == 1);
    const auto libc = probe.modules.Open("libc.so", &error), log = probe.modules.Open("liblog.so", &error);
    Check("logical-scope", libc && log && probe.modules.Find(libc, "malloc") &&
          !probe.modules.Find(libc, "__android_log_write") && !probe.modules.Find(log, "malloc") &&
          probe.modules.Find(log, "__android_log_print") && probe.modules.Close(libc) &&
          probe.modules.Close(log) && probe.modules.Mapped() == 1);
    auto lifecycle = reinterpret_cast<int (*)()>(probe.modules.Find(root, "bionic_lifecycle"));
    Check("lifecycle", lifecycle && lifecycle() == 0 && trace == 1 && !violations);
    Check("close-pins-callback", probe.modules.Close(root) && !probe.modules.Find(root, "bionic_value") &&
          probe.modules.Mapped() == 1 && trace == 1);
    return nullptr;
}
}  // namespace

int ProbeLibraries(const std::wstring& cases, const std::wstring& helper) {
    using namespace winapk::guest;
    LoadError error;
    auto libraries = BionicLibraries::Load(helper, &error);
    if (!libraries) {
        std::printf("reject %s helper %s: %s\n", CategoryName(error.category), error.where.c_str(), error.reason.c_str());
        return 2;
    }
    std::vector<HostSymbol> imports(libraries->Imports().begin(), libraries->Imports().end());
    imports.push_back({"t07_n1_trace", reinterpret_cast<void*>(&Trace)});
    // Negative graphs need the callback too: a broken transaction must not accidentally run it.
    ModuleSet modules((std::filesystem::path(cases) / L"good").wstring(), imports, libraries->Libraries());
    Probe probe{cases, *libraries, modules, imports};
    {
        ThreadRuntime runtime(&modules);
        runtime.Call(Cases, &probe);
        // Pending process-lifetime callbacks are not live worker/context/heap resources.
        if (!runtime.Idle()) Check("context-cleanup", false);
    }
    Check("shutdown-callback", trace == 19 && !violations);
    std::printf("result T07.N1 %d/%d PASS\n", checked, checked);
    return 0;
}
