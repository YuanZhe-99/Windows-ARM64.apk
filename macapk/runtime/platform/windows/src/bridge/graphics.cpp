// graphics.cpp -- Windows host EGL/GLES runtime and full Java OpenGL ES 1.0–3.2 / EGL 1.4–1.5 JNI
// bindings (GPU-P2, ADR-0005).
#include "graphics.h"
#include "angle_runtime.h"
#include "bridge.h"
#include "compositor.h"
#include "embed.h"
#include "guest_graphics.h"

#ifdef WINAPK_HAS_ANGLE_HEADERS
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <EGL/eglext_angle.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <map>
#include <mutex>
#include <utility>
#include <vector>
#endif

namespace winapk::bridge {

std::wstring PrivateAngleDirectory() {
    HMODULE bridge = nullptr;
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCWSTR>(&PrivateAngleDirectory), &bridge)) return {};
    std::wstring file(32768, L'\0');
    const DWORD n = GetModuleFileNameW(bridge, file.data(), static_cast<DWORD>(file.size()));
    if (!n || n >= file.size()) return {};
    file.resize(n);
    return (std::filesystem::path(file).parent_path() / L"angle").wstring();
}

#ifdef WINAPK_HAS_ANGLE_HEADERS
namespace {

std::mutex g_angleMu;
std::string g_javaRequest = "auto";
HMODULE g_libEgl = nullptr;
HMODULE g_libGles = nullptr;
PFNEGLGETPROCADDRESSPROC g_eglGetProc = nullptr;
PFNEGLGETCURRENTCONTEXTPROC g_eglGetCurrentContext = nullptr;
std::map<void*, int> g_displayRefs;

bool EnsureAngleModulesLocked() {
    if (g_libEgl && g_libGles && g_eglGetProc) return true;
    const std::wstring dir = PrivateAngleDirectory();
    if (dir.empty()) return false;
    constexpr DWORD flags = LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32;
    const std::filesystem::path base(dir);
    if (!g_libGles) g_libGles = LoadLibraryExW((base / L"libGLESv2.dll").c_str(), nullptr, flags);
    if (g_libGles && !g_libEgl) g_libEgl = LoadLibraryExW((base / L"libEGL.dll").c_str(), nullptr, flags);
    if (!g_libEgl || !g_libGles) {
        if (g_libEgl) { FreeLibrary(g_libEgl); g_libEgl = nullptr; }
        if (g_libGles) { FreeLibrary(g_libGles); g_libGles = nullptr; }
        return false;
    }
    g_eglGetProc = reinterpret_cast<PFNEGLGETPROCADDRESSPROC>(GetProcAddress(g_libEgl, "eglGetProcAddress"));
    g_eglGetCurrentContext = reinterpret_cast<PFNEGLGETCURRENTCONTEXTPROC>(GetProcAddress(g_libEgl, "eglGetCurrentContext"));
    return g_eglGetProc != nullptr;
}

bool EnsureAngleModules() {
    std::lock_guard lock(g_angleMu);
    return EnsureAngleModulesLocked();
}

void winapk_egl_display_ref(void* d) {
    if (!d) return;
    std::lock_guard lock(g_angleMu);
    ++g_displayRefs[d];
}

int winapk_egl_display_unref(void* d) {
    if (!d) return 1;
    std::lock_guard lock(g_angleMu);
    auto it = g_displayRefs.find(d);
    if (it == g_displayRefs.end()) return 1;
    if (--it->second > 0) return 0;
    g_displayRefs.erase(it);
    return 1;
}

void* winapk_ensure_surfaceview_window(int* w, int* h) {
    return EnsureJavaSurfaceViewWindow(w, h);
}

// Backend-aware EGLDisplay wrapper for Java `EGL14.eglGetDisplay` / `MacApkEGL10`.
EGLDisplay EGLAPIENTRY WinApkEglGetDisplay(EGLNativeDisplayType displayId) {
    std::string req;
    {
        std::lock_guard lock(g_angleMu);
        req = g_javaRequest;
        if (req == "off" || !EnsureAngleModulesLocked()) return EGL_NO_DISPLAY;
    }
    auto getPlat = reinterpret_cast<PFNEGLGETPLATFORMDISPLAYEXTPROC>(g_eglGetProc("eglGetPlatformDisplayEXT"));
    auto realInit = reinterpret_cast<PFNEGLINITIALIZEPROC>(GetProcAddress(g_libEgl, "eglInitialize"));
    auto realTerm = reinterpret_cast<PFNEGLTERMINATEPROC>(GetProcAddress(g_libEgl, "eglTerminate"));
    if (!getPlat) return EGL_NO_DISPLAY;

    const auto candidates = SelectAngleBackends(req);
    for (size_t i = 0; i < candidates.size(); ++i) {
        const char* backend = candidates[i];
        const bool isVk = std::strcmp(backend, "vulkan") == 0;
        const EGLint attrs[] = {
            EGL_PLATFORM_ANGLE_TYPE_ANGLE, isVk ? EGL_PLATFORM_ANGLE_TYPE_VULKAN_ANGLE : EGL_PLATFORM_ANGLE_TYPE_D3D11_ANGLE,
            EGL_PLATFORM_ANGLE_DEVICE_TYPE_ANGLE, EGL_PLATFORM_ANGLE_DEVICE_TYPE_HARDWARE_ANGLE,
            EGL_NONE
        };
        EGLDisplay dpy = getPlat(EGL_PLATFORM_ANGLE_ANGLE, reinterpret_cast<void*>(displayId), attrs);
        if (dpy == EGL_NO_DISPLAY) continue;
        if (candidates.size() == 1) return dpy;
        // For `auto`, verify the first candidate initializes before handing it back.
        if (realInit && realInit(dpy, nullptr, nullptr)) {
            InstallAngleBlobCache(dpy, reinterpret_cast<void* (*)(const char*)>(g_eglGetProc), backend);
            if (realTerm && g_displayRefs.find(dpy) == g_displayRefs.end()) {
                // Keep initialized if already live, else leave clean for caller's eglInitialize.
                realTerm(dpy);
                dpy = getPlat(EGL_PLATFORM_ANGLE_ANGLE, reinterpret_cast<void*>(displayId), attrs);
            }
            return dpy;
        }
    }
    return EGL_NO_DISPLAY;
}

EGLBoolean EGLAPIENTRY WinApkEglInitialize(EGLDisplay dpy, EGLint* major, EGLint* minor) {
    if (!EnsureAngleModules()) return EGL_FALSE;
    auto realInit = reinterpret_cast<PFNEGLINITIALIZEPROC>(GetProcAddress(g_libEgl, "eglInitialize"));
    if (!realInit || !realInit(dpy, major, minor)) return EGL_FALSE;
    auto queryStr = reinterpret_cast<PFNEGLQUERYSTRINGPROC>(GetProcAddress(g_libEgl, "eglQueryString"));
    const char* ver = queryStr ? queryStr(dpy, EGL_VERSION) : nullptr;
    (void)ver;
    std::string req;
    { std::lock_guard lock(g_angleMu); req = g_javaRequest; }
    const auto cands = SelectAngleBackends(req);
    if (!cands.empty()) {
        InstallAngleBlobCache(dpy, reinterpret_cast<void* (*)(const char*)>(g_eglGetProc), cands[0]);
    }
    return EGL_TRUE;
}

// Strips Android-only `EGL_RECORDABLE_ANDROID` (0x3142) which ANGLE rejects with `EGL_BAD_ATTRIBUTE`.
EGLBoolean EGLAPIENTRY WinApkEglChooseConfig(EGLDisplay dpy, const EGLint* attribList,
                                             EGLConfig* configs, EGLint configSize, EGLint* numConfig) {
    if (!EnsureAngleModules()) return EGL_FALSE;
    auto realChoose = reinterpret_cast<PFNEGLCHOOSECONFIGPROC>(GetProcAddress(g_libEgl, "eglChooseConfig"));
    if (!realChoose) return EGL_FALSE;
    std::vector<EGLint> filtered;
    if (attribList) {
        for (size_t i = 0; attribList[i] != EGL_NONE; i += 2) {
            if (attribList[i] == 0x3142 /* EGL_RECORDABLE_ANDROID */) continue;
            filtered.push_back(attribList[i]);
            filtered.push_back(attribList[i + 1]);
        }
        filtered.push_back(EGL_NONE);
    }
    return realChoose(dpy, attribList ? filtered.data() : nullptr, configs, configSize, numConfig);
}

void* gl3_resolve(const char* name) {
    if (!name || !EnsureAngleModules()) return nullptr;
    if (std::strcmp(name, "eglGetDisplay") == 0) return reinterpret_cast<void*>(&WinApkEglGetDisplay);
    if (std::strcmp(name, "eglInitialize") == 0) return reinterpret_cast<void*>(&WinApkEglInitialize);
    if (std::strcmp(name, "eglChooseConfig") == 0) return reinterpret_cast<void*>(&WinApkEglChooseConfig);
    if (std::strncmp(name, "egl", 3) == 0) {
        if (void* p = reinterpret_cast<void*>(GetProcAddress(g_libEgl, name))) return p;
    } else {
        if (void* p = reinterpret_cast<void*>(GetProcAddress(g_libGles, name))) return p;
    }
    return g_eglGetProc ? reinterpret_cast<void*>(g_eglGetProc(name)) : nullptr;
}

bool HasCurrentContext() {
    if (!g_eglGetCurrentContext && !EnsureAngleModules()) return false;
    return g_eglGetCurrentContext && g_eglGetCurrentContext() != EGL_NO_CONTEXT;
}

void ThrowJava(JNIEnv* env, const char* type, const char* message) {
    if (env->ExceptionCheck()) return;
    if (jclass cls = env->FindClass(type)) {
        env->ThrowNew(cls, message);
        env->DeleteLocalRef(cls);
    }
}

// Nested namespace holds the 141 unqualified `gl*` wrappers and upstream's `.inc` bridges
// so unqualified `gl*` calls inside `gles20_bridges.inc` resolve dynamically through `gl3_resolve`
// instead of linking against `libGLESv2.lib`.
namespace jgl {

using winapk::bridge::gl3_resolve;
using winapk::bridge::winapk_egl_display_ref;
using winapk::bridge::winapk_egl_display_unref;
using winapk::bridge::winapk_ensure_surfaceview_window;

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-parameter"
#pragma clang diagnostic ignored "-Wunused-variable"
#pragma clang diagnostic ignored "-Wunused-function"

#include "gles20_dyn_procs.inc"

#define MACAPK_HAS_ANGLE 1
#include "../../../../src/gles20_bridges.inc"

static void* gl3_buffer_ptr(JNIEnv* env, jobject buf) {
    if (!buf) return nullptr;
    void* base = env->GetDirectBufferAddress(buf);
    if (!base) return nullptr;
    EnsureBufferCache(env);
    jint p = env->CallIntMethod(buf, gBufCache.mPosition);
    int es = InferElemSizeCached(env, buf);
    if (p > 0) base = static_cast<char*>(base) + static_cast<jlong>(p) * es;
    return base;
}

static bool gl3_check_offset(JNIEnv* env, jarray arr, jint offset, const char* what) {
    if (!arr) return true;
    if (offset >= 0 && offset < env->GetArrayLength(arr)) return true;
    if (jclass ex = env->FindClass("java/lang/IllegalArgumentException")) {
        env->ThrowNew(ex, what);
        env->DeleteLocalRef(ex);
    }
    return false;
}

static bool gl3_check_config_span(JNIEnv* env, jobjectArray arr, jint offset, jint count, const char* what) {
    if (!arr) return true;
    if (offset >= 0 && count >= 0 && env->GetArrayLength(arr) - offset >= count) return true;
    if (jclass ex = env->FindClass("java/lang/IllegalArgumentException")) {
        env->ThrowNew(ex, what);
        env->DeleteLocalRef(ex);
    }
    return false;
}

#include "../../../../src/gles3_bridges.inc"
#include "../../../../src/gles1_bridges.inc"

inline void Bind(JNIEnv* env, const char* cls, const JNINativeMethod* methods, size_t count) {
    winapk::bridge::Bind(env, cls, methods, static_cast<int>(count));
}

#include "../../../../src/egl14_bridges.inc"

#pragma clang diagnostic pop

}  // namespace jgl

// Strict overrides preserving T10.java's off-context IllegalStateException and read-only ByteBuffer
// IllegalArgumentException while returning full real GL_EXTENSIONS when a context is current.
jstring JNICALL StrictGlGetString(JNIEnv* env, jclass, jint name) {
    if (!HasCurrentContext()) {
        ThrowJava(env, "java/lang/IllegalStateException", "GRAPHICS_CAPABILITY_MISSING: no current EGL context");
        return nullptr;
    }
    const GLubyte* s = jgl::glGetString(static_cast<GLenum>(name));
    return s ? env->NewStringUTF(reinterpret_cast<const char*>(s)) : env->NewStringUTF("");
}

static size_t BytesPerPixel(GLenum format, GLenum type) {
    switch (type) {
        case GL_UNSIGNED_SHORT_5_6_5:
        case GL_UNSIGNED_SHORT_4_4_4_4:
        case GL_UNSIGNED_SHORT_5_5_5_1:
            return 2;
        case GL_FLOAT:
            return format == GL_RGBA ? 16 : (format == GL_RGB ? 12 : 4);
        default:
            return format == GL_ALPHA ? 1 : (format == GL_RGB ? 3 : 4);
    }
}

void JNICALL StrictGlReadPixelsBuf(JNIEnv* env, jclass, jint x, jint y, jint w, jint h,
                                   jint format, jint type, jobject dst) {
    if (!HasCurrentContext()) {
        ThrowJava(env, "java/lang/IllegalStateException", "GRAPHICS_CAPABILITY_MISSING: no current EGL context");
        return;
    }
    if (!dst || w < 0 || h < 0) {
        ThrowJava(env, "java/lang/IllegalArgumentException", "glReadPixels requires non-null writable Buffer and non-negative size");
        return;
    }
    jclass bufCls = env->FindClass("java/nio/Buffer");
    if (!bufCls) return;
    const jmethodID isRoMid = env->GetMethodID(bufCls, "isReadOnly", "()Z");
    const jmethodID remMid = env->GetMethodID(bufCls, "remaining", "()I");
    const bool readOnly = env->CallBooleanMethod(dst, isRoMid) == JNI_TRUE;
    const jint remaining = env->CallIntMethod(dst, remMid);
    env->DeleteLocalRef(bufCls);
    if (env->ExceptionCheck()) return;
    if (readOnly) {
        ThrowJava(env, "java/lang/IllegalArgumentException", "glReadPixels requires a writable Buffer");
        return;
    }
    jgl::EnsureBufferCache(env);
    const int elemSize = jgl::InferElemSizeCached(env, dst);
    const uint64_t availBytes = remaining > 0 ? static_cast<uint64_t>(remaining) * static_cast<uint64_t>(elemSize) : 0;
    const uint64_t neededBytes = BytesPerPixel(static_cast<GLenum>(format), static_cast<GLenum>(type)) *
                                 static_cast<uint64_t>(w) * static_cast<uint64_t>(h);
    if (availBytes < neededBytes) {
        ThrowJava(env, "java/lang/IllegalArgumentException", "glReadPixels Buffer remaining capacity too small");
        return;
    }
    jgl::ScopedBufferBytes b(env, dst, /*write_back=*/true);
    if (!b.ptr) {
        ThrowJava(env, "java/lang/IllegalArgumentException", "glReadPixels Buffer has no backing memory");
        return;
    }
    jgl::glReadPixels(x, y, w, h, static_cast<GLenum>(format), static_cast<GLenum>(type), b.ptr);
}

#define EGL_ENTRIES(X) \
    X(PFNEGLGETPROCADDRESSPROC, eglGetProcAddress) \
    X(PFNEGLINITIALIZEPROC, eglInitialize) \
    X(PFNEGLBINDAPIPROC, eglBindAPI) \
    X(PFNEGLCHOOSECONFIGPROC, eglChooseConfig) \
    X(PFNEGLCREATEWINDOWSURFACEPROC, eglCreateWindowSurface) \
    X(PFNEGLCREATECONTEXTPROC, eglCreateContext) \
    X(PFNEGLMAKECURRENTPROC, eglMakeCurrent) \
    X(PFNEGLSWAPBUFFERSPROC, eglSwapBuffers) \
    X(PFNEGLDESTROYCONTEXTPROC, eglDestroyContext) \
    X(PFNEGLDESTROYSURFACEPROC, eglDestroySurface) \
    X(PFNEGLTERMINATEPROC, eglTerminate) \
    X(PFNEGLGETERRORPROC, eglGetError)

}  // namespace

