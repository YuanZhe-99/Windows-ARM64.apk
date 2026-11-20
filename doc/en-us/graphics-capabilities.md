# Graphics capabilities

P5/P6 are in progress after G3. No graphics gate has passed yet. The Java bitmap path remains
independent; enabling guest native does not enable GLES or guest Vulkan.

## Verification paths

| Path | State | Required evidence |
|---|---|---|
| Host ANGLE/D3D11 | PASS | Adreno 8cx Gen 3 hardware, driver `30.0.4122.4000`, EGL 1.5 / GLES 3.0; draw/readback, resize and teardown |
| Host ANGLE/Vulkan on Dozen | PASS; user-approved experiment (ADR-0004) | same Adreno GPU via D3D12, Mesa `26.3.0-devel (git-e3fc7749c6)`, Vulkan `1.2.359`, EGL 1.5 / GLES 2.0; `translated=yes software=no` |
| Java GLES into ANGLE | PASS (`GLES10/11/20/30/31/32` + `EGL14/15/EGLExt`) | real APK `GLSurfaceView` ES 2.0 & ES 3.0 (VAO/UBO/`glTexStorage2D`/FBO MRT/instancing/blit), worker-thread `EGL14` `SurfaceTexture` (`MacApkGlWindow`), resize, restore, teardown, and explicit ES3-on-Dozen refusal |
| Managed native window | PASS for the reference profile below | real restricted APK, client extent, replacement and refused references; no drawing |
| Guest GLES/N2 | PASS (`libEGL.so` + `libGLESv1_CM/v2/v3.so`) | normal NDK APK (`DT_NEEDED` ES2 + ES3); multi-thread `pthread` `share_context`, `Pbuffer`, `KHR_surfaceless_context`, `EGLImageKHR` (`glEGLImageTargetTexture2DOES`), ES 3.0 VAO on D3D11 + ES 3.0 refusal on Dozen, resize, minimize/restore, surface replacement and explicit Vulkan refusal |
| Guest Vulkan instance/WSI | PASS for T13.wsi on Dozen | real APK, Android→Win32 surface, actual support/capabilities, resize, replacement and refusal; no drawing |
| Guest Vulkan transfer-clear | PASS for the bounded T13 profile on Dozen | real guest device/queue, command buffer and swapchain; three presents across resize |
| Guest Vulkan transfer buffer | PASS for the bounded T13 profile on Dozen | coherent allocation and GPU fill, exact 16-word readback and lifecycle refusals |
| Guest Vulkan graphics pipeline, image and callbacks | PASS on Dozen (`libvulkan.so` 1.0–1.3, 290+ commands) | SPIR-V `VkGraphicsPipelineCreateInfo`, `VkPipelineCache`, descriptors/sampler/push-constants, `4x4` `VkImage` render-pass draw & readback (`0xFFCC8822`), `--embed` WSI readback, and CFG-exempt `VkAllocationCallbacks` + `VK_EXT_debug_report`/`debug_utils` thunks |
| Guest Vulkan compute experiment | FAIL on this Dozen stack; not shipped | `vkCreateComputePipelines` returned `VK_ERROR_OUT_OF_HOST_MEMORY` on Adreno SM 6.2 (`WaveLaneMin=64, Max=128`) while WARP SM 6.6+ succeeded |
| Guest Vulkan validation layer and G4 | NOT_RUN / BLOCKED | `VK_LAYER_KHRONOS_validation` not installed (`NOT_RUN`); vendor ICD absent (`BLOCKED`) |
| Vendor ARM64 Vulkan ICD | BLOCKED | a machine with that driver; Dozen results cannot fill this row |

## ANGLE source build

