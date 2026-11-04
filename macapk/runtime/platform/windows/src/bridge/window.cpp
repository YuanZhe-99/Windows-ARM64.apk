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
// owns every window. Calls from other threads that create or change windows are marshalled to
// it with SendMessage (they block until the loop runs them); presentBitmap only copies pixels
// under a lock and invalidates. Frames are driven from the UI thread: each tick calls the
// renderer's macapkSurfaceChanged (when the size changed) and macapkDrawFrame, as the macOS
// side's display link does. Coordinates: "points" are Windows DIPs (1/96 inch); a point is a
// dp because the runtime starts with density = 160 × the window's DPI scale.
#include "bridge.h"

#include <windows.h>
#include <windowsx.h>

#include <atomic>
#include <cstdint>
#include <functional>
#include <map>
#include <mutex>
#include <vector>

namespace winapk::bridge {
namespace {

constexpr UINT kCallMessage = WM_APP + 1;  // wParam 0, lParam: std::function<void()>*
constexpr UINT_PTR kFrameTimer = 1;
constexpr int kActionDown = 0, kActionUp = 1, kActionMove = 2;  // MotionEvent actions

/// A canvas view: the renderer that draws it and the last frame it presented.
struct View {
    int widthPt = 0, heightPt = 0;  ///< the size it was created with
    jobject renderer = nullptr;     ///< global ref to a MetalSurfaceCallbacks
    std::mutex mu;                  ///< guards the frame below
    std::vector<uint32_t> pixels;   ///< ARGB (0xAARRGGBB), top-down, width × height
    int frameW = 0, frameH = 0;
    int sizedW = -1, sizedH = -1;   ///< the size last reported by macapkSurfaceChanged
};

/// An app window and the view it shows.
struct Window {
    HWND hwnd = nullptr;
    View* content = nullptr;
    COLORREF background = RGB(255, 255, 255);
    bool pointerDown = false;
};

JavaVM* g_vm = nullptr;
DWORD g_uiThread = 0;
HWND g_dispatcher = nullptr;              // message-only window on the UI thread
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

/// Runs `fn` on the UI thread and waits for it.
/// Notes: from the UI thread it runs inline; from another thread it blocks until the UI
/// thread's message loop runs it (before runLoop starts, that is when it starts).
void RunOnUi(const std::function<void()>& fn) {
    if (GetCurrentThreadId() == g_uiThread) { fn(); return; }
    SendMessageW(g_dispatcher, kCallMessage, 0, reinterpret_cast<LPARAM>(&fn));
}

/// Returns the DPI scale of `hwnd` (or the system's): 1.0 at 96 DPI.
float Scale(HWND hwnd) {
    const UINT dpi = hwnd ? GetDpiForWindow(hwnd) : GetDpiForSystem();
    return dpi ? dpi / 96.0f : 1.0f;
}

/// Converts UTF-8 to UTF-16.
std::wstring Wide(const std::string& s) {
    std::wstring w(MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), w.data(), static_cast<int>(w.size()));
    return w;
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
    static jmethodID mid = [&] {
        jclass c = env->GetObjectClass(g_touch);
        jmethodID m = env->GetMethodID(c, "dispatch", "(IFFFF)V");
        env->DeleteLocalRef(c);
        return m;
    }();
    if (!mid) return;
    env->CallVoidMethod(g_touch, mid, action, x, y, vw, vh);
    CheckCallback(env, "TouchDispatcher.dispatch");
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
    static jmethodID mid = [&] {
        jclass c = env->GetObjectClass(g_touch);
        jmethodID m = env->GetMethodID(c, "dispatchWheel", "(FFFFFF)Z");
        env->DeleteLocalRef(c);
        return m;
    }();
    if (!mid) return;
    env->CallBooleanMethod(g_touch, mid, p.x / s, vh - p.y / s, vw, vh,
                           horizontal ? 0.0f : notches, horizontal ? -notches : 0.0f);
    CheckCallback(env, "TouchDispatcher.dispatchWheel");
}

/// Maps a Windows virtual key to an Android KeyEvent keycode; 0 when there is none.
/// Notes: the first round covers common keys and Back (PLAN.md §8.4); Escape and
/// Browser Back are Android's BACK.
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

/// Sends a key to TouchDispatcher.dispatchKey(action, keyCode, unicodeChar, metaState).
void Key(bool down, WPARAM vk, LPARAM lp) {
    const int code = AndroidKey(vk);
    if (!g_touch || code == 0) return;
    BYTE state[256];
    GetKeyboardState(state);
    wchar_t ch[4] = {};
    const int n = ToUnicode(static_cast<UINT>(vk), (lp >> 16) & 0xff, state, ch, 4, 4);  // 4: keep dead-key state
    const int unicode = n == 1 ? ch[0] : 0;
    int meta = 0;
    if (GetKeyState(VK_SHIFT) & 0x8000) meta |= 0x1;      // META_SHIFT_ON
    if (GetKeyState(VK_CONTROL) & 0x8000) meta |= 0x1000; // META_CTRL_ON
    JNIEnv* env = Env();
    static jmethodID mid = [&] {
        jclass c = env->GetObjectClass(g_touch);
        jmethodID m = env->GetMethodID(c, "dispatchKey", "(IIII)V");
        env->DeleteLocalRef(c);
        return m;
    }();
    if (!mid) return;
    env->CallVoidMethod(g_touch, mid, down ? 0 : 1, code, unicode, meta);
    CheckCallback(env, "TouchDispatcher.dispatchKey");
}

/// Runs one frame of `w`'s view: reports a new size, then asks the renderer to draw.
void Tick(Window* w) {
    View* v = w->content;
    if (!v || !v->renderer) return;
    RECT rc;
    GetClientRect(w->hwnd, &rc);
    if (rc.right <= 0 || rc.bottom <= 0) return;  // minimised
    JNIEnv* env = Env();
    static jmethodID changed = nullptr, draw = nullptr;
    if (!draw) {
        jclass c = env->FindClass("com/macapk/loader/MetalSurfaceCallbacks");
        changed = env->GetMethodID(c, "macapkSurfaceChanged", "(II)V");
        draw = env->GetMethodID(c, "macapkDrawFrame", "()V");
        env->DeleteLocalRef(c);
    }
    if (rc.right != v->sizedW || rc.bottom != v->sizedH) {
        v->sizedW = rc.right;
        v->sizedH = rc.bottom;
        env->CallVoidMethod(v->renderer, changed, static_cast<jint>(rc.right), static_cast<jint>(rc.bottom));
        CheckCallback(env, "macapkSurfaceChanged");
    }
    env->CallVoidMethod(v->renderer, draw);
    CheckCallback(env, "macapkDrawFrame");
}

/// Paints the view's last frame, scaled to the client area.
/// Notes: the frame is opaque ARGB; its bytes are BGRA in memory, the layout of a 32-bit
/// top-down DIB, so it is drawn without conversion. Alpha is not composited.
void Paint(Window* w) {
    PAINTSTRUCT ps;
    HDC dc = BeginPaint(w->hwnd, &ps);
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

/// Handles an app window's messages.
LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    auto it = g_windows.find(hwnd);
    Window* w = it == g_windows.end() ? nullptr : it->second;
    if (!w) return DefWindowProcW(hwnd, msg, wp, lp);
    switch (msg) {
        case WM_PAINT: Paint(w); return 0;
        case WM_ERASEBKGND: return 1;
        case WM_TIMER: if (wp == kFrameTimer) Tick(w); return 0;
        case WM_LBUTTONDOWN:
            SetCapture(hwnd);
            w->pointerDown = true;
            Touch(w, kActionDown, lp);
            return 0;
        case WM_MOUSEMOVE:
            // Only a move with the button held drags; with the mouse captured, other moves
            // (the cursor passing by) must not turn a tap into a drag.
            if (w->pointerDown && (wp & MK_LBUTTON)) Touch(w, kActionMove, lp);
            return 0;
        case WM_LBUTTONUP:
            if (w->pointerDown) {
                w->pointerDown = false;
                ReleaseCapture();
                Touch(w, kActionUp, lp);
            }
            return 0;
        case WM_MOUSEWHEEL: Wheel(w, wp, lp, false); return 0;
        case WM_MOUSEHWHEEL: Wheel(w, wp, lp, true); return 0;
        case WM_KEYDOWN: case WM_SYSKEYDOWN:
            if (msg == WM_KEYDOWN || wp != VK_F4) { Key(true, wp, lp); if (msg == WM_KEYDOWN) return 0; }
            break;
        case WM_KEYUP: case WM_SYSKEYUP: Key(false, wp, lp); if (msg == WM_KEYUP) return 0; break;
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
            DestroyWindow(hwnd);
            return 0;
        case WM_DESTROY:
            KillTimer(hwnd, kFrameTimer);
            g_windows.erase(hwnd);
            delete w;
            PostQuitMessage(0);
            return 0;
        default: break;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

/// Runs functions sent from other threads (RunOnUi).
LRESULT CALLBACK DispatcherProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == kCallMessage) {
        (*reinterpret_cast<const std::function<void()>*>(lp))();
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
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

/// Creates a hidden app window sized to the app; returns its handle.
jlong JNICALL CreateWindowNative(JNIEnv* env, jclass, jstring title) {
    const std::wstring t = Wide(Utf8(env, title));
    HWND result = nullptr;
    RunOnUi([&] {
        const float s = Scale(nullptr);
        RECT r{0, 0, static_cast<LONG>(g_phoneW * s), static_cast<LONG>(g_phoneH * s)};
        AdjustWindowRectExForDpi(&r, WS_OVERLAPPEDWINDOW, FALSE, 0, GetDpiForSystem());
        HWND hwnd = CreateWindowExW(0, L"WinApkAppWindow", t.c_str(), WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
                                    r.right - r.left, r.bottom - r.top, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
        if (!hwnd) return;
        g_windows[hwnd] = new Window{hwnd};
        SetTimer(hwnd, kFrameTimer, 16, nullptr);
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
    RunOnUi([&] {
        if (Window* w = FromHandle(window)) {
            w->content = reinterpret_cast<View*>(view);
            g_current = w->content;
            if (w->content) w->content->sizedW = w->content->sizedH = -1;
            InvalidateRect(w->hwnd, nullptr, FALSE);
        }
    });
}

void JNICALL ShowWindowNative(JNIEnv*, jclass, jlong window) {
    RunOnUi([&] {
        if (Window* w = FromHandle(window)) {
            ShowWindow(w->hwnd, SW_SHOWNORMAL);
            SetForegroundWindow(w->hwnd);
        }
    });
}

void JNICALL CloseWindowNative(JNIEnv*, jclass, jlong window) {
    RunOnUi([&] { if (Window* w = FromHandle(window)) DestroyWindow(w->hwnd); });
}

void JNICALL SetWindowTitle(JNIEnv* env, jclass, jlong window, jstring title) {
    const std::wstring t = Wide(Utf8(env, title));
    RunOnUi([&] { if (Window* w = FromHandle(window)) SetWindowTextW(w->hwnd, t.c_str()); });
}

/// Sets the color shown before the first frame.
void JNICALL SetStartingWindowColor(JNIEnv*, jclass, jint argb) {
    const COLORREF c = RGB((argb >> 16) & 0xff, (argb >> 8) & 0xff, argb & 0xff);
    RunOnUi([&] { for (auto& [h, w] : g_windows) { w->background = c; InvalidateRect(h, nullptr, FALSE); } });
}

/// Attaches the renderer that draws `view` (a MetalSurfaceCallbacks).
void JNICALL SetRenderer(JNIEnv* env, jclass, jlong view, jobject callbacks) {
    auto* v = reinterpret_cast<View*>(view);
    if (!v) return;
    jobject ref = callbacks ? env->NewGlobalRef(callbacks) : nullptr;
    RunOnUi([&] {
        if (v->renderer) Env()->DeleteGlobalRef(v->renderer);
        v->renderer = ref;
        v->sizedW = v->sizedH = -1;
    });
}

/// Detaches `view`'s renderer.
void JNICALL ReleaseRenderer(JNIEnv*, jclass, jlong view) {
    auto* v = reinterpret_cast<View*>(view);
    if (!v) return;
    RunOnUi([&] {
        if (v->renderer) Env()->DeleteGlobalRef(v->renderer);
        v->renderer = nullptr;
    });
}

void JNICALL RegisterTouchDispatcher(JNIEnv* env, jclass, jobject dispatcher) {
    jobject ref = dispatcher ? env->NewGlobalRef(dispatcher) : nullptr;
    RunOnUi([&] {
        if (g_touch) Env()->DeleteGlobalRef(g_touch);
        g_touch = ref;
    });
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
    }
    // InvalidateRect is safe from any thread.
    for (auto& [hwnd, win] : g_windows) if (win->content == v) InvalidateRect(hwnd, nullptr, FALSE);
}

/// Runs the UI thread's message loop until the app's window closes.
void JNICALL RunLoop(JNIEnv*, jclass) {
    MSG m;
    while (GetMessageW(&m, nullptr, 0, 0) > 0) {
        TranslateMessage(&m);
        DispatchMessageW(&m);
    }
}

/// Shapes every app window for `landscape` (wider than tall) or portrait, keeping its area.
void Orient(bool landscape) {
    const int w = g_phoneW, h = g_phoneH;
    if ((w > h) != landscape && w != h) { g_phoneW = h; g_phoneH = w; }
    RunOnUi([] {
        for (auto& [hwnd, win] : g_windows) {
            RECT rc;
            GetClientRect(hwnd, &rc);
            const bool wide = rc.right > rc.bottom;
            if (wide == (g_phoneW > g_phoneH) || rc.right == rc.bottom) continue;
            RECT r{0, 0, rc.bottom, rc.right};
            AdjustWindowRectExForDpi(&r, WS_OVERLAPPEDWINDOW, FALSE, 0, GetDpiForWindow(hwnd));
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
    RunOnUi([&] { for (auto& [hwnd, w] : g_windows) SetWindowDisplayAffinity(hwnd, secure ? WDA_EXCLUDEFROMCAPTURE : WDA_NONE); });
}

/// Minimises the app's windows (Activity.moveTaskToBack).
void JNICALL MoveTaskToBack(JNIEnv*, jclass) {
    RunOnUi([] { for (auto& [hwnd, w] : g_windows) ShowWindow(hwnd, SW_MINIMIZE); });
}

/// Restores and raises the app's windows.
void JNICALL MoveTaskToFront(JNIEnv*, jclass) {
    RunOnUi([] { for (auto& [hwnd, w] : g_windows) { ShowWindow(hwnd, SW_RESTORE); SetForegroundWindow(hwnd); } });
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

}  // namespace winapk::bridge

extern "C" __declspec(dllexport) void winapk_bridge_set_phone_size(int w, int h) {
    using namespace winapk::bridge;
    if (w > 0 && h > 0) { g_phoneW = w; g_phoneH = h; g_userLandscape = w > h; }
}
