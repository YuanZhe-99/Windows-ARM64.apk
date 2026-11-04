// native-probe.exe -- the standalone guest ELF experiment (PLAN.md §9.1), no JVM, no window.
//
//   native-probe load <elf>                  load and report, or print the refusal
//   native-probe suite <elf>                 T05: call every n0_* export, compare exact values
//   native-probe stress <elf> [--iterations N] [--copies K] [--rejected <elf>]
//
// Exit codes: 0 every check passed; 1 usage or unreadable input; 2 the module was refused
// (the line starts "reject CATEGORY"); 3 a case or a stress check failed.
//
// The calls into guest code use host prototypes whose types match the guest's AAPCS64
// signature exactly (guest long = int64_t). abi-compatibility.md says which signatures that is
// proven for; this file is its evidence. Host state is only read: x18 must still be the TEB
// and sp 16-byte aligned whenever the guest calls back, and x19-x28/d8-d15 must survive.

#include "elf_loader.h"

#include <windows.h>
#include <psapi.h>

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <functional>
#include <memory>
#include <set>
#include <string>
#include <vector>

namespace {

using winapk::guest::HostSymbol;
using winapk::guest::LoadError;
using winapk::guest::Module;

// ---- host state checks, read only (PLAN.md §9.3: no sentinel is written to x18) ----

uint64_t ReadX18() {
    uint64_t v;
    __asm__ volatile("mov %0, x18" : "=r"(v));
    return v;
}
uint64_t ReadSp() {
    uint64_t v;
    __asm__ volatile("mov %0, sp" : "=r"(v));
    return v;
}

uint64_t g_teb;                // this thread's TEB, which x18 must hold
int g_callbacks;               // host functions the guest called
std::vector<std::string> g_violations;
const Module* g_module;        // the module the reentry callback calls back into

void CheckHostState(const char* who) {
    g_callbacks++;
    if (ReadX18() != g_teb) g_violations.push_back(std::string(who) + ": x18 is not the TEB");
    if (ReadSp() & 15) g_violations.push_back(std::string(who) + ": sp is not 16-byte aligned");
}

// ---- the host symbols the T05 module imports ----

int64_t host_counter;
int64_t HostAdd(int64_t a, int64_t b) {
    CheckHostState("host_add");
    return a + b;
}
int64_t HostReenter(int64_t depth) {
    CheckHostState("host_reenter");
    auto f = reinterpret_cast<int64_t (*)(int64_t)>(g_module->Find("n0_reenter"));
    return f ? f(depth) * 2 : -1;
}
double HostScale(double x, int32_t n) {
    CheckHostState("callback");
    return x * n;
}

const HostSymbol kImports[] = {
    {"host_add", reinterpret_cast<void*>(&HostAdd)},
    {"host_reenter", reinterpret_cast<void*>(&HostReenter)},
    {"host_counter", &host_counter},
};

// ---- callee-saved registers ----

#define SET(n) "movz x" #n ", #" #n ", lsl #48\n movk x" #n ", #0x5a5a\n movk x" #n ", #" #n ", lsl #16\n"
#define CHECKX(n) "movz x2, #" #n ", lsl #48\n movk x2, #0x5a5a\n movk x2, #" #n ", lsl #16\n" \
                  "cmp x" #n ", x2\n cset x3, ne\n orr x0, x0, x3, lsl #" #n "\n"
#define CHECKD(d, n, bit) "fmov x4, d" #d "\n movz x2, #" #n ", lsl #48\n movk x2, #0x5a5a\n movk x2, #" #n ", lsl #16\n" \
                  "cmp x4, x2\n cset x3, ne\n orr x0, x0, x3, lsl #" #bit "\n"

/// Calls fn(arg) with known values in x19-x27 and d8-d15, and sp in x28.
/// Returns: fn's result; *changed gets a bit for each register that did not survive:
/// bits 19-27 for x19-x27, bit 28 for x28 or sp, bits 40-47 for d8-d15.
__attribute__((noinline)) int64_t CallPreserved(void* fn, int64_t arg, uint64_t* changed) {
    register uint64_t x0 __asm__("x0") = static_cast<uint64_t>(arg);
    register uint64_t x1 __asm__("x1");
    register void* x16 __asm__("x16") = fn;
    __asm__ volatile(
        SET(19) SET(20) SET(21) SET(22) SET(23) SET(24) SET(25) SET(26) SET(27)
        "mov x28, sp\n"
        "fmov d8, x19\n fmov d9, x20\n fmov d10, x21\n fmov d11, x22\n"
        "fmov d12, x23\n fmov d13, x24\n fmov d14, x25\n fmov d15, x26\n"
        "blr x16\n"
        "mov x1, x0\n mov x0, #0\n"
        CHECKX(19) CHECKX(20) CHECKX(21) CHECKX(22) CHECKX(23) CHECKX(24) CHECKX(25) CHECKX(26) CHECKX(27)
        "mov x2, sp\n cmp x28, x2\n cset x3, ne\n orr x0, x0, x3, lsl #28\n"
        CHECKD(8, 19, 40) CHECKD(9, 20, 41) CHECKD(10, 21, 42) CHECKD(11, 22, 43)
        CHECKD(12, 23, 44) CHECKD(13, 24, 45) CHECKD(14, 25, 46) CHECKD(15, 26, 47)
        : "+r"(x0), "=r"(x1), "+r"(x16)
        :
        : "x2", "x3", "x4", "x5", "x6", "x7", "x8", "x9", "x10", "x11", "x12", "x13", "x14", "x15",
          "x17", "x19", "x20", "x21", "x22", "x23", "x24", "x25", "x26", "x27", "x28", "x30",
          "v0", "v1", "v2", "v3", "v4", "v5", "v6", "v7", "v8", "v9", "v10", "v11", "v12", "v13",
          "v14", "v15", "v16", "v17", "v18", "v19", "v20", "v21", "v22", "v23", "v24", "v25",
          "v26", "v27", "v28", "v29", "v30", "v31", "cc", "memory");
    *changed = x0;
    return static_cast<int64_t>(x1);
}

/// Breaks the convention on purpose, zeroing x20 without saving it: the negative case that
/// shows CallPreserved notices a callee-saved register that did not survive.
__attribute__((naked)) int64_t BreakX20(int64_t) { __asm__ volatile("mov x20, #0\n ret"); }

// ---- the T05 cases: guest signatures as host types, and the exact expected values ----

struct Vec4f { float x, y, z, w; };
struct Complexd { double re, im; };
struct Pair32 { int32_t a, b; };
struct Pair64 { int64_t a, b; };
struct Triple64 { int64_t a, b, c; };
struct Mixed { uint8_t tag; double value; int16_t small; };

struct Case {
    const char* name;
    std::function<std::string(const Module&)> run;  // "" when it passed, else what differed
};

template <class F>
F Fn(const Module& m, const char* name) {
    return reinterpret_cast<F>(m.Find(name));
}

std::string Want(bool ok, const std::string& got, const std::string& want) {
    return ok ? "" : "got " + got + ", want " + want;
}
std::string I(int64_t v) { return std::to_string(v); }
std::string D(double v) {
    char b[64];
    std::snprintf(b, sizeof b, "%.17g", v);
    return b;
}

#define EXPORT(m, type, name) \
    auto name = Fn<type>(m, #name); \
    if (!name) return std::string("export " #name " missing");

const std::vector<Case>& Cases() {
    static const std::vector<Case> cases = {
        {"int.i32", [](const Module& m) {
             EXPORT(m, int32_t (*)(int32_t, int32_t), n0_add_i32);
             const int32_t r = n0_add_i32(-7, 3);
             return Want(r == -4, I(r), "-4");
         }},
        {"int.long64", [](const Module& m) {
             EXPORT(m, int64_t (*)(int64_t, int64_t), n0_mul_long);
             const int64_t r = n0_mul_long(3000000000, 3);
             return Want(r == 9000000000, I(r), "9000000000");
         }},
        {"int.narrow", [](const Module& m) {
             EXPORT(m, int64_t (*)(uint8_t, int16_t, int8_t, uint16_t), n0_narrow);
             const int64_t r = n0_narrow(200, -300, -5, 60000);
             return Want(r == 59895, I(r), "59895");
         }},
        // The same call with garbage above each narrow value: the guest extends them itself,
        // so the host needs not (AAPCS64 leaves the upper bits unspecified).
        {"int.narrow-dirty", [](const Module& m) {
             EXPORT(m, int64_t (*)(uint64_t, uint64_t, uint64_t, uint64_t), n0_narrow);
             const int64_t r = n0_narrow(0xABCDEF00000000C8, 0xFFFF0000FFFFFED4, 0x12345678000000FB, 0x00000000DEADEA60);
             return Want(r == 59895, I(r), "59895");
         }},
        {"ptr.store", [](const Module& m) {
             EXPORT(m, int64_t* (*)(int64_t*, int64_t), n0_store);
             int64_t buf[2] = {};
             int64_t* r = n0_store(buf, 0x1122334455667788);
             return Want(r == buf + 1 && buf[0] == 0x1122334455667788, I(buf[0]), "0x1122334455667788 and p + 1");
         }},
        {"fp.basic", [](const Module& m) {
             EXPORT(m, double (*)(float, double, float, double), n0_fp);
             const double r = n0_fp(1.5f, 2.0, 0.25f, 8.0);
             return Want(r == 5.0, D(r), "5");
         }},
        {"fp.mixed", [](const Module& m) {
             EXPORT(m, double (*)(int32_t, double, int64_t, float, uint8_t, double), n0_mixed);
             const double r = n0_mixed(7, 0.5, int64_t{1} << 40, 0.25f, 255, -1.5);
             return Want(r == 1099511628037.25, D(r), "1099511628037.25");
         }},
        {"stack.spill", [](const Module& m) {
             EXPORT(m, double (*)(int64_t, int64_t, int64_t, int64_t, int64_t, int64_t, int64_t, int64_t, int64_t,
                                  int64_t, double, double, double, double, double, double, double, double, double, double),
                    n0_stack);
             const double r = n0_stack(100, 200, 300, 400, 500, 600, 700, 800, 900, 1000,
                                       0.5, 1.0, 1.5, 2.0, 2.5, 3.0, 3.5, 4.0, 4.5, 5.0);
             return Want(r == 38692.5, D(r), "38692.5");
         }},
        {"stack.small", [](const Module& m) {
             EXPORT(m, int64_t (*)(int64_t, int64_t, int64_t, int64_t, int64_t, int64_t, int64_t, int64_t, int32_t,
                                   int8_t, int16_t, int32_t),
                    n0_stack_small);
             const int64_t r = n0_stack_small(1, 2, 3, 4, 5, 6, 7, 8, -9, -8, 300, -123456);
             return Want(r == -130220, I(r), "-130220");
         }},
        {"hfa.vec4f", [](const Module& m) {
             EXPORT(m, Vec4f (*)(Vec4f, float), n0_vec_scale);
             const Vec4f r = n0_vec_scale({1, 2, 3, 4}, 0.5f);
             return Want(r.x == 0.5f && r.y == 1 && r.z == 1.5f && r.w == 2,
                         D(r.x) + "," + D(r.y) + "," + D(r.z) + "," + D(r.w), "0.5,1,1.5,2");
         }},
        {"hfa.complex", [](const Module& m) {
             EXPORT(m, Complexd (*)(Complexd, Complexd), n0_cmul);
             const Complexd r = n0_cmul({1.5, 2}, {-2, 0.5});
             return Want(r.re == -4 && r.im == -3.25, D(r.re) + "," + D(r.im), "-4,-3.25");
         }},
        {"struct.pair32", [](const Module& m) {
             EXPORT(m, Pair32 (*)(int32_t, int32_t), n0_pair32);
             const Pair32 r = n0_pair32(7, -9);
             return Want(r.a == -9 && r.b == 7, I(r.a) + "," + I(r.b), "-9,7");
         }},
        {"struct.pair64", [](const Module& m) {
             EXPORT(m, Pair64 (*)(int64_t, int64_t), n0_pair64);
             const Pair64 r = n0_pair64(int64_t{1} << 40, -5);
             return Want(r.a == -5 && r.b == int64_t{1} << 40, I(r.a) + "," + I(r.b), "-5,1099511627776");
         }},
        {"struct.indirect", [](const Module& m) {
             EXPORT(m, Triple64 (*)(int64_t), n0_triple);
             const Triple64 r = n0_triple(11);
             return Want(r.a == 11 && r.b == 22 && r.c == 33, I(r.a) + "," + I(r.b) + "," + I(r.c), "11,22,33");
         }},
        {"struct.byref", [](const Module& m) {
             EXPORT(m, double (*)(Mixed), n0_mixed_struct);
             const double r = n0_mixed_struct({200, 0.75, -1234});
             return Want(r == -1033.25, D(r), "-1033.25");
         }},
        {"va.fixed-prototype", [](const Module& m) {
             EXPORT(m, double (*)(int32_t, int64_t, double, int64_t, double), n0_va_sum);
             const double r = n0_va_sum(2, 10, 0.5, -3, 0.25);
             return Want(r == 7.75, D(r), "7.75");
         }},
        {"data.relative", [](const Module& m) {
             EXPORT(m, const char* (*)(int32_t), n0_name);
             EXPORT(m, int64_t (*)(int32_t, int64_t), n0_op);
             const char* s = n0_name(1);
             const int64_t a = n0_op(0, 12), b = n0_op(1, 12);
             return Want(s && std::strcmp(s, "beta") == 0 && a == 144 && b == -12,
                         std::string(s ? s : "null") + "," + I(a) + "," + I(b), "beta,144,-12");
         }},
        {"data.abs64", [](const Module& m) {
             auto ref = static_cast<int64_t* const*>(m.Find("n0_counter_ref"));
             void* counter = m.Find("n0_counter");
             return Want(ref && counter && *ref == counter, "", "n0_counter_ref == &n0_counter");
         }},
        {"data.bss", [](const Module& m) {
             EXPORT(m, int64_t (*)(), n0_bump);
             EXPORT(m, int32_t (*)(), n0_bss_clean);
             const int64_t a = n0_bump(), b = n0_bump();
             const int32_t clean = n0_bss_clean();
             return Want(a == 1 && b == 2 && clean == 1, I(a) + "," + I(b) + "," + I(clean), "1,2,1");
         }},
        {"import.call", [](const Module& m) {
             EXPORT(m, int64_t (*)(int64_t, int64_t), n0_call_import);
             host_counter = 5;
             const int64_t r = n0_call_import(20, 22);
             return Want(r == 89 && host_counter == 6, I(r) + "," + I(host_counter), "89,6");
         }},
        {"callback.pointer", [](const Module& m) {
             EXPORT(m, double (*)(double (*)(double, int32_t), double), n0_apply);
             const double r = n0_apply(&HostScale, 2.5);
             return Want(r == 8.5, D(r), "8.5");
         }},
        {"callback.reenter", [](const Module& m) {
             EXPORT(m, int64_t (*)(int64_t), n0_reenter);
             const int before = g_callbacks;
             const int64_t r = n0_reenter(5);
             const int calls = g_callbacks - before;
             return Want(r == 89 && calls == 5, I(r) + " after " + I(calls) + " host calls", "89 after 5");
         }},
        {"regs.callee-saved", [](const Module& m) {
             void* f = m.Find("n0_pressure");
             if (!f) return std::string("export n0_pressure missing");
             uint64_t changed = 0;
             const int64_t r = CallPreserved(f, 10, &changed);
             char mask[32];
             std::snprintf(mask, sizeof mask, "changed 0x%llx", static_cast<unsigned long long>(changed));
             return Want(r == 2973 && changed == 0, I(r) + ", " + mask, "2973, changed 0x0");
         }},
        {"regs.harness-detects", [](const Module&) {
             uint64_t changed = 0;
             CallPreserved(reinterpret_cast<void*>(&BreakX20), 0, &changed);
             char mask[32];
             std::snprintf(mask, sizeof mask, "0x%llx", static_cast<unsigned long long>(changed));
             return Want(changed == (uint64_t{1} << 20), mask, "0x100000 (x20 only)");
         }},
        {"regs.sp-entry", [](const Module& m) {
             EXPORT(m, uint64_t (*)(), n0_sp_mod16);
             const uint64_t r = n0_sp_mod16();
             return Want(r == 0, I(static_cast<int64_t>(r)), "0");
         }},
        {"symbols.lookup", [](const Module& m) {
             // Undefined and unknown names are not exports; a defined one is.
             const bool ok = m.Find("n0_add_i32") && !m.Find("host_add") && !m.Find("n0_absent");
             return Want(ok, "", "n0_add_i32 found, host_add and n0_absent not");
         }},
        // Every host call above ran with x18 = TEB and an aligned sp, and x18 is still the TEB.
        {"host.state", [](const Module&) {
             const bool ok = g_violations.empty() && g_callbacks > 0 && ReadX18() == g_teb;
             return Want(ok, g_violations.empty() ? I(g_callbacks) + " callbacks" : g_violations.front(),
                         "no violation in any callback");
         }},
    };
    return cases;
}

/// Runs every case on `m`. Returns: the number that failed. Side effects: prints a line per
/// case when `print`.
int RunSuite(const Module& m, bool print) {
    g_module = &m;
    g_teb = reinterpret_cast<uint64_t>(NtCurrentTeb());
    g_callbacks = 0;
    g_violations.clear();
    int failed = 0;
    for (const Case& c : Cases()) {
        const std::string r = c.run(m);
        if (!r.empty()) failed++;
        if (print) std::printf("case %s %s%s%s\n", c.name, r.empty() ? "PASS" : "FAIL", r.empty() ? "" : ": ", r.c_str());
    }
    return failed;
}

std::string Utf8(const std::wstring& w) {
    std::string s(WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, nullptr, 0, nullptr, nullptr), '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, s.data(), static_cast<int>(s.size()), nullptr, nullptr);
    s.pop_back();
    return s;
}

/// Loads `path`, printing the refusal when there is one.
std::unique_ptr<Module> LoadOrReport(const std::wstring& path, bool quiet = false) {
    LoadError e;
    auto m = winapk::guest::Load(path, kImports, &e);
    if (!m && !quiet) {
        std::printf("reject %s %s%s%s: %s\n", winapk::guest::CategoryName(e.category),
                    Utf8(std::filesystem::path(path).filename().wstring()).c_str(), e.where.empty() ? "" : " ",
                    e.where.c_str(), e.reason.c_str());
    }
    return m;
}

void PrintModule(const Module& m) {
    const auto& s = m.Stats();
    std::printf("load %s: profile N0, PT_LOAD %s, 0x%zx bytes at 0x%llx\n", m.Name().c_str(), s.segments.c_str(),
                m.Size(), static_cast<unsigned long long>(m.Base()));
    std::printf("  relocations %s; %u symbols (%s hash); RELRO 0x%llx bytes\n", s.relocations.c_str(), s.symbols,
                s.hash.c_str(), static_cast<unsigned long long>(s.relroBytes));
    std::printf("  screened 0x%llx code bytes: %u instructions, %u data words, no svc, TPIDR_EL0 or x18\n",
                static_cast<unsigned long long>(s.codeBytes), s.instructions, s.dataWords);
}

void PrintHost() {
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    PROCESS_MITIGATION_DYNAMIC_CODE_POLICY dyn{};
    PROCESS_MITIGATION_CONTROL_FLOW_GUARD_POLICY cfg{};
    GetProcessMitigationPolicy(GetCurrentProcess(), ProcessDynamicCodePolicy, &dyn, sizeof dyn);
    GetProcessMitigationPolicy(GetCurrentProcess(), ProcessControlFlowGuardPolicy, &cfg, sizeof cfg);
    std::printf("native-probe %s (ARM64): host page 0x%lx, allocation granularity 0x%lx; dynamic code %s; CFG %s\n",
                WINAPK_VERSION, si.dwPageSize, si.dwAllocationGranularity,
                dyn.ProhibitDynamicCode ? "prohibited" : "allowed", cfg.EnableControlFlowGuard ? "on" : "off");
}

/// What a leak would grow: bytes allocated in every heap of the process (exact, unlike the
/// commit, which a heap keeps after a free), the commit itself, the private regions of the
/// address space (a module that is not released keeps one), and the handle count.
struct Usage {
    long long heapBytes, privateBytes, regions, handles;
};
Usage Measure() {
    HANDLE heaps[64];
    const DWORD n = GetProcessHeaps(64, heaps);
    long long heap = 0;
    for (DWORD i = 0; i < n && i < 64; i++) {
        HEAP_SUMMARY hs{sizeof hs};
        if (HeapSummary(heaps[i], 0, &hs)) heap += static_cast<long long>(hs.cbAllocated);
    }
    PROCESS_MEMORY_COUNTERS_EX pmc{sizeof pmc};
    GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&pmc), sizeof pmc);
    long long regions = 0;
    MEMORY_BASIC_INFORMATION mbi;
    for (uint8_t* a = nullptr; VirtualQuery(a, &mbi, sizeof mbi) == sizeof mbi; a = static_cast<uint8_t*>(mbi.BaseAddress) + mbi.RegionSize) {
        if (mbi.State != MEM_FREE && mbi.Type == MEM_PRIVATE && mbi.BaseAddress == mbi.AllocationBase) regions++;
    }
    DWORD h = 0;
    GetProcessHandleCount(GetCurrentProcess(), &h);
    return {heap, static_cast<long long>(pmc.PrivateUsage), regions, h};
}

// Repeated load/run/unload must not grow (PLAN.md §9 G2): after warm-up the cycles run in two
// equal halves, and the second may not grow; the heap settles in the first, a leak grows in
// both. K copies at once must land at K distinct bases and all pass; a refused load
// (`rejected`) must leave nothing behind.
int Stress(const std::wstring& path, int iterations, int copies, const std::wstring& rejected) {
    const std::string name = Utf8(std::filesystem::path(path).filename().wstring());
    int failed = 0;
    auto cycle = [&] {
        auto m = LoadOrReport(path);
        if (!m) return false;
        if (RunSuite(*m, false) != 0) return false;
        if (!rejected.empty() && LoadOrReport(rejected, true)) return false;  // must be refused
        return true;
    };
    const int warmup = iterations / 4 + 1, half = iterations / 2;
    for (int i = 0; i < warmup; i++) failed += !cycle();
    const Usage u0 = Measure();
    for (int i = 0; i < half; i++) failed += !cycle();
    const Usage u1 = Measure();
    for (int i = 0; i < half; i++) failed += !cycle();
    const Usage u2 = Measure();
    const long long handles = u2.handles - u0.handles, regions = u2.regions - u0.regions;
    std::printf("stress %s: %d load/run/unload cycles%s, %s; per %d cycles heap allocations %+lld then %+lld bytes, "
                "commit %+lld then %+lld KiB; private regions %+lld, handles %+lld; after %d warm-up cycles\n",
                name.c_str(), 2 * half, rejected.empty() ? "" : " (each with a refused load)", failed ? "FAIL" : "all PASS",
                half, u1.heapBytes - u0.heapBytes, u2.heapBytes - u1.heapBytes, (u1.privateBytes - u0.privateBytes) / 1024,
                (u2.privateBytes - u1.privateBytes) / 1024, regions, handles, warmup);
    // A leak of one allocation per cycle shows in both halves; settling of the heap in neither.
    bool ok = failed == 0 && u2.heapBytes - u1.heapBytes <= 0 && u1.heapBytes - u0.heapBytes <= 0 && regions <= 0 && handles <= 0;

    std::vector<std::unique_ptr<Module>> all;
    std::set<uintptr_t> bases;
    int copyFails = 0;
    for (int i = 0; i < copies; i++) {
        auto m = LoadOrReport(path);
        if (!m) { copyFails++; continue; }
        bases.insert(m->Base());
        all.push_back(std::move(m));
    }
    for (const auto& m : all) copyFails += RunSuite(*m, false) != 0;
    std::printf("stress %s: %d copies loaded at once at %zu distinct bases, %s\n", name.c_str(), copies, bases.size(),
                copyFails ? "FAIL" : "all PASS");
    ok = ok && copyFails == 0 && static_cast<int>(bases.size()) == copies;
    std::printf("result stress %s\n", ok ? "PASS" : "FAIL");
    return ok ? 0 : 3;
}

int UsageError() {
    std::fputs("usage: native-probe load <elf>\n"
               "       native-probe suite <elf>\n"
               "       native-probe stress <elf> [--iterations N] [--copies K] [--rejected <elf>]\n", stderr);
    return 1;
}

}  // namespace