`build-lock.json` chooses ANGLE `801a1a5f6e00ed0028937067b7663fd7053bff31`.
One audited patch to its pinned Chromium build dependency changes a custom-toolchain runtime
directory value from the wrong list type to the same `"None"` string its normal Python path
emits. The same patch selects the locked SDK 10.0.26100.0 in both Python toolchain scripts,
which otherwise force SDK 28000 despite the GN override. No SDK is silently installed.
It also omits Chromium debugger/symbolizer DLL copies, which these EGL/GLES DLL targets do not
use; CRT copying remains unchanged. Missing optional x64 SDK debuggers do not justify an install.
The NTDDI build target is set to `NTDDI_WIN11_GE`, the SDK 26100 definition, rather than
the undefined newer `NTDDI_WIN11_BR` that hides required header declarations on this SDK.
ANGLE renderer code is unchanged; no Mac.apk/Metal patch is copied.
The patch's forward application to clean pinned Git blobs and its reverse check both pass;
the build also accepts the same patch on CRLF checkouts without changing global Git settings.
The source build and ARM64 PE staging pass. `scripts/windows/angle-args.gn` requests ARM64 EGL/GLES DLLs
with Vulkan and D3D11 only, no bundled Vulkan loader, SwiftShader, OpenCL or WebGPU backend.
The system's existing Vulkan loader/ICDs are used unchanged; no driver is copied or installed.
The source and tools live under ignored `build/windows-arm64/deps`, never the Mac.apk subtree.
depot_tools and ANGLE DEPS are recorded; build tools may be emulated, shipped PE outputs may not.
Restricted/private upstream trace assets and unused OpenCL/Dawn dependencies are excluded.
Rust and Siso are disabled for these C++ DLL targets. The pinned resource-compiler object is
fetched over public HTTPS and checked against its upstream hash plus a recorded SHA-256;
this avoids gsutil's MAX_PATH extraction failure without changing Windows settings.

Upstream's `doc/DevSetup.md` describes standalone GN/Ninja builds and backend selection.
Its `gni/angle.gni` still has a comment saying Windows ARM64 lacks Vulkan, while the actual
default condition permits the Windows Vulkan backend. Treat this as a reason to test the real
build and renderer, not permission to infer support from the comment or build alone.

## Host probe contract

P5a loads libEGL/libGLESv2 from a controlled private DLL directory, selects Vulkan or D3D11
explicitly, create a Win32 EGL window surface/context and verify deterministic shader pixels.
The report must include the requested/actual backend, GL/EGL versions, renderer, extensions,
device/driver identity and translated/software status. A missing requested backend is a failure,
never a silent switch. Vulkan and D3D11 run as separate short processes.

The minimum negative case makes the explicitly selected Vulkan path unavailable and must not
succeed through D3D11. Host rendering never proves Java GLES, guest GLES, guest Vulkan or G4.
Resize/context/surface lifetime coverage grows with the corresponding implemented callers,
not empty speculative interfaces.

The optional `build.ps1 -Component AngleProbe` and `test.ps1 -Suite GraphicsHost` implement this
host increment: two positive processes and one Vulkan-driver-unavailable negative process.
The negative uses child-only loader environment overrides, never a system ICD change.
Tests strip the child PATH to Windows directories, check DLL hashes/machine types and assert
two pixels before and after a real window resize; successful normal EGL teardown is required.
`bin/angle.json` records the ANGLE recipe/patch and DLL hashes; `bin/angle-probe.json` records
the host fixture's source hash, compiler/configuration, executable hash and expected output.

`GraphicsHost`: **6 PASS, 2.38 s** on Windows ARM64 (`P5-GraphicsHost.log`). Both positive
processes drew and read exact RGBA `64,128,191,255` at two points before and after the client
area changed from `304x201` to `400x265`; both exited normally after teardown. The negative
process exited 2 with explicit Vulkan unavailable, without switching to D3D11. The host probe
does not test `auto` or packaging.

## Java GLES increment

The Windows Java GLES path uses private ARM64 ANGLE (`bin\angle\libEGL.dll`, `libGLESv2.dll`,
`d3dcompiler_47.dll`) with an atomic on-disk `EGL_ANDROID_blob_cache` (`angle_runtime.cpp`) and the
Direct3D 11 multi-layer hardware compositor (`compositor.cpp`, ADR-0005). `--graphics=off` is the
CLI default; `vulkan` and `d3d11` request those backends explicitly without fallback; `auto` prefers
`vulkan` when a native GPU-vendor Vulkan ICD is present and prefers hardware `d3d11` (GLES 3.0) when
host Vulkan is translated (such as Mesa Dozen).

