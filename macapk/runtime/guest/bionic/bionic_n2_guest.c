// bionic_n2_guest.c -- Pure Android AArch64 guest-ABI variadics and setjmp/longjmp for profile N2.
// Compiled by Android NDK clang (`--target=aarch64-linux-android30 -nostdlib -fno-builtin`).
// No guest `va_list` or `jmp_buf` ever crosses into Windows host code; only fixed-signature
// scalar/pointer helpers (`__winapk_n2_*`) cross the ABI boundary.
#include <errno.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <wchar.h>

_Static_assert(sizeof(long) == 8 && sizeof(size_t) == 8, "guest LP64 ABI required");

// Fixed-signature host imports provided by `bionic_n2.cpp`:
extern void* malloc(size_t size);
extern void free(void* ptr);
extern int __winapk_n2_fmt_int(char* dst, size_t cap, const char* spec, uint64_t bits, int is_signed);
extern int __winapk_n2_fmt_double(char* dst, size_t cap, const char* spec, double val);
extern int __winapk_n2_stream_write(void* stream, const void* buf, size_t len);
extern int __winapk_n2_stream_read_line(void* stream, char* buf, size_t cap);
extern int __winapk_n2_scan_one(const char* src, const char* spec, void* out_ptr, int kind, size_t* consumed);
extern int __winapk_n2_log_write(int prio, const char* tag, const char* msg);
extern int __winapk_n2_open(const char* path, int flags, uint32_t mode);
extern int __winapk_n2_openat(int dirfd, const char* path, int flags, uint32_t mode);
extern int __winapk_n2_fcntl(int fd, int cmd, uint64_t arg);
extern int __winapk_n2_ioctl(int fd, uint64_t req, uint64_t arg);
extern int __winapk_n2_prctl(int option, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5);
extern long __winapk_n2_syscall(long num, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6);

static size_t g_strlen(const char* s) {
    size_t n = 0;
    if (!s) return 0;
    while (s[n]) n++;
    return n;
}

static size_t g_strnlen(const char* s, size_t lim) {
    size_t n = 0;
    if (!s) return 0;
    while (n < lim && s[n]) n++;
    return n;
}

struct out_buf {
    char* dst;
    size_t cap;
    size_t len;
};

static void out_put(struct out_buf* o, const char* src, size_t n) {
    for (size_t i = 0; i < n; i++) {
        if (o->dst && o->cap > 0 && o->len + 1 < o->cap) {
            o->dst[o->len] = src[i];
        }
        o->len++;
    }
}

static void out_pad(struct out_buf* o, char ch, int count) {
    for (int i = 0; i < count; i++) out_put(o, &ch, 1);
}

