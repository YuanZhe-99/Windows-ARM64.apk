#include <jni.h>
extern void t09_load_event(int value);

/// Records dependency-first ELF initialization independently of Java's explicit load.
__attribute__((constructor)) static void initialize(void) { t09_load_event(2); }

/// Returns a deterministic value used by both the root constructor and dependency native export.
jint t09_dependency_value(void) { return 3141; }

/// Marks an incorrectly automatic dependency JNI hook invocation.
JNIEXPORT jint JNI_OnLoad(JavaVM* vm, void* reserved) {
    (void)vm; (void)reserved;
    t09_load_event(9);
    return JNI_VERSION_1_6;
}

/// Supplies a native method only from a DT_NEEDED dependency, not the root image.
JNIEXPORT jint Java_com_winapk_probe_GuestJniProbe_exportDependency(JNIEnv* env, jclass type) {
    (void)env; (void)type;
    return t09_dependency_value();
}
