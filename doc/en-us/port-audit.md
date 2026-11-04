# Port audit

What in Mac.apk is tied to macOS, how each part is handled on Windows ARM64, and in which phase.
Audited at Mac.apk `ff2afbf` (merged as the `macapk/` subtree), 2026-11-03, on Windows 11 ARM64
(build 26300). Re-run the searches below after every Mac.apk merge and update the tables.

## Method

- Read: `BUILDING.md`, `runtime/CMakeLists.txt`, `runtime/translator/CMakeLists.txt`,
  `runtime/scripts/build.sh`, `scripts/build_framework_res.sh`, `debug/build_java_only.sh`, the JVM options of `runtime/src/jvm_loader.cpp`, the native method
  declarations of `runtime/java`, and the file list and includes of `runtime/src` and
  `runtime/translator/src`.
- Searched `macapk/` for the couplings `PLAN.md` §7 names (counts below).
- Built: the whole Java side with the Windows ARM64 JDK (`scripts/windows/build.ps1 -Component
  Java`); a Windows host with an embedded JVM (`-Component Host`, fixture T01).
- Not yet: building or reading `runtime/translator` in depth (P2), the AOSP framework resources
  (needs the Android SDK's `aapt2`), any APK.

## Areas

| Area | Upstream paths | Size | macOS coupling | Windows handling | Phase |
|---|---|---|---|---|---|
| Android framework | `runtime/java/android`, `androidx`, `dalvik`, `javax`, `libcore`, `org` | 3,049 Java files, 578k lines (with the rest of `runtime/java`) | 26 files call `NativeCocoa`; font and storage code names macOS paths | reused as is; calls move to platform-neutral host interfaces | P1 |
| DEX converter | `runtime/java/com/macapk/dex` | in the above | `AndroidSdk.java` looks for the SDK in macOS and Linux places | reused; the SDK lookup gains Windows places | P1 |
| Boot and loader | `runtime/java/com/macapk/loader` (`Boot.java`, `NativeCocoa.java`, …) | 57 natives in `NativeCocoa` | windows, surfaces, input, host services through Cocoa JNI; macOS paths | a host start-up service and `HostWindow`/`HostEventLoop`/`HostFramePresenter` (`PLAN.md` §4.3) | P1 |
| JNI host | `runtime/src/*.cpp` | 17 files, 21k lines | `dlfcn`, `mach`, `sys/*` (see below) | per file: POSIX calls get Windows equivalents behind platform interfaces | P1–P4 |
| macOS host services | `runtime/src/*.mm` | 22 files, 30k lines (`cocoa_bridge.mm` 14k) | Objective-C++, AppKit, AVFoundation, Metal, VideoToolbox | never built on Windows; replaced only where a phase needs the service | P1, P8 |
| Guest native | `runtime/translator` | 25 files, 121k lines (`bionic_shim.cpp` 67k, `arm64_native.cpp` 16k) | Mach memory, `x18`/`TPIDR_EL0` patching, Mach-O wrapping, Darwin signals | the ELF parsing is reused; memory, execution, TLS and Bionic are rebuilt per capability (`PLAN.md` §5) | P2–P4 |
| Graphics | `runtime/src/angle_glue.cpp`, `gles_bridge.cpp`, `egl14/gles*_bridges.inc`, `translator/src/bionic_shim_vulkan.cpp` | 6k lines | ANGLE on Metal, MoltenVK | ANGLE for Windows ARM64 (Vulkan, Direct3D 11); guest Vulkan to the Windows loader | P5, P6 |
| Skia | `thirdparty/skia-prebuilt`, `skia_metal_bridge.mm` | prebuilt `.dylib` | Metal | later: a Windows ARM64 Skia GPU bridge | P8 |
| Build | `runtime/CMakeLists.txt`, `runtime/scripts/build.sh`, `scripts/*.sh` | 835-line build script | OBJCXX, Homebrew, `codesign`, entitlements, `.dylib` staging, `python3` | `scripts/windows/*.ps1`; `CMakeLists.txt` branches to `platform/windows/windows.cmake` | P0 (done for Java and Host) |
| Translator build | `runtime/translator/CMakeLists.txt` | 2 targets | `macapk-translator` turns an ELF `.so` into a Mach-O `.dylib`; `libmacapk-bionic-shim.dylib` links capstone, zlib, libpng, ANGLE and the Security and CoreFoundation frameworks | not built on Windows: the guest loader of P2 maps ELF itself (no Mach-O or PE wrapping); capstone, zlib and libpng come from vcpkg if reused | P2 |
| Framework resources | `scripts/build_framework_res.sh` | one-time | needs `aapt2` (SDK build-tools or apktool's macOS binary) and a ~20 MB AOSP source tarball; makes a ~48 MB `framework-res-api36.apk`, never committed | the same recipe from PowerShell with the SDK's `aapt2` (build-tools 36.0.0, `build-lock.json`) | P1 |
| Launcher | `launcher/*.swift` | 8 files | SwiftUI app bundle | replaced by the MyGo front end | done (front end) |
| Prebuilts | `thirdparty/angle-prebuilt`, `skia-prebuilt`, MoltenVK | macOS binaries | macOS only | never shipped on Windows | — |

## Coupling counts

Lines matching each pattern (`grep -c`, summed), at `ff2afbf`.

| Pattern | `runtime/src` | `runtime/translator` | `runtime/java` | `launcher` | `scripts` |
|---|---|---|---|---|---|
| `Cocoa` | 579 | 32 | 274 | 4 | 0 |
| `AppKit` | 157 | 7 | 139 | 14 | 3 |
| `Metal` | 264 | 76 | 124 | 1 | 38 |
| Mach (`mach_`, `<mach/`, Mach-O) | 38 | 268 | 16 | 1 | 19 |
| `dyld` | 34 | 125 | 4 | 3 | 5 |
| `.dylib` | 19 | 134 | 51 | 18 | 158 |
| `/opt/homebrew` | 2 | 16 | 3 | 1 | 31 |
| `/Library` | 20 | 10 | 33 | 34 | 13 |
| `mmap` | 8 | 178 | 20 | 0 | 0 |
| `pthread_` | 26 | 914 | 2 | 0 | 0 |
| `sigaction` | 1 | 218 | 0 | 0 | 0 |
| `x18` | 3 | 383 | 5 | 5 | 20 |
| `TPIDR_EL0` | 0 | 87 | 0 | 1 | 1 |
| `svc` | 0 | 95 | 80 | 1 | 0 |

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
and `jdk_unsupported_patch` modules, `net_spi` and `bcprov`.

| Check | Result |
|---|---|
| The Java side compiles unchanged with Microsoft OpenJDK 21.0.13 ARM64 | PASS: 78 s, 120 warnings (internal `Unsafe` API and deprecations), no error |
| `android-framework.jar` | 14,262 KB, 5,981 entries |
| `macapk-loader.jar` | 1,380 KB, 475 entries |
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
| `native_sqlite.cpp` | 783 | CoreFoundation | ported in P1 with a Windows SQLite build |
| `native_os.cpp`, `icu_host_bridge.cpp`, `native_linker.cpp`, `jit_wx_guard.cpp` | 1,291 | `dlfcn`, `pthread`, `sys/statvfs` | per capability; `jit_wx_guard` follows the W^X rule (`AGENTS.md`) |
| `angle_glue.cpp`, `gles_bridge.cpp` and the `.inc` bridges | 5,963 | `pthread` | P5 |
| `apk_native_exec.cpp`, `app_process_group.cpp`, `app_native_stub.cpp`, `host_fonts.cpp` | 650 | `signal` | P1/P2 |

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

P1 starts from the Java side built here, `NativeCocoa`'s 57 natives and `main.cpp`'s start-up
steps; P2 starts from `elf64_reader.cpp`, `elf_reader.cpp` and the memory parts of
`arm64_native.cpp`.
