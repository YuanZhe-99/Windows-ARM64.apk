// T06 -- ELF files the N0 loader must refuse before running any of their code (PLAN.md §9.3).
//
// One source, one defect per build: scripts/windows/build.ps1 -Component NativeProbe compiles it
// with one NEG_* define per fixture (or with another target or link option), and derives the
// structural cases (bad magic, truncation, overlapping segments, an unknown relocation, ...) by
// patching single fields of the T05 library. test.ps1 -Suite Native checks each refusal's error
// category and reason.

#include <stdint.h>

int64_t neg_value(void) { return 42; }  // the one leaf every variant shares

#if defined(NEG_SVC)
// A raw Linux system call (getpid): never passed to Windows (AGENTS.md, host state).
int64_t neg_svc(void) {
    register int64_t nr __asm__("x8") = 172;
    register int64_t ret __asm__("x0");
    __asm__ volatile("svc #0" : "=r"(ret) : "r"(nr) : "memory");
    return ret;
}
#elif defined(NEG_TPIDR)
// Reads Android's thread pointer: TPIDR_EL0 belongs to the Windows host.
void* neg_tp(void) {
    void* p;
    __asm__ volatile("mrs %0, tpidr_el0" : "=r"(p));
    return p;
}
#elif defined(NEG_X18)
// Reads x18, which holds the Windows TEB on the host.
uint64_t neg_x18(void) {
    uint64_t v;
    __asm__ volatile("mov %0, x18" : "=r"(v));
    return v;
}
#elif defined(NEG_TLS)
// Thread-local storage: not in N0 (PT_TLS, TLSDESC relocations).
__thread int64_t neg_tls_value = 7;
int64_t neg_tls(void) { return neg_tls_value; }
#elif defined(NEG_CTOR)
// A constructor: N0 runs no guest code while loading. host_mark is never bound.
extern void host_mark(void);
__attribute__((constructor)) static void neg_init(void) { host_mark(); }
#elif defined(NEG_MISSING)
// Two imports the host does not provide: both must be listed, before anything runs.
extern int64_t host_missing(int64_t v);
extern int64_t host_also_missing;
int64_t neg_missing(int64_t v) { return host_missing(v) + host_also_missing; }
#elif defined(NEG_IFUNC)
// An IFUNC: its resolver would run guest code while loading (R_AARCH64_IRELATIVE).
static int64_t neg_impl(void) { return 1; }
static int64_t (*neg_resolve(void))(void) { return neg_impl; }
// Hidden and taken by address, so the linker must emit IRELATIVE (a call would be inlined).
__attribute__((visibility("hidden"))) int64_t neg_ifunc(void) __attribute__((ifunc("neg_resolve")));
int64_t (*const neg_ifunc_ptr)(void) = neg_ifunc;
#endif
