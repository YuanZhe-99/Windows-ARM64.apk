// T07 dependency: initialization values and own-definition binding, without libc or ELF TLS.
extern void t07_trace(int value);
int shared_value = 7;
static int initialized;

/// Publishes dependency-first construction through an audited fixed-signature host callback.
__attribute__((constructor)) static void initialize(void) {
    initialized = shared_value;
    t07_trace(1);
}

/// Returns the value established by this image's own constructor and definition.
int dependency_value(void) { return initialized; }
