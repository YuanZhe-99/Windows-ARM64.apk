// Android EGL/GLES fixed signatures. Driver objects and current state stay on the Win32 thread.
#include "guest_graphics.h"
#include "bridge.h"

#ifdef WINAPK_HAS_ANGLE_HEADERS
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <EGL/eglext_angle.h>
#include <GLES2/gl2.h>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <new>
#include <utility>

namespace winapk::bridge {
namespace {

static_assert(sizeof(EGLint) == 4 && sizeof(EGLBoolean) == 4 && sizeof(GLint) == 4 &&
              sizeof(GLenum) == 4 && sizeof(GLfloat) == 4 && sizeof(GLboolean) == 1 &&
              sizeof(EGLDisplay) == 8 && sizeof(EGLSurface) == 8 && sizeof(EGLContext) == 8);

#define EGL_DRIVER(X) \
    X(eglGetProcAddress) X(eglInitialize) X(eglBindAPI) X(eglChooseConfig) \
    X(eglCreateContext) X(eglCreateWindowSurface) X(eglMakeCurrent) X(eglSwapBuffers) \
    X(eglDestroyContext) X(eglDestroySurface) X(eglTerminate) X(eglGetError) X(eglGetConfigAttrib)

// Fixed scalar/pointer ARM64 signatures; no variadic calls, callbacks, or native handle forwarding.
#define GL_VOID(X) \
    X(glClear, (GLbitfield a), (a)) \
    X(glClearColor, (GLfloat a, GLfloat b, GLfloat c, GLfloat d), (a,b,c,d)) \
    X(glViewport, (GLint x, GLint y, GLsizei w, GLsizei h), (x,y,w,h)) \
    X(glScissor, (GLint x, GLint y, GLsizei w, GLsizei h), (x,y,w,h)) \
    X(glEnable, (GLenum a), (a)) X(glDisable, (GLenum a), (a)) \
    X(glBlendFunc, (GLenum a, GLenum b), (a,b)) \
    X(glGenTextures, (GLsizei n, GLuint* a), (n,a)) \
    X(glDeleteTextures, (GLsizei n, const GLuint* a), (n,a)) \
    X(glBindTexture, (GLenum a, GLuint b), (a,b)) \
    X(glTexParameteri, (GLenum a, GLenum b, GLint c), (a,b,c)) \
    X(glTexImage2D, (GLenum a, GLint b, GLint c, GLsizei d, GLsizei e, GLint f, GLenum g, GLenum h, const void* i), (a,b,c,d,e,f,g,h,i)) \
    X(glGenFramebuffers, (GLsizei n, GLuint* a), (n,a)) \
    X(glDeleteFramebuffers, (GLsizei n, const GLuint* a), (n,a)) \
    X(glBindFramebuffer, (GLenum a, GLuint b), (a,b)) \
    X(glFramebufferTexture2D, (GLenum a, GLenum b, GLenum c, GLuint d, GLint e), (a,b,c,d,e)) \
    X(glShaderSource, (GLuint a, GLsizei b, const GLchar* const* c, const GLint* d), (a,b,c,d)) \
    X(glCompileShader, (GLuint a), (a)) X(glDeleteShader, (GLuint a), (a)) \
    X(glGetShaderiv, (GLuint a, GLenum b, GLint* c), (a,b,c)) \
    X(glAttachShader, (GLuint a, GLuint b), (a,b)) \
    X(glLinkProgram, (GLuint a), (a)) X(glUseProgram, (GLuint a), (a)) X(glDeleteProgram, (GLuint a), (a)) \
    X(glGetProgramiv, (GLuint a, GLenum b, GLint* c), (a,b,c)) \
    X(glBindAttribLocation, (GLuint a, GLuint b, const GLchar* c), (a,b,c)) \
    X(glEnableVertexAttribArray, (GLuint a), (a)) X(glDisableVertexAttribArray, (GLuint a), (a)) \
    X(glVertexAttribPointer, (GLuint a, GLint b, GLenum c, GLboolean d, GLsizei e, const void* f), (a,b,c,d,e,f)) \
    X(glDrawArrays, (GLenum a, GLint b, GLsizei c), (a,b,c)) \
    X(glReadPixels, (GLint a, GLint b, GLsizei c, GLsizei d, GLenum e, GLenum f, void* g), (a,b,c,d,e,f,g))
#define GL_RESULT(X) \
    X(GLenum, glCheckFramebufferStatus, (GLenum a), (a)) \
    X(GLuint, glCreateShader, (GLenum a), (a)) \
    X(GLuint, glCreateProgram, (), ())

/// Owns one host display and one logical configuration/context/window, all on the window thread.
struct Session {
#define FIELD(name) decltype(&::name) name = nullptr;
#define VFIELD(name, args, call) FIELD(name)
#define RFIELD(type, name, args, call) FIELD(name)
    EGL_DRIVER(FIELD)
    GL_VOID(VFIELD)
    GL_RESULT(RFIELD)
    FIELD(glGetString) FIELD(glGetError)
#undef RFIELD
#undef VFIELD
#undef FIELD
    HMODULE egl = nullptr, gles = nullptr;
    EGLDisplay display = EGL_NO_DISPLAY;
    EGLConfig config = nullptr;
    EGLContext context = EGL_NO_CONTEXT;
    EGLSurface drawable = EGL_NO_SURFACE;
    HWND window = nullptr;
    void* native = nullptr;
    void* configId = nullptr;
    void* contextId = nullptr;
    void* surfaceId = nullptr;
    uint64_t owner = 0;
    uint64_t current = 0;
    int width = 0, height = 0;
    bool presented = false;
    bool lost = false;
    const char* backend = nullptr;
    bool rendererChecked = false;
};

Session session;
constexpr uintptr_t kDisplay = 1;
uintptr_t nextId = 2; // never reused; access is serialized on the owner thread
std::atomic<uint64_t> nextThread{1};
thread_local const uint64_t caller = nextThread.fetch_add(1, std::memory_order_relaxed);
thread_local EGLint lastError = EGL_SUCCESS;
thread_local GLenum lastGlError = GL_NO_ERROR;

/// Sets an EGL error for the calling guest thread.
EGLint Fail(EGLint error) { lastError = error; return error; }

/// Contains allocation errors at the fixed guest ABI boundary.
template<class F> bool Dispatch(F&& f) {
    try {
        return DispatchGuestGraphics(std::forward<F>(f));
    } catch (const std::bad_alloc&) {
        return false;
    }
}

/// Runs a logical EGL command and copies its failure code to the calling guest thread.
template<class F> EGLBoolean Run(F&& f) {
    EGLint error = EGL_BAD_ACCESS;
    const uint64_t id = caller;
    if (!Dispatch([&] { error = f(id); })) error = EGL_BAD_ACCESS;
    return error == EGL_SUCCESS ? EGL_TRUE : (Fail(error), EGL_FALSE);
}

/// Validates the bridge display identity and its owner.
EGLint CheckDisplay(EGLDisplay display, uint64_t id) {
    if (reinterpret_cast<uintptr_t>(display) != kDisplay) return EGL_BAD_DISPLAY;
    if (!session.owner) return EGL_NOT_INITIALIZED;
    return session.owner == id ? EGL_SUCCESS : EGL_BAD_ACCESS;
}

/// Consumes one native EGL error; notices loss instead of silently replacing a context.
EGLint DriverError() {
    auto error = session.eglGetError();
    if (error == EGL_CONTEXT_LOST) session.lost = true;
    return error == EGL_SUCCESS ? EGL_BAD_ACCESS : error;
}

/// Clears the actual and logical current bindings on the owner thread.
EGLint Unbind() {
    if (!session.current) return EGL_SUCCESS;
    if (!session.eglMakeCurrent(session.display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT))
        return DriverError();
    session.current = 0;
    return EGL_SUCCESS;
}

/// Destroys one drawable, optionally maintaining the logical token for explicit cleanup.
EGLint DropSurface(bool keepToken) {
    if (auto error = Unbind(); error != EGL_SUCCESS && error != EGL_CONTEXT_LOST) return error;
    session.current = 0;
    if (session.drawable && !session.eglDestroySurface(session.display, session.drawable)) return DriverError();
    session.drawable = EGL_NO_SURFACE;
    if (session.native) ReleaseSurface(session.native);
    session.native = nullptr;
    session.presented = false;
    if (!keepToken) {
        if (session.window) InvalidateRect(session.window, nullptr, FALSE);
        session.window = nullptr;
        session.surfaceId = nullptr;
    }
    return EGL_SUCCESS;
}

/// Releases the session, including when the application closes without calling eglTerminate.
EGLint Shutdown() {
    EGLint error = DropSurface(false);
    if (session.context && !session.eglDestroyContext(session.display, session.context))
        error = DriverError();
    if (session.owner) {
        if (!session.eglTerminate(session.display)) error = DriverError();
        LogLine(error == EGL_SUCCESS ? "[winapk] guest EGL teardown complete" : "GRAPHICS_RENDER_FAILED: guest EGL teardown");
    }
    if (session.native) ReleaseSurface(session.native);
    if (session.egl) FreeLibrary(session.egl);
    if (session.gles) FreeLibrary(session.gles);
    session = {};
    return error;
}

/// Opens private ARM64 ANGLE and selects explicit hardware backends, never the default display.
EGLint Start(uint64_t owner) {
    const std::string& request = GuestGraphicsRequest();
    if (request == "off") return EGL_NOT_INITIALIZED;
    const std::wstring directory = PrivateAngleDirectory();
    if (directory.empty()) return EGL_NOT_INITIALIZED;
    constexpr DWORD flags = LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32;
    session.gles = LoadLibraryExW((directory + L"\\libGLESv2.dll").c_str(), nullptr, flags);
    session.egl = LoadLibraryExW((directory + L"\\libEGL.dll").c_str(), nullptr, flags);
    if (!session.egl || !session.gles) { Shutdown(); return EGL_NOT_INITIALIZED; }
#define LOAD(lib, name) session.name = reinterpret_cast<decltype(session.name)>(GetProcAddress(session.lib, #name)); \
    if (!session.name) { Shutdown(); return EGL_NOT_INITIALIZED; }
#define ELOAD(name) LOAD(egl, name)
#define GLOAD(name) LOAD(gles, name)
#define VLOAD(name, args, call) GLOAD(name)
#define RLOAD(type, name, args, call) GLOAD(name)
    EGL_DRIVER(ELOAD)
    GL_VOID(VLOAD)
    GL_RESULT(RLOAD)
    GLOAD(glGetString) GLOAD(glGetError)
#undef RLOAD
#undef VLOAD
#undef GLOAD
#undef ELOAD
#undef LOAD
    auto get = reinterpret_cast<PFNEGLGETPLATFORMDISPLAYEXTPROC>(session.eglGetProcAddress("eglGetPlatformDisplayEXT"));
    if (!get) { Shutdown(); return EGL_NOT_INITIALIZED; }
    for (int i = 0; i < (request == "auto" ? 2 : 1); ++i) {
        const bool vulkan = request == "vulkan" || (request == "auto" && !i);
        const char* backend = vulkan ? "vulkan" : "d3d11";
        const EGLint attrs[] = {EGL_PLATFORM_ANGLE_TYPE_ANGLE,
            vulkan ? EGL_PLATFORM_ANGLE_TYPE_VULKAN_ANGLE : EGL_PLATFORM_ANGLE_TYPE_D3D11_ANGLE,
            EGL_PLATFORM_ANGLE_DEVICE_TYPE_ANGLE, EGL_PLATFORM_ANGLE_DEVICE_TYPE_HARDWARE_ANGLE, EGL_NONE};
        session.display = get(EGL_PLATFORM_ANGLE_ANGLE, EGL_DEFAULT_DISPLAY, attrs);
        if (session.display && session.eglInitialize(session.display, nullptr, nullptr)) {
            session.owner = owner;
            constexpr EGLint rgba[] = {EGL_SURFACE_TYPE, EGL_WINDOW_BIT, EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
                EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8, EGL_NONE};
            EGLint count = 0;
            if (session.eglBindAPI(EGL_OPENGL_ES_API) &&
                session.eglChooseConfig(session.display, rgba, &session.config, 1, &count) && count == 1) {
                session.configId = reinterpret_cast<void*>(nextId++);
                session.backend = backend;
                LogLine("[winapk] guest EGL request=" + request + " backend=" + backend);
                return EGL_SUCCESS;
            }
            Shutdown();
            return EGL_NOT_INITIALIZED;
        }
        if (request == "auto" && !i) LogLine("[winapk] guest EGL auto: Vulkan unavailable; trying d3d11");
        else LogLine("GRAPHICS_CAPABILITY_MISSING: guest EGL backend=" + std::string(backend) + "; no fallback");
    }
    Shutdown();
    return EGL_NOT_INITIALIZED;
}

/// Ensures the drawable refers to the live window and recreates it at a new extent.
EGLint EnsureSurface(uint64_t resume) {
    if (session.lost) return EGL_CONTEXT_LOST;
    HWND window;
    int width, height;
    if (!ResolveSurface(session.native, window, width, height) || window != session.window || width <= 0 || height <= 0)
        return EGL_BAD_SURFACE;
    if (!session.drawable || width != session.width || height != session.height) {
        if (auto error = Unbind(); error != EGL_SUCCESS) return error;
        if (session.drawable && !session.eglDestroySurface(session.display, session.drawable)) return DriverError();
        session.drawable = session.eglCreateWindowSurface(session.display, session.config, window, nullptr);
        if (!session.drawable) return DriverError();
        session.width = width;
        session.height = height;
    }
    if (resume && !session.current) {
        if (!session.eglMakeCurrent(session.display, session.drawable, session.drawable, session.context)) return DriverError();
        session.current = resume;
        if (!session.rendererChecked) {
            const auto* text = reinterpret_cast<const char*>(session.glGetString(GL_RENDERER));
            const bool vulkan = std::strcmp(session.backend, "vulkan") == 0;
            const bool matches = text && (vulkan ? std::strstr(text, "Vulkan") != nullptr :
                (std::strstr(text, "Direct3D11") || std::strstr(text, "D3D11")));
            if (!matches) {
                Unbind();
                return EGL_BAD_MATCH;
            }
            session.rendererChecked = true;
            LogLine(std::string("[winapk] guest GLES renderer backend=") + session.backend + " name=" + text);
        }
    }
    return EGL_SUCCESS;
}

/// Returns the only supported logical display, never a Win32 display handle.
EGLDisplay GetDisplay(void* native) {
    if (native) { Fail(EGL_BAD_PARAMETER); return EGL_NO_DISPLAY; }
    return reinterpret_cast<EGLDisplay>(kDisplay);
}

/// Initializes the admitted profile; the reported EGL level excludes unimplemented 1.5 commands.
EGLBoolean Initialize(EGLDisplay d, EGLint* major, EGLint* minor) {
    return Run([=](uint64_t id) {
        if (reinterpret_cast<uintptr_t>(d) != kDisplay) return EGL_BAD_DISPLAY;
        auto error = session.owner ? CheckDisplay(d,id) : Start(id);
        if (error != EGL_SUCCESS) return error;
        if (major) *major = 1;
        if (minor) *minor = 4;
        return EGL_SUCCESS;
    });
}

/// Tests the supplied attributes against the single real RGBA8 window configuration.
EGLBoolean ChooseConfig(EGLDisplay d, const EGLint* attributes, EGLConfig* out, EGLint capacity, EGLint* count) {
    return Run([=](uint64_t id) {
        if (auto error = CheckDisplay(d,id); error != EGL_SUCCESS) return error;
        if (!count || capacity < 0) return EGL_BAD_PARAMETER;
        bool match = true;
        if (attributes) {
            bool end = false;
            for (int i = 0; i < 32; ++i) {
                EGLint key = attributes[2*i], actual = 0;
                if (key == EGL_NONE) { end = true; break; }
                const EGLint value = attributes[2*i+1];
                if (!session.eglGetConfigAttrib(session.display, session.config, key, &actual)) return DriverError();
                if (value == EGL_DONT_CARE) continue;
                switch (key) {
                    case EGL_SURFACE_TYPE: match &= (value & ~EGL_WINDOW_BIT) == 0 && (actual & value) == value; break;
                    case EGL_RENDERABLE_TYPE: match &= (value & ~EGL_OPENGL_ES2_BIT) == 0 && (actual & value) == value; break;
                    case EGL_RED_SIZE: case EGL_GREEN_SIZE: case EGL_BLUE_SIZE: case EGL_ALPHA_SIZE:
                    case EGL_DEPTH_SIZE: case EGL_STENCIL_SIZE: match &= actual >= value; break;
                    default: return EGL_BAD_ATTRIBUTE;
                }
            }
            if (!end) return EGL_BAD_ATTRIBUTE;
        }
        *count = match && (!out || capacity > 0) ? 1 : 0;
        if (*count && out) *out = session.configId;
        return EGL_SUCCESS;
    });
}

/// Creates the one unshared ES2 context and returns an opaque incarnation token.
EGLContext CreateContext(EGLDisplay d, EGLConfig config, EGLContext share, const EGLint* attrs) {
    EGLContext result = EGL_NO_CONTEXT;
    Run([&](uint64_t id) {
        if (auto error = CheckDisplay(d,id); error != EGL_SUCCESS) return error;
        if (config != session.configId) return EGL_BAD_CONFIG;
        if (share || !attrs || attrs[0] != EGL_CONTEXT_CLIENT_VERSION || attrs[1] != 2 || attrs[2] != EGL_NONE)
            return EGL_BAD_MATCH;
        if (session.context) return EGL_BAD_ALLOC;
        session.context = session.eglCreateContext(session.display, session.config, EGL_NO_CONTEXT, attrs);
        if (!session.context) return DriverError();
        session.lost = false;
        result = session.contextId = reinterpret_cast<void*>(nextId++);
        return EGL_SUCCESS;
    });
    return result;
}

/// Creates the one window surface from a managed native window reference.
EGLSurface CreateSurface(EGLDisplay d, EGLConfig config, void* native, const EGLint* attrs) {
    EGLSurface result = EGL_NO_SURFACE;
    Run([&](uint64_t id) {
        if (auto error = CheckDisplay(d,id); error != EGL_SUCCESS) return error;
        if (config != session.configId) return EGL_BAD_CONFIG;
        if (attrs && attrs[0] != EGL_NONE) return EGL_BAD_ATTRIBUTE;
        if (session.surfaceId) return EGL_BAD_ALLOC;
        HWND window;
        int width, height;
        if (!ResolveSurface(native, window, width, height) || !RetainSurface(native)) return EGL_BAD_NATIVE_WINDOW;
        session.native = native;
        session.window = window;
        if (auto error = EnsureSurface(0); error != EGL_SUCCESS) { DropSurface(false); return error; }
        result = session.surfaceId = reinterpret_cast<void*>(nextId++);
        return EGL_SUCCESS;
    });
    return result;
}

/// Binds the exact read/draw pair for the guest owner, or unbinds all three handles.
EGLBoolean MakeCurrent(EGLDisplay d, EGLSurface draw, EGLSurface read, EGLContext context) {
    return Run([=](uint64_t id) {
        if (auto error = CheckDisplay(d,id); error != EGL_SUCCESS) return error;
        if (!draw && !read && !context) return Unbind();
        if (!context || context != session.contextId) return EGL_BAD_CONTEXT;
        if (!draw || draw != read || draw != session.surfaceId) return EGL_BAD_SURFACE;
        return EnsureSurface(id);
    });
}

/// Presents the bound drawable and records ownership so bitmap painting cannot erase it.
EGLBoolean Swap(EGLDisplay d, EGLSurface surface) {
    return Run([=](uint64_t id) {
        if (auto error = CheckDisplay(d,id); error != EGL_SUCCESS) return error;
        if (!surface || surface != session.surfaceId || !session.native) return EGL_BAD_SURFACE;
        if (session.current != id) return EGL_BAD_CURRENT_SURFACE;
        if (auto error = EnsureSurface(id); error != EGL_SUCCESS) return error;
        if (!session.eglSwapBuffers(session.display, session.drawable)) return DriverError();
        session.presented = true;
        return EGL_SUCCESS;
    });
}

/// Queries the live surface's client extent, including zero while minimized.
EGLBoolean QuerySurface(EGLDisplay d, EGLSurface surface, EGLint key, EGLint* value) {
    return Run([=](uint64_t id) {
        if (auto error = CheckDisplay(d,id); error != EGL_SUCCESS) return error;
        if (!value) return EGL_BAD_PARAMETER;
        HWND window;
        int w,h;
        if (!surface || surface != session.surfaceId || !ResolveSurface(session.native, window, w, h)) return EGL_BAD_SURFACE;
        if (key != EGL_WIDTH && key != EGL_HEIGHT) return EGL_BAD_ATTRIBUTE;
        *value = key == EGL_WIDTH ? w : h;
        return EGL_SUCCESS;
    });
}

/// Destroys an unbound surface token; detached tokens are accepted for cleanup.
EGLBoolean DestroySurface(EGLDisplay d, EGLSurface surface) {
    return Run([=](uint64_t id) {
        if (auto error = CheckDisplay(d,id); error != EGL_SUCCESS) return error;
        if (!surface || surface != session.surfaceId) return EGL_BAD_SURFACE;
        if (session.current) return EGL_BAD_ACCESS;
        return DropSurface(false);
    });
}

/// Destroys an unbound context and invalidates its token.
EGLBoolean DestroyContext(EGLDisplay d, EGLContext context) {
    return Run([=](uint64_t id) {
        if (auto error = CheckDisplay(d,id); error != EGL_SUCCESS) return error;
        if (!context || context != session.contextId) return EGL_BAD_CONTEXT;
        if (session.current) return EGL_BAD_ACCESS;
        if (!session.eglDestroyContext(session.display, session.context)) return DriverError();
        session.context = EGL_NO_CONTEXT;
        session.contextId = nullptr;
        session.lost = false;
        return EGL_SUCCESS;
    });
}

/// Releases all objects in the guest display.
EGLBoolean Terminate(EGLDisplay d) {
    return Run([=](uint64_t id) {
        if (auto error = CheckDisplay(d,id); error != EGL_SUCCESS) return error;
        return Shutdown();
    });
}

/// Reports and clears the calling thread's EGL error.
EGLint Error() { const auto value = lastError; lastError = EGL_SUCCESS; return value; }

/// Reports bridge-owned strings; no host extensions are forwarded.
const char* QueryString(EGLDisplay d, EGLint key) {
    const char* result = nullptr;
    Run([&](uint64_t id) {
        if (auto error = CheckDisplay(d,id); error != EGL_SUCCESS) return error;
        switch (key) {
            case EGL_VENDOR: result = "Windows-ARM64.apk (ANGLE)"; break;
            case EGL_CLIENT_APIS: result = "OpenGL_ES"; break;
            case EGL_VERSION: result = "1.4 Windows-ARM64.apk restricted"; break;
            case EGL_EXTENSIONS: result = ""; break;
            default: return EGL_BAD_PARAMETER;
        }
        return EGL_SUCCESS;
    });
    return result;
}

/// Performs a GL command only with the calling guest's live current context.
template<class F> void RunGl(F&& f) {
    const uint64_t id = caller;
    GLenum error = GL_INVALID_OPERATION;
    Dispatch([&] {
        if (session.current != id || !session.native || EnsureSurface(id) != EGL_SUCCESS) return;
        f();
        error = GL_NO_ERROR;
    });
    if (error && !lastGlError) lastGlError = error;
}

#define VWRAP(name, args, call) \
    /** Executes the typed guest GL command on its current context. */ \
    void G_##name args { RunGl([&] { session.name call; }); }
#define RWRAP(type, name, args, call) \
    /** Returns the real result of the typed guest GL query. */ \
    type G_##name args { type result{}; RunGl([&] { result = session.name call; }); return result; }
GL_VOID(VWRAP)
GL_RESULT(RWRAP)
#undef RWRAP
#undef VWRAP

/// Returns a queued bridge error or the current context's driver error.
GLenum GlError() {
    if (lastGlError) { const auto value = lastGlError; lastGlError = GL_NO_ERROR; return value; }
    GLenum result = GL_NO_ERROR;
    const auto id = caller;
    Dispatch([&] { if (session.current == id) result = session.glGetError(); });
    return result;
}

/// Copies supported driver strings into guest-thread storage, never exposing driver addresses.
const GLubyte* GlString(GLenum key) {
    struct Strings { char values[3][512]{}; };
    thread_local Strings strings;
    auto* local = &strings;
    const GLubyte* result = nullptr;
    RunGl([&] {
        if (key == GL_EXTENSIONS) { result = reinterpret_cast<const GLubyte*>(""); return; }
        if (key == GL_VERSION) { result = reinterpret_cast<const GLubyte*>("OpenGL ES 2.0 Windows-ARM64.apk restricted"); return; }
        const auto* value = session.glGetString(key);
        if (!value) return;
        auto& out = local->values[key == GL_VENDOR ? 0 : key == GL_RENDERER ? 1 : 2];
        std::snprintf(out, sizeof out, "%s", reinterpret_cast<const char*>(value));
        result = reinterpret_cast<const GLubyte*>(out);
    });
    return result;
}

using Proc = void (*)();
Proc GetProc(const char* name);

#define SYMBOL(name, fn) {#name, reinterpret_cast<void*>(&fn)}
const guest::HostSymbol eglSymbols[] = {
    SYMBOL(eglGetDisplay, GetDisplay), SYMBOL(eglInitialize, Initialize), SYMBOL(eglChooseConfig, ChooseConfig),
    SYMBOL(eglCreateWindowSurface, CreateSurface), SYMBOL(eglCreateContext, CreateContext), SYMBOL(eglMakeCurrent, MakeCurrent),
    SYMBOL(eglSwapBuffers, Swap), SYMBOL(eglQuerySurface, QuerySurface), SYMBOL(eglDestroySurface, DestroySurface),
    SYMBOL(eglDestroyContext, DestroyContext), SYMBOL(eglTerminate, Terminate), SYMBOL(eglQueryString, QueryString),
    SYMBOL(eglGetError, Error), SYMBOL(eglGetProcAddress, GetProc),
};
#define VSYMBOL(name, args, call) SYMBOL(name, G_##name),
#define RSYMBOL(type, name, args, call) SYMBOL(name, G_##name),
const guest::HostSymbol glSymbols[] = {
    GL_VOID(VSYMBOL) GL_RESULT(RSYMBOL)
    SYMBOL(glGetError, GlError), SYMBOL(glGetString, GlString),
};
#undef RSYMBOL
#undef VSYMBOL
#undef SYMBOL

/// Returns only the same fixed guest trampolines admitted as ELF imports.
Proc GetProc(const char* name) {
    if (!name || strnlen(name, 128) == 128) return nullptr;
    for (const auto& entry : eglSymbols) if (!std::strcmp(entry.name,name)) return reinterpret_cast<Proc>(entry.address);
    for (const auto& entry : glSymbols) if (!std::strcmp(entry.name,name)) return reinterpret_cast<Proc>(entry.address);
    return nullptr;
}

} // namespace

void AddGuestGraphicsLibraries(std::vector<guest::BuiltinLibrary>& libraries) {
    libraries.push_back({"libEGL.so", {std::begin(eglSymbols), std::end(eglSymbols)}, ""});
    libraries.push_back({"libGLESv2.so", {std::begin(glSymbols), std::end(glSymbols)}, ""});
}

bool GuestGraphicsActive() { return session.owner != 0; }
bool GuestGraphicsPresents(HWND window) { return window == session.window && session.presented; }
void RetireGuestGraphics(HWND window, bool closing) {
    if (!session.owner || (session.window && session.window != window)) return;
    if (closing) Shutdown();
    else if (session.window == window && session.native) DropSurface(true);
}

} // namespace winapk::bridge
#else
namespace winapk::bridge {
void AddGuestGraphicsLibraries(std::vector<guest::BuiltinLibrary>&) {}
bool GuestGraphicsActive() { return false; }
bool GuestGraphicsPresents(HWND) { return false; }
void RetireGuestGraphics(HWND, bool) {}
} // namespace winapk::bridge
#endif
