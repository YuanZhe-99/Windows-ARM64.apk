// T05 -- the N0 ABI leaf suite (PLAN.md §5.2, §9.3), guest side.
//
// Built by scripts/windows/build.ps1 -Component NativeProbe with the NDK of build-lock.json
// into libn0abi.so (and its relocation/hash/page-size variants), with -nostdlib: no libc, no
// dependencies, no TLS, no constructors. native-probe.exe loads it, calls every n0_* export
// with fixed arguments and compares the results with the exact values in its case table
// (macapk/runtime/platform/windows/src/native_probe/main.cpp); the names here and there match.
//
// Every value is chosen to be exact in binary floating point, so a result is either right or
// an ABI mismatch, never a rounding difference. Guest `long` is 64-bit (LP64).

#include <stdarg.h>
#include <stdint.h>

// ---- host imports, bound by native-probe (JUMP_SLOT for calls, GLOB_DAT for data) ----
extern int64_t host_add(int64_t a, int64_t b);
extern int64_t host_reenter(int64_t depth);  // calls n0_reenter(depth) back, doubles it
extern int64_t host_counter;                 // a host variable the guest reads and writes

// ---- integers, pointers, narrow types ----
int32_t n0_add_i32(int32_t a, int32_t b) { return a + b; }
long n0_mul_long(long a, long b) { return a * b; }
int64_t* n0_store(int64_t* p, int64_t v) { *p = v; return p + 1; }
// The callee may not assume the upper bits of a narrow argument (AAPCS64); it extends them.
int64_t n0_narrow(uint8_t a, int16_t b, int8_t c, uint16_t d) { return (int64_t)a + b + c + d; }

// ---- floating point and mixed registers ----
double n0_fp(float a, double b, float c, double d) { return a * b + c * d; }
double n0_mixed(int32_t a, double b, int64_t c, float d, uint8_t e, double f) {
    return a + b + (double)c + d + e + f;
}

// ---- more arguments than registers: x0-x7 and v0-v7 fill, the rest go on the stack ----
double n0_stack(int64_t i1, int64_t i2, int64_t i3, int64_t i4, int64_t i5, int64_t i6,
                int64_t i7, int64_t i8, int64_t i9, int64_t i10,
                double d1, double d2, double d3, double d4, double d5, double d6,
                double d7, double d8, double d9, double d10) {
    return (double)(i1 + 2 * i2 + 3 * i3 + 4 * i4 + 5 * i5 + 6 * i6 + 7 * i7 + 8 * i8 +
                    9 * i9 + 10 * i10) +
           d1 + 2 * d2 + 3 * d3 + 4 * d4 + 5 * d5 + 6 * d6 + 7 * d7 + 8 * d8 + 9 * d9 + 10 * d10;
}
// Small types past the registers each take an 8-byte stack slot (AAPCS64; Apple packs them).
int64_t n0_stack_small(int64_t a1, int64_t a2, int64_t a3, int64_t a4, int64_t a5,
                       int64_t a6, int64_t a7, int64_t a8, int32_t s1, int8_t s2,
                       int16_t s3, int32_t s4) {
    return a1 + a2 + a3 + a4 + a5 + a6 + a7 + a8 + 1000 * s1 + 100 * s2 + 10 * s3 + s4;
}

// ---- aggregates ----
typedef struct { float x, y, z, w; } Vec4f;                 // HFA: v0-v3
typedef struct { double re, im; } Complexd;                 // HFA: v0-v1
typedef struct { int32_t a, b; } Pair32;                    // 8 bytes: x0
typedef struct { int64_t a, b; } Pair64;                    // 16 bytes: x0, x1
typedef struct { int64_t a, b, c; } Triple64;               // 24 bytes: returned through x8
typedef struct { uint8_t tag; double value; int16_t small; } Mixed;  // 24 bytes: passed by reference

