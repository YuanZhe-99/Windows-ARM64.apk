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
| Guest Vulkan/N2 presentation | NOT_RUN | device/queue dispatch, swapchain drawing, callbacks and synchronization |
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

The Windows Java GLES experiment uses one full-client `GLSurfaceView`, a host-owned EGL context
and explicitly bound GLES20 methods. It does not expose EGL handles to Java or guest code.
`--graphics=off` is the default; the Java bitmap path does not load ANGLE. `vulkan` and `d3d11`
request those backends explicitly; `auto` may try D3D11 after Vulkan initialization fails.
This increment starts with viewport, scissor, clear, queries and direct RGBA8 readback. Other
GLES methods remain unbound; GL10 calls and custom EGL configuration are refused. Mixed
composition, app-owned EGL and automatic recovery from context loss are outside the profile.
The current context is scoped to the Win32 render callback, never the Activity/UI caller's
Java thread. Resize renews the drawable while retaining the context; minimized windows stop
GLES draws and request a new frame on restore.

Build `Angle` before `Host` or `All` to provide the pinned headers. ANGLE DLLs still load only
from the bridge's private `angle` directory when a `GLSurfaceView` uses the requested backend.
`test.ps1 -Suite GraphicsJava` runs the real `hello-gles.apk` under CheckJNI: explicit Vulkan
through Dozen, `auto` falling back to D3D11 when the child cannot see Vulkan, and explicit
Vulkan refusal with the same ICD restriction. The APK asserts two different RGBA colors with
a positioned direct buffer, read-only and off-context refusal, and renderer callbacks.
The suite passed 4 checks on the same Adreno hardware. The client changed from `360x720`
to `424x752`; the D3D11 case also minimized and restored at `424x752`, retaining the same
context. The explicit unavailable case exited 1 before any draw. `doctor` only inventories
ARM64 DLL files; its `angleBackends` list stays empty because it does not render.
Shaders, textures, blending and FBOs are not yet verified through Java GLES; the host probe's
shader test does not fill that gap.

## Managed Android native window

The restricted `libandroid.so` provider admits only `ANativeWindow_fromSurface`, acquire,
release and width/height. It matches the actual `Surface` owned by the attached plain
`SurfaceView`'s holder, not a Java-supplied address. An opaque guest token owns references to that
attachment. A resize updates its client-pixel extent. Replacing the
content or destroying the window retires the attachment, even while guest references remain.
Size queries on stale or unknown tokens return a negative error; acquire/release never
dereference such tokens. Releasing the Java `Surface` refuses new conversions but does not
cancel native references already acquired to a still-live attachment.

The endpoint does not give guest code an `HWND` or driver function pointer. It does not implement Android buffer
queues, `ANativeWindow_lock`, or a general `SurfaceHolder.surfaceDestroyed` dispatcher.
The bridge publishes a zero client extent on minimize, but minimized-size queries are not
part of this fixture.

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
same time. Guest Vulkan will use separate dispatch and ownership.

The logical `libEGL.so` and `libGLESv2.so` expose a fixed set of ES2 and window EGL
commands only. `eglGetProcAddress` returns a bridge entry from the same table or null.
No extensions are advertised; ES3 contexts, pbuffer/surfaceless contexts, multiple
displays, custom native displays and shared contexts are unsupported. The reported 1.4/ES2
version strings identify the profile; they are not conformance claims. Calls require a
running window message loop, and display ownership cannot transfer between guest threads.
Context loss is returned as an error; a real loss, minimize/restore on guest EGL and
long-running stability have not been exercised.

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

This increment does not expose logical devices, swapchains, command buffers or callbacks.
Neither support queries nor WSI creation count as presenting a frame or passing G4.

`test.ps1 -Suite VulkanWsi` launches one normal NDK API-30 APK twice: once with the system
ICD available and once with the child unable to discover it. It verifies extension/proc
scopes and rejects AHardwareBuffer, allocation callbacks, nonempty `pNext` and stale handles.
The window changes from `360x720` to `424x752`; replacing the view makes the old surface
report `VK_ERROR_SURFACE_LOST_KHR`. No driver is installed or copied.
