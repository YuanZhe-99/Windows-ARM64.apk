#pragma once

#include "../../../../guest/loader/elf_loader.h"
#include <jni.h>
#include <windows.h>

namespace winapk::bridge {

/// Publishes the current window and its actual holder Surface; a different pair retires the old one.
/// Notes: only the window thread publishes; Java reference is owned by its View until retired.
void PublishSurface(HWND window, jobject surface, jfieldID released, int width, int height);
/// Updates an attached window's client size without changing the attachment identity.
void ResizeSurface(HWND window, int width, int height);
/// Removes the attachment before its JNI reference or window disappears. Null retires any window.
void RetireSurface(HWND window);
/// Takes a snapshot of a live guest token on the window thread, without exposing its HWND to the guest.
bool ResolveSurface(void* token, HWND& window, int& width, int& height);
/// Retains a live guest token for a graphics surface until that surface is destroyed.
bool RetainSurface(void* token);
/// Releases the graphics surface's lease on a native window token.
void ReleaseSurface(void* token);
/// Reports whether a host window/Surface attachment is currently published for guest WSI.
bool SurfaceBridgeReady();
/// Lists the five fixed Android native window exports in the restricted logical library.
guest::BuiltinLibrary NativeWindowLibrary();

} // namespace winapk::bridge
