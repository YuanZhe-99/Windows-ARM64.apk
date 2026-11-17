/* libhello.so: one JNI function for the hello-jni fixture (G1.5 negative, T09 positive). */
#include <jni.h>

static int load_count;

/** Reports real hook execution through an explicitly registered, non-export-named entry. */
static jint count(JNIEnv* env, jclass type) { (void)env; (void)type; return load_count; }

/** Registers one method in the app's loader, and records one successful explicit initialization. */
JNIEXPORT jint JNI_OnLoad(JavaVM* vm, void* reserved) {
    (void)reserved;
    JNIEnv* env = NULL;
    if ((*vm)->GetEnv(vm, (void**)&env, JNI_VERSION_1_6) != JNI_OK) return JNI_ERR;
    jclass type = (*env)->FindClass(env, "org/winapk/fixture/jni/MainActivity");
    JNINativeMethod method = {"loadCount", "()I", (void*)count};
    if (!type || (*env)->RegisterNatives(env, type, &method, 1) != JNI_OK) return JNI_ERR;
    (*env)->DeleteLocalRef(env, type);
    load_count++;
    return JNI_VERSION_1_6;
}

/** Returns 42 + the argument, so a caller can tell a real native call from a stub. */
JNIEXPORT jint JNICALL Java_org_winapk_fixture_jni_MainActivity_nativeValue(JNIEnv* env, jclass cls, jint x) {
    (void)env;
    (void)cls;
    return 42 + x;
}

/** Supplies a native declared in a class not loaded until after the explicit library load. */
JNIEXPORT jint Java_org_winapk_fixture_jni_LateNative_value(JNIEnv* env, jclass cls) {
    (void)env; (void)cls;
    return 91;
}
