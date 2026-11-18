// window.cpp -- com.macapk.loader.NativeCocoa on Windows: the app's Win32 window, its
// software-rendered canvas, input and the message loop (PLAN.md §8.2–§8.4).
//
// NativeCocoa is upstream's name for its host window interface; it is kept as a transitional
// compatibility interface (PLAN.md §8.2) so the Android framework's callers stay unchanged:
// on Windows the same Java declarations are bound to the Win32 implementations below. Only
// the window, presentation and input natives the Java path needs are bound; the others stay
// unbound and throw UnsatisfiedLinkError naming themselves.
//
// Threads: init() and runLoop() run on the JVM's main thread, which becomes the UI thread and
// owns every window. Calls from other threads that create or change windows are posted to it
// and wait (RunOnUi): every 2 s of waiting logs UI_THREAD_BLOCKED, and a call the running loop
// has not started within 10 s is abandoned with UI_CALL_TIMEOUT, so a JNI caller holding a lock
// the UI thread waits for cannot hang the app (PLAN.md §8.3). presentBitmap only copies pixels
// under a lock and invalidates. Frames are driven from the UI thread: each tick calls the
// renderer's macapkSurfaceChanged (when the size changed) and macapkDrawFrame, as the macOS
// side's display link does; a minimised window keeps drawing at 2 Hz, as upstream throttles a
// hidden one, so app logic tied to frames keeps running. Coordinates: "points" are Windows DIPs
// (1/96 inch); a point is a dp because the runtime starts with density = 160 × the window's
// DPI scale.
#include "bridge.h"
#include "embed.h"
#include "graphics.h"
#include "guest_graphics.h"
#include "guest_vulkan.h"
#include "surface.h"

#include <windows.h>
#include <shellscalingapi.h>
#include <windowsx.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <vector>

namespace winapk::bridge {
namespace {

constexpr UINT kCallMessage = WM_APP + 1;  // wParam 0, lParam: std::shared_ptr<UiCall>*
constexpr UINT_PTR kFrameTimer = 1;
constexpr int kActionDown = 0, kActionUp = 1, kActionMove = 2, kActionCancel = 3;  // MotionEvent actions
constexpr ULONGLONG kHiddenFrameMs = 500;   // a minimised window draws at 2 Hz (upstream's hidden rate)
constexpr int kUiWaitLogMs = 2000, kUiAbandonMs = 10000;
constexpr int kFocusLogLines = 60;          // as upstream's [focus] trace

/// A canvas view: the renderer that draws it and the last frame it presented.
struct View {
    int widthPt = 0, heightPt = 0;  ///< the size it was created with
    jobject renderer = nullptr;     ///< global ref to a MetalSurfaceCallbacks
    std::mutex mu;                  ///< guards the frame below
    std::vector<uint32_t> pixels;   ///< ARGB (0xAARRGGBB), top-down, width × height
    int frameW = 0, frameH = 0;
    int sizedW = -1, sizedH = -1;   ///< the size last reported by macapkSurfaceChanged
    uint64_t rendererRevision = 0;   ///< invalidates an EGL owner when callbacks change
    jobject holderSurface = nullptr; ///< actual holder Surface for plain SurfaceView, global reference
    jfieldID surfaceReleased = nullptr;
};

/// An app window and the view it shows.
struct Window {
    HWND hwnd = nullptr;
    View* content = nullptr;
    COLORREF background = RGB(255, 255, 255);
    bool pointerDown = false;
    LPARAM lastPointer = 0;         ///< where the held pointer last was (client pixels)
    std::deque<wchar_t> keyChars;   ///< characters sent with their WM_KEYDOWN, their WM_CHAR still to come
    bool minimized = false;
    ULONGLONG lastHiddenFrame = 0;  ///< when a minimised window last drew
    std::shared_ptr<JavaGraphics> graphics;
    View* graphicsView = nullptr;
    uint64_t graphicsRevision = 0;
    bool inGraphicsFrame = false;
    bool closingGraphics = false;
    bool graphicsExposed = false;
};

/// A function posted to the UI thread by RunOnUi, shared by the waiting caller and the loop.
struct UiCall {
    enum State { kPending, kRunning, kDone, kAbandoned };
    const std::function<void()>* fn = nullptr;  ///< the caller's; valid while the caller waits
    std::mutex mu;
    std::condition_variable cv;
    State state = kPending;
};

JavaVM* g_vm = nullptr;
DWORD g_uiThread = 0;
HWND g_dispatcher = nullptr;              // message-only window on the UI thread
std::atomic<int> g_loop{0};               // the message loop: 0 not started, 1 running, 2 ended
jobject g_touch = nullptr;                // global ref to com.macapk.loader.TouchDispatcher
std::atomic<int> g_phoneW{400}, g_phoneH{800};  // the app's size, in points
std::map<HWND, Window*> g_windows;        // UI thread only
std::mutex g_viewsMu;
std::vector<View*> g_views;               // every view made; never freed while the VM lives
std::atomic<View*> g_current{nullptr};    // the view presentBitmap draws to
std::atomic<float> g_maxAspect{0};        // the app's max aspect ratio, 0 = uncapped
bool g_userLandscape = false;             // the orientation the app booted in (the "user rotation")
std::mutex g_dirsMu;
std::string g_dataSubdir, g_nativeLibDir; // for the guest loader (P2–P4); not used by the Java path
std::string g_graphicsRequest = "off";   // CLI option, captured before app code
bool g_graphicsFailed = false;            // window thread; read after Boot.run returns
bool g_guestGraphicsCommand = false;      // guards synchronous driver reentry
HWND g_guestDeferredClose = nullptr;

/// Publishes client pixels for the currently attached plain SurfaceView, with no graphics allocation.
void PublishWindowSurface(Window* w) {
    View* v = w->content;
    if (!v || !v->holderSurface) {
        RetireSurface(w->hwnd);
        return;
    }
    RECT rc{};
    if (!w->minimized) GetClientRect(w->hwnd, &rc);
    PublishSurface(w->hwnd, v->holderSurface, v->surfaceReleased, rc.right, rc.bottom);
}

/// Returns the JNIEnv of the calling thread, attaching it to the VM when it is not.
JNIEnv* Env() {
    JNIEnv* env = nullptr;
    if (g_vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_21) == JNI_EDETACHED)
        g_vm->AttachCurrentThread(reinterpret_cast<void**>(&env), nullptr);
    return env;
}

/// Logs and clears a pending Java exception thrown by a callback.
void CheckCallback(JNIEnv* env, const char* what) {
    if (!env->ExceptionCheck()) return;
    LogLine(std::string("[winapk] window: exception in ") + what);
    env->ExceptionDescribe();
    env->ExceptionClear();
}

/// Runs `fn`, the work of the native `what`, on the UI thread and waits for it.
/// Returns: true when it ran; false when it was abandoned or the loop had ended.
/// Notes: from the UI thread it runs inline. From another thread it is posted and waited for:
/// every 2 s of waiting logs UI_THREAD_BLOCKED; once the loop runs, a call it has not started
/// within 10 s is abandoned with UI_CALL_TIMEOUT. A call already started is never abandoned,
/// because `fn` lives on this stack. Before runLoop starts the wait has no limit (boot is slow
/// and nothing is wrong), and after the loop ends nothing runs: the call fails at once.
bool RunOnUi(const char* what, const std::function<void()>& fn) {
    if (GetCurrentThreadId() == g_uiThread) { fn(); return true; }
    auto call = std::make_shared<UiCall>();
    call->fn = &fn;
    auto* msg = new std::shared_ptr<UiCall>(call);
    if (g_loop == 2 || !PostMessageW(g_dispatcher, kCallMessage, 0, reinterpret_cast<LPARAM>(msg))) {
        delete msg;
        LogLine(std::string("[winapk] window: ") + what + " skipped: the UI thread's message loop has ended");
        return false;
    }
    std::unique_lock<std::mutex> lock(call->mu);
    int waited = 0;
    while (!call->cv.wait_for(lock, std::chrono::milliseconds(kUiWaitLogMs), [&] { return call->state == UiCall::kDone; })) {
        waited += kUiWaitLogMs;
        const int loop = g_loop;
        if (call->state == UiCall::kPending && (loop == 2 || (loop == 1 && waited >= kUiAbandonMs))) {
            call->state = UiCall::kAbandoned;
            LogLine(std::string("[winapk] window: UI_CALL_TIMEOUT: ") + what + " was not run by the UI thread in " +
                    std::to_string(waited / 1000) + " s; abandoned");
            return false;
        }
        const char* where = call->state == UiCall::kRunning ? "running" : loop == 0 ? "queued until the message loop starts" : "queued";
        LogLine(std::string("[winapk] window: UI_THREAD_BLOCKED: ") + what + " has waited " + std::to_string(waited / 1000) +
                " s for the UI thread (" + where + ", caller thread " + std::to_string(GetCurrentThreadId()) + ")");
    }
    return true;
}

/// Returns the effective DPI of the primary display, where a new window opens.
/// Notes: not GetDpiForSystem, which keeps the scale of sign-in until the person signs out
/// again, while windows already get the display's current DPI.
UINT DefaultDpi() {
    UINT x = 96, y = 96;
    if (FAILED(GetDpiForMonitor(MonitorFromPoint(POINT{0, 0}, MONITOR_DEFAULTTOPRIMARY), MDT_EFFECTIVE_DPI, &x, &y))) return 96;
    return x;
}

/// Returns the DPI scale of `hwnd` (or of the primary display): 1.0 at 96 DPI.
float Scale(HWND hwnd) {
    const UINT dpi = hwnd ? GetDpiForWindow(hwnd) : DefaultDpi();
    return dpi ? dpi / 96.0f : 1.0f;
}

/// Converts UTF-8 to UTF-16.
std::wstring Wide(const std::string& s) {
    std::wstring w(MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), w.data(), static_cast<int>(w.size()));
    return w;
}

