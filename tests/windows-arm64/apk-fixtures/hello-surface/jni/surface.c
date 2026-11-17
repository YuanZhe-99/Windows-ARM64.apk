#include <android/native_window_jni.h>
#include <stddef.h>
#include <stdint.h>

static ANativeWindow* held;

/** Acquires twice through JNI, verifies the borrowed dimensions and keeps exactly one reference. */
JNIEXPORT jlong JNICALL Java_org_winapk_fixture_surface_MainActivity_open(
    JNIEnv* env, jclass type, jobject surface, jobject unrelated) {
    (void)type;
    if (ANativeWindow_fromSurface(env, NULL) || ANativeWindow_fromSurface(env, unrelated)) return 0;
    ANativeWindow* first = ANativeWindow_fromSurface(env, surface);
    if (!first) return 0;
    ANativeWindow* same = ANativeWindow_fromSurface(env, surface);
    if (same != first) return 0;
    ANativeWindow_acquire(first);
    ANativeWindow_release(same);
    ANativeWindow_release(first);
    held = first;
    return (jlong)(uintptr_t)held;
}

/** Packs the two actual client dimensions without narrowing negative refusal codes. */
JNIEXPORT jlong JNICALL Java_org_winapk_fixture_surface_MainActivity_dimensions(JNIEnv* env, jclass type) {
    (void)env;
    (void)type;
    const int32_t w = ANativeWindow_getWidth(held), h = ANativeWindow_getHeight(held);
    return w < 0 || h < 0 ? -1 : ((jlong)w << 32) | (uint32_t)h;
}

/** Releasing the Java Surface forbids conversion but leaves the guest's live reference intact. */
JNIEXPORT jboolean JNICALL Java_org_winapk_fixture_surface_MainActivity_released(
    JNIEnv* env, jclass type, jobject surface) {
    (void)type;
    return ANativeWindow_fromSurface(env, surface) == NULL && ANativeWindow_getWidth(held) > 0;
}

/** The replacement has its own token, with stale and unknown tokens returning errors. */
JNIEXPORT jboolean JNICALL Java_org_winapk_fixture_surface_MainActivity_replace(
    JNIEnv* env, jclass type, jobject previous, jobject surface) {
    (void)type;
    if (ANativeWindow_fromSurface(env, previous) ||
        ANativeWindow_getWidth(held) >= 0 || ANativeWindow_getHeight(held) >= 0) return JNI_FALSE;
    ANativeWindow* fresh = ANativeWindow_fromSurface(env, surface);
    if (!fresh || fresh == held || ANativeWindow_getWidth(fresh) <= 0 || ANativeWindow_getHeight(fresh) <= 0) return JNI_FALSE;
    ANativeWindow_release(held);
    ANativeWindow_acquire(held);
    ANativeWindow_release(held);
    ANativeWindow_release(fresh);
    ANativeWindow* unknown = (ANativeWindow*)(uintptr_t)UINTPTR_MAX;
    ANativeWindow_acquire(unknown);
    ANativeWindow_release(unknown);
    return ANativeWindow_getWidth(fresh) < 0 && ANativeWindow_getHeight(unknown) < 0;
}