int vsnprintf(char* dst, size_t cap, const char* fmt, va_list ap_in) {
    if (!fmt) {
        if (dst && cap > 0) dst[0] = '\0';
        errno = EINVAL;
        return -1;
    }
    struct out_buf out = {dst, cap, 0};
    va_list ap;
    va_copy(ap, ap_in);

    while (*fmt) {
        if (*fmt != '%') {
            out_put(&out, fmt++, 1);
            continue;
        }
        fmt++;
        if (*fmt == '%') {
            out_put(&out, fmt++, 1);
            continue;
        }

        // Parse flags, width, precision, length modifier in guest code so `va_arg` stays 100% guest-ABI.
        int left = 0, plus = 0, space = 0, zero = 0, alt = 0;
        for (;;) {
            if (*fmt == '-') left = 1;
            else if (*fmt == '+') plus = 1;
            else if (*fmt == ' ') space = 1;
            else if (*fmt == '0') zero = 1;
            else if (*fmt == '#') alt = 1;
            else break;
            fmt++;
        }

        int has_width = 0, width = 0;
        if (*fmt == '*') {
            fmt++;
            has_width = 1;
            width = va_arg(ap, int);
            if (width < 0) { left = 1; width = -width; }
        } else if (*fmt >= '0' && *fmt <= '9') {
            has_width = 1;
            while (*fmt >= '0' && *fmt <= '9') {
                width = width * 10 + (*fmt++ - '0');
                if (width > 65536) width = 65536;
            }
        }

        int has_prec = 0, prec = 0;
        if (*fmt == '.') {
            fmt++;
            has_prec = 1;
            if (*fmt == '*') {
                fmt++;
                prec = va_arg(ap, int);
                if (prec < 0) { has_prec = 0; prec = 0; }
            } else {
                while (*fmt >= '0' && *fmt <= '9') {
                    prec = prec * 10 + (*fmt++ - '0');
                    if (prec > 65536) prec = 65536;
                }
            }
        }

        // Length modifier: 0=none, 1=hh, 2=h, 3=l, 4=ll/j, 5=z/t, 6=L
        int len_mod = 0;
        if (*fmt == 'h') {
            fmt++;
            len_mod = 2;
            if (*fmt == 'h') { fmt++; len_mod = 1; }
        } else if (*fmt == 'l') {
            fmt++;
            len_mod = 3;
            if (*fmt == 'l') { fmt++; len_mod = 4; }
        } else if (*fmt == 'j') {
            fmt++;
            len_mod = 4;
        } else if (*fmt == 'z' || *fmt == 't') {
            fmt++;
            len_mod = 5;
        } else if (*fmt == 'L') {
            fmt++;
            len_mod = 6;
        }

        char conv = *fmt;
        if (!conv) break;
        fmt++;

        if (conv == 's') {
            const char* s = va_arg(ap, const char*);
            if (!s) s = "(null)";
            size_t slen = has_prec ? g_strnlen(s, (size_t)prec) : g_strlen(s);
            int pad = (has_width && width > (int)slen) ? (width - (int)slen) : 0;
            if (!left) out_pad(&out, ' ', pad);
            out_put(&out, s, slen);
            if (left) out_pad(&out, ' ', pad);
            continue;
        }
        if (conv == 'c') {
            char ch = (char)va_arg(ap, int);
            int pad = (has_width && width > 1) ? (width - 1) : 0;
            if (!left) out_pad(&out, ' ', pad);
            out_put(&out, &ch, 1);
            if (left) out_pad(&out, ' ', pad);
            continue;
        }
        if (conv == 'n') {
            if (len_mod == 4 || len_mod == 3 || len_mod == 5) {
                int64_t* p = va_arg(ap, int64_t*);
                if (p) *p = (int64_t)out.len;
            } else {
                int* p = va_arg(ap, int*);
                if (p) *p = (int)out.len;
            }
            continue;
        }

        // Build normalized single-item format spec `%[flags][width][.prec]ll<conv>` or `%...<float_conv>`
        char spec[48];
        int sp = 0;
        spec[sp++] = '%';
        if (left) spec[sp++] = '-';
        if (plus) spec[sp++] = '+';
        if (space) spec[sp++] = ' ';
        if (zero) spec[sp++] = '0';
        if (alt) spec[sp++] = '#';
        if (has_width) {
            char tmp[16];
            int ti = 0, wv = width;
            do { tmp[ti++] = (char)('0' + (wv % 10)); wv /= 10; } while (wv > 0 && ti < 15);
            while (ti > 0) spec[sp++] = tmp[--ti];
        }
        if (has_prec) {
            spec[sp++] = '.';
            char tmp[16];
            int ti = 0, pv = prec;
            do { tmp[ti++] = (char)('0' + (pv % 10)); pv /= 10; } while (pv > 0 && ti < 15);
            while (ti > 0) spec[sp++] = tmp[--ti];
        }

        char piece[512];
        if (conv == 'd' || conv == 'i') {
            int64_t v;
            if (len_mod == 1) v = (int8_t)va_arg(ap, int);
            else if (len_mod == 2) v = (int16_t)va_arg(ap, int);
            else if (len_mod == 3) v = va_arg(ap, long);
            else if (len_mod == 4) v = va_arg(ap, long long);
            else if (len_mod == 5) v = va_arg(ap, ptrdiff_t);
            else v = va_arg(ap, int);
            spec[sp++] = 'l'; spec[sp++] = 'l'; spec[sp++] = conv; spec[sp] = '\0';
            int n = __winapk_n2_fmt_int(piece, sizeof(piece), spec, (uint64_t)v, 1);
            if (n > 0) out_put(&out, piece, (size_t)n < sizeof(piece) ? (size_t)n : sizeof(piece) - 1);
        } else if (conv == 'u' || conv == 'x' || conv == 'X' || conv == 'o') {
            uint64_t v;
            if (len_mod == 1) v = (uint8_t)va_arg(ap, unsigned int);
            else if (len_mod == 2) v = (uint16_t)va_arg(ap, unsigned int);
            else if (len_mod == 3) v = va_arg(ap, unsigned long);
            else if (len_mod == 4) v = va_arg(ap, unsigned long long);
            else if (len_mod == 5) v = va_arg(ap, size_t);
            else v = va_arg(ap, unsigned int);
            spec[sp++] = 'l'; spec[sp++] = 'l'; spec[sp++] = conv; spec[sp] = '\0';
            int n = __winapk_n2_fmt_int(piece, sizeof(piece), spec, v, 0);
            if (n > 0) out_put(&out, piece, (size_t)n < sizeof(piece) ? (size_t)n : sizeof(piece) - 1);
        } else if (conv == 'p') {
            uintptr_t v = (uintptr_t)va_arg(ap, void*);
            if (v == 0) {
                out_put(&out, "0x0", 3);
            } else {
                spec[sp++] = '#'; spec[sp++] = 'l'; spec[sp++] = 'l'; spec[sp++] = 'x'; spec[sp] = '\0';
                int n = __winapk_n2_fmt_int(piece, sizeof(piece), "%#llx", (uint64_t)v, 0);
                if (n > 0) out_put(&out, piece, (size_t)n < sizeof(piece) ? (size_t)n : sizeof(piece) - 1);
            }
        } else if (conv == 'f' || conv == 'F' || conv == 'e' || conv == 'E' ||
                   conv == 'g' || conv == 'G' || conv == 'a' || conv == 'A') {
            double dv = 0.0;
            if (len_mod == 6) {
                // Android AArch64 `long double` is IEEE 754 binary128 in the `q0..q7` SIMD/FP `va_list` bank.
                // Read as a 16-byte SIMD vector (`q` register bank) and convert 1/15/112 -> 1/11/52 without `__trunctfdf2`.
                typedef uint64_t q128_v __attribute__((vector_size(16)));
                q128_v q = va_arg(ap, q128_v);
                uint64_t lo = q[0];
                uint64_t hi = q[1];
                uint64_t sign = (hi >> 63) << 63;
                int exp15 = (int)((hi >> 48) & 0x7fff);
                uint64_t mant52 = ((hi & 0xffffffffffffULL) << 4) | (lo >> 60);
                uint64_t d64 = sign;
                if (exp15 == 0x7fff) {
                    d64 |= (0x7ffULL << 52) | (mant52 ? (mant52 | (1ULL << 51)) : 0);
                } else if (exp15 != 0) {
                    int exp11 = exp15 - 16383 + 1023;
                    if (exp11 >= 0x7ff) d64 |= (0x7ffULL << 52);
                    else if (exp11 > 0) d64 |= ((uint64_t)exp11 << 52) | mant52;
                }
                union { uint64_t u; double d; } u = {d64};
                dv = u.d;
            } else {
                dv = va_arg(ap, double);
            }
            spec[sp++] = conv; spec[sp] = '\0';
            int n = __winapk_n2_fmt_double(piece, sizeof(piece), spec, dv);
            if (n > 0) out_put(&out, piece, (size_t)n < sizeof(piece) ? (size_t)n : sizeof(piece) - 1);
        } else {
            out_put(&out, &conv, 1);
        }
    }

    va_end(ap);
    if (dst && cap > 0) {
        dst[out.len < cap ? out.len : cap - 1] = '\0';
    }
    return (int)out.len;
}

