// compositor.h -- Direct3D 11 / DXGI multi-layer hardware compositor and 3-slot asynchronous
// `--embed` staging readback (ADR-0005, GPU-P1).
//
// Host-domain code only. Composites SurfaceFlinger-ordered layers:
//   [below-window SurfaceViews (zOrder == 1)]
//     -> [ViewCanvasDriver UI layer with preserved straight-alpha punchHole cutouts]
//     -> [above-window SurfaceViews (zOrder == 2 or legacy 0)]
// Presents via a DXGI flip-model swapchain on standalone HWNDs (when multi-layer or offscreen GPU
// layers are active) and via a 3-slot D3D11_USAGE_STAGING ring into `Local\winapk-frame-<token>`
// when `--embed` is active.
#pragma once

#include <windows.h>

#include <cstddef>
#include <cstdint>

namespace winapk::bridge {

/// Alpha interpretation of a submitted layer texture.
enum class LayerAlpha {
    Opaque,        ///< Ignore source alpha and force A = 1.0 (e.g. RGBX swapchains)
    Straight,      ///< Unpremultiplied source alpha (Android Bitmap / BufferedImage ARGB)
    Premultiplied  ///< Premultiplied source alpha (Skia / GPU RGBA render targets)
};

/// Pixel channel order of CPU-uploaded layer data.
enum class LayerPixels {
    Rgba8,  ///< Byte order R, G, B, A
    Bgra8   ///< Byte order B, G, R, A (`0xAARRGGBB` little-endian `uint32_t`)
};

/// Placement and Z-order of a non-UI `SurfaceView` layer in client-area pixels.
struct SurfaceLayerPlacement {
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
    bool visible = true;
    bool zOrderOnTop = false;  ///< false = below UI (hole-punched); true = above UI
};

/// Submits or updates the main Java UI canvas (`ViewCanvasDriver`) for `hwnd`.
/// Inputs: `argb` is top-down `0xAARRGGBB` (`w * h` pixels); `dirtyX/Y/W/H` optionally restricts
///         upload to a dirty sub-rectangle when the dimensions match the previous frame.
/// Returns: true when hardware D3D11 composited/published the frame; false when no hardware D3D11
///          device is available (caller falls back to GDI / software shared-memory copy).
bool CompositorSubmitUiArgb(HWND hwnd, const uint32_t* argb, int w, int h,
                            int dirtyX = 0, int dirtyY = 0, int dirtyW = 0, int dirtyH = 0);

/// Submits or updates a numbered `SurfaceView` / producer layer from CPU/staging bytes.
/// Inputs: `layerKey` (> 0 for Java `presentSurfaceBitmap` keys, < 0 for native window tokens);
///         `data` has `rowPitch` bytes per row; `bottomUp` flips OpenGL readbacks on the GPU.
bool CompositorSubmitSurfaceBytes(HWND hwnd, int layerKey, const void* data, size_t rowPitch,
                                  int w, int h, LayerPixels fmt, LayerAlpha alpha, bool bottomUp,
                                  const SurfaceLayerPlacement& placement);

/// Submits or updates a `SurfaceView` / producer layer by zero-copy GPU copy from a DXGI shared
/// `ID3D11Texture2D` handle (such as ANGLE D3D11's `EGL_D3D_TEXTURE_2D_SHARE_HANDLE_ANGLE`).
bool CompositorSubmitSurfaceSharedTexture(HWND hwnd, int layerKey, HANDLE dxgiSharedHandle,
                                          int w, int h, LayerAlpha alpha, bool bottomUp,
                                          const SurfaceLayerPlacement& placement);

/// Updates placement/visibility of an existing `SurfaceView` layer, or removes it when `drop`.
void CompositorUpdateSurfacePlacement(HWND hwnd, int layerKey, const SurfaceLayerPlacement& placement, bool drop);

/// Removes all `SurfaceView` producer layers and stale UI content for `hwnd` when switching root views.
void CompositorClearSurfaceLayers(HWND hwnd);

/// Publishes any in-flight asynchronous `--embed` staging frame that completed since the last submit.
void CompositorFlushEmbed(HWND hwnd);

/// Repaints `hwnd`'s composited frame (used on `WM_PAINT` / `WM_SIZE` when hardware layers exist).
/// Returns: true if `WinApkCompositor` owns presentation for `hwnd`.
bool CompositorPresentWindow(HWND hwnd, int clientW, int clientH);

/// Destroys all D3D11/DXGI resources associated with `hwnd`.
void CompositorDestroyWindow(HWND hwnd);

/// Reports whether `WinApkCompositor` has an active hardware D3D11 device in this process.
bool CompositorAvailable();

}  // namespace winapk::bridge
