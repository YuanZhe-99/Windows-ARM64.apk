// Windows host EGL and the bounded Java GLES20 JNI surface.
#include "graphics.h"
#include "bridge.h"

#ifdef WINAPK_HAS_ANGLE_HEADERS
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <EGL/eglext_angle.h>
#include <GLES2/gl2.h>
#include <cstring>
#include <filesystem>
#include <utility>
#endif

namespace winapk::bridge {

#ifdef WINAPK_HAS_ANGLE_HEADERS
namespace {

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

#define GL_ENTRIES(X) \
    X(PFNGLCLEARPROC, glClear) \
    X(PFNGLCLEARCOLORPROC, glClearColor) \
    X(PFNGLDISABLEPROC, glDisable) \
    X(PFNGLENABLEPROC, glEnable) \
    X(PFNGLGETERRORPROC, glGetError) \
    X(PFNGLGETSTRINGPROC, glGetString) \
    X(PFNGLREADPIXELSPROC, glReadPixels) \
    X(PFNGLSCISSORPROC, glScissor) \
    X(PFNGLVIEWPORTPROC, glViewport)

/// Holds only the driver entry points used by the Java GLES20 profile.
struct Gl {
#define FIELD(Type, Name) Type Name = nullptr;
    GL_ENTRIES(FIELD)
};
thread_local const Gl* currentGl = nullptr;

/// Finds the fixed private ANGLE directory relative to the loaded bridge, never to the APK.
std::filesystem::path AngleDirectory() {
    HMODULE bridge = nullptr;
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCWSTR>(&AngleDirectory), &bridge)) return {};
    std::wstring file(32768, L'\0');
    const DWORD n = GetModuleFileNameW(bridge, file.data(), static_cast<DWORD>(file.size()));
    if (!n || n >= file.size()) return {};
    file.resize(n);
    return std::filesystem::path(file).parent_path() / L"angle";
}

/// Throws a Java exception without replacing one already in flight.
void Throw(JNIEnv* env, const char* type, const char* message) {
    if (env->ExceptionCheck()) return;
    jclass cls = env->FindClass(type);
    if (cls) { env->ThrowNew(cls, message); env->DeleteLocalRef(cls); }
}

/// Rejects calls made outside a host-owned current context, including on another Java thread.
const Gl* RequireGl(JNIEnv* env) {
    if (!currentGl) Throw(env, "java/lang/IllegalStateException", "GRAPHICS_CAPABILITY_MISSING: no current Windows GLES context");
    return currentGl;
}

/// Clears the requested buffers on the calling thread's current context.
void JNICALL Clear(JNIEnv* env, jclass, jint mask) {
    if (auto* gl = RequireGl(env)) gl->glClear(static_cast<GLbitfield>(mask));
}
/// Sets the color used by subsequent clears.
void JNICALL ClearColor(JNIEnv* env, jclass, jfloat r, jfloat g, jfloat b, jfloat a) {
    if (auto* gl = RequireGl(env)) gl->glClearColor(r, g, b, a);
}
/// Enables a GLES capability.
void JNICALL Enable(JNIEnv* env, jclass, jint cap) {
    if (auto* gl = RequireGl(env)) gl->glEnable(static_cast<GLenum>(cap));
}
/// Disables a GLES capability.
void JNICALL Disable(JNIEnv* env, jclass, jint cap) {
    if (auto* gl = RequireGl(env)) gl->glDisable(static_cast<GLenum>(cap));
}
/// Returns and clears the actual GLES error.
jint JNICALL Error(JNIEnv* env, jclass) {
    auto* gl = RequireGl(env);
    return gl ? static_cast<jint>(gl->glGetError()) : 0;
}
/// Returns a driver string; the Java bridge exposes no extension entry points.
jstring JNICALL String(JNIEnv* env, jclass, jint name) {
    auto* gl = RequireGl(env);
    if (!gl) return nullptr;
    if (name == GL_EXTENSIONS) return env->NewStringUTF("");
    const auto* text = gl->glGetString(static_cast<GLenum>(name));
    return text ? env->NewStringUTF(reinterpret_cast<const char*>(text)) : nullptr;
}
/// Sets the GLES scissor rectangle in drawable pixels.
void JNICALL Scissor(JNIEnv* env, jclass, jint x, jint y, jint w, jint h) {
    if (auto* gl = RequireGl(env)) gl->glScissor(x, y, w, h);
}
/// Sets the GLES viewport in drawable pixels.
void JNICALL Viewport(JNIEnv* env, jclass, jint x, jint y, jint w, jint h) {
    if (auto* gl = RequireGl(env)) gl->glViewport(x, y, w, h);
}
/// Reads RGBA8 into the writable range [position, limit) of a direct ByteBuffer.
void JNICALL ReadPixels(JNIEnv* env, jclass, jint x, jint y, jint w, jint h, jint format, jint type, jobject dst) {
    auto* gl = RequireGl(env);
    if (!gl) return;
    jclass bytes = env->FindClass("java/nio/ByteBuffer");
    if (!bytes) return;
    const bool byteBuffer = dst && env->IsInstanceOf(dst, bytes);
    auto* data = byteBuffer ? static_cast<unsigned char*>(env->GetDirectBufferAddress(dst)) : nullptr;
    if (data && w >= 0 && h >= 0 && format == GL_RGBA && type == GL_UNSIGNED_BYTE) {
        const auto position = env->GetMethodID(bytes, "position", "()I");
        const auto remaining = env->GetMethodID(bytes, "remaining", "()I");
        const auto readOnly = env->GetMethodID(bytes, "isReadOnly", "()Z");
        const jint offset = env->CallIntMethod(dst, position);
        if (env->ExceptionCheck()) { env->DeleteLocalRef(bytes); return; }
        const jint space = env->CallIntMethod(dst, remaining);
        if (env->ExceptionCheck()) { env->DeleteLocalRef(bytes); return; }
        const bool writable = !env->CallBooleanMethod(dst, readOnly);
        if (env->ExceptionCheck()) { env->DeleteLocalRef(bytes); return; }
        // RGBA8 rows need no additional padding at the default pack alignment of four.
        const jlong capacity = env->GetDirectBufferCapacity(dst);
        const auto needed = 4ull * static_cast<unsigned>(w) * static_cast<unsigned>(h);
        if (writable && offset >= 0 && space >= 0 && capacity >= offset &&
            needed <= static_cast<unsigned>(space) && needed <= static_cast<uint64_t>(capacity - offset)) {
            gl->glReadPixels(x, y, w, h, GL_RGBA, GL_UNSIGNED_BYTE, data + offset);
            env->DeleteLocalRef(bytes);
            return;
        }
    }
    env->DeleteLocalRef(bytes);
    Throw(env, "java/lang/IllegalArgumentException", "Windows GLES20 readback requires a writable direct ByteBuffer with room for RGBA8");
}

} // namespace

