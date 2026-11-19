// surface.h -- multi-surface Android ANativeWindow registry and CPU lock/post bridge (GPU-P1).
#pragma once

#include "compositor.h"
#include "../../../../guest/loader/elf_loader.h"
#include <jni.h>
#include <windows.h>

namespace winapk::bridge {

/// Publishes a live holder `Surface` on `window`.
/// Inputs: `replaceRoot` retires the previous root attachment on `window` when `SetContentView`
///         replaces a root `SurfaceView`; `false` registers an additional child `SurfaceView`
///         inside a `ViewGroup` without retiring sibling surfaces.
void PublishSurface(HWND window, jobject surface, jfieldID released, int width, int height, bool replaceRoot = true);

/// Updates client dimensions of root/default-sized surfaces attached to `window`.
void ResizeSurface(HWND window, int width, int height);

/// Updates an individual published `Surface`'s pixel bounds and Z-order placement.
void UpdateSurfaceBounds(HWND window, jobject surface, const SurfaceLayerPlacement& placement);

/// Retires one specific `Surface` identity when its `View` renderer is released.
void RetireSurfaceObject(JNIEnv* env, jobject surface);

/// Removes all attachments on `window` before its JNI references or HWND disappear. Null retires all.
void RetireSurface(HWND window);

/// Takes a snapshot of a live guest `ANativeWindow*` token without exposing `HWND` to guest code.
bool ResolveSurface(void* token, HWND& window, int& width, int& height, SurfaceLayerPlacement* placementOut = nullptr);

/// Retains a live guest token for an EGL/Vulkan surface until that surface is destroyed.
bool RetainSurface(void* token);

/// Releases the graphics surface's lease on a native window token.
void ReleaseSurface(void* token);

/// Reports whether the surface bridge is active for guest WSI.
bool SurfaceBridgeReady();

/// Lists the Android native window exports in the logical `libandroid.so` library.
guest::BuiltinLibrary NativeWindowLibrary();

} // namespace winapk::bridge
