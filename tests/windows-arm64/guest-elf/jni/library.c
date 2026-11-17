#include <jni.h>
extern void t09_load_event(int value);
extern jint t09_dependency_value(void);

/// Records the root constructor after the dependency is callable, before any JNI hook.
__attribute__((constructor)) static void initialize(void) {
    t09_load_event(t09_dependency_value() == 3141 ? 1 : 8);
}

#if !defined(NO_ONLOAD) && !defined(BAD_VERSION) && !defined(PENDING_EXCEPTION) && !defined(DATA_ONLOAD)
/// Supplies an explicitly registered result that must win over a competing Java_* export.
static jint registered(JNIEnv* env, jclass type, jint value) { (void)env; (void)type; return value + 7; }
#endif

#if defined(BAD_VERSION)
/// Supplies a method callable during its hook but never after the hook returns an invalid version.
static jint failed_registration(JNIEnv* env, jclass type) { (void)env; (void)type; return 666; }
#endif

#if defined(DATA_ONLOAD)
/// Exports a data symbol under the hook name; admission must refuse it before any call.
int data_hook __asm__("JNI_OnLoad") = 17;
#endif

#if !defined(NO_ONLOAD) && !defined(DATA_ONLOAD)
/// Exercises exactly-once JNI loading, real registration, invalid versions or a pending Java exception.
JNIEXPORT jint JNI_OnLoad(JavaVM* vm, void* reserved) {
    (void)reserved;
#if defined(BAD_VERSION)
    t09_load_event(4);
    JNIEnv* env = NULL;
    if ((*vm)->GetEnv(vm, (void**)&env, JNI_VERSION_1_6) != JNI_OK) return JNI_ERR;
    jclass type = (*env)->FindClass(env, "com/winapk/probe/GuestJniProbe");
    JNINativeMethod method = {"badVersionRegistered", "()I", (void*)failed_registration};
    if (!type || (*env)->RegisterNatives(env, type, &method, 1) != JNI_OK) return JNI_ERR;
    jmethodID call = (*env)->GetStaticMethodID(env, type, "badVersionRegistered", "()I");
    if (!call || (*env)->CallStaticIntMethod(env, type, call) != 666 || (*env)->ExceptionCheck(env)) return JNI_ERR;
    (*env)->DeleteLocalRef(env, type);
    return 0x1234;
#else
    JNIEnv* env = NULL;
    if ((*vm)->GetEnv(vm, (void**)&env, JNI_VERSION_1_6) != JNI_OK) return JNI_ERR;
#if defined(PENDING_EXCEPTION)
    t09_load_event(5);
    jclass type = (*env)->FindClass(env, "java/lang/IllegalStateException");
    if (type) { (*env)->ThrowNew(env, type, "T09 OnLoad expected"); (*env)->DeleteLocalRef(env, type); }
#else
    t09_load_event(3);
    jclass type = (*env)->FindClass(env, "com/winapk/probe/GuestJniProbe");
    JNINativeMethod method = {"loadRegistered", "(I)I", (void*)registered};
    if (!type || (*env)->RegisterNatives(env, type, &method, 1) != JNI_OK) return JNI_ERR;
    (*env)->DeleteLocalRef(env, type);
#endif
    return JNI_VERSION_1_6;
#endif
}
#endif

/// Supplies the ordinary short JNI name and preserves signed 64-bit arguments/results.
JNIEXPORT jlong Java_com_winapk_probe_GuestJniProbe_exportSimple(JNIEnv* env, jclass type, jlong value) {
    (void)env; (void)type;
    return value - 1;
}
/// Must lose to the matching short name even when both names exist.
JNIEXPORT jlong Java_com_winapk_probe_GuestJniProbe_exportSimple__J(JNIEnv* env, jclass type, jlong value) {
    (void)env; (void)type; (void)value;
    return -999;
}
/// Resolves a long name containing primitive arrays, object package slashes and a semicolon.
JNIEXPORT jobject Java_com_winapk_probe_GuestJniProbe_exportDescriptor___3ILjava_lang_String_2(
        JNIEnv* env, jclass type, jintArray values, jstring text) {
    (void)type;
    jint first = 0;
    if ((*env)->GetArrayLength(env, values) != 1) return NULL;
    (*env)->GetIntArrayRegion(env, values, 0, 1, &first);
    return first == 7 ? text : NULL;
}
/// Supplies the int overload's JNI long name.
JNIEXPORT jint Java_com_winapk_probe_GuestJniProbe_exportOverload__I(JNIEnv* env, jclass type, jint value) {
    (void)env; (void)type;
    return value + 100;
}
/// Supplies the long overload's distinct JNI long name.
JNIEXPORT jlong Java_com_winapk_probe_GuestJniProbe_exportOverload__J(JNIEnv* env, jclass type, jlong value) {
    (void)env; (void)type;
    return value + 4294967296LL;
}
/// Supplies escaped underscore and Unicode UTF-16 units in a JNI short name.
JNIEXPORT jlong Java_com_winapk_probe_GuestJniProbe_export_1_04e2d(JNIEnv* env, jclass type, jlong value) {
    (void)env; (void)type;
    return value + 3;
}
/// Would return the wrong value if export binding overwrote an explicit JNI registration.
JNIEXPORT jint Java_com_winapk_probe_GuestJniProbe_loadRegistered(JNIEnv* env, jclass type, jint value) {
    (void)env; (void)type; (void)value;
    return -999;
}
/// Must never bind the JVM-only "1bad" identifier: JNI's ambiguous-escape rule forbids lookup.
JNIEXPORT jint Java_com_winapk_probe_GuestJniProbe_1bad(JNIEnv* env, jclass type) {
    (void)env; (void)type;
    return 444;
}
