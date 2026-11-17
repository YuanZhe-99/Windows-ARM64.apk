// T08 synchronization: audited no-dependency ELF, using real Android LP64 pthread declarations.
#include <errno.h>
#include <pthread.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <time.h>

_Static_assert(sizeof(pthread_mutex_t) == 40 && _Alignof(pthread_mutex_t) == 4, "Android mutex layout");
_Static_assert(sizeof(pthread_cond_t) == 48 && _Alignof(pthread_cond_t) == 4, "Android condition layout");

/// Checks x18/sp and guest/host errno separation on the calling host thread.
extern int t08_host_state(const int* guest_errno);

/// Places mutex storage at four, not eight, byte alignment with overrun sentinels.
struct guarded_mutex {
    uint32_t before;
    pthread_mutex_t mutex;
    uint32_t after;
};
_Static_assert(offsetof(struct guarded_mutex, mutex) == 4, "unaligned for host pointers");

/// Holds one eight-thread counter workload; only ready flags use guest atomic operations.
struct counter {
    pthread_mutex_t mutex;
    int value;
    int ready[8];
};

/// Associates each counter worker with its independently published readiness flag.
struct counter_arg { struct counter* counter; int index; };

/// Increments shared data only while holding the guest mutex, preserving errno through contention.
static void* increment(void* pointer) {
    struct counter_arg* arg = pointer;
    errno = 800 + arg->index;
    __atomic_store_n(&arg->counter->ready[arg->index], 1, __ATOMIC_RELEASE);
    for (int i = 0; i < 256; i++) {
        if (pthread_mutex_lock(&arg->counter->mutex)) return (void*)1;
        arg->counter->value++;
        if (pthread_mutex_unlock(&arg->counter->mutex)) return (void*)2;
    }
    return errno == 800 + arg->index && t08_host_state(&errno) ? NULL : (void*)3;
}

/// Proves mutual exclusion, memory visibility and eight simultaneous lock contenders.
static int contention(void) {
    struct counter counter = {.mutex = PTHREAD_MUTEX_INITIALIZER};
    struct counter_arg args[8];
    pthread_t tokens[8];
    errno = 1800;
    if (pthread_mutex_lock(&counter.mutex)) return 1;
    for (int i = 0; i < 8; i++) {
        args[i] = (struct counter_arg){&counter, i};
        if (pthread_create(&tokens[i], NULL, increment, &args[i])) return 2;
    }
    for (int i = 0; i < 8; i++)
        while (!__atomic_load_n(&counter.ready[i], __ATOMIC_ACQUIRE)) {}
    if (pthread_mutex_destroy(&counter.mutex) != EBUSY || pthread_mutex_unlock(&counter.mutex)) return 3;
    for (int i = 0; i < 8; i++) {
        void* result = NULL;
        if (pthread_join(tokens[i], &result) || result) return 4;
    }
    if (counter.value != 2048 || errno != 1800 || !t08_host_state(&errno)) return 5;
    return pthread_mutex_destroy(&counter.mutex) ? 6 : 0;
}

/// Verifies Android static kinds, recursion saturation and exact guest-sized writes.
static int kinds(void) {
    struct guarded_mutex normal = {0x12345678, PTHREAD_MUTEX_INITIALIZER, 0x87654321};
    pthread_mutex_t recursive = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;
    pthread_mutex_t checked = PTHREAD_ERRORCHECK_MUTEX_INITIALIZER_NP;
    errno = 1801;
    if (pthread_mutex_trylock(&normal.mutex) || pthread_mutex_trylock(&normal.mutex) != EBUSY ||
        pthread_mutex_unlock(&normal.mutex) || pthread_mutex_destroy(&normal.mutex)) return 1;
    if (normal.before != 0x12345678 || normal.after != 0x87654321) return 2;
    if (pthread_mutex_lock(&checked) || pthread_mutex_lock(&checked) != EDEADLK ||
        pthread_mutex_trylock(&checked) != EBUSY || pthread_mutex_unlock(&checked) ||
        pthread_mutex_unlock(&checked) != EPERM || pthread_mutex_destroy(&checked)) return 3;
    for (int i = 0; i < 2048; i++)
        if (pthread_mutex_lock(&recursive)) return 4;
    if (pthread_mutex_trylock(&recursive) != EAGAIN || pthread_mutex_lock(&recursive) != EAGAIN) return 5;
    for (int i = 0; i < 2048; i++)
        if (pthread_mutex_unlock(&recursive)) return 6;
    if (pthread_mutex_unlock(&recursive) != EPERM || pthread_mutex_destroy(&recursive) || errno != 1801) return 7;
    return 0;
}

