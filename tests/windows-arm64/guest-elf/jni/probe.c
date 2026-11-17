// T09: real JNI descriptors, Android va_list/default promotions, references and worker lifecycle.
#include <jni.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdint.h>
#include <string.h>

extern int t09_host_state(jclass type);
static const char* class_name = "com/winapk/probe/GuestJniProbe";
static const char* mix_signature = "(ZBCSIJFDLjava/lang/Object;IIIIIIIIDDDDDDDD)D";

/// Calls a static double V entry twice with the same va_list, proving va_copy preserves the caller.
static double mix_v(JNIEnv* env, jclass type, jmethodID method, ...) {
    va_list list;
    va_start(list, method);
    double first = (*env)->CallStaticDoubleMethodV(env, type, method, list);
    if ((*env)->ExceptionCheck(env)) { va_end(list); return 0; }
    double second = (*env)->CallStaticDoubleMethodV(env, type, method, list);
    va_end(list);
    return first == second ? first : 0;
}

/// Calls the guest V constructor entry with its own Android va_list.
static jobject construct_v(JNIEnv* env, jclass type, jmethodID method, ...) {
    va_list list;
    va_start(list, method);
    jobject result = (*env)->NewObjectV(env, type, method, list);
    va_end(list);
    return result;
}

/// Calls instance and nonvirtual V entries using one guest va_list without consuming it.
static jlong number_v(JNIEnv* env, jobject object, jclass type, jmethodID method, ...) {
    va_list list;
    va_start(list, method);
    jlong virtual_value = (*env)->CallLongMethodV(env, object, method, list);
    if ((*env)->ExceptionCheck(env)) { va_end(list); return 0; }
    jlong base = (*env)->CallNonvirtualLongMethodV(env, object, type, method, list);
    va_end(list);
    return virtual_value == base + 100 ? base : 0;
}

