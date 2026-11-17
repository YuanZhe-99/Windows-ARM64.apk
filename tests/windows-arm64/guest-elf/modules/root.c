// T07 root: a real DT_NEEDED edge, weak imports, constructors and strict rollback negatives.
#include <stdint.h>
#include <dlfcn.h>
extern int dependency_value(void);
extern void t07_trace(int value);
extern int optional_value(void) __attribute__((weak));
#if defined(T07_MISSING)
extern int required_missing(void);
#endif
int shared_value = 42;
static int initialized;

#if defined(T07_BAD_INIT)
/// Supplies a malformed array target that must be rejected before dependency initialization.
__attribute__((section(".init_array"), used))
static void (*const invalid_initializer)(void) = (void (*)(void))(uintptr_t)1;
#else
/// Reads dependency state to prove dependencies are initialized before their parent.
__attribute__((constructor)) static void initialize(void) {
    initialized = dependency_value() + shared_value;
    t07_trace(2);
}
#endif

/// Exercises dependency imports, root-local definitions and zero-valued unresolved weak imports.
int root_value(void) {
#if defined(T07_MISSING)
    return required_missing();
#else
    return initialized + (optional_value ? optional_value() : 0);
#endif
}

/// Checks guest-visible loader handles, pending-error consumption, flags and close semantics.
int dynamic_roundtrip(void) {
    if (dlerror() != 0) return 1;
    void* library = dlopen("libdep.so", RTLD_NOW | RTLD_LOCAL);
    if (!library || dlerror()) return 2;
    int (*value)(void) = (int (*)(void))dlsym(library, "dependency_value");
    if (!value || value() != 7 || dlerror()) return 3;
    if (dlsym(library, "absent") != 0 || !dlerror() || dlerror()) return 4;
    if (dlclose(library) || dlclose(library) != -1 || !dlerror() || dlerror()) return 5;
    if (dlopen("libdep.so", RTLD_LAZY) != 0 || !dlerror() || dlerror()) return 6;
    return 0;
}
