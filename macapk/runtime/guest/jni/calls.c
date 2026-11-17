// Android AArch64 only: guest va_list never reaches a host variadic JNI entry.
#include <jni.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

_Static_assert(sizeof(jvalue) == 8 && sizeof(jlong) == 8 && sizeof(void*) == 8, "JNI guest LP64");
_Static_assert(offsetof(struct JNINativeInterface, GetObjectRefType) == 232 * 8, "JNI 1.6 slots");
_Static_assert(sizeof(struct JNINativeInterface) == 233 * 8 && sizeof(jobjectRefType) == 4, "bounded JNI table");

extern const char* winapk_jni_signature(JNIEnv*, jmethodID, jint mode, jint result);

/// Decodes at most 64 signature-validated arguments, copying only the guest's va_list.
static int arguments(JNIEnv* env, jmethodID method, int mode, int result, va_list list, jvalue* values) {
    const char* type = winapk_jni_signature(env, method, mode, result);
    if (!type) return 0;  // Host has left an explicit pending Java exception.
    type++;
    va_list copy;
    va_copy(copy, list);
    for (int i = 0; *type != ')'; i++) {
        switch (*type++) {
        case 'Z': values[i].z = (jboolean)va_arg(copy, int); break;
        case 'B': values[i].b = (jbyte)va_arg(copy, int); break;
        case 'C': values[i].c = (jchar)va_arg(copy, int); break;
        case 'S': values[i].s = (jshort)va_arg(copy, int); break;
        case 'I': values[i].i = va_arg(copy, jint); break;
        case 'J': values[i].j = va_arg(copy, jlong); break;
        case 'F': values[i].f = (jfloat)va_arg(copy, double); break;
        case 'D': values[i].d = va_arg(copy, jdouble); break;
        default:
            values[i].l = va_arg(copy, jobject);
            while (*type == '[') type++;
            if (*type == 'L') type++;
            if (type[-1] == 'L') { while (*type != ';') type++; type++; }
            else if (type[-1] == '[') type++;  // Primitive array element.
            break;
        }
    }
    va_end(copy);
    return 1;
}

/// Invokes a constructor using guest-side V decoding and the checked fixed A entry.
static jobject construct_v(JNIEnv* env, jclass type, jmethodID method, va_list list) {
    jvalue values[64];
    return arguments(env, method, 3, 'V', list, values) ? (*env)->NewObjectA(env, type, method, values) : NULL;
}

/// Captures Android varargs for an object constructor without a host va_list.
static jobject construct(JNIEnv* env, jclass type, jmethodID method, ...) {
    va_list list;
    va_start(list, method);
    jobject result = construct_v(env, type, method, list);
    va_end(list);
    return result;
}

// Macro-generated declarations share exactly the same descriptor/default-promotion contract.
#define CALL(Name, CType, Code) \
    /** Decodes guest V arguments for a checked instance call. */ \
    static CType call_##Name##_v(JNIEnv* env, jobject object, jmethodID method, va_list list) { \
        jvalue values[64]; \
        if (!arguments(env, method, 0, Code, list, values)) return (CType)0; \
        return (*env)->Call##Name##MethodA(env, object, method, values); \
    } \
    /** Captures guest varargs for an instance call. */ \
    static CType call_##Name(JNIEnv* env, jobject object, jmethodID method, ...) { \
        va_list list; va_start(list, method); \
        CType result = call_##Name##_v(env, object, method, list); va_end(list); return result; \
    } \
    /** Decodes guest V arguments for a checked nonvirtual call. */ \
    static CType nonvirtual_##Name##_v(JNIEnv* env, jobject object, jclass type, jmethodID method, va_list list) { \
        jvalue values[64]; \
        if (!arguments(env, method, 1, Code, list, values)) return (CType)0; \
        return (*env)->CallNonvirtual##Name##MethodA(env, object, type, method, values); \
    } \
    /** Captures guest varargs for a nonvirtual call. */ \
    static CType nonvirtual_##Name(JNIEnv* env, jobject object, jclass type, jmethodID method, ...) { \
        va_list list; va_start(list, method); \
        CType result = nonvirtual_##Name##_v(env, object, type, method, list); va_end(list); return result; \
    } \
    /** Decodes guest V arguments for a checked static call. */ \
    static CType static_##Name##_v(JNIEnv* env, jclass type, jmethodID method, va_list list) { \
        jvalue values[64]; \
        if (!arguments(env, method, 2, Code, list, values)) return (CType)0; \
        return (*env)->CallStatic##Name##MethodA(env, type, method, values); \
    } \
    /** Captures guest varargs for a static call. */ \
    static CType static_##Name(JNIEnv* env, jclass type, jmethodID method, ...) { \
        va_list list; va_start(list, method); \
        CType result = static_##Name##_v(env, type, method, list); va_end(list); return result; \
    }