/// Checks constructor/instance/nonvirtual/static .../V/A and all scalar result categories.
static int calls(JNIEnv* env) {
    jclass type = (*env)->FindClass(env, class_name);
    if (!type || !t09_host_state(type)) return 1;
    jfieldID initialized = (*env)->GetStaticFieldID(env, type, "initialized", "Z");
    if (!initialized || (*env)->GetStaticBooleanField(env, type, initialized)) return 19;
    jclass initializer = (*env)->FindClass(env, "com/winapk/probe/GuestJniProbe$Initialized");
    if (!initializer || !(*env)->GetStaticBooleanField(env, type, initialized)) return 20;
    (*env)->DeleteLocalRef(env, initializer);
    jclass child = (*env)->FindClass(env, "com/winapk/probe/GuestJniProbe$Child");
    if (!child) return 2;
    jmethodID constructor = (*env)->GetMethodID(env, child, "<init>", "(JFD)V");
    jmethodID number = (*env)->GetMethodID(env, type, "number", "(I)J");
    jmethodID mix = (*env)->GetStaticMethodID(env, type, "mix", mix_signature);
    if ((*env)->ExceptionCheck(env) || !constructor || !number || !mix) return 3;
    jobject object = (*env)->NewObject(env, child, constructor, (jlong)4294967297LL, 2.0, 3.0);
    if ((*env)->ExceptionCheck(env) || !object) return 4;
    jlong expected = 4294967309LL;  // long + 2 + 3 + increment 7.
    jlong value = (*env)->CallLongMethod(env, object, number, 7);
    if ((*env)->ExceptionCheck(env) || value != expected + 100) return 5;
    value = (*env)->CallNonvirtualLongMethod(env, object, type, number, 7);
    if ((*env)->ExceptionCheck(env) || value != expected) return 6;
    if (number_v(env, object, type, number, 7) != expected || (*env)->ExceptionCheck(env)) return 7;
    jvalue increment = {.i = 7};
    value = (*env)->CallLongMethodA(env, object, number, &increment);
    if ((*env)->ExceptionCheck(env) || value != expected + 100) return 8;
    value = (*env)->CallNonvirtualLongMethodA(env, object, type, number, &increment);
    if ((*env)->ExceptionCheck(env) || value != expected) return 9;
    jobject from_v = construct_v(env, child, constructor, (jlong)4294967297LL, 2.0, 3.0);
    if ((*env)->ExceptionCheck(env) || !from_v) return 10;
    jvalue init[3] = {{.j = 4294967297LL}, {.f = 2.0f}, {.d = 3.0}};
    jobject from_a = (*env)->NewObjectA(env, child, constructor, init);
    if ((*env)->ExceptionCheck(env) || !from_a) return 11;
    value = (*env)->CallLongMethod(env, from_v, number, 7);
    if ((*env)->ExceptionCheck(env) || value != expected + 100) return 12;
    value = (*env)->CallLongMethod(env, from_a, number, 7);
    if ((*env)->ExceptionCheck(env) || value != expected + 100) return 13;
#define MIX_ARGS JNI_TRUE, -7, 0x4e2d, -123, 29, (jlong)4294967297LL, 1.5, 2.25, object, \
                 1, 2, 3, 4, 5, 6, 7, 8, 1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0
    double result = (*env)->CallStaticDoubleMethod(env, type, mix, MIX_ARGS);
    if ((*env)->ExceptionCheck(env) || result != 4294967297.75) return 14;
    if (mix_v(env, type, mix, MIX_ARGS) != 4294967297.75 || (*env)->ExceptionCheck(env)) return 15;
#undef MIX_ARGS
    jvalue args[25] = {{.z = JNI_TRUE}, {.b = -7}, {.c = 0x4e2d}, {.s = -123}, {.i = 29},
                      {.j = 4294967297LL}, {.f = 1.5f}, {.d = 2.25}, {.l = object}};
    for (int i = 0; i < 8; i++) { args[9 + i].i = i + 1; args[17 + i].d = i + 1; }
    result = (*env)->CallStaticDoubleMethodA(env, type, mix, args);
    if ((*env)->ExceptionCheck(env) || result != 4294967297.75) return 16;
#define SCALAR(Name, JavaName, Descriptor, Expected) \
    { jmethodID id = (*env)->GetStaticMethodID(env, type, #JavaName "Value", "()" Descriptor); \
      if (!id) return 17; \
      /* Each return uses its matching JNI result category, never a generic host vararg. */ \
      if ((*env)->CallStatic##Name##Method(env, type, id) != (Expected) || (*env)->ExceptionCheck(env)) return 18; }
    SCALAR(Boolean, boolean, "Z", JNI_TRUE)
    SCALAR(Byte, byte, "B", -7)
    SCALAR(Char, char, "C", 0x4e2d)
    SCALAR(Short, short, "S", -123)
    SCALAR(Int, int, "I", -123456789)
    SCALAR(Float, float, "F", 1.5f)
#undef SCALAR
    jfieldID count = (*env)->GetFieldID(env, type, "count", "I");
    jmethodID add = (*env)->GetMethodID(env, type, "add", "(I)V");
    if (!count || !add) return 21;
    (*env)->CallVoidMethod(env, object, add, 3);
    if ((*env)->ExceptionCheck(env) || (*env)->GetIntField(env, object, count) != 3) return 22;
    (*env)->SetIntField(env, object, count, 10);
    (*env)->CallVoidMethodA(env, object, add, &increment);
    if ((*env)->ExceptionCheck(env) || (*env)->GetIntField(env, object, count) != 17) return 23;
    (*env)->DeleteLocalRef(env, from_a); (*env)->DeleteLocalRef(env, from_v);
    (*env)->DeleteLocalRef(env, object); (*env)->DeleteLocalRef(env, child); (*env)->DeleteLocalRef(env, type);
    return 0;
}

/// Checks UTF-16/modified-UTF8, primitive/object arrays, globals/weak refs and local frames.
static int references(JNIEnv* env) {
    if ((*env)->PushLocalFrame(env, 32)) return 1;
    jclass type = (*env)->FindClass(env, class_name);
    if (!type) return 2;
    jmethodID arrays = (*env)->GetStaticMethodID(env, type, "arrays", "(Ljava/lang/String;[I)Ljava/lang/String;");
    if (!arrays) return 3;
    const jchar text[] = {0x4e2d, 0, 0x6587};
    jstring string = (*env)->NewString(env, text, 3);
    jintArray array = (*env)->NewIntArray(env, 3);
    if (!string || !array) return 4;
    const jint numbers[] = {-3, 42, 99};
    (*env)->SetIntArrayRegion(env, array, 0, 3, numbers);
    const jchar* wide = (*env)->GetStringChars(env, string, NULL);
    if (!wide || memcmp(wide, text, sizeof text)) return 5;
    (*env)->ReleaseStringChars(env, string, wide);
    const char* modified = (*env)->GetStringUTFChars(env, string, NULL);
    if (!modified || strcmp(modified, "\xe4\xb8\xad\xc0\x80\xe6\x96\x87")) return 6;
    (*env)->ReleaseStringUTFChars(env, string, modified);
    if ((*env)->GetStringLength(env, string) != 3 || (*env)->GetStringUTFLength(env, string) != 8) return 7;
    jvalue args[] = {{.l = string}, {.l = array}};
    jobject answer = (*env)->CallStaticObjectMethodA(env, type, arrays, args);
    if ((*env)->ExceptionCheck(env) || !(*env)->IsSameObject(env, answer, string)) return 8;
    answer = (*env)->CallStaticObjectMethod(env, type, arrays, string, array);
    if ((*env)->ExceptionCheck(env) || !(*env)->IsSameObject(env, answer, string)) return 9;
    jint* pinned = (*env)->GetIntArrayElements(env, array, NULL);
    if (!pinned || pinned[1] != 42) return 10;
    pinned[1] = 77;
    (*env)->ReleaseIntArrayElements(env, array, pinned, JNI_COMMIT);
    (*env)->ReleaseIntArrayElements(env, array, pinned, 0);
    jint check[3];
    (*env)->GetIntArrayRegion(env, array, 0, 3, check);
    if (check[1] != 77) return 11;
    jobject global = (*env)->NewGlobalRef(env, string);
    jweak weak = (*env)->NewWeakGlobalRef(env, string);
    if (!global || !weak || (*env)->GetObjectRefType(env, global) != JNIGlobalRefType ||
        (*env)->GetObjectRefType(env, weak) != JNIWeakGlobalRefType) return 12;
    jobject retained = (*env)->PopLocalFrame(env, string);
    if (!(*env)->IsSameObject(env, global, retained) || !(*env)->IsSameObject(env, weak, retained)) return 13;
    jobject local = (*env)->NewLocalRef(env, weak);
    if (!local || (*env)->GetObjectRefType(env, local) != JNILocalRefType) return 14;
    (*env)->DeleteLocalRef(env, local); (*env)->DeleteLocalRef(env, retained);
    (*env)->DeleteWeakGlobalRef(env, weak); (*env)->DeleteGlobalRef(env, global);
    return (*env)->ExceptionCheck(env) ? 15 : 0;
}

/// Checks real pending Java exceptions and explicit metadata/category refusals without swallowing them.
static int exceptions(JNIEnv* env) {
    jclass type = (*env)->FindClass(env, class_name);
    if (!type) return 1;
    jmethodID fail = (*env)->GetStaticMethodID(env, type, "fail", "()V");
    if (!fail) return 2;
    (*env)->CallStaticVoidMethod(env, type, fail);
    if (!(*env)->ExceptionCheck(env)) return 3;
    jthrowable error = (*env)->ExceptionOccurred(env);
    (*env)->ExceptionClear(env);
    jclass illegal = (*env)->FindClass(env, "java/lang/IllegalArgumentException");
    if (!error || !illegal || !(*env)->IsInstanceOf(env, error, illegal)) return 4;
    (*env)->DeleteLocalRef(env, error);
    jmethodID bad = (*env)->GetMethodID(env, type, "missing", "()V");
    if (bad || !(*env)->ExceptionCheck(env)) return 5;
    (*env)->ExceptionClear(env);
    bad = (*env)->GetMethodID(env, type, "bad", "(V)V");
    if (bad || !(*env)->ExceptionCheck(env)) return 6;
    (*env)->ExceptionClear(env);
    (*env)->CallStaticVoidMethodA(env, type, (jmethodID)(uintptr_t)0x123, NULL);
    if (!(*env)->ExceptionCheck(env)) return 7;
    (*env)->ExceptionClear(env);
    // The known static method must not enter a host instance-call entry.
    (*env)->CallVoidMethodA(env, type, fail, NULL);
    if (!(*env)->ExceptionCheck(env)) return 8;
    (*env)->ExceptionClear(env);
    jclass missing = (*env)->FindClass(env, "com/winapk/probe/MissingT09");
    if (missing || !(*env)->ExceptionCheck(env)) return 9;
    error = (*env)->ExceptionOccurred(env);
    (*env)->ExceptionClear(env);
    jclass linkage = (*env)->FindClass(env, "java/lang/NoClassDefFoundError");
    if (!linkage || !(*env)->IsInstanceOf(env, error, linkage)) return 10;
    (*env)->DeleteLocalRef(env, error); (*env)->DeleteLocalRef(env, linkage);
    jmethodID vm_faults = (*env)->GetStaticMethodID(env, type, "vmFaults", "()Z");
    if (!vm_faults || !(*env)->CallStaticBooleanMethod(env, type, vm_faults) || (*env)->ExceptionCheck(env)) return 11;
    (*env)->DeleteLocalRef(env, illegal); (*env)->DeleteLocalRef(env, type);
    return 0;
}

/// Holds independent per-worker tokens/results; no JNIEnv is shared between successful workers.
struct worker {
    JavaVM* vm;
    JNIEnv* foreign;
    int mode, result, destructor;
    pthread_key_t key;
};

/// Proves JNI remains attached through Bionic key destructors, before automatic host-thread detach.
static void destructor(void* argument) {
    struct worker* worker = argument;
    JNIEnv* env = NULL;
    if ((*worker->vm)->GetEnv(worker->vm, (void**)&env, JNI_VERSION_1_6) != JNI_OK) return;
    jclass type = (*env)->FindClass(env, class_name);
    worker->destructor = type && t09_host_state(type);
    if (type) (*env)->DeleteLocalRef(env, type);
}

/// Exercises explicit/automatic, daemon/normal attachment and rejects a cross-thread environment.
static void* worker(void* argument) {
    struct worker* worker = argument;
    if (worker->mode == 2) { (*worker->foreign)->GetVersion(worker->foreign); return NULL; }
    JNIEnv* env = (JNIEnv*)(uintptr_t)1;
    if ((*worker->vm)->GetEnv(worker->vm, (void**)&env, JNI_VERSION_1_6) != JNI_EDETACHED || env) return NULL;
    JavaVMAttachArgs bad_args = {0x1234, "T09 refused", NULL};
    if ((*worker->vm)->AttachCurrentThread(worker->vm, &env, &bad_args) != JNI_EVERSION || env) return NULL;
    JavaVMAttachArgs args = {JNI_VERSION_1_6, "T09 worker", NULL};
    int status = worker->mode ?
        (*worker->vm)->AttachCurrentThreadAsDaemon(worker->vm, &env, &args) :
        (*worker->vm)->AttachCurrentThread(worker->vm, &env, &args);
    if (status != JNI_OK || !env) return NULL;
    JNIEnv* again = NULL;
    if ((*worker->vm)->AttachCurrentThread(worker->vm, &again, &args) != JNI_OK || again != env) return NULL;
    jclass type = (*env)->FindClass(env, class_name);
    if (!type || !t09_host_state(type)) return NULL;
    (*env)->DeleteLocalRef(env, type);
    if (!worker->mode) {
        if ((*worker->vm)->DetachCurrentThread(worker->vm) != JNI_OK) return NULL;
        env = (JNIEnv*)(uintptr_t)1;
        if ((*worker->vm)->GetEnv(worker->vm, (void**)&env, JNI_VERSION_1_6) != JNI_EDETACHED || env) return NULL;
    } else if (pthread_setspecific(worker->key, worker)) return NULL;
    worker->result = 1;
    return NULL;
}

/// Checks VM versions, borrowed-thread ownership and worker attachment/destructor cleanup.
static int workers(JNIEnv* env, JavaVM* vm) {
    JavaVM* returned = NULL;
    if ((*env)->GetJavaVM(env, &returned) != JNI_OK || returned != vm || (*env)->GetVersion(env) != JNI_VERSION_1_6) return 1;
    JNIEnv* again = NULL;
    if ((*vm)->GetEnv(vm, (void**)&again, JNI_VERSION_1_6) != JNI_OK || again != env) return 2;
    if ((*vm)->DetachCurrentThread(vm) != JNI_ERR) return 3;  // Borrowed main thread.
    again = (JNIEnv*)(uintptr_t)1;
    if ((*vm)->GetEnv(vm, (void**)&again, 0x1234) != JNI_EVERSION || again) return 4;
    struct worker cases[2] = {{.vm = vm}, {.vm = vm, .mode = 1}};
    if (pthread_key_create(&cases[1].key, destructor)) return 5;
    pthread_t threads[2];
    for (int i = 0; i < 2; i++) if (pthread_create(&threads[i], NULL, worker, &cases[i])) return 6;
    for (int i = 0; i < 2; i++) if (pthread_join(threads[i], NULL) || !cases[i].result) return 7;
    if (pthread_key_delete(cases[1].key) || !cases[1].destructor) return 8;
    return 0;
}

/// Selects four semantic groups or a precise fatal JNI/guest-PC refusal in a fresh process.
int t09_case(JNIEnv* env, JavaVM* vm, int number) {
    switch (number) {
    case 0: return calls(env);
    case 1: return references(env);
    case 2: return exceptions(env);
    case 3: return workers(env, vm);
    case 4: (*env)->MonitorEnter(env, NULL); return 1;  // Unimplemented slot 217.
    case 5: {
        struct worker cross = {.vm = vm, .foreign = env, .mode = 2};
        pthread_t thread;
        if (pthread_create(&thread, NULL, worker, &cross) || pthread_join(thread, NULL)) return 1;
        return 2;
    }
    case 6: __builtin_trap();
    case 7: *(volatile int*)(uintptr_t)0x40 = 1; return 1;
    default: return -1;
    }
}