/// Attempts operations from the wrong owner without blocking.
static void* wrong_owner(void* pointer) {
    errno = 812;
    int ok = pthread_mutex_trylock(pointer) == EBUSY &&
             pthread_mutex_unlock(pointer) == EPERM && pthread_mutex_destroy(pointer) == EBUSY;
    return ok && errno == 812 ? NULL : (void*)1;
}

/// Checks generation/address binding, destroy/reinit, invalid words and explicit attribute refusal.
static int lifecycle(void) {
    pthread_mutex_t mutex, copied, stale;
    pthread_mutexattr_t attributes = 0;
    errno = 1802;
    memset(&mutex, 0x5a, sizeof mutex);  // Explicit init accepts uninitialized guest storage.
    if (pthread_mutex_init(&mutex, &attributes) != ENOTSUP || ((unsigned char*)&mutex)[0] != 0x5a) return 1;
    if (pthread_mutex_init(&mutex, NULL) || pthread_mutex_init(&mutex, NULL) != EBUSY) return 2;
    // Copying pthread objects is not a valid pthread operation; test the shim's diagnostic contract.
    memcpy(&copied, &mutex, sizeof mutex);
    memcpy(&stale, &mutex, sizeof mutex);
    if (pthread_mutex_trylock(&copied) != EINVAL || pthread_mutex_destroy(&copied) != EINVAL) return 3;
    if (pthread_mutex_lock(&mutex)) return 4;
    pthread_t worker;
    void* result = NULL;
    if (pthread_create(&worker, NULL, wrong_owner, &mutex) || pthread_join(worker, &result) || result) return 5;
    if (pthread_mutex_unlock(&mutex) || pthread_mutex_destroy(&mutex)) return 6;
    if (pthread_mutex_lock(&mutex) != EINVAL || pthread_mutex_destroy(&mutex) != EINVAL) return 7;
    if (pthread_mutex_init(&mutex, NULL)) return 8;
    memcpy(&copied, &mutex, sizeof mutex);
    memcpy(&mutex, &stale, sizeof mutex);
    if (pthread_mutex_trylock(&mutex) != EINVAL) return 9;
    memcpy(&mutex, &copied, sizeof mutex);  // Restore current bytes solely to clean up the test.
    if (pthread_mutex_destroy(&mutex)) return 10;
    memset(&mutex, 0x5a, sizeof mutex);
    if (pthread_mutex_lock(&mutex) != EINVAL || errno != 1802) return 11;
    return 0;
}

/// Holds predicate state shared by signal/broadcast tests; never accesses a host lock directly.
struct channel {
    pthread_mutex_t mutex;
    pthread_cond_t ready, gate;
    int arrived, open, count;
};

/// Exchanges 256 request/acknowledgement signals, always waiting in predicate loops.
static void* responder(void* pointer) {
    struct channel* channel = pointer;
    errno = 813;
    if (pthread_mutex_lock(&channel->mutex)) return (void*)1;
    for (int i = 1; i <= 256; i++) {
        while (channel->open < i)
            if (pthread_cond_wait(&channel->gate, &channel->mutex)) return (void*)2;
        if (pthread_mutex_trylock(&channel->mutex) != EBUSY) return (void*)3;  // Reacquired before return.
        channel->count = i;
        if (pthread_cond_signal(&channel->ready)) return (void*)4;
    }
    if (pthread_mutex_unlock(&channel->mutex)) return (void*)5;
    return errno == 813 && t08_host_state(&errno) ? NULL : (void*)6;
}

