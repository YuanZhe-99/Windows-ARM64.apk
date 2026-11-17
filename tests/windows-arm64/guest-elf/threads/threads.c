// T08: real Android declarations, explicit emulated TLS, no libc/DT_NEEDED or ELF PT_TLS.
// Built only from this audited source; never evidence for ordinary NDK default binaries.
#include <errno.h>
#include <pthread.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>

_Static_assert(sizeof(pthread_t) == 8, "Android LP64 pthread_t");
_Static_assert(sizeof(pthread_key_t) == 4, "Android key layout");

// Supplied by the probe, checking x18/sp and separation from host CRT errno at each callback.
extern int t08_host_state(const int* guest_errno);
/// Resolves the compiler's emutls descriptor; the malformed-descriptor case calls it explicitly.
extern void* __emutls_get_address(void* descriptor);

static __thread int initialized = 41;
static __thread uint64_t zero;
static __thread unsigned char aligned[65] __attribute__((aligned(64)));
static pthread_key_t key;
static int release_workers;

/// Captures each worker's independent state until its join completes.
struct result {
    int id, ready, failed, destructors, rounds;
    pthread_t self;
    uintptr_t errno_address, initialized_address, zero_address, aligned_address;
};

/// Checks key clearing before callbacks, TLS lifetime, re-arming and the four-round limit.
static void destroy(void* pointer) {
    struct result* result = pointer;
    if (pthread_getspecific(key) != NULL || errno != 1000 + result->id ||
        initialized != 2000 + result->id || zero != (uint64_t)(3000 + result->id) ||
        aligned[64] != (unsigned char)result->id || !t08_host_state(&errno))
        result->failed = 1;
    result->destructors++;
    if (result->destructors < result->rounds && pthread_setspecific(key, result) != 0)
        result->failed = 1;
}

/// Keeps all workers alive simultaneously, then returns an exact pointer after isolated writes.
static void* worker(void* pointer) {
    struct result* result = pointer;
    result->self = pthread_self();
    result->errno_address = (uintptr_t)&errno;
    result->initialized_address = (uintptr_t)&initialized;
    result->zero_address = (uintptr_t)&zero;
    result->aligned_address = (uintptr_t)&aligned[0];
    if (errno != 0 || initialized != 41 || zero != 0 || aligned[0] != 0 || aligned[64] != 0 ||
        ((uintptr_t)&aligned[0] & 63) || pthread_getspecific(key) != NULL ||
        !t08_host_state(&errno))
        result->failed = 1;
    errno = 1000 + result->id;
    initialized = 2000 + result->id;
    zero = (uint64_t)(3000 + result->id);
    aligned[64] = (unsigned char)result->id;
    if (pthread_setspecific(key, result) != 0 || pthread_getspecific(key) != result)
        result->failed = 1;
    __atomic_store_n(&result->ready, 1, __ATOMIC_RELEASE);
    while (!__atomic_load_n(&release_workers, __ATOMIC_ACQUIRE)) {}
    if (errno != 1000 + result->id || initialized != 2000 + result->id ||
        !pthread_equal(result->self, pthread_self()))
        result->failed = 1;
    return result;
}

/// Checks concurrent contexts, full pointer-sized join results and TLS destructor rounds.
static int threads(int count, int rounds) {
    struct result results[32] = {0};
    pthread_t tokens[32];
    int created = 0, failure = 0;
    errno = 17777;
    if (initialized != 41 || zero != 0 || aligned[64] != 0) return 1;
    initialized = 17778;
    zero = 17779;
    if (pthread_key_create(&key, destroy)) return 2;
    release_workers = 0;
    for (int i = 0; i < count; i++) {
        results[i].id = i + 1;
        results[i].rounds = rounds;
        if (pthread_create(&tokens[i], NULL, worker, &results[i])) { failure = 3; break; }
        created++;
    }
    for (int i = 0; i < created; i++)
        while (!__atomic_load_n(&results[i].ready, __ATOMIC_ACQUIRE)) {}
    for (int i = 0; i < created; i++) {
        if (results[i].self != tokens[i] || pthread_equal(tokens[i], pthread_self()) ||
            results[i].errno_address == (uintptr_t)&errno ||
            results[i].initialized_address == (uintptr_t)&initialized)
            failure = 4;
        for (int j = 0; j < i; j++)
            if (tokens[i] == tokens[j] || results[i].errno_address == results[j].errno_address ||
                results[i].initialized_address == results[j].initialized_address ||
                results[i].zero_address == results[j].zero_address ||
                results[i].aligned_address == results[j].aligned_address)
                failure = 5;
    }
    __atomic_store_n(&release_workers, 1, __ATOMIC_RELEASE);
    for (int i = 0; i < created; i++) {
        void* value = NULL;
        if (pthread_join(tokens[i], &value) || value != &results[i] || results[i].failed ||
            results[i].destructors != (rounds > 4 ? 4 : rounds))
            failure = 6;
        if (pthread_join(tokens[i], NULL) != ESRCH) failure = 7;
    }
    if (errno != 17777 || initialized != 17778 || zero != 17779 || aligned[64] != 0 ||
        pthread_getspecific(key) != NULL || !t08_host_state(&errno))
        failure = 8;
    if (pthread_key_delete(key)) failure = 9;
    return failure;
}

