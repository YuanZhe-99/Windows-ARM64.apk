# Upstream contracts

What Windows-ARM64.apk relies on from its upstreams, and how far each point has been checked.
Mac.apk, Go and MyGo follow their latest version, so every point is re-checked when that upstream
moves. The Mac.apk points below hold at `93c0351` (merged 2026-11-27); "read" means read in the source,
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
| Fixed-signature calls follow AAPCS64 (registers, stack slots, HFAs, `x8`, callee-saved registers); Windows variadic calls pass floating point in integer registers | fixed signatures call directly; never share host varargs/va_list, audited optional arguments need an explicit entry ([abi-compatibility.md](abi-compatibility.md)) | verified for fixed signatures (T05), integer-only optional mode (T08 io); the variadic difference read |
| The data model is LLP64: `long` is 32-bit | guest `long` uses its own 64-bit type across the boundary | read |
| Unwinding uses its own `.pdata`/`.xdata` metadata and SEH | guest DWARF unwinding and host thunks are separate problems | read |
| [CFG](https://learn.microsoft.com/en-us/windows/win32/secbp/control-flow-guard) needs compiler/linker instrumentation and valid-target metadata; executable VirtualAlloc/VirtualProtect pages are valid targets by default | PE JNI entries have explicit GFIDs; inspect Guard/CF Instrumented/FID table flags and query actual process policy; screened guest transfer remains explicit, not a sandbox | Debug `/guard:cf` Host/Java/Native/Bionic/Jni/ApkJni PASS; running policy enabled, all 64 PE entries found in the loaded GFID table |
| [Precise unbiased interrupt time](https://learn.microsoft.com/en-us/windows/win32/api/realtimeapiset/nf-realtimeapiset-queryunbiasedinterrupttimeprecise) is in 100-ns units and excludes suspend; its SDK import library is mincore | explicit timespec conversion and relative monotonic deadlines; not wall-clock sleeps | read; T08 io tests units, wall-clock epoch and a minimum sleep deadline; suspend NOT_RUN |
| [WriteFile](https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-writefile) with both OVERLAPPED offsets at `0xFFFFFFFF` targets EOF; a synchronous handle waits and updates its file pointer | append does not seek then write or expose an OVERLAPPED object to the guest | read; T08 io verifies append after a seek to the beginning |
| [AddVectoredExceptionHandler](https://learn.microsoft.com/en-us/windows/win32/api/errhandlingapi/nf-errhandlingapi-addvectoredexceptionhandler) with First=1 precedes existing handlers; unknown faults must continue searching | only registered guest executable PCs are handled; JVM/JIT and all other PCs remain in the normal chain; no HotSpot handler is replaced | T08 host SEH and T09 Java/JIT exceptions, guest trap: PASS; First=0 missed the guest trap with the locked Microsoft JVM, which exited 1 instead |

| The JVM finds `Java_*` natives only in libraries loaded by `System.loadLibrary`, not in the program embedding it | host natives are registered with `RegisterNatives` ([decisions.md](decisions.md)) | verified (T01) |
| On this PC (Adreno 8cx Gen 3) the Vulkan loader offers only Mesa Dozen, Vulkan 1.2 translated to Direct3D 12 | `doctor` reports `VULKAN_DRIVER_TRANSLATED`; graphics gates need a vendor driver or a recorded decision | verified (`doctor`) |

## Android NDK

| Point | Consequence | Checked |
|---|---|---|
| `x18` is reserved for the platform on Android ARM64 | each binary is still checked; absence in one binary proves nothing general | verified for T05's builds: the screen finds no `x18` in them |
| NDK r30 links with a 16 KiB maximum page size and, at API 30, packs relative relocations as `DT_ANDROID_RELR` | segments share a 4 KiB host page only with the same permissions; RELR is applied | verified (T05 builds) |
| ELF TLS has several access models and TLSDESC (AOSP `elf-tls.md`) | each model is supported or refused explicitly | read |
| Bionic LP64 `pthread_t` is a 64-bit `long`, key is signed 32-bit; LLVM emutls uses size/alignment/object/initializer descriptor fields | explicit guest types and a managed resolver, never Windows `long`, CRT pthread objects or hardware thread-pointer swapping | read in NDK r30 headers and compiler-rt `emutls.c`; T08 verifies C emutls initialization/alignment and thread/key semantics |
| NDK LP64 mutex/condition layouts are 40/48 bytes, alignment 4; static kind/clock bits are in `pthread.h`; Bionic non-PI recursion has an 11-bit depth-minus-one counter | guest-sized markers and host-side object tables, not copied host locks; unsupported attributes/clocks fail explicitly | NDK r30 headers and AOSP `pthread_mutex.cpp` / `pthread_cond.cpp` read; T08 `sync` covers the declared subset |
| AArch64 stat is 128 bytes and timespec is two signed 64-bit fields; open/open64 have an optional integer mode | explicit guest layout and three-register optional-mode entry; never pass host stat, long or va_list | NDK r30 `sys/stat.h`, `bits/timespec.h`, `fcntl.h` read; T08 io checks actual declarations, sentinel bounds, values and mode/no-mode calls |
| A normally linked API-30 NDK C shared library needs libc/libdl/liblog and `LIBC` versions; CRT start files also import `__cxa_atexit`, `__cxa_finalize`, `__register_atfork` | declare exact logical providers; validate version requirements; implement real lifecycle records, never empty successful stubs | NDK r30 normal link and llvm-readelf inspected (2026-11-17); T07 N1 runs the recorded source/link, including callbacks |
| [DSO object destruction ABI](https://itanium-cxx-abi.github.io/cxx-abi/abi.html#dso-dtor) registers function/argument/DSO, finalizes in reverse order, selects a DSO or all, and must not call an entry twice | separate guest registry; remove before guest callback; graceful runtime teardown finalizes remaining records while code/heap live | specification read; T07 N1 verifies selection, reentry/once-only and shutdown; not proof of guest C++ exception support |

## JNI

| Point | Consequence | Checked |
|---|---|---|
| [JNI functions](https://docs.oracle.com/en/java/javase/21/docs/specs/jni/functions.html): stable slot indices, per-thread environments and opaque references/IDs; jvalue is a union of JNI primitive/reference types | explicit private tables, fixed-signature wrappers and thread ownership checks; never copy the JVM table | specification read; NDK/host static layout assertions and T09 real-JVM calls, refs and misuse cases |
| FindClass uses the native library's loader and initializes the named class; lookup failure reports NoClassDefFoundError | configured loader via Class.forName with initialization, translating only ClassNotFoundException; use same loader on workers | isolated URLClassLoader, FindClass side effect and missing class in T09 |
| .../V method calls use default C promotions; A calls accept jvalue arrays | guest va_copy/va_arg by recorded method signature, then fixed host A entry | T09 mixed register/stack values, repeated V, constructors and virtual/nonvirtual dispatch |
| JNI native symbol lookup tries the short name, then the long name with encoded argument descriptors; names escape UTF-16 units and ambiguous escapes fail | encode underscores, array/object descriptors and Unicode; refuse unescaped `0`–`3` at the start or after a mapped slash; never replace a recorded registration | T09 short/int-long overload/Unicode/dependency exports, explicit-registration precedence and JVM-only `1bad` lookup refusal |
| JNI_OnLoad is a Java library initialization hook, not an ELF dependency constructor | invoke only the explicit root's own export, once, checking the returned version and pending exceptions; pin failures but refuse their registered native entries | T09 normal NDK root/dependency, root without a hook, wrong version, pending exception, data hook and post-failure entry refusal |

## MyGo

| Point | Consequence | Checked |
|---|---|---|
| MyGo's distribution documents `windows/arm64` | the front end can be native ARM64 | read; the Go module builds and tests on Windows 11 ARM64 |
| MyGo is a Go tool dependency (`go tool mygo …`) | its version moves with `go.mod` | built |
