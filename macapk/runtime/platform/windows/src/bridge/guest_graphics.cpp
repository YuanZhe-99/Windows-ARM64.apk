// guest_graphics.cpp -- Full multi-threaded Android guest EGL 1.5 + OpenGL ES 1.1/2.0/3.0/3.1
// bridge over private ARM64 ANGLE and WinApkCompositor (GPU-P3, ADR-0005).
//
// Architecture:
//   - Guest threads execute GLES commands directly on the calling thread against ANGLE (no
//     Win32 UI-thread serialization bottleneck or cross-thread deadlocks).
//   - Multiple `EGLContext`s, `share_context` resource sharing across render and IO worker
//     threads, `eglCreatePbufferSurface`, `EGL_KHR_surfaceless_context`, `EGLImageKHR` and
//     `EGLSyncKHR` are supported.
//   - Guest window surfaces (`ANativeWindow*`) are backed by dynamically resized ANGLE Pbuffers
//     composited in SurfaceFlinger Z-order by `WinApkCompositor` (zero-copy DXGI shared texture
//     on D3D11, FBO-0 RGBA8 transfer on Vulkan), working identically in standalone windows and
//     `--embed` panes without `HWND` swapchain collisions.
#include "guest_graphics.h"
#include "angle_runtime.h"
#include "bridge.h"
#include "compositor.h"
#include "embed.h"
#include "guest_vulkan.h"
#include "surface.h"

