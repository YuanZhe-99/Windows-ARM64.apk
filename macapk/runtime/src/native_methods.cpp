// native_methods.cpp -- JNI exports the Android framework reimpl
#include <pthread/qos.h>
// (Java code under runtime/java/) calls into.
//
// Layout: each Java class that needs native methods has a small block
// here. Block = a static array of JNINativeMethod entries followed by a
// RegisterNatives call inside RegisterAllNativeMethods. We keep all
// JNI signatures at one location so the surface is auditable.

#include "macapk/native_methods.h"
#include "macapk/gms_signin.h"
#include "macapk/native_host_proxy.h"
#include "macapk/cocoa_bridge.h"
#include "macapk/self_capture.h"   // SelfCaptureGlWanted
#include "macapk/gles_bridge.h"
#include "macapk/user_verification_bridge.h"

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include <jni.h>
#include <mach/mach_time.h>  // mach_absolute_time: the System.nanoTime() clock (Java vsync)
#include <malloc/malloc.h>   // malloc_size, for the bundled-sqlite3 allocator (macapk_sq_size)
#include <mutex>
#include <unordered_map>
#include <string>
#include <vector>
#include <unistd.h>       // getpid
#include <sys/sysctl.h>   // sysctl/KERN_PROC -- pairip-gate P_TRACED debugger check
#include <sys/proc.h>     // P_TRACED

#ifdef MACAPK_HAS_ANGLE
// Direct GLES2 / EGL headers. We forward Java GLES20 native methods
// straight to ANGLE's libGLESv2 symbols (we already link the dylib).
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include "angle_glue.h"   // AngleMarkFrameWithheld
#endif