/// Returns a method of TouchDispatcher, looked up once per call site.
jmethodID TouchMethod(JNIEnv* env, const char* name, const char* sig) {
    jclass c = env->GetObjectClass(g_touch);
    jmethodID m = env->GetMethodID(c, name, sig);
    env->DeleteLocalRef(c);
    return m;
}

/// Sends one pointer event to TouchDispatcher.dispatch(action, x, y, viewW, viewH).
/// Notes: x and y are points with y measured from the bottom, the Cocoa convention the
/// dispatcher flips back (TouchDispatcher.doDispatch).
void Touch(Window* w, int action, LPARAM lp) {
    if (!g_touch) return;
    RECT rc;
    GetClientRect(w->hwnd, &rc);
    const float s = Scale(w->hwnd);
    const float vw = rc.right / s, vh = rc.bottom / s;
    const float x = GET_X_LPARAM(lp) / s, y = vh - GET_Y_LPARAM(lp) / s;
    JNIEnv* env = Env();
    static jmethodID mid = TouchMethod(env, "dispatch", "(IFFFF)V");
    if (!mid) return;
    env->CallVoidMethod(g_touch, mid, action, x, y, vw, vh);
    CheckCallback(env, "TouchDispatcher.dispatch");
}

/// Ends a held pointer that Windows took away (capture lost, window deactivated) with
/// ACTION_CANCEL at its last position, releasing mouse capture if this window still holds it,
/// so neither the app nor Win32's system-command handling sees a press that never lifts.
void CancelPointer(Window* w) {
    if (!w->pointerDown) return;
    w->pointerDown = false;
    if (GetCapture() == w->hwnd) ReleaseCapture();
    Touch(w, kActionCancel, w->lastPointer);
}

