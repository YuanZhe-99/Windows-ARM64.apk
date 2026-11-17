#include <jni.h>

static int hooks;

/** Distinguishes ELF dependency loading from explicit Java JNI initialization. */
JNIEXPORT jint JNI_OnLoad(JavaVM* vm, void* reserved) {
    (void)vm; (void)reserved;
    hooks++;
    return JNI_VERSION_1_6;
}

/** Keeps the dependency DT_NEEDED and exposes whether its hook has run. */
int dependency_marker(void) { return 3141 + hooks; }

/** Supplies a JNI method only from the root's dependency scope. */
JNIEXPORT jint Java_org_winapk_fixture_jni_MainActivity_dependencyValue(JNIEnv* env, jclass type) {
    (void)env; (void)type;
    return dependency_marker();
}