namespace macapk {

// Window policy phase 2a, defined in cocoa_bridge.mm. It belongs in
// macapk/cocoa_bridge.h next to CocoaApplyWindowOrientation; it is declared here
// only because the lane that added it did not own that header (move it there
// with the header's next edit).
void CocoaSetTopActivityConfigChanges(int real_config_changes);
// Window policy phase 3, defined in cocoa_bridge.mm; forward-declared here for
// the same reason (they belong in macapk/cocoa_bridge.h).
bool CocoaActivityWindowTransition(const std::string& component, int event, bool occludes,
                                   bool has_request, int request, int behind,
                                   int real_config_changes, int token, int cover_token,
                                   int* app_w_px, int* app_h_px, int* change);
void CocoaApplyWindowOrientationFor(const std::string& component, int token,
                                    int screen_orientation, int behind);
int  CocoaWindowOverrideOrientation(const std::string& component, int record_orientation);

namespace {

// ---------- com.macapk.loader.NativeLog ----------
//
// Bare-bones log sink. Every "android.util.Log" call inside the framework
// reimpl ultimately funnels through here so we have ONE printf path,
// which makes it easy to redirect later (file, GUI launcher tail, etc.).

// Minimum level to EMIT, read once from MACAPK_LOG_LEVEL. Default 0 = emit
// everything (unchanged for direct CLI runs + the debug harnesses that grep
// the log). The launcher UI sets MACAPK_LOG_LEVEL=5 by default so a normal
// run is quiet (WARN+ERROR only, so a crash still surfaces) and FASTER -- the
// framework logs every classload/JNI call at INFO, tens of thousands of lines
// per boot. Levels match NativeLog: V=2 D=3 I=4 W=5 E=6. This one funnel gates
// both framework NativeLog.* and app-side android.util.Log.
static int macapk_log_min_level() {
    static const int lvl = [] {
        const char* e = std::getenv("MACAPK_LOG_LEVEL");
        return (e && *e) ? std::atoi(e) : 0;
    }();
    return lvl;
}

void Log_print(JNIEnv* env, jclass /*cls*/, jint level,
               jstring tag_j, jstring msg_j) {
    // Gate FIRST -- skip the two GetStringUTFChars copies + the fprintf when
    // this level is suppressed. This is where the per-boot log-spam cost dies.
    if (level < macapk_log_min_level()) return;
    const char* tag = tag_j ? env->GetStringUTFChars(tag_j, nullptr) : "";
    const char* msg = msg_j ? env->GetStringUTFChars(msg_j, nullptr) : "";
    const char* lv  =
        level <= 2 ? "V" :
        level == 3 ? "D" :
        level == 4 ? "I" :
        level == 5 ? "W" :
        level >= 6 ? "E" : "?";
    std::fprintf(stderr, "[%s] %s: %s\n", lv, tag, msg);
    if (tag_j) env->ReleaseStringUTFChars(tag_j, tag);
    if (msg_j) env->ReleaseStringUTFChars(msg_j, msg);
}

// ---------- com.macapk.gms.NativeSignIn ----------
//
// The bridge that lets the Java GMS layer put Google's sign-in page on screen.
// It exists because the decision "we need an account" is made in Java (inside a
// licence check) while the window is AppKit, and because an installed app
// launched from the Dock has no launcher process to delegate to.
//
// ⚠ THIS BLOCKS FOR AS LONG AS A HUMAN TAKES. That is correct and is why the
// Java caller runs it on a background thread: the alternative -- returning
// immediately and calling back later -- would need a callback path through JNI
// for no benefit, since every caller wants to continue only once there IS an
// account. MacApkPresentGoogleSignIn marshals the AppKit work onto the main
// queue and parks the calling thread, which is webview_bridge.mm's documented
// one-directional pattern (a guest thread may wait on main; main never waits on
// a guest thread).
jstring GmsSignIn_present(JNIEnv* env, jclass) {
    std::string token = MacApkPresentGoogleSignIn();
    if (token.empty()) return nullptr;   // cancelled or failed; never an empty string
    return env->NewStringUTF(token.c_str());
}

jboolean GmsSignIn_active(JNIEnv*, jclass) {
    return MacApkGoogleSignInActive() ? JNI_TRUE : JNI_FALSE;
}

const JNINativeMethod kGmsSignInMethods[] = {
    {const_cast<char*>("presentSignInWindow"),
     const_cast<char*>("()Ljava/lang/String;"),
     reinterpret_cast<void*>(GmsSignIn_present)},
    {const_cast<char*>("signInWindowActive"),
     const_cast<char*>("()Z"),
     reinterpret_cast<void*>(GmsSignIn_active)},
};

const JNINativeMethod kNativeLogMethods[] = {
    {const_cast<char*>("print"),
     const_cast<char*>("(ILjava/lang/String;Ljava/lang/String;)V"),
     reinterpret_cast<void*>(Log_print)},
};

// ---------- com.macapk.loader.NativeCocoa ----------
//
// All NSWindow / NSView / MTKView creation, plus the runloop. The
// returned pointers are stuffed in `long` fields on Java view objects;
// they're opaque to the Java side.

jboolean Cocoa_init(JNIEnv*, jclass) {
    return CocoaInit() ? JNI_TRUE : JNI_FALSE;
}

void Cocoa_setPhoneSize(JNIEnv*, jclass, jint w, jint h) {
    CocoaSetPhoneSize(static_cast<int>(w), static_cast<int>(h));
}

void Cocoa_setStartingWindowColor(JNIEnv*, jclass, jint argb) {
    CocoaSetStartingWindowColor(static_cast<int>(argb));
}

void Cocoa_setMaxAspectRatio(JNIEnv*, jclass, jfloat ratio) {
    CocoaSetMaxAspectRatio(static_cast<float>(ratio));
}

void Cocoa_applyRequestedOrientation(JNIEnv*, jclass, jboolean landscape) {
    CocoaApplyRequestedOrientation(landscape == JNI_TRUE);
}

void Cocoa_restoreUserOrientation(JNIEnv*, jclass) {
    CocoaRestoreUserOrientation();
}

void Cocoa_applyWindowOrientation(JNIEnv*, jclass, jint screen_orientation) {
    CocoaApplyWindowOrientation(static_cast<int>(screen_orientation));
}

// v1.0.1649 (window policy phase 2a): the top activity's real configChanges,
// pushed by ActivityHost on every resume so the drag hold never needs a
// synchronous JNI call from the AppKit thread.
void Cocoa_setTopActivityConfigChanges(JNIEnv*, jclass, jint real_config_changes) {
    CocoaSetTopActivityConfigChanges(static_cast<int>(real_config_changes));
}

// v1.0.1649 (window policy phase 3, every activity gets its own box): the
// activity-stack hooks ActivityHost calls on the Android owner thread.
std::string WindowComponentString(JNIEnv* env, jstring s) {
    if (!s) return std::string();
    const char* c = env->GetStringUTFChars(s, nullptr);
    std::string out = c ? c : "";
    if (c) env->ReleaseStringUTFChars(s, c);
    return out;
}

jintArray Cocoa_activityWindowTransition(JNIEnv* env, jclass, jstring component, jint event,
                                         jboolean occludes, jboolean has_request, jint request,
                                         jint behind, jint real_config_changes, jint token,
                                         jint cover_token) {
    int w = 0, h = 0, change = 0;
    if (!CocoaActivityWindowTransition(WindowComponentString(env, component),
                                       static_cast<int>(event), occludes == JNI_TRUE,
                                       has_request == JNI_TRUE, static_cast<int>(request),
                                       static_cast<int>(behind),
                                       static_cast<int>(real_config_changes),
                                       static_cast<int>(token), static_cast<int>(cover_token),
                                       &w, &h, &change))
        return nullptr;
    jintArray out = env->NewIntArray(3);
    if (!out) return nullptr;
    const jint v[3] = {static_cast<jint>(w), static_cast<jint>(h), static_cast<jint>(change)};
    env->SetIntArrayRegion(out, 0, 3, v);
    return out;
}

void Cocoa_applyWindowOrientationFor(JNIEnv* env, jclass, jstring component, jint token,
                                     jint screen_orientation, jint behind) {
    CocoaApplyWindowOrientationFor(WindowComponentString(env, component), static_cast<int>(token),
                                   static_cast<int>(screen_orientation), static_cast<int>(behind));
}

jint Cocoa_windowOverrideOrientation(JNIEnv* env, jclass, jstring component,
                                     jint record_orientation) {
    return static_cast<jint>(CocoaWindowOverrideOrientation(WindowComponentString(env, component),
                                                            static_cast<int>(record_orientation)));
}

jint Cocoa_phoneWidth(JNIEnv*, jclass)  { return CocoaPhoneWidth();  }
jint Cocoa_displayHz(JNIEnv*, jclass)   { return CocoaDisplayHz();  }
jfloat Cocoa_backingScale(JNIEnv*, jclass) { return CocoaBackingScale(); }
jboolean Cocoa_postHostMouse(JNIEnv*, jclass, jint action, jfloat fx, jfloat fy) {
    return CocoaPostHostMouse(static_cast<int>(action), fx, fy) ? JNI_TRUE : JNI_FALSE;
}
jint Cocoa_phoneHeight(JNIEnv*, jclass) { return CocoaPhoneHeight(); }

void Cocoa_runLoop(JNIEnv*, jclass) { CocoaRunLoop(); }

jlong Cocoa_createWindow(JNIEnv* env, jclass, jstring title_j) {
    const char* title = title_j ? env->GetStringUTFChars(title_j, nullptr) : "Mac.apk";
    void* handle = CocoaCreateWindow(title);
    if (title_j) env->ReleaseStringUTFChars(title_j, title);
    return reinterpret_cast<jlong>(handle);
}

jlong Cocoa_getPlaceholderWindow(JNIEnv*, jclass) {
    return reinterpret_cast<jlong>(CocoaGetPlaceholderWindow());
}

// v1.0.690: open a URL/URI on the macOS host (NSWorkspace). This is the
// resolution target for an APK's IMPLICIT ACTION_VIEW/ACTION_SENDTO/ACTION_DIAL
// intent -- the honest Mac-native answer for "open this http/mailto/tel link"
// when there is no in-APK component to hand it to. Returns true if the host
// accepted it.
jboolean Cocoa_openHostUrl(JNIEnv* env, jclass, jstring url_j) {
    if (!url_j) return JNI_FALSE;
    const char* url = env->GetStringUTFChars(url_j, nullptr);
    bool ok = url && url[0] ? CocoaOpenHostUrl(url) : false;
    if (url) env->ReleaseStringUTFChars(url_j, url);
    return ok ? JNI_TRUE : JNI_FALSE;
}

// The App Link sign-in window (com.macapk.loader.AppLinks): show `url` in the
// Mac.apk window; the navigation to `prefix` is handed back to the app.
jboolean Cocoa_openAppLinkWindow(JNIEnv* env, jclass, jstring url_j, jstring prefix_j) {
    if (!url_j || !prefix_j) return JNI_FALSE;
    const char* url = env->GetStringUTFChars(url_j, nullptr);
    const char* prefix = env->GetStringUTFChars(prefix_j, nullptr);
    bool ok = url && prefix && CocoaOpenAppLinkWindow(url, prefix);
    if (url) env->ReleaseStringUTFChars(url_j, url);
    if (prefix) env->ReleaseStringUTFChars(prefix_j, prefix);
    return ok ? JNI_TRUE : JNI_FALSE;
}

// Host URL intake (MACAPK_URL_SCHEMES): the URLs LaunchServices delivered via
// kAEGetURL since the last call, oldest first; an empty array when none.
jobjectArray Cocoa_takeHostUrls(JNIEnv* env, jclass) {
    std::vector<std::string> urls;
    CocoaTakeHostUrls(urls);
    jclass strCls = env->FindClass("java/lang/String");
    if (!strCls) return nullptr;
    jobjectArray arr = env->NewObjectArray(static_cast<jsize>(urls.size()), strCls, nullptr);
    env->DeleteLocalRef(strCls);
    if (!arr) return nullptr;
    for (size_t i = 0; i < urls.size(); i++) {
        jstring js = env->NewStringUTF(urls[i].c_str());
        if (!js) { env->ExceptionClear(); continue; }
        env->SetObjectArrayElement(arr, static_cast<jsize>(i), js);
        env->DeleteLocalRef(js);
    }
    return arr;
}

// Host URL intake: bundle paths of the applications LaunchServices already lists
// for `scheme`; null when the query is unavailable (unknown), an empty array for
// "no handler".
jobjectArray Cocoa_urlSchemeHandlers(JNIEnv* env, jclass, jstring scheme_j) {
    if (!scheme_j) return nullptr;
    const char* sc = env->GetStringUTFChars(scheme_j, nullptr);
    if (!sc) return nullptr;
    std::vector<std::string> apps;
    bool ok = CocoaUrlSchemeHandlers(sc, apps);
    env->ReleaseStringUTFChars(scheme_j, sc);
    if (!ok) return nullptr;
    jclass strCls = env->FindClass("java/lang/String");
    if (!strCls) return nullptr;
    jobjectArray arr = env->NewObjectArray(static_cast<jsize>(apps.size()), strCls, nullptr);
    env->DeleteLocalRef(strCls);
    if (!arr) return nullptr;
    for (size_t i = 0; i < apps.size(); i++) {
        jstring js = env->NewStringUTF(apps[i].c_str());
        if (!js) { env->ExceptionClear(); continue; }
        env->SetObjectArrayElement(arr, static_cast<jsize>(i), js);
        env->DeleteLocalRef(js);
    }
    return arr;
}

// Host URL intake, redirect routing (MACAPK_URL_REDIRECT_OPENER): make this app's
// bundle the default handler of `scheme`. See CocoaSetUrlSchemeDefault.
jint Cocoa_setUrlSchemeDefault(JNIEnv* env, jclass, jstring bundle_j, jstring scheme_j) {
    if (!bundle_j || !scheme_j) return -1;
    const char* b = env->GetStringUTFChars(bundle_j, nullptr);
    const char* sc = env->GetStringUTFChars(scheme_j, nullptr);
    int r = (b && sc) ? CocoaSetUrlSchemeDefault(b, sc) : -1;
    if (b) env->ReleaseStringUTFChars(bundle_j, b);
    if (sc) env->ReleaseStringUTFChars(scheme_j, sc);
    return static_cast<jint>(r);
}

jlong Cocoa_createMetalSurfaceView(JNIEnv*, jclass, jint w, jint h) {
    return reinterpret_cast<jlong>(
        CocoaCreateMetalSurfaceView(static_cast<int>(w), static_cast<int>(h)));
}

// v1.0.91: Path-B variant for SurfaceView's GL render target. Does NOT
// claim the Canvas overlay; see cocoa_bridge.h docstring for the
// "Flappy splash overlay-on-top-of-GL" reason this exists.
jlong Cocoa_createMetalSurfaceViewForGl(JNIEnv*, jclass, jint w, jint h) {
    return reinterpret_cast<jlong>(
        CocoaCreateMetalSurfaceViewForGl(static_cast<int>(w),
                                         static_cast<int>(h)));
}

// MACAPK_GLSV_RENDER_MODE (GLSurfaceView.macapkWithheldTickNeedsFrame): the Java
// view is about to withhold this tick. Returns true when it should draw anyway
// because a self-capture waits for a real "gl" frame (then the frame is not
// marked); otherwise marks the frame so AngleEndFrame skips the readbacks.
jboolean Cocoa_beginWithheldTick(JNIEnv*, jclass, jboolean mayDraw) {
#ifdef MACAPK_HAS_ANGLE
    const bool wants = mayDraw == JNI_TRUE && SelfCaptureGlWanted();
    AngleMarkFrameWithheld(!wants);
    return wants ? JNI_TRUE : JNI_FALSE;
#else
    (void)mayDraw;
    return JNI_FALSE;
#endif
}

void Cocoa_setContentView(JNIEnv*, jclass, jlong window, jlong view) {
    CocoaSetWindowContentView(reinterpret_cast<void*>(window),
                              reinterpret_cast<void*>(view));
}

void Cocoa_showWindow(JNIEnv*, jclass, jlong window) {
    CocoaShowWindow(reinterpret_cast<void*>(window));
}

void Cocoa_closeWindow(JNIEnv*, jclass, jlong window) {
    CocoaCloseWindow(reinterpret_cast<void*>(window));
}

void Cocoa_setWindowTitle(JNIEnv* env, jclass, jlong window, jstring title_j) {
    const char* title = title_j ? env->GetStringUTFChars(title_j, nullptr) : "";
    CocoaSetWindowTitle(reinterpret_cast<void*>(window), title);
    if (title_j) env->ReleaseStringUTFChars(title_j, title);
}

void Cocoa_setRenderer(JNIEnv* env, jclass, jlong metal_view, jobject glsurfaceview) {
    if (!metal_view || !glsurfaceview) return;
    JavaVM* vm = nullptr;
    env->GetJavaVM(&vm);
    // The render delegate is a single slot per MTKView: the LAST registrant
    // receives every macapkSurfaceChanged/macapkDrawFrame callback. Log who
    // claims it so surface-handoff bugs (a later registrant silently stealing
    // the delegate from the app's real SurfaceView) are visible in any run.
    {
        jclass ocls = env->GetObjectClass(glsurfaceview);
        jclass ccls = env->FindClass("java/lang/Class");
        jmethodID mid = ccls ? env->GetMethodID(ccls, "getName", "()Ljava/lang/String;") : nullptr;
        if (mid) {
            jstring name_j = (jstring) env->CallObjectMethod(ocls, mid);
            if (name_j) {
                const char* name = env->GetStringUTFChars(name_j, nullptr);
                std::fprintf(stderr,
                    "[I] cocoa_v2: setRenderer view=%p target=%s\n",
                    reinterpret_cast<void*>(metal_view), name ? name : "?");
                if (name) env->ReleaseStringUTFChars(name_j, name);
                env->DeleteLocalRef(name_j);
            }
        }
        if (env->ExceptionCheck()) env->ExceptionClear();
        if (ccls) env->DeleteLocalRef(ccls);
        env->DeleteLocalRef(ocls);
    }
    jobject g_ref = env->NewGlobalRef(glsurfaceview);
    CocoaMetalSurfaceSetRenderer(reinterpret_cast<void*>(metal_view),
                                 vm,
                                 reinterpret_cast<long>(g_ref));
}

void Cocoa_registerTouchDispatcher(JNIEnv* env, jclass, jobject dispatcher) {
    if (!dispatcher) return;
    JavaVM* vm = nullptr;
    env->GetJavaVM(&vm);
    jobject g_ref = env->NewGlobalRef(dispatcher);
    CocoaRegisterTouchDispatcher(vm, reinterpret_cast<long>(g_ref));
}

// Boot.java hands the per-version data subdir name here. The shim's
// fopen path-remap uses it instead of the raw package name when
// expanding relative paths -- so two versions of the same APK
// (`<pkg>_<v1>` vs `<pkg>_<v2>`) get isolated filesystem state.
// Routes through dlsym so we don't need a hard link-time dep on
// the bionic shim (it's loaded by main.cpp before this Java method
// gets called).
// Mirror one android.os.SystemProperties entry into the bionic shim's property
// store, so a GUEST asking `__system_property_get("ro.build.version.sdk", ...)`
// gets the same answer Java gives. Before this the shim answered "not found"
// for every key, so a guest's inlined `android_get_device_api_level()` returned
// -1 while Java said PresentedSdk.get().
//
// PUSH, never derive: the value arrives already computed by Java. Re-deriving
// the device level in C++ would recreate exactly the multi-source incoherence
// PresentedSdk exists to end. Same dlsym idiom as setDataSubdir below, and for
// the same reason -- the shim is dlopened by main.cpp, not link-time linked.
void Cocoa_exitProcess(JNIEnv* env, jclass, jint code, jstring file_j) {
    std::string file;
    if (file_j) {
        const char* f = env->GetStringUTFChars(file_j, nullptr);
        if (f) { file = f; env->ReleaseStringUTFChars(file_j, f); }
    }
    macapk::CocoaExitProcess((int) code, file.empty() ? nullptr : file.c_str());
}

jboolean Cocoa_hostRenderTick(JNIEnv*, jclass) {
    return macapk::CocoaHostRenderTick() ? JNI_TRUE : JNI_FALSE;
}

jboolean Cocoa_hostHideWindowsForQuit(JNIEnv*, jclass) {
    return macapk::CocoaHostHideWindowsForQuit() ? JNI_TRUE : JNI_FALSE;
}

void Cocoa_quitBackstopExit(JNIEnv*, jclass) {
    macapk::CocoaQuitBackstopExit();
}

jboolean Cocoa_debugTrackpad(JNIEnv* env, jclass, jint kind, jfloatArray a_j) {
    if (!a_j) return JNI_FALSE;
    const jsize n = env->GetArrayLength(a_j);
    std::vector<float> a(static_cast<size_t>(n));
    if (n > 0) env->GetFloatArrayRegion(a_j, 0, n, a.data());
    return CocoaDebugTrackpad(static_cast<int>(kind), a.data(), static_cast<int>(n))
        ? JNI_TRUE : JNI_FALSE;
}

void Cocoa_debugQuitGesture(JNIEnv* env, jclass, jstring kind_j) {
    std::string kind;
    if (kind_j) {
        const char* k = env->GetStringUTFChars(kind_j, nullptr);
        if (k) { kind = k; env->ReleaseStringUTFChars(kind_j, k); }
    }
    macapk::CocoaDebugQuitGesture(kind.c_str());
}

void Cocoa_moveTaskToBack(JNIEnv*, jclass) { macapk::CocoaMoveTaskToBack(); }
void Cocoa_moveTaskToFront(JNIEnv*, jclass) { macapk::CocoaMoveTaskToFront(); }
void Cocoa_releaseRenderer(JNIEnv*, jclass, jlong view) {
    macapk::CocoaMetalSurfaceReleaseRenderer(reinterpret_cast<void*>(view));
}

void Cocoa_setSystemProperty(JNIEnv* env, jclass, jstring key_j, jstring val_j) {
    if (!key_j) return;
    const char* key = env->GetStringUTFChars(key_j, nullptr);
    const char* val = val_j ? env->GetStringUTFChars(val_j, nullptr) : nullptr;
    if (key) {
        typedef void (*put_fn_t)(const char*, const char*);
        put_fn_t put_fn = reinterpret_cast<put_fn_t>(
            dlsym(RTLD_DEFAULT, "__macapk_property_put"));
        if (put_fn) {
            put_fn(key, val);
        } else {
            // Loud once per key rather than silent: a missing bridge here means
            // every guest silently goes back to api level -1.
            std::fprintf(stderr,
                "[macapk] shim missing __macapk_property_put (key=%s): %s\n",
                key, dlerror());
        }
    }
    if (val) env->ReleaseStringUTFChars(val_j, val);
    if (key) env->ReleaseStringUTFChars(key_j, key);
}

// The shim's synthesized host file for a /proc path (bionic_shim.cpp
// __macapk_remap_proc_path), so Java reads of /proc/self/* see what native
// code sees. Null when the path is not one the shim answers.
jstring Cocoa_remapProcPath(JNIEnv* env, jclass, jstring path_j) {
    if (!path_j) return nullptr;
    typedef int (*remap_fn_t)(const char*, char*, size_t);
    static remap_fn_t fn = reinterpret_cast<remap_fn_t>(
        dlsym(RTLD_DEFAULT, "__macapk_remap_proc_path"));
    if (!fn) return nullptr;
    const char* path = env->GetStringUTFChars(path_j, nullptr);
    if (!path) return nullptr;
    char out[PATH_MAX];
    int n = fn(path, out, sizeof out);
    env->ReleaseStringUTFChars(path_j, path);
    return n > 0 ? env->NewStringUTF(out) : nullptr;
}

void Cocoa_setDataSubdir(JNIEnv* env, jclass, jstring subdir_j) {
    if (!subdir_j) return;
    const char* subdir = env->GetStringUTFChars(subdir_j, nullptr);
    if (subdir) {
        typedef void (*set_fn_t)(const char*);
        set_fn_t set_fn = reinterpret_cast<set_fn_t>(
            dlsym(RTLD_DEFAULT, "__macapk_set_data_subdir"));
        if (set_fn) {
            set_fn(subdir);
        } else {
            std::fprintf(stderr,
                "[macapk] shim missing __macapk_set_data_subdir: %s\n",
                dlerror());
        }
        env->ReleaseStringUTFChars(subdir_j, subdir);
    }
}

void Cocoa_setNativeLibDir(JNIEnv* env, jclass, jstring dir_j) {
    if (!dir_j) return;
    const char* dir = env->GetStringUTFChars(dir_j, nullptr);
    if (dir) {
        typedef void (*set_fn_t)(const char*);
        set_fn_t set_fn = reinterpret_cast<set_fn_t>(
            dlsym(RTLD_DEFAULT, "__macapk_set_guest_lib_dir"));
        if (set_fn) set_fn(dir);
        else std::fprintf(stderr,
                 "[macapk] shim missing __macapk_set_guest_lib_dir: %s\n", dlerror());
        env->ReleaseStringUTFChars(dir_j, dir);
    }
}

// /system/fonts for native text stacks: Boot materializes the directory
// (android.graphics.MacApkSystemFonts) and the shim's path remap serves
// /system/etc/fonts.xml and /system/fonts/<file> from it.
void Cocoa_setSystemFontsDir(JNIEnv* env, jclass, jstring dir_j) {
    if (!dir_j) return;
    const char* dir = env->GetStringUTFChars(dir_j, nullptr);
    if (dir) {
        typedef void (*set_fn_t)(const char*);
        set_fn_t set_fn = reinterpret_cast<set_fn_t>(
            dlsym(RTLD_DEFAULT, "__macapk_set_system_fonts_dir"));
        if (set_fn) set_fn(dir);
        else std::fprintf(stderr,
                 "[macapk] shim missing __macapk_set_system_fonts_dir: %s\n", dlerror());
        env->ReleaseStringUTFChars(dir_j, dir);
    }
}

// NativeCocoa.hostFontFile (host_fonts.cpp, CoreText). C linkage: this
// declaration sits in an unnamed namespace, where a C++ name would get internal
// linkage and never meet its definition.
extern "C" jstring Cocoa_hostFontFile(JNIEnv* env, jclass, jstring ps_j);

// Stage S3 of the shared-storage design: one volume path for every installed
// app (SharedStorage.adoptCanonicalVolume). The shim's setter has existed since
// S0 (bionic_shim.cpp __macapk_set_shared_storage_root); this is its caller.
void Cocoa_setSharedStorageRoot(JNIEnv* env, jclass, jstring dir_j) {
    if (!dir_j) return;
    const char* dir = env->GetStringUTFChars(dir_j, nullptr);
    if (dir) {
        typedef void (*set_fn_t)(const char*);
        set_fn_t set_fn = reinterpret_cast<set_fn_t>(
            dlsym(RTLD_DEFAULT, "__macapk_set_shared_storage_root"));
        if (set_fn) set_fn(dir);
        else std::fprintf(stderr,
                 "[macapk] shim missing __macapk_set_shared_storage_root: %s\n", dlerror());
        env->ReleaseStringUTFChars(dir_j, dir);
    }
}

// Translation splash. AppClassLoader.translate() calls these around the
// translator-subprocess invocation so the user sees "Translating
// libgame.so…" with a live status line instead of a black window for
// 2-5 minutes. See CocoaShowTranslationOverlay in cocoa_bridge.mm for
// the resource-cost note (~zero; two NSTextFields, no animations).
void Cocoa_showTranslationOverlay(JNIEnv* env, jclass, jstring libname_j) {
    const char* libname = libname_j
        ? env->GetStringUTFChars(libname_j, nullptr)
        : "";
    CocoaShowTranslationOverlay(libname ? libname : "");
    if (libname_j && libname) {
        env->ReleaseStringUTFChars(libname_j, libname);
    }
}

void Cocoa_updateTranslationOverlay(JNIEnv* env, jclass, jstring detail_j) {
    const char* detail = detail_j
        ? env->GetStringUTFChars(detail_j, nullptr)
        : "";
    CocoaUpdateTranslationOverlay(detail ? detail : "");
    if (detail_j && detail) {
        env->ReleaseStringUTFChars(detail_j, detail);
    }
}

void Cocoa_hideTranslationOverlay(JNIEnv*, jclass) {
    CocoaHideTranslationOverlay();
}

}  // namespace -- reopened just below: the two declarations in between name
   // cocoa_bridge.mm functions, so they must not get internal linkage.
// The pinned-pixel present entry points, defined in cocoa_bridge.mm beside
// CocoaPresentBitmap (its "THE PIXEL WINDOW" comment is the contract): the
// native side calls `acquire` right before its first read of the pixels and
// `release` right after its last, and does nothing but read memory in between.
typedef const uint32_t* (*MacapkPixelsAcquireFn)(void* ctx);
typedef void (*MacapkPixelsReleaseFn)(void* ctx);
void CocoaPresentBitmapFrom(int width, int height, MacapkPixelsAcquireFn acquire,
                            MacapkPixelsReleaseFn release, void* ctx);
void CocoaPresentOverlayBitmapFrom(int width, int height, MacapkPixelsAcquireFn acquire,
                                   MacapkPixelsReleaseFn release, void* ctx);
void CocoaPresentSurfaceBitmapFrom(int surface_key, int width, int height,
                                   MacapkPixelsAcquireFn acquire,
                                   MacapkPixelsReleaseFn release, void* ctx);
int CocoaPresentFrameFrom(int width, int height, int bl, int bt, int br, int bb, int flags,
                          MacapkPixelsAcquireFn acquire, MacapkPixelsReleaseFn release,
                          void* ctx);
namespace {

// Present fast path, step 2 (2026-09-25, P1.2 of
// docs/canvas-present-cost-at-full-screen_2026-09-25.md): PIN the Java int[]
// with GetPrimitiveArrayCritical for just the native pixel passes instead of
// GetIntArrayElements, which in HotSpot mallocs a new C-heap array and copies
// the whole int[] into it on every call (jdk21u jni.cpp,
// DEFINE_GETSCALARARRAYELEMENTS: NEW_C_HEAP_ARRAY_RETURN_NULL +
// arraycopy_to_native, *isCopy = JNI_TRUE; GetPrimitiveArrayCritical pins and
// returns the array base with *isCopy = JNI_FALSE). Victim: a 24 MB malloc +
// copy + free per presented frame at the 3360x1796 full-screen window, the
// ~1.5-2.5 ms residual of F-Droid's measured 6.6-7.0 ms present. The pin is a
// GCLocker critical region under our -XX:+UseSerialGC, so the native side does
// only its pixel reads inside it and releases before the CGImage, the self-
// capture, the dump and the main-thread dispatch.
// BISECT GATE, default ON: MACAPK_PRESENT_CRITICAL=0 (or
// MACAPK_PRESENT_FASTPATH=0, every present fast-path step at once) restores
// GetIntArrayElements.
bool PresentPinEnabled() {
    static const bool on = [] {
        const char* all = std::getenv("MACAPK_PRESENT_FASTPATH");
        const char* one = std::getenv("MACAPK_PRESENT_CRITICAL");
        return !(all && std::strcmp(all, "0") == 0)
            && !(one && std::strcmp(one, "0") == 0);
    }();
    return on;
}

struct PinnedIntArray {
    JNIEnv*   env;
    jintArray array;
    void*     elems;
};
const uint32_t* AcquirePinnedIntArray(void* ctx) {
    auto* p = static_cast<PinnedIntArray*>(ctx);
    p->elems = p->env->GetPrimitiveArrayCritical(p->array, nullptr);
    return static_cast<const uint32_t*>(p->elems);
}
void ReleasePinnedIntArray(void* ctx) {
    auto* p = static_cast<PinnedIntArray*>(ctx);
    // A failed pin (null, with a pending OutOfMemoryError) has nothing to
    // release; the release callback runs regardless, by contract.
    if (p->elems) p->env->ReleasePrimitiveArrayCritical(p->array, p->elems, JNI_ABORT);
    p->elems = nullptr;
}

// presentBitmap(int[] argb, int w, int h) — push a Java-side Canvas-
// painted Bitmap to the screen. The pixels are only READ (JNI_ABORT: nothing is
// copied back), and the native side keeps its own premultiplied copy for the
// CGDataProvider's lifetime, so the int[] is free again when this returns.
void Cocoa_presentBitmap(JNIEnv* env, jclass,
                          jintArray argb_j, jint width, jint height) {
    if (!argb_j || width <= 0 || height <= 0) return;
    jsize len = env->GetArrayLength(argb_j);
    if (len < width * height) return;
    if (PresentPinEnabled()) {
        PinnedIntArray pin{env, argb_j, nullptr};
        CocoaPresentBitmapFrom(width, height, AcquirePinnedIntArray,
                               ReleasePinnedIntArray, &pin);
        return;
    }
    jint* elems = env->GetIntArrayElements(argb_j, nullptr);
    if (!elems) return;
    CocoaPresentBitmap(reinterpret_cast<const uint32_t*>(elems), width, height);
    env->ReleaseIntArrayElements(argb_j, elems, JNI_ABORT);  // don't copy back
}

// presentOverlayBitmap / clearOverlayBitmap (2026-07-23): the GL-surface
// twin of presentBitmap, for WindowManager overlays (Dialog/AlertDialog/
// PopupWindow) shown on top of a GL-rendered app -- see
// WindowOverlayStack.presentForGl (Java) + CocoaPresentOverlayBitmap
// (cocoa_bridge.mm) for the full mechanism. Pinned the same way, behind the
// same gate.
void Cocoa_presentOverlayBitmap(JNIEnv* env, jclass,
                                 jintArray argb_j, jint width, jint height) {
    if (!argb_j || width <= 0 || height <= 0) return;
    jsize len = env->GetArrayLength(argb_j);
    if (len < width * height) return;
    if (PresentPinEnabled()) {
        PinnedIntArray pin{env, argb_j, nullptr};
        CocoaPresentOverlayBitmapFrom(width, height, AcquirePinnedIntArray,
                                      ReleasePinnedIntArray, &pin);
        return;
    }
    jint* elems = env->GetIntArrayElements(argb_j, nullptr);
    if (!elems) return;
    CocoaPresentOverlayBitmap(reinterpret_cast<const uint32_t*>(elems), width, height);
    env->ReleaseIntArrayElements(argb_j, elems, JNI_ABORT);
}

void Cocoa_clearOverlayBitmap(JNIEnv*, jclass) {
    CocoaClearOverlayBitmap();
}

// presentSurfaceBitmap(int key, int[] argb, int w, int h) (2026-09-26): a
// lockCanvas SurfaceView's frame, presented through that surface's own layer
// (CocoaPresentSurfaceBitmapFrom in cocoa_bridge.mm has the contract) instead
// of over the view tree's overlay. Pinned the same way, behind the same gate,
// as presentBitmap.
const uint32_t* AcquirePlainPointer(void* ctx) {
    return static_cast<const uint32_t*>(ctx);
}
void Cocoa_presentSurfaceBitmap(JNIEnv* env, jclass, jint key,
                                jintArray argb_j, jint width, jint height) {
    if (!argb_j || width <= 0 || height <= 0) return;
    const jsize len = env->GetArrayLength(argb_j);
    if ((long long) len < (long long) width * height) return;
    if (PresentPinEnabled()) {
        PinnedIntArray pin{env, argb_j, nullptr};
        CocoaPresentSurfaceBitmapFrom(key, width, height, AcquirePinnedIntArray,
                                      ReleasePinnedIntArray, &pin);
        return;
    }
    jint* elems = env->GetIntArrayElements(argb_j, nullptr);
    if (!elems) return;
    CocoaPresentSurfaceBitmapFrom(key, width, height, AcquirePlainPointer, nullptr, elems);
    env->ReleaseIntArrayElements(argb_j, elems, JNI_ABORT);
}

// presentFrame(int[] argb, int w, int h, int bl, int bt, int br, int bb,
// int flags) -> int: the IOSurface-ring present with the exact diff, elision
// and validator (P2/P3 of docs/canvas-present-cost-at-full-screen_2026-09-25.md;
// CocoaPresentFrameFrom in cocoa_bridge.mm is the whole contract). Called by
// ViewCanvasDriver only when MACAPK_PRESENT_IOSURFACE=1, which is off by
// default; otherwise presentBitmap above is the present, unchanged. The pixels
// are read under the same pin and the same bisect gate as presentBitmap, and
// only during the call. Returns NativeCocoa.PRESENT_NONE / _PUBLISHED /
// _ELIDED_IDENTICAL / _FALLBACK.
jint Cocoa_presentFrame(JNIEnv* env, jclass, jintArray argb_j, jint width, jint height,
                        jint bl, jint bt, jint br, jint bb, jint flags) {
    if (!argb_j || width <= 0 || height <= 0) return 0;
    const jsize len = env->GetArrayLength(argb_j);
    if ((long long) len < (long long) width * height) return 0;
    if (PresentPinEnabled()) {
        PinnedIntArray pin{env, argb_j, nullptr};
        return CocoaPresentFrameFrom(width, height, bl, bt, br, bb, flags,
                                     AcquirePinnedIntArray, ReleasePinnedIntArray, &pin);
    }
    jint* elems = env->GetIntArrayElements(argb_j, nullptr);
    if (!elems) return 0;
    const int r = CocoaPresentFrameFrom(
        width, height, bl, bt, br, bb, flags,
        [](void* c) -> const uint32_t* { return static_cast<const uint32_t*>(c); },
        nullptr, elems);
    env->ReleaseIntArrayElements(argb_j, elems, JNI_ABORT);  // don't copy back
    return r;
}

const JNINativeMethod kNativeCocoaMethods[] = {
    {const_cast<char*>("init"),                   const_cast<char*>("()Z"),
     reinterpret_cast<void*>(Cocoa_init)},
    {const_cast<char*>("setPhoneSize"),           const_cast<char*>("(II)V"),
     reinterpret_cast<void*>(Cocoa_setPhoneSize)},
    {const_cast<char*>("setMaxAspectRatio"),      const_cast<char*>("(F)V"),
     reinterpret_cast<void*>(Cocoa_setMaxAspectRatio)},
    {const_cast<char*>("setStartingWindowColor"), const_cast<char*>("(I)V"),
     reinterpret_cast<void*>(Cocoa_setStartingWindowColor)},
    {const_cast<char*>("applyRequestedOrientation"), const_cast<char*>("(Z)V"),
     reinterpret_cast<void*>(Cocoa_applyRequestedOrientation)},
    {const_cast<char*>("restoreUserOrientation"), const_cast<char*>("()V"),
     reinterpret_cast<void*>(Cocoa_restoreUserOrientation)},
    {const_cast<char*>("applyWindowOrientation"), const_cast<char*>("(I)V"),
     reinterpret_cast<void*>(Cocoa_applyWindowOrientation)},
    {const_cast<char*>("setTopActivityConfigChanges"), const_cast<char*>("(I)V"),
     reinterpret_cast<void*>(Cocoa_setTopActivityConfigChanges)},
    {const_cast<char*>("activityWindowTransition"),
     const_cast<char*>("(Ljava/lang/String;IZZIIIII)[I"),
     reinterpret_cast<void*>(Cocoa_activityWindowTransition)},
    {const_cast<char*>("applyWindowOrientationFor"), const_cast<char*>("(Ljava/lang/String;III)V"),
     reinterpret_cast<void*>(Cocoa_applyWindowOrientationFor)},
    {const_cast<char*>("windowOverrideOrientation"), const_cast<char*>("(Ljava/lang/String;I)I"),
     reinterpret_cast<void*>(Cocoa_windowOverrideOrientation)},
    {const_cast<char*>("phoneWidth"),             const_cast<char*>("()I"),
     reinterpret_cast<void*>(Cocoa_phoneWidth)},
    {const_cast<char*>("displayHz"),              const_cast<char*>("()I"),
     reinterpret_cast<void*>(Cocoa_displayHz)},
    {const_cast<char*>("backingScale"),           const_cast<char*>("()F"),
     reinterpret_cast<void*>(Cocoa_backingScale)},
    {const_cast<char*>("postHostMouse"),          const_cast<char*>("(IFF)Z"),
     reinterpret_cast<void*>(Cocoa_postHostMouse)},
    {const_cast<char*>("phoneHeight"),            const_cast<char*>("()I"),
     reinterpret_cast<void*>(Cocoa_phoneHeight)},
    {const_cast<char*>("runLoop"),                const_cast<char*>("()V"),
     reinterpret_cast<void*>(Cocoa_runLoop)},
    {const_cast<char*>("openHostUrl"),            const_cast<char*>("(Ljava/lang/String;)Z"),
     reinterpret_cast<void*>(Cocoa_openHostUrl)},
    {const_cast<char*>("takeHostUrls"),           const_cast<char*>("()[Ljava/lang/String;"),
     reinterpret_cast<void*>(Cocoa_takeHostUrls)},
    {const_cast<char*>("openAppLinkWindow"),      const_cast<char*>("(Ljava/lang/String;Ljava/lang/String;)Z"),
     reinterpret_cast<void*>(Cocoa_openAppLinkWindow)},
    {const_cast<char*>("urlSchemeHandlers"),      const_cast<char*>("(Ljava/lang/String;)[Ljava/lang/String;"),
     reinterpret_cast<void*>(Cocoa_urlSchemeHandlers)},
    {const_cast<char*>("setUrlSchemeDefault"),    const_cast<char*>("(Ljava/lang/String;Ljava/lang/String;)I"),
     reinterpret_cast<void*>(Cocoa_setUrlSchemeDefault)},
    {const_cast<char*>("createWindow"),
     const_cast<char*>("(Ljava/lang/String;)J"),
     reinterpret_cast<void*>(Cocoa_createWindow)},
    {const_cast<char*>("createMetalSurfaceView"), const_cast<char*>("(II)J"),
     reinterpret_cast<void*>(Cocoa_createMetalSurfaceView)},
    {const_cast<char*>("createMetalSurfaceViewForGl"), const_cast<char*>("(II)J"),
     reinterpret_cast<void*>(Cocoa_createMetalSurfaceViewForGl)},
    {const_cast<char*>("beginWithheldTick"), const_cast<char*>("(Z)Z"),
     reinterpret_cast<void*>(Cocoa_beginWithheldTick)},
    {const_cast<char*>("getPlaceholderWindow"),   const_cast<char*>("()J"),
     reinterpret_cast<void*>(Cocoa_getPlaceholderWindow)},
    {const_cast<char*>("setContentView"),         const_cast<char*>("(JJ)V"),
     reinterpret_cast<void*>(Cocoa_setContentView)},
    {const_cast<char*>("showWindow"),             const_cast<char*>("(J)V"),
     reinterpret_cast<void*>(Cocoa_showWindow)},
    {const_cast<char*>("closeWindow"),            const_cast<char*>("(J)V"),
     reinterpret_cast<void*>(Cocoa_closeWindow)},
    {const_cast<char*>("setRenderer"),
     const_cast<char*>("(JLcom/macapk/loader/MetalSurfaceCallbacks;)V"),
     reinterpret_cast<void*>(Cocoa_setRenderer)},
    {const_cast<char*>("setWindowTitle"),
     const_cast<char*>("(JLjava/lang/String;)V"),
     reinterpret_cast<void*>(Cocoa_setWindowTitle)},
    {const_cast<char*>("registerTouchDispatcher"),
     const_cast<char*>("(Lcom/macapk/loader/TouchDispatcher;)V"),
     reinterpret_cast<void*>(Cocoa_registerTouchDispatcher)},
    {const_cast<char*>("exitProcess"),            const_cast<char*>("(ILjava/lang/String;)V"),
     reinterpret_cast<void*>(Cocoa_exitProcess)},
    {const_cast<char*>("debugTrackpad"),          const_cast<char*>("(I[F)Z"),
     reinterpret_cast<void*>(Cocoa_debugTrackpad)},
    {const_cast<char*>("debugQuitGesture"),       const_cast<char*>("(Ljava/lang/String;)V"),
     reinterpret_cast<void*>(Cocoa_debugQuitGesture)},
    {const_cast<char*>("hostRenderTick"),         const_cast<char*>("()Z"),
     reinterpret_cast<void*>(Cocoa_hostRenderTick)},
    {const_cast<char*>("hostHideWindowsForQuit"), const_cast<char*>("()Z"),
     reinterpret_cast<void*>(Cocoa_hostHideWindowsForQuit)},
    {const_cast<char*>("quitBackstopExit"),       const_cast<char*>("()V"),
     reinterpret_cast<void*>(Cocoa_quitBackstopExit)},
    {const_cast<char*>("moveTaskToBack"),         const_cast<char*>("()V"),
     reinterpret_cast<void*>(Cocoa_moveTaskToBack)},
    {const_cast<char*>("moveTaskToFront"),        const_cast<char*>("()V"),
     reinterpret_cast<void*>(Cocoa_moveTaskToFront)},
    {const_cast<char*>("releaseRenderer"),        const_cast<char*>("(J)V"),
     reinterpret_cast<void*>(Cocoa_releaseRenderer)},
    {const_cast<char*>("setSystemProperty"),
     const_cast<char*>("(Ljava/lang/String;Ljava/lang/String;)V"),
     reinterpret_cast<void*>(Cocoa_setSystemProperty)},
    {const_cast<char*>("setDataSubdir"),
     const_cast<char*>("(Ljava/lang/String;)V"),
     reinterpret_cast<void*>(Cocoa_setDataSubdir)},
    {const_cast<char*>("remapProcPath"),
     const_cast<char*>("(Ljava/lang/String;)Ljava/lang/String;"),
     reinterpret_cast<void*>(Cocoa_remapProcPath)},
    {const_cast<char*>("setNativeLibDir"),
     const_cast<char*>("(Ljava/lang/String;)V"),
     reinterpret_cast<void*>(Cocoa_setNativeLibDir)},
    {const_cast<char*>("setSystemFontsDir"),
     const_cast<char*>("(Ljava/lang/String;)V"),
     reinterpret_cast<void*>(Cocoa_setSystemFontsDir)},
    {const_cast<char*>("hostFontFile"),
     const_cast<char*>("(Ljava/lang/String;)Ljava/lang/String;"),
     reinterpret_cast<void*>(Cocoa_hostFontFile)},
    {const_cast<char*>("setSharedStorageRoot"),
     const_cast<char*>("(Ljava/lang/String;)V"),
     reinterpret_cast<void*>(Cocoa_setSharedStorageRoot)},
    {const_cast<char*>("showTranslationOverlay"),
     const_cast<char*>("(Ljava/lang/String;)V"),
     reinterpret_cast<void*>(Cocoa_showTranslationOverlay)},
    {const_cast<char*>("updateTranslationOverlay"),
     const_cast<char*>("(Ljava/lang/String;)V"),
     reinterpret_cast<void*>(Cocoa_updateTranslationOverlay)},
    {const_cast<char*>("hideTranslationOverlay"),
     const_cast<char*>("()V"),
     reinterpret_cast<void*>(Cocoa_hideTranslationOverlay)},
    {const_cast<char*>("presentBitmap"),
     const_cast<char*>("([III)V"),
     reinterpret_cast<void*>(Cocoa_presentBitmap)},
    {const_cast<char*>("presentOverlayBitmap"),
     const_cast<char*>("([III)V"),
     reinterpret_cast<void*>(Cocoa_presentOverlayBitmap)},
    {const_cast<char*>("clearOverlayBitmap"),
     const_cast<char*>("()V"),
     reinterpret_cast<void*>(Cocoa_clearOverlayBitmap)},
    // In LOCKSTEP with NativeCocoa.presentSurfaceBitmap (Bind() aborts the
    // process on a name the class does not declare).
    {const_cast<char*>("presentSurfaceBitmap"),
     const_cast<char*>("(I[III)V"),
     reinterpret_cast<void*>(Cocoa_presentSurfaceBitmap)},
    // Registered in LOCKSTEP with the `native` declaration in NativeCocoa.java:
    // Bind() aborts the process when RegisterNatives meets a name the class does
    // not declare, so this row and that declaration land together or not at all.
    {const_cast<char*>("presentFrame"),
     const_cast<char*>("([IIIIIIII)I"),
     reinterpret_cast<void*>(Cocoa_presentFrame)},
};

// ---------- com.macapk.loader.NativeGles (placeholder) ----------
//
// The GLES bindings are stubbed for now -- v2 will reuse runtime/src/
// angle_glue.cpp once the boot path reaches the GL stage. The Java
// side compiles against this binding so we can grow into it.

jboolean Gles_init(JNIEnv*, jclass, jlong layer, jint w, jint h) {
    return GlesInit(reinterpret_cast<void*>(layer), w, h) ? JNI_TRUE : JNI_FALSE;
}
void Gles_beginFrame(JNIEnv*, jclass) { GlesBeginFrame(); }
void Gles_endFrame(JNIEnv*, jclass)   { GlesEndFrame(); }

const JNINativeMethod kNativeGlesMethods[] = {
    {const_cast<char*>("init"),       const_cast<char*>("(JII)Z"),
     reinterpret_cast<void*>(Gles_init)},
    {const_cast<char*>("beginFrame"), const_cast<char*>("()V"),
     reinterpret_cast<void*>(Gles_beginFrame)},
    {const_cast<char*>("endFrame"),   const_cast<char*>("()V"),
     reinterpret_cast<void*>(Gles_endFrame)},
};

#include "gles20_bridges.inc"

// ---------- android.opengl.GLES30 / GLES31 / GLES31Ext / GLES32 ----------
//
// The GLES3.x Java binding surface. Unlike GLES20 (whose ANGLE symbols we
// link directly via <GLES2/gl2.h>), the GLES3.x entry points are NOT in the
// GLES2 header set ANGLE ships, so the generated bridges below resolve each
// entry point lazily by name through gl3_resolve() and call it through a
// typed function pointer. ANGLE's libGLESv2 DOES export the full GLES3.0/3.1/
// 3.2 entry-point set, so dlsym/eglGetProcAddress find them at runtime; when a
// symbol is absent (or ANGLE is not built into this tree) the resolved pointer
// is null and the bridge becomes a safe no-op (returning 0 / "") -- it can
// NEVER hard-fail the dylib link or RegisterNatives. The Java decls in
// runtime/java/android/opengl/GLES3*.java and the registration tables
// (kGl3GLES30Methods etc.) are emitted IN LOCKSTEP with these bridges by
// debug/gles_gen/gen.py, so no `native` decl is ever left unregistered.

#ifdef MACAPK_HAS_ANGLE
#include <EGL/egl.h>   // eglGetProcAddress, EGLDisplay (for handle marshaling)
#else
// When ANGLE headers are absent (dev trees without a built ANGLE), the GLES3
// bridges below still reference the GL scalar typedefs. Provide the minimal
// set so the generated gles3_bridges.inc compiles to safe no-ops (gl3_resolve
// returns null in that config). These match the Khronos khrplatform.h widths;
// they are only used for the fn-pointer casts, which never execute when ANGLE
// is missing. (Guarded so this never clashes with <GLES2/gl2.h> above.)
typedef int            GLint;
typedef unsigned int   GLuint;
typedef int            GLsizei;
typedef unsigned int   GLenum;
typedef unsigned int   GLbitfield;
typedef float          GLfloat;
typedef unsigned char  GLboolean;
typedef signed char    GLbyte;
typedef char           GLchar;
typedef long long      GLint64;
#endif

// Resolve a GLES entry point by name. Tries the process-global symbol table
// first (ANGLE's libGLESv2 exports core GLES3 functions directly, which is the
// most reliable path on macOS), then falls back to eglGetProcAddress (which
// ANGLE supports for both core + extension functions). Returns null when the
// symbol cannot be found, making the calling bridge a safe no-op.
static void* gl3_resolve(const char* name) {
    if (!name) return nullptr;
    if (void* p = dlsym(RTLD_DEFAULT, name)) return p;
#ifdef MACAPK_HAS_ANGLE
    if (void* p = reinterpret_cast<void*>(eglGetProcAddress(name))) return p;
#endif
    return nullptr;
}

// Marshal a java.nio.Buffer argument to a raw pointer for the GLES3 bridges.
// Direct buffers (the allocateDirect() path every engine uses for GL data)
// yield a zero-copy address advanced by position() scaled to the buffer's
// element size, reusing the SAME element-size + cache machinery the GLES20
// buffer path uses (EnsureBufferCache / InferElemSizeCached above) so typed
// buffers (IntBuffer/FloatBuffer with a non-zero position) resolve identically
// across the GLES2 and GLES3 surfaces. Heap-backed buffers (no direct address)
// return null: a GLES3 call then sees a null pointer, which is the safe "no
// data" outcome rather than reading freed JNI-array memory after this function
// returns (we cannot keep a Get*ArrayElements pin alive past the bridge call).
static void* gl3_buffer_ptr(JNIEnv* env, jobject buf) {
    if (!buf) return nullptr;
    void* base = env->GetDirectBufferAddress(buf);
    if (!base) return nullptr;
    EnsureBufferCache(env);
    jint p  = env->CallIntMethod(buf, gBufCache.mPosition);
    int  es = InferElemSizeCached(env, buf);
    if (p > 0) base = static_cast<char*>(base) + (jlong)p * es;
    return base;
}

// ---- Array+offset bounds validation for the generated GLES3 bridges and the
// hand-written EGL14 ones.
//
// Every `<type>[] arr, int offset` pair the GLES3/EGL bindings expose ends up
// as raw pointer arithmetic (`base + offset`) handed straight to the driver.
// With no check that is a HEAP out-of-bounds read AND write from a value the
// app fully controls. AOSP validates before it pins anything: `offset < 0` and
// `length - offset < needed` each raise IllegalArgumentException (see the
// generated bindings, e.g. frameworks/base/core/jni/android_opengl_GLES20.cpp
// :3576-3597, android16-release; the same preamble is emitted for every
// array+offset entry point in the family).
//
// We cannot derive AOSP's per-entry-point `needed` from a javap signature, so
// we enforce AOSP's floor: offset must be non-negative and must leave at least
// one element. That is exactly the check AOSP emits for the many single-valued
// entry points, and it is what makes the pointer we hand the driver land
// inside the allocation instead of anywhere in the heap.
//
// Ordering is load-bearing: this runs BEFORE GetPrimitiveArrayCritical. JNI
// forbids almost every JNI call (FindClass and ThrowNew included) inside a
// critical section, so validating late would be undefined behaviour of its own.
//
// A null array is NOT an error here. AOSP throws "params == null", but this
// family has always forwarded a null array as a null pointer, and every caller
// that relies on that is passing null deliberately (the Buffer overloads share
// these bridges). Changing that is a separate, app-visible decision.
static bool gl3_check_offset(JNIEnv* env, jarray arr, jint offset,
                             const char* what) {
    if (!arr) return true;
    if (offset >= 0 && offset < env->GetArrayLength(arr)) return true;
    jclass ex = env->FindClass("java/lang/IllegalArgumentException");
    if (ex) env->ThrowNew(ex, what);
    return false;
}

// The EGL config-array variant: eglChooseConfig/eglGetConfigs fill `config_size`
// entries starting at `offset`, so AOSP demands the array actually hold that
// many (android_opengl_EGL14.cpp:399-406, "length - configsOffset <
// config_size < needed"). SetObjectArrayElement is bounds-checked by the JVM,
// so the failure mode here is a pending ArrayIndexOutOfBoundsException and a
// half-filled array rather than memory corruption, but it is still wrong.
static bool gl3_check_config_span(JNIEnv* env, jobjectArray arr, jint offset,
                                  jint count, const char* what) {
    if (!arr) return true;
    if (offset >= 0 && count >= 0 && env->GetArrayLength(arr) - offset >= count)
        return true;
    jclass ex = env->FindClass("java/lang/IllegalArgumentException");
    if (ex) env->ThrowNew(ex, what);
    return false;
}

#include "gles3_bridges.inc"
// GLES1 fixed-function bridges (generated by debug/gles_gen/gen_gles1.py).
// Same conventions as gles3_bridges.inc above -- gl3_resolve / gl3_buffer_ptr /
// gl3_check_offset are all in scope here. INERT unless the context is ES1; see
// the header of the .inc.
#include "gles1_bridges.inc"

// ---------- helper ----------

void Bind(JNIEnv* env, const char* class_name,
          const JNINativeMethod* methods, jint count) {
    jclass cls = env->FindClass(class_name);
    if (!cls) {
        std::fprintf(stderr,
                     "[macapk] FindClass(%s) failed -- jar missing or"
                     " misnamed?\n", class_name);
        env->ExceptionDescribe();
        std::abort();
    }
    if (env->RegisterNatives(cls, methods, count) != JNI_OK) {
        std::fprintf(stderr,
                     "[macapk] RegisterNatives(%s) failed.\n",
                     class_name);
        env->ExceptionDescribe();
        std::abort();
    }
    env->DeleteLocalRef(cls);
}

// EGL14 / EGL15 / EGLExt bridges + RegisterEgl14Methods. Included here, after
// Bind() and gl3_resolve() are in scope, so the hand-written EGL handle
// marshaling can forward to ANGLE's libEGL and register against the
// android.opengl.EGL14/15/EGLExt classes.
#include "egl14_bridges.inc"

}  // namespace

// The native half of EGL14 windows (GitHub #16), exported for the bionic shim's
// eglSwapBuffers / eglMakeCurrent / eglQuerySurface; see egl14_bridges.inc.
extern "C" void* __macapk_egl14_window_resolve(void* dpy, void* surf) {
    return egl14_native_resolve(dpy, surf);
}
extern "C" int __macapk_egl14_window_swap(void* dpy, void* surf) {
    return egl14_native_swap(dpy, surf);
}

namespace {
// Java-callable: install the JNI tracer for the calling thread's
// JNIEnv. Bound to NativeLog so we don't need a new Java class.
// The actual installer is in libmacapk-bionic-shim.dylib; we
// dlsym it lazily so this runtime binary doesn't need to link
// against the shim statically.
void JNICALL nativeInstallJniTraceImpl(JNIEnv* env, jclass /*cls*/) {
    using InstallFn = void (*)(JNIEnv*);
    static InstallFn fn = reinterpret_cast<InstallFn>(
        dlsym(RTLD_DEFAULT, "__macapk_install_jni_trace_env"));
    if (fn && env) fn(env);
}
const JNINativeMethod kNativeInstallJniTrace[] = {
    {const_cast<char*>("nativeInstallJniTrace"),
     const_cast<char*>("()V"),
     reinterpret_cast<void*>(nativeInstallJniTraceImpl)},
};

// Elevate the CALLING thread (macapk-activity = the Android UI thread) to
// USER_INTERACTIVE QoS. Real Android runs the UI thread at an elevated priority
// so it preempts background work. Without it, when an app saturates the CPU with
// background Dispatchers.Default coroutine workers (Room, image decode, cronet,
// molecule viewmodels) AND recomposes every frame, this thread starves: the
// per-vsync drawInMTKView stops firing, the Compose frame clock (withFrameNanos)
// dies, and in-flight animations (e.g. a bottom sheet's open tween) freeze at
// their start value. Reproduced in debug/composetest MACAPK_CT_MODE=heavysheet
// (sheet stuck Hidden under CPU+recompose load) and the root of Reddit's auth
// bottom sheet staying at its hidden anchor. Java Thread.setPriority(MAX) is not
// honored by HotSpot on macOS for raising; the pthread QoS class is the real
// lever. Must be called ON the target thread (pthread_set_qos_class_SELF).
void JNICALL nativeBoostCurrentThreadQosImpl(JNIEnv*, jclass) {
    pthread_set_qos_class_self_np(QOS_CLASS_USER_INTERACTIVE, 0);
}
const JNINativeMethod kNativeQos[] = {
    {const_cast<char*>("nativeBoostCurrentThreadQos"),
     const_cast<char*>("()V"),
     reinterpret_cast<void*>(nativeBoostCurrentThreadQosImpl)},
};

// android.os.SystemClock.elapsedRealtimeNanos() backing (MACAPK_GUEST_CLOCK_NANOTIME).
// AOSP system/core libutils SystemClock.cpp:68 reads clock_gettime(CLOCK_BOOTTIME):
// the monotonic clock PLUS time suspended. System.nanoTime() here is HotSpot's
// (mach_absolute_time() * numer) / denom (os_bsd.cpp:765-767, see JavaVsyncNanoTime),
// and mach_continuous_time() is that same tick count plus the time the host slept
// (Libc gen/clock_gettime.c:115-116 serves it as CLOCK_MONOTONIC_RAW, which is what
// the bionic shim maps a guest CLOCK_BOOTTIME to). Converting it with the SAME formula
// keeps elapsedRealtimeNanos() >= System.nanoTime() and equal to the guest's
// clock_gettime(CLOCK_BOOTTIME), the two identities AOSP code relies on.
jlong JNICALL nativeBootTimeNanosImpl(JNIEnv*, jclass) {
    static const mach_timebase_info_data_t tb = [] {
        mach_timebase_info_data_t t{};
        mach_timebase_info(&t);
        return t;
    }();
    return static_cast<jlong>((mach_continuous_time() * tb.numer) / tb.denom);
}
const JNINativeMethod kNativeBootTime[] = {
    {const_cast<char*>("nativeBootTimeNanos"),
     const_cast<char*>("()J"),
     reinterpret_cast<void*>(nativeBootTimeNanosImpl)},
};

// ---------- pairip integrity-gate emulation (MACAPK_PAIRIP_GATE_EMU, default OFF) ----------
// Play-built apps wrap a startup program in the pairip integrity VM
// (com.pairip.VMRunner.executeVM -> libpairipcore ExecuteProgram). On Mac.apk that VM
// FALSELY detonates (libpairipcore +0x440c4) because our HotSpot-on-macOS process STRUCTURE
// differs from a zygote-forked ART-on-Linux process -- NOT because anything is tampered
// (Mac.apk does not patch .text, forge signatures, or attach a debugger). RE confirmed the
// integrity is a SEPARABLE gate: the engine (libminecraftpe) is cleartext, decrypted by
// nothing; the entire coupling is the one symbol ExecuteProgram whose return is discarded, so
// passing the gate = the cleartext engine runs. We therefore re-register executeVM to this
// wrapper and reimplement the REAL checks ourselves, so a genuine debugger/tamper is still
// caught (feed poison) and only the platform false-positive is passed. This CORRECTS a
// false-positive while PRESERVING detection -- the same in-layer-ptrace-emu precedent CLAUDE.md
// rules IN scope. It is NOT a .text patch: pairip's bytes stay pristine, so its own self-hash
// still passes. Removal condition: retire if/when Mac.apk presents a process surface faithful
// enough that the VM passes on its own. docs/pairip-frontier-301578-refutations_2026-09-17.md.
static bool macapk_gate_self_traced() {
    // FAITHFUL debugger check: a real ptrace/lldb attach sets P_TRACED on our own proc.
    struct kinfo_proc kp; std::memset(&kp, 0, sizeof(kp)); size_t len = sizeof(kp);
    int mib[4] = {CTL_KERN, KERN_PROC, KERN_PROC_PID, getpid()};
    if (::sysctl(mib, 4, &kp, &len, nullptr, 0) != 0 || len == 0) return false;
    return (kp.kp_proc.p_flag & P_TRACED) != 0;
}
// DIAGNOSTIC (MACAPK_PAIRIP_INJECT_STRINGS=<path>, default OFF): populate the app's pairip
// string pools (com.googleplay.licensing.PP.NDWvIdB + the other obfuscated pool classes) from a
// captured device dump, at the exact startup moment the device's VM would. This is NOT the fix --
// it is the diagnostic the fleet verdict calls for (docs/pairip-faithful-execution-fleet-verdict_
// 2026-09-17.md): it CONFIRMS the hypothesis "populating the pools unblocks render" end-to-end and
// reveals any further dominoes, before we build the general string-decode replication. The values
// are per-image captured constants (SEPARABLE from the integrity hash, Lane 3 proof), so injecting
// them faithfully mirrors what the VM would emit -- but because they are captured, not decoded by
// us, this stays behind a default-OFF flag. REMOVAL CONDITION: a minimal interpreter of the pairip
// string-decode + SetStaticObjectField handlers (Lane 1) that computes these values generally.
// File format: repeated <u16 clen><class internal-name><u16 flen><field><u32 vlen><utf8 value>.
static void macapk_pairip_inject_pool(JNIEnv* env, const char* path) {
    FILE* f = std::fopen(path, "rb");
    if (!f) { std::fprintf(stderr, "[pairip-inject] cannot open %s\n", path); return; }
    std::fseek(f, 0, SEEK_END); long sz = std::ftell(f); std::fseek(f, 0, SEEK_SET);
    if (sz <= 0) { std::fclose(f); return; }
    std::vector<unsigned char> buf((size_t)sz);
    if (std::fread(buf.data(), 1, (size_t)sz, f) != (size_t)sz) { std::fclose(f); return; }
    std::fclose(f);
    size_t p = 0; int nset = 0, nmiss = 0; jclass curCls = nullptr; std::string curName;
    auto rd16 = [&](size_t& o)->unsigned { unsigned v = buf[o] | (buf[o+1] << 8); o += 2; return v; };
    auto rd32 = [&](size_t& o)->unsigned { unsigned v = buf[o] | (buf[o+1]<<8) | (buf[o+2]<<16) | ((unsigned)buf[o+3]<<24); o += 4; return v; };
    while (p + 2 <= buf.size()) {
        unsigned clen = rd16(p); if (p + clen > buf.size()) break;
        std::string cname((char*)&buf[p], clen); p += clen;
        unsigned flen = rd16(p); if (p + flen > buf.size()) break;
        std::string fname((char*)&buf[p], flen); p += flen;
        unsigned vlen = rd32(p); if (p + vlen > buf.size()) break;
        std::string value((char*)&buf[p], vlen); p += vlen;
        // FindClass per class (cache the current one; the file groups by class)
        if (cname != curName) {
            if (curCls) env->DeleteLocalRef(curCls);
            curName = cname; curCls = env->FindClass(cname.c_str());
            if (env->ExceptionCheck()) env->ExceptionClear();
        }
        if (!curCls) { nmiss++; continue; }
        jfieldID fid = env->GetStaticFieldID(curCls, fname.c_str(), "Ljava/lang/String;");
        if (env->ExceptionCheck()) env->ExceptionClear();
        if (!fid) { nmiss++; continue; }
        jstring js = env->NewStringUTF(value.c_str());
        if (env->ExceptionCheck()) { env->ExceptionClear(); if (js) env->DeleteLocalRef(js); nmiss++; continue; }
        env->SetStaticObjectField(curCls, fid, js);
        if (env->ExceptionCheck()) env->ExceptionClear(); else nset++;
        env->DeleteLocalRef(js);
    }
    if (curCls) env->DeleteLocalRef(curCls);
    std::fprintf(stderr, "[pairip-inject] populated %d pooled string fields (%d missed) from %s\n",
                 nset, nmiss, path);
    std::fflush(stderr);
}
jobject JNICALL macapk_pairip_gate(JNIEnv* env, jclass cls,
                                   jbyteArray program, jobjectArray args) {
    // DIAGNOSTIC (MACAPK_PAIRIP_GATE_TRACE, default-OFF): one line per executeVM call --
    // its arg count (0=integrity/startup, >=1=resolver) and whether the ref env resolved.
    // This is how we tell "the startup program never reached the gate" from "the gate ran
    // but the ref env was UNSET" -- the two ways a pairip app boots with a null string pool.
    if (std::getenv("MACAPK_PAIRIP_GATE_TRACE")) {
        static int gate_n = 0;
        std::fprintf(stderr, "[pairip-gate][trace] executeVM call #%d nargs=%d INJECT_STRINGS=%s IMPORT_TABLE=%s\n",
                     ++gate_n, (int)(args ? env->GetArrayLength(args) : -1),
                     std::getenv("MACAPK_PAIRIP_INJECT_STRINGS") ? "set" : "UNSET",
                     std::getenv("MACAPK_PAIRIP_IMPORT_TABLE")   ? "set" : "UNSET");
        std::fflush(stderr);
    }
    // DIAGNOSTIC (MACAPK_PAIRIP_DUMP_PROGRAM=<path-prefix>, default-OFF): dump the DECRYPTED
    // pairip VM bytecode our gate receives (VmDecryptor already ran in VMRunner.invoke), so
    // the clVm import-resolver program can be analysed OFFLINE for a general (hole_offset ->
    // import_name) table -- Path B, resolving the reloc-less GOT holes in our loader without
    // running the VM. Filename carries nargs (0=integrity, >=1=resolver) + length to tell
    // programs apart. Read-only; no app-name branch. Remove once the table decoder lands.
    if (const char* dp = std::getenv("MACAPK_PAIRIP_DUMP_PROGRAM")) {
        jsize plen = program ? env->GetArrayLength(program) : 0;
        jsize na   = args ? env->GetArrayLength(args) : 0;
        if (plen > 0) {
            if (jbyte* pb = env->GetByteArrayElements(program, nullptr)) {
                char path[600];
                std::snprintf(path, sizeof path, "%s.nargs%d.%d.bin", dp, (int)na, (int)plen);
                if (FILE* f = std::fopen(path, "wb")) {
                    std::fwrite(pb, 1, (size_t)plen, f); std::fclose(f);
                    std::fprintf(stderr, "[pairip-gate] dumped %d-byte decrypted program "
                                 "(nargs=%d) -> %s\n", (int)plen, (int)na, path);
                }
                env->ReleaseByteArrayElements(program, pb, JNI_ABORT);
            }
        }
    }
    // Detection preserved: a genuine debugger -> do NOT pass (throw = the app aborts, exactly
    // as pairip's own detonation would). We deliberately do NOT catch this false; a real
    // attacker still fails. (Signature tamper is caught separately by the app's own
    // com.pairip.SignatureCheck.verifyIntegrity, which runs regardless of this gate.)
    if (macapk_gate_self_traced()) {
        jclass rte = env->FindClass("java/lang/SecurityException");
        if (rte) env->ThrowNew(rte, "pairip: debugger detected");
        return nullptr;
    }
    // pairip runs TWO kinds of program through executeVM (RE 2026-09-17,
    // docs/pairip-gate-intercept-and-maesdk-reloc_2026-09-17.md):
    //  - INTEGRITY programs (the Java startup gate, invoked with an EMPTY Object[0]):
    //    they self-hash the process STRUCTURE and structurally FALSE-detonate on our
    //    HotSpot-on-macOS process. We bypass them (return null; VMRunner discards it).
    //  - RESOLVER programs (a native DT_INIT import-linker, invoked from libpairipcore's
    //    ExecuteProgram with Object[1]{Long(image_base)}): they resolve a pairip-protected
    //    lib's STRIPPED imports (libmaesdk / libPlayFabMultiplayer's ~189 / ~135 reloc-less
    //    .got.plt slots) via dlsym + image_base-relative GOT writes. These are BENIGN and
    //    MUST run, or the lib's first constructor branches through an un-based .plt[0] and
    //    SIGSEGVs. Blocking them (the old blanket null) was the maesdk-init crash.
    // Distinguisher = the call SHAPE (a non-empty args array carrying image_base), a
    // pairip-framework property (not app data). Run the resolver on the REAL executeVM
    // captured at NativeMethodBind (see bionic_shim.cpp __macapk_pairip_real_executeVM).
    jsize nargs = args ? env->GetArrayLength(args) : 0;
    if (nargs >= 1) {
        using Fn = jobject (*)(JNIEnv*, jclass, jbyteArray, jobjectArray);
        static Fn real = nullptr;
        if (!real) {
            if (auto getter = reinterpret_cast<void* (*)()>(
                    dlsym(RTLD_DEFAULT, "__macapk_pairip_real_executeVM"))) {
                real = reinterpret_cast<Fn>(getter());
            }
        }
        if (real) {
            // SKIP A RESOLVER THAT HAS NOTHING LEFT TO RESOLVE (v1.0.1635).
            //
            // The loader fills a protected lib's stripped-import holes from the device-
            // harvested table BEFORE its DT_INIT runs (fill_pairip_import_holes), and an
            // image that is not import-protected (libminecraftpe) has no holes at all. For
            // such a target the resolver program cannot contribute anything: measured with
            // MACAPK_PAIRIP_RESOLVER_DIFF=1 on Minecraft 1.26.45.1, each of its three
            // resolver runs (libmaesdk, libPlayFabMultiplayer, libminecraftpe) changed ZERO
            // words in its target image's writable segments. What it DID do was detonate:
            // its self-verify fails on our image_base and the poisoned VM makes wild stores
            // (caught faults include data WRITES at libpairipcore+0x2a020 / +0x29ff0) until
            // one of them faults. The guard catches THAT fault, but the stores before it
            // land wherever they land. When they land on HotSpot's own structures the JVM
            // dies later (C2 Arena::destruct_contents / SymbolTable / executeVM entry,
            // measured 2026-09-22): 5/16 Minecraft launches through the installed stub once
            // faster library loading moved the resolvers into the JVM's busiest seconds.
            //
            // Returning null here is exactly what the caller already received after every
            // caught detonation, so the observable result is unchanged and only the wild
            // stores go away. A target that still HAS unfilled holes (no table, or a table
            // that missed some) keeps running the real resolver as before. Detection is
            // untouched: the debugger check above runs first.
            // MACAPK_PAIRIP_RUN_REDUNDANT_RESOLVERS=1 restores the old always-run (A/B).
            if (!std::getenv("MACAPK_PAIRIP_RUN_REDUNDANT_RESOLVERS")) {
                uint64_t target = 0;
                if (jobject a0 = env->GetObjectArrayElement(args, 0)) {
                    if (jclass lc = env->FindClass("java/lang/Long")) {
                        if (env->IsInstanceOf(a0, lc)) {
                            jmethodID lv = env->GetMethodID(lc, "longValue", "()J");
                            if (lv) target = (uint64_t)env->CallLongMethod(a0, lv);
                        }
                        env->DeleteLocalRef(lc);
                    }
                    env->DeleteLocalRef(a0);
                }
                if (env->ExceptionCheck()) env->ExceptionClear();
                using Needed = int (*)(uint64_t);
                static Needed needed = reinterpret_cast<Needed>(
                    dlsym(RTLD_DEFAULT, "__macapk_pairip_image_resolver_needed"));
                if (target && needed && needed(target) == 0) {
                    std::fprintf(stderr, "[pairip-gate] resolver for image_base=%#llx skipped: "
                                 "no unfilled import holes (table-complete or not import-"
                                 "protected), so it could only detonate\n",
                                 (unsigned long long)target);
                    return nullptr;
                }
            }
            // Run the resolver under the shim's detonation guard: the pairip VM's
            // CFG-flattened FNV self-verify poisons AFTER the resolver's GOT writes land
            // (it diverges because our image_base is a macOS-high, non-device address),
            // so the guard catches the SIGBUS and returns null as if the resolver
            // completed, keeping the imports it resolved.
            using Run = void* (*)(void*, void*, void*, void*, void*);
            static Run runner = reinterpret_cast<Run>(
                dlsym(RTLD_DEFAULT, "__macapk_pairip_run_resolver"));
            // DIAGNOSTIC (MACAPK_PAIRIP_RESOLVER_DIFF=1, default OFF): what does a resolver
            // program actually change inside its target image? Snapshot the image's
            // writable segments, run it, and diff them 8 bytes at a time.
            uint64_t diff_base = 0;
            std::vector<std::pair<uint64_t, std::vector<uint64_t>>> diff_snap;
            if (std::getenv("MACAPK_PAIRIP_RESOLVER_DIFF")) {
                jobject a0 = env->GetObjectArrayElement(args, 0);
                jclass lc = a0 ? env->FindClass("java/lang/Long") : nullptr;
                jmethodID lv = lc ? env->GetMethodID(lc, "longValue", "()J") : nullptr;
                if (lv) diff_base = (uint64_t)env->CallLongMethod(a0, lv);
                if (env->ExceptionCheck()) env->ExceptionClear();
                using Ranges = int (*)(uint64_t, uint64_t*, uint64_t*, int);
                using Complete = int (*)(uint64_t);
                auto rr = reinterpret_cast<Ranges>(dlsym(RTLD_DEFAULT, "__macapk_arm64_image_rw_ranges"));
                auto cp = reinterpret_cast<Complete>(dlsym(RTLD_DEFAULT, "__macapk_pairip_image_imports_complete"));
                uint64_t lo[8], hi[8];
                int n = rr ? rr(diff_base, lo, hi, 8) : -1;
                for (int i = 0; i < n; ++i) {
                    std::vector<uint64_t> v((size_t)((hi[i] - lo[i]) / 8));
                    std::memcpy(v.data(), reinterpret_cast<const void*>(lo[i]), v.size() * 8);
                    diff_snap.emplace_back(lo[i], std::move(v));
                }
                std::fprintf(stderr, "[pairip-gate] resolver diff: image_base=%#llx rw_segs=%d "
                             "imports_complete=%d\n", (unsigned long long)diff_base, n,
                             cp ? cp(diff_base) : -2);
            }
            jobject rv = runner
                ? reinterpret_cast<jobject>(runner(reinterpret_cast<void*>(real), env, cls, program, args))
                : real(env, cls, program, args);   // fallback: guard unavailable
            if (!diff_snap.empty()) {
                long changed = 0, shown = 0;
                for (const auto& sg : diff_snap) {
                    const uint64_t* now = reinterpret_cast<const uint64_t*>(sg.first);
                    for (size_t i = 0; i < sg.second.size(); ++i) {
                        if (now[i] == sg.second[i]) continue;
                        ++changed;
                        if (shown++ < 12)
                            std::fprintf(stderr, "[pairip-gate]   +%#llx: %#llx -> %#llx\n",
                                         (unsigned long long)(sg.first + i * 8 - diff_base),
                                         (unsigned long long)sg.second[i], (unsigned long long)now[i]);
                    }
                }
                std::fprintf(stderr, "[pairip-gate] resolver diff: %ld 8-byte word(s) changed in the "
                             "image's writable segments\n", changed);
            }
            return rv;
        }
        std::fprintf(stderr, "[pairip-gate] resolver call but real executeVM not captured; "
                             "imports left unresolved\n");
        return nullptr;
    }
    // Integrity program on a clean, untampered process: bypass the structural false-positive.
    //
    // EXPERIMENT (MACAPK_PAIRIP_RUN_STARTUP, default OFF): instead of bypassing, RUN the
    // startup/integrity program under the detonation guard. pairip's StartupLauncher.launch()
    // (called from the app's Application <clinit>) runs THIS program via VMRunner.invoke, and it
    // is the suspected populator of the obfuscated string pool com.googleplay.licensing.PP.NDWvIdB
    // (40 static String fields, no <clinit> -- set externally). Bypassing it leaves that pool
    // null, which cascades to MCPE onCreate aborting at Braze (new StringBuilder(null)) -> the
    // black screen. If the program decrypts the strings BEFORE its FNV self-hash detonates, the
    // guard catches the SIGBUS/SIGSEGV and we keep the decrypted strings; if it verifies before it
    // decrypts (as the import resolver does), we get nothing and this is a no-op vs the bypass.
    // Diagnostic lever only -- the DEFAULT stays the clean bypass that boots MC past pairip.
    // MEASURED 2026-09-17 (v1.0.1552): REFUTED. With the flag ON the startup program DETONATES
    // (same poison-length FNV, guard catches it) but NDWvIdB stays null -- MCPE onCreate still
    // NPEs at Braze. So the startup/integrity program verifies before it decrypts (or does not
    // populate the pool at all); the string pool is written by a different, still-unidentified
    // mechanism. Kept as a labelled dead-end so it is not re-tried.
    if (std::getenv("MACAPK_PAIRIP_RUN_STARTUP")) {
        using Fn = jobject (*)(JNIEnv*, jclass, jbyteArray, jobjectArray);
        static Fn real2 = nullptr;
        if (!real2) {
            if (auto getter = reinterpret_cast<void* (*)()>(
                    dlsym(RTLD_DEFAULT, "__macapk_pairip_real_executeVM"))) {
                real2 = reinterpret_cast<Fn>(getter());
            }
        }
        if (real2) {
            std::fprintf(stderr, "[pairip-gate] MACAPK_PAIRIP_RUN_STARTUP: running the startup/integrity "
                                 "program under the detonation guard (to decrypt the string pool)\n");
            std::fflush(stderr);
            using Run = void* (*)(void*, void*, void*, void*, void*);
            static Run runner2 = reinterpret_cast<Run>(
                dlsym(RTLD_DEFAULT, "__macapk_pairip_run_resolver"));
            if (runner2) {
                return reinterpret_cast<jobject>(runner2(
                    reinterpret_cast<void*>(real2), env, cls, program, args));
            }
            return real2(env, cls, program, args);
        }
    }
    // DIAGNOSTIC: on the FIRST integrity (startup) call, optionally populate the app's pairip
    // string pools from a captured device dump (see macapk_pairip_inject_pool above). Runs once,
    // at the same point the device's VM would, so the pool is set before any library <clinit>
    // reads it. Default OFF; removal condition = the general string-decode replication.
    if (const char* ip = std::getenv("MACAPK_PAIRIP_INJECT_STRINGS")) {
        static bool injected = false;
        if (!injected) { injected = true; macapk_pairip_inject_pool(env, ip); }
    }
    return nullptr;
}
// Java-callable (com.macapk.loader.NativeLog.nativeRegisterPairipGate): rebind the app's
// com.pairip.VMRunner.executeVM native to macapk_pairip_gate. Called from Boot after the app's
// Application <clinit> has loaded libpairipcore (executeVM registered) and before
// attachBaseContext runs the startup program. Last-writer-wins RegisterNatives; no .text touched.
jboolean JNICALL nativeRegisterPairipGateImpl(JNIEnv* env, jclass /*self*/, jclass vmRunner) {
    if (!vmRunner) return JNI_FALSE;
    JNINativeMethod m = {
        const_cast<char*>("executeVM"),
        const_cast<char*>("([B[Ljava/lang/Object;)Ljava/lang/Object;"),
        reinterpret_cast<void*>(macapk_pairip_gate)};
    jint rc = env->RegisterNatives(vmRunner, &m, 1);
    if (env->ExceptionCheck()) env->ExceptionClear();
    if (rc != JNI_OK) {
        std::fprintf(stderr, "[pairip-gate] RegisterNatives(executeVM) failed rc=%d\n", (int)rc);
        return JNI_FALSE;
    }
    std::fprintf(stderr, "[pairip-gate] executeVM re-bound to the in-layer integrity gate "
                         "(detection preserved; platform false-positive corrected)\n");
    return JNI_TRUE;
}
const JNINativeMethod kNativePairipGate[] = {
    {const_cast<char*>("nativeRegisterPairipGate"),
     const_cast<char*>("(Ljava/lang/Class;)Z"),
     reinterpret_cast<void*>(nativeRegisterPairipGateImpl)},
};

// com.macapk.loader.NativeLooper -- bridges a per-thread Java Looper to its
// native NDK ALooper (MacApkLooper) so Looper.loop() services native fds
// (Unity's vsync) + a wake self-pipe ON THE OWNER THREAD. Forwarders dlsym the
// shim symbols (loaded by main.cpp before any Java runs).
jlong JNICALL NativeLooper_forCurrentThread(JNIEnv*, jclass) {
    using Fn = long (*)();
    static Fn fn = reinterpret_cast<Fn>(
        dlsym(RTLD_DEFAULT, "__macapk_looper_for_current_thread"));
    return fn ? static_cast<jlong>(fn()) : 0;
}
void JNICALL NativeLooper_pollOnce(JNIEnv*, jclass, jlong nl, jint timeoutMs) {
    using Fn = void (*)(void*, int);
    static Fn fn = reinterpret_cast<Fn>(
        dlsym(RTLD_DEFAULT, "__macapk_looper_native_poll"));
    if (fn) fn(reinterpret_cast<void*>(nl), static_cast<int>(timeoutMs));
}
void JNICALL NativeLooper_wake(JNIEnv*, jclass, jlong nl) {
    using Fn = void (*)(void*);
    static Fn fn = reinterpret_cast<Fn>(
        dlsym(RTLD_DEFAULT, "__macapk_looper_wake"));
    if (fn) fn(reinterpret_cast<void*>(nl));
}
// Non-blocking service of this looper's sensor / input-queue fds, for a busy
// Java loop pass (android.os.Looper, MACAPK_POLL_FDS_WHEN_BUSY).
void JNICALL NativeLooper_pollPlainFds(JNIEnv*, jclass, jlong nl) {
    using Fn = void (*)(void*);
    static Fn fn = reinterpret_cast<Fn>(
        dlsym(RTLD_DEFAULT, "__macapk_looper_poll_plain_fds"));
    if (fn) fn(reinterpret_cast<void*>(nl));
}
// If this thread's native looper owns Unity's display/gfx fds, run the poll
// loop here (blocks); returns 0 immediately for a thread with no native fds.
jint JNICALL NativeLooper_runMainPoll(JNIEnv*, jclass, jlong nl) {
    using Fn = int (*)(void*);
    static Fn fn = reinterpret_cast<Fn>(
        dlsym(RTLD_DEFAULT, "__macapk_looper_run_main_poll"));
    return fn ? static_cast<jint>(fn(reinterpret_cast<void*>(nl))) : 0;
}
const JNINativeMethod kNativeLooperMethods[] = {
    {const_cast<char*>("forCurrentThread"), const_cast<char*>("()J"),
     reinterpret_cast<void*>(NativeLooper_forCurrentThread)},
    {const_cast<char*>("pollOnce"), const_cast<char*>("(JI)V"),
     reinterpret_cast<void*>(NativeLooper_pollOnce)},
    {const_cast<char*>("wake"), const_cast<char*>("(J)V"),
     reinterpret_cast<void*>(NativeLooper_wake)},
    {const_cast<char*>("runMainPoll"), const_cast<char*>("(J)I"),
     reinterpret_cast<void*>(NativeLooper_runMainPoll)},
    {const_cast<char*>("pollPlainFds"), const_cast<char*>("(J)V"),
     reinterpret_cast<void*>(NativeLooper_pollPlainFds)},
};

// ==== BEGIN macapk-java-vsync ====
// MACAPK_CHOREO_VSYNC_THREADS (2026-09-27): the display vsync for per-thread
// Java Choreographers.
//
// What real Android does. A Choreographer's FrameDisplayEventReceiver
// registers a vsync fd on its thread's native Looper; scheduleVsync() asks
// SurfaceFlinger for ONE vsync event; the event arrives on that fd, the
// thread's nativePollOnce dispatches onVsync ON THAT THREAD, and onVsync posts
// doFrame as an async message (frameworks/base core/java/android/view/
// Choreographer.java, FrameDisplayEventReceiver; DisplayEventReceiver). The
// timestamp is in the System.nanoTime() timebase (FrameCallback.doFrame
// javadoc).
//
// What we did before. A per-thread Choreographer ran each frame callback on a
// Handler.postDelayed(refresh interval) timer: no vsync, its own clock. The
// NDK AChoreographer already had a real vsync (bionic_shim.cpp
// __macapk_arm64_choreographer_fire, driven by the cocoa_bridge.mm display
// tick); the Java one did not.
//
// This block is that vsync for Java: scheduleVsync marks ONE request on the
// thread's native looper (the MacApkLooper its Java Looper polls), the display
// tick (JavaVsyncFire, the same tick that fires the NDK AChoreographer) turns
// every pending request into a delivered vsync stamped in the System.nanoTime()
// clock and wakes the looper, and the owning thread's Looper.loop() takes it
// (takeVsync) and hands it to the Choreographer's receiver (AOSP
// FrameDisplayEventReceiver.onVsync). Each scheduleVsync gets exactly ONE vsync:
// a tick delivers only to a pending request, so a second tick before the take
// neither re-delivers nor moves the stamp -- the Choreographer asks again from
// doFrame, as AOSP's does, when it has more frames to run.
//
// debug/geh_unity-swappy-frame-pacing-bridge_gate.sh compiles THIS BLOCK
// VERBATIM (sed between the BEGIN/END markers) into its test dylib, so keep it
// self-contained: <jni.h>, <dlfcn.h>, <mach/mach_time.h> and the C++ standard
// library only, nothing else from this file.

struct JavaVsyncSlot {
    bool     requested     = false;   // scheduleVsync called, no tick since
    bool     delivered     = false;   // a tick delivered it, not taken yet
    int64_t  frame_time_ns = 0;       // that tick's System.nanoTime() stamp
    uint64_t tick          = 0;       // that tick's sequence number (JavaVsyncFireAt)
};
// Leaked on purpose, like bionic_shim.cpp's g_vsync_feed_mu: the display tick
// can still be running when ::exit() runs static destructors, and locking a
// destroyed mutex aborts the process on its way out.
std::mutex& JavaVsyncMu() {
    static std::mutex* m = new std::mutex();
    return *m;
}
std::unordered_map<void*, JavaVsyncSlot>& JavaVsyncSlots() {
    static auto* s = new std::unordered_map<void*, JavaVsyncSlot>();
    return *s;
}
// How many slots are `requested`. Lets the per-tick fire skip the lock when no
// Java Choreographer is waiting (every app that has none).
std::atomic<int> g_java_vsync_requested{0};
std::atomic<int> g_java_vsync_delivered_total{0};
// ONE shim notification per display tick, not one per taker (both guarded by
// JavaVsyncMu). Several per-thread Choreographers can take the SAME tick
// (Unity's "UnityChoreographer" beside an ad SDK's ExoPlayer
// "FrameReleaseChoreographer" or an adquality frame monitor). The first draft
// of this change stamped FramePacer's render start from each notification, and a
// second one on the same tick marked every frame pre-released (review, issue 3).
// The stamp has since moved to the guest's own wait return, but "one vsync" must
// still mean one display tick to the shim, so the tick carries a sequence number
// and only the FIRST take of a tick newer than the last one noted notifies. A
// take of an OLDER tick (a slow looper taking a vsync after another looper
// already took a newer one) does not notify either.
uint64_t g_java_vsync_tick_seq   = 0;   // last tick sequence handed out
uint64_t g_java_vsync_noted_tick = 0;   // last tick the shim was told about

// System.nanoTime() on this JVM, computed the way HotSpot computes it on macOS:
// openjdk jdk21u src/hotspot/os/bsd/os_bsd.cpp os::javaTimeNanos() is
//   (mach_absolute_time() * _timebase_info.numer) / _timebase_info.denom
// (plus a monotonic clamp that never changes a fresh reading). Using the same
// source and formula is what puts a vsync stamp in the timebase AOSP promises.
int64_t JavaVsyncNanoTime() {
    static const mach_timebase_info_data_t tb = [] {
        mach_timebase_info_data_t t{};
        mach_timebase_info(&t);
        return t;
    }();
    return static_cast<int64_t>((mach_absolute_time() * tb.numer) / tb.denom);
}

// Wake the owning thread's poll: the same wake Handler.enqueue uses
// (bionic_shim.cpp __macapk_looper_wake writes the looper's wake pipe).
void JavaVsyncWake(void* looper) {
    using Fn = void (*)(void*);
    static Fn fn = reinterpret_cast<Fn>(dlsym(RTLD_DEFAULT, "__macapk_looper_wake"));
    if (fn && looper) fn(looper);
}

// Tell the shim's FramePacer that a vsync-paced Java Choreographer just took a
// frame (its engagement signal and the MUTEX_FAIR latch; see bionic_shim.cpp
// __macapk_arm64_choreo_vsync_delivered). Called for the FIRST take of each
// display tick only (g_java_vsync_noted_tick). Absent in an older shim: no-op.
void JavaVsyncNoteDelivered() {
    using Fn = void (*)();
    static Fn fn = reinterpret_cast<Fn>(
        dlsym(RTLD_DEFAULT, "__macapk_arm64_choreo_vsync_delivered"));
    if (fn) fn();
}
// Mark the CALLING thread as a vsync client, on EVERY take: a condvar its vsync
// callback then signals is what releases a vsync-paced render thread, and the
// shim stamps FramePacer's render start at that wait's return (bionic_shim.cpp,
// the VSYNC-RELEASE condvar block). Absent in an older shim: no-op.
void JavaVsyncNoteThread() {
    using Fn = void (*)();
    static Fn fn = reinterpret_cast<Fn>(
        dlsym(RTLD_DEFAULT, "__macapk_arm64_choreo_vsync_thread_note"));
    if (fn) fn();
}

void JNICALL NativeLooper_scheduleVsync(JNIEnv*, jclass, jlong nl) {
    void* L = reinterpret_cast<void*>(nl);
    if (!L) return;
    std::lock_guard<std::mutex> lk(JavaVsyncMu());
    JavaVsyncSlot& s = JavaVsyncSlots()[L];
    if (!s.requested) {
        s.requested = true;
        g_java_vsync_requested.fetch_add(1, std::memory_order_release);
    }
}

// -1 = nothing delivered (and always -1 for a null looper, which is how
// android.os.Looper probes whether these natives are bound at all).
jlong JNICALL NativeLooper_takeVsync(JNIEnv*, jclass, jlong nl) {
    void* L = reinterpret_cast<void*>(nl);
    if (!L) return -1;
    int64_t ft = 0;
    bool note = false;
    {
        std::lock_guard<std::mutex> lk(JavaVsyncMu());
        auto& slots = JavaVsyncSlots();
        auto it = slots.find(L);
        if (it == slots.end() || !it->second.delivered) return -1;
        it->second.delivered = false;
        ft = it->second.frame_time_ns;
        if (it->second.tick > g_java_vsync_noted_tick) {   // once per display tick
            g_java_vsync_noted_tick = it->second.tick;
            note = true;
        }
    }
    JavaVsyncNoteThread();
    if (note) JavaVsyncNoteDelivered();
    return static_cast<jlong>(ft);
}

// One display vsync at frame_time_ns (System.nanoTime() base): deliver it to
// every looper with a pending request and wake them. Wakes run outside the
// lock. Returns the number of loopers woken.
int JavaVsyncFireAt(int64_t frame_time_ns) {
    if (g_java_vsync_requested.load(std::memory_order_acquire) == 0) return 0;
    std::vector<void*> woken;
    {
        std::lock_guard<std::mutex> lk(JavaVsyncMu());
        const uint64_t tick = ++g_java_vsync_tick_seq;
        for (auto& kv : JavaVsyncSlots()) {
            JavaVsyncSlot& s = kv.second;
            if (!s.requested) continue;
            s.requested     = false;
            s.delivered     = true;
            s.frame_time_ns = frame_time_ns;
            s.tick          = tick;
            g_java_vsync_requested.fetch_sub(1, std::memory_order_acq_rel);
            woken.push_back(kv.first);
        }
    }
    for (void* L : woken) JavaVsyncWake(L);
    const int n = static_cast<int>(woken.size());
    if (n > 0 && g_java_vsync_delivered_total.fetch_add(n) == 0) {
        std::fprintf(stderr,
            "[choreo-java] first display vsync delivered to %d per-thread Java "
            "Choreographer looper(s) (MACAPK_CHOREO_VSYNC_THREADS)\n", n);
    }
    return n;
}
// ==== END macapk-java-vsync ====

const JNINativeMethod kNativeLooperVsyncMethods[] = {
    {const_cast<char*>("scheduleVsync"), const_cast<char*>("(J)V"),
     reinterpret_cast<void*>(NativeLooper_scheduleVsync)},
    {const_cast<char*>("takeVsync"), const_cast<char*>("(J)J"),
     reinterpret_cast<void*>(NativeLooper_takeVsync)},
};

// --- NativeImageCodec: native macOS image decode (webp/heic/...) ---
// Implemented in cocoa_bridge.mm via CGImageSource. Returns a malloc'd
// ARGB buffer (0xAARRGGBB) the caller must free.
// A COMPLETE sqlite3_mem_methods for a bundled sqlite3 (e.g. inside libmaesdk / OneDS OfflineStorage)
// whose allocator otherwise ends up with null method pointers on our runtime. sqlite3's own sentinel
// is only `if (m.xMalloc==0) install-default`, so a PARTIAL methods struct (xMalloc set, xRoundup/xSize
// null) is not repaired and mallocWithAlarm() then calls a null m.xRoundup(size) -> SIGSEGV, with
// sqlite3_open failing SQLITE_NOMEM. We install all 8 fields (macOS libc malloc + malloc_size), matching
// sqlite3's own mem1.c/SQLITE_SYSTEM_MALLOC backend. See docs/pairip-libmaesdk-oneds-httpclient_2026-09-17.md.
extern "C" {
    // Optional allocator trace (MACAPK_ONEDS_SQLITE_TRACE), read ONCE (not per-malloc).
    // Confirmed 2026-09-18 the allocator behaves: valid pointers + 8-byte roundup; the
    // OneDS "(7) NOMEM" on DB open is sqlite3's graceful path, not this allocator failing.
    static bool macapk_sq_trace() { static bool t = std::getenv("MACAPK_ONEDS_SQLITE_TRACE") != nullptr; return t; }
    static void* macapk_sq_malloc(int n) {
        void* p = n > 0 ? std::malloc((size_t)n) : nullptr;
        if (macapk_sq_trace()) { static int c=0; if(c++<40) std::fprintf(stderr,"[sqlite-mem] xMalloc(%d)=%p\n",n,p); }
        return p;
    }
    static void  macapk_sq_free(void* p)            { std::free(p); }
    static void* macapk_sq_realloc(void* p, int n)  { return std::realloc(p, (size_t)n); }
    static int   macapk_sq_size(void* p)            { return p ? (int)malloc_size(p) : 0; }
    static int   macapk_sq_roundup(int n)           { int r=(n + 7) & ~7; if (macapk_sq_trace()){static int c=0; if(c++<40) std::fprintf(stderr,"[sqlite-mem] xRoundup(%d)=%d\n",n,r);} return r; }
    static int   macapk_sq_init(void*)              { return 0; /* SQLITE_OK */ }
    static void  macapk_sq_shutdown(void*)          { }
}
// Layout MUST match sqlite3_mem_methods: xMalloc,xFree,xRealloc,xSize,xRoundup,xInit,xShutdown,pAppData.
struct macapk_sqlite3_mem_methods {
    void* (*xMalloc)(int); void (*xFree)(void*); void* (*xRealloc)(void*, int);
    int (*xSize)(void*); int (*xRoundup)(int); int (*xInit)(void*); void (*xShutdown)(void*); void* pAppData;
};
static macapk_sqlite3_mem_methods g_macapk_sq_methods = {
    macapk_sq_malloc, macapk_sq_free, macapk_sq_realloc, macapk_sq_size,
    macapk_sq_roundup, macapk_sq_init, macapk_sq_shutdown, nullptr
};
// Noop mutex methods for the bundled sqlite3 (sqlite3_mutex_methods layout). On our runtime the SDK's
// custom sqlite3GlobalConfig.mutex is also left with null method pointers (after the allocator is fixed,
// sqlite3MutexInit calls a null mutex.xMutexAlloc -- the x0=SQLITE_MUTEX_STATIC_MAIN=2 null-recovery).
// A no-op mutex (single-connection telemetry, no real concurrency) mirrors sqlite3's own mutex_noop.c.
extern "C" {
    static int   macapk_mx_init()          { return 0; }
    static int   macapk_mx_end()           { return 0; }
    static void* macapk_mx_alloc(int)      { static int one; return &one; } // one static handle for all
    static void  macapk_mx_free(void*)     { }
    static void  macapk_mx_enter(void*)    { }
    static int   macapk_mx_try(void*)      { return 0; }  // SQLITE_OK == acquired
    static void  macapk_mx_leave(void*)    { }
    static int   macapk_mx_held(void*)     { return 1; }
    static int   macapk_mx_notheld(void*)  { return 1; }
}
struct macapk_sqlite3_mutex_methods {
    int (*xMutexInit)(void); int (*xMutexEnd)(void); void* (*xMutexAlloc)(int); void (*xMutexFree)(void*);
    void (*xMutexEnter)(void*); int (*xMutexTry)(void*); void (*xMutexLeave)(void*);
    int (*xMutexHeld)(void*); int (*xMutexNotheld)(void*);
};
static macapk_sqlite3_mutex_methods g_macapk_mutex_methods = {
    macapk_mx_init, macapk_mx_end, macapk_mx_alloc, macapk_mx_free,
    macapk_mx_enter, macapk_mx_try, macapk_mx_leave, macapk_mx_held, macapk_mx_notheld
};
// EXPERIMENTAL DIAGNOSTIC (gated MACAPK_ONEDS_SQLITE_FIX, per-build offset -> to retire): write our
// complete allocator + mutex methods DIRECTLY into the bundled sqlite3's sqlite3GlobalConfig, to get
// past the OneDS-broken OS layer without the guest Bionic-variadic sqlite3_config call (x18/canary ABI
// edge from the host). m_addr is sqlite3GlobalConfig.m (config+0x20); mutex is at config+0x60 == m+0x40
// (Sqlite3Config layout: ints/flags, then sqlite3_mem_methods m (0x40), then sqlite3_mutex_methods).
// Positive-ID'd to this libmaesdk build. Generic fix = a guest-side sqlite3_config with x18 set up.
// Runtime-derive sqlite3GlobalConfig.m GENERICALLY (no hardcoded per-build offset) by disassembling the
// bundled sqlite3's sqlite3_config: its SQLITE_CONFIG_MALLOC case does `sqlite3GlobalConfig.m = *methods`
// as a 64-byte copy -- `adrp xN,#page; add xN,xN,#off; str q?,[xN,#0x30]; ...,#0x20; ...,#0x10; str q?,[xN]`.
// A str-q quartet covering [xN .. xN+0x30] at an adrp+add-computed address uniquely identifies .m.
extern "C" void* macapk_derive_sqlite3_config_m(void* config_fn) {
    if (!config_fn) return nullptr;
    const uint32_t* code = reinterpret_cast<const uint32_t*>(config_fn);
    uint64_t reg_addr[32] = {0}; bool reg_val[32] = {false};
    uint64_t grp_base = 0; unsigned grp_hits = 0;
    uint64_t bases[96]; int nb = 0;                                   // all 64-byte-quartet base addresses seen
    for (int i = 0; i < 1200; i++) {
        uint32_t insn = code[i];
        uint64_t pc = reinterpret_cast<uint64_t>(code + i);
        if ((insn & 0x9F000000u) == 0x90000000u) {                    // adrp Rd, imm
            int rd = insn & 0x1F;
            int64_t imm = (int64_t)((((insn >> 5) & 0x7FFFF) << 2) | ((insn >> 29) & 3));
            if (imm & (1LL << 20)) imm |= ~((1LL << 21) - 1);         // sign-extend 21-bit
            reg_addr[rd] = (pc & ~0xFFFULL) + (uint64_t)(imm << 12); reg_val[rd] = true;
        } else if ((insn & 0xFF800000u) == 0x91000000u) {             // add Rd, Rn, #imm12 {,LSL #12}
            int rd = insn & 0x1F, rn = (insn >> 5) & 0x1F;
            uint32_t imm12 = (insn >> 10) & 0xFFF, sh = (insn >> 22) & 1;
            if (reg_val[rn]) { reg_addr[rd] = reg_addr[rn] + (sh ? (imm12 << 12) : imm12); reg_val[rd] = true; }
            else if (rd != rn) reg_val[rd] = false;
        } else if ((insn & 0xFFC00000u) == 0x3D800000u) {             // str q, [Rn, #imm12*16]
            int rn = (insn >> 5) & 0x1F; uint64_t off = (uint64_t)((insn >> 10) & 0xFFF) * 16;
            if (reg_val[rn] && off <= 0x30 && (off % 0x10) == 0) {
                if (reg_addr[rn] != grp_base) { grp_base = reg_addr[rn]; grp_hits = 0; }
                grp_hits |= 1u << (off / 0x10);
                if (grp_hits == 0xF) {                                // completed a [base..base+0x30] quartet
                    bool seen = false; for (int k = 0; k < nb; k++) if (bases[k] == grp_base) seen = true;
                    if (!seen && nb < 96) bases[nb++] = grp_base;
                    grp_hits = 0;                                     // allow re-detecting the next struct
                }
            }
        }
    }
    // sqlite3GlobalConfig.m (0x40 bytes) is immediately followed by .mutex (also copied via a quartet),
    // so .m is the unique base X whose X+0x40 is ALSO a quartet base. (CONFIG_MUTEX's own quartet is the
    // only other one whose +0x40 is not a base -> the next field is pcache2 at +0x48.)
    for (int a = 0; a < nb; a++)
        for (int b = 0; b < nb; b++)
            if (bases[b] == bases[a] + 0x40) return reinterpret_cast<void*>(bases[a]);
    return nb == 1 ? reinterpret_cast<void*>(bases[0]) : nullptr;     // fallback: a lone quartet is .m
}
extern "C" int macapk_install_sqlite3_allocator(void* m_addr) {
    if (!m_addr) return -1;
    std::memcpy(m_addr, &g_macapk_sq_methods, sizeof(g_macapk_sq_methods));  // sqlite3GlobalConfig.m (8 fptrs)
    void* mutex_addr = reinterpret_cast<char*>(m_addr) + 0x40;               // sqlite3GlobalConfig.mutex
    std::memcpy(mutex_addr, &g_macapk_mutex_methods, sizeof(g_macapk_mutex_methods)); // 9 fptrs
    std::fprintf(stderr, "[oneds] wrote complete sqlite3 allocator@%p + mutex@%p\n", m_addr, mutex_addr);
    return 0;
}

// A minimal but complete sqlite3 VFS backed by macOS POSIX, for a bundled sqlite3 compiled with
// SQLITE_OS_OTHER (no built-in unix VFS) -- e.g. OneDS/libmaesdk, whose custom VFS never registers on
// our runtime, so sqlite3_open fails "no such vfs". We register this as the default so the SDK's
// OfflineStorage can open its DB. Single-process, no-op file locking (telemetry uses one connection).
// Struct layouts match sqlite3.h (bundled version 3.34.1: sqlite3_vfs iVersion 3, io_methods iVersion 1).
// See docs/pairip-libmaesdk-oneds-httpclient_2026-09-17.md.
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
typedef long long macapk_sq_i64;
namespace {
// SQLite result codes / flags we need.
enum { SQLITE_OK_=0, SQLITE_ERROR_=1, SQLITE_IOERR_=10, SQLITE_CANTOPEN_=14, SQLITE_NOTFOUND_=12,
       SQLITE_IOERR_SHORT_READ_=(10|(2<<8)), SQLITE_IOERR_WRITE_=(10|(3<<8)), SQLITE_IOERR_FSYNC_=(10|(4<<8)),
       SQLITE_IOERR_TRUNCATE_=(10|(6<<8)), SQLITE_IOERR_FSTAT_=(10|(7<<8)), SQLITE_IOERR_DELETE_=(10|(23<<8)) };
enum { OPEN_READONLY=0x1, OPEN_READWRITE=0x2, OPEN_CREATE=0x4, OPEN_EXCLUSIVE=0x10, OPEN_DELETEONCLOSE=0x8 };
struct MacFile;
struct IoMethods {
    int iVersion;
    int (*xClose)(MacFile*);
    int (*xRead)(MacFile*, void*, int, macapk_sq_i64);
    int (*xWrite)(MacFile*, const void*, int, macapk_sq_i64);
    int (*xTruncate)(MacFile*, macapk_sq_i64);
    int (*xSync)(MacFile*, int);
    int (*xFileSize)(MacFile*, macapk_sq_i64*);
    int (*xLock)(MacFile*, int);
    int (*xUnlock)(MacFile*, int);
    int (*xCheckReservedLock)(MacFile*, int*);
    int (*xFileControl)(MacFile*, int, void*);
    int (*xSectorSize)(MacFile*);
    int (*xDeviceCharacteristics)(MacFile*);
};
struct MacFile { const IoMethods* pMethods; int fd; int deleteOnClose; char path[1024]; };
int f_close(MacFile* f){ if(f->fd>=0) close(f->fd); if(f->deleteOnClose&&f->path[0]) unlink(f->path); f->fd=-1; return SQLITE_OK_; }
int f_read(MacFile* f, void* buf, int amt, macapk_sq_i64 ofst){
    ssize_t n=pread(f->fd, buf, (size_t)amt, (off_t)ofst);
    if(n==amt) return SQLITE_OK_;
    if(n>=0){ std::memset((char*)buf+n, 0, (size_t)(amt-n)); return SQLITE_IOERR_SHORT_READ_; }
    return SQLITE_IOERR_;
}
int f_write(MacFile* f, const void* buf, int amt, macapk_sq_i64 ofst){
    ssize_t n=pwrite(f->fd, buf, (size_t)amt, (off_t)ofst);
    return n==amt ? SQLITE_OK_ : SQLITE_IOERR_WRITE_;
}
int f_truncate(MacFile* f, macapk_sq_i64 sz){ return ftruncate(f->fd,(off_t)sz)==0?SQLITE_OK_:SQLITE_IOERR_TRUNCATE_; }
int f_sync(MacFile* f, int){ return fsync(f->fd)==0?SQLITE_OK_:SQLITE_IOERR_FSYNC_; }
int f_fileSize(MacFile* f, macapk_sq_i64* pSz){ struct stat st; if(fstat(f->fd,&st)!=0) return SQLITE_IOERR_FSTAT_; *pSz=(macapk_sq_i64)st.st_size; return SQLITE_OK_; }
int f_lock(MacFile*, int){ return SQLITE_OK_; }          // single-process: no-op locking
int f_unlock(MacFile*, int){ return SQLITE_OK_; }
int f_checkReserved(MacFile*, int* pOut){ *pOut=0; return SQLITE_OK_; }
int f_fileControl(MacFile*, int, void*){ return SQLITE_NOTFOUND_; }
int f_sectorSize(MacFile*){ return 4096; }
int f_deviceChar(MacFile*){ return 0; }
const IoMethods g_io = { 1, f_close, f_read, f_write, f_truncate, f_sync, f_fileSize,
                         f_lock, f_unlock, f_checkReserved, f_fileControl, f_sectorSize, f_deviceChar };
struct MacVfs {
    int iVersion; int szOsFile; int mxPathname; void* pNext; const char* zName; void* pAppData;
    int (*xOpen)(MacVfs*, const char*, MacFile*, int, int*);
    int (*xDelete)(MacVfs*, const char*, int);
    int (*xAccess)(MacVfs*, const char*, int, int*);
    int (*xFullPathname)(MacVfs*, const char*, int, char*);
    void* (*xDlOpen)(MacVfs*, const char*);
    void (*xDlError)(MacVfs*, int, char*);
    void* (*xDlSym)(MacVfs*, void*, const char*);
    void (*xDlClose)(MacVfs*, void*);
    int (*xRandomness)(MacVfs*, int, char*);
    int (*xSleep)(MacVfs*, int);
    int (*xCurrentTime)(MacVfs*, double*);
    int (*xGetLastError)(MacVfs*, int, char*);
    int (*xCurrentTimeInt64)(MacVfs*, macapk_sq_i64*);
    void* xSetSystemCall; void* xGetSystemCall; void* xNextSystemCall;
};
// Route file ops through the shim's __macapk_arm64_open so Android paths (/data/data, /sdcard, ...)
// are remapped to the host cache dir exactly like the guest's own libc calls.
typedef int (*shim_open_fn)(const char*, int, unsigned short);
static shim_open_fn macapk_shim_open() {
    static shim_open_fn fn = reinterpret_cast<shim_open_fn>(dlsym(RTLD_DEFAULT, "__macapk_arm64_open"));
    return fn;
}
// /data/local/tmp is NOT in the shim's remap set, but OneDS puts its telemetry DB there. Mirror it
// under the android root (a real host dir). Returns 1 + host path in buf, or 0 to fall through to the
// shim (which handles /data/data, /sdcard, ...).
static int macapk_vfs_hostpath(const char* path, char* buf, size_t n) {
    if (!path || std::strncmp(path, "/data/local/tmp", 15) != 0) return 0;
    typedef const char* (*rootfn)();
    static rootfn gr = reinterpret_cast<rootfn>(dlsym(RTLD_DEFAULT, "__macapk_get_android_root"));
    const char* root = gr ? gr() : nullptr;
    if (!root || !*root) return 0;
    std::snprintf(buf, n, "%s%s", root, path);   // <android_root>/data/local/tmp/...
    return 1;
}
static void macapk_mkdirs_parent(const char* p) {
    char d[1200]; std::snprintf(d, sizeof(d), "%s", p);
    char* slash = std::strrchr(d, '/'); if (!slash) return; *slash = 0;
    for (char* c = d + 1; *c; ++c) if (*c == '/') { *c = 0; mkdir(d, 0755); *c = '/'; }
    mkdir(d, 0755);
}
static int macapk_vfs_open(const char* path, int oflags) {
    char hp[1200];
    if (macapk_vfs_hostpath(path, hp, sizeof(hp))) {
        if (oflags & O_CREAT) macapk_mkdirs_parent(hp);
        return open(hp, oflags, 0644);
    }
    shim_open_fn fn = macapk_shim_open();
    return fn ? fn(path, oflags, 0644) : open(path, oflags, 0644);
}
int v_open(MacVfs*, const char* zName, MacFile* f, int flags, int* pOutFlags){
    int oflags=0;
    if(flags&OPEN_READWRITE) oflags|=O_RDWR; else oflags|=O_RDONLY;
    if(flags&OPEN_CREATE)    oflags|=O_CREAT;
    if(flags&OPEN_EXCLUSIVE) oflags|=O_EXCL;
    int fd = zName ? macapk_vfs_open(zName, oflags) : -1;
    if(fd<0 && (flags&OPEN_READWRITE)){ // retry read-only, as sqlite's unix VFS does
        fd=macapk_vfs_open(zName, O_RDONLY); if(fd>=0 && pOutFlags) *pOutFlags=OPEN_READONLY;
    }
    if(fd<0){ std::fprintf(stderr,"[oneds] vfs xOpen FAILED path='%s' flags=0x%x errno=%d\n", zName?zName:"(null)", flags, errno); return SQLITE_CANTOPEN_; }
    f->pMethods=&g_io; f->fd=fd; f->deleteOnClose=(flags&OPEN_DELETEONCLOSE)?1:0;
    f->path[0]=0; if(zName){ std::strncpy(f->path, zName, sizeof(f->path)-1); f->path[sizeof(f->path)-1]=0; }
    if(pOutFlags) *pOutFlags = flags;
    return SQLITE_OK_;
}
int v_delete(MacVfs*, const char* zName, int){
    char hp[1200]; const char* p = macapk_vfs_hostpath(zName, hp, sizeof(hp)) ? hp : zName;
    if(unlink(p)!=0 && errno!=ENOENT) return SQLITE_IOERR_DELETE_; return SQLITE_OK_;
}
int v_access(MacVfs*, const char* zName, int flags, int* pResOut){
    int m = flags==0 ? F_OK : (flags==1 ? R_OK|W_OK : R_OK);   // 0=EXISTS,1=READWRITE,2=READ
    char hp[1200]; const char* p = macapk_vfs_hostpath(zName, hp, sizeof(hp)) ? hp : zName;
    *pResOut = (access(p, m)==0) ? 1 : 0; return SQLITE_OK_;
}
int v_fullPathname(MacVfs*, const char* zName, int nOut, char* zOut){
    if(zName[0]=='/'){ std::strncpy(zOut, zName, (size_t)nOut-1); zOut[nOut-1]=0; }
    else { char cwd[1024]; if(!getcwd(cwd,sizeof(cwd))) cwd[0]=0; std::snprintf(zOut,(size_t)nOut,"%s/%s",cwd,zName); }
    return SQLITE_OK_;
}
int v_randomness(MacVfs*, int nByte, char* zOut){ arc4random_buf(zOut,(size_t)nByte); return nByte; }
int v_sleep(MacVfs*, int us){ usleep((unsigned)us); return us; }
int v_currentTime(MacVfs*, double* pt){ *pt = 2440587.5 + (double)time(nullptr)/86400.0; return SQLITE_OK_; }
int v_currentTimeInt64(MacVfs*, macapk_sq_i64* pt){ *pt = (macapk_sq_i64)(2440587.5*86400000.0) + (macapk_sq_i64)time(nullptr)*1000; return SQLITE_OK_; }
int v_getLastError(MacVfs*, int, char*){ return 0; }
void* v_dlOpen(MacVfs*, const char*){ return nullptr; }
void v_dlError(MacVfs*, int n, char* z){ if(n>0) z[0]=0; }
void* v_dlSym(MacVfs*, void*, const char*){ return nullptr; }
void v_dlClose(MacVfs*, void*){}
MacVfs g_vfs = { 3, (int)sizeof(MacFile), 1024, nullptr, "macapk_posix", nullptr,
    v_open, v_delete, v_access, v_fullPathname, v_dlOpen, v_dlError, v_dlSym, v_dlClose,
    v_randomness, v_sleep, v_currentTime, v_getLastError, v_currentTimeInt64, nullptr, nullptr, nullptr };
} // namespace
// Register our POSIX VFS with a bundled SQLITE_OS_OTHER sqlite3 via its own sqlite3_vfs_register(vfs, makeDefault).
// register_fn is resolved by the loader from the guest image. Non-variadic, so a plain host call is safe.
extern "C" int macapk_register_sqlite3_vfs(void* register_fn) {
    if (!register_fn) return -1;
    int rc = reinterpret_cast<int(*)(void*, int)>(register_fn)(&g_vfs, 1 /*makeDefault*/);
    std::fprintf(stderr, "[oneds] registered macapk_posix sqlite3 VFS (default): rc=%d\n", rc);
    return rc;
}

// Register a minimal stub HttpClient with a loaded Microsoft 1DS/OneDS telemetry SDK
// (github.com/microsoft/cpp_client_telemetry, statically archived into e.g. Minecraft's
// libmaesdk.so) so LogManagerImpl construction does not deref a null m_httpClient and crash.
// cci_fn is the SDK's own JNI registration export
// (Java_com_microsoft_applications_events_HttpClient_createClientInstance), resolved by the loader
// from the guest image (arm64_native.cpp, right after run_init_array). Generic: the loader only
// calls this when that export is present in a live image, never keyed on package name. Telemetry
// HTTP then degrades to an offline no-op. See docs/pairip-libmaesdk-oneds-httpclient_2026-09-17.md.
extern "C" void macapk_register_oneds_httpclient(void* cci_fn) {
    static bool s_done = false;
    if (s_done || !cci_fn) return;
    JavaVM* vm = nullptr; jsize n = 0;
    if (JNI_GetCreatedJavaVMs(&vm, 1, &n) != JNI_OK || n <= 0 || !vm) return;
    JNIEnv* env = nullptr;
    bool attached = false;
    if (vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK || !env) {
        if (vm->AttachCurrentThread(reinterpret_cast<void**>(&env), nullptr) != JNI_OK || !env) return;
        attached = true;
    }
    jclass cls = env->FindClass("com/macapk/loader/OneDSHttpClientStub");
    if (env->ExceptionCheck()) env->ExceptionClear();
    if (cls) {
        jmethodID ctor = env->GetMethodID(cls, "<init>", "()V");
        jobject stub = ctor ? env->NewObject(cls, ctor) : nullptr;
        if (env->ExceptionCheck()) { env->ExceptionClear(); stub = nullptr; }
        if (stub) {
            // HttpClient_Android::CreateClientInstance(env, java_client): sets the static s_client +
            // s_java_vm and caches java_client's createTask/executeTask method IDs.
            reinterpret_cast<void(*)(JNIEnv*, jobject)>(cci_fn)(env, stub);
            if (env->ExceptionCheck()) env->ExceptionClear();
            s_done = true;
            std::fprintf(stderr, "[oneds] registered stub HttpClient via createClientInstance "
                                 "(telemetry HTTP -> offline no-op)\n");
        }
    } else {
        std::fprintf(stderr, "[oneds] OneDSHttpClientStub not findable on this thread's loader; "
                             "HttpClient not registered\n");
    }
    if (attached) vm->DetachCurrentThread();
}

extern "C" uint32_t* MacApkDecodeImageBytes(const uint8_t* data, size_t len,
                                            int* outW, int* outH);

// jint[] decode(byte[] data) -> [w, h, argb...] or null.
jintArray ImageCodec_decode(JNIEnv* env, jclass, jbyteArray data_j) {
    if (!data_j) return nullptr;
    jsize len = env->GetArrayLength(data_j);
    if (len <= 0) return nullptr;
    jbyte* bytes = env->GetByteArrayElements(data_j, nullptr);
    if (!bytes) return nullptr;
    int w = 0, h = 0;
    uint32_t* px = MacApkDecodeImageBytes(
        reinterpret_cast<const uint8_t*>(bytes), (size_t)len, &w, &h);
    env->ReleaseByteArrayElements(data_j, bytes, JNI_ABORT);
    if (!px || w <= 0 || h <= 0) { if (px) free(px); return nullptr; }
    // Pack [w, h, pixels...] into a jintArray.
    jsize total = 2 + w * h;
    jintArray out = env->NewIntArray(total);
    if (!out) { free(px); return nullptr; }
    jint header[2] = { w, h };
    env->SetIntArrayRegion(out, 0, 2, header);
    env->SetIntArrayRegion(out, 2, w * h, reinterpret_cast<const jint*>(px));
    free(px);
    return out;
}

const JNINativeMethod kNativeImageCodecMethods[] = {
    {const_cast<char*>("decode"), const_cast<char*>("([B)[I"),
     reinterpret_cast<void*>(ImageCodec_decode)},
};
}  // namespace

// Forward-declared from native_sqlite.cpp; registers
// com.macapk.loader.NativeSqlite's JNI methods.
namespace sqlite { void RegisterNativeSqliteMethods(JNIEnv* env); }
namespace osnative { void RegisterNativeOsMethods(JNIEnv* env); }
// Forward-declared from audio_backend.mm; registers
// com.macapk.loader.NativeAudio's JNI methods (CoreAudio output).
namespace audio  { void RegisterNativeAudioMethods(JNIEnv* env); }
// Forward-declared from media_decode.mm; registers
// com.macapk.loader.NativeMedia's JNI methods (AudioToolbox decode).
namespace media  { void RegisterNativeMediaMethods(JNIEnv* env); }
// Forward-declared from video_toolbox.mm; registers the REST of
// com.macapk.loader.NativeMedia's JNI methods (VideoToolbox video decode --
// the audio half above and the video half here are two separate
// RegisterNatives calls against the same jclass, same pattern already used
// for NativeLog's kNativeLogMethods/kNativeQos below).
namespace video  { void RegisterNativeVideoMethods(JNIEnv* env); }
// Forward-declared from video_vp8_decode.mm; registers NativeMedia's five
// VP8-specific natives (nativeVp8Open/_Decode/_Poll/_Flush/_Close, libvpx
// software decode -- VideoToolbox has no VP8 decoder on any macOS version).
// ⚠ v1.0.984 shipped video_vp8_decode.mm with this call DEFERRED ("a
// separate, concurrently-routed change this lane does not own" -- see that
// file's own former "WIRING STILL NEEDED" section 1, and MediaCodec.java's
// safeVp8Open() doc, both written anticipating exactly this gap). It was
// never landed: confirmed by grepping this file for "vp8" before this edit
// (zero hits) and by MediaCodecList/MediaCodec's own comments describing
// the not-yet-registered window as current. So until this line, EVERY VP8
// open() call from Java hit an UnsatisfiedLinkError -- caught by
// safeVp8Open's LinkageError guard and turned into "failed to open" (0),
// silently, exactly the shape that guard was built to survive. Landed here
// (found while wiring VP9's identical registration, below) because this
// file is this lane's to own and the fix is a two-line addition with no
// ownership conflict.
namespace video_vp8 { void RegisterNativeVp8Methods(JNIEnv* env); }
// Forward-declared from video_vp9_decode.mm; registers NativeMedia's five
// VP9-specific natives (nativeVp9Open/_Decode/_Poll/_Flush/_Close, libvpx
// software decode). VideoToolbox probes VP9 as genuinely absent on this
// host (OSStatus -12906 kVTCouldNotFindVideoDecoderErr even with a real
// config record -- docs/videotoolbox-probe-audit_2026-08-16.md section 3),
// so this is rung 2 of CLAUDE.md's decision ladder, the same rung VP8 used.
namespace video_vp9 { void RegisterNativeVp9Methods(JNIEnv* env); }
// Forward-declared from audio_input.mm; registers
// com.macapk.loader.AudioInputBridge's JNI methods (AVAudioEngine mic capture).
// v1.0.940: the MacBook has a microphone, so AudioRecord stopped returning
// silence while claiming STATE_INITIALIZED.
namespace audioin { void RegisterAudioInputMethods(JNIEnv* env); }
// Forward-declared from camera_capture.mm; registers
// com.macapk.loader.CameraBridge's JNI methods (AVFoundation capture).
// v1.0.940: the MacBook has a camera, so we stopped pretending otherwise.
namespace camera { void RegisterCameraBridgeMethods(JNIEnv* env); }
// Forward-declared from gamecontroller_bridge.mm; registers
// com.macapk.loader.GamepadBridge's JNI method (GameController.framework ->
// Android gamepads; docs/host-game-controllers_2026-10-02.md).
namespace gamepad { void RegisterGamepadBridgeMethods(JNIEnv* env); }
// Forward-declared from location_bridge.mm; registers
// com.macapk.loader.LocationBridge's JNI methods (macOS Location Services ->
// android.location.LocationManager).
namespace location { void RegisterLocationBridgeMethods(JNIEnv* env); }
namespace bluetooth { void RegisterBluetoothBridgeMethods(JNIEnv* env); }
namespace bluetoothperipheral { void RegisterBluetoothPeripheralBridgeMethods(JNIEnv* env); }
// Forward-declared from nowplaying_bridge.mm; registers
// android.media.session.MacApkNowPlaying (the active MediaSession <-> macOS
// Now Playing and its remote commands). Soft-fails on a jar that lacks it.
namespace nowplaying { void RegisterNowPlayingBridgeMethods(JNIEnv* env); }
// Forward-declared from webview_bridge.mm; registers
// android.webkit.MacApkWebViewBridge's JNI methods (system WKWebView engine
// behind android.webkit.WebView). Soft-fails: a registration miss disables
// the engine for the run instead of aborting boot.
namespace webview { void RegisterWebViewBridgeMethods(JNIEnv* env); }
// Forward-declared from clipboard_bridge.mm; registers
// android.content.ClipboardManager$MacApkHostPasteboard's JNI methods (the
// macOS general pasteboard behind ClipboardManager, gate MACAPK_CLIPBOARD_HOST).
// Soft-fails: a registration miss leaves ClipboardManager on its pre-bridge
// path for the run instead of aborting boot.
namespace clipboard { void RegisterClipboardBridgeMethods(JNIEnv* env); }
// com.macapk.icu.HostIcu's JNI methods (ICU number skeletons through the host's
// libicucore, for android.icu.text.MeasureFormat). Soft-fails: a miss leaves unit
// formatting throwing UnsupportedOperationException. runtime/src/icu_host_bridge.cpp.
namespace icuhost { bool RegisterIcuHostMethods(JNIEnv* env); }
// Defined in native_linker.cpp: com.macapk.loader.NativeLinker.nativeLinkOnly, a
// dlopen with NO JNI_OnLoad -- how the Android linker maps a DT_NEEDED dependency.
// AppClassLoader's SoLoader dependency premap uses it (MACAPK_SOLOADER_PREMAP_LINK_ONLY,
// docs/soloader-premap-jni-onload-timing_2026-09-29.md). Soft-fails.
namespace nativelinker { void RegisterNativeLinkerMethods(JNIEnv* env); }
// Defined in document_picker.mm (v1.0.1653): DocumentPickerHandler.nativePick, the
// macOS open/save panel behind GET_CONTENT / OPEN_DOCUMENT / CREATE_DOCUMENT
// (MACAPK_DOC_PICKER). Soft-fails: a jar without the declaration cancels picks.
void RegisterDocumentPickerMethods(JNIEnv* env);
// Forward-declared from wallpaper_bridge.mm; registers
// com.macapk.loader.monet.HostWallpaper's JNI methods (which desktop picture the screen shows,
// and its pixels), the input to Material You from the Mac wallpaper
// (MACAPK_MONET_WALLPAPER_SEED, docs/monet-wallpaper-seed_2026-09-26.md). Soft-fails: a miss
// leaves the fixed palette and logs "wallpaper natives unbound".
namespace wallpaper { void RegisterWallpaperBridgeMethods(JNIEnv* env); }
// Forward-declared from photos_bridge.mm; registers android.provider.MediaStoreProvider's
// nativePhotos* JNI methods, the macOS Photos library as a read-only MediaStore source (MACAPK_PHOTOKIT,
// docs/photos-library-mediastore_2026-09-29.md). Soft-fails: no Photos source.
namespace photos { void RegisterPhotosBridgeMethods(JNIEnv* env); }
// Defined in cocoa_bridge.mm; registers com.macapk.loader.NativeCocoa's
// setSecureContent(boolean), the FLAG_SECURE -> NSWindow.sharingType mirror
// (gate MACAPK_SECURE_WINDOW_SHARING). Soft-fails, so it is NOT a row in
// kNativeCocoaMethods: Bind() would abort boot on a jar that lacks the decl.
namespace cocoasecure { void RegisterSecureContentMethods(JNIEnv* env); }
#ifdef MACAPK_HAS_ANGLE
// Defined in angle_glue.cpp; registers javax.microedition.khronos.egl.MacApkEGL10's
// nativeCreateSurfaceViewWindow: the window surface a SurfaceView GL thread that
// survived the EGL10 refusal gets on the SurfaceView's native window
// (MACAPK_GL_SURVIVOR_CONTEXT, docs/openttd-survivor-gl-context_2026-09-26.md).
// Soft-fails, so it is not a Bind() row: a jar without the declaration must not
// abort boot (MacApkEGL10 probes for the method and keeps the legacy refusal).
void RegisterAngleGlueNatives(JNIEnv* env);
#endif

// See macapk/native_methods.h and the macapk-java-vsync block above.
int JavaVsyncFire() { return JavaVsyncFireAt(JavaVsyncNanoTime()); }

void RegisterAllNativeMethods(JNIEnv* env) {
    Bind(env, "com/macapk/loader/NativeLog",   kNativeInstallJniTrace,
         sizeof(kNativeInstallJniTrace) / sizeof(kNativeInstallJniTrace[0]));
    Bind(env, "com/macapk/loader/NativeLog",   kNativeLogMethods,
         sizeof(kNativeLogMethods)   / sizeof(kNativeLogMethods[0]));
    Bind(env, "com/macapk/loader/NativeLog",   kNativeQos,
         sizeof(kNativeQos)          / sizeof(kNativeQos[0]));
    RegisterHostProxyNativeMethods(env);
    Bind(env, "com/macapk/loader/NativeLog",   kNativeBootTime,
         sizeof(kNativeBootTime)     / sizeof(kNativeBootTime[0]));
    Bind(env, "com/macapk/loader/NativeLog",   kNativePairipGate,
         sizeof(kNativePairipGate)   / sizeof(kNativePairipGate[0]));
    Bind(env, "com/macapk/loader/NativeCocoa", kNativeCocoaMethods,
         sizeof(kNativeCocoaMethods) / sizeof(kNativeCocoaMethods[0]));
    Bind(env, "com/macapk/gms/NativeSignIn",   kGmsSignInMethods,
         sizeof(kGmsSignInMethods)   / sizeof(kGmsSignInMethods[0]));
    Bind(env, "com/macapk/loader/NativeGles",  kNativeGlesMethods,
         sizeof(kNativeGlesMethods)  / sizeof(kNativeGlesMethods[0]));
    Bind(env, "android/opengl/GLES20",          kGles20Methods,
         sizeof(kGles20Methods)      / sizeof(kGles20Methods[0]));
    Bind(env, "android/opengl/GLUtils",         kGLUtilsMethods,
         sizeof(kGLUtilsMethods)     / sizeof(kGLUtilsMethods[0]));
    Bind(env, "android/opengl/GLES30",          kGl3GLES30Methods,
         sizeof(kGl3GLES30Methods)   / sizeof(kGl3GLES30Methods[0]));
    Bind(env, "android/opengl/GLES31",          kGl3GLES31Methods,
         sizeof(kGl3GLES31Methods)   / sizeof(kGl3GLES31Methods[0]));
    Bind(env, "android/opengl/GLES31Ext",       kGl3GLES31ExtMethods,
         sizeof(kGl3GLES31ExtMethods)/ sizeof(kGl3GLES31ExtMethods[0]));
    Bind(env, "android/opengl/GLES32",          kGl3GLES32Methods,
         sizeof(kGl3GLES32Methods)   / sizeof(kGl3GLES32Methods[0]));
    // GLES1 fixed-function. Registered UNCONDITIONALLY: registration only makes
    // the Java method resolvable, and an app that never calls them is unaffected.
    // Whether they DO anything depends on the ES1 context selection in
    // angle_glue.cpp, which main.cpp drives from an imported-symbol scan.
    Bind(env, "android/opengl/GLES10",          kGl1GLES10Methods,
         sizeof(kGl1GLES10Methods)   / sizeof(kGl1GLES10Methods[0]));
    Bind(env, "android/opengl/GLES11",          kGl1GLES11Methods,
         sizeof(kGl1GLES11Methods)   / sizeof(kGl1GLES11Methods[0]));
    RegisterEgl14Methods(env);
    Bind(env, "com/macapk/loader/NativeLooper", kNativeLooperMethods,
         sizeof(kNativeLooperMethods) / sizeof(kNativeLooperMethods[0]));
    // MACAPK_CHOREO_VSYNC_THREADS natives, registered SOFT (not a Bind() row):
    // a framework jar older than this runtime lacks the two declarations and
    // must still boot. android.os.Looper.macapkCanReceiveVsync() probes for them
    // and keeps the Choreographer on its Handler timer when they are unbound.
    {
        jclass nl = env->FindClass("com/macapk/loader/NativeLooper");
        if (nl) {
            if (env->RegisterNatives(nl, kNativeLooperVsyncMethods,
                    sizeof(kNativeLooperVsyncMethods) / sizeof(kNativeLooperVsyncMethods[0]))
                    != JNI_OK) {
                if (env->ExceptionCheck()) env->ExceptionClear();
                std::fprintf(stderr, "[macapk] NativeLooper.scheduleVsync/takeVsync not "
                             "declared by this framework jar; per-thread Choreographers "
                             "keep the Handler timer\n");
            }
            env->DeleteLocalRef(nl);
        } else if (env->ExceptionCheck()) {
            env->ExceptionClear();
        }
    }
    Bind(env, "com/macapk/loader/NativeImageCodec", kNativeImageCodecMethods,
         sizeof(kNativeImageCodecMethods) / sizeof(kNativeImageCodecMethods[0]));
    sqlite::RegisterNativeSqliteMethods(env);
    osnative::RegisterNativeOsMethods(env);
    audio::RegisterNativeAudioMethods(env);
    RegisterUserVerificationNativeMethods(env);
    audioin::RegisterAudioInputMethods(env);
    media::RegisterNativeMediaMethods(env);
    video::RegisterNativeVideoMethods(env);
    video_vp8::RegisterNativeVp8Methods(env);
    video_vp9::RegisterNativeVp9Methods(env);
    camera::RegisterCameraBridgeMethods(env);
    gamepad::RegisterGamepadBridgeMethods(env);
    location::RegisterLocationBridgeMethods(env);
    bluetooth::RegisterBluetoothBridgeMethods(env);
    bluetoothperipheral::RegisterBluetoothPeripheralBridgeMethods(env);
    nowplaying::RegisterNowPlayingBridgeMethods(env);
    webview::RegisterWebViewBridgeMethods(env);
    clipboard::RegisterClipboardBridgeMethods(env);
    icuhost::RegisterIcuHostMethods(env);
    nativelinker::RegisterNativeLinkerMethods(env);
    wallpaper::RegisterWallpaperBridgeMethods(env);
    photos::RegisterPhotosBridgeMethods(env);
    cocoasecure::RegisterSecureContentMethods(env);
    RegisterDocumentPickerMethods(env);
#ifdef MACAPK_HAS_ANGLE
    RegisterAngleGlueNatives(env);
#endif
}

}  // namespace macapk