int snprintf(char* dst, size_t cap, const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int r = vsnprintf(dst, cap, fmt, ap);
    va_end(ap);
    return r;
}

int __vsnprintf_chk(char* dst, size_t cap, int flag, size_t dstlen, const char* fmt, va_list ap) {
    (void)flag;
    size_t bound = dstlen < cap ? dstlen : cap;
    return vsnprintf(dst, bound, fmt, ap);
}

int vsprintf(char* dst, const char* fmt, va_list ap) {
    return vsnprintf(dst, (size_t)0x7fffffff, fmt, ap);
}

int sprintf(char* dst, const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int r = vsprintf(dst, fmt, ap);
    va_end(ap);
    return r;
}

int vasprintf(char** strp, const char* fmt, va_list ap) {
    if (!strp) return -1;
    *strp = NULL;
    va_list copy;
    va_copy(copy, ap);
    int len = vsnprintf(NULL, 0, fmt, copy);
    va_end(copy);
    if (len < 0) return -1;
    char* buf = (char*)malloc((size_t)len + 1);
    if (!buf) return -1;
    vsnprintf(buf, (size_t)len + 1, fmt, ap);
    *strp = buf;
    return len;
}

int asprintf(char** strp, const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int r = vasprintf(strp, fmt, ap);
    va_end(ap);
    return r;
}

