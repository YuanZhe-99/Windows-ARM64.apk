# Graphics capabilities

P5/P6 are in progress after G3. No graphics gate has passed yet. The Java bitmap path remains
independent; enabling guest native does not enable GLES or guest Vulkan.

## Verification paths

| Path | State | Required evidence |
|---|---|---|
| Host ANGLE/D3D11 | PASS | Adreno 8cx Gen 3 hardware, driver `30.0.4122.4000`, EGL 1.5 / GLES 3.0; draw/readback, resize and teardown |
| Host ANGLE/Vulkan on Dozen | PASS; user-approved experiment (ADR-0004) | same Adreno GPU via D3D12, Mesa `26.3.0-devel (git-e3fc7749c6)`, Vulkan `1.2.359`, EGL 1.5 / GLES 2.0; `translated=yes software=no` |
| Java GLES into ANGLE | PASS for the bounded GLES20 profile below | real APK `GLSurfaceView` colors, resize, restore, teardown, and explicit refusal |
| Managed native window | PASS for the reference profile below | real restricted APK, client extent, replacement and refused references; no drawing |
| Guest GLES/N2 | PASS for the bounded T11 profile | normal NDK APK; texture/FBO, blended shader, resize, surface replacement, and explicit Vulkan refusal |
| Guest Vulkan instance/WSI | PASS for T13.wsi on Dozen | real APK, Android→Win32 surface, actual support/capabilities, resize, replacement and refusal; no drawing |
| Guest Vulkan transfer-clear | PASS for the bounded T13 profile on Dozen | real guest device/queue, command buffer and swapchain; three presents across resize |
| Guest Vulkan transfer buffer | PASS for the bounded T13 profile on Dozen | coherent allocation and GPU fill, exact 16-word readback and lifecycle refusals |
| Guest Vulkan graphics pipeline, image and callbacks | PASS for the bounded T13 profile on Dozen | SPIR-V vertex/fragment `VkGraphicsPipelineCreateInfo`, `4x4` `VkImage` render-pass draw and image-to-buffer readback (`0xFFCC8822`), plus guest `VkAllocationCallbacks` and `VK_EXT_debug_report` thunks |
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
`vulkan` ES 2.0 on Dozen). `test.ps1 -Suite GraphicsJava` runs the real `hello-gles.apk` under
CheckJNI across 5 checks: explicit Vulkan through Dozen, `auto` selecting hardware D3D11 on a
translated-Vulkan host, `auto` falling back to D3D11 when Vulkan is hidden, and explicit Vulkan
refusal.

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

With `--native=restricted` and an enabled `--graphics` backend, a plain `SurfaceView` can
back one guest EGL display/config/context/window surface. These are bridge tokens; their
driver handles stay on the Win32 message thread. The guest thread owns the logical binding,
and commands run synchronously on the message thread without holding a registry lock.
A resized attachment renews the native drawable, not the guest context; an invalidated
attachment cannot swap or be bound again. Guest EGL and Java EGL cannot own ANGLE at the
same time. Guest Vulkan uses separate dispatch and ownership.

The logical `libEGL.so` and `libGLESv2.so` expose a fixed set of ES2 and window EGL
commands only. `eglGetProcAddress` returns a bridge entry from the same table or null.
No extensions are advertised; ES3 contexts, pbuffer/surfaceless contexts, multiple
displays, custom native displays and shared contexts are unsupported. The reported 1.4/ES2
version strings identify the profile; they are not conformance claims. Calls require a
running window message loop, and display ownership cannot transfer between guest threads.
At a minimized, zero-size managed surface, the bridge refuses swap with `EGL_BAD_SURFACE`.
On restore, the fixture draws and reads back with the same guest context, surface and program
before exercising resize and replacement. This is not real context loss or a stability run.
T11 keeps its original three process launches. The first extended run observed one Vulkan
swap failure after resize/replacement; the reruns passed. Driver swap errors now log the EGL
error code, rather than losing it behind a numeric fixture step.

`test.ps1 -Suite GraphicsGuest` runs one API-30 APK against explicit hardware D3D11 and
Vulkan/Dozen, plus one launch with the Vulkan ICD hidden only from the child. It checks
exact RGBA8 texture upload via an FBO, blended triangle readback (1–2 bytes of tolerance), retention of
the same context on resize and surface replacement, an extension-free view, proc identity,
and invalid native window, ES3, stale/current surface and unavailable backend errors.
The app sees `EGL_NOT_INITIALIZED` when explicit Vulkan fails; it can close normally.

## Guest Vulkan instance and WSI increment

The restricted `libvulkan.so` profile uses only the System32 Vulkan loader when
`--native=restricted --graphics=vulkan` is selected. It filters the instance extensions,
translates Android surface creation to Win32, keeps instance, physical-device and surface
handles private, and checks actual presentation support and capabilities. The message
thread owns surface creation, queries and teardown. ANGLE does not participate.

