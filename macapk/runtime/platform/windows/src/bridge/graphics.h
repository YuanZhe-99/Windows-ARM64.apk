// graphics.h -- Windows host EGL/GLES runtime and full Java OpenGL ES 1.0–3.2 / EGL 1.4–1.5 JNI
// bindings (GPU-P2).
#pragma once

#include <jni.h>
#include <windows.h>

#include <memory>
#include <string>

namespace winapk::bridge {

/// Records the process `--graphics` mode (`off`, `auto`, `d3d11`, `vulkan`) for Java `EGL14` /
/// `MacApkEGL10` / `TextureView` contexts as well as `GLSurfaceView`.
void SetJavaGraphicsRequest(const std::string& request);

/// Returns the active top-level app `HWND` and client extent for `EGL14` `SurfaceView` windows.
void* EnsureJavaSurfaceViewWindow(int* widthOut, int* heightOut);

/// Owns a Win32 EGL surface/context for host-driven `GLSurfaceView` (ES 1.1, 2.0, or 3.x).
class JavaGraphics {
public:
    /// Creates an EGL session from the private ANGLE directory for `clientVersion` (1, 2, or 3).
    static std::shared_ptr<JavaGraphics> Open(HWND window, const std::string& request,
                                              int clientVersion, std::string& error);
    /// Releases the EGL surface and context and decrements the display reference count.
    ~JavaGraphics();
    /// Makes the context current, renewing only the drawable on a client-size change.
    bool Begin(int width, int height, std::string& error);
    /// Optionally presents the completed frame and always unbinds the context.
    bool End(bool present, std::string& error);
private:
    struct State;
    explicit JavaGraphics(std::unique_ptr<State> state);
    std::unique_ptr<State> state_;
};

/// Registers all 667+ Java OpenGL ES (`GLES10`, `GLES11`, `GLES20`, `GLUtils`, `GLES30`,
/// `GLES31`, `GLES31Ext`, `GLES32`) and EGL (`EGL14`, `EGL15`, `EGLExt`) JNI methods.
bool RegisterJavaGraphics(JNIEnv* env);

} // namespace winapk::bridge
