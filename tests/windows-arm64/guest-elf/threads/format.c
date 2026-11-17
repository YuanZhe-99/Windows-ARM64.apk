// T08: calls the real Android varargs declarations; helper ELF keeps all va_list handling guest-side.
#include <android/log.h>
#include <errno.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern int t08_host_state(const int* guest_errno);

/// Checks LP64 integers, register-to-stack overflow and bounded field rendering.
static int integers(void) {
    char text[256];
    errno = 201;
    const char* expected = "1,2,3,4,5,6,7,8,9,10|-9223372036854775808|18446744073709551615";
    int size = snprintf(text, sizeof text, "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d|%ld|%zu",
                        1, 2, 3, 4, 5, 6, 7, 8, 9, 10, (long)INT64_MIN, SIZE_MAX);
    if (size != (int)strlen(expected) || strcmp(text, expected)) return 1;
    expected = "-0000017|0x00002a|abc   |0023  ||0x1234";
    size = snprintf(text, sizeof text, "%+08d|%#08x|%-6.3s|%*.*d|%.0u|%p",
                    -17, 42u, "abcdef", -6, 4, 23, 0u, (void*)(uintptr_t)0x1234);
    if (size != (int)strlen(expected) || strcmp(text, expected)) return 2;
    expected = "FFFFFFFFFFFFFFFF|-1|255|-1|65535";
    size = snprintf(text, sizeof text, "%llX|%hhd|%hhu|%hd|%hu",
                    (unsigned long long)UINT64_MAX, 255, -1, 65535, -1);
    if (size != (int)strlen(expected) || strcmp(text, expected)) return 3;
    return errno == 201 && t08_host_state(&errno) ? 0 : 4;
}

/// Copies a guest va_list and verifies both V calls independently consume the same arguments.
static int copied(const char* format, ...) {
    char first[128], second[128];
    va_list one, two;
    va_start(one, format);
    va_copy(two, one);
    int a = vsnprintf(first, sizeof first, format, one);
    int b = vsnprintf(second, sizeof second, format, two);
    va_end(two);
    va_end(one);
    const char* expected = "v=4294967297:1,2,3,4,5,6,7,8:X";
    return a == b && a == (int)strlen(expected) && !strcmp(first, expected) && !strcmp(first, second);
}

/// Checks V variants and va_copy without handing a guest va_list to any host function.
static int lists(void) {
    errno = 202;
    if (!copied("v=%ld:%d,%d,%d,%d,%d,%d,%d,%d:%c", (long)4294967297LL, 1, 2, 3, 4, 5, 6, 7, 8, 'X')) return 1;
    return errno == 202 && t08_host_state(&errno) ? 0 : 2;
}

/// Checks full counts, termination/sentinels, unsupported formats and bounded parsing failures.
static int bounds(void) {
    char text[12];
    errno = 203;
    memset(text, 0x55, sizeof text);
    if (snprintf(text, 4, "%s", "abcdef") != 6 || memcmp(text, "abc", 4) || text[4] != 0x55 ||
        snprintf(NULL, 0, "%d:%s", 123, "abc") != 7 || snprintf(text, 1, "%d", 123) != 3 ||
        text[0] || text[1] != 'b' || errno != 203) return 1;
    if (snprintf(text, sizeof text, "a%cb", 0) != 3 || memcmp(text, "a\0b", 4)) return 2;
    int forbidden = 77;
    const char* write_count = "%n";  // Deliberate runtime refusal; Android's format checker rejects a literal.
    if (snprintf(text, sizeof text, write_count, &forbidden) != -1 || errno != ENOTSUP || forbidden != 77 || text[0] ||
        snprintf(text, sizeof text, "%f", 1.5) != -1 || errno != ENOTSUP || text[0] ||
        snprintf(text, sizeof text, "%ls", L"wide") != -1 || errno != ENOTSUP) return 3;
    const char* positional = "%2$d";
    if (snprintf(text, sizeof text, positional, 1, 2) != -1 || errno != ENOTSUP ||
        snprintf(text, sizeof text, "%*d", 4097, 1) != -1 || errno != EOVERFLOW || text[0]) return 4;
    char* oversized = malloc(1024 * 1024 + 2);
    if (!oversized) return 5;
    memset(oversized, 'a', 1024 * 1024 + 1);
    oversized[1024 * 1024 + 1] = 0;
    int size = snprintf(NULL, 0, "%s", oversized);
    int error = errno;
    oversized[4097] = 0;
    int format_size = snprintf(text, sizeof text, oversized, 0);
    int format_error = errno;
    free(oversized);
    if (size != -1 || error != EOVERFLOW || format_size != -1 || format_error != EOVERFLOW) return 6;
    return t08_host_state(&errno) ? 0 : 7;
}

/// Calls the guest Android log V entry with a genuine guest va_list.
static int log_v(const char* format, ...) {
    va_list arguments;
    va_start(arguments, format);
    int result = __android_log_vprint(3, "T08", format, arguments);
    va_end(arguments);
    return result;
}

/// Emits exact UTF-8/literal-percent bytes and rejects unsupported priorities and overflow.
static int logs(void) {
    const char* chinese = "\xe4\xb8\xad\xe6\x96\x87";
    errno = 204;
    if (__android_log_write(4, "T08", "literal %ld UTF8 \xe4\xb8\xad\xe6\x96\x87") != 1 ||
        __android_log_print(5, "T08", "n=%ld text=%.6s %%", (long)4294967297LL, chinese) != 1 ||
        log_v("v=%d:%s", 7, "ok") != 1 || errno != 204) return 1;
    if (__android_log_write(1, "T08", "bad") != -1 || errno != EINVAL) return 2;
    char tag[129];
    memset(tag, 't', 128);
    tag[128] = 0;
    if (__android_log_write(4, tag, "bad") != -1 || errno != EOVERFLOW ||
        __android_log_print(4, "T08", "%*s", 4096, "") != -1 || errno != EOVERFLOW ||
        __android_log_print(4, "T08", "%f", 2.5) != -1 || errno != ENOTSUP) return 3;
    return t08_host_state(&errno) ? 0 : 4;
}

/// Dispatches four compact formatter/log cases with no long stress loop.
void* t08_case(void* argument) {
    int result;
    switch ((uintptr_t)argument) {
    case 0: result = integers(); break;
    case 1: result = lists(); break;
    case 2: result = bounds(); break;
    case 3: result = logs(); break;
    default: result = 99;
    }
    return (void*)(uintptr_t)result;
}
