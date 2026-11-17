# Port audit

What in Mac.apk is tied to macOS, how each part is handled on Windows ARM64, and in which phase.
Audited at Mac.apk `ff2afbf` (2026-11-03) and re-checked at `3f35611` (2026-11-16, upstream's v1.0.2379 sync), merged as the `macapk/` subtree, on Windows 11 ARM64
(build 26300). Re-run the searches below after every Mac.apk merge and update the tables.

## Method

- Read: `BUILDING.md`, `runtime/CMakeLists.txt`, `runtime/translator/CMakeLists.txt`,
  `runtime/scripts/build.sh`, `scripts/build_framework_res.sh`, `debug/build_java_only.sh`, the JVM options of `runtime/src/jvm_loader.cpp`, the native method
  declarations of `runtime/java`, and the file list and includes of `runtime/src` and
  `runtime/translator/src`.
- Searched `macapk/` for the couplings `PLAN.md` §7 names (counts below).
- Built: the whole Java side with the Windows ARM64 JDK (`scripts/windows/build.ps1 -Component
  Java`); a Windows host with an embedded JVM (`-Component Host`, fixture T01).
- P1: the AOSP framework resources (built with the SDK's `aapt2`) and the Java path of a pure-Java
  APK (below).
- P2: `runtime/translator/include/macapk/elf64_reader.h`, `elf_reader.h`, `elf64_reader.cpp` and the
  new `guest_rwx.h` and `guest_jit_pages.h` read for the guest loader (below).

## Areas

| Area | Upstream paths | Size | macOS coupling | Windows handling | Phase |
|---|---|---|---|---|---|
| Android framework | `runtime/java/android`, `androidx`, `dalvik`, `javax`, `libcore`, `org` | 3,017 Java files, 593k lines (with the rest of `runtime/java`) | 26 files call `NativeCocoa`; font and storage code names macOS paths | reused as is; calls move to platform-neutral host interfaces | P1 |
| DEX converter | `runtime/java/com/macapk/dex` | in the above | `AndroidSdk.java` looks for the SDK in macOS and Linux places | reused; the SDK lookup gains Windows places | P1 |
| Boot and loader | `runtime/java/com/macapk/loader` (`Boot.java`, `NativeCocoa.java`, …) | 57 natives in `NativeCocoa` | windows, surfaces, input, host services through Cocoa JNI; macOS paths | a host start-up service and `HostWindow`/`HostEventLoop`/`HostFramePresenter` (`PLAN.md` §4.3) | P1 |
| JNI host | `runtime/src/*.cpp` | 19 files, 22k lines | `dlfcn`, `mach`, `sys/*` (see below) | per file: POSIX calls get Windows equivalents behind platform interfaces | P1–P4 |
| macOS host services | `runtime/src/*.mm` | 26 files, 32k lines (`cocoa_bridge.mm` 14k) | Objective-C++, AppKit, AVFoundation, Metal, VideoToolbox | never built on Windows; replaced only where a phase needs the service | P1, P8 |
| Guest native | `runtime/translator` | 34 files, 152k lines (`bionic_shim.cpp` 69k, `arm64_native.cpp` 16k) | Mach memory, `x18`/`TPIDR_EL0` patching, Mach-O wrapping, Darwin signals | the ELF record layouts are reused; the N0 loader maps from program headers (P2, below); memory, execution, TLS and Bionic are rebuilt per capability (`PLAN.md` §5) | P2 (N0 done)–P4 |
| Graphics | `runtime/src/angle_glue.cpp`, `gles_bridge.cpp`, `egl14/gles*_bridges.inc`, `translator/src/bionic_shim_vulkan.cpp` | 6k lines | ANGLE on Metal, MoltenVK | ANGLE for Windows ARM64 (Vulkan, Direct3D 11); guest Vulkan to the Windows loader | P5, P6 |
| Skia | `thirdparty/skia-prebuilt`, `skia_metal_bridge.mm` | prebuilt `.dylib` | Metal | later: a Windows ARM64 Skia GPU bridge | P8 |
| Build | `runtime/CMakeLists.txt`, `runtime/scripts/build.sh`, `scripts/*.sh` | 835-line build script | OBJCXX, Homebrew, `codesign`, entitlements, `.dylib` staging, `python3` | `scripts/windows/*.ps1`; `CMakeLists.txt` branches to `platform/windows/windows.cmake` | P0 (done for Java and Host) |
| Translator build | `runtime/translator/CMakeLists.txt` | 2 targets | `macapk-translator` turns an ELF `.so` into a Mach-O `.dylib`; `libmacapk-bionic-shim.dylib` links capstone, zlib, libpng, ANGLE and the Security and CoreFoundation frameworks | not built on Windows: the guest loader of P2 maps ELF itself (no Mach-O or PE wrapping); the loader's screen uses Capstone 5.0.9 from vcpkg | P2 |
| Framework resources | `scripts/build_framework_res.sh` | one-time | needs `aapt2` (SDK build-tools or apktool's macOS binary) and a ~20 MB AOSP source tarball; makes a ~48 MB `framework-res-api36.apk`, never committed | done: the same recipe in `build.ps1 -Component FrameworkRes` with the SDK's `aapt2` (build-tools 36.0.0); `resources.arsc` stays stored rather than re-deflated, so the package is 66 MB | P1 |
| Launcher | `launcher/*.swift` | 8 files | SwiftUI app bundle | replaced by the MyGo front end | done (front end) |
| Prebuilts | `thirdparty/angle-prebuilt`, `skia-prebuilt`, MoltenVK | macOS binaries | macOS only | never shipped on Windows | — |

## Coupling counts

Lines matching each pattern (`grep -c`, summed), at `3f35611`.

| Pattern | `runtime/src` | `runtime/translator` | `runtime/java` | `launcher` | `scripts` |
|---|---|---|---|---|---|
| `Cocoa` | 581 | 33 | 275 | 4 | 0 |
| `AppKit` | 161 | 7 | 141 | 14 | 3 |
| `Metal` | 264 | 77 | 124 | 1 | 38 |
| Mach (`mach_`, `<mach/`, Mach-O) | 132 | 234 | 18 | 3 | 21 |
| `dyld` | 34 | 133 | 4 | 4 | 5 |
| `.dylib` | 22 | 140 | 63 | 21 | 158 |
| `/opt/homebrew` | 2 | 16 | 3 | 1 | 32 |
| `/Library` | 22 | 11 | 35 | 37 | 16 |
| `mmap` | 9 | 226 | 29 | 0 | 1 |
| `pthread_` | 42 | 963 | 2 | 0 | 0 |
| `sigaction` | 1 | 218 | 0 | 0 | 0 |
| `x18` | 6 | 643 | 25 | 9 | 22 |
| `TPIDR_EL0` | 0 | 83 | 0 | 1 | 1 |
| `svc` | 0 | 149 | 128 | 1 | 0 |

The guest-native code carries almost all register, signal, thread and memory coupling; the Java
side's coupling is mostly `NativeCocoa` calls and host paths.

## JNI surface

| What | Count | Handling |
|---|---|---|
| `native` declarations in `runtime/java` | 1,073 in 54 files; GLES/EGL 667, `NativeCocoa` 57, `NativeAudio` 43, `NativeMedia` 38, `CameraBridge` 30, `NativeSqlite` 26 | registered per phase; a native without a Windows implementation fails with a stable error, never returns a stand-in |
| `Java_*` exports in `runtime/src` | 155 | registered explicitly on Windows (`RegisterNatives`): the JVM finds `Java_*` names only in libraries loaded with `System.loadLibrary` |
| `RegisterNatives` calls in `runtime/src` | 66 | kept |
| `System.loadLibrary`/`System.load` in `runtime/java` | 42 | each reviewed in P1; guest `.so` loading goes to the guest loader, never `LoadLibrary` |

## Java side on Windows

`build.ps1 -Component Java` repeats the Java steps of `runtime/scripts/build.sh`: one `javac
--release 21` pass, `android-framework.jar` (everything but `com/macapk`) with the bundled fonts,
`macapk-loader.jar` (`com/macapk` with the resolver's `META-INF/services`), the `java_base_patch`
and `jdk_unsupported_patch` modules, `net_spi` and `bcprov`; since `8b45e48`, Android's `org.apache.http.legacy`
(upstream's converted jar) on the classpath and unpacked into `android-framework.jar`, never over a
compiled class.

| Check | Result |
|---|---|
| The Java side compiles unchanged with Microsoft OpenJDK 21.0.13 ARM64 | PASS at `3f35611`: javac's limit of 100 warnings (internal `Unsafe` API and deprecations), no error |
| `android-framework.jar` | 14,979 KB, 6,415 entries |
| `macapk-loader.jar` | 1,556 KB, 530 entries |
| Patch modules load on the JVM (`--patch-module java.base=… --patch-module jdk.unsupported=…`) | PASS (T01) |
| `build-id.txt` (upstream's DEX-cache identity, `build_ids.py`) | NOT_RUN: needs Python; re-expressed in P1 with the runtime that reads it |
| `pairip-refs` | absent in the public repository (as upstream says); not staged |

## JVM options

Upstream's `jvm_loader.cpp` options, and their state on the Windows JVM.

| Option | Windows |
|---|---|
| `--enable-native-access=ALL-UNNAMED`, the `--add-opens` of `java.io`, `java.util`, `java.lang`, `jdk.internal.loader`, the `--add-exports` of `sun.net.www.protocol.http(s)` | accepted by `JNI_CreateJavaVM` with unknown options refused (T01) |
| `-Xrs`, `-Xss16m` | accepted (T01) |
| `-XX:+UseSerialGC`, heap sizing, `-Djava.net.preferIPv4Stack`, verification flags, AOT cache | standard HotSpot options; checked when P1 starts an app with them |
| `--patch-module` for `java.base` and `jdk.unsupported` | accepted (T01, with `java.exe`) |

## Host sources

`runtime/src/*.cpp` by platform dependency.

| File | Lines | POSIX/Apple includes | Windows handling |
|---|---|---|---|
| `main.cpp` | 3,938 | `dlfcn`, `mach`, `sys/sysctl`, `sys/wait`, `unistd` | replaced by the Windows host entry (`winapk run`, P1); its steps re-expressed |
| `native_methods.cpp` | 4,525 | `dlfcn`, `mach_time`, `sys/proc`, `sys/sysctl` | ported per method behind `HostClock`, `HostFilesystem` |
| `apk_unpacker.cpp` | 4,520 | `dlfcn`, `sys/attr`, `sys/qos`, `sys/wait` | APK parsing reused; file and process calls replaced (P1) |
| `window_policy.cpp`, `window_traits.cpp` | 3,017 | `sys/stat` | reused for window rules; presentation goes to `HostWindow` |
| `jvm_loader.cpp` | 818 | `dlfcn`, `mach`, `sys/mman` | the Windows loader already exists in `platform/windows` (`LoadLibraryEx` of `jvm.dll`) |
| `native_sqlite.cpp` | 783 | CoreFoundation | built into `winapk-bridge.dll` against vcpkg's SQLite (P1); a `_WIN32` branch replaces its CoreFoundation case mapping and collation with Win32 NLS |
| `native_os.cpp`, `icu_host_bridge.cpp`, `native_linker.cpp`, `jit_wx_guard.cpp` | 1,291 | `dlfcn`, `pthread`, `sys/statvfs` | per capability; `jit_wx_guard` follows the W^X rule (`AGENTS.md`) |
| `angle_glue.cpp`, `gles_bridge.cpp` and the `.inc` bridges | 5,963 | `pthread` | P5 |
| `apk_native_exec.cpp`, `app_process_group.cpp`, `app_native_stub.cpp`, `host_fonts.cpp` | 650 | `signal` | P1/P2 |

## Java path on Windows (P1)

What a pure-Java APK needs from the host, as `winapk run` provides it (G1, [platform-notes.md](platform-notes.md)).

| Upstream piece | Windows |
|---|---|
| `main.cpp`'s start-up (install, unpack, DEX conversion, JVM options, natives, `Boot.run`) | re-expressed in `run.cpp`: the APK is read natively (`apk.cpp`: ZIP, ZIP64, inflate, binary manifest, entry-name screening), DEX is converted by com.macapk.dex in a child process as upstream does, but a `winapk dex` whose JVM gets its arguments as UTF-16 strings rather than a `java.exe` (ADR-0003), cached by the APK, converter and framework hashes, then the JVM is created in-process with upstream's options minus the macOS-only ones (hosts-file path, AOT cache); `winapk.exe` has the UTF-8 active code page, so option strings carry Unicode paths |
| `build-id.txt` and `MACAPK_BUILD_JAVA_ID` (`build_ids.py`, read by `main.cpp`) | `winapk run` exports `MACAPK_BUILD_JAVA_ID` from both jars' whole digests, so the app's own dynamic-DEX cache persists and is invalidated by any converter or framework change (ADR-0003); `MACAPK_BUILD_NATIVE_ID` waits for the guest loader (P2–P4) |
| `NativeLog` (4 of 5 natives) | `print`, `nativeBoostCurrentThreadQos` (thread priority above normal), `nativeBootTimeNanos` (the performance counter, HotSpot's `nanoTime` source) bound; `nativeInstallJniTrace` and `nativeRegisterPairipGate` unbound |
| `NativeCocoa` window, presentation, input (29 of 57 natives) | bound in `window.cpp` (ADR-0002): `init`, `runLoop`, sizes and scale, windows and views, the renderer, `presentBitmap`, touch registration, orientation and aspect cap, `setSecureContent` (`WDA_EXCLUDEFROMCAPTURE`), task to back/front, data and library dirs, `exitProcess`. Window state as upstream: a minimised window draws at 2 Hz (upstream's hidden-window throttle), restoring it calls `ActivityHost.macapkTaskToFront` (upstream's unhide hook), focus changes are logged, and losing focus or the mouse capture mid-press releases capture and sends `ACTION_CANCEL`. Text: a character no mapped key delivered (an IME commit, a dead-key composition, an unmapped key) goes to `dispatchKey` as KEYCODE_UNKNOWN with the character, one UTF-16 unit per DOWN/UP pair, upstream's shape for composed text. Unbound, each failing with `UnsatisfiedLinkError` where called: the AOSP window-policy session (`activityWindowTransition`, `windowOverrideOrientation`, `applyWindowOrientationFor`, `setTopActivityConfigChanges`), URL and scheme handling, the IOSurface ring (`presentFrame`), GL surfaces and overlays, `hostFontFile`, `setSystemFontsDir`, `setSystemProperty`, the translation overlay, trackpad and quit-gesture debugging |
| `NativeSqlite` (26 natives) | bound: upstream's `native_sqlite.cpp` itself, built into `winapk-bridge.dll` against SQLite 3.54.1, with the Unicode `upper`/`lower`/`LIKE` and the LOCALIZED, UNICODE and PHONEBOOK collations every Android connection carries. Upstream quirk kept: it converts strings as modified UTF-8, so a character outside the BMP in SQL text or a column differs from a device (macOS alike) |
| Other host natives Boot reaches (`NativeOs.setenv`, `GamepadBridge`, Monet wallpaper) | unbound; Boot logs each and goes on |
| `AppClassLoader.findLibrary`'s stub fallback | off gives `GUEST_NATIVE_DISABLED`; restricted rejects unredirected JVM loading. Only guest DEX calls redirect to `GuestNativeLoader`; host/JDK calls and macOS's original redirect targets are unchanged |
| `ClassFileRewriter`'s `/proc/meminfo` alias `/tmp/mcmeminf` | no such folder on Windows: an app reading `/proc/meminfo` gets nothing (`Boot` logs it); needs a same-length Windows path or another redirect |
| An app's own `System.load(absolutePath)` | Windows DEX redirects it (and Runtime.load, System/Runtime.loadLibrary and mapLibraryName) to the process-private guest namespace; absolute paths outside its admitted APK directory fail. AppClassLoader offers existing/later classes for JNI exports; Windows skips macOS facade creation |
| `dir_naming.h` (new at `8b45e48`: the version's dots folded out of an app's folder name) | not needed: `winapk run` names the folder after the package alone |

## Guest ELF loader (P2)

What the N0 loader (`runtime/guest/loader`, [native-profiles.md](native-profiles.md)) takes from upstream.

| Upstream piece | Windows |
|---|---|
| `elf64_reader.h`'s record layouts (`Elf64Ehdr`, `Elf64Phdr`, `Elf64Sym`, `Elf64Rela`) and `elf_reader.h`'s constants | used as they are |
| `elf64_reader.cpp`'s parser | not used: it finds symbols through section headers, reads with POSIX `open`/`pread` and copies segments for translation; the loader works from program headers (`PLAN.md` §9.2) |
| `arm64_native.cpp`'s memory and patching (rewriting `x18` and `TPIDR_EL0` uses, Mach VM) | not used: N0 refuses such code instead of patching it (`PLAN.md` §5.3) |
| `guest_rwx.h`, `guest_jit_pages.h` (new at `8b45e48`: executable pages for guest JITs) | not used: N0 allows no executable allocation |
| Capstone, which the translator links | the loader's screen uses Capstone 5.0.9 from vcpkg |

## Findings that change the plan's assumptions

- **Vulkan on this PC is translated.** The Adreno 8cx Gen 3 driver installs no Vulkan ICD; the
  loader lists two Mesa Dozen devices (Vulkan 1.2 on Direct3D 12: the Adreno and the software
  Basic Render Driver). The Win32 present test passes on them. Guest Vulkan (P6) and ANGLE's
  Vulkan backend (P5) would run on a translation layer here; `doctor` reports it
  (`VULKAN_DRIVER_TRANSLATED`), and G4 needs a PC with a vendor Vulkan driver or an explicit
  decision ([decisions.md](decisions.md)).
- **No Vulkan headers ship with Windows or the SDK**: they come from vcpkg at a locked baseline
  (build time only).
- **The Java side needs no change to compile** on the Windows ARM64 JDK; its platform coupling is
  at run time.

## Next

P2's profile N0 is done (G2). P3 now has a separate T08 thread foundation (managed pthreads,
errno, keys/destructors, C emulated TLS and untimed mutex/condition synchronization), implemented under `runtime/guest/bionic` rather
than porting Darwin's shim. T07 adds dependency/constructor transactions and guest dl APIs under
`runtime/guest/loader` and `runtime/guest/bionic`; T08 now also covers private-heap memory/strings,
mapped files/time and guest-PC fault diagnostics. A small Android-ABI helper keeps restricted
formatting/va_list entirely guest-side; fixed byte logs reach the host. P3's declared C/Bionic
scope now includes logical system libraries, validated import versions and real CRT lifetime
records, tested with a normally linked NDK C fixture. P4 now passes G3 for the declared subset
([native-profiles.md](native-profiles.md)), including APK library redirects, the real Activity
matrix and a CFG-enabled build. Next are P5/P6 graphics, with the vendor-Vulkan path blocked
as described above. P1's open items are a density
change while an app runs and the host natives an app may need next (`NativeOs`, host fonts); the
interface's switch from `winapk.Mock` to `winapk run` belongs to P7 (`PLAN.md` §14).
