// P5a host-only EGL/GLES experiment. A passing host backend never establishes Java/guest graphics.
#include "doctor.h"
#include <windows.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <EGL/eglext_angle.h>
#include <GLES2/gl2.h>
#include <vulkan/vulkan.h>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>

namespace {

/// Owns the fixed host EGL/GLES entries used by this experiment, not an extension catch-all.
struct Api {
#define ENTRIES(X) \
    X(PFNEGLGETPROCADDRESSPROC, eglGetProcAddress) \
    X(PFNEGLQUERYSTRINGPROC, eglQueryString) \
    X(PFNEGLINITIALIZEPROC, eglInitialize) \
    X(PFNEGLBINDAPIPROC, eglBindAPI) \
    X(PFNEGLCHOOSECONFIGPROC, eglChooseConfig) \
    X(PFNEGLCREATEWINDOWSURFACEPROC, eglCreateWindowSurface) \
    X(PFNEGLCREATECONTEXTPROC, eglCreateContext) \
    X(PFNEGLMAKECURRENTPROC, eglMakeCurrent) \
    X(PFNEGLQUERYSURFACEPROC, eglQuerySurface) \
    X(PFNEGLSWAPBUFFERSPROC, eglSwapBuffers) \
    X(PFNEGLDESTROYCONTEXTPROC, eglDestroyContext) \
    X(PFNEGLDESTROYSURFACEPROC, eglDestroySurface) \
    X(PFNEGLTERMINATEPROC, eglTerminate) \
    X(PFNEGLGETERRORPROC, eglGetError) \
    X(PFNGLGETSTRINGPROC, glGetString) \
    X(PFNGLGETERRORPROC, glGetError) \
    X(PFNGLCREATESHADERPROC, glCreateShader) \
    X(PFNGLSHADERSOURCEPROC, glShaderSource) \
    X(PFNGLCOMPILESHADERPROC, glCompileShader) \
    X(PFNGLGETSHADERIVPROC, glGetShaderiv) \
    X(PFNGLGETSHADERINFOLOGPROC, glGetShaderInfoLog) \
    X(PFNGLDELETESHADERPROC, glDeleteShader) \
    X(PFNGLCREATEPROGRAMPROC, glCreateProgram) \
    X(PFNGLATTACHSHADERPROC, glAttachShader) \
    X(PFNGLBINDATTRIBLOCATIONPROC, glBindAttribLocation) \
    X(PFNGLLINKPROGRAMPROC, glLinkProgram) \
    X(PFNGLGETPROGRAMIVPROC, glGetProgramiv) \
    X(PFNGLGETPROGRAMINFOLOGPROC, glGetProgramInfoLog) \
    X(PFNGLDELETEPROGRAMPROC, glDeleteProgram) \
    X(PFNGLUSEPROGRAMPROC, glUseProgram) \
    X(PFNGLVIEWPORTPROC, glViewport) \
    X(PFNGLVERTEXATTRIBPOINTERPROC, glVertexAttribPointer) \
    X(PFNGLENABLEVERTEXATTRIBARRAYPROC, glEnableVertexAttribArray) \
    X(PFNGLDRAWARRAYSPROC, glDrawArrays) \
    X(PFNGLREADPIXELSPROC, glReadPixels)
#define FIELD(Type, Name) Type Name = nullptr;
    ENTRIES(FIELD)
#undef FIELD
    /// Resolves only the declared host functions from the two private ARM64 libraries.
    bool Load(HMODULE egl, HMODULE gles) {
#define LOAD(Type, Name) \
        Name = reinterpret_cast<Type>(GetProcAddress(#Name[0] == 'e' ? egl : gles, #Name)); \
        if (!Name) { std::printf("GRAPHICS_CAPABILITY_MISSING entry %s\n", #Name); return false; }
        ENTRIES(LOAD)
#undef LOAD
        return true;
    }
#undef ENTRIES
};

/// Pumps the owning window thread between driver calls, never while holding a JVM/guest lock.
void Pump() {
    MSG message;
    while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
}

/// Handles close/paint without embedding runtime or guest behavior in the probe window.
LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    if (message == WM_CLOSE) { DestroyWindow(window); return 0; }
    return DefWindowProcW(window, message, wparam, lparam);
}

/// Checks one complete space-delimited EGL extension name.
bool Extension(const char* text, const char* name) {
    if (!text) return false;
    const size_t length = std::strlen(name);
    for (const char* at = text; (at = std::strstr(at, name)); at += length)
        if ((at == text || at[-1] == ' ') && (at[length] == ' ' || !at[length])) return true;
    return false;
}

/// Reports the actual ANGLE device; rejects software instead of treating it as hardware evidence.
bool Device(const Api& api, EGLDisplay display, bool vulkan) {
    const auto queryDisplay = reinterpret_cast<PFNEGLQUERYDISPLAYATTRIBEXTPROC>(api.eglGetProcAddress("eglQueryDisplayAttribEXT"));
    const auto queryDevice = reinterpret_cast<PFNEGLQUERYDEVICEATTRIBEXTPROC>(api.eglGetProcAddress("eglQueryDeviceAttribEXT"));
    EGLAttrib value = 0;
    if (!queryDisplay || !queryDevice || !queryDisplay(display, EGL_DEVICE_EXT, &value)) return false;
    const auto device = reinterpret_cast<EGLDeviceEXT>(value);
    if (vulkan) {
        EGLAttrib instance = 0, physical = 0;
        if (!queryDevice(device, EGL_VULKAN_INSTANCE_ANGLE, &instance) ||
            !queryDevice(device, EGL_VULKAN_PHYSICAL_DEVICE_ANGLE, &physical)) return false;
        HMODULE loader = GetModuleHandleW(L"vulkan-1.dll");
        const auto get = loader ? reinterpret_cast<PFN_vkGetInstanceProcAddr>(GetProcAddress(loader, "vkGetInstanceProcAddr")) : nullptr;
        if (!get) return false;
        const auto properties = reinterpret_cast<PFN_vkGetPhysicalDeviceProperties2>(
            get(reinterpret_cast<VkInstance>(instance), "vkGetPhysicalDeviceProperties2"));
        if (!properties) return false;
        VkPhysicalDeviceProperties2 info{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};
        properties(reinterpret_cast<VkPhysicalDevice>(physical), &info);
        VkPhysicalDeviceDriverProperties driver{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES};
        if (info.properties.apiVersion >= VK_API_VERSION_1_2) {
            info.pNext = &driver;
            properties(reinterpret_cast<VkPhysicalDevice>(physical), &info);
        }
        const bool hasDriver = info.properties.apiVersion >= VK_API_VERSION_1_2;
        const bool translated = driver.driverID == VK_DRIVER_ID_MESA_DOZEN;
        std::printf("device backend=vulkan name=%s driver=%s info=%s api=%u.%u.%u translated=%s software=%s vendor=0x%x device=0x%x driverVersion=%u\n",
            info.properties.deviceName, driver.driverName, driver.driverInfo,
            VK_API_VERSION_MAJOR(info.properties.apiVersion), VK_API_VERSION_MINOR(info.properties.apiVersion),
            VK_API_VERSION_PATCH(info.properties.apiVersion), hasDriver ? (translated ? "yes" : "no") : "unknown",
            info.properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_CPU ? "yes" : "no",
            info.properties.vendorID, info.properties.deviceID, info.properties.driverVersion);
        return info.properties.deviceType != VK_PHYSICAL_DEVICE_TYPE_CPU;
    }
    EGLAttrib raw = 0;
    if (!queryDevice(device, EGL_D3D11_DEVICE_ANGLE, &raw)) return false;
    auto* d3d = reinterpret_cast<ID3D11Device*>(raw); // Borrowed host COM object, never a guest handle.
    IDXGIDevice* dxgi = nullptr;
    IDXGIAdapter* adapter = nullptr;
    IDXGIAdapter1* typed = nullptr;
    DXGI_ADAPTER_DESC1 description{};
    bool ok = d3d && SUCCEEDED(d3d->QueryInterface(IID_PPV_ARGS(&dxgi))) &&
        SUCCEEDED(dxgi->GetAdapter(&adapter)) && SUCCEEDED(adapter->QueryInterface(IID_PPV_ARGS(&typed))) &&
        SUCCEEDED(typed->GetDesc1(&description));
    if (ok) {
        LARGE_INTEGER driver{};
        const bool hasDriver = SUCCEEDED(adapter->CheckInterfaceSupport(__uuidof(IDXGIDevice), &driver));
        std::printf("device backend=d3d11 name=%s vendor=0x%x device=0x%x software=%s driver=%u.%u.%u.%u driverKnown=%s\n",
            winapk::Utf8(description.Description).c_str(), description.VendorId, description.DeviceId,
            description.Flags & DXGI_ADAPTER_FLAG_SOFTWARE ? "yes" : "no",
            HIWORD(driver.HighPart), LOWORD(driver.HighPart), HIWORD(driver.LowPart), LOWORD(driver.LowPart),
            hasDriver ? "yes" : "no");
        ok = hasDriver && !(description.Flags & DXGI_ADAPTER_FLAG_SOFTWARE);
    }
    if (typed) typed->Release();
    if (adapter) adapter->Release();
    if (dxgi) dxgi->Release();
    return ok;
}

/// Compiles one deterministic ES2 shader, preserving driver diagnostics on failure.
GLuint Shader(const Api& api, GLenum kind, const char* text) {
    GLuint shader = api.glCreateShader(kind);
    api.glShaderSource(shader, 1, &text, nullptr);
    api.glCompileShader(shader);
    GLint compiled = GL_FALSE;
    api.glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (compiled) return shader;
    char log[2048]{};
    api.glGetShaderInfoLog(shader, sizeof log, nullptr, log);
    std::printf("GRAPHICS_RENDER_FAILED shader: %s\n", log);
    api.glDeleteShader(shader);
    return 0;
}

/// Draws a full-covering triangle and reads two interior pixels with a one-byte rounding tolerance.
bool Draw(const Api& api, EGLDisplay display, EGLSurface surface, HWND window, GLuint program) {
    EGLint width = 0, height = 0;
    RECT client{};
    if (!api.eglQuerySurface(display, surface, EGL_WIDTH, &width) ||
        !api.eglQuerySurface(display, surface, EGL_HEIGHT, &height) || width < 8 || height < 8) return false;
    if (!GetClientRect(window, &client) || width != client.right || height != client.bottom) {
        std::printf("GRAPHICS_RENDER_FAILED surface extent=%dx%d client=%ldx%ld\n",
            width, height, client.right, client.bottom);
        return false;
    }
    const GLfloat positions[] = {-1, -1, 3, -1, -1, 3};
    api.glUseProgram(program);
    api.glViewport(0, 0, width, height);
    api.glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, positions);
    api.glEnableVertexAttribArray(0);
    api.glDrawArrays(GL_TRIANGLES, 0, 3);
    bool ok = true;
    for (int divisor : {2, 4}) {
        GLubyte pixel[4]{};
        api.glReadPixels(width / divisor, height / divisor, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
        const int expected[] = {64, 128, 191, 255};
        for (int n = 0; n < 4; n++) {
            const int delta = static_cast<int>(pixel[n]) - expected[n];
            ok = ok && delta >= -1 && delta <= 1;
        }
        std::printf("pixel size=%dx%d at=%d,%d rgba=%u,%u,%u,%u tolerance=1\n", width, height,
            width / divisor, height / divisor, pixel[0], pixel[1], pixel[2], pixel[3]);
    }
    return ok && api.glGetError() == GL_NO_ERROR && api.eglSwapBuffers(display, surface);
}

/// Runs a single explicitly selected backend with reverse-order EGL/Win32 teardown.
int Probe(const std::filesystem::path& directory, bool vulkan) {
    const char* backend = vulkan ? "vulkan" : "d3d11";
    std::printf("request T10.host backend=%s (not G4)\n", backend);
    HMODULE libraries[2]{};
    int index = 0;
    for (const auto* name : {L"libEGL.dll", L"libGLESv2.dll"}) {
        auto path = directory / name;
        if (winapk::PeArch(path.wstring()) != "arm64") {
            std::printf("GRAPHICS_CAPABILITY_MISSING ARM64 ANGLE DLL\n"); return 2;
        }
        libraries[index] = LoadLibraryExW(path.c_str(), nullptr,
            LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
        if (!libraries[index++]) { std::printf("GRAPHICS_CAPABILITY_MISSING ANGLE load error=%lu\n", GetLastError()); return 2; }
    }
    Api api;
    if (!api.Load(libraries[0], libraries[1])) return 2;
    auto platform = reinterpret_cast<PFNEGLGETPLATFORMDISPLAYEXTPROC>(api.eglGetProcAddress("eglGetPlatformDisplayEXT"));
    const char* extensions = api.eglQueryString(EGL_NO_DISPLAY, EGL_EXTENSIONS);
    if (!platform || !Extension(extensions, "EGL_ANGLE_platform_angle") ||
        !Extension(extensions, vulkan ? "EGL_ANGLE_platform_angle_vulkan" : "EGL_ANGLE_platform_angle_d3d")) {
        std::printf("GRAPHICS_CAPABILITY_MISSING requested ANGLE backend extension\n"); return 2;
    }
    const EGLint attributes[] = {EGL_PLATFORM_ANGLE_TYPE_ANGLE,
        vulkan ? EGL_PLATFORM_ANGLE_TYPE_VULKAN_ANGLE : EGL_PLATFORM_ANGLE_TYPE_D3D11_ANGLE,
        EGL_PLATFORM_ANGLE_DEVICE_TYPE_ANGLE, EGL_PLATFORM_ANGLE_DEVICE_TYPE_HARDWARE_ANGLE, EGL_NONE};
    EGLDisplay display = platform(EGL_PLATFORM_ANGLE_ANGLE, EGL_DEFAULT_DISPLAY, attributes);
    EGLint major = 0, minor = 0;
    if (display == EGL_NO_DISPLAY || !api.eglInitialize(display, &major, &minor)) {
        std::printf("GRAPHICS_CAPABILITY_MISSING backend=%s egl=0x%x; no fallback\n", backend, api.eglGetError()); return 2;
    }
    HINSTANCE instance = GetModuleHandleW(nullptr);
    WNDCLASSW cls{};
    cls.lpfnWndProc = WindowProc; cls.hInstance = instance; cls.lpszClassName = L"WinApkAngleProbe";
    RegisterClassW(&cls);
    HWND window = CreateWindowExW(0, cls.lpszClassName, L"Windows-ARM64.apk ANGLE probe", WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT, 320, 240, nullptr, nullptr, instance, nullptr);
    EGLSurface surface = EGL_NO_SURFACE;
    EGLContext context = EGL_NO_CONTEXT;
    GLuint program = 0;
    bool ok = false;
    do {
        const EGLint configArgs[] = {EGL_SURFACE_TYPE, EGL_WINDOW_BIT, EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
            EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8, EGL_NONE};
        EGLConfig config{};
        EGLint count = 0;
        if (!window || !api.eglBindAPI(EGL_OPENGL_ES_API) || !api.eglChooseConfig(display, configArgs, &config, 1, &count) || !count) break;
        surface = api.eglCreateWindowSurface(display, config, window, nullptr);
        const EGLint contextArgs[] = {EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE};
        context = api.eglCreateContext(display, config, EGL_NO_CONTEXT, contextArgs);
        if (surface == EGL_NO_SURFACE || context == EGL_NO_CONTEXT || !api.eglMakeCurrent(display, surface, surface, context)) break;
        const char* renderer = reinterpret_cast<const char*>(api.glGetString(GL_RENDERER));
        const char* version = reinterpret_cast<const char*>(api.glGetString(GL_VERSION));
        if (!renderer || !version) break;
        std::printf("renderer backend=%s name=%s gl=%s egl=%d.%d\n", backend, renderer, version, major, minor);
        std::printf("egl.extensions=%s\ngl.extensions=%s\n", api.eglQueryString(display, EGL_EXTENSIONS),
            reinterpret_cast<const char*>(api.glGetString(GL_EXTENSIONS)));
        if (!(vulkan ? std::strstr(renderer, "Vulkan") != nullptr :
            std::strstr(renderer, "Direct3D11") || std::strstr(renderer, "D3D11")) || !Device(api, display, vulkan)) break;
        GLuint vertex = Shader(api, GL_VERTEX_SHADER, "attribute vec2 position;void main(){gl_Position=vec4(position,0.0,1.0);}");
        GLuint fragment = Shader(api, GL_FRAGMENT_SHADER, "precision mediump float;void main(){gl_FragColor=vec4(0.25,0.5,0.75,1.0);}");
        if (vertex && fragment) {
            program = api.glCreateProgram();
            api.glAttachShader(program, vertex); api.glAttachShader(program, fragment);
            api.glBindAttribLocation(program, 0, "position"); api.glLinkProgram(program);
        }
        if (vertex) api.glDeleteShader(vertex);
        if (fragment) api.glDeleteShader(fragment);
        GLint linked = GL_FALSE;
        if (program) api.glGetProgramiv(program, GL_LINK_STATUS, &linked);
        if (!linked) {
            char log[2048]{};
            if (program) api.glGetProgramInfoLog(program, sizeof log, nullptr, log);
            std::printf("GRAPHICS_RENDER_FAILED link: %s\n", log); break;
        }
        Pump();
        if (!Draw(api, display, surface, window, program)) break;
        SetWindowPos(window, nullptr, 0, 0, 416, 304, SWP_NOMOVE | SWP_NOZORDER);
        Pump();
        // A swap lets ANGLE observe the new Win32 extent before querying and checking it.
        if (!api.eglSwapBuffers(display, surface)) break;
        Pump();
        ok = Draw(api, display, surface, window, program);
    } while (false);
    if (program) api.glDeleteProgram(program);
    ok = api.eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT) && ok;
    if (context != EGL_NO_CONTEXT) ok = api.eglDestroyContext(display, context) && ok;
    if (surface != EGL_NO_SURFACE) ok = api.eglDestroySurface(display, surface) && ok;
    ok = api.eglTerminate(display) && ok;
    if (window && IsWindow(window)) DestroyWindow(window);
    UnregisterClassW(cls.lpszClassName, instance);
    for (HMODULE library : libraries) FreeLibrary(library);
    std::printf("result T10.host %s backend=%s (not G4)\n", ok ? "PASS" : "FAIL", backend);
    return ok ? 0 : 3;
}
} // namespace

/// Accepts one backend and private DLL directory; no auto selection or hidden backend fallback exists.
int wmain(int argc, wchar_t** argv) {
    if (argc != 3 || (std::wstring(argv[1]) != L"vulkan" && std::wstring(argv[1]) != L"d3d11")) return 64;
    SetDefaultDllDirectories(LOAD_LIBRARY_SEARCH_DEFAULT_DIRS | LOAD_LIBRARY_SEARCH_USER_DIRS);
    return Probe(std::filesystem::absolute(argv[2]), std::wstring(argv[1]) == L"vulkan");
}