int vfprintf(FILE* stream, const char* fmt, va_list ap) {
    char stack_buf[2048];
    va_list copy;
    va_copy(copy, ap);
    int len = vsnprintf(stack_buf, sizeof(stack_buf), fmt, copy);
    va_end(copy);
    if (len < 0) return -1;
    if ((size_t)len < sizeof(stack_buf)) {
        return __winapk_n2_stream_write(stream, stack_buf, (size_t)len);
    }
    char* heap_buf = (char*)malloc((size_t)len + 1);
    if (!heap_buf) return -1;
    vsnprintf(heap_buf, (size_t)len + 1, fmt, ap);
    int w = __winapk_n2_stream_write(stream, heap_buf, (size_t)len);
    free(heap_buf);
    return w;
}

int fprintf(FILE* stream, const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int r = vfprintf(stream, fmt, ap);
    va_end(ap);
    return r;
}

int vprintf(const char* fmt, va_list ap) {
    return vfprintf(stdout, fmt, ap);
}

int printf(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int r = vfprintf(stdout, fmt, ap);
    va_end(ap);
    return r;
}

int vswprintf(wchar_t* ws, size_t n, const wchar_t* wfmt, va_list ap) {
    if (!ws || n == 0 || !wfmt) return -1;
    // Convert ASCII/BMP format template to UTF-8, format, widen back to UTF-32 `wchar_t` (4 bytes on Android).
    char fmt8[1024];
    size_t fi = 0;
    while (wfmt[fi] && fi + 1 < sizeof(fmt8)) {
        fmt8[fi] = (wfmt[fi] < 128) ? (char)wfmt[fi] : '?';
        fi++;
    }
    fmt8[fi] = '\0';
    char out8[2048];
    int len = vsnprintf(out8, sizeof(out8), fmt8, ap);
    if (len < 0) { ws[0] = 0; return -1; }
    size_t copy_n = (size_t)len < (n - 1) ? (size_t)len : (n - 1);
    for (size_t i = 0; i < copy_n; i++) ws[i] = (wchar_t)(unsigned char)out8[i];
    ws[copy_n] = 0;
    return (size_t)len < n ? len : -1;
}

int swprintf(wchar_t* ws, size_t n, const wchar_t* wfmt, ...) {
    va_list ap;
    va_start(ap, wfmt);
    int r = vswprintf(ws, n, wfmt, ap);
    va_end(ap);
    return r;
}

int __android_log_vprint(int prio, const char* tag, const char* fmt, va_list ap) {
    char buf[4096];
    int n = vsnprintf(buf, sizeof(buf), fmt, ap);
    if (n < 0) return -1;
    return __winapk_n2_log_write(prio, tag, buf);
}

int __android_log_print(int prio, const char* tag, const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int r = __android_log_vprint(prio, tag, fmt, ap);
    va_end(ap);
    return r;
}

void syslog(int prio, const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    __android_log_vprint(4, "syslog", fmt, ap);
    (void)prio;
    va_end(ap);
}

