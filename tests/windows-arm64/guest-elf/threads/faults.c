// T08 fault attribution: the positive callback stays in the host's normal SEH chain.
#include <errno.h>
#include <stdint.h>

extern int t08_host_exception(void);
extern int t08_host_state(const int* guest_errno);

/// Exercises a host-handled exception or one deliberately fatal guest-PC instruction.
void* t08_case(void* argument) {
    (void)argument;
#if defined(T08_GUEST_FAULT)
    volatile uintptr_t address = 0;
    *(volatile uint64_t*)address = 1;
#elif defined(T08_GUEST_TRAP)
    __builtin_trap();
#else
    errno = 191;
    if (t08_host_exception() && t08_host_state(&errno) && errno == 191) return 0;
#endif
    return (void*)1;
}