Vec4f n0_vec_scale(Vec4f v, float k) { Vec4f r = {v.x * k, v.y * k, v.z * k, v.w * k}; return r; }
Complexd n0_cmul(Complexd a, Complexd b) {
    Complexd r = {a.re * b.re - a.im * b.im, a.re * b.im + a.im * b.re};
    return r;
}
Pair32 n0_pair32(int32_t a, int32_t b) { Pair32 r = {b, a}; return r; }
Pair64 n0_pair64(int64_t a, int64_t b) { Pair64 r = {b, a}; return r; }
Triple64 n0_triple(int64_t base) { Triple64 r = {base, base * 2, base * 3}; return r; }
double n0_mixed_struct(Mixed m) { return m.tag + m.value + m.small; }

// ---- data: RELATIVE (pointer tables), ABS64 (a pointer to a preemptible global), BSS ----
// Exported, so the compiler keeps them as pointers (a private table becomes offsets).
const char* const n0_names[] = {"alpha", "beta", "gamma"};
static int64_t square(int64_t x) { return x * x; }
static int64_t negate(int64_t x) { return -x; }
int64_t (*const n0_ops[])(int64_t) = {square, negate};
int64_t n0_counter;                       // .bss: zero when loaded
int64_t* const n0_counter_ref = &n0_counter;
static uint8_t big_bss[100000];           // spans many pages past the file's end

const char* n0_name(int32_t i) { return n0_names[i]; }
int64_t n0_op(int32_t i, int64_t x) { return n0_ops[i](x); }
int64_t n0_bump(void) { return ++*n0_counter_ref; }
// Returns 1 when every byte of big_bss is zero, then dirties it (a second load starts clean).
int32_t n0_bss_clean(void) {
    for (uint32_t i = 0; i < sizeof big_bss; i++) {
        if (big_bss[i] != 0) return 0;
    }
    for (uint32_t i = 0; i < sizeof big_bss; i += 4096) big_bss[i] = 1;
    return 1;
}

// ---- calls between guest and host ----
int64_t n0_call_import(int64_t a, int64_t b) {
    int64_t r = host_add(a, b) * 2 + host_counter;
    host_counter += 1;
    return r;
}
double n0_apply(double (*f)(double, int32_t), double x) { return f(x, 3) + 1; }
// n0_reenter(d) = 2 * n0_reenter(d - 1) + d through the host, n0_reenter(0) = 1.
int64_t n0_reenter(int64_t depth) { return depth == 0 ? 1 : host_reenter(depth - 1) + depth; }

// Keeps 12 integers and 8 doubles live across host calls, so the compiler must hold them in
// x19-x28 and d8-d15; the host checks that its own values there survive (callee-saved).
int64_t n0_pressure(int64_t s) {
    int64_t a = s + 1, b = s + 2, c = s + 3, d = s + 4, e = s + 5, f = s + 6;
    int64_t g = s + 7, h = s + 8, i = s + 9, j = s + 10, k = s + 11, l = s + 12;
    double p = s * 0.5, q = s * 0.25, r = s * 2.0, t = s * 4.0;
    double u = s + 0.5, v = s + 0.25, w = s - 0.5, x = s - 0.25;
    int64_t sum = host_add(a, b);
    sum += host_add(c, d) * 3;
    sum += host_add(e, f) * 5;
    return sum + a * b + c * d + e * f + g * h + i * j + k * l +
           (int64_t)(p * q + r * t + u * v + w * x);
}

// The stack pointer's alignment as the guest sees it on entry (must be 0).
uint64_t n0_sp_mod16(void) {
    uint64_t sp;
    __asm__ volatile("mov %0, sp" : "=r"(sp));
    return sp & 15;
}

// A variadic guest function. AAPCS64 passes variadic arguments like fixed ones, so the host
// calls it through a fixed prototype of the promoted types; a Windows variadic call would put
// the doubles in integer registers (abi-compatibility.md).
double n0_va_sum(int32_t n, ...) {
    va_list ap;
    va_start(ap, n);
    double s = 0;
    for (int32_t k = 0; k < n; k++) {
        s += (double)va_arg(ap, int64_t);
        s += va_arg(ap, double);
    }
    va_end(ap);
    return s;
}
