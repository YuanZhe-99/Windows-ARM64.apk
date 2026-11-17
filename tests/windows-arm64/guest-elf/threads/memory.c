// T08 memory: real Android prototypes; no libc dependency; -fno-builtin keeps shim calls observable.
#include <errno.h>
#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

extern int t08_host_state(const int* guest_errno);

/// Checks zero sizes, alignment, zeroing, overflow and failure-preserving realloc.
static int allocations(void) {
    errno = 173;
    void* zero = malloc(0);
    void* empty = calloc(0, SIZE_MAX);
    if (!zero || !empty || ((uintptr_t)zero & 15) || ((uintptr_t)empty & 15)) return 1;
    free(zero);
    free(empty);
    free(NULL);
    unsigned char* pointer = calloc(17, 3);
    if (!pointer || ((uintptr_t)pointer & 15)) return 2;
    for (int i = 0; i < 51; i++) {
        if (pointer[i]) return 3;
        pointer[i] = (unsigned char)(i + 1);
    }
    pointer = realloc(pointer, 127);
    if (!pointer) return 4;
    for (int i = 0; i < 51; i++) if (pointer[i] != i + 1) return 5;
    pointer = realloc(pointer, 17);
    if (!pointer || errno != 173) return 6;
    volatile size_t huge = SIZE_MAX;
    if (realloc(pointer, huge) || errno != ENOMEM) return 7;
    for (int i = 0; i < 17; i++) if (pointer[i] != i + 1) return 8;
    errno = 173;
    if (calloc(huge, 2) || errno != ENOMEM) return 9;
    errno = 173;
    if (malloc(huge) || errno != ENOMEM) return 10;
    errno = 173;
    if (realloc(pointer, 0) || errno != 173) return 11;
    pointer = realloc(NULL, 23);
    if (!pointer || errno != 173) return 12;
    free(pointer);
    return errno == 173 && t08_host_state(&errno) ? 0 : 13;
}

/// Checks byte wrappers' bounds, overlap direction, unsigned comparison and empty ranges.
static int bytes(void) {
    unsigned char data[10] = {99, 0, 1, 2, 3, 4, 5, 6, 7, 98};
    const unsigned char high[] = {0xff}, low[] = {1};
    errno = 174;
    if (memmove(data + 3, data + 1, 5) != data + 3 ||
        memcmp(data, (unsigned char[]){99, 0, 1, 0, 1, 2, 3, 4, 7, 98}, 10)) return 1;
    if (memmove(data + 1, data + 3, 5) != data + 1 ||
        memcmp(data, (unsigned char[]){99, 0, 1, 2, 3, 4, 3, 4, 7, 98}, 10)) return 2;
    if (memset(data + 1, 0x181, 8) != data + 1 || data[0] != 99 || data[9] != 98) return 3;
    for (int i = 1; i < 9; i++) if (data[i] != 0x81) return 4;
    if (memcpy(data + 1, low, 1) != data + 1 || data[1] != 1 ||
        memcpy(data + 2, high, 0) != data + 2 || data[2] != 0x81 ||
        memcmp(high, low, 1) <= 0 || memcmp(low, high, 1) >= 0 || memcmp(high, low, 0)) return 5;
    if (memchr(data, 0x181, 10) != data + 2 || memchr(data, 55, 10) ||
        memchr(data, 99, 0)) return 6;
    return errno == 174 && t08_host_state(&errno) ? 0 : 7;
}

/// Checks termination/padding, unsigned ordering, missing matches and byte-preserving UTF-8.
static int strings(void) {
    const char* utf8 = "caf\xc3\xa9 \xe4\xb8\xad\xe6\x96\x87";
    const char raw[] = {'a', 'b', 'c', 'd'};
    char buffer[20];
    errno = 175;
    if (strlen(utf8) != 12 || strnlen(raw, 4) != 4 || strnlen(utf8, 6) != 6 ||
        strnlen(utf8, 20) != 12 || strnlen(raw, 0)) return 1;
    if (strcmp("\xff", "\x01") <= 0 || strcmp("abc", "abc") || strcmp("a", "ab") >= 0 ||
        strncmp("abc", "abd", 2) || strncmp("abc", "abd", 3) >= 0 || strncmp("a", "z", 0)) return 2;
    memset(buffer, 77, sizeof buffer);
    if (strcpy(buffer, utf8) != buffer || memcmp(buffer, utf8, 13) || buffer[13] != 77) return 3;
    if (strncpy(buffer, "a", 4) != buffer || memcmp(buffer, "a\0\0", 4) ||
        strncpy(buffer + 4, raw, 3) != buffer + 4 || memcmp(buffer + 4, "abc", 3) ||
        buffer[7] != utf8[7]) return 4;
    const char* text = "abca";
    if (strchr(text, 'a') != text || strrchr(text, 'a') != text + 3 ||
        strchr(text, 0) != text + 4 || strrchr(text, 0) != text + 4 ||
        strchr(text, 'z') || strrchr(text, 'z') || strstr(text, "bc") != text + 1 ||
        strstr(text, "") != text || strstr(text, "abcd")) return 5;
    char* copy = strdup(utf8);
    if (!copy || strcmp(copy, utf8)) return 6;
    free(copy);
    return errno == 175 && t08_host_state(&errno) ? 0 : 7;
}

/// Frees a parent allocation and returns a new owned string across the guest thread boundary.
static void* transfer(void* pointer) {
    if (errno || ((unsigned char*)pointer)[0] != 42) return NULL;
    errno = 176;
    free(pointer);
    char* result = strdup("worker");
    return errno == 176 && t08_host_state(&errno) ? result : NULL;
}

/// Checks allocation-table ownership shared across eight joinable guest threads.
static int sharing(void) {
    pthread_t workers[8];
    int count = 0, failed = 0;
    errno = 177;
    for (int i = 0; i < 8; i++) {
        unsigned char* pointer = malloc(64);
        if (!pointer) { failed = 1; break; }
        pointer[0] = 42;
        if (pthread_create(&workers[i], NULL, transfer, pointer)) {
            free(pointer);
            failed = 2;
            break;
        }
        count++;
    }
    for (int i = 0; i < count; i++) {
        void* result = NULL;
        if (pthread_join(workers[i], &result) || !result || strcmp(result, "worker")) failed = 3;
        free(result);
    }
    return failed ? failed : errno == 177 && t08_host_state(&errno) ? 0 : 4;
}

/// Dispatches compact positive cases or one deliberately fatal ownership violation.
void* t08_case(void* argument) {
#if defined(T08_MEMORY_INVALID)
    (void)argument;
    volatile uintptr_t pointer = 1;
    free((void*)pointer);
    return (void*)1;
#elif defined(T08_MEMORY_DOUBLE)
    (void)argument;
    void* pointer = malloc(13);
    volatile uintptr_t stale = (uintptr_t)pointer;
    free(pointer);
    free((void*)stale);
    return (void*)1;
#else
    int result;
    switch ((uintptr_t)argument) {
    case 0: result = allocations(); break;
    case 1: result = bytes(); break;
    case 2: result = strings(); break;
    case 3: result = sharing(); break;
    default: result = 99;
    }
    return (void*)(uintptr_t)result;
#endif
}