Build `Angle` before `Host` or `All` to stage the ARM64 DLLs (`stage-app-resources.ps1` and
`package.ps1` bundle them automatically). `winapk doctor` probes `bin\angle` headlessly via 1x1
Pbuffers and reports `angleBackends` with each backend's actual GLES ceiling (`d3d11` ES 3.0,
`vulkan` ES 2.0 on Dozen). `graphics.cpp` binds all upstream `GLES10`, `GLES11`, `GLES20`, `GLUtils`,
`GLES30`, `GLES31`, `GLES31Ext`, `GLES32`, `EGL14`, `EGL15` and `EGLExt` JNI methods dynamically to
private ARM64 ANGLE, supporting ES 1.1/2.0/3.x `GLSurfaceView`s as well as off-thread `EGL14` /
`MacApkEGL10` / `SurfaceTexture` (`MacApkGlWindow`) producers. `test.ps1 -Suite GraphicsJava` runs
`hello-gles.apk` and `hello-gles3.apk` across 8 checks: explicit Vulkan through Dozen, `auto`
selecting hardware D3D11 on a translated-Vulkan host, `auto` falling back to D3D11 when Vulkan is
hidden, explicit Vulkan refusal, GLES 3.0 `#version 300 es` VAO/UBO/`glTexStorage2D`/FBO-blit +
worker-thread `EGL14` `SurfaceTexture` swap on D3D11, and explicit ES 3.0 refusal on Dozen's ES 2.0
Vulkan backend.

## Managed Android native window and multi-layer compositing

`surface.cpp` (`libandroid.so`) provides `ANativeWindow_fromSurface`, `acquire`, `release`,
`getWidth`, `getHeight`, `getFormat`, `setBuffersGeometry`, `lock` and `unlockAndPost` across both
root `SurfaceView`s and child `SurfaceView`s nested inside a `ViewGroup`. Acquiring a `SurfaceView`
notifies `Surface.macapkNoteNativeWindowConsumer` so `SurfaceView.draw` punches a transparent hole
in `ViewCanvasDriver`, and `compositor.cpp` composites `[below-window SurfaceViews] → [UI layer with
straight-alpha cutouts] → [zOrderOnTop SurfaceViews and WindowManager overlays]` on hardware D3D11
with a 3-slot asynchronous `D3D11_USAGE_STAGING` readback ring for `--embed` panes.

`test.ps1 -Suite Surface`: **4 PASS, 3.48 s** on Windows ARM64. The normally linked NDK API-30
fixture declares `DT_NEEDED libandroid.so`. The same reference tracks `360x720` to `424x752`;
the test checks unrelated/null objects, Java release, old attachment invalidation, fresh
identity and spent/unknown tokens in one APK process with CheckJNI.

## Guest EGL/GLES profile

With `--native=restricted` and an enabled `--graphics` backend, logical `libEGL.so`,
`libGLESv1_CM.so`, `libGLESv2.so` and `libGLESv3.so` (`guest_graphics.cpp`,
`guest_gles_autogen.inc`) expose EGL 1.5 (`EGL_KHR_create_context`,
`EGL_KHR_surfaceless_context`, `EGL_KHR_image_base`, `EGL_KHR_gl_texture_2D_image`,
`EGL_KHR_gl_renderbuffer_image`, `EGL_KHR_fence_sync`, `EGL_KHR_wait_sync`) and 505 GLES
1.0/1.1/2.0/3.0/3.1/3.2 + OES/EXT/KHR entry points. Guest threads execute GLES commands directly on
the calling thread against ANGLE without serializing onto the Win32 message thread, supporting
multiple `EGLContext`s, `share_context` across render and IO worker `pthread`s,
`eglCreatePbufferSurface`, surfaceless `eglMakeCurrent` and `EGLImageKHR` texture handoff.