/// Sends a wheel turn to TouchDispatcher.dispatchWheel; one notch is one line (1.0).
void Wheel(Window* w, WPARAM wp, LPARAM lp, bool horizontal) {
    if (!g_touch) return;
    POINT p{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
    ScreenToClient(w->hwnd, &p);
    RECT rc;
    GetClientRect(w->hwnd, &rc);
    const float s = Scale(w->hwnd);
    const float vw = rc.right / s, vh = rc.bottom / s;
    const float notches = GET_WHEEL_DELTA_WPARAM(wp) / static_cast<float>(WHEEL_DELTA);
    JNIEnv* env = Env();
    static jmethodID mid = TouchMethod(env, "dispatchWheel", "(FFFFFF)Z");
    if (!mid) return;
    env->CallBooleanMethod(g_touch, mid, p.x / s, vh - p.y / s, vw, vh,
                           horizontal ? 0.0f : notches, horizontal ? -notches : 0.0f);
    CheckCallback(env, "TouchDispatcher.dispatchWheel");
}

/// Maps a Windows virtual key to an Android KeyEvent keycode; 0 when there is none.
/// Notes: the first round covers common keys and Back (PLAN.md §8.4); Escape and
/// Browser Back are Android's BACK. A key with no keycode that types a character still
/// reaches the app, through WM_CHAR (Text).
int AndroidKey(WPARAM vk) {
    if (vk >= 'A' && vk <= 'Z') return 29 + static_cast<int>(vk - 'A');  // KEYCODE_A..Z
    if (vk >= '0' && vk <= '9') return 7 + static_cast<int>(vk - '0');   // KEYCODE_0..9
    switch (vk) {
        case VK_ESCAPE: case VK_BROWSER_BACK: return 4;  // BACK
        case VK_RETURN: return 66;                       // ENTER
        case VK_BACK: return 67;                         // DEL
        case VK_DELETE: return 112;                      // FORWARD_DEL
        case VK_TAB: return 61;
        case VK_SPACE: return 62;
        case VK_UP: return 19;
        case VK_DOWN: return 20;
        case VK_LEFT: return 21;
        case VK_RIGHT: return 22;
        case VK_HOME: return 122;                        // MOVE_HOME
        case VK_END: return 123;                         // MOVE_END
        case VK_OEM_COMMA: return 55;
        case VK_OEM_PERIOD: return 56;
        case VK_OEM_MINUS: return 69;
        case VK_OEM_PLUS: return 70;                     // EQUALS
        case VK_OEM_1: return 74;                        // SEMICOLON
        case VK_OEM_2: return 76;                        // SLASH
        case VK_OEM_7: return 75;                        // APOSTROPHE
        case VK_SHIFT: case VK_LSHIFT: return 59;
        case VK_RSHIFT: return 60;
        case VK_CONTROL: case VK_LCONTROL: return 113;
        case VK_RCONTROL: return 114;
        default: return 0;
    }
}

/// Calls TouchDispatcher.dispatchKey(action, keyCode, unicodeChar, metaState).
void DispatchKey(int action, int code, int unicode, int meta) {
    JNIEnv* env = Env();
    static jmethodID mid = TouchMethod(env, "dispatchKey", "(IIII)V");
    if (!mid) return;
    env->CallVoidMethod(g_touch, mid, action, code, unicode, meta);
    CheckCallback(env, "TouchDispatcher.dispatchKey");
}

/// Sends a mapped key to the app with the character it types.
/// Returns: that character (0 for none), so the WM_CHAR TranslateMessage makes of it is not sent again.
wchar_t Key(bool down, WPARAM vk, LPARAM lp) {
    const int code = AndroidKey(vk);
    if (!g_touch || code == 0) return 0;
    BYTE state[256];
    GetKeyboardState(state);
    wchar_t ch[4] = {};
    const int n = ToUnicode(static_cast<UINT>(vk), (lp >> 16) & 0xff, state, ch, 4, 4);  // 4: keep dead-key state
    const int unicode = n == 1 ? ch[0] : 0;
    int meta = 0;
    if (GetKeyState(VK_SHIFT) & 0x8000) meta |= 0x1;      // META_SHIFT_ON
    if (GetKeyState(VK_CONTROL) & 0x8000) meta |= 0x1000; // META_CTRL_ON
    DispatchKey(down ? 0 : 1, code, unicode, meta);
    return static_cast<wchar_t>(unicode);
}

/// Sends a typed UTF-16 code unit that no mapped key delivered: an input method's commit, a
/// dead-key composition, a key with no Android keycode, a character sent as a packet.
/// Notes: upstream's shape for composed text (cocoa_bridge.mm insertText): KEYCODE_UNKNOWN
/// with the character, DOWN then UP, one pair per code unit, so a surrogate pair reassembles
/// in the field's buffer. Control characters come only as keys.
void Text(wchar_t c) {
    if (!g_touch || c < 0x20 || c == 0x7f) return;
    DispatchKey(0, 0, c, 0);
    DispatchKey(1, 0, c, 0);
}

/// Calls ActivityHost.macapkTaskToFront: the app's window came back from the taskbar, which
/// on Windows is where moveTaskToBack sends it. Notes: a no-op in Java unless the runtime
/// sent the task back (upstream calls it when the Mac app is unhidden).
void TaskToFront() {
    JNIEnv* env = Env();
    static jclass cls = [&] {
        jclass c = env->FindClass("com/macapk/loader/ActivityHost");
        jclass g = c ? static_cast<jclass>(env->NewGlobalRef(c)) : nullptr;
        if (c) env->DeleteLocalRef(c);
        return g;
    }();
    static jmethodID mid = cls ? env->GetStaticMethodID(cls, "macapkTaskToFront", "()V") : nullptr;
    if (!mid) { env->ExceptionClear(); return; }
    env->CallStaticVoidMethod(cls, mid);
    CheckCallback(env, "ActivityHost.macapkTaskToFront");
}

/// Closes the app on a graphics error rather than pretending a bitmap fallback drew it.
void FailGraphics(Window* w, const std::string& category, const std::string& detail) {
    LogLine(category + ": Java GLES " + detail);
    g_graphicsFailed = true;
    w->closingGraphics = true;
    PostMessageW(w->hwnd, WM_CLOSE, 0, 0);
}

/// Runs one full-client Java GLES frame, owning the current context only during callbacks.
void TickGraphics(Window* w, View* v, JNIEnv* env, jclass cls) {
    if (w->minimized || w->inGraphicsFrame || w->closingGraphics) return;
    RECT rc{};
    if (!GetClientRect(w->hwnd, &rc) || rc.right <= 0 || rc.bottom <= 0) return;
    static jmethodID requested = nullptr, draw = nullptr;
    if (!requested || !draw) {
        requested = env->GetMethodID(cls, "macapkWindowsClientVersion", "()I");
        if (requested) draw = env->GetMethodID(cls, "macapkWindowsDrawFrame", "(IIZZ)Z");
        if (!requested || !draw) {
            CheckCallback(env, "bind Windows GLSurfaceView");
            FailGraphics(w, "GRAPHICS_CAPABILITY_MISSING", "Windows GLSurfaceView callbacks are missing");
            return;
        }
    }
    const jint version = env->CallIntMethod(v->renderer, requested);
    if (env->ExceptionCheck()) {
        CheckCallback(env, "macapkWindowsClientVersion");
        FailGraphics(w, "GRAPHICS_RENDER_FAILED", "client version callback failed");
        return;
    }
    if (version == -1) return; // The Activity has not installed its Renderer yet.
    if (version != 2 || g_graphicsRequest == "off") {
        const char* detail = version == -2 ? "custom EGL configuration is not implemented" :
            version != 2 ? "only GLES2 is implemented" : "use --graphics=auto, vulkan or d3d11";
        FailGraphics(w, "GRAPHICS_CAPABILITY_MISSING", detail);
        return;
    }
    w->inGraphicsFrame = true;
    std::string error;
    const bool fresh = !w->graphics;
    if (fresh) {
        // A second live HWND would share an EGLDisplay; don't terminate another owner's display.
        for (const auto& [handle, other] : g_windows) {
            if (handle != w->hwnd && other->graphics) {
                w->inGraphicsFrame = false;
                FailGraphics(w, "GRAPHICS_CAPABILITY_MISSING", "only one EGL window is supported");
                return;
            }
        }
        w->graphics = JavaGraphics::Open(w->hwnd, g_graphicsRequest, error);
        w->graphicsView = v;
        w->graphicsRevision = v->rendererRevision;
    }
    if (!w->graphics) {
        w->inGraphicsFrame = false;
        FailGraphics(w, "GRAPHICS_CAPABILITY_MISSING", error);
        return;
    }
    if (!w->graphics->Begin(rc.right, rc.bottom, error)) {
        w->inGraphicsFrame = false;
        FailGraphics(w, "GRAPHICS_RENDER_FAILED", error);
        return;
    }
    const bool exposed = w->graphicsExposed;
    w->graphicsExposed = false;
    const bool rendered = env->CallBooleanMethod(v->renderer, draw, static_cast<jint>(rc.right),
        static_cast<jint>(rc.bottom), static_cast<jboolean>(fresh), static_cast<jboolean>(exposed));
    const bool threw = env->ExceptionCheck();
    if (threw) CheckCallback(env, "macapkWindowsDrawFrame");
    const bool stillCurrent = w->content == v && w->graphicsRevision == v->rendererRevision && !w->closingGraphics;
    const bool ended = w->graphics->End(rendered && !threw && stillCurrent, error);
    w->inGraphicsFrame = false;
    if (!ended || threw)
        FailGraphics(w, "GRAPHICS_RENDER_FAILED", threw ? "renderer callback failed" : error);
    else if (w->closingGraphics)
        PostMessageW(w->hwnd, WM_CLOSE, 0, 0);
}

/// Runs one frame of `w`'s view: reports a new size, then asks the renderer to draw.
/// Notes: a minimised window has no client area; it keeps its last size and draws at 2 Hz.
void Tick(Window* w) {
    View* v = w->content;
    if (w->inGraphicsFrame || w->closingGraphics || g_guestGraphicsCommand) return;
    if (w->graphics && (v != w->graphicsView || v->rendererRevision != w->graphicsRevision)) {
        w->graphics.reset();
        w->graphicsView = nullptr;
        InvalidateRect(w->hwnd, nullptr, FALSE);
    }
    if (!v || !v->renderer) return;
    JNIEnv* env = Env();
    static const jclass glView = [&] {
        jclass local = env->FindClass("android/opengl/GLSurfaceView");
        if (!local) { CheckCallback(env, "load GLSurfaceView"); return static_cast<jclass>(nullptr); }
        jclass global = static_cast<jclass>(env->NewGlobalRef(local));
        env->DeleteLocalRef(local);
        return global;
    }();
    if (glView && env->IsInstanceOf(v->renderer, glView)) {
        if (GuestGraphicsActive() || GuestVulkanOwnsWindow()) {
            FailGraphics(w, "GRAPHICS_CAPABILITY_MISSING", "guest graphics already owns the window");
            return;
        }
        TickGraphics(w, v, env, glView);
        return;
    }
    if (w->minimized) {
        const ULONGLONG now = GetTickCount64();
        if (now - w->lastHiddenFrame < kHiddenFrameMs) return;
        w->lastHiddenFrame = now;
    }
    static jmethodID changed = nullptr, draw = nullptr;
    if (!draw) {
        jclass c = env->FindClass("com/macapk/loader/MetalSurfaceCallbacks");
        changed = env->GetMethodID(c, "macapkSurfaceChanged", "(II)V");
        draw = env->GetMethodID(c, "macapkDrawFrame", "()V");
        env->DeleteLocalRef(c);
    }
    RECT rc;
    GetClientRect(w->hwnd, &rc);
    if (!w->minimized && rc.right > 0 && rc.bottom > 0 && (rc.right != v->sizedW || rc.bottom != v->sizedH)) {
        v->sizedW = rc.right;
        v->sizedH = rc.bottom;
        env->CallVoidMethod(v->renderer, changed, static_cast<jint>(rc.right), static_cast<jint>(rc.bottom));
        CheckCallback(env, "macapkSurfaceChanged");
    }
    if (v->sizedW <= 0) return;  // never sized: nothing to draw into yet
    env->CallVoidMethod(v->renderer, draw);
    CheckCallback(env, "macapkDrawFrame");
}

/// Paints the view's last frame, scaled to the client area.
/// Notes: the frame is opaque ARGB; its bytes are BGRA in memory, the layout of a 32-bit
/// top-down DIB, so it is drawn without conversion. Alpha is not composited.
void Paint(Window* w) {
    PAINTSTRUCT ps;
    HDC dc = BeginPaint(w->hwnd, &ps);
    if ((w->graphics && w->graphicsView == w->content) || GuestGraphicsPresents(w->hwnd) ||
        GuestVulkanPresents(w->hwnd)) {
        EndPaint(w->hwnd, &ps);
        return;
    }
    RECT rc;
    GetClientRect(w->hwnd, &rc);
    View* v = w->content;
    bool drawn = false;
    if (v) {
        std::lock_guard<std::mutex> lock(v->mu);
        if (v->frameW > 0 && v->frameH > 0) {
            BITMAPINFO bi{};
            bi.bmiHeader.biSize = sizeof(bi.bmiHeader);
            bi.bmiHeader.biWidth = v->frameW;
            bi.bmiHeader.biHeight = -v->frameH;  // top-down
            bi.bmiHeader.biPlanes = 1;
            bi.bmiHeader.biBitCount = 32;
            bi.bmiHeader.biCompression = BI_RGB;
            SetStretchBltMode(dc, HALFTONE);
            StretchDIBits(dc, 0, 0, rc.right, rc.bottom, 0, 0, v->frameW, v->frameH, v->pixels.data(), &bi,
                          DIB_RGB_COLORS, SRCCOPY);
            drawn = true;
        }
    }
    if (!drawn) {
        HBRUSH b = CreateSolidBrush(w->background);
        FillRect(dc, &rc, b);
        DeleteObject(b);
    }
    EndPaint(w->hwnd, &ps);
}

/// Logs a focus change, at most kFocusLogLines times (upstream's [focus] trace).
void FocusLog(const char* what) {
    static int lines = 0;
    if (++lines <= kFocusLogLines) LogLine(std::string("[winapk] window: focus ") + what);
}

/// Handles an app window's messages.
LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    auto it = g_windows.find(hwnd);
    Window* w = it == g_windows.end() ? nullptr : it->second;
    if (!w) return DefWindowProcW(hwnd, msg, wp, lp);
    switch (msg) {
        case WM_PAINT: Paint(w); return 0;
        case WM_ERASEBKGND: return 1;
        case WM_TIMER: if (wp == kFrameTimer) Tick(w); return 0;
        case kEmbedWindowMessage: PumpEmbedCommands(); return 0;
        case WM_LBUTTONDOWN:
            SetCapture(hwnd);
            w->pointerDown = true;
            w->lastPointer = lp;
            Touch(w, kActionDown, lp);
            return 0;
        case WM_MOUSEMOVE:
            // Only a move with the button held drags; with the mouse captured, other moves
            // (the cursor passing by) must not turn a tap into a drag.
            if (w->pointerDown && (wp & MK_LBUTTON)) { w->lastPointer = lp; Touch(w, kActionMove, lp); }
            return 0;
        case WM_LBUTTONUP:
            if (w->pointerDown) {
                w->pointerDown = false;
                ReleaseCapture();
                Touch(w, kActionUp, lp);
            }
            return 0;
        case WM_CAPTURECHANGED:
            // Another window took the mouse mid-press (a system dialog, Alt+Tab).
            if (reinterpret_cast<HWND>(lp) != hwnd) CancelPointer(w);
            return 0;
        case WM_MOUSEWHEEL: Wheel(w, wp, lp, false); return 0;
        case WM_MOUSEHWHEEL: Wheel(w, wp, lp, true); return 0;
        case WM_KEYDOWN:
            if (const wchar_t c = Key(true, wp, lp)) {
                if (w->keyChars.size() == 8) w->keyChars.pop_front();  // a WM_CHAR that never came
                w->keyChars.push_back(c);
            }
            return 0;
        case WM_SYSKEYDOWN:
            if (wp != VK_F4) Key(true, wp, lp);  // Alt+F4 still closes the window
            break;
        case WM_KEYUP:
            Key(false, wp, lp);
            return 0;
        case WM_SYSKEYUP: Key(false, wp, lp); break;
        case WM_CHAR:
            // TranslateMessage's character for a key: already sent with it when the key is
            // mapped, else sent here (IME commits arrive this way too). Matched by value, not
            // by order: the key's WM_KEYUP may be dispatched before its WM_CHAR.
            if (!w->keyChars.empty() && w->keyChars.front() == static_cast<wchar_t>(wp)) w->keyChars.pop_front();
            else Text(static_cast<wchar_t>(wp));
            return 0;
        case WM_ACTIVATE:
            FocusLog(LOWORD(wp) == WA_INACTIVE ? "lost" : "gained");
            if (LOWORD(wp) == WA_INACTIVE) CancelPointer(w);
            break;
        case WM_SIZE:
            if (wp == SIZE_MINIMIZED) ResizeSurface(hwnd, 0, 0);
            else {
                RECT client{};
                if (GetClientRect(hwnd, &client)) ResizeSurface(hwnd, client.right, client.bottom);
            }
            if (wp == SIZE_MINIMIZED && !w->minimized) {
                w->minimized = true;
                w->lastHiddenFrame = 0;
                LogLine(w->graphics ? "[winapk] window: minimised; Java GLES suspended" :
                    "[winapk] window: minimised; drawing at 2 Hz");
            } else if (wp != SIZE_MINIMIZED && w->minimized) {
                w->minimized = false;
                w->graphicsExposed = static_cast<bool>(w->graphics);
                LogLine("[winapk] window: restored; drawing at full rate");
                TaskToFront();
            }
            return 0;
        case WM_SIZING: {
            // Keep the app's max aspect ratio while the person drags an edge.
            const float cap = g_maxAspect;
            if (cap <= 0) break;
            RECT* r = reinterpret_cast<RECT*>(lp);
            RECT frame{0, 0, 0, 0};
            AdjustWindowRectExForDpi(&frame, WS_OVERLAPPEDWINDOW, FALSE, 0, GetDpiForWindow(hwnd));
            const LONG fw = frame.right - frame.left, fh = frame.bottom - frame.top;
            const LONG cw = r->right - r->left - fw, ch = r->bottom - r->top - fh;
            const LONG longer = cw > ch ? cw : ch, shorter = cw > ch ? ch : cw;
            if (shorter > 0 && longer > shorter * cap) {
                const LONG fixed = static_cast<LONG>(shorter * cap);
                if (cw > ch) r->right = r->left + fixed + fw; else r->bottom = r->top + fixed + fh;
            }
            return TRUE;
        }
        case WM_DPICHANGED: {
            const RECT* r = reinterpret_cast<const RECT*>(lp);
            SetWindowPos(hwnd, nullptr, r->left, r->top, r->right - r->left, r->bottom - r->top, SWP_NOZORDER | SWP_NOACTIVATE);
            return 0;
        }
        case WM_CLOSE:
            // The person closed the app: end the loop; Boot.run returns and the process exits.
            if (g_guestGraphicsCommand) {
                g_guestDeferredClose = hwnd;
                return 0;
            }
            if (w->inGraphicsFrame) {
                w->closingGraphics = true;
                return 0;
            }
            w->closingGraphics = true;
            RetireGuestGraphics(hwnd, true);
            RetireGuestVulkan(hwnd, true);
            w->graphics.reset();
            DestroyWindow(hwnd);
            return 0;
        case WM_DESTROY:
            KillTimer(hwnd, kFrameTimer);
            RetireSurface(hwnd);
            RetireGuestGraphics(hwnd, true);
            RetireGuestVulkan(hwnd, true);
            g_windows.erase(hwnd);
            delete w;
            PostQuitMessage(0);
            return 0;
        default: break;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

/// Runs functions posted by RunOnUi from other threads, unless their caller gave up.
LRESULT CALLBACK DispatcherProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg != kCallMessage) return DefWindowProcW(hwnd, msg, wp, lp);
    auto* p = reinterpret_cast<std::shared_ptr<UiCall>*>(lp);
    std::shared_ptr<UiCall> call = std::move(*p);
    delete p;
    {
        std::lock_guard<std::mutex> lock(call->mu);
        if (call->state == UiCall::kAbandoned) return 0;
        call->state = UiCall::kRunning;
    }
    (*call->fn)();
    {
        std::lock_guard<std::mutex> lock(call->mu);
        call->state = UiCall::kDone;
    }
    call->cv.notify_all();
    return 0;
}

