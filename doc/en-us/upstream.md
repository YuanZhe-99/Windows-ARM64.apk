# Upstream contracts

What Windows-ARM64.apk relies on from its upstreams, and how far each point has been checked.
Mac.apk, Go and MyGo follow their latest version, so every point is re-checked when that upstream
moves. The Mac.apk points below hold at `3f35611` (merged 2026-11-16); "read" means read in the source,
"built" or "verified" that P0 built or ran it on Windows 11 ARM64 ([port-audit.md](port-audit.md)).

## Mac.apk

| Point | Consequence | Checked |
|---|---|---|
| The Framework and the DEX translator are Java (`runtime/java/`, `runtime/java/com/macapk/dex/`) | kept as they are; only platform calls and path assumptions are isolated | built: compiles unchanged on the Windows ARM64 JDK |
| `Boot.run(apk, classesJar, appDir, manifest)` is the start-up entry; the calling thread runs `NativeCocoa.runLoop()` and the Activity runs on its own thread; the app's data dir is named after `appDir`'s folder; files in `appDir` that are not its own (`Boot.isCodeDirArtifact`, or a `.macapk` prefix) are moved to the data dir | `winapk run` names `appDir` after the package and keeps its cache stamp as `.macapk-winapk-stamp` | verified (G1) |
| `NativeCocoa.java` carries the window, Surface, presentation and host-service JNI methods; frames are pulled by calling the renderer's `macapkDrawFrame()` on the UI thread, which presents ARGB pixels with `presentBitmap`; touches go to `TouchDispatcher.dispatch` in points with y from the bottom | Windows binds the same declarations to Win32 code (ADR-0002) | verified (G1) |
| `AppClassLoader.findLibrary` translates APK libraries and falls back to a no-op stub | with guest native off, Windows throws `GUEST_NATIVE_DISABLED` first (a platform-conditional check) | verified (G1.5) |
| `MACAPK_FW_RES_APK` names the platform resource package; `-Dmacapk.android.root` the Android root; `-Dmacapk.display.density` the density | set by `winapk run` | verified (G1) |
| `MACAPK_BUILD_JAVA_ID` (from `build-id.txt`, exported by `main.cpp`) keys `dalvik.system.MacApkDexConverter`'s cache; without it that cache does not persist | exported by `winapk run` (ADR-0003) | set (G1); the dynamic-DEX path is not exercised by a fixture yet |
| `ActivityHost.macapkTaskToFront()` brings back a task the runtime sent back; a no-op otherwise | called when the window is restored from the taskbar | verified (G1.3) |
| `MACAPK_INJECT_FILE` starts `TouchDispatcher`'s input driver, which reads `key`, `tap` and other lines from that file | used by `test.ps1` to send Back while the UI thread is suspended | verified (G1.3) |
| `runtime/src/native_sqlite.cpp` is portable C++ apart from CoreFoundation case mapping and collation | built on Windows with a `_WIN32` branch for those | verified (T03 SQLite) |
| `runtime/src/jvm_loader.cpp` embeds the JVM with macOS loading assumptions | a Windows `jvm.dll` loader with its own options is needed | verified: the Windows loader is in `platform/windows`, and the JDK accepts upstream's module options (T01) |
| `runtime/CMakeLists.txt` enables OBJCXX at the top level and finds Apple and Homebrew dependencies | languages, sources, links and discovery become platform-conditional | built: an `if(WIN32)` branch includes `platform/windows/windows.cmake` before the macOS project |
| `runtime/translator/src/arm64_native.cpp` interleaves ELF mapping and relocation with macOS memory, register and patching | the N0 loader reuses only the ELF record layouts (`elf64_reader.h`); code that uses `x18`, `TPIDR_EL0` or `svc` is refused rather than patched | read; layouts built (G2) |
| `runtime/scripts/build.sh` puts `thirdparty/apache-http-legacy/org.apache.http.legacy.jvm.jar` on the framework's classpath and unpacks it into `android-framework.jar`, never over a compiled class | `build.ps1 -Component Java` does the same | built (`3f35611`) |
| `main.cpp` names an app's folders `<package>_<version>` with the version's dots folded (`dir_naming.h`) | `winapk run` names them after the package alone, so no version dot appears in an app's paths | read |
| `arm64_pack.cpp` and `arm64_macho_emit.cpp` wrap guest code as Mach-O | Windows uses one host PE bridge and a virtual guest library registry instead | read |
| `bionic_shim.cpp` ties Bionic, syscalls and threads to Darwin mechanisms | split by capability, never ported file-wide | read |
| `bionic_shim_vulkan.cpp` adapts guest Vulkan to MoltenVK and Metal surfaces | its extension handling is reused; a Win32 backend is added | read |
| `angle_glue.cpp` and `gles_bridge.cpp` are the GLES/ANGLE bridge entries | host surface, backend choice and guest ABI are separated | read |
| `NativeLoaderShim.java` relies on host dynamic libraries tied to a ClassLoader; an app's own `System.load(path)` reaches the JVM directly | renaming `.dylib` to `.dll` is not enough; guest libraries go through a guest loader, and `System.load` of an APK library must be routed there too (open, [port-audit.md](port-audit.md)) | read |
| Basic Canvas is Java2D; AGSL and RenderEffect run on Skia's Metal backend with readback (`BUILDING.md`) | Canvas, GLES, Vulkan and AGSL are separate work items, not one Metal replacement | read |
| The build needs macOS 26.4 for the `x18` ABI entitlement and commits macOS ANGLE, MoltenVK and Skia `.dylib` prebuilts | none of that applies or ships on Windows; dependencies are built or obtained for ARM64 Windows | read |
| The launcher is Swift (`launcher/`) and the build and release scripts are bash (`scripts/`, `runtime/scripts/`) | the MyGo front end replaces the launcher on Windows; the scripts' steps are re-expressed, not renamed | read |
| The code is AGPL-3.0 or commercial; the name and logo are reserved (`COMMERCIAL.md`, `TRADEMARKS.md`) | see [third-party.md](third-party.md) | read |

