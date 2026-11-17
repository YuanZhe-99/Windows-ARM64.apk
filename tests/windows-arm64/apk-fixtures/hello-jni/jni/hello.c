/* libhello.so: one JNI function for the hello-jni fixture (G1.5 negative, T09 positive). */
#include <jni.h>
#include <pthread.h>
#include <stdint.h>
#include <string.h>

static int load_count;
static JavaVM* machine;
extern int dependency_marker(void);
extern int version_count(void);

/** Reports real hook execution through an explicitly registered, non-export-named entry. */
static jint count(JNIEnv* env, jclass type) { (void)env; (void)type; return load_count; }

/** Registers one method in the app's loader, and records one successful explicit initialization. */
JNIEXPORT jint JNI_OnLoad(JavaVM* vm, void* reserved) {
    (void)reserved;
    JNIEnv* env = NULL;
    if (dependency_marker() != 3141 || version_count() != 0) return JNI_ERR;
    if ((*vm)->GetEnv(vm, (void**)&env, JNI_VERSION_1_6) != JNI_OK) return JNI_ERR;
    jclass type = (*env)->FindClass(env, "org/winapk/fixture/jni/MainActivity");
    JNINativeMethod method = {"loadCount", "()I", (void*)count};
    if (!type || (*env)->RegisterNatives(env, type, &method, 1) != JNI_OK) return JNI_ERR;
    (*env)->DeleteLocalRef(env, type);
    load_count++;
    machine = vm;
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

/** Verifies guest Android varargs, strings, arrays and nested Java-to-native callback values. */
JNIEXPORT jstring Java_org_winapk_fixture_jni_MainActivity_nativeMatrix(
        JNIEnv* env, jclass type, jstring text, jintArray values, jdouble real, jlong wide) {
    jmethodID callback = (*env)->GetStaticMethodID(env, type, "matrixCallback", "(Ljava/lang/String;[IDJ)Ljava/lang/String;");
    if (!callback) return NULL;
    jstring result = (*env)->CallStaticObjectMethod(env, type, callback, text, values, real, wide);
    if ((*env)->ExceptionCheck(env) || !result) return NULL;
    const char* bytes = (*env)->GetStringUTFChars(env, result, NULL);
    if (!bytes) return NULL;
    int ok = strcmp(bytes, "JNI:nested") == 0;
    (*env)->ReleaseStringUTFChars(env, result, bytes);
    jint first = 0;
    (*env)->GetIntArrayRegion(env, values, 0, 1, &first);
    if ((*env)->ExceptionCheck(env)) return NULL;
    jmethodID throws = (*env)->GetStaticMethodID(env, type, "throwExpected", "()V");
    if (!throws) return NULL;
    (*env)->CallStaticVoidMethod(env, type, throws);
    jthrowable pending = (*env)->ExceptionOccurred(env);
    (*env)->ExceptionClear(env);
    jclass expected = (*env)->FindClass(env, "java/lang/IllegalStateException");
    ok = ok && first == 99 && pending && expected && (*env)->IsInstanceOf(env, pending, expected);
    if (pending) (*env)->DeleteLocalRef(env, pending);
    if (expected) (*env)->DeleteLocalRef(env, expected);
    return ok ? result : NULL;
}

/** Returns a floating value only when all mixed fixed-signature arguments survive exactly. */
JNIEXPORT jdouble Java_org_winapk_fixture_jni_MainActivity_nativeNumbers(
        JNIEnv* env, jclass type, jlong wide, jfloat small, jdouble real) {
    (void)env; (void)type;
    return wide == 4294967297LL && small == 1.5f && real == -2.25 ? 9.75 : -999.0;
}

/** Preserves an instance receiver and 64-bit integer result on the actual APK native path. */
JNIEXPORT jlong Java_org_winapk_fixture_jni_MainActivity_instanceValue(JNIEnv* env, jobject object, jlong value) {
    (void)env;
    return object ? value + 1 : 0;
}

/** Leaves a pending guest-thrown Java exception for the real JVM invocation to propagate. */
JNIEXPORT void Java_org_winapk_fixture_jni_MainActivity_nativeThrowBack(JNIEnv* env, jclass type) {
    (void)type;
    jclass error = (*env)->FindClass(env, "java/lang/IllegalArgumentException");
    if (error) {
        (*env)->ThrowNew(env, error, "T09 expected");
        (*env)->DeleteLocalRef(env, error);
    }
}

/** Reports the bad-version hook count through the successful root, never the failed root. */
JNIEXPORT jint Java_org_winapk_fixture_jni_MainActivity_badVersionCount(JNIEnv* env, jclass type) {
    (void)env; (void)type;
    return version_count();
}

/** Attaches an actual guest-created worker, initializes a new app class, callbacks, then detaches. */
static void* worker(void* argument) {
    (void)argument;
    JNIEnv* env = NULL;
    if ((*machine)->AttachCurrentThread(machine, &env, NULL) != JNI_OK) return (void*)1;
    jclass type = (*env)->FindClass(env, "org/winapk/fixture/jni/WorkerCallbacks");
    jmethodID callback = type ? (*env)->GetStaticMethodID(env, type, "value", "()I") : NULL;
    jint result = callback ? (*env)->CallStaticIntMethod(env, type, callback) : 0;
    if ((*env)->ExceptionCheck(env)) { (*env)->ExceptionClear(env); result = 0; }
    if (type) (*env)->DeleteLocalRef(env, type);
    if ((*machine)->DetachCurrentThread(machine) != JNI_OK) return (void*)2;
    return (void*)(intptr_t)result;
}

/** Joins the worker before its JNI entry returns, keeping thread/module lifetime deterministic. */
JNIEXPORT jint Java_org_winapk_fixture_jni_MainActivity_nativeWorker(JNIEnv* env, jclass type) {
    (void)env; (void)type;
    pthread_t thread;
    void* result = NULL;
    if (pthread_create(&thread, NULL, worker, NULL) || pthread_join(thread, &result)) return 0;
    return (jint)(intptr_t)result;
}

/** Calls a declared unsupported JNI entry; the process must fail explicitly rather than return zero. */
JNIEXPORT void Java_org_winapk_fixture_jni_MainActivity_nativeUnsupported(JNIEnv* env, jclass type) {
    (*env)->MonitorEnter(env, type);
}