/// Destroys all channel state after its workers have joined.
static int destroy_channel(struct channel* channel) {
    return pthread_cond_destroy(&channel->ready) || pthread_cond_destroy(&channel->gate) ||
           pthread_mutex_destroy(&channel->mutex);
}

/// Tests untimed signal wait/relock without time-based scheduling assumptions.
static int signals(void) {
    struct channel channel = {.mutex = PTHREAD_MUTEX_INITIALIZER,
                              .ready = PTHREAD_COND_INITIALIZER, .gate = PTHREAD_COND_INITIALIZER};
    pthread_t token;
    errno = 1803;
    // A signal with no waiters is not retained; the predicate protocol, not a queued signal, drives the test.
    if (pthread_cond_signal(&channel.gate) || pthread_create(&token, NULL, responder, &channel)) return 1;
    if (pthread_mutex_lock(&channel.mutex)) return 2;
    for (int i = 1; i <= 256; i++) {
        channel.open = i;
        if (pthread_cond_signal(&channel.gate)) return 3;
        while (channel.count < i)
            if (pthread_cond_wait(&channel.ready, &channel.mutex)) return 4;
    }
    if (pthread_mutex_unlock(&channel.mutex)) return 5;
    void* result = NULL;
    if (pthread_join(token, &result) || result || channel.count != 256 || errno != 1803) return 6;
    return destroy_channel(&channel) ? 7 : 0;
}

/// Waits for the broadcast gate, then verifies reacquired ownership and protected state.
static void* receiver(void* pointer) {
    struct channel* channel = pointer;
    errno = 814;
    if (pthread_mutex_lock(&channel->mutex)) return (void*)1;
    channel->arrived++;
    if (pthread_cond_signal(&channel->ready)) return (void*)2;
    while (!channel->open)
        if (pthread_cond_wait(&channel->gate, &channel->mutex)) return (void*)3;
    if (pthread_mutex_trylock(&channel->mutex) != EBUSY) return (void*)4;
    channel->count++;
    if (pthread_mutex_unlock(&channel->mutex)) return (void*)5;
    return errno == 814 && t08_host_state(&errno) ? NULL : (void*)6;
}

/// Tests broadcast, waiter-pinned destruction and rejecting a second mutex binding.
static int broadcasts(int binding_error) {
    struct channel channel = {.mutex = PTHREAD_MUTEX_INITIALIZER,
                              .ready = PTHREAD_COND_INITIALIZER, .gate = PTHREAD_COND_INITIALIZER};
    pthread_mutex_t other = PTHREAD_MUTEX_INITIALIZER;
    pthread_t tokens[8];
    errno = 1804;
    if (pthread_mutex_lock(&channel.mutex)) return 1;
    for (int i = 0; i < 8; i++)
        if (pthread_create(&tokens[i], NULL, receiver, &channel)) return 2;
    while (channel.arrived < 8)
        if (pthread_cond_wait(&channel.ready, &channel.mutex)) return 3;
    if (pthread_cond_destroy(&channel.gate) != EBUSY) return 4;
    if (pthread_mutex_unlock(&channel.mutex)) return 5;
    // All workers are waiting on gate: the unlocked mutex must remain pinned.
    if (pthread_mutex_destroy(&channel.mutex) != EBUSY || pthread_mutex_lock(&channel.mutex)) return 6;
    if (binding_error) {
        if (pthread_mutex_lock(&other) || pthread_cond_wait(&channel.gate, &other) != EINVAL ||
            pthread_mutex_trylock(&other) != EBUSY || pthread_mutex_unlock(&other)) return 7;
    }
    channel.open = 1;
    if (pthread_cond_broadcast(&channel.gate) || pthread_mutex_unlock(&channel.mutex)) return 8;
    for (int i = 0; i < 8; i++) {
        void* result = NULL;
        if (pthread_join(tokens[i], &result) || result) return 9;
    }
    if (channel.count != 8 || errno != 1804) return 10;
    if (pthread_mutex_destroy(&other)) return 11;
    return destroy_channel(&channel) ? 12 : 0;
}

