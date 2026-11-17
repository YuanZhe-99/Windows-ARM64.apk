// T07/N1 admission: a normally linked NDK C library, not a -nostdlib leaf.
#include <android/log.h>
#include <dlfcn.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>

extern int __cxa_atexit(void (*function)(void*), void* argument, void* dso);
extern void __cxa_finalize(void* dso);
extern int __register_atfork(void (*prepare)(void), void (*parent)(void), void (*child)(void), void* dso);
extern void t07_n1_trace(int value, const int* guest_errno);
static int identity, first_dso, second_dso, fork_dso, quota_dso, order, fork_calls, exit_calls, callback_failure;

/// Supplies an entry that must never start during runtime teardown.
static void* forbidden_worker(void* pointer) { return pointer; }

/// Proves process-lifetime callbacks run in a managed context before code and heap teardown.
static void late(void* argument) {
    pthread_t worker;
    void* allocation = malloc(17);
    int ok = allocation && argument == &identity && pthread_create(&worker, NULL, forbidden_worker, NULL) == ENOTSUP;
    free(allocation);
    t07_n1_trace(ok ? 9 : 8, &errno);
}

/// Records real constructor execution and a pending DSO callback only after all linking succeeds.
__attribute__((constructor)) static void initialize(void) {
    t07_n1_trace(1, &errno);
    if (__cxa_atexit(late, &identity, &identity)) callback_failure = 1;
}

/// Records destructor order and registers another callback during finalization to test reentry.
static void record(void* argument) {
    int value = (int)(uintptr_t)argument;
    order = order * 10 + value;
    if (value == 3 && __cxa_atexit(record, (void*)(uintptr_t)4, &first_dso)) callback_failure = 1;
}

/// Makes any wrongly invoked fork hook visible through the fixture's return value.
static void fork_hook(void) { fork_calls++; }

/// Counts actual quota registrations invoked by explicit finalization.
static void quota_callback(void* argument) { (void)argument; exit_calls++; }

/// Checks versioned Bionic/liblog/libdl imports with exact guest LP64/errno/handle semantics.
int bionic_value(void) {
#if defined(BIONIC_MISSING)
    return (int)fork();
#else
    errno = 211;
    char* text = malloc(64);
    if (!text) return 1;
    int size = snprintf(text, 64, "N1:%ld", (long)4294967297LL);
    int result = size == 13 && !strcmp(text, "N1:4294967297") && errno == 211;
    free(text);
    void* library = dlopen("libc.so", RTLD_NOW | RTLD_LOCAL);
    if (!library || !dlsym(library, "malloc") || dlerror() || dlclose(library)) return 2;
    if (__android_log_print(4, "T07", "%s", "versioned Bionic") != 1) return 3;
    return result ? 0 : 4;
#endif
}

/// Checks DSO selection, reverse/reentrant exactly-once finalization and real bounded fork records.
int bionic_lifecycle(void) {
    errno = 212;
    if (__cxa_atexit(record, (void*)(uintptr_t)1, &first_dso) ||
        __cxa_atexit(record, (void*)(uintptr_t)2, &second_dso) ||
        __cxa_atexit(record, (void*)(uintptr_t)3, &first_dso)) return 1;
    __cxa_finalize(&first_dso);
    if (order != 341 || callback_failure) return 2;
    __cxa_finalize(&first_dso);
    __cxa_finalize(&second_dso);
    if (order != 3412 || errno != 212) return 3;
    for (int i = 0; i < 128; i++)
        if (__register_atfork(fork_hook, fork_hook, fork_hook, &fork_dso)) return 4;
    if (__register_atfork(fork_hook, fork_hook, fork_hook, &fork_dso) != ENOMEM || errno != 212 || fork_calls) return 5;
    __cxa_finalize(&fork_dso);
    if (__register_atfork(fork_hook, fork_hook, fork_hook, &fork_dso)) return 6;
    __cxa_finalize(&fork_dso);
    if (fork_calls || callback_failure) return 7;
    if (__cxa_atexit(NULL, NULL, &quota_dso) != -1 || errno != EINVAL) return 8;
    errno = 212;
    // The constructor retains one pending callback; exactly 255 more fit this declared profile.
    for (int i = 0; i < 255; i++) if (__cxa_atexit(quota_callback, NULL, &quota_dso)) return 9;
    if (__cxa_atexit(quota_callback, NULL, &quota_dso) != -1 || errno != ENOMEM) return 10;
    errno = 212;
    __cxa_finalize(&quota_dso);
    return exit_calls == 255 && errno == 212 ? 0 : 11;
}