Guest window surfaces (`ANativeWindow*`) are backed by dynamically resized ANGLE Pbuffers and
composited in Z-order by `WinApkCompositor` (zero-copy `EGL_D3D_TEXTURE_2D_SHARE_HANDLE_ANGLE` on
D3D11, FBO-0 RGBA8 transfer on Vulkan), working identically in standalone windows and `--embed`
panes. Host WSI extensions (`eglCreatePlatformWindowSurface`) and unknown extensions remain null.
Minimized (`0x0`) and retired surfaces refuse `eglSwapBuffers` with `EGL_BAD_SURFACE`.

`test.ps1 -Suite GraphicsGuest` runs an API-30 NDK APK (`DT_NEEDED` `libandroid.so`, `libEGL.so`,
`libGLESv2.so`, `libGLESv3.so`) against explicit hardware D3D11, explicit Vulkan/Dozen, and a child
with Vulkan hidden. It verifies a background `pthread_create` IO worker uploading an RGBA8 texture
through a `share_context` + Pbuffer + surfaceless binding + `EGLImageKHR` /
`glEGLImageTargetTexture2DOES` consumed by the main context, GLES 3.0 (`#version 300 es` / VAO) on
D3D11 and honest ES 3.0 `EGL_BAD_MATCH` refusal on Dozen's ES 2.0 backend, blended triangle
readback, minimize/restore, resize (`360x720` → `424x752`), surface replacement, and explicit
unavailable refusal (`EGL_NOT_INITIALIZED`).

## Guest Vulkan 1.0–1.3 and WSI (`guest_vulkan.cpp`)

`guest_vulkan.cpp` and `guest_vulkan_autogen.inc` (generated from locked `Vulkan-Headers` 1.4.363)
expose 290+ Vulkan 1.0–1.3 core, promoted and KHR/EXT instance, physical-device, device, queue and
command-buffer entry points across multiple `VkInstance`s, `VkDevice`s and `VkQueue`s via System32's
`vulkan-1.dll`. Under ADR-0005, guest Vulkan is enabled when `--graphics=vulkan`, when
`--graphics=auto` runs on a native GPU-vendor Vulkan ICD (`HasNativeVendorVulkan() == true`), or
when `--graphics=auto` runs an APK whose manifest requires `android.hardware.vulkan.*`.

Host-only Win32 surface/interop extensions (`VK_KHR_win32_surface`, `VK_KHR_external_*_win32`,
`VK_KHR_win32_keyed_mutex`), Android `AHardwareBuffer` extensions (`VK_ANDROID_*`), and POSIX `fd`
external memory/sync extensions (`VK_KHR_external_*_fd`) are filtered out and refused with
`VK_ERROR_EXTENSION_NOT_PRESENT` (both in `ppEnabledExtensionNames` and `pNext`). Standard Vulkan
1.1–1.3 `pNext` chains (`VkPhysicalDeviceProperties2`, `VkPhysicalDeviceDriverProperties`,
`VkPhysicalDeviceFeatures2`, dynamic rendering, synchronization2, timeline semaphores, descriptor
update templates) pass directly.

### Swapchain WSI and `--embed` compositing

`vkCreateAndroidSurfaceKHR` translates managed `ANativeWindow*` tokens to `VkSurfaceKHR`, ordering
non-CPU physical devices ahead of software adapters. On `--embed` panes, swapchain presents
composite through `WinApkCompositor` — preferring zero-copy `VK_KHR_external_memory_win32` DXGI
shared textures when supported by a native vendor ICD and falling back automatically to a
host-coherent `VkBuffer` `vkCmdCopyImageToBuffer` transfer readback on translated ICDs (Mesa Dozen).
Minimized (`0x0`) and resized surfaces latch `VK_ERROR_OUT_OF_DATE_KHR` until swapchain recreation;
retired surfaces return `VK_ERROR_SURFACE_LOST_KHR`. This single-machine Dozen verification does not
stand in for native vendor Vulkan ICD acceptance.

### Transfer buffer readback and compute note

