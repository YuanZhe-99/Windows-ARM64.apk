#include <jni.h>

static int hooks;

/** Exposes hook attempts without requiring the rejected library to bind a Java method. */
int version_count(void) { return hooks; }

/** Returns a deliberately unsupported version, after one observable side effect. */
JNIEXPORT jint JNI_OnLoad(JavaVM* vm, void* reserved) {
    (void)vm; (void)reserved;
    hooks++;
    return 0x1234;
}