void SetJavaGraphicsRequest(const std::string& request) {
    std::lock_guard lock(g_angleMu);
    g_javaRequest = request;
}

struct JavaGraphics::State {
    HWND window = nullptr;
    HMODULE egl = nullptr, gles = nullptr;
    EGLDisplay display = EGL_NO_DISPLAY;
    EGLContext context = EGL_NO_CONTEXT;
    EGLSurface surface = EGL_NO_SURFACE;
    EGLConfig config = nullptr;
    bool initialized = false;
    bool bound = false;
    int width = 0, height = 0;
    PFNGLGETSTRINGPROC glGetString = nullptr;
    PFNGLREADPIXELSPROC glReadPixels = nullptr;
    PFNGLBINDFRAMEBUFFERPROC glBindFramebuffer = nullptr;
    PFNGLGETINTEGERVPROC glGetIntegerv = nullptr;
#define FIELD(Type, Name) Type Name = nullptr;
    EGL_ENTRIES(FIELD)
#undef FIELD

    ~State() {
        bool ok = true;
        if (bound) {
            ok = eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT) == EGL_TRUE;
        }
        if (surface != EGL_NO_SURFACE) ok = (eglDestroySurface(display, surface) == EGL_TRUE) && ok;
        if (context != EGL_NO_CONTEXT) ok = (eglDestroyContext(display, context) == EGL_TRUE) && ok;
        if (initialized) {
            if (winapk_egl_display_unref(display)) {
                ok = (eglTerminate(display) == EGL_TRUE) && ok;
            }
            LogLine(ok ? "[winapk] Java GLES teardown complete" : "GRAPHICS_RENDER_FAILED: Java GLES teardown");
        }
        if (egl) FreeLibrary(egl);
        if (gles) FreeLibrary(gles);
    }

    bool Load(const std::filesystem::path& directory, std::string& error) {
        constexpr DWORD flags = LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32;
        if (!directory.empty()) {
            gles = LoadLibraryExW((directory / L"libGLESv2.dll").c_str(), nullptr, flags);
            if (gles) egl = LoadLibraryExW((directory / L"libEGL.dll").c_str(), nullptr, flags);
        }
        if (!egl || !gles) { error = "private ANGLE DLLs not available"; return false; }
#define LOAD_EGL(Type, Name) \
        Name = reinterpret_cast<Type>(GetProcAddress(egl, #Name)); \
        if (!Name) { error = "missing ANGLE entry " #Name; return false; }
        EGL_ENTRIES(LOAD_EGL)
#undef LOAD_EGL
        glGetString = reinterpret_cast<PFNGLGETSTRINGPROC>(GetProcAddress(gles, "glGetString"));
        glReadPixels = reinterpret_cast<PFNGLREADPIXELSPROC>(GetProcAddress(gles, "glReadPixels"));
        glBindFramebuffer = reinterpret_cast<PFNGLBINDFRAMEBUFFERPROC>(GetProcAddress(gles, "glBindFramebuffer"));
        glGetIntegerv = reinterpret_cast<PFNGLGETINTEGERVPROC>(GetProcAddress(gles, "glGetIntegerv"));
        if (!glGetString || !glReadPixels || !glBindFramebuffer || !glGetIntegerv) {
            error = "missing core GLES entry points";
            return false;
        }
        EnsureAngleModules();
        return true;
    }
};

JavaGraphics::JavaGraphics(std::unique_ptr<State> state) : state_(std::move(state)) {}
JavaGraphics::~JavaGraphics() = default;

std::shared_ptr<JavaGraphics> JavaGraphics::Open(HWND window, const std::string& request,
                                                 int clientVersion, std::string& error) {
    const std::filesystem::path directory = PrivateAngleDirectory();
    const bool automatic = request == "auto";
    bool nativeVk = false;
    const auto candidates = SelectAngleBackends(request, &nativeVk);
    if (candidates.empty()) {
        error = "unsupported graphics request " + request;
        return {};
    }
    if (automatic && !nativeVk) {
        LogLine("[winapk] Java GLES auto: no native vendor Vulkan ICD; preferring hardware d3d11");
    }
    const EGLint renderableBit = clientVersion >= 3 ? 0x0040 /* EGL_OPENGL_ES3_BIT_KHR */
                               : (clientVersion == 1 ? EGL_OPENGL_ES_BIT : EGL_OPENGL_ES2_BIT);
    for (size_t i = 0; i < candidates.size(); ++i) {
        const char* backend = candidates[i];
        const bool vulkan = std::strcmp(backend, "vulkan") == 0;
        auto state = std::make_unique<State>();
        state->window = window;
        if (!state->Load(directory, error)) return {};
        const EGLint configWithDepth[] = {
            EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
            EGL_RENDERABLE_TYPE, renderableBit,
            EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8,
            EGL_DEPTH_SIZE, 16, EGL_STENCIL_SIZE, 8, EGL_NONE
        };
        const EGLint configColorOnly[] = {
            EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
            EGL_RENDERABLE_TYPE, renderableBit,
            EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8, EGL_NONE
        };
        const EGLint contextArgs[] = {EGL_CONTEXT_CLIENT_VERSION, clientVersion, EGL_NONE};
        const EGLint displayArgs[] = {
            EGL_PLATFORM_ANGLE_TYPE_ANGLE, vulkan ? EGL_PLATFORM_ANGLE_TYPE_VULKAN_ANGLE : EGL_PLATFORM_ANGLE_TYPE_D3D11_ANGLE,
            EGL_PLATFORM_ANGLE_DEVICE_TYPE_ANGLE, EGL_PLATFORM_ANGLE_ANGLE == 0 ? 0 : EGL_PLATFORM_ANGLE_DEVICE_TYPE_HARDWARE_ANGLE,
            EGL_NONE
        };
        auto getDisplay = reinterpret_cast<PFNEGLGETPLATFORMDISPLAYEXTPROC>(state->eglGetProcAddress("eglGetPlatformDisplayEXT"));
        if (getDisplay) state->display = getDisplay(EGL_PLATFORM_ANGLE_ANGLE, EGL_DEFAULT_DISPLAY, displayArgs);
        EGLint major = 0, minor = 0, configs = 0;
        state->initialized = state->display != EGL_NO_DISPLAY && state->eglInitialize(state->display, &major, &minor);
        if (state->initialized) {
            winapk_egl_display_ref(state->display);
            InstallAngleBlobCache(state->display,
                reinterpret_cast<void* (*)(const char*)>(state->eglGetProcAddress), backend);
        }
        bool chose = state->initialized && state->eglBindAPI(EGL_OPENGL_ES_API) &&
            ((state->eglChooseConfig(state->display, configWithDepth, &state->config, 1, &configs) && configs > 0) ||
             (state->eglChooseConfig(state->display, configColorOnly, &state->config, 1, &configs) && configs > 0));
        if (chose) {
            state->context = state->eglCreateContext(state->display, state->config, EGL_NO_CONTEXT, contextArgs);
            if (state->context != EGL_NO_CONTEXT) {
                auto graphics = std::shared_ptr<JavaGraphics>(new JavaGraphics(std::move(state)));
                RECT rect{};
                if (GetClientRect(window, &rect) && graphics->Begin(rect.right, rect.bottom, error)) {
                    const auto* renderer = reinterpret_cast<const char*>(graphics->state_->glGetString(GL_RENDERER));
                    const auto* version = reinterpret_cast<const char*>(graphics->state_->glGetString(GL_VERSION));
                    const bool matches = renderer && (vulkan ? std::strstr(renderer, "Vulkan") :
                        (std::strstr(renderer, "Direct3D11") ? renderer : std::strstr(renderer, "D3D11")));
                    if (matches && version) {
                        LogLine("[winapk] Java GLES request=" + request + " backend=" + backend + " renderer=" +
                            renderer + " gl=" + version + " egl=" + std::to_string(major) + "." + std::to_string(minor));
                        SendEmbedEvent(std::string("backend gles-") + backend);
                        if (graphics->End(false, error)) return graphics;
                    } else {
                        graphics->End(false, error);
                        error = "actual renderer did not match the requested backend";
                    }
                }
            } else error = "eglCreateContext error=" + std::to_string(state->eglGetError());
        } else error = "EGL initialization/configuration error=" + std::to_string(state->eglGetError());
        error = std::string("backend=") + backend + " " + error;
        if (automatic && i + 1 < candidates.size()) {
            LogLine("[winapk] Java GLES auto: " + error + "; trying " + candidates[i + 1]);
        }
    }
    if (!automatic) error += "; no fallback";
    return {};
}

bool JavaGraphics::Begin(int width, int height, std::string& error) {
    auto& s = *state_;
    if (s.bound) { error = "nested Windows GLES frame"; return false; }
    if (s.surface != EGL_NO_SURFACE && (width != s.width || height != s.height)) {
        if (!s.eglDestroySurface(s.display, s.surface)) {
            error = "eglDestroySurface on resize error=" + std::to_string(s.eglGetError());
            return false;
        }
        s.surface = EGL_NO_SURFACE;
    }
    if (s.surface == EGL_NO_SURFACE) {
        s.surface = s.eglCreateWindowSurface(s.display, s.config, s.window, nullptr);
        s.width = width;
        s.height = height;
    }
    if (s.surface == EGL_NO_SURFACE || !s.eglMakeCurrent(s.display, s.surface, s.surface, s.context)) {
        error = "EGL drawable/current error=" + std::to_string(s.eglGetError());
        return false;
    }
    s.bound = true;
    return true;
}

bool JavaGraphics::End(bool present, std::string& error) {
    auto& s = *state_;
    if (present && IsEmbedded() && s.width > 0 && s.height > 0) {
        GLint prevFbo = 0;
        s.glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prevFbo);
        if (prevFbo != 0) s.glBindFramebuffer(GL_FRAMEBUFFER, 0);
        std::vector<uint8_t> pixels(static_cast<size_t>(s.width) * static_cast<size_t>(s.height) * 4);
        s.glReadPixels(0, 0, s.width, s.height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
        if (prevFbo != 0) s.glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(prevFbo));
        const SurfaceLayerPlacement full{0, 0, s.width, s.height, true, false};
        if (!CompositorSubmitSurfaceBytes(s.window, -1, pixels.data(), static_cast<size_t>(s.width) * 4,
                                          s.width, s.height, LayerPixels::Rgba8, LayerAlpha::Opaque, true, full)) {
            PublishEmbedGlRgba(pixels.data(), s.width, s.height);
        }
    }
    const bool swapped = !present || s.eglSwapBuffers(s.display, s.surface);
    if (!swapped) error = "eglSwapBuffers error=" + std::to_string(s.eglGetError());
    s.bound = !s.eglMakeCurrent(s.display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    if (s.bound) error = "eglMakeCurrent unbind error=" + std::to_string(s.eglGetError());
    return swapped && !s.bound;
}

bool RegisterJavaGraphics(JNIEnv* env) {
    bool ok = true;
    ok = Bind(env, "android/opengl/GLES20", jgl::kGles20Methods, static_cast<int>(std::size(jgl::kGles20Methods))) && ok;
    ok = Bind(env, "android/opengl/GLUtils", jgl::kGLUtilsMethods, static_cast<int>(std::size(jgl::kGLUtilsMethods))) && ok;
    ok = Bind(env, "android/opengl/GLES30", jgl::kGl3GLES30Methods, static_cast<int>(std::size(jgl::kGl3GLES30Methods))) && ok;
    ok = Bind(env, "android/opengl/GLES31", jgl::kGl3GLES31Methods, static_cast<int>(std::size(jgl::kGl3GLES31Methods))) && ok;
    ok = Bind(env, "android/opengl/GLES31Ext", jgl::kGl3GLES31ExtMethods, static_cast<int>(std::size(jgl::kGl3GLES31ExtMethods))) && ok;
    ok = Bind(env, "android/opengl/GLES32", jgl::kGl3GLES32Methods, static_cast<int>(std::size(jgl::kGl3GLES32Methods))) && ok;
    ok = Bind(env, "android/opengl/GLES10", jgl::kGl1GLES10Methods, static_cast<int>(std::size(jgl::kGl1GLES10Methods))) && ok;
    ok = Bind(env, "android/opengl/GLES11", jgl::kGl1GLES11Methods, static_cast<int>(std::size(jgl::kGl1GLES11Methods))) && ok;
    jgl::RegisterEgl14Methods(env);

    const JNINativeMethod strictMethods[] = {
        {const_cast<char*>("glGetString"), const_cast<char*>("(I)Ljava/lang/String;"), reinterpret_cast<void*>(&StrictGlGetString)},
        {const_cast<char*>("glReadPixels"), const_cast<char*>("(IIIIIILjava/nio/Buffer;)V"), reinterpret_cast<void*>(&StrictGlReadPixelsBuf)},
    };
    ok = Bind(env, "android/opengl/GLES20", strictMethods, static_cast<int>(std::size(strictMethods))) && ok;
    return ok;
}

#else

void SetJavaGraphicsRequest(const std::string&) {}
void* EnsureJavaSurfaceViewWindow(int* w, int* h) { if (w) *w = 0; if (h) *h = 0; return nullptr; }
struct JavaGraphics::State {};
JavaGraphics::~JavaGraphics() = default;
std::shared_ptr<JavaGraphics> JavaGraphics::Open(HWND, const std::string&, int, std::string& error) {
    error = "bridge built without pinned ANGLE headers; build Angle, then Host";
    return {};
}
bool JavaGraphics::Begin(int, int, std::string&) { return false; }
bool JavaGraphics::End(bool, std::string&) { return false; }
bool RegisterJavaGraphics(JNIEnv*) { return true; }

#endif

} // namespace winapk::bridge