/// Holds one host-owned window drawable, EGL context and their DLL leases.
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
    Gl gl;
    EGL_ENTRIES(FIELD)
#undef FIELD

    /// Releases native resources in their ownership order; partial initialization is safe.
    ~State() {
        bool ok = true;
        if (bound) {
            currentGl = nullptr;
            ok = eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT) == EGL_TRUE;
        }
        if (surface != EGL_NO_SURFACE) ok = (eglDestroySurface(display, surface) == EGL_TRUE) && ok;
        if (context != EGL_NO_CONTEXT) ok = (eglDestroyContext(display, context) == EGL_TRUE) && ok;
        if (initialized) {
            ok = (eglTerminate(display) == EGL_TRUE) && ok;
            LogLine(ok ? "[winapk] Java GLES teardown complete" : "GRAPHICS_RENDER_FAILED: Java GLES teardown");
        }
        if (egl) FreeLibrary(egl);
        if (gles) FreeLibrary(gles);
    }

    /// Loads the two pinned build outputs and resolves the fixed ABI entries.
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
#define LOAD_GL(Type, Name) \
        gl.Name = reinterpret_cast<Type>(GetProcAddress(gles, #Name)); \
        if (!gl.Name) { error = "missing ANGLE entry " #Name; return false; }
        GL_ENTRIES(LOAD_GL)
#undef LOAD_GL
        return true;
    }
};

/// Adopts a fully owned EGL state.
JavaGraphics::JavaGraphics(std::unique_ptr<State> state) : state_(std::move(state)) {}
JavaGraphics::~JavaGraphics() = default;

std::shared_ptr<JavaGraphics> JavaGraphics::Open(HWND window, const std::string& request, std::string& error) {
    const auto directory = AngleDirectory();
    const bool automatic = request == "auto";
    const int count = automatic ? 2 : 1;
    for (int i = 0; i < count; ++i) {
        const bool vulkan = request == "vulkan" || (automatic && i == 0);
        const char* backend = vulkan ? "vulkan" : "d3d11";
        auto state = std::make_unique<State>();
        state->window = window;
        if (!state->Load(directory, error)) return {};
        constexpr EGLint configArgs[] = {EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
            EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT, EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8,
            EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8, EGL_NONE};
        constexpr EGLint contextArgs[] = {EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE};
        const EGLint displayArgs[] = {
            EGL_PLATFORM_ANGLE_TYPE_ANGLE, vulkan ? EGL_PLATFORM_ANGLE_TYPE_VULKAN_ANGLE : EGL_PLATFORM_ANGLE_TYPE_D3D11_ANGLE,
            EGL_PLATFORM_ANGLE_DEVICE_TYPE_ANGLE, EGL_PLATFORM_ANGLE_DEVICE_TYPE_HARDWARE_ANGLE, EGL_NONE};
        auto getDisplay = reinterpret_cast<PFNEGLGETPLATFORMDISPLAYEXTPROC>(state->eglGetProcAddress("eglGetPlatformDisplayEXT"));
        if (getDisplay) state->display = getDisplay(EGL_PLATFORM_ANGLE_ANGLE, EGL_DEFAULT_DISPLAY, displayArgs);
        EGLint major = 0, minor = 0, configs = 0;
        state->initialized = state->display != EGL_NO_DISPLAY && state->eglInitialize(state->display, &major, &minor);
        if (state->initialized && state->eglBindAPI(EGL_OPENGL_ES_API) &&
            state->eglChooseConfig(state->display, configArgs, &state->config, 1, &configs) && configs > 0) {
            state->context = state->eglCreateContext(state->display, state->config, EGL_NO_CONTEXT, contextArgs);
            if (state->context != EGL_NO_CONTEXT) {
                auto graphics = std::shared_ptr<JavaGraphics>(new JavaGraphics(std::move(state)));
                RECT rect{};
                if (GetClientRect(window, &rect) && graphics->Begin(rect.right, rect.bottom, error)) {
                    const auto* renderer = reinterpret_cast<const char*>(currentGl->glGetString(GL_RENDERER));
                    const auto* version = reinterpret_cast<const char*>(currentGl->glGetString(GL_VERSION));
                    const bool matches = renderer && (vulkan ? std::strstr(renderer, "Vulkan") :
                        (std::strstr(renderer, "Direct3D11") ? renderer : std::strstr(renderer, "D3D11")));
                    if (matches && version) {
                        LogLine("[winapk] Java GLES request=" + request + " backend=" + backend + " renderer=" +
                            renderer + " gl=" + version + " egl=" + std::to_string(major) + "." + std::to_string(minor));
                        if (graphics->End(false, error)) return graphics;
                    } else {
                        graphics->End(false, error);
                        error = "actual renderer did not match the requested backend";
                    }
                }
            } else error = "eglCreateContext error=" + std::to_string(state->eglGetError());
        } else error = "EGL initialization/configuration error=" + std::to_string(state->eglGetError());
        error = std::string("backend=") + backend + " " + error;
        if (automatic && i == 0) LogLine("[winapk] Java GLES auto: " + error + "; trying d3d11");
    }
    if (!automatic) error += "; no fallback";
    return {};
}

bool JavaGraphics::Begin(int width, int height, std::string& error) {
    auto& s = *state_;
    if (currentGl) { error = "nested Windows GLES frame"; return false; }
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
    currentGl = &s.gl;
    return true;
}

bool JavaGraphics::End(bool present, std::string& error) {
    auto& s = *state_;
    const bool swapped = !present || s.eglSwapBuffers(s.display, s.surface);
    if (!swapped) error = "eglSwapBuffers error=" + std::to_string(s.eglGetError());
    currentGl = nullptr;
    s.bound = !s.eglMakeCurrent(s.display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    if (s.bound) error = "eglMakeCurrent unbind error=" + std::to_string(s.eglGetError());
    return swapped && !s.bound;
}

bool RegisterJavaGraphics(JNIEnv* env) {
#define M(Name, Signature, Function) {const_cast<char*>(Name), const_cast<char*>(Signature), reinterpret_cast<void*>(&Function)}
    const JNINativeMethod methods[] = {
        M("glClear", "(I)V", Clear),
        M("glClearColor", "(FFFF)V", ClearColor),
        M("glDisable", "(I)V", Disable),
        M("glEnable", "(I)V", Enable),
        M("glGetError", "()I", Error),
        M("glGetString", "(I)Ljava/lang/String;", String),
        M("glReadPixels", "(IIIIIILjava/nio/Buffer;)V", ReadPixels),
        M("glScissor", "(IIII)V", Scissor),
        M("glViewport", "(IIII)V", Viewport),
    };
#undef M
    return Bind(env, "android/opengl/GLES20", methods, static_cast<int>(std::size(methods)));
}

#else

/// Opaque state is never created in a build without ANGLE headers.
struct JavaGraphics::State {};
JavaGraphics::~JavaGraphics() = default;
std::shared_ptr<JavaGraphics> JavaGraphics::Open(HWND, const std::string&, std::string& error) {
    error = "bridge built without pinned ANGLE headers; build Angle, then Host";
    return {};
}
bool JavaGraphics::Begin(int, int, std::string&) { return false; } // Open never creates a session.
bool JavaGraphics::End(bool, std::string&) { return false; }
bool RegisterJavaGraphics(JNIEnv*) { return true; } // No GLES natives registered on a Java-only build.

#endif
} // namespace winapk::bridge
