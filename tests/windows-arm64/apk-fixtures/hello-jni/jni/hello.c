/* libhello.so: one JNI function for the hello-jni fixture (G1.5 negative, T09 positive). */
#include <jni.h>

/* Returns 42 + the argument, so a caller can tell a real native call from a stub. */
JNIEXPORT jint JNICALL Java_org_winapk_fixture_jni_MainActivity_nativeValue(JNIEnv* env, jclass cls, jint x) {
    (void)env;
    (void)cls;
    return 42 + x;
}