`hello-vulkan-wsi.apk` verifies coherent host-visible buffer mapping, GPU `vkCmdFillBuffer` +
transfer-to-host barrier readback, and explicit refusal of `VK_ANDROID_*` and `VK_KHR_external_*_fd`
`pNext` chains. On Adreno 8cx Gen 3 (`HighestShaderModel=0x62`, `WaveLaneMin=64..128`), Mesa Dozen's
compute shader compiler emits Shader Model 6.6+ subgroup ops; compute pipelines remain bound by the
host driver's shader model.

### Graphics pipeline, descriptors, pipeline cache and guest callbacks

Within `hello-vulkan-wsi.apk` (`test.ps1 -Suite VulkanWsi` + `TestRealRuntimeEmbed`), `libvulkan.so`
verifies:

- **Offscreen `VkImage`, descriptors, sampler, push constants and `VkPipelineCache`:** `4x4`
  `VK_FORMAT_R8G8B8A8_UNORM` optimal image, `VkSampler`, `VkDescriptorSetLayout` + `VkDescriptorPool`
  + `VkDescriptorSet` (`vkUpdateDescriptorSets` UBO), `VkPushConstantRange` (`vkCmdPushConstants`),
  `VkPipelineCache` (`vkGetPipelineCacheData`), SPIR-V graphics pipeline draw and image-to-buffer
  readback asserting all 16 words equal `0xFFCC8822`, plus `--embed` pane WSI pixel readback.
- **CFG-exempt guest callbacks (`__declspec(guard(nocf))`):** heap-interned `VkAllocationCallbacks`
  thunks (verifying `NULL` refusal as `VK_ERROR_OUT_OF_HOST_MEMORY` and balanced `allocs == frees`),
  dual host/bridge `VK_EXT_debug_report` and `VK_EXT_debug_utils` messengers (with non-executable
  callback pointer refusal), and `VK_EXT_device_memory_report` `pNext` callback screening.

## P8 Skia / AGSL / RenderEffect over Direct3D 12 (`T15`)

`NativeSkiaD3D12` (`macapk/runtime/platform/windows/src/bridge/skia_d3d12.cpp`) creates an
independent hardware `IDXGIAdapter1`, `ID3D12Device` and direct `ID3D12CommandQueue` (refusing
software WARP adapters by default) and binds Skiko `0.150.2` (`skiko-windows-arm64.dll`, `ARM64`
PE) via `DirectContextKt._nMakeDirect3D`. `MacApkSkia` retains the Java2D Canvas + GPU shader /
filter + explicit straight-alpha readback architecture (`PLAN.md` §15), reporting
`Direct3D12: Qualcomm(R) Adreno(TM) 8cx Gen 3` and passing all 11 pixel contracts of
`AgslSmokeWindows` (`T15`, `test.ps1 -Suite Agsl`). In addition, `NativeSkiaD3D12.presentSharedLayer`
and `CompositorSubmitSurfaceSharedNtHandle` (`compositor.cpp`, GPU-P6) connect Skia's Direct3D 12
surfaces to `WinApkCompositor` across a DXGI NT shared handle (`D3D12_HEAP_FLAG_SHARED` +
`ID3D12Device::CreateSharedHandle` → `ID3D11Device1::OpenSharedResource1`), verified end-to-end in
`AgslSmokeWindows`, while `NativeCocoa.presentFrame` (`window.cpp`) diffs dirty bounds row-by-row,
elides unchanged frames (`PRESENT_ELIDED_IDENTICAL`), and uploads only the changed sub-box to D3D11.
`TestRealRuntimeEmbed` and `test.ps1 -Suite Stability` (`T14.gpu.d3d11`, `T14.gpu.vulkan`) verify
live pane ↔ standalone `Reattach`, portrait ↔ landscape `Resize`, minimize/restore cycles and flat
post-warmup commit/handle counts across both `d3d11` and `vulkan` backends. When
`skiko-windows-arm64.dll` or Direct3D 12 is unavailable (`WINAPK_DISABLE_SKIA=1`),
`MacApkSkia.ensureLoaded()` fails explicitly with `GRAPHICS_CAPABILITY_MISSING` and never falls back
silently to unfiltered CPU rendering.