int vsscanf(const char* str, const char* fmt, va_list ap_in) {
    if (!str || !fmt) return -1;
    const char* s = str;
    const char* f = fmt;
    int assigned = 0;
    va_list ap;
    va_copy(ap, ap_in);

    while (*f) {
        if (*f == ' ' || *f == '\t' || *f == '\n' || *f == '\r') {
            while (*f == ' ' || *f == '\t' || *f == '\n' || *f == '\r') f++;
            while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r') s++;
            continue;
        }
        if (*f != '%') {
            if (*s != *f) break;
            s++; f++;
            continue;
        }
        f++;
        if (*f == '%') {
            if (*s != '%') break;
            s++; f++;
            continue;
        }
        int suppress = 0;
        if (*f == '*') { suppress = 1; f++; }
        char spec[64];
        int sp = 0;
        spec[sp++] = '%';
        if (suppress) spec[sp++] = '*';
        while (*f >= '0' && *f <= '9' && sp < 48) spec[sp++] = *f++;

        int len_mod = 0; // 0=32, 1=8, 2=16, 3=64, 4=double
        if (*f == 'h') {
            spec[sp++] = *f++;
            len_mod = 2;
            if (*f == 'h') { spec[sp++] = *f++; len_mod = 1; }
        } else if (*f == 'l') {
            spec[sp++] = *f++;
            len_mod = 3;
            if (*f == 'l') { spec[sp++] = *f++; len_mod = 3; }
        } else if (*f == 'z' || *f == 'j' || *f == 't') {
            f++;
            spec[sp++] = 'l'; spec[sp++] = 'l';
            len_mod = 3;
        }
        if (*f == '[') {
            spec[sp++] = *f++;
            if (*f == '^') spec[sp++] = *f++;
            if (*f == ']') spec[sp++] = *f++;
            while (*f && *f != ']' && sp < 58) spec[sp++] = *f++;
            if (*f == ']') spec[sp++] = *f++;
            spec[sp] = '\0';
            void* dst = suppress ? NULL : va_arg(ap, void*);
            size_t used = 0;
            int r = __winapk_n2_scan_one(s, spec, dst, suppress ? 0 : 10, &used);
            if (r <= 0 && !suppress) break;
            s += used;
            if (!suppress && r > 0) assigned++;
            continue;
        }

        char conv = *f;
        if (!conv) break;
        f++;
        if (conv == 'n') {
            if (!suppress) {
                if (len_mod == 3) *va_arg(ap, int64_t*) = (int64_t)(s - str);
                else *va_arg(ap, int*) = (int)(s - str);
            }
            continue;
        }
        spec[sp++] = conv;
        spec[sp] = '\0';

        // kind: 0=suppress, 1=i32, 2=i64, 3=u32, 4=u64, 5=f32, 6=f64, 7=i16, 8=i8, 10=str/char
        int kind = 0;
        if (!suppress) {
            if (conv == 'd' || conv == 'i') kind = (len_mod == 3) ? 2 : (len_mod == 2 ? 7 : (len_mod == 1 ? 8 : 1));
            else if (conv == 'u' || conv == 'x' || conv == 'X' || conv == 'o') kind = (len_mod == 3) ? 4 : 3;
            else if (conv == 'p') kind = 4;
            else if (conv == 'f' || conv == 'e' || conv == 'g' || conv == 'F' || conv == 'E' || conv == 'G') kind = (len_mod == 3) ? 6 : 5;
            else kind = 10;
        }
        void* dst = suppress ? NULL : va_arg(ap, void*);
        size_t used = 0;
        int r = __winapk_n2_scan_one(s, spec, dst, kind, &used);
        if (r < 0 && assigned == 0 && !*s) { va_end(ap); return -1; }
        if (r <= 0 && !suppress) break;
        s += used;
        if (!suppress && r > 0) assigned++;
    }
    va_end(ap);
    return assigned;
}

int sscanf(const char* str, const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int r = vsscanf(str, fmt, ap);
    va_end(ap);
    return r;
}

int fscanf(FILE* stream, const char* fmt, ...) {
    char line[4096];
    if (__winapk_n2_stream_read_line(stream, line, sizeof(line)) < 0) return -1;
    va_list ap;
    va_start(ap, fmt);
    int r = vsscanf(line, fmt, ap);
    va_end(ap);
    return r;
}

// Variadic POSIX file/control wrappers extracting optional guest-stack/reg arguments cleanly:

int open(const char* path, int flags, ...) {
    uint32_t mode = 0;
    if (flags & 0100 /* O_CREAT */) {
        va_list ap;
        va_start(ap, flags);
        mode = va_arg(ap, unsigned int);
        va_end(ap);
    }
    return __winapk_n2_open(path, flags, mode);
}

int open64(const char* path, int flags, ...) {
    uint32_t mode = 0;
    if (flags & 0100) {
        va_list ap;
        va_start(ap, flags);
        mode = va_arg(ap, unsigned int);
        va_end(ap);
    }
    return __winapk_n2_open(path, flags, mode);
}

