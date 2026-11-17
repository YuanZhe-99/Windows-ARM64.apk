// T07: one dependency pair and compact negative graph transactions, no JVM or window.
#include "modules.h"
#include "../../../../guest/bionic/threads.h"

#include <cstdio>
#include <thread>
#include <vector>

namespace {
int trace;
winapk::guest::ModuleSet* constructing;
bool reentryOk;

/// Records constructor order through a fixed-signature guest-to-host import.
void Trace(int value) {
    trace = trace * 10 + value;
    if (value != 1 || !constructing) return;
    winapk::guest::LoadError error;
    const uint64_t handle = constructing->Open("libroot.so", &error);
    const bool sameThread = handle && constructing->Find(handle, "root_value") && constructing->Close(handle);
    bool competingThread = false;
    // A constructor may join a host/guest callback thread: the registry must not hold a lock
    // across this callback, and the competing open must not see a half-initialized graph.
    std::thread competing([&] {
        winapk::guest::LoadError competingError;
        competingThread = constructing->Open("libroot.so", &competingError) == 0 &&
                          competingError.reason.find("concurrent") != std::string::npos;
    });
    competing.join();
    reentryOk = sameThread && competingThread;
}

/// Holds the caller-owned import table and directory while the test context is active.
struct Probe {
    winapk::guest::ModuleSet& modules;
    int failed = 0, checked = 0;

    /// Records one semantic contract rather than assertions for every internal state field.
    void Check(const char* name, bool ok) {
        checked++;
        failed += !ok;
        std::printf("case T07.%s %s\n", name, ok ? "PASS" : "FAIL");
    }
};

/// Tests failure rollback first, then initialization/lookup/refcounts and reuse in one namespace.
void* Cases(void* argument) {
    using namespace winapk::guest;
    auto& probe = *static_cast<Probe*>(argument);
    auto& modules = probe.modules;
    LoadError error;
    trace = 0;
    const bool missing = modules.Open("libbadroot.so", &error) == 0 &&
                         error.category == Category::SymbolUnresolved &&
                         error.reason.find("required_missing") != std::string::npos;
    probe.Check("missing-import-rollback", missing && modules.Mapped() == 0 && trace == 0);
    const bool initializer = modules.Open("libbadinit.so", &error) == 0 &&
                             error.category == Category::AbiUnsupported &&
                             error.reason.find("executable") != std::string::npos;
    probe.Check("initializer-rollback", initializer && modules.Mapped() == 0 && trace == 0);
    const bool cycle = modules.Open("libcycle.so", &error) == 0 &&
                       error.reason.find("cycles") != std::string::npos;
    const bool alias = modules.Open("libcycle-seed.so", &error) == 0 &&
                       error.reason.find("SONAME") != std::string::npos;
    const bool path = modules.Open("../libdep.so", &error) == 0;
    const bool absent = modules.Open("libabsent.so", &error) == 0 && error.category == Category::SymbolUnresolved;
    probe.Check("cycle-name-scope", cycle && alias && path && absent && modules.Mapped() == 0 && trace == 0);

    constructing = &modules;
    reentryOk = false;
    const uint64_t root = modules.Open("libroot.so", &error);
    constructing = nullptr;
    auto value = reinterpret_cast<int (*)()>(modules.Find(root, "root_value"));
    probe.Check("dependency-order-weak", root && value && value() == 49 && trace == 12 && modules.Mapped() == 2);
    probe.Check("initializer-reentry", reentryOk);
    auto dynamic = reinterpret_cast<int (*)()>(modules.Find(root, "dynamic_roundtrip"));
    probe.Check("guest-dl-roundtrip", dynamic && dynamic() == 0 && trace == 12);
    auto dependency = reinterpret_cast<int (*)()>(modules.Find(root, "dependency_value"));
    auto rootData = static_cast<int*>(modules.Find(root, "shared_value"));
    probe.Check("lookup-own-definition", dependency && dependency() == 7 && rootData && *rootData == 42 &&
                !modules.Find(root, "t07_trace") && !modules.Find(root, "absent"));
    const uint64_t second = modules.Open("libroot.so", &error);
    const bool once = second == root && trace == 12 && modules.Mapped() == 2;
    const bool firstClose = modules.Close(root) && modules.Find(root, "root_value");
    const bool finalClose = modules.Close(root) && !modules.Find(root, "root_value") && !modules.Close(root);
    const uint64_t reopened = modules.Open("libroot.so", &error);
    probe.Check("repeat-reference-pinning", once && firstClose && finalClose && reopened == root &&
                trace == 12 && value && value() == 49 && modules.Mapped() == 2);
    const size_t retained = modules.Mapped();
    const bool rollback = modules.Open("libbadroot.so", &error) == 0;
    probe.Check("existing-graph-preserved", rollback && modules.Mapped() == retained && trace == 12 &&
                dependency && dependency() == 7 && !modules.Close(UINT64_MAX));
    if (reopened) modules.Close(reopened);
    return nullptr;
}
}  // namespace

int ProbeModules(const std::wstring& directory) {
    auto threads = winapk::guest::ThreadRuntime::Imports();
    std::vector<winapk::guest::HostSymbol> imports(threads.begin(), threads.end());
    imports.push_back({"t07_trace", reinterpret_cast<void*>(&Trace)});
    winapk::guest::ModuleSet modules(directory, imports);
    Probe probe{modules};
    winapk::guest::ThreadRuntime runtime(&modules);
    runtime.Call(Cases, &probe);
    probe.Check("context-cleanup", runtime.Idle());
    std::printf("result T07 %d/%d PASS\n", probe.checked - probe.failed, probe.checked);
    return probe.failed ? 3 : 0;
}