/// Returns the window a Java handle names, or null (UI thread only).
Window* FromHandle(jlong h) {
    auto it = g_windows.find(reinterpret_cast<HWND>(h));
    return it == g_windows.end() ? nullptr : it->second;
}

// ---- natives ------------------------------------------------------------------------------

/// Makes the calling thread the UI thread: per-monitor DPI awareness, the window classes and
/// the dispatcher. Returns false when Windows refuses a class.
jboolean JNICALL Init(JNIEnv* env, jclass) {
    env->GetJavaVM(&g_vm);
    if (jclass sys = env->FindClass("java/lang/System")) {
        const auto property = env->GetStaticMethodID(sys, "getProperty", "(Ljava/lang/String;)Ljava/lang/String;");
        jstring key = env->NewStringUTF("winapk.graphics");
        auto mode = static_cast<jstring>(env->CallStaticObjectMethod(sys, property, key));
        if (!env->ExceptionCheck() && mode) g_graphicsRequest = Utf8(env, mode);
        if (mode) env->DeleteLocalRef(mode);
        env->DeleteLocalRef(key);
        env->DeleteLocalRef(sys);
        if (env->ExceptionCheck()) return JNI_FALSE;
    } else return JNI_FALSE;
    g_uiThread = GetCurrentThreadId();
    SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    HINSTANCE inst = GetModuleHandleW(nullptr);
    WNDCLASSEXW wc{sizeof wc};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = inst;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.lpszClassName = L"WinApkAppWindow";
    wc.style = CS_HREDRAW | CS_VREDRAW;
    if (!RegisterClassExW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) return JNI_FALSE;
    WNDCLASSEXW dc{sizeof dc};
    dc.lpfnWndProc = DispatcherProc;
    dc.hInstance = inst;
    dc.lpszClassName = L"WinApkDispatcher";
    if (!RegisterClassExW(&dc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) return JNI_FALSE;
    g_dispatcher = CreateWindowExW(0, dc.lpszClassName, L"", 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr, inst, nullptr);
    return g_dispatcher ? JNI_TRUE : JNI_FALSE;
}

/// Sets the app's size in points (the window's client area at 100% is this many pixels).
void JNICALL SetPhoneSize(JNIEnv*, jclass, jint w, jint h) {
    if (w > 0 && h > 0) { g_phoneW = w; g_phoneH = h; g_userLandscape = w > h; }
}

jint JNICALL PhoneWidth(JNIEnv*, jclass) { return g_phoneW; }
jint JNICALL PhoneHeight(JNIEnv*, jclass) { return g_phoneH; }

/// Returns the DPI scale of the app window (of the system before there is one).
jfloat JNICALL BackingScale(JNIEnv*, jclass) {
    View* v = g_current;
    HWND hwnd = nullptr;
    if (v) for (auto& [h, w] : g_windows) if (w->content == v) hwnd = h;
    return Scale(hwnd);
}

/// Returns the refresh rate of the primary display, in Hz.
jint JNICALL DisplayHz(JNIEnv*, jclass) {
    DEVMODEW dm{};
    dm.dmSize = sizeof dm;
    if (EnumDisplaySettingsW(nullptr, ENUM_CURRENT_SETTINGS, &dm) && dm.dmDisplayFrequency > 1) return dm.dmDisplayFrequency;
    return 60;
}

/// Creates a hidden app window sized to the app; returns its handle (0 when it failed).
jlong JNICALL CreateWindowNative(JNIEnv* env, jclass, jstring title) {
    const std::wstring t = Wide(Utf8(env, title));
    HWND result = nullptr;
    RunOnUi("createWindow", [&] {
        const float s = Scale(nullptr);
        HWND hwnd = nullptr;
        if (IsEmbedded()) {
            const int pw = std::max(1, static_cast<int>(std::lround(g_phoneW * s)));
            const int ph = std::max(1, static_cast<int>(std::lround(g_phoneH * s)));
            hwnd = CreateWindowExW(WS_EX_TOOLWINDOW, L"WinApkAppWindow", t.c_str(), WS_POPUP,
                                   -32000, -32000, pw, ph, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
        } else {
            RECT r{0, 0, static_cast<LONG>(g_phoneW * s), static_cast<LONG>(g_phoneH * s)};
            AdjustWindowRectExForDpi(&r, WS_OVERLAPPEDWINDOW, FALSE, 0, DefaultDpi());
            hwnd = CreateWindowExW(0, L"WinApkAppWindow", t.c_str(), WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
                                   r.right - r.left, r.bottom - r.top, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
        }
        if (!hwnd) return;
        auto* win = new Window{hwnd};
        g_windows[hwnd] = win;
        SetTimer(hwnd, kFrameTimer, 16, nullptr);
        if (IsEmbedded()) {
            SetEmbedDispatcher(hwnd, [hwnd](const EmbedCmd& cmd) {
                auto it = g_windows.find(hwnd);
                if (it == g_windows.end()) return;
                Window* w = it->second;
                RECT rc{};
                GetClientRect(hwnd, &rc);
                const float sc = Scale(hwnd);
                const float vw = rc.right > 0 ? rc.right / sc : static_cast<float>(g_phoneW);
                const float vh = rc.bottom > 0 ? rc.bottom / sc : static_cast<float>(g_phoneH);
                switch (cmd.kind) {
                    case EmbedCmdKind::Resize: {
                        g_phoneW = cmd.w;
                        g_phoneH = cmd.h;
                        g_userLandscape = cmd.w > cmd.h;
                        const int pw = std::max(1, static_cast<int>(std::lround(cmd.w * sc)));
                        const int ph = std::max(1, static_cast<int>(std::lround(cmd.h * sc)));
                        SetWindowPos(hwnd, nullptr, -32000, -32000, pw, ph, SWP_NOZORDER | SWP_NOACTIVATE);
                        Tick(w);
                        break;
                    }
                    case EmbedCmdKind::Pointer:
                        if (g_touch) {
                            JNIEnv* e = Env();
                            static jmethodID mid = TouchMethod(e, "dispatch", "(IFFFF)V");
                            if (mid) {
                                e->CallVoidMethod(g_touch, mid, cmd.action, cmd.x, vh - cmd.y, vw, vh);
                                CheckCallback(e, "TouchDispatcher.dispatch");
                            }
                        }
                        break;
                    case EmbedCmdKind::Wheel:
                        if (g_touch) {
                            JNIEnv* e = Env();
                            static jmethodID mid = TouchMethod(e, "dispatchWheel", "(FFFFFF)Z");
                            if (mid) {
                                e->CallBooleanMethod(g_touch, mid, cmd.x, vh - cmd.y, vw, vh, cmd.dy, cmd.dx);
                                CheckCallback(e, "TouchDispatcher.dispatchWheel");
                            }
                        }
                        break;
                    case EmbedCmdKind::Key: {
                        const int code = AndroidKey(static_cast<WPARAM>(cmd.w));
                        if (g_touch && code != 0) DispatchKey(cmd.action ? 0 : 1, code, cmd.h, cmd.meta);
                        break;
                    }
                    case EmbedCmdKind::Text:
                        for (wchar_t ch : cmd.text) Text(ch);
                        break;
                    case EmbedCmdKind::Back:
                        if (g_touch) { DispatchKey(0, 4, 0, 0); DispatchKey(1, 4, 0, 0); }
                        break;
                    case EmbedCmdKind::Stop:
                        SendMessageW(hwnd, WM_CLOSE, 0, 0);
                        break;
                }
            });
        }
        result = hwnd;
    });
    return reinterpret_cast<jlong>(result);
}

/// Creates a canvas view of `w` × `h` points; returns its handle.
jlong JNICALL CreateSurfaceView(JNIEnv*, jclass, jint w, jint h) {
    auto* v = new View;
    v->widthPt = w;
    v->heightPt = h;
    std::lock_guard<std::mutex> lock(g_viewsMu);
    g_views.push_back(v);
    return reinterpret_cast<jlong>(v);
}

/// Makes `view` the content of `window` and the target of presentBitmap.
void JNICALL SetContentView(JNIEnv*, jclass, jlong window, jlong view) {
    RunOnUi("setContentView", [&] {
        if (Window* w = FromHandle(window)) {
            if (w->content != reinterpret_cast<View*>(view)) {
                RetireSurface(w->hwnd);
                RetireGuestGraphics(w->hwnd);
                RetireGuestVulkan(w->hwnd);
            }
            w->content = reinterpret_cast<View*>(view);
            g_current = w->content;
            PublishWindowSurface(w);
            if (w->content) w->content->sizedW = w->content->sizedH = -1;
            InvalidateRect(w->hwnd, nullptr, FALSE);
        }
    });
}

void JNICALL ShowWindowNative(JNIEnv*, jclass, jlong window) {
    RunOnUi("showWindow", [&] {
        if (Window* w = FromHandle(window)) {
            if (IsEmbedded()) {
                SendEmbedEvent("state running");
                Tick(w);
            } else {
                ShowWindow(w->hwnd, SW_SHOWNORMAL);
                SetForegroundWindow(w->hwnd);
            }
        }
    });
}

void JNICALL CloseWindowNative(JNIEnv*, jclass, jlong window) {
    RunOnUi("closeWindow", [&] { if (Window* w = FromHandle(window)) SendMessageW(w->hwnd, WM_CLOSE, 0, 0); });
}

void JNICALL SetWindowTitle(JNIEnv* env, jclass, jlong window, jstring title) {
    const std::wstring t = Wide(Utf8(env, title));
    RunOnUi("setWindowTitle", [&] { if (Window* w = FromHandle(window)) SetWindowTextW(w->hwnd, t.c_str()); });
}

/// Sets the color shown before the first frame.
void JNICALL SetStartingWindowColor(JNIEnv*, jclass, jint argb) {
    const COLORREF c = RGB((argb >> 16) & 0xff, (argb >> 8) & 0xff, argb & 0xff);
    RunOnUi("setStartingWindowColor", [&] { for (auto& [h, w] : g_windows) { w->background = c; InvalidateRect(h, nullptr, FALSE); } });
}

/// Attaches the renderer that draws `view` (a MetalSurfaceCallbacks).
void JNICALL SetRenderer(JNIEnv* env, jclass, jlong view, jobject callbacks) {
    auto* v = reinterpret_cast<View*>(view);
    if (!v) return;
    jobject surface = nullptr;
    jfieldID released = nullptr;
    if (callbacks) {
        jclass cls = env->FindClass("android/view/SurfaceView");
        if (!cls) return;
        const bool isSurface = env->IsInstanceOf(callbacks, cls);
        if (isSurface) {
            const auto holder = env->GetMethodID(cls, "macapkWindowsHolderSurface", "()Landroid/view/Surface;");
            if (holder) {
                jobject value = env->CallNonvirtualObjectMethod(callbacks, cls, holder);
                if (!env->ExceptionCheck() && value) {
                    jclass type = env->FindClass("android/view/Surface");
                    if (type) {
                        released = env->GetFieldID(type, "mReleased", "Z");
                        if (released) surface = env->NewGlobalRef(value);
                        env->DeleteLocalRef(type);
                    }
                    env->DeleteLocalRef(value);
                }
            }
        }
        env->DeleteLocalRef(cls);
        if (env->ExceptionCheck()) return;
    }
    jobject ref = callbacks ? env->NewGlobalRef(callbacks) : nullptr;
    if (callbacks && !ref) {
        if (surface) env->DeleteGlobalRef(surface);
        return;
    }
    const bool ran = RunOnUi("setRenderer", [&] {
        JNIEnv* ui = Env();
        const bool sameSurface = surface && v->holderSurface && ui->IsSameObject(surface, v->holderSurface);
        if (v->holderSurface && !sameSurface) {
            for (auto& [hwnd, w] : g_windows) if (w->content == v) {
                RetireSurface(hwnd);
                RetireGuestGraphics(hwnd);
                RetireGuestVulkan(hwnd);
            }
            ui->DeleteGlobalRef(v->holderSurface);
        }
        if (sameSurface) ui->DeleteGlobalRef(surface);
        else {
            v->holderSurface = surface;
            v->surfaceReleased = released;
        }
        if (v->renderer) ui->DeleteGlobalRef(v->renderer);
        v->renderer = ref;
        ++v->rendererRevision;
        v->sizedW = v->sizedH = -1;
        if (!sameSurface && v->holderSurface)
            for (auto& [hwnd, w] : g_windows) if (w->content == v) PublishWindowSurface(w);
    });
    if (!ran) {
        if (ref) env->DeleteGlobalRef(ref);
        if (surface) env->DeleteGlobalRef(surface);
    }
}

/// Detaches `view`'s renderer.
void JNICALL ReleaseRenderer(JNIEnv*, jclass, jlong view) {
    auto* v = reinterpret_cast<View*>(view);
    if (!v) return;
    RunOnUi("releaseRenderer", [&] {
        if (v->holderSurface) {
            for (auto& [hwnd, w] : g_windows) if (w->content == v) {
                RetireSurface(hwnd);
                RetireGuestGraphics(hwnd);
                RetireGuestVulkan(hwnd);
            }
            Env()->DeleteGlobalRef(v->holderSurface);
            v->holderSurface = nullptr;
            v->surfaceReleased = nullptr;
        }
        if (v->renderer) Env()->DeleteGlobalRef(v->renderer);
        v->renderer = nullptr;
        ++v->rendererRevision;
    });
}

void JNICALL RegisterTouchDispatcher(JNIEnv* env, jclass, jobject dispatcher) {
    jobject ref = dispatcher ? env->NewGlobalRef(dispatcher) : nullptr;
    const bool ran = RunOnUi("registerTouchDispatcher", [&] {
        if (g_touch) Env()->DeleteGlobalRef(g_touch);
        g_touch = ref;
    });
    if (!ran && ref) env->DeleteGlobalRef(ref);
}

/// Copies a frame (ARGB, top-down, `w` × `h`) to the current view and repaints it.
/// Notes: `argb` is read only during the call (upstream's contract).
void JNICALL PresentBitmap(JNIEnv* env, jclass, jintArray argb, jint w, jint h) {
    View* v = g_current;
    if (!v || !argb || w <= 0 || h <= 0 || env->GetArrayLength(argb) < static_cast<jsize>(w) * h) return;
    {
        std::lock_guard<std::mutex> lock(v->mu);
        v->pixels.resize(static_cast<size_t>(w) * h);
        env->GetIntArrayRegion(argb, 0, w * h, reinterpret_cast<jint*>(v->pixels.data()));
        v->frameW = w;
        v->frameH = h;
        if (IsEmbedded()) {
            SendEmbedEvent("backend java2d");
            PublishEmbedArgb(v->pixels.data(), w, h);
        }
    }
    // InvalidateRect is safe from any thread.
    for (auto& [hwnd, win] : g_windows) if (win->content == v) InvalidateRect(hwnd, nullptr, FALSE);
}

/// Runs the UI thread's message loop until the app's window closes.
/// Side effects: from its start, RunOnUi's waits are limited; after it ends, RunOnUi fails at
/// once and waiting callers give up.
void JNICALL RunLoop(JNIEnv* env, jclass) {
    g_loop = 1;
    MSG m;
    while (GetMessageW(&m, nullptr, 0, 0) > 0) {
        TranslateMessage(&m);
        DispatchMessageW(&m);
    }
    g_loop = 2;
    if (g_graphicsFailed) {
        jclass error = env->FindClass("java/lang/IllegalStateException");
        if (error) {
            env->ThrowNew(error, "Windows Java GLES failed; see graphics diagnostic");
            env->DeleteLocalRef(error);
        }
    }
}

/// Shapes every app window for `landscape` (wider than tall) or portrait, keeping its area.
void Orient(bool landscape) {
    const int w = g_phoneW, h = g_phoneH;
    if ((w > h) != landscape && w != h) { g_phoneW = h; g_phoneH = w; }
    RunOnUi("orientation", [] {
        for (auto& [hwnd, win] : g_windows) {
            RECT rc;
            GetClientRect(hwnd, &rc);
            const bool wide = rc.right > rc.bottom;
            if (wide == (g_phoneW > g_phoneH) || rc.right == rc.bottom) continue;
            RECT r{0, 0, rc.bottom, rc.right};
            if (!IsEmbedded()) AdjustWindowRectExForDpi(&r, WS_OVERLAPPEDWINDOW, FALSE, 0, GetDpiForWindow(hwnd));
            SetWindowPos(hwnd, nullptr, 0, 0, r.right - r.left, r.bottom - r.top, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
        }
    });
}

/// Applies Activity.setRequestedOrientation: landscape or portrait.
void JNICALL ApplyRequestedOrientation(JNIEnv*, jclass, jboolean landscape) { Orient(landscape); }

/// Applies an ActivityInfo.SCREEN_ORIENTATION_* value: the landscape and portrait families
/// shape the window; any other value returns to the user rotation (a PC has no sensor and is
/// modelled as rotation-locked, as upstream models a Mac).
void JNICALL ApplyWindowOrientation(JNIEnv*, jclass, jint o) {
    switch (o) {
        case 0: case 6: case 8: case 11: Orient(true); break;   // LANDSCAPE, SENSOR_, REVERSE_, USER_
        case 1: case 7: case 9: case 12: Orient(false); break;  // PORTRAIT, SENSOR_, REVERSE_, USER_
        default: Orient(g_userLandscape); break;
    }
}

void JNICALL RestoreUserOrientation(JNIEnv*, jclass) { Orient(g_userLandscape); }

/// Caps the window's aspect ratio (AOSP's pre-O max aspect ratio); 0 removes the cap.
void JNICALL SetMaxAspectRatio(JNIEnv*, jclass, jfloat ratio) { g_maxAspect = ratio >= 1.0f ? ratio : 0.0f; }

/// There is no placeholder window on Windows: the app's window is created by createWindow.
jlong JNICALL GetPlaceholderWindow(JNIEnv*, jclass) { return 0; }

/// Keeps FLAG_SECURE windows out of screenshots and screen sharing.
void JNICALL SetSecureContent(JNIEnv*, jclass, jboolean secure) {
    RunOnUi("setSecureContent", [&] { for (auto& [hwnd, w] : g_windows) SetWindowDisplayAffinity(hwnd, secure ? WDA_EXCLUDEFROMCAPTURE : WDA_NONE); });
}

/// Minimises the app's windows (Activity.moveTaskToBack); restoring one brings the task back
/// (WM_SIZE → ActivityHost.macapkTaskToFront).
void JNICALL MoveTaskToBack(JNIEnv*, jclass) {
    RunOnUi("moveTaskToBack", [] { for (auto& [hwnd, w] : g_windows) ShowWindow(hwnd, SW_MINIMIZE); });
}

/// Restores and raises the app's windows.
void JNICALL MoveTaskToFront(JNIEnv*, jclass) {
    RunOnUi("moveTaskToFront", [] { for (auto& [hwnd, w] : g_windows) { ShowWindow(hwnd, SW_RESTORE); SetForegroundWindow(hwnd); } });
}

/// Records the app's data subdirectory and native library folder for the guest loader.
void JNICALL SetDataSubdir(JNIEnv* env, jclass, jstring dir) { std::lock_guard<std::mutex> l(g_dirsMu); g_dataSubdir = Utf8(env, dir); }
void JNICALL SetNativeLibDir(JNIEnv* env, jclass, jstring dir) { std::lock_guard<std::mutex> l(g_dirsMu); g_nativeLibDir = Utf8(env, dir); }

/// Ends the process now with `code`, as upstream's native _exit does (no JVM shutdown).
/// Notes: a relaunch request is not supported on Windows yet and is logged.
void JNICALL ExitProcessNative(JNIEnv* env, jclass, jint code, jstring relaunch) {
    if (relaunch) LogLine("[winapk] window: relaunch after exit is not supported; exiting");
    (void)env;
    std::fflush(nullptr);
    TerminateProcess(GetCurrentProcess(), static_cast<UINT>(code));
}

#define M(name, sig, fn) {const_cast<char*>(name), const_cast<char*>(sig), reinterpret_cast<void*>(fn)}
const JNINativeMethod kMethods[] = {
    M("init", "()Z", Init),
    M("setPhoneSize", "(II)V", SetPhoneSize),
    M("phoneWidth", "()I", PhoneWidth),
    M("phoneHeight", "()I", PhoneHeight),
    M("backingScale", "()F", BackingScale),
    M("displayHz", "()I", DisplayHz),
    M("createWindow", "(Ljava/lang/String;)J", CreateWindowNative),
    M("createMetalSurfaceView", "(II)J", CreateSurfaceView),
    M("setContentView", "(JJ)V", SetContentView),
    M("showWindow", "(J)V", ShowWindowNative),
    M("closeWindow", "(J)V", CloseWindowNative),
    M("setWindowTitle", "(JLjava/lang/String;)V", SetWindowTitle),
    M("setStartingWindowColor", "(I)V", SetStartingWindowColor),
    M("setRenderer", "(JLcom/macapk/loader/MetalSurfaceCallbacks;)V", SetRenderer),
    M("releaseRenderer", "(J)V", ReleaseRenderer),
    M("registerTouchDispatcher", "(Lcom/macapk/loader/TouchDispatcher;)V", RegisterTouchDispatcher),
    M("presentBitmap", "([III)V", PresentBitmap),
    M("runLoop", "()V", RunLoop),
    M("exitProcess", "(ILjava/lang/String;)V", ExitProcessNative),
    M("applyRequestedOrientation", "(Z)V", ApplyRequestedOrientation),
    M("applyWindowOrientation", "(I)V", ApplyWindowOrientation),
    M("restoreUserOrientation", "()V", RestoreUserOrientation),
    M("setMaxAspectRatio", "(F)V", SetMaxAspectRatio),
    M("getPlaceholderWindow", "()J", GetPlaceholderWindow),
    M("setSecureContent", "(Z)V", SetSecureContent),
    M("moveTaskToBack", "()V", MoveTaskToBack),
    M("moveTaskToFront", "()V", MoveTaskToFront),
    M("setDataSubdir", "(Ljava/lang/String;)V", SetDataSubdir),
    M("setNativeLibDir", "(Ljava/lang/String;)V", SetNativeLibDir),
};
#undef M

}  // namespace

bool RegisterWindow(JNIEnv* env) {
    return Bind(env, "com/macapk/loader/NativeCocoa", kMethods, static_cast<int>(std::size(kMethods)));
}

bool DispatchGuestGraphics(const std::function<void()>& command) {
    if (g_loop != 1) return false; // The caller may otherwise block Activity startup or shutdown.
    bool ran = false;
    return RunOnUi("guest graphics", [&] {
        if (g_guestGraphicsCommand) return;
        g_guestGraphicsCommand = true;
        try {
            command();
            ran = true;
        } catch (const std::bad_alloc&) {
            LogLine("GRAPHICS_RENDER_FAILED: guest graphics allocation failed");
        }
        g_guestGraphicsCommand = false;
        if (g_guestDeferredClose) {
            PostMessageW(g_guestDeferredClose, WM_CLOSE, 0, 0);
            g_guestDeferredClose = nullptr;
        }
    }) && ran;
}

const std::string& GuestGraphicsRequest() {
    static const std::string off = "off";
    for (const auto& [hwnd, w] : g_windows)
        if (w->graphics || w->inGraphicsFrame || w->closingGraphics) return off;
    return g_graphicsRequest;
}

}  // namespace winapk::bridge

extern "C" __declspec(dllexport) void winapk_bridge_set_phone_size(int w, int h) {
    using namespace winapk::bridge;
    if (w > 0 && h > 0) { g_phoneW = w; g_phoneH = h; g_userLandscape = w > h; }
}