/// Verifies condition storage bounds, lifecycle markers, attribute/clock/deep-recursion refusals.
static int condition_errors(void) {
    struct { uint32_t before; pthread_cond_t cond; uint32_t after; } guarded =
        {0x12345678, PTHREAD_COND_INITIALIZER, 0x87654321};
    pthread_cond_t monotonic = PTHREAD_COND_INITIALIZER_MONOTONIC_NP, copied, saved;
    pthread_condattr_t attributes = 0;
    pthread_mutex_t mutex = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;
    errno = 1805;
    if (pthread_cond_signal(&monotonic) != ENOTSUP || pthread_cond_destroy(&monotonic) != ENOTSUP) return 1;
    if (pthread_cond_init(&guarded.cond, &attributes) != ENOTSUP) return 2;
    if (pthread_cond_init(&guarded.cond, NULL) || pthread_cond_init(&guarded.cond, NULL) != EBUSY) return 3;
    if (pthread_cond_wait(&guarded.cond, &mutex) != EPERM) return 4;
    if (pthread_mutex_lock(&mutex) || pthread_mutex_lock(&mutex) ||
        pthread_cond_wait(&guarded.cond, &mutex) != ENOTSUP) return 5;
    if (pthread_mutex_unlock(&mutex) || pthread_mutex_unlock(&mutex) || pthread_mutex_destroy(&mutex)) return 6;
    memcpy(&copied, &guarded.cond, sizeof copied);
    if (pthread_cond_signal(&copied) != EINVAL || pthread_cond_destroy(&copied) != EINVAL) return 7;
    memcpy(&saved, &guarded.cond, sizeof saved);
    if (pthread_cond_broadcast(&guarded.cond) || pthread_cond_destroy(&guarded.cond)) return 8;
    if (pthread_cond_signal(&guarded.cond) != EINVAL || pthread_cond_destroy(&guarded.cond) != EINVAL) return 9;
    if (pthread_cond_init(&guarded.cond, NULL)) return 10;
    memcpy(&copied, &guarded.cond, sizeof copied);
    memcpy(&guarded.cond, &saved, sizeof saved);
    if (pthread_cond_signal(&guarded.cond) != EINVAL) return 11;
    memcpy(&guarded.cond, &copied, sizeof copied);
    if (pthread_cond_destroy(&guarded.cond)) return 12;
    if (guarded.before != 0x12345678 || guarded.after != 0x87654321 || errno != 1805) return 13;
    return broadcasts(1) ? 14 : 0;
}

/// Runs one T08 sync case or an explicit unsupported/fatal negative fixture.
void* t08_case(void* argument) {
#if defined(T08_SYNC_MISSING)
    (void)argument;
    pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
    pthread_cond_t condition = PTHREAD_COND_INITIALIZER;
    struct timespec deadline = {0};
    return (void*)(uintptr_t)pthread_cond_timedwait(&condition, &mutex, &deadline);
#elif defined(T08_SYNC_OWNED)
    (void)argument;
    static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
    return (void*)(uintptr_t)pthread_mutex_lock(&mutex);  // Teardown must not free an owned host record.
#else
    switch ((uintptr_t)argument) {
        case 0: return (void*)(uintptr_t)contention();
        case 1: return (void*)(uintptr_t)kinds();
        case 2: return (void*)(uintptr_t)lifecycle();
        case 3: return (void*)(uintptr_t)signals();
        case 4: return (void*)(uintptr_t)broadcasts(0);
        case 5: return (void*)(uintptr_t)condition_errors();
        default: return (void*)99;
    }
#endif
}
