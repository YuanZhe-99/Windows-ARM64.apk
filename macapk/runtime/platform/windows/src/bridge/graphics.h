// Host-only EGL owner for a Java GLSurfaceView. No guest handles or dispatch.
#pragma once

#include <jni.h>
#include <windows.h>

#include <memory>
#include <string>

namespace winapk::bridge {

/// Owns a single Win32 EGL surface/context; all operations run on the window thread.
class JavaGraphics {
public:
    /// Creates an EGL session from the private ANGLE directory, or supplies a diagnostic.
    static std::shared_ptr<JavaGraphics> Open(HWND window, const std::string& request, std::string& error);
    /// Releases the EGL surface and context before unloading its DLLs.
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

/// Registers the supported GLES20 natives, without loading ANGLE.
bool RegisterJavaGraphics(JNIEnv* env);

} // namespace winapk::bridge