#ifdef WINAPK_HAS_ANGLE_HEADERS
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <EGL/eglext_angle.h>
#include <GLES/gl.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <GLES3/gl32.h>

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace winapk::bridge {
namespace {

static_assert(sizeof(EGLint) == 4 && sizeof(EGLBoolean) == 4 && sizeof(GLint) == 4 &&
              sizeof(GLenum) == 4 && sizeof(GLfloat) == 4 && sizeof(GLboolean) == 1 &&
              sizeof(GLintptr) == 8 && sizeof(GLsizeiptr) == 8 &&
              sizeof(GLint64) == 8 && sizeof(GLuint64) == 8 &&
              sizeof(EGLDisplay) == 8 && sizeof(EGLSurface) == 8 && sizeof(EGLContext) == 8 &&
              sizeof(EGLImageKHR) == 8 && sizeof(EGLSyncKHR) == 8);

constexpr uintptr_t kLogicalDisplay = 1;

struct GuestConfigEntry {
    EGLConfig hostConfig = nullptr;
    EGLint renderableMask = EGL_OPENGL_ES2_BIT;
};

struct GuestContextEntry {
    EGLContext hostContext = nullptr;
    EGLConfig guestConfig = nullptr;
    int clientMajor = 2;
    uint64_t boundThread = 0;
};

struct GuestSurfaceEntry {
    bool isWindow = false;
    EGLConfig hostConfig = nullptr;
    EGLSurface hostPbuffer = EGL_NO_SURFACE;
    void* nativeToken = nullptr;
    HWND hwnd = nullptr;
    int width = 0, height = 0;
    uint64_t boundThread = 0;
    bool presented = false;
};

struct AngleDispatch {
    HMODULE egl = nullptr;
    HMODULE gles = nullptr;
    EGLDisplay display = EGL_NO_DISPLAY;
    const char* backend = nullptr;
    int maxEsMajor = 2;
    int initRefs = 0;
    bool lost = false;
    bool injectLoss = false;
    bool lossInjected = false;
    bool loggedRenderer = false;

    PFNEGLGETPROCADDRESSPROC eglGetProcAddress = nullptr;
    PFNEGLINITIALIZEPROC eglInitialize = nullptr;
    PFNEGLTERMINATEPROC eglTerminate = nullptr;
    PFNEGLGETERRORPROC eglGetError = nullptr;
    PFNEGLQUERYSTRINGPROC eglQueryString = nullptr;
    PFNEGLBINDAPIPROC eglBindAPI = nullptr;
    PFNEGLQUERYAPIPROC eglQueryAPI = nullptr;
    PFNEGLWAITCLIENTPROC eglWaitClient = nullptr;
    PFNEGLRELEASETHREADPROC eglReleaseThread = nullptr;
    PFNEGLGETCONFIGSPROC eglGetConfigs = nullptr;
    PFNEGLCHOOSECONFIGPROC eglChooseConfig = nullptr;
    PFNEGLGETCONFIGATTRIBPROC eglGetConfigAttrib = nullptr;
    PFNEGLCREATEPBUFFERSURFACEPROC eglCreatePbufferSurface = nullptr;
    PFNEGLDESTROYSURFACEPROC eglDestroySurface = nullptr;
    PFNEGLQUERYSURFACEPROC eglQuerySurface = nullptr;
    PFNEGLSURFACEATTRIBPROC eglSurfaceAttrib = nullptr;
    PFNEGLBINDTEXIMAGEPROC eglBindTexImage = nullptr;
    PFNEGLRELEASETEXIMAGEPROC eglReleaseTexImage = nullptr;
    PFNEGLSWAPINTERVALPROC eglSwapInterval = nullptr;
    PFNEGLCREATECONTEXTPROC eglCreateContext = nullptr;
    PFNEGLDESTROYCONTEXTPROC eglDestroyContext = nullptr;
    PFNEGLMAKECURRENTPROC eglMakeCurrent = nullptr;
    PFNEGLQUERYCONTEXTPROC eglQueryContext = nullptr;
    PFNEGLWAITGLPROC eglWaitGL = nullptr;
    PFNEGLWAITNATIVEPROC eglWaitNative = nullptr;
    PFNEGLQUERYSURFACEPOINTERANGLEPROC eglQuerySurfacePointerANGLE = nullptr;

    PFNEGLCREATEIMAGEKHRPROC eglCreateImageKHR = nullptr;
    PFNEGLDESTROYIMAGEKHRPROC eglDestroyImageKHR = nullptr;
    PFNEGLCREATESYNCKHRPROC eglCreateSyncKHR = nullptr;
    PFNEGLDESTROYSYNCKHRPROC eglDestroySyncKHR = nullptr;
    PFNEGLCLIENTWAITSYNCKHRPROC eglClientWaitSyncKHR = nullptr;
    PFNEGLWAITSYNCKHRPROC eglWaitSyncKHR = nullptr;
    PFNEGLGETSYNCATTRIBKHRPROC eglGetSyncAttribKHR = nullptr;

    PFNGLGETERRORPROC glGetError = nullptr;
    PFNGLGETSTRINGPROC glGetString = nullptr;
    PFNGLGETSTRINGIPROC glGetStringi = nullptr;
    PFNGLFLUSHPROC glFlush = nullptr;
    PFNGLFINISHPROC glFinish = nullptr;
    PFNGLREADPIXELSPROC glReadPixels = nullptr;
    PFNGLBINDFRAMEBUFFERPROC glBindFramebuffer = nullptr;
    PFNGLGETINTEGERVPROC glGetIntegerv = nullptr;
};

std::mutex g_mu;
AngleDispatch g_drv;
uintptr_t g_nextHandle = 0x100;
std::map<void*, GuestConfigEntry> g_configs;
std::map<void*, EGLConfig> g_hostToGuestConfig;
std::map<void*, GuestContextEntry> g_contexts;
std::map<void*, GuestSurfaceEntry> g_surfaces;
std::map<void*, EGLImageKHR> g_images;
std::map<void*, EGLSyncKHR> g_syncs;

std::atomic<uint64_t> g_nextThreadId{1};
struct GuestThreadState {
    uint64_t id = g_nextThreadId.fetch_add(1, std::memory_order_relaxed);
    EGLContext guestCtx = EGL_NO_CONTEXT;
    EGLContext hostCtx = EGL_NO_CONTEXT;
    EGLSurface guestDraw = EGL_NO_SURFACE;
    EGLSurface guestRead = EGL_NO_SURFACE;
    EGLint eglError = EGL_SUCCESS;
    GLenum unboundGlError = GL_NO_ERROR;
    char strBuf[4][2048]{};
    unsigned strSlot = 0;
};
thread_local GuestThreadState t_tls;

EGLint SetEglError(EGLint err) {
    t_tls.eglError = err;
    return err;
}

bool GuestGlReady() {
    if (t_tls.hostCtx != EGL_NO_CONTEXT) return true;
    if (!t_tls.unboundGlError) t_tls.unboundGlError = GL_INVALID_OPERATION;
    return false;
}

void NoteGlUnsupported() {
    if (!t_tls.unboundGlError) t_tls.unboundGlError = GL_INVALID_OPERATION;
}

void* ResolveAngleGl(const char* name) {
    if (!name || !g_drv.gles) return nullptr;
    if (void* p = reinterpret_cast<void*>(GetProcAddress(g_drv.gles, name))) return p;
    return g_drv.eglGetProcAddress ? reinterpret_cast<void*>(g_drv.eglGetProcAddress(name)) : nullptr;
}

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-function"
#include "guest_gles_autogen.inc"
#pragma clang diagnostic pop

GLenum GL_APIENTRY G_glGetError() {
    if (t_tls.unboundGlError != GL_NO_ERROR) {
        const GLenum e = t_tls.unboundGlError;
        t_tls.unboundGlError = GL_NO_ERROR;
        return e;
    }
    if (t_tls.hostCtx == EGL_NO_CONTEXT || !g_drv.glGetError) return GL_NO_ERROR;
    return g_drv.glGetError();
}

const GLubyte* GL_APIENTRY G_glGetString(GLenum name) {
    if (!GuestGlReady() || !g_drv.glGetString) return nullptr;
    const GLubyte* s = g_drv.glGetString(name);
    if (!s) return nullptr;
    char* dst = t_tls.strBuf[(t_tls.strSlot++) & 3];
    std::snprintf(dst, sizeof(t_tls.strBuf[0]), "%s", reinterpret_cast<const char*>(s));
    return reinterpret_cast<const GLubyte*>(dst);
}

const GLubyte* GL_APIENTRY G_glGetStringi(GLenum name, GLuint index) {
    if (!GuestGlReady() || !g_drv.glGetStringi) return nullptr;
    const GLubyte* s = g_drv.glGetStringi(name, index);
    if (!s) return nullptr;
    char* dst = t_tls.strBuf[(t_tls.strSlot++) & 3];
    std::snprintf(dst, sizeof(t_tls.strBuf[0]), "%s", reinterpret_cast<const char*>(s));
    return reinterpret_cast<const GLubyte*>(dst);
}

// Debug callbacks from guest code are safely ignored at the host driver boundary so raw guest
// code pointers are never invoked on driver worker threads without `x18`/TLS context.
void GL_APIENTRY G_glDebugMessageCallback(GLDEBUGPROC, const void*) {
    if (!GuestGlReady()) return;
}
void GL_APIENTRY G_glDebugMessageCallbackKHR(GLDEBUGPROCKHR, const void*) {
    if (!GuestGlReady()) return;
}

void GL_APIENTRY G_glEGLImageTargetTexture2DOES(GLenum target, GLeglImageOES image) {
    if (!GuestGlReady()) return;
    EGLImageKHR hostImg = EGL_NO_IMAGE_KHR;
    {
        std::lock_guard lock(g_mu);
        auto it = g_images.find(image);
        if (it == g_images.end()) { NoteGlUnsupported(); return; }
        hostImg = it->second;
    }
    static auto fn = reinterpret_cast<PFNGLEGLIMAGETARGETTEXTURE2DOESPROC>(ResolveAngleGl("glEGLImageTargetTexture2DOES"));
    if (fn) fn(target, static_cast<GLeglImageOES>(hostImg));
    else NoteGlUnsupported();
}

void GL_APIENTRY G_glEGLImageTargetRenderbufferStorageOES(GLenum target, GLeglImageOES image) {
    if (!GuestGlReady()) return;
    EGLImageKHR hostImg = EGL_NO_IMAGE_KHR;
    {
        std::lock_guard lock(g_mu);
        auto it = g_images.find(image);
        if (it == g_images.end()) { NoteGlUnsupported(); return; }
        hostImg = it->second;
    }
    static auto fn = reinterpret_cast<PFNGLEGLIMAGETARGETRENDERBUFFERSTORAGEOESPROC>(ResolveAngleGl("glEGLImageTargetRenderbufferStorageOES"));
    if (fn) fn(target, static_cast<GLeglImageOES>(hostImg));
    else NoteGlUnsupported();
}

void ShutdownLocked() {
    if (t_tls.hostCtx != EGL_NO_CONTEXT && g_drv.display != EGL_NO_DISPLAY && g_drv.eglMakeCurrent) {
        g_drv.eglMakeCurrent(g_drv.display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        t_tls.hostCtx = EGL_NO_CONTEXT;
        t_tls.guestCtx = EGL_NO_CONTEXT;
        t_tls.guestDraw = t_tls.guestRead = EGL_NO_SURFACE;
    }
    for (auto& [id, img] : g_images) {
        if (img && g_drv.eglDestroyImageKHR) g_drv.eglDestroyImageKHR(g_drv.display, img);
    }
    g_images.clear();
    for (auto& [id, syn] : g_syncs) {
        if (syn && g_drv.eglDestroySyncKHR) g_drv.eglDestroySyncKHR(g_drv.display, syn);
    }
    g_syncs.clear();
    for (auto& [id, surf] : g_surfaces) {
        if (surf.hostPbuffer != EGL_NO_SURFACE && g_drv.eglDestroySurface) {
            g_drv.eglDestroySurface(g_drv.display, surf.hostPbuffer);
        }
        if (surf.nativeToken) {
            if (surf.hwnd) {
                const int key = -static_cast<int>(reinterpret_cast<uintptr_t>(surf.nativeToken));
                CompositorUpdateSurfacePlacement(surf.hwnd, key, {}, true);
            }
            ReleaseSurface(surf.nativeToken);
        }
    }
    g_surfaces.clear();
    for (auto& [id, ctx] : g_contexts) {
        if (ctx.hostContext != EGL_NO_CONTEXT && g_drv.eglDestroyContext) {
            g_drv.eglDestroyContext(g_drv.display, ctx.hostContext);
        }
    }
    g_contexts.clear();
    g_configs.clear();
    g_hostToGuestConfig.clear();

    bool ok = true;
    if (g_drv.display != EGL_NO_DISPLAY && g_drv.eglTerminate) {
        ok = g_drv.eglTerminate(g_drv.display) == EGL_TRUE;
        LogLine(ok ? "[winapk] guest EGL teardown complete" : "GRAPHICS_RENDER_FAILED: guest EGL teardown");
    }
    if (g_drv.egl) FreeLibrary(g_drv.egl);
    if (g_drv.gles) FreeLibrary(g_drv.gles);
    g_drv = {};
}

std::string QueryGuestGraphicsRequest() {
    std::string req = "off";
    if (!DispatchGuestGraphics([&] { req = GuestGraphicsRequest(); })) {
        // Fallback when called off-main while loop is active: read JVM system property captured at boot.
        req = "auto";
    }
    return req;
}

EGLint InitDriverLocked(const std::string& request) {
    if (request == "off" || GuestVulkanOwnsWindow()) return EGL_NOT_INITIALIZED;
    const std::wstring dir = PrivateAngleDirectory();
    if (dir.empty()) return EGL_NOT_INITIALIZED;
    constexpr DWORD flags = LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32;
    g_drv.gles = LoadLibraryExW((dir + L"\\libGLESv2.dll").c_str(), nullptr, flags);
    g_drv.egl = g_drv.gles ? LoadLibraryExW((dir + L"\\libEGL.dll").c_str(), nullptr, flags) : nullptr;
    if (!g_drv.egl || !g_drv.gles) { ShutdownLocked(); return EGL_NOT_INITIALIZED; }

#define REQ_EGL(name) \
    g_drv.name = reinterpret_cast<decltype(g_drv.name)>(GetProcAddress(g_drv.egl, #name)); \
    if (!g_drv.name) { ShutdownLocked(); return EGL_NOT_INITIALIZED; }
    REQ_EGL(eglGetProcAddress)
    REQ_EGL(eglInitialize)
    REQ_EGL(eglTerminate)
    REQ_EGL(eglGetError)
    REQ_EGL(eglQueryString)
    REQ_EGL(eglBindAPI)
    REQ_EGL(eglQueryAPI)
    REQ_EGL(eglWaitClient)
    REQ_EGL(eglReleaseThread)
    REQ_EGL(eglGetConfigs)
    REQ_EGL(eglChooseConfig)
    REQ_EGL(eglGetConfigAttrib)
    REQ_EGL(eglCreatePbufferSurface)
    REQ_EGL(eglDestroySurface)
    REQ_EGL(eglQuerySurface)
    REQ_EGL(eglSurfaceAttrib)
    REQ_EGL(eglBindTexImage)
    REQ_EGL(eglReleaseTexImage)
    REQ_EGL(eglSwapInterval)
    REQ_EGL(eglCreateContext)
    REQ_EGL(eglDestroyContext)
    REQ_EGL(eglMakeCurrent)
    REQ_EGL(eglQueryContext)
    REQ_EGL(eglWaitGL)
    REQ_EGL(eglWaitNative)
#undef REQ_EGL

#define REQ_GL(name) \
    g_drv.name = reinterpret_cast<decltype(g_drv.name)>(GetProcAddress(g_drv.gles, #name)); \
    if (!g_drv.name) { ShutdownLocked(); return EGL_NOT_INITIALIZED; }
    REQ_GL(glGetError)
    REQ_GL(glGetString)
    REQ_GL(glFlush)
    REQ_GL(glFinish)
    REQ_GL(glReadPixels)
    REQ_GL(glBindFramebuffer)
    REQ_GL(glGetIntegerv)
#undef REQ_GL
    g_drv.glGetStringi = reinterpret_cast<PFNGLGETSTRINGIPROC>(ResolveAngleGl("glGetStringi"));
    g_drv.eglQuerySurfacePointerANGLE = reinterpret_cast<PFNEGLQUERYSURFACEPOINTERANGLEPROC>(
        g_drv.eglGetProcAddress("eglQuerySurfacePointerANGLE"));
    g_drv.eglCreateImageKHR = reinterpret_cast<PFNEGLCREATEIMAGEKHRPROC>(g_drv.eglGetProcAddress("eglCreateImageKHR"));
    g_drv.eglDestroyImageKHR = reinterpret_cast<PFNEGLDESTROYIMAGEKHRPROC>(g_drv.eglGetProcAddress("eglDestroyImageKHR"));
    g_drv.eglCreateSyncKHR = reinterpret_cast<PFNEGLCREATESYNCKHRPROC>(g_drv.eglGetProcAddress("eglCreateSyncKHR"));
    g_drv.eglDestroySyncKHR = reinterpret_cast<PFNEGLDESTROYSYNCKHRPROC>(g_drv.eglGetProcAddress("eglDestroySyncKHR"));
    g_drv.eglClientWaitSyncKHR = reinterpret_cast<PFNEGLCLIENTWAITSYNCKHRPROC>(g_drv.eglGetProcAddress("eglClientWaitSyncKHR"));
    g_drv.eglWaitSyncKHR = reinterpret_cast<PFNEGLWAITSYNCKHRPROC>(g_drv.eglGetProcAddress("eglWaitSyncKHR"));
    g_drv.eglGetSyncAttribKHR = reinterpret_cast<PFNEGLGETSYNCATTRIBKHRPROC>(g_drv.eglGetProcAddress("eglGetSyncAttribKHR"));

    auto getPlat = reinterpret_cast<PFNEGLGETPLATFORMDISPLAYEXTPROC>(g_drv.eglGetProcAddress("eglGetPlatformDisplayEXT"));
    if (!getPlat) { ShutdownLocked(); return EGL_NOT_INITIALIZED; }

    bool nativeVk = false;
    const auto candidates = SelectAngleBackends(request, &nativeVk);
    if (candidates.empty()) { ShutdownLocked(); return EGL_NOT_INITIALIZED; }
    if (request == "auto" && !nativeVk) {
        LogLine("[winapk] guest EGL auto: no native vendor Vulkan ICD; preferring hardware d3d11");
    }

    for (size_t i = 0; i < candidates.size(); ++i) {
        const char* backend = candidates[i];
        const bool isVk = std::strcmp(backend, "vulkan") == 0;
        const EGLint dispAttrs[] = {
            EGL_PLATFORM_ANGLE_TYPE_ANGLE, isVk ? EGL_PLATFORM_ANGLE_TYPE_VULKAN_ANGLE : EGL_PLATFORM_ANGLE_TYPE_D3D11_ANGLE,
            EGL_PLATFORM_ANGLE_DEVICE_TYPE_ANGLE, EGL_PLATFORM_ANGLE_DEVICE_TYPE_HARDWARE_ANGLE,
            EGL_NONE
        };
        g_drv.display = getPlat(EGL_PLATFORM_ANGLE_ANGLE, EGL_DEFAULT_DISPLAY, dispAttrs);
        if (g_drv.display != EGL_NO_DISPLAY && g_drv.eglInitialize(g_drv.display, nullptr, nullptr)) {
            InstallAngleBlobCache(g_drv.display, reinterpret_cast<void* (*)(const char*)>(g_drv.eglGetProcAddress), backend);
            if (!g_drv.eglBindAPI(EGL_OPENGL_ES_API)) {
                g_drv.eglTerminate(g_drv.display);
                g_drv.display = EGL_NO_DISPLAY;
                continue;
            }
            // Probe actual ES ceiling (3 vs 2) and renderer identity on a 1x1 Pbuffer.
            EGLConfig probeCfg = nullptr;
            EGLint numCfg = 0;
            constexpr EGLint pbCfgAttrs[] = {
                EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
                EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
                EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8,
                EGL_NONE
            };
            if (!g_drv.eglChooseConfig(g_drv.display, pbCfgAttrs, &probeCfg, 1, &numCfg) || numCfg < 1) {
                g_drv.eglTerminate(g_drv.display);
                g_drv.display = EGL_NO_DISPLAY;
                continue;
            }
            constexpr EGLint pbAttrs[] = {EGL_WIDTH, 1, EGL_HEIGHT, 1, EGL_NONE};
            EGLSurface pb = g_drv.eglCreatePbufferSurface(g_drv.display, probeCfg, pbAttrs);
            int ceiling = 0;
            std::string rendererStr;
            for (int maj : {3, 2}) {
                const EGLint ctxAttrs[] = {EGL_CONTEXT_CLIENT_VERSION, maj, EGL_NONE};
                EGLContext c = g_drv.eglCreateContext(g_drv.display, probeCfg, EGL_NO_CONTEXT, ctxAttrs);
                if (c != EGL_NO_CONTEXT) {
                    if (pb != EGL_NO_SURFACE && g_drv.eglMakeCurrent(g_drv.display, pb, pb, c)) {
                        const char* r = reinterpret_cast<const char*>(g_drv.glGetString(GL_RENDERER));
                        if (r) rendererStr = r;
                        g_drv.eglMakeCurrent(g_drv.display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
                    }
                    g_drv.eglDestroyContext(g_drv.display, c);
                    ceiling = maj;
                    break;
                }
            }
            if (pb != EGL_NO_SURFACE) g_drv.eglDestroySurface(g_drv.display, pb);

            const bool matchBackend = !rendererStr.empty() &&
                rendererStr.find("Microsoft Basic Render Driver") == std::string::npos &&
                (isVk ? rendererStr.find("Vulkan") != std::string::npos
                      : (rendererStr.find("Direct3D11") != std::string::npos || rendererStr.find("D3D11") != std::string::npos));
            if (ceiling >= 2 && matchBackend) {
                g_drv.backend = backend;
                g_drv.maxEsMajor = ceiling;
                g_drv.initRefs = 1;
                wchar_t lossEnv[8]{};
                g_drv.injectLoss = GetEnvironmentVariableW(L"WINAPK_TEST_INJECT_CONTEXT_LOSS", lossEnv, 8) > 0 && lossEnv[0] == L'1';
                LogLine("[winapk] guest EGL request=" + request + " backend=" + backend + " maxEs=" + std::to_string(ceiling));
                LogLine("[winapk] guest GLES renderer backend=" + std::string(backend) + " name=" + rendererStr);
                g_drv.loggedRenderer = true;
                SendEmbedEvent(std::string("backend gles-") + backend);
                return EGL_SUCCESS;
            }
            g_drv.eglTerminate(g_drv.display);
            g_drv.display = EGL_NO_DISPLAY;
        }
        if (request == "auto" && i + 1 < candidates.size()) {
            LogLine(std::string("[winapk] guest EGL auto: ") + backend + " unavailable; trying " + candidates[i + 1]);
        } else {
            LogLine("GRAPHICS_CAPABILITY_MISSING: guest EGL backend=" + std::string(backend) + "; no fallback");
        }
    }
    ShutdownLocked();
    return EGL_NOT_INITIALIZED;
}

EGLConfig InternHostConfigLocked(EGLConfig hostCfg) {
    auto it = g_hostToGuestConfig.find(hostCfg);
    if (it != g_hostToGuestConfig.end()) return it->second;
    EGLint renderable = EGL_OPENGL_ES2_BIT;
    g_drv.eglGetConfigAttrib(g_drv.display, hostCfg, EGL_RENDERABLE_TYPE, &renderable);
    if (g_drv.maxEsMajor < 3) renderable &= ~0x0040 /* EGL_OPENGL_ES3_BIT_KHR */;
    void* token = reinterpret_cast<void*>(g_nextHandle++);
    g_configs[token] = {hostCfg, renderable};
    g_hostToGuestConfig[hostCfg] = token;
    return token;
}

bool CheckDisplayLocked(EGLDisplay d, EGLint& errOut) {
    if (reinterpret_cast<uintptr_t>(d) != kLogicalDisplay) { errOut = EGL_BAD_DISPLAY; return false; }
    if (g_drv.initRefs <= 0 || g_drv.display == EGL_NO_DISPLAY) { errOut = EGL_NOT_INITIALIZED; return false; }
    return true;
}

// Recreates a virtual window surface's backing Pbuffer when its ANativeWindow resized while >0x0.
bool SyncWindowSurfaceSizeLocked(void* guestSurfToken, GuestSurfaceEntry& surf) {
    if (!surf.isWindow) return true;
    HWND hwnd = nullptr;
    int w = 0, h = 0;
    if (!ResolveSurface(surf.nativeToken, hwnd, w, h) || hwnd != surf.hwnd) return false;
    if (w <= 0 || h <= 0) return true; // Minimized: keep existing Pbuffer until Swap refuses or window restores.
    if (surf.hostPbuffer != EGL_NO_SURFACE && surf.width == w && surf.height == h) return true;

    const bool rebindHere = (t_tls.guestDraw == guestSurfToken || t_tls.guestRead == guestSurfToken) &&
                            t_tls.hostCtx != EGL_NO_CONTEXT;
    if (rebindHere) {
        g_drv.eglMakeCurrent(g_drv.display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    }
    const EGLint pbAttrs[] = {EGL_WIDTH, w, EGL_HEIGHT, h, EGL_NONE};
    EGLSurface fresh = g_drv.eglCreatePbufferSurface(g_drv.display, surf.hostConfig, pbAttrs);
    if (fresh == EGL_NO_SURFACE) {
        if (rebindHere && surf.hostPbuffer != EGL_NO_SURFACE) {
            g_drv.eglMakeCurrent(g_drv.display, surf.hostPbuffer, surf.hostPbuffer, t_tls.hostCtx);
        }
        return false;
    }
    if (surf.hostPbuffer != EGL_NO_SURFACE) {
        g_drv.eglDestroySurface(g_drv.display, surf.hostPbuffer);
    }
    surf.hostPbuffer = fresh;
    surf.width = w;
    surf.height = h;
    if (rebindHere) {
        EGLSurface hostDraw = surf.hostPbuffer;
        EGLSurface hostRead = surf.hostPbuffer;
        if (t_tls.guestDraw != guestSurfToken) {
            auto dit = g_surfaces.find(t_tls.guestDraw);
            hostDraw = dit != g_surfaces.end() ? dit->second.hostPbuffer : EGL_NO_SURFACE;
        }
        if (t_tls.guestRead != guestSurfToken) {
            auto rit = g_surfaces.find(t_tls.guestRead);
            hostRead = rit != g_surfaces.end() ? rit->second.hostPbuffer : EGL_NO_SURFACE;
        }
        g_drv.eglMakeCurrent(g_drv.display, hostDraw, hostRead, t_tls.hostCtx);
    }
    return true;
}

// ---- Guest EGL 1.5 + KHR Entry Points ----

EGLint EGLAPIENTRY E_GetError() {
    const EGLint e = t_tls.eglError;
    t_tls.eglError = EGL_SUCCESS;
    return e;
}

EGLDisplay EGLAPIENTRY E_GetDisplay(EGLNativeDisplayType nativeId) {
    if (nativeId != EGL_DEFAULT_DISPLAY) {
        SetEglError(EGL_BAD_PARAMETER);
        return EGL_NO_DISPLAY;
    }
    SetEglError(EGL_SUCCESS);
    return reinterpret_cast<EGLDisplay>(kLogicalDisplay);
}

EGLBoolean EGLAPIENTRY E_Initialize(EGLDisplay d, EGLint* major, EGLint* minor) {
    if (reinterpret_cast<uintptr_t>(d) != kLogicalDisplay) {
        SetEglError(EGL_BAD_DISPLAY);
        return EGL_FALSE;
    }
    std::string req;
    {
        std::lock_guard lock(g_mu);
        if (g_drv.initRefs > 0 && g_drv.display != EGL_NO_DISPLAY) {
            ++g_drv.initRefs;
            if (major) *major = 1;
            if (minor) *minor = 5;
            SetEglError(EGL_SUCCESS);
            return EGL_TRUE;
        }
    }
    req = QueryGuestGraphicsRequest();
    std::lock_guard lock(g_mu);
    if (g_drv.initRefs == 0) {
        const EGLint rc = InitDriverLocked(req);
        if (rc != EGL_SUCCESS) {
            SetEglError(rc);
            return EGL_FALSE;
        }
    } else {
        ++g_drv.initRefs;
    }
    if (major) *major = 1;
    if (minor) *minor = 5;
    SetEglError(EGL_SUCCESS);
    return EGL_TRUE;
}

EGLBoolean EGLAPIENTRY E_Terminate(EGLDisplay d) {
    std::lock_guard lock(g_mu);
    EGLint err = EGL_SUCCESS;
    if (!CheckDisplayLocked(d, err)) { SetEglError(err); return EGL_FALSE; }
    if (--g_drv.initRefs <= 0) {
        ShutdownLocked();
    }
    SetEglError(EGL_SUCCESS);
    return EGL_TRUE;
}

const char* EGLAPIENTRY E_QueryString(EGLDisplay d, EGLint name) {
    if (d == EGL_NO_DISPLAY && name == EGL_EXTENSIONS) {
        SetEglError(EGL_SUCCESS);
        return "EGL_EXT_client_extensions";
    }
    std::lock_guard lock(g_mu);
    EGLint err = EGL_SUCCESS;
    if (!CheckDisplayLocked(d, err)) { SetEglError(err); return nullptr; }
    SetEglError(EGL_SUCCESS);
    switch (name) {
        case EGL_VENDOR: return "Windows-ARM64.apk (ANGLE)";
        case EGL_VERSION: return "1.5 Windows-ARM64.apk";
        case EGL_CLIENT_APIS: return "OpenGL_ES";
        case EGL_EXTENSIONS:
            return "EGL_KHR_create_context EGL_KHR_surfaceless_context "
                   "EGL_KHR_image_base EGL_KHR_gl_texture_2D_image "
                   "EGL_KHR_gl_renderbuffer_image EGL_KHR_fence_sync EGL_KHR_wait_sync";
        default:
            SetEglError(EGL_BAD_PARAMETER);
            return nullptr;
    }
}

EGLBoolean EGLAPIENTRY E_BindAPI(EGLenum api) {
    if (api != EGL_OPENGL_ES_API) { SetEglError(EGL_BAD_PARAMETER); return EGL_FALSE; }
    SetEglError(EGL_SUCCESS);
    return EGL_TRUE;
}

EGLenum EGLAPIENTRY E_QueryAPI() {
    SetEglError(EGL_SUCCESS);
    return EGL_OPENGL_ES_API;
}

EGLBoolean EGLAPIENTRY E_WaitClient() {
    if (t_tls.hostCtx != EGL_NO_CONTEXT && g_drv.glFinish) g_drv.glFinish();
    SetEglError(EGL_SUCCESS);
    return EGL_TRUE;
}

EGLBoolean EGLAPIENTRY E_WaitGL() { return E_WaitClient(); }
EGLBoolean EGLAPIENTRY E_WaitNative(EGLint) { SetEglError(EGL_SUCCESS); return EGL_TRUE; }

EGLBoolean EGLAPIENTRY E_ReleaseThread() {
    std::lock_guard lock(g_mu);
    if (t_tls.hostCtx != EGL_NO_CONTEXT && g_drv.display != EGL_NO_DISPLAY && g_drv.eglMakeCurrent) {
        g_drv.eglMakeCurrent(g_drv.display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        auto cit = g_contexts.find(t_tls.guestCtx);
        if (cit != g_contexts.end() && cit->second.boundThread == t_tls.id) cit->second.boundThread = 0;
        for (void* st : {t_tls.guestDraw, t_tls.guestRead}) {
            auto sit = g_surfaces.find(st);
            if (sit != g_surfaces.end() && sit->second.boundThread == t_tls.id) sit->second.boundThread = 0;
        }
        t_tls.hostCtx = EGL_NO_CONTEXT;
        t_tls.guestCtx = EGL_NO_CONTEXT;
        t_tls.guestDraw = t_tls.guestRead = EGL_NO_SURFACE;
    }
    if (g_drv.eglReleaseThread) g_drv.eglReleaseThread();
    t_tls.eglError = EGL_SUCCESS;
    return EGL_TRUE;
}

EGLBoolean EGLAPIENTRY E_GetConfigs(EGLDisplay d, EGLConfig* configs, EGLint configSize, EGLint* numConfig) {
    std::lock_guard lock(g_mu);
    EGLint err = EGL_SUCCESS;
    if (!CheckDisplayLocked(d, err)) { SetEglError(err); return EGL_FALSE; }
    if (!numConfig || configSize < 0) { SetEglError(EGL_BAD_PARAMETER); return EGL_FALSE; }
    constexpr EGLint pbFilter[] = {EGL_SURFACE_TYPE, EGL_PBUFFER_BIT, EGL_NONE};
    EGLint total = 0;
    if (!g_drv.eglChooseConfig(g_drv.display, pbFilter, nullptr, 0, &total)) {
        SetEglError(g_drv.eglGetError());
        return EGL_FALSE;
    }
    if (!configs) {
        *numConfig = total;
        SetEglError(EGL_SUCCESS);
        return EGL_TRUE;
    }
    const EGLint cap = std::min(total, configSize);
    std::vector<EGLConfig> hostCfgs(cap > 0 ? static_cast<size_t>(cap) : 1);
    EGLint got = 0;
    if (!g_drv.eglChooseConfig(g_drv.display, pbFilter, hostCfgs.data(), cap, &got)) {
        SetEglError(g_drv.eglGetError());
        return EGL_FALSE;
    }
    for (EGLint i = 0; i < got; ++i) configs[i] = InternHostConfigLocked(hostCfgs[static_cast<size_t>(i)]);
    *numConfig = got;
    SetEglError(EGL_SUCCESS);
    return EGL_TRUE;
}

EGLBoolean EGLAPIENTRY E_ChooseConfig(EGLDisplay d, const EGLint* attribList,
                                      EGLConfig* configs, EGLint configSize, EGLint* numConfig) {
    std::lock_guard lock(g_mu);
    EGLint err = EGL_SUCCESS;
    if (!CheckDisplayLocked(d, err)) { SetEglError(err); return EGL_FALSE; }
    if (!numConfig || configSize < 0) { SetEglError(EGL_BAD_PARAMETER); return EGL_FALSE; }

    std::vector<EGLint> translated;
    bool hasSurfaceType = false;
    if (attribList) {
        bool terminated = false;
        for (int i = 0; i < 128; ++i) {
            const EGLint k = attribList[2 * i];
            if (k == EGL_NONE) { terminated = true; break; }
            EGLint v = attribList[2 * i + 1];
            if (k == 0x3142 /* EGL_RECORDABLE_ANDROID */) continue;
            if (k == EGL_SURFACE_TYPE) {
                hasSurfaceType = true;
                if (v != EGL_DONT_CARE) {
                    // Unsupported pixmap/multisample-resolve bits return 0 matches cleanly.
                    if (v & ~(EGL_WINDOW_BIT | EGL_PBUFFER_BIT)) {
                        *numConfig = 0;
                        SetEglError(EGL_SUCCESS);
                        return EGL_TRUE;
                    }
                    v = EGL_PBUFFER_BIT; // Virtual window surfaces and pbuffers both use ANGLE pbuffers.
                }
            } else if (k == EGL_RENDERABLE_TYPE && v != EGL_DONT_CARE) {
                if ((v & 0x0040 /* EGL_OPENGL_ES3_BIT_KHR */) && g_drv.maxEsMajor < 3) {
                    *numConfig = 0;
                    SetEglError(EGL_SUCCESS);
                    return EGL_TRUE;
                }
            }
            translated.push_back(k);
            translated.push_back(v);
        }
        if (!terminated) { SetEglError(EGL_BAD_ATTRIBUTE); return EGL_FALSE; }
    }
    if (!hasSurfaceType) {
        translated.push_back(EGL_SURFACE_TYPE);
        translated.push_back(EGL_PBUFFER_BIT);
    }
    translated.push_back(EGL_NONE);

    EGLint total = 0;
    if (!g_drv.eglChooseConfig(g_drv.display, translated.data(), nullptr, 0, &total)) {
        const EGLint de = g_drv.eglGetError();
        SetEglError(de == EGL_SUCCESS ? EGL_BAD_ATTRIBUTE : de);
        return EGL_FALSE;
    }
    if (!configs || configSize == 0 || total == 0) {
        *numConfig = configs ? 0 : total;
        if (!configs) *numConfig = total;
        else if (configSize > 0) *numConfig = 0;
        SetEglError(EGL_SUCCESS);
        return EGL_TRUE;
    }
    const EGLint cap = std::min(total, configSize);
    std::vector<EGLConfig> hostCfgs(static_cast<size_t>(cap));
    EGLint got = 0;
    if (!g_drv.eglChooseConfig(g_drv.display, translated.data(), hostCfgs.data(), cap, &got)) {
        const EGLint de = g_drv.eglGetError();
        SetEglError(de == EGL_SUCCESS ? EGL_BAD_ATTRIBUTE : de);
        return EGL_FALSE;
    }
    for (EGLint i = 0; i < got; ++i) configs[i] = InternHostConfigLocked(hostCfgs[static_cast<size_t>(i)]);
    *numConfig = got;
    SetEglError(EGL_SUCCESS);
    return EGL_TRUE;
}

EGLBoolean EGLAPIENTRY E_GetConfigAttrib(EGLDisplay d, EGLConfig cfg, EGLint attr, EGLint* value) {
    std::lock_guard lock(g_mu);
    EGLint err = EGL_SUCCESS;
    if (!CheckDisplayLocked(d, err)) { SetEglError(err); return EGL_FALSE; }
    if (!value) { SetEglError(EGL_BAD_PARAMETER); return EGL_FALSE; }
    auto it = g_configs.find(cfg);
    if (it == g_configs.end()) { SetEglError(EGL_BAD_CONFIG); return EGL_FALSE; }
    if (attr == 0x3142 /* EGL_RECORDABLE_ANDROID */) {
        *value = 1;
        SetEglError(EGL_SUCCESS);
        return EGL_TRUE;
    }
    if (!g_drv.eglGetConfigAttrib(g_drv.display, it->second.hostConfig, attr, value)) {
        SetEglError(g_drv.eglGetError());
        return EGL_FALSE;
    }
    if (attr == EGL_SURFACE_TYPE) *value |= (EGL_WINDOW_BIT | EGL_PBUFFER_BIT);
    else if (attr == EGL_RENDERABLE_TYPE) *value = it->second.renderableMask;
    SetEglError(EGL_SUCCESS);
    return EGL_TRUE;
}

EGLContext EGLAPIENTRY E_CreateContext(EGLDisplay d, EGLConfig cfg, EGLContext shareCtx, const EGLint* attribList) {
    std::lock_guard lock(g_mu);
    EGLint err = EGL_SUCCESS;
    if (!CheckDisplayLocked(d, err)) { SetEglError(err); return EGL_NO_CONTEXT; }
    auto cfgIt = g_configs.find(cfg);
    if (cfgIt == g_configs.end()) { SetEglError(EGL_BAD_CONFIG); return EGL_NO_CONTEXT; }

    EGLContext hostShare = EGL_NO_CONTEXT;
    if (shareCtx != EGL_NO_CONTEXT) {
        auto shIt = g_contexts.find(shareCtx);
        if (shIt == g_contexts.end()) { SetEglError(EGL_BAD_CONTEXT); return EGL_NO_CONTEXT; }
        hostShare = shIt->second.hostContext;
    }

    int reqMajor = 1;
    int reqMinor = 0;
    std::vector<EGLint> hostAttrs;
    if (attribList) {
        bool terminated = false;
        for (int i = 0; i < 64; ++i) {
            const EGLint k = attribList[2 * i];
            if (k == EGL_NONE) { terminated = true; break; }
            const EGLint v = attribList[2 * i + 1];
            if (k == EGL_CONTEXT_CLIENT_VERSION) reqMajor = v;
            else if (k == EGL_CONTEXT_MINOR_VERSION_KHR) reqMinor = v;
            hostAttrs.push_back(k);
            hostAttrs.push_back(v);
        }
        if (!terminated) { SetEglError(EGL_BAD_ATTRIBUTE); return EGL_NO_CONTEXT; }
    }
    hostAttrs.push_back(EGL_NONE);

    if (reqMajor < 1 || reqMajor > g_drv.maxEsMajor || (reqMajor == 3 && reqMinor > 1)) {
        SetEglError(EGL_BAD_MATCH);
        return EGL_NO_CONTEXT;
    }

    EGLContext hostCtx = g_drv.eglCreateContext(g_drv.display, cfgIt->second.hostConfig, hostShare, hostAttrs.data());
    if (hostCtx == EGL_NO_CONTEXT) {
        const EGLint de = g_drv.eglGetError();
        SetEglError(de == EGL_SUCCESS ? EGL_BAD_MATCH : de);
        return EGL_NO_CONTEXT;
    }
    g_drv.lost = false;
    void* token = reinterpret_cast<void*>(g_nextHandle++);
    g_contexts[token] = {hostCtx, cfg, reqMajor, 0};
    SetEglError(EGL_SUCCESS);
    return token;
}

EGLBoolean EGLAPIENTRY E_DestroyContext(EGLDisplay d, EGLContext ctx) {
    std::lock_guard lock(g_mu);
    EGLint err = EGL_SUCCESS;
    if (!CheckDisplayLocked(d, err)) { SetEglError(err); return EGL_FALSE; }
    auto it = g_contexts.find(ctx);
    if (it == g_contexts.end()) { SetEglError(EGL_BAD_CONTEXT); return EGL_FALSE; }
    if (it->second.boundThread != 0 && it->second.boundThread != t_tls.id) {
        SetEglError(EGL_BAD_ACCESS);
        return EGL_FALSE;
    }
    if (t_tls.guestCtx == ctx) {
        g_drv.eglMakeCurrent(g_drv.display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        t_tls.hostCtx = EGL_NO_CONTEXT;
        t_tls.guestCtx = EGL_NO_CONTEXT;
        t_tls.guestDraw = t_tls.guestRead = EGL_NO_SURFACE;
    }
    g_drv.eglDestroyContext(g_drv.display, it->second.hostContext);
    g_contexts.erase(it);
    SetEglError(EGL_SUCCESS);
    return EGL_TRUE;
}

EGLSurface EGLAPIENTRY E_CreateWindowSurface(EGLDisplay d, EGLConfig cfg,
                                             EGLNativeWindowType nativeWin, const EGLint* attribList) {
    std::lock_guard lock(g_mu);
    EGLint err = EGL_SUCCESS;
    if (!CheckDisplayLocked(d, err)) { SetEglError(err); return EGL_NO_SURFACE; }
    auto cfgIt = g_configs.find(cfg);
    if (cfgIt == g_configs.end()) { SetEglError(EGL_BAD_CONFIG); return EGL_NO_SURFACE; }
    if (attribList) {
        for (int i = 0; attribList[i] != EGL_NONE; i += 2) {
            if (attribList[i] != EGL_RENDER_BUFFER && attribList[i] != 0x309D /* EGL_GL_COLORSPACE */) {
                SetEglError(EGL_BAD_ATTRIBUTE);
                return EGL_NO_SURFACE;
            }
        }
    }

    void* token = reinterpret_cast<void*>(nativeWin);
    HWND hwnd = nullptr;
    int w = 0, h = 0;
    if (!ResolveSurface(token, hwnd, w, h)) {
        SetEglError(EGL_BAD_NATIVE_WINDOW);
        return EGL_NO_SURFACE;
    }
    // Reject creating a duplicate live EGLWindowSurface on the exact same ANativeWindow.
    for (const auto& [sid, existing] : g_surfaces) {
        if (existing.isWindow && existing.nativeToken == token) {
            SetEglError(EGL_BAD_ALLOC);
            return EGL_NO_SURFACE;
        }
    }
    if (!RetainSurface(token)) {
        SetEglError(EGL_BAD_NATIVE_WINDOW);
        return EGL_NO_SURFACE;
    }

    const int initW = std::max(1, w);
    const int initH = std::max(1, h);
    const EGLint pbAttrs[] = {EGL_WIDTH, initW, EGL_HEIGHT, initH, EGL_NONE};
    EGLSurface pb = g_drv.eglCreatePbufferSurface(g_drv.display, cfgIt->second.hostConfig, pbAttrs);
    if (pb == EGL_NO_SURFACE) {
        ReleaseSurface(token);
        const EGLint de = g_drv.eglGetError();
        SetEglError(de == EGL_SUCCESS ? EGL_BAD_ALLOC : de);
        return EGL_NO_SURFACE;
    }

    void* surfId = reinterpret_cast<void*>(g_nextHandle++);
    g_surfaces[surfId] = {true, cfgIt->second.hostConfig, pb, token, hwnd, initW, initH, 0, false};
    SetEglError(EGL_SUCCESS);
    return surfId;
}

EGLSurface EGLAPIENTRY E_CreatePbufferSurface(EGLDisplay d, EGLConfig cfg, const EGLint* attribList) {
    std::lock_guard lock(g_mu);
    EGLint err = EGL_SUCCESS;
    if (!CheckDisplayLocked(d, err)) { SetEglError(err); return EGL_NO_SURFACE; }
    auto cfgIt = g_configs.find(cfg);
    if (cfgIt == g_configs.end()) { SetEglError(EGL_BAD_CONFIG); return EGL_NO_SURFACE; }

    EGLint w = 1, h = 1;
    std::vector<EGLint> pbAttrs;
    if (attribList) {
        for (int i = 0; attribList[i] != EGL_NONE; i += 2) {
            if (attribList[i] == EGL_WIDTH) w = attribList[i + 1];
            else if (attribList[i] == EGL_HEIGHT) h = attribList[i + 1];
            pbAttrs.push_back(attribList[i]);
            pbAttrs.push_back(attribList[i + 1]);
        }
    }
    pbAttrs.push_back(EGL_NONE);
    if (w < 0 || h < 0) { SetEglError(EGL_BAD_PARAMETER); return EGL_NO_SURFACE; }

    EGLSurface pb = g_drv.eglCreatePbufferSurface(g_drv.display, cfgIt->second.hostConfig, pbAttrs.data());
    if (pb == EGL_NO_SURFACE) {
        const EGLint de = g_drv.eglGetError();
        SetEglError(de == EGL_SUCCESS ? EGL_BAD_ALLOC : de);
        return EGL_NO_SURFACE;
    }
    void* surfId = reinterpret_cast<void*>(g_nextHandle++);
    g_surfaces[surfId] = {false, cfgIt->second.hostConfig, pb, nullptr, nullptr, w, h, 0, false};
    SetEglError(EGL_SUCCESS);
    return surfId;
}

EGLBoolean EGLAPIENTRY E_DestroySurface(EGLDisplay d, EGLSurface surf) {
    HWND dropHwnd = nullptr;
    int dropKey = 0;
    {
        std::lock_guard lock(g_mu);
        EGLint err = EGL_SUCCESS;
        if (!CheckDisplayLocked(d, err)) { SetEglError(err); return EGL_FALSE; }
        auto it = g_surfaces.find(surf);
        if (it == g_surfaces.end()) { SetEglError(EGL_BAD_SURFACE); return EGL_FALSE; }
        if (it->second.boundThread != 0) {
            SetEglError(EGL_BAD_ACCESS);
            return EGL_FALSE;
        }
        if (it->second.hostPbuffer != EGL_NO_SURFACE) {
            g_drv.eglDestroySurface(g_drv.display, it->second.hostPbuffer);
        }
        if (it->second.nativeToken) {
            dropHwnd = it->second.hwnd;
            dropKey = -static_cast<int>(reinterpret_cast<uintptr_t>(it->second.nativeToken));
            ReleaseSurface(it->second.nativeToken);
        }
        g_surfaces.erase(it);
    }
    if (dropHwnd && dropKey != 0) {
        CompositorUpdateSurfacePlacement(dropHwnd, dropKey, {}, true);
        InvalidateRect(dropHwnd, nullptr, FALSE);
    }
    SetEglError(EGL_SUCCESS);
    return EGL_TRUE;
}

EGLBoolean EGLAPIENTRY E_MakeCurrent(EGLDisplay d, EGLSurface draw, EGLSurface read, EGLContext ctx) {
    std::lock_guard lock(g_mu);
    EGLint err = EGL_SUCCESS;
    if (!CheckDisplayLocked(d, err)) { SetEglError(err); return EGL_FALSE; }

    if (ctx == EGL_NO_CONTEXT) {
        if (draw != EGL_NO_SURFACE || read != EGL_NO_SURFACE) {
            SetEglError(EGL_BAD_MATCH);
            return EGL_FALSE;
        }
        if (t_tls.hostCtx != EGL_NO_CONTEXT) {
            if (!g_drv.eglMakeCurrent(g_drv.display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT)) {
                SetEglError(g_drv.eglGetError());
                return EGL_FALSE;
            }
            auto oldCtx = g_contexts.find(t_tls.guestCtx);
            if (oldCtx != g_contexts.end() && oldCtx->second.boundThread == t_tls.id) oldCtx->second.boundThread = 0;
            for (void* st : {t_tls.guestDraw, t_tls.guestRead}) {
                auto oldSurf = g_surfaces.find(st);
                if (oldSurf != g_surfaces.end() && oldSurf->second.boundThread == t_tls.id) oldSurf->second.boundThread = 0;
            }
            t_tls.hostCtx = EGL_NO_CONTEXT;
            t_tls.guestCtx = EGL_NO_CONTEXT;
            t_tls.guestDraw = t_tls.guestRead = EGL_NO_SURFACE;
        }
        SetEglError(EGL_SUCCESS);
        return EGL_TRUE;
    }

    if (g_drv.lost) { SetEglError(EGL_CONTEXT_LOST); return EGL_FALSE; }
    auto ctxIt = g_contexts.find(ctx);
    if (ctxIt == g_contexts.end()) { SetEglError(EGL_BAD_CONTEXT); return EGL_NO_CONTEXT != nullptr ? EGL_FALSE : EGL_FALSE; }
    if (ctxIt->second.boundThread != 0 && ctxIt->second.boundThread != t_tls.id) {
        SetEglError(EGL_BAD_ACCESS);
        return EGL_FALSE;
    }

    // Surfaceless context (`EGL_KHR_surfaceless_context`): both draw and read are EGL_NO_SURFACE.
    EGLSurface hostDraw = EGL_NO_SURFACE;
    EGLSurface hostRead = EGL_NO_SURFACE;
    GuestSurfaceEntry* drawEntry = nullptr;
    GuestSurfaceEntry* readEntry = nullptr;
    if (draw != EGL_NO_SURFACE || read != EGL_NO_SURFACE) {
        if (draw == EGL_NO_SURFACE || read == EGL_NO_SURFACE) {
            SetEglError(EGL_BAD_MATCH);
            return EGL_FALSE;
        }
        auto dIt = g_surfaces.find(draw);
        auto rIt = g_surfaces.find(read);
        if (dIt == g_surfaces.end() || rIt == g_surfaces.end()) {
            SetEglError(EGL_BAD_SURFACE);
            return EGL_FALSE;
        }
        drawEntry = &dIt->second;
        readEntry = &rIt->second;
        if ((drawEntry->boundThread != 0 && drawEntry->boundThread != t_tls.id) ||
            (readEntry->boundThread != 0 && readEntry->boundThread != t_tls.id)) {
            SetEglError(EGL_BAD_ACCESS);
            return EGL_FALSE;
        }
        if (!SyncWindowSurfaceSizeLocked(draw, *drawEntry) ||
            (read != draw && !SyncWindowSurfaceSizeLocked(read, *readEntry))) {
            SetEglError(EGL_BAD_SURFACE);
            return EGL_FALSE;
        }
        hostDraw = drawEntry->hostPbuffer;
        hostRead = readEntry->hostPbuffer;
    }

    if (!g_drv.eglMakeCurrent(g_drv.display, hostDraw, hostRead, ctxIt->second.hostContext)) {
        const EGLint de = g_drv.eglGetError();
        if (de == EGL_CONTEXT_LOST) g_drv.lost = true;
        SetEglError(de == EGL_SUCCESS ? EGL_BAD_ACCESS : de);
        return EGL_FALSE;
    }

    // Clear previous thread bindings before recording the new ones.
    if (t_tls.guestCtx != EGL_NO_CONTEXT && t_tls.guestCtx != ctx) {
        auto prevC = g_contexts.find(t_tls.guestCtx);
        if (prevC != g_contexts.end() && prevC->second.boundThread == t_tls.id) prevC->second.boundThread = 0;
    }
    for (void* prevS : {t_tls.guestDraw, t_tls.guestRead}) {
        if (prevS != EGL_NO_SURFACE && prevS != draw && prevS != read) {
            auto ps = g_surfaces.find(prevS);
            if (ps != g_surfaces.end() && ps->second.boundThread == t_tls.id) ps->second.boundThread = 0;
        }
    }

    ctxIt->second.boundThread = t_tls.id;
    if (drawEntry) drawEntry->boundThread = t_tls.id;
    if (readEntry) readEntry->boundThread = t_tls.id;
    t_tls.guestCtx = ctx;
    t_tls.hostCtx = ctxIt->second.hostContext;
    t_tls.guestDraw = draw;
    t_tls.guestRead = read;
    SetEglError(EGL_SUCCESS);
    return EGL_TRUE;
}

EGLContext EGLAPIENTRY E_GetCurrentContext() {
    SetEglError(EGL_SUCCESS);
    return t_tls.guestCtx;
}

EGLSurface EGLAPIENTRY E_GetCurrentSurface(EGLint readdraw) {
    if (readdraw != EGL_DRAW && readdraw != EGL_READ) {
        SetEglError(EGL_BAD_PARAMETER);
        return EGL_NO_SURFACE;
    }
    SetEglError(EGL_SUCCESS);
    return readdraw == EGL_DRAW ? t_tls.guestDraw : t_tls.guestRead;
}

EGLDisplay EGLAPIENTRY E_GetCurrentDisplay() {
    SetEglError(EGL_SUCCESS);
    return t_tls.guestCtx != EGL_NO_CONTEXT ? reinterpret_cast<EGLDisplay>(kLogicalDisplay) : EGL_NO_DISPLAY;
}

EGLBoolean EGLAPIENTRY E_QueryContext(EGLDisplay d, EGLContext ctx, EGLint attr, EGLint* value) {
    std::lock_guard lock(g_mu);
    EGLint err = EGL_SUCCESS;
    if (!CheckDisplayLocked(d, err)) { SetEglError(err); return EGL_FALSE; }
    if (!value) { SetEglError(EGL_BAD_PARAMETER); return EGL_FALSE; }
    auto it = g_contexts.find(ctx);
    if (it == g_contexts.end()) { SetEglError(EGL_BAD_CONTEXT); return EGL_FALSE; }
    if (attr == EGL_CONTEXT_CLIENT_VERSION) {
        *value = it->second.clientMajor;
        SetEglError(EGL_SUCCESS);
        return EGL_TRUE;
    }
    if (!g_drv.eglQueryContext(g_drv.display, it->second.hostContext, attr, value)) {
        SetEglError(g_drv.eglGetError());
        return EGL_FALSE;
    }
    SetEglError(EGL_SUCCESS);
    return EGL_TRUE;
}

EGLBoolean EGLAPIENTRY E_QuerySurface(EGLDisplay d, EGLSurface surf, EGLint attr, EGLint* value) {
    std::lock_guard lock(g_mu);
    EGLint err = EGL_SUCCESS;
    if (!CheckDisplayLocked(d, err)) { SetEglError(err); return EGL_FALSE; }
    if (!value) { SetEglError(EGL_BAD_PARAMETER); return EGL_FALSE; }
    auto it = g_surfaces.find(surf);
    if (it == g_surfaces.end()) { SetEglError(EGL_BAD_SURFACE); return EGL_FALSE; }

    if (it->second.isWindow) {
        HWND hwnd = nullptr;
        int w = 0, h = 0;
        if (!ResolveSurface(it->second.nativeToken, hwnd, w, h) || hwnd != it->second.hwnd) {
            SetEglError(EGL_BAD_SURFACE);
            return EGL_FALSE;
        }
        if (w > 0 && h > 0 && it->second.boundThread == t_tls.id) {
            SyncWindowSurfaceSizeLocked(surf, it->second);
        }
        if (attr == EGL_WIDTH) { *value = w; SetEglError(EGL_SUCCESS); return EGL_TRUE; }
        if (attr == EGL_HEIGHT) { *value = h; SetEglError(EGL_SUCCESS); return EGL_TRUE; }
        if (attr == EGL_RENDER_BUFFER) { *value = EGL_BACK_BUFFER; SetEglError(EGL_SUCCESS); return EGL_TRUE; }
    }
    if (!g_drv.eglQuerySurface(g_drv.display, it->second.hostPbuffer, attr, value)) {
        SetEglError(g_drv.eglGetError());
        return EGL_FALSE;
    }
    SetEglError(EGL_SUCCESS);
    return EGL_TRUE;
}

EGLBoolean EGLAPIENTRY E_SurfaceAttrib(EGLDisplay d, EGLSurface surf, EGLint attr, EGLint val) {
    std::lock_guard lock(g_mu);
    EGLint err = EGL_SUCCESS;
    if (!CheckDisplayLocked(d, err)) { SetEglError(err); return EGL_FALSE; }
    auto it = g_surfaces.find(surf);
    if (it == g_surfaces.end()) { SetEglError(EGL_BAD_SURFACE); return EGL_FALSE; }
    if (!g_drv.eglSurfaceAttrib(g_drv.display, it->second.hostPbuffer, attr, val)) {
        SetEglError(g_drv.eglGetError());
        return EGL_FALSE;
    }
    SetEglError(EGL_SUCCESS);
    return EGL_TRUE;
}

EGLBoolean EGLAPIENTRY E_BindTexImage(EGLDisplay d, EGLSurface surf, EGLint buffer) {
    std::lock_guard lock(g_mu);
    EGLint err = EGL_SUCCESS;
    if (!CheckDisplayLocked(d, err)) { SetEglError(err); return EGL_FALSE; }
    auto it = g_surfaces.find(surf);
    if (it == g_surfaces.end()) { SetEglError(EGL_BAD_SURFACE); return EGL_FALSE; }
    if (!g_drv.eglBindTexImage(g_drv.display, it->second.hostPbuffer, buffer)) {
        SetEglError(g_drv.eglGetError());
        return EGL_FALSE;
    }
    SetEglError(EGL_SUCCESS);
    return EGL_TRUE;
}

EGLBoolean EGLAPIENTRY E_ReleaseTexImage(EGLDisplay d, EGLSurface surf, EGLint buffer) {
    std::lock_guard lock(g_mu);
    EGLint err = EGL_SUCCESS;
    if (!CheckDisplayLocked(d, err)) { SetEglError(err); return EGL_FALSE; }
    auto it = g_surfaces.find(surf);
    if (it == g_surfaces.end()) { SetEglError(EGL_BAD_SURFACE); return EGL_FALSE; }
    if (!g_drv.eglReleaseTexImage(g_drv.display, it->second.hostPbuffer, buffer)) {
        SetEglError(g_drv.eglGetError());
        return EGL_FALSE;
    }
    SetEglError(EGL_SUCCESS);
    return EGL_TRUE;
}

EGLBoolean EGLAPIENTRY E_SwapInterval(EGLDisplay d, EGLint) {
    std::lock_guard lock(g_mu);
    EGLint err = EGL_SUCCESS;
    if (!CheckDisplayLocked(d, err)) { SetEglError(err); return EGL_FALSE; }
    SetEglError(EGL_SUCCESS);
    return EGL_TRUE;
}

EGLBoolean EGLAPIENTRY E_SwapBuffers(EGLDisplay d, EGLSurface surf) {
    HWND hwnd = nullptr;
    int layerKey = 0;
    int drawW = 0, drawH = 0;
    int liveW = 0, liveH = 0;
    EGLSurface hostPbuf = EGL_NO_SURFACE;
    SurfaceLayerPlacement placement{};
    bool isWindow = false;
    bool isD3d11 = false;

    {
        std::lock_guard lock(g_mu);
        EGLint err = EGL_SUCCESS;
        if (!CheckDisplayLocked(d, err)) { SetEglError(err); return EGL_FALSE; }
        auto it = g_surfaces.find(surf);
        if (it == g_surfaces.end()) { SetEglError(EGL_BAD_SURFACE); return EGL_FALSE; }
        if (t_tls.guestDraw != surf || t_tls.hostCtx == EGL_NO_CONTEXT) {
            SetEglError(EGL_BAD_SURFACE);
            return EGL_FALSE;
        }
        if (g_drv.injectLoss && !g_drv.lossInjected) {
            g_drv.lossInjected = true;
            g_drv.lost = true;
            g_drv.eglMakeCurrent(g_drv.display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
            t_tls.hostCtx = EGL_NO_CONTEXT;
            LogLine("GRAPHICS_RENDER_FAILED: guest eglSwapBuffers error=12302 (injected context loss)");
            SetEglError(EGL_CONTEXT_LOST);
            return EGL_FALSE;
        }
        if (g_drv.lost) { SetEglError(EGL_CONTEXT_LOST); return EGL_FALSE; }

        isWindow = it->second.isWindow;
        if (!isWindow) {
            if (g_drv.glFlush) g_drv.glFlush();
            SetEglError(EGL_SUCCESS);
            return EGL_TRUE;
        }
        if (!ResolveSurface(it->second.nativeToken, hwnd, liveW, liveH, &placement) ||
            hwnd != it->second.hwnd || liveW <= 0 || liveH <= 0) {
            SetEglError(EGL_BAD_SURFACE);
            return EGL_FALSE;
        }
        hostPbuf = it->second.hostPbuffer;
        drawW = it->second.width;
        drawH = it->second.height;
        layerKey = -static_cast<int>(reinterpret_cast<uintptr_t>(it->second.nativeToken));
        isD3d11 = g_drv.backend && std::strcmp(g_drv.backend, "d3d11") == 0;
        it->second.presented = true;
    }

    bool submitted = false;
    if (isD3d11 && g_drv.eglQuerySurfacePointerANGLE) {
        void* shareHandle = nullptr;
        if (g_drv.eglQuerySurfacePointerANGLE(g_drv.display, hostPbuf, EGL_D3D_TEXTURE_2D_SHARE_HANDLE_ANGLE, &shareHandle) && shareHandle) {
            g_drv.glFinish();
            submitted = CompositorSubmitSurfaceSharedTexture(hwnd, layerKey, static_cast<HANDLE>(shareHandle),
                                                             drawW, drawH, LayerAlpha::Opaque, false, placement);
        }
    }
    if (!submitted && drawW > 0 && drawH > 0) {
        GLint prevFbo = 0;
        g_drv.glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prevFbo);
        if (prevFbo != 0) g_drv.glBindFramebuffer(GL_FRAMEBUFFER, 0);
        std::vector<uint8_t> pixels(static_cast<size_t>(drawW) * static_cast<size_t>(drawH) * 4);
        g_drv.glReadPixels(0, 0, drawW, drawH, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
        if (prevFbo != 0) g_drv.glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(prevFbo));
        if (!CompositorSubmitSurfaceBytes(hwnd, layerKey, pixels.data(), static_cast<size_t>(drawW) * 4,
                                          drawW, drawH, LayerPixels::Rgba8, LayerAlpha::Opaque, true, placement)) {
            PublishEmbedGlRgba(pixels.data(), drawW, drawH);
        }
    }
    InvalidateRect(hwnd, nullptr, FALSE);

    // If the client area resized since this frame began, renew the Pbuffer for the next frame.
    if (liveW != drawW || liveH != drawH) {
        std::lock_guard lock(g_mu);
        auto it = g_surfaces.find(surf);
        if (it != g_surfaces.end()) SyncWindowSurfaceSizeLocked(surf, it->second);
    }

    SetEglError(EGL_SUCCESS);
    return EGL_TRUE;
}

// ---- EGLImageKHR & EGLSyncKHR ----

EGLImageKHR EGLAPIENTRY E_CreateImageKHR(EGLDisplay d, EGLContext ctx, EGLenum target,
                                         EGLClientBuffer buffer, const EGLint* attribList) {
    std::lock_guard lock(g_mu);
    EGLint err = EGL_SUCCESS;
    if (!CheckDisplayLocked(d, err)) { SetEglError(err); return EGL_NO_IMAGE_KHR; }
    if (!g_drv.eglCreateImageKHR) { SetEglError(EGL_BAD_PARAMETER); return EGL_NO_IMAGE_KHR; }
    EGLContext hostCtx = EGL_NO_CONTEXT;
    if (ctx != EGL_NO_CONTEXT) {
        auto it = g_contexts.find(ctx);
        if (it == g_contexts.end()) { SetEglError(EGL_BAD_CONTEXT); return EGL_NO_IMAGE_KHR; }
        hostCtx = it->second.hostContext;
    }
    EGLImageKHR img = g_drv.eglCreateImageKHR(g_drv.display, hostCtx, target, buffer, attribList);
    if (img == EGL_NO_IMAGE_KHR) {
        SetEglError(g_drv.eglGetError());
        return EGL_NO_IMAGE_KHR;
    }
    void* token = reinterpret_cast<void*>(g_nextHandle++);
    g_images[token] = img;
    SetEglError(EGL_SUCCESS);
    return token;
}

EGLImage EGLAPIENTRY E_CreateImage15(EGLDisplay d, EGLContext ctx, EGLenum target,
                                     EGLClientBuffer buffer, const EGLAttrib* attribList) {
    std::vector<EGLint> attrs32;
    if (attribList) {
        for (size_t i = 0; attribList[i] != EGL_NONE; i += 2) {
            attrs32.push_back(static_cast<EGLint>(attribList[i]));
            attrs32.push_back(static_cast<EGLint>(attribList[i + 1]));
        }
        attrs32.push_back(EGL_NONE);
    }
    return E_CreateImageKHR(d, ctx, target, buffer, attribList ? attrs32.data() : nullptr);
}

EGLBoolean EGLAPIENTRY E_DestroyImageKHR(EGLDisplay d, EGLImageKHR image) {
    std::lock_guard lock(g_mu);
    EGLint err = EGL_SUCCESS;
    if (!CheckDisplayLocked(d, err)) { SetEglError(err); return EGL_FALSE; }
    auto it = g_images.find(image);
    if (it == g_images.end()) { SetEglError(EGL_BAD_PARAMETER); return EGL_FALSE; }
    const EGLBoolean ok = g_drv.eglDestroyImageKHR ? g_drv.eglDestroyImageKHR(g_drv.display, it->second) : EGL_TRUE;
    g_images.erase(it);
    SetEglError(ok ? EGL_SUCCESS : g_drv.eglGetError());
    return ok;
}

EGLSyncKHR EGLAPIENTRY E_CreateSyncKHR(EGLDisplay d, EGLenum type, const EGLint* attribList) {
    std::lock_guard lock(g_mu);
    EGLint err = EGL_SUCCESS;
    if (!CheckDisplayLocked(d, err)) { SetEglError(err); return EGL_NO_SYNC_KHR; }
    if (!g_drv.eglCreateSyncKHR) { SetEglError(EGL_BAD_PARAMETER); return EGL_NO_SYNC_KHR; }
    EGLSyncKHR syn = g_drv.eglCreateSyncKHR(g_drv.display, type, attribList);
    if (syn == EGL_NO_SYNC_KHR) { SetEglError(g_drv.eglGetError()); return EGL_NO_SYNC_KHR; }
    void* token = reinterpret_cast<void*>(g_nextHandle++);
    g_syncs[token] = syn;
    SetEglError(EGL_SUCCESS);
    return token;
}

EGLSync EGLAPIENTRY E_CreateSync15(EGLDisplay d, EGLenum type, const EGLAttrib* attribList) {
    std::vector<EGLint> attrs32;
    if (attribList) {
        for (size_t i = 0; attribList[i] != EGL_NONE; i += 2) {
            attrs32.push_back(static_cast<EGLint>(attribList[i]));
            attrs32.push_back(static_cast<EGLint>(attribList[i + 1]));
        }
        attrs32.push_back(EGL_NONE);
    }
    return E_CreateSyncKHR(d, type, attribList ? attrs32.data() : nullptr);
}

EGLBoolean EGLAPIENTRY E_DestroySyncKHR(EGLDisplay d, EGLSyncKHR sync) {
    std::lock_guard lock(g_mu);
    EGLint err = EGL_SUCCESS;
    if (!CheckDisplayLocked(d, err)) { SetEglError(err); return EGL_FALSE; }
    auto it = g_syncs.find(sync);
    if (it == g_syncs.end()) { SetEglError(EGL_BAD_PARAMETER); return EGL_FALSE; }
    const EGLBoolean ok = g_drv.eglDestroySyncKHR ? g_drv.eglDestroySyncKHR(g_drv.display, it->second) : EGL_TRUE;
    g_syncs.erase(it);
    SetEglError(ok ? EGL_SUCCESS : g_drv.eglGetError());
    return ok;
}

EGLint EGLAPIENTRY E_ClientWaitSyncKHR(EGLDisplay d, EGLSyncKHR sync, EGLint flags, EGLTimeKHR timeout) {
    EGLSyncKHR hostSync = EGL_NO_SYNC_KHR;
    EGLDisplay hostDpy = EGL_NO_DISPLAY;
    PFNEGLCLIENTWAITSYNCKHRPROC fn = nullptr;
    {
        std::lock_guard lock(g_mu);
        EGLint err = EGL_SUCCESS;
        if (!CheckDisplayLocked(d, err)) { SetEglError(err); return EGL_FALSE; }
        auto it = g_syncs.find(sync);
        if (it == g_syncs.end() || !g_drv.eglClientWaitSyncKHR) { SetEglError(EGL_BAD_PARAMETER); return EGL_FALSE; }
        hostSync = it->second;
        hostDpy = g_drv.display;
        fn = g_drv.eglClientWaitSyncKHR;
    }
    const EGLint rc = fn(hostDpy, hostSync, flags, timeout);
    SetEglError(rc == EGL_FALSE ? EGL_BAD_PARAMETER : EGL_SUCCESS);
    return rc;
}

EGLint EGLAPIENTRY E_WaitSyncKHR(EGLDisplay d, EGLSyncKHR sync, EGLint flags) {
    std::lock_guard lock(g_mu);
    EGLint err = EGL_SUCCESS;
    if (!CheckDisplayLocked(d, err)) { SetEglError(err); return EGL_FALSE; }
    auto it = g_syncs.find(sync);
    if (it == g_syncs.end() || !g_drv.eglWaitSyncKHR) { SetEglError(EGL_BAD_PARAMETER); return EGL_FALSE; }
    const EGLint rc = g_drv.eglWaitSyncKHR(g_drv.display, it->second, flags);
    SetEglError(rc ? EGL_SUCCESS : g_drv.eglGetError());
    return rc;
}

EGLBoolean EGLAPIENTRY E_WaitSync15(EGLDisplay d, EGLSync sync, EGLint flags) {
    return E_WaitSyncKHR(d, sync, flags) ? EGL_TRUE : EGL_FALSE;
}

EGLBoolean EGLAPIENTRY E_GetSyncAttribKHR(EGLDisplay d, EGLSyncKHR sync, EGLint attr, EGLint* val) {
    std::lock_guard lock(g_mu);
    EGLint err = EGL_SUCCESS;
    if (!CheckDisplayLocked(d, err)) { SetEglError(err); return EGL_FALSE; }
    if (!val) { SetEglError(EGL_BAD_PARAMETER); return EGL_FALSE; }
    auto it = g_syncs.find(sync);
    if (it == g_syncs.end() || !g_drv.eglGetSyncAttribKHR) { SetEglError(EGL_BAD_PARAMETER); return EGL_FALSE; }
    const EGLBoolean ok = g_drv.eglGetSyncAttribKHR(g_drv.display, it->second, attr, val);
    SetEglError(ok ? EGL_SUCCESS : g_drv.eglGetError());
    return ok;
}

void (*EGLAPIENTRY E_GetProcAddress(const char* name))();

#define EGL_SYM(name, fn) {#name, reinterpret_cast<void*>(&fn)}
const guest::HostSymbol kEglSymbols[] = {
    EGL_SYM(eglGetError, E_GetError),
    EGL_SYM(eglGetDisplay, E_GetDisplay),
    EGL_SYM(eglInitialize, E_Initialize),
    EGL_SYM(eglTerminate, E_Terminate),
    EGL_SYM(eglQueryString, E_QueryString),
    EGL_SYM(eglGetConfigs, E_GetConfigs),
    EGL_SYM(eglChooseConfig, E_ChooseConfig),
    EGL_SYM(eglGetConfigAttrib, E_GetConfigAttrib),
    EGL_SYM(eglBindAPI, E_BindAPI),
    EGL_SYM(eglQueryAPI, E_QueryAPI),
    EGL_SYM(eglWaitClient, E_WaitClient),
    EGL_SYM(eglReleaseThread, E_ReleaseThread),
    EGL_SYM(eglCreateWindowSurface, E_CreateWindowSurface),
    EGL_SYM(eglCreatePbufferSurface, E_CreatePbufferSurface),
    EGL_SYM(eglDestroySurface, E_DestroySurface),
    EGL_SYM(eglQuerySurface, E_QuerySurface),
    EGL_SYM(eglSurfaceAttrib, E_SurfaceAttrib),
    EGL_SYM(eglBindTexImage, E_BindTexImage),
    EGL_SYM(eglReleaseTexImage, E_ReleaseTexImage),
    EGL_SYM(eglSwapInterval, E_SwapInterval),
    EGL_SYM(eglCreateContext, E_CreateContext),
    EGL_SYM(eglDestroyContext, E_DestroyContext),
    EGL_SYM(eglMakeCurrent, E_MakeCurrent),
    EGL_SYM(eglGetCurrentContext, E_GetCurrentContext),
    EGL_SYM(eglGetCurrentSurface, E_GetCurrentSurface),
    EGL_SYM(eglGetCurrentDisplay, E_GetCurrentDisplay),
    EGL_SYM(eglQueryContext, E_QueryContext),
    EGL_SYM(eglWaitGL, E_WaitGL),
    EGL_SYM(eglWaitNative, E_WaitNative),
    EGL_SYM(eglSwapBuffers, E_SwapBuffers),
    EGL_SYM(eglGetProcAddress, E_GetProcAddress),
    EGL_SYM(eglCreateImageKHR, E_CreateImageKHR),
    EGL_SYM(eglCreateImage, E_CreateImage15),
    EGL_SYM(eglDestroyImageKHR, E_DestroyImageKHR),
    EGL_SYM(eglDestroyImage, E_DestroyImageKHR),
    EGL_SYM(eglCreateSyncKHR, E_CreateSyncKHR),
    EGL_SYM(eglCreateSync, E_CreateSync15),
    EGL_SYM(eglDestroySyncKHR, E_DestroySyncKHR),
    EGL_SYM(eglDestroySync, E_DestroySyncKHR),
    EGL_SYM(eglClientWaitSyncKHR, E_ClientWaitSyncKHR),
    EGL_SYM(eglClientWaitSync, E_ClientWaitSyncKHR),
    EGL_SYM(eglWaitSyncKHR, E_WaitSyncKHR),
    EGL_SYM(eglWaitSync, E_WaitSync15),
    EGL_SYM(eglGetSyncAttribKHR, E_GetSyncAttribKHR),
};
#undef EGL_SYM

std::vector<guest::HostSymbol> BuildGles2And3SymbolList() {
    std::vector<guest::HostSymbol> out(std::begin(kAutoGles2And3Symbols), std::end(kAutoGles2And3Symbols));
    out.push_back({"glGetError", reinterpret_cast<void*>(&G_glGetError)});
    out.push_back({"glGetString", reinterpret_cast<void*>(&G_glGetString)});
    out.push_back({"glGetStringi", reinterpret_cast<void*>(&G_glGetStringi)});
    out.push_back({"glDebugMessageCallback", reinterpret_cast<void*>(&G_glDebugMessageCallback)});
    out.push_back({"glDebugMessageCallbackKHR", reinterpret_cast<void*>(&G_glDebugMessageCallbackKHR)});
    out.push_back({"glEGLImageTargetTexture2DOES", reinterpret_cast<void*>(&G_glEGLImageTargetTexture2DOES)});
    out.push_back({"glEGLImageTargetRenderbufferStorageOES", reinterpret_cast<void*>(&G_glEGLImageTargetRenderbufferStorageOES)});
    return out;
}

std::vector<guest::HostSymbol> BuildGles1SymbolList() {
    std::vector<guest::HostSymbol> out(std::begin(kAutoGles1Symbols), std::end(kAutoGles1Symbols));
    out.push_back({"glGetError", reinterpret_cast<void*>(&G_glGetError)});
    out.push_back({"glGetString", reinterpret_cast<void*>(&G_glGetString)});
    return out;
}

void (*EGLAPIENTRY E_GetProcAddress(const char* name))() {
    if (!name || !*name) return nullptr;
    static const std::unordered_map<std::string_view, void*> kProcMap = [] {
        std::unordered_map<std::string_view, void*> m;
        for (const auto& s : kEglSymbols) m.emplace(s.name, s.address);
        for (const auto& s : kAutoGles2And3Symbols) m.emplace(s.name, s.address);
        for (const auto& s : kAutoGles1Symbols) m.emplace(s.name, s.address);
        m.emplace("glGetError", reinterpret_cast<void*>(&G_glGetError));
        m.emplace("glGetString", reinterpret_cast<void*>(&G_glGetString));
        m.emplace("glGetStringi", reinterpret_cast<void*>(&G_glGetStringi));
        m.emplace("glDebugMessageCallback", reinterpret_cast<void*>(&G_glDebugMessageCallback));
        m.emplace("glDebugMessageCallbackKHR", reinterpret_cast<void*>(&G_glDebugMessageCallbackKHR));
        m.emplace("glEGLImageTargetTexture2DOES", reinterpret_cast<void*>(&G_glEGLImageTargetTexture2DOES));
        m.emplace("glEGLImageTargetRenderbufferStorageOES", reinterpret_cast<void*>(&G_glEGLImageTargetRenderbufferStorageOES));
        return m;
    }();
    auto it = kProcMap.find(name);
    if (it == kProcMap.end()) return nullptr;
    return reinterpret_cast<void (*)()>(it->second);
}

}  // namespace

bool GuestGraphicsActive() {
    std::lock_guard lock(g_mu);
    return g_drv.initRefs > 0;
}

bool GuestGraphicsPresents(HWND window) {
    (void)window;
    // Guest EGL window surfaces present via WinApkCompositor, so Paint delegates to CompositorPresentWindow.
    return false;
}

void RetireGuestGraphics(HWND window, bool closing) {
    std::lock_guard lock(g_mu);
    if (closing) {
        ShutdownLocked();
        return;
    }
    for (auto& [id, surf] : g_surfaces) {
        if (surf.isWindow && (!window || surf.hwnd == window)) {
            if (surf.nativeToken) {
                const int key = -static_cast<int>(reinterpret_cast<uintptr_t>(surf.nativeToken));
                CompositorUpdateSurfacePlacement(surf.hwnd, key, {}, true);
                ReleaseSurface(surf.nativeToken);
                surf.nativeToken = nullptr;
            }
            surf.hwnd = nullptr;
        }
    }
}

void AddGuestGraphicsLibraries(std::vector<guest::BuiltinLibrary>& libraries) {
    std::vector<guest::HostSymbol> eglSyms(std::begin(kEglSymbols), std::end(kEglSymbols));
    auto gles1Syms = BuildGles1SymbolList();
    auto gles2Syms = BuildGles2And3SymbolList();
    libraries.push_back({"libEGL.so", std::move(eglSyms), ""});
    libraries.push_back({"libGLESv1_CM.so", std::move(gles1Syms), ""});
    libraries.push_back({"libGLESv2.so", gles2Syms, ""});
    libraries.push_back({"libGLESv3.so", std::move(gles2Syms), ""});
}

}  // namespace winapk::bridge
#else
namespace winapk::bridge {
bool GuestGraphicsActive() { return false; }
bool GuestGraphicsPresents(HWND) { return false; }
void RetireGuestGraphics(HWND, bool) {}
void AddGuestGraphicsLibraries(std::vector<guest::BuiltinLibrary>&) {}
}  // namespace winapk::bridge
#endif
