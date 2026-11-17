# Graphics capabilities

P5/P6 are in progress after G3. No graphics gate has passed yet. The Java bitmap path remains
independent; enabling guest native does not enable GLES or guest Vulkan.

## Verification paths

| Path | State | Required evidence |
|---|---|---|
| Host ANGLE/D3D11 | PASS | Adreno 8cx Gen 3 hardware, driver `30.0.4122.4000`, EGL 1.5 / GLES 3.0; draw/readback, resize and teardown |
| Host ANGLE/Vulkan on Dozen | PASS; user-approved experiment (ADR-0004) | same Adreno GPU via D3D12, Mesa `26.3.0-devel (git-e3fc7749c6)`, Vulkan `1.2.359`, EGL 1.5 / GLES 2.0; `translated=yes software=no` |
| Java GLES into ANGLE | NOT_RUN | real APK GLSurfaceView values/lifecycle, not a host probe |
| Guest GLES/N2 | NOT_RUN | managed Surface/ANativeWindow, audited EGL/GLES calls and function pointers, positive/negative APK contracts |
| Guest Vulkan/N2 | NOT_RUN | separate Android/Win32 WSI and dispatch/handle/callback contracts, not ANGLE's private device |
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
process exited 2 with explicit Vulkan unavailable, without switching to D3D11. Product
`auto` selection and packaging are not covered.