int openat(int dirfd, const char* path, int flags, ...) {
    uint32_t mode = 0;
    if (flags & 0100) {
        va_list ap;
        va_start(ap, flags);
        mode = va_arg(ap, unsigned int);
        va_end(ap);
    }
    return __winapk_n2_openat(dirfd, path, flags, mode);
}

int openat64(int dirfd, const char* path, int flags, ...) {
    uint32_t mode = 0;
    if (flags & 0100) {
        va_list ap;
        va_start(ap, flags);
        mode = va_arg(ap, unsigned int);
        va_end(ap);
    }
    return __winapk_n2_openat(dirfd, path, flags, mode);
}

int fcntl(int fd, int cmd, ...) {
    va_list ap;
    va_start(ap, cmd);
    uint64_t arg = va_arg(ap, uint64_t);
    va_end(ap);
    return __winapk_n2_fcntl(fd, cmd, arg);
}

int ioctl(int fd, int req, ...) {
    va_list ap;
    va_start(ap, req);
    uint64_t arg = va_arg(ap, uint64_t);
    va_end(ap);
    return __winapk_n2_ioctl(fd, (uint64_t)(unsigned int)req, arg);
}

int prctl(int option, ...) {
    va_list ap;
    va_start(ap, option);
    uint64_t a2 = va_arg(ap, uint64_t);
    uint64_t a3 = va_arg(ap, uint64_t);
    uint64_t a4 = va_arg(ap, uint64_t);
    uint64_t a5 = va_arg(ap, uint64_t);
    va_end(ap);
    return __winapk_n2_prctl(option, a2, a3, a4, a5);
}

long syscall(long num, ...) {
    va_list ap;
    va_start(ap, num);
    uint64_t a1 = va_arg(ap, uint64_t);
    uint64_t a2 = va_arg(ap, uint64_t);
    uint64_t a3 = va_arg(ap, uint64_t);
    uint64_t a4 = va_arg(ap, uint64_t);
    uint64_t a5 = va_arg(ap, uint64_t);
    uint64_t a6 = va_arg(ap, uint64_t);
    va_end(ap);
    return __winapk_n2_syscall(num, a1, a2, a3, a4, a5, a6);
}

// Pure AArch64 guest-domain `setjmp` and `longjmp` (Android `jmp_buf` is 32x `long` = 256 bytes;
// we save callee-saved `x19..x30`, `sp`, and `d8..d15` in the first 176 bytes).
__asm__(
".text\n"
".globl setjmp\n"
".type setjmp, %function\n"
".globl _setjmp\n"
".type _setjmp, %function\n"
"setjmp:\n"
"_setjmp:\n"
"    stp x19, x20, [x0, #0]\n"
"    stp x21, x22, [x0, #16]\n"
"    stp x23, x24, [x0, #32]\n"
"    stp x25, x26, [x0, #48]\n"
"    stp x27, x28, [x0, #64]\n"
"    stp x29, x30, [x0, #80]\n"
"    mov x9, sp\n"
"    str x9, [x0, #96]\n"
"    stp d8, d9, [x0, #104]\n"
"    stp d10, d11, [x0, #120]\n"
"    stp d12, d13, [x0, #136]\n"
"    stp d14, d15, [x0, #152]\n"
"    mov w0, #0\n"
"    ret\n"
"\n"
".globl longjmp\n"
".type longjmp, %function\n"
".globl _longjmp\n"
".type _longjmp, %function\n"
"longjmp:\n"
"_longjmp:\n"
"    ldp x19, x20, [x0, #0]\n"
"    ldp x21, x22, [x0, #16]\n"
"    ldp x23, x24, [x0, #32]\n"
"    ldp x25, x26, [x0, #48]\n"
"    ldp x27, x28, [x0, #64]\n"
"    ldp x29, x30, [x0, #80]\n"
"    ldr x9, [x0, #96]\n"
"    mov sp, x9\n"
"    ldp d8, d9, [x0, #104]\n"
"    ldp d10, d11, [x0, #120]\n"
"    ldp d12, d13, [x0, #136]\n"
"    ldp d14, d15, [x0, #152]\n"
"    cmp w1, #0\n"
"    csinc w0, w1, wzr, ne\n"
"    ret\n"
);