CALL(Object, jobject, 'L')
CALL(Boolean, jboolean, 'Z')
CALL(Byte, jbyte, 'B')
CALL(Char, jchar, 'C')
CALL(Short, jshort, 'S')
CALL(Int, jint, 'I')
CALL(Long, jlong, 'J')
CALL(Float, jfloat, 'F')
CALL(Double, jdouble, 'D')
#undef CALL

/// Decodes guest V arguments for a void instance call.
static void call_Void_v(JNIEnv* env, jobject object, jmethodID method, va_list list) {
    jvalue values[64];
    if (arguments(env, method, 0, 'V', list, values)) (*env)->CallVoidMethodA(env, object, method, values);
}
/// Captures guest varargs for a void instance call.
static void call_Void(JNIEnv* env, jobject object, jmethodID method, ...) {
    va_list list; va_start(list, method); call_Void_v(env, object, method, list); va_end(list);
}
/// Decodes guest V arguments for a void nonvirtual call.
static void nonvirtual_Void_v(JNIEnv* env, jobject object, jclass type, jmethodID method, va_list list) {
    jvalue values[64];
    if (arguments(env, method, 1, 'V', list, values)) (*env)->CallNonvirtualVoidMethodA(env, object, type, method, values);
}
/// Captures guest varargs for a void nonvirtual call.
static void nonvirtual_Void(JNIEnv* env, jobject object, jclass type, jmethodID method, ...) {
    va_list list; va_start(list, method); nonvirtual_Void_v(env, object, type, method, list); va_end(list);
}
/// Decodes guest V arguments for a void static call.
static void static_Void_v(JNIEnv* env, jclass type, jmethodID method, va_list list) {
    jvalue values[64];
    if (arguments(env, method, 2, 'V', list, values)) (*env)->CallStaticVoidMethodA(env, type, method, values);
}
/// Captures guest varargs for a void static call.
static void static_Void(JNIEnv* env, jclass type, jmethodID method, ...) {
    va_list list; va_start(list, method); static_Void_v(env, type, method, list); va_end(list);
}

/// Installs only Android-side variadic/V entry addresses into the private runtime-owned table.
void winapk_jni_install(void** entries) {
    struct JNINativeInterface* table = (struct JNINativeInterface*)entries;
    table->NewObject = construct;
    table->NewObjectV = construct_v;
#define INSTALL(Name) \
    table->Call##Name##Method = call_##Name; table->Call##Name##MethodV = call_##Name##_v; \
    table->CallNonvirtual##Name##Method = nonvirtual_##Name; table->CallNonvirtual##Name##MethodV = nonvirtual_##Name##_v; \
    table->CallStatic##Name##Method = static_##Name; table->CallStatic##Name##MethodV = static_##Name##_v
    INSTALL(Object); INSTALL(Boolean); INSTALL(Byte); INSTALL(Char); INSTALL(Short);
    INSTALL(Int); INSTALL(Long); INSTALL(Float); INSTALL(Double); INSTALL(Void);
#undef INSTALL
}