The WSI checks alone do not count as presenting a frame or passing G4.

`test.ps1 -Suite VulkanWsi` launches one normal NDK API-30 APK twice: once with the system
ICD available and once with the child unable to discover it. It verifies extension/proc
scopes and rejects AHardwareBuffer, allocation callbacks, nonempty `pNext` and stale handles.
The window changes from `360x720` to `424x752`; replacing the view makes the old surface
report `VK_ERROR_SURFACE_LOST_KHR`. No driver is installed or copied.

### Swapchain transfer-clear profile

One logical device and present queue, one FIFO swapchain and one primary command buffer
form a separate, bounded profile. The guest records image transitions and a color clear
against a token for the acquired image, submits on the owned queue and presents. The
driver’s device, queue, swapchain and image handles stay private. The profile does not
implement shader pipelines, general images, custom allocators or debug callbacks.

The `VulkanWsi` suite checks three transfer clears/presents in one positive process:
at launch, after restoring the minimized window, and after resizing it. Acquire on a
minimized zero-size surface returns `VK_ERROR_OUT_OF_DATE_KHR`; the old swapchain stays
stale after restore at the original extent. The guest destroys and recreates it on
restore and on resize. The fixture checks actual present return values, not swapchain
pixel readback. This is not vendor Vulkan or G4 acceptance.

### Transfer buffer readback

The same APK process also exercises one bounded transfer-destination buffer and one
host-visible, coherent device-memory allocation. The bridge translates both tokens, records
an actual fill and transfer-to-host barrier, and admits mapping only after the queue
completes. The APK checks every word against a known pattern. This is a transfer test,
not shader execution, general resource support or swapchain pixel readback.

The compute experiment did not advance past pipeline creation because Qualcomm's D3D12 driver on
Adreno 8cx Gen 3 reports `HighestShaderModel=0x62` with `WaveLaneMin=64, WaveLaneMax=128`, whereas
Mesa Dozen's compute compilation emits subgroup requirements targeting Shader Model 6.6+. The
tested bridge does not export compute commands.

### Graphics pipeline, offscreen image and guest callbacks

Within the same single-launch `hello-vulkan-wsi.apk` fixture on Adreno 8cx Gen 3 + Dozen, the
restricted `libvulkan.so` provider also verifies:

- **Offscreen `VkImage` and render pass:** one `4x4` `VK_FORMAT_R8G8B8A8_UNORM` optimal image
  (`COLOR_ATTACHMENT_BIT | TRANSFER_SRC_BIT`), dedicated `VkDeviceMemory` binding, `VkImageView`,
  single-subpass `VkRenderPass` (`CLEAR` to `STORE`, `UNDEFINED` to `TRANSFER_SRC_OPTIMAL`), and
  `VkFramebuffer`.
- **SPIR-V graphics pipeline:** NDK `glslc`-compiled vertex and fragment `VkShaderModule`s, empty
  `VkPipelineLayout`, `vkCreateGraphicsPipelines`, `vkCmdBeginRenderPass`, `vkCmdBindPipeline`,
  `vkCmdDraw`, `vkCmdEndRenderPass` and `vkCmdCopyImageToBuffer` into the coherent readback buffer,
  asserting all 16 words equal the fragment shader's `0xFFCC8822` output.
- **Guest callbacks:** screened `VkAllocationCallbacks` on shader module creation/destruction
  (invoked on the calling guest thread with host ICD heap isolation, verifying `NULL` refusal as
  `VK_ERROR_OUT_OF_HOST_MEMORY` and balanced `allocs == frees`), plus `VK_EXT_debug_report`
  (`vkCreateDebugReportCallbackEXT`, non-executable callback refusal, `vkDebugReportMessageEXT`
  and `vkDestroyDebugReportCallbackEXT`).

## P8 Skia / AGSL / RenderEffect over Direct3D 12 (`T15`)

`NativeSkiaD3D12` (`macapk/runtime/platform/windows/src/bridge/skia_d3d12.cpp`) creates an
independent hardware `IDXGIAdapter1`, `ID3D12Device` and direct `ID3D12CommandQueue` (refusing
software WARP adapters by default) and binds Skiko `0.150.2` (`skiko-windows-arm64.dll`, `ARM64`
PE) via `DirectContextKt._nMakeDirect3D`. `MacApkSkia` retains the Java2D Canvas + GPU shader /
filter + explicit straight-alpha readback architecture (`PLAN.md` §15), reporting
`Direct3D12: Qualcomm(R) Adreno(TM) 8cx Gen 3` and passing all 11 pixel contracts of
`AgslSmokeWindows` (`T15`, `test.ps1 -Suite Agsl`). When `skiko-windows-arm64.dll` or Direct3D 12
is unavailable (`WINAPK_DISABLE_SKIA=1`), `MacApkSkia.ensureLoaded()` fails explicitly with
`GRAPHICS_CAPABILITY_MISSING` and never falls back silently to unfiltered CPU rendering.
