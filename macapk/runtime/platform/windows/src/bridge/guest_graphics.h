#pragma once

#include "surface.h"
#include <functional>

namespace winapk::bridge {

/// Runs one bounded guest graphics command on the HWND owner; returns false if the loop cannot accept it.
bool DispatchGuestGraphics(const std::function<void()>& command);
/// Returns the selected ANGLE backend on the window thread, or off if a Java frame/display owns ANGLE.
const std::string& GuestGraphicsRequest();
/// Adds restricted EGL/GLES logical libraries when the ANGLE headers were available at build time.
void AddGuestGraphicsLibraries(std::vector<guest::BuiltinLibrary>& libraries);
/// Reports whether a guest EGL session has reserved ANGLE on the window thread.
bool GuestGraphicsActive();
/// Reports whether the attached guest EGL drawable owns presentation of this window.
bool GuestGraphicsPresents(HWND window);
/// Retires the guest EGL drawable for a detached HWND, and tears down the display on window close.
void RetireGuestGraphics(HWND window, bool closing = false);
/// Resolves the bridge's private ANGLE directory; never uses caller paths or the process CWD.
std::wstring PrivateAngleDirectory();

} // namespace winapk::bridge