## Windows ARM64 ABI

| Point | Consequence | Checked |
|---|---|---|
| `x18` holds the TEB and `TPIDR_EL0` is reserved | guest TLS and shadow-call-stack schemes never use them; no swap on entry | verified: x18 is the TEB in every guest callback (T05 `host.state`) |
| Fixed-signature calls follow AAPCS64 (registers, stack slots, HFAs, `x8`, callee-saved registers); variadic calls pass floating point in integer registers | fixed signatures call directly, variadic ones never cross ([abi-compatibility.md](abi-compatibility.md)) | verified for fixed signatures (T05); the variadic difference read |
| The data model is LLP64: `long` is 32-bit | guest `long` uses its own 64-bit type across the boundary | read |
| Unwinding uses its own `.pdata`/`.xdata` metadata and SEH | guest DWARF unwinding and host thunks are separate problems | read |

| The JVM finds `Java_*` natives only in libraries loaded by `System.loadLibrary`, not in the program embedding it | host natives are registered with `RegisterNatives` ([decisions.md](decisions.md)) | verified (T01) |
| On this PC (Adreno 8cx Gen 3) the Vulkan loader offers only Mesa Dozen, Vulkan 1.2 translated to Direct3D 12 | `doctor` reports `VULKAN_DRIVER_TRANSLATED`; graphics gates need a vendor driver or a recorded decision | verified (`doctor`) |

## Android NDK

| Point | Consequence | Checked |
|---|---|---|
| `x18` is reserved for the platform on Android ARM64 | each binary is still checked; absence in one binary proves nothing general | verified for T05's builds: the screen finds no `x18` in them |
| NDK r30 links with a 16 KiB maximum page size and, at API 30, packs relative relocations as `DT_ANDROID_RELR` | segments share a 4 KiB host page only with the same permissions; RELR is applied | verified (T05 builds) |
| ELF TLS has several access models and TLSDESC (AOSP `elf-tls.md`) | each model is supported or refused explicitly | read |

## MyGo

| Point | Consequence | Checked |
|---|---|---|
| MyGo's distribution documents `windows/arm64` | the front end can be native ARM64 | read; the Go module builds and tests on Windows 11 ARM64 |
| MyGo is a Go tool dependency (`go tool mygo …`) | its version moves with `go.mod` | built |