/// Exercises deletion without callbacks and generation-safe slot reuse.
static int deleted_keys(void) {
    pthread_key_t first, second;
    struct result result = {0};
    errno = 712;
    if (pthread_key_create(&first, destroy) || pthread_setspecific(first, &result)) return 1;
    if (pthread_key_delete(first) || result.destructors != 0) return 2;
    if (pthread_key_create(&second, NULL)) return 3;
    int failure = 0;
    if (first == second || pthread_getspecific(first) != NULL || pthread_getspecific(second) != NULL ||
        pthread_setspecific(first, &result) != EINVAL || pthread_key_delete(first) != EINVAL ||
        pthread_setspecific(-1, &result) != EINVAL || pthread_key_delete(-1) != EINVAL)
        failure = 4;
    if (pthread_setspecific(second, &result) || pthread_getspecific(second) != &result ||
        pthread_setspecific(second, NULL) || pthread_getspecific(second) != NULL)
        failure = 5;
    if (pthread_key_delete(second) || errno != 712) failure = 6;
    return failure;
}

/// Proves key exhaustion fails explicitly and every slot can be reclaimed.
static int exhausted_keys(void) {
    pthread_key_t keys[128], extra;
    int count = 0, failure = 0;
    errno = 713;
    for (; count < 128; count++)
        if (pthread_key_create(&keys[count], NULL)) { failure = 1; break; }
    if (count == 128 && pthread_key_create(&extra, NULL) != EAGAIN) failure = 2;
    for (int i = 0; i < count; i++)
        if (pthread_key_delete(keys[i])) failure = 3;
    if (pthread_key_create(&extra, NULL) || pthread_key_delete(extra) || errno != 713) failure = 4;
    return failure;
}

/// Checks unsupported attributes and invalid/self joins without changing guest errno.
static int errors(void) {
    pthread_attr_t attributes = {0};
    pthread_t token = 0;
    errno = 714;
    if (pthread_create(&token, &attributes, worker, NULL) != ENOTSUP || token != 0) return 1;
    if (pthread_join(pthread_self(), NULL) != EDEADLK || pthread_join(-1, NULL) != ESRCH) return 2;
    return errno == 714 ? 0 : 3;
}

/// Checks the compiler-required byte shim's return value, unsigned-byte conversion and bounds.
static int bytes(void) {
    unsigned char data[7] = {1, 2, 3, 4, 5, 6, 7};
    void* (*volatile fill)(void*, int, size_t) = memset;  // Do not fold the import into stores.
    if (fill(data + 1, 0x1ff, 5) != data + 1 || data[0] != 1 || data[6] != 7) return 1;
    for (int i = 1; i < 6; i++) if (data[i] != 255) return 2;
    if (fill(data, 0, 0) != data || data[0] != 1) return 3;
    if (fill(data, 0, sizeof data) != data) return 4;
    for (int i = 0; i < 7; i++) if (data[i] != 0) return 5;
    return 0;
}

#if defined(T08_UNJOINED)
/// Returns normally while deliberately leaving its joinable registry entry for the fatal test.
static void* unjoined(void* argument) { return argument; }
#endif

/// Dispatches one deterministic T08 case, returning zero or the numbered failed assertion.
void* t08_case(void* argument) {
#if defined(T08_MISSING)
    (void)argument;
    pthread_detach(pthread_self());  // Not implemented: must be refused before any entry runs.
    return NULL;
#elif defined(T08_BAD_EMUTLS)
    (void)argument;
    struct { uint64_t size, alignment, object; const void* initializer; } descriptor = {1, 3, 0, NULL};
    return __emutls_get_address(&descriptor);  // Must terminate rather than invent a TLS address.
#elif defined(T08_UNJOINED)
    pthread_t token;
    return (void*)(uintptr_t)pthread_create(&token, NULL, unjoined, argument);
#else
    switch ((uintptr_t)argument) {
        case 0: return (void*)(uintptr_t)threads(8, 2);
        case 1: return (void*)(uintptr_t)threads(32, 2);
        case 2: return (void*)(uintptr_t)threads(8, 9);
        case 3: return (void*)(uintptr_t)deleted_keys();
        case 4: return (void*)(uintptr_t)exhausted_keys();
        case 5: return (void*)(uintptr_t)errors();
        case 6: return (void*)(uintptr_t)bytes();
        default: return (void*)99;
    }
#endif
}