int wmain(int argc, wchar_t** argv) {
    if (argc < 3) return UsageError();
    const std::wstring cmd = argv[1], path = argv[2];
    if (GetFileAttributesW(path.c_str()) == INVALID_FILE_ATTRIBUTES) {
        std::fprintf(stderr, "native-probe: cannot read %s\n", Utf8(path).c_str());
        return 1;
    }
    PrintHost();
    if (cmd == L"load" || cmd == L"suite") {
        auto m = LoadOrReport(path);
        if (!m) return 2;
        PrintModule(*m);
        if (cmd == L"load") return 0;
        const int failed = RunSuite(*m, true);
        std::printf("result %zu/%zu PASS\n", Cases().size() - failed, Cases().size());
        return failed ? 3 : 0;
    }
    if (cmd == L"stress") {
        int iterations = 1000, copies = 32;
        std::wstring rejected;
        for (int i = 3; i + 1 < argc; i += 2) {
            const std::wstring opt = argv[i];
            if (opt == L"--iterations") iterations = _wtoi(argv[i + 1]);
            else if (opt == L"--copies") copies = _wtoi(argv[i + 1]);
            else if (opt == L"--rejected") rejected = argv[i + 1];
            else return UsageError();
        }
        if (iterations <= 0 || copies <= 0) return UsageError();
        return Stress(path, iterations, copies, rejected);
    }
    return UsageError();
}
