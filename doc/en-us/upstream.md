# Upstream contracts

What Windows-ARM64.apk relies on from its upstreams, and how far each point has been checked.
Mac.apk, Go and MyGo follow their latest version, so every point is re-checked when that upstream
moves. The Mac.apk points below hold at `ff2afbf` (merged 2026-11-03); "read" means read in the source,
"built" or "verified" that P0 built or ran it on Windows 11 ARM64 ([port-audit.md](port-audit.md)).

## Mac.apk

| Point | Consequence | Checked |
|---|---|---|
| The Framework and the DEX translator are Java (`runtime/java/`, `runtime/java/com/macapk/dex/`) | kept as they are; only platform calls and path assumptions are isolated | built: compiles unchanged on the Windows ARM64 JDK |
| `Boot.java` couples start-up to `NativeCocoa`, native libraries and macOS paths | a host start-up service is split out; the app lifecycle stays | read |
| `NativeCocoa.java` carries the window, Surface, presentation and host-service JNI methods | a platform-neutral entry is added and calls move to it step by step | read |
| `runtime/src/jvm_loader.cpp` embeds the JVM with macOS loading assumptions | a Windows `jvm.dll` loader with its own options is needed | verified: the Windows loader is in `platform/windows`, and the JDK accepts upstream's module options (T01) |
| `runtime/CMakeLists.txt` enables OBJCXX at the top level and finds Apple and Homebrew dependencies | languages, sources, links and discovery become platform-conditional | built: an `if(WIN32)` branch includes `platform/windows/windows.cmake` before the macOS project |
| `runtime/translator/src/arm64_native.cpp` interleaves ELF mapping and relocation with macOS memory, register and patching | the parsing is reused; host memory and execution policy are separated | read |
| `arm64_pack.cpp` and `arm64_macho_emit.cpp` wrap guest code as Mach-O | Windows uses one host PE bridge and a virtual guest library registry instead | read |
| `bionic_shim.cpp` ties Bionic, syscalls and threads to Darwin mechanisms | split by capability, never ported file-wide | read |
| `bionic_shim_vulkan.cpp` adapts guest Vulkan to MoltenVK and Metal surfaces | its extension handling is reused; a Win32 backend is added | read |
| `angle_glue.cpp` and `gles_bridge.cpp` are the GLES/ANGLE bridge entries | host surface, backend choice and guest ABI are separated | read |
| `NativeLoaderShim.java` relies on host dynamic libraries tied to a ClassLoader | renaming `.dylib` to `.dll` is not enough; guest libraries go through a guest loader | read |
| Basic Canvas is Java2D; AGSL and RenderEffect run on Skia's Metal backend with readback (`BUILDING.md`) | Canvas, GLES, Vulkan and AGSL are separate work items, not one Metal replacement | read |
| The build needs macOS 26.4 for the `x18` ABI entitlement and commits macOS ANGLE, MoltenVK and Skia `.dylib` prebuilts | none of that applies or ships on Windows; dependencies are built or obtained for ARM64 Windows | read |
| The launcher is Swift (`launcher/`) and the build and release scripts are bash (`scripts/`, `runtime/scripts/`) | the MyGo front end replaces the launcher on Windows; the scripts' steps are re-expressed, not renamed | read |
| The code is AGPL-3.0 or commercial; the name and logo are reserved (`COMMERCIAL.md`, `TRADEMARKS.md`) | see [third-party.md](third-party.md) | read |

## Windows ARM64 ABI

| Point | Consequence | Checked |
|---|---|---|
| `x18` holds the TEB and `TPIDR_EL0` is reserved | guest TLS and shadow-call-stack schemes never use them; no swap on entry | read |
| The data model is LLP64: `long` is 32-bit | guest `long` uses its own 64-bit type across the boundary | read |
| Unwinding uses its own `.pdata`/`.xdata` metadata and SEH | guest DWARF unwinding and host thunks are separate problems | read |

| The JVM finds `Java_*` natives only in libraries loaded by `System.loadLibrary`, not in the program embedding it | host natives are registered with `RegisterNatives` ([decisions.md](decisions.md)) | verified (T01) |
| On this PC (Adreno 8cx Gen 3) the Vulkan loader offers only Mesa Dozen, Vulkan 1.2 translated to Direct3D 12 | `doctor` reports `VULKAN_DRIVER_TRANSLATED`; graphics gates need a vendor driver or a recorded decision | verified (`doctor`) |

## Android NDK

| Point | Consequence | Checked |
|---|---|---|
| `x18` is reserved for the platform on Android ARM64 | each binary is still checked; absence in one binary proves nothing general | read |
| ELF TLS has several access models and TLSDESC (AOSP `elf-tls.md`) | each model is supported or refused explicitly | read |

## MyGo

| Point | Consequence | Checked |
|---|---|---|
| MyGo's distribution documents `windows/arm64` | the front end can be native ARM64 | read; the Go module builds and tests on Windows 11 ARM64 |
| MyGo is a Go tool dependency (`go tool mygo …`) | its version moves with `go.mod` | built |
