# Development

## Requirements

- The latest Go release, named in `go.mod` (an older Go downloads it automatically). MyGo is a Go
  tool dependency (`go tool mygo …`) at its latest tag.
- A Windows 11 ARM64 machine for every run of the runtime; graphics work needs a desktop session
  and a GPU with an ARM64 Vulkan driver.
- For the runtime: what `build-lock.json` locks. Visual Studio 2026 with the ARM64 C++ tools,
  Clang, CMake and vcpkg components provides clang-cl, MSVC, the Windows SDK, CMake, Ninja and
  vcpkg (none of them on PATH). The JDK is Microsoft OpenJDK 21 for ARM64, installed with
  `winget install --id Microsoft.OpenJDK.21 -e --architecture arm64` (its MSI sets the system
  `JAVA_HOME` and PATH; `WINAPK_JAVA_HOME` overrides it for the build). The Android SDK
  (build-tools 36.0.0, android-36, cmdline-tools) and NDK r30 build the platform resources and
  the fixtures, and the Kotlin fixture uses the kotlinc 2.4.0 that Android Studio bundles (or
  `KOTLIN_HOME`); Android Studio's default location `%LOCALAPPDATA%\Android\Sdk` is found without
  `ANDROID_HOME` (installed with Android Studio or `android sdk install`).
  `scripts/windows/check-requirements.ps1` (run in PowerShell 7, `pwsh`, like all scripts under
  `scripts/windows/`) checks all of it and installs nothing.

## Commands

```bash
gofmt -l cmd internal tools
go vet ./...
go test ./...
CGO_ENABLED=0 go test ./...
go tool mygo vet .
go tool mygo i18n check
go run ./tools/checkdocs           # doc/en-us and doc/zh-cn mirror each other
```

The runtime's commands are in the next section.

## Building and testing the runtime

The Go module ignores `./build` when expanding package wildcards. This generated/download
tree can contain large third-party checkouts; it is not product Go source. Root `cmd`,
`internal`, `locales` and `tools` remain covered by the standard Go commands.

```powershell
.\scripts\windows\check-requirements.ps1
.\scripts\windows\build.ps1 -Component All          # or Java, FrameworkRes, Fixtures, Host, NativeProbe, JniProbe
.\scripts\windows\build.ps1 -Component Angle        # optional P5a source build; separate from Java
.\scripts\windows\build.ps1 -Component AngleProbe
.\scripts\windows\test.ps1 -Suite GraphicsHost      # host-only; not G4
.\scripts\windows\build.ps1 -Component Host         # after Angle, to enable the Java GLES bridge
.\scripts\windows\test.ps1 -Suite GraphicsJava      # real APK; not guest GLES or G4
.\scripts\windows\test.ps1 -Suite Surface           # one APK; native window lifetime only
.\scripts\windows\test.ps1 -Suite GraphicsGuest     # guest EGL/GLES T11; not guest Vulkan or G4
.\scripts\windows\test.ps1 -Suite VulkanWsi         # guest WSI and transfer-clear T13; not G4
.\scripts\windows\test.ps1 -Suite Host
.\scripts\windows\test.ps1 -Suite Java
.\scripts\windows\test.ps1 -Suite Native
.\scripts\windows\test.ps1 -Suite Bionic
.\scripts\windows\test.ps1 -Suite Modules
.\scripts\windows\test.ps1 -Suite Memory
.\scripts\windows\test.ps1 -Suite Io
.\scripts\windows\test.ps1 -Suite Faults
.\scripts\windows\test.ps1 -Suite Format
.\scripts\windows\test.ps1 -Suite Libraries
.\scripts\windows\test.ps1 -Suite Jni
.\scripts\windows\test.ps1 -Suite ApkJni
.\build\windows-arm64\bin\native-probe.exe suite .\tests\fixtures\elf\libn0abi.so
.\build\windows-arm64\bin\native-probe.exe sync .\tests\fixtures\elf\libsync.so
.\build\windows-arm64\bin\winapk.exe doctor --json
.\build\windows-arm64\bin\winapk.exe inspect .\tests\fixtures\hello-java.apk --json
.\build\windows-arm64\bin\winapk.exe run .\tests\fixtures\hello-java.apk --native=off
.\build\windows-arm64\bin\winapk.exe run .\tests\fixtures\hello-jni.apk --native=restricted
.\build\windows-arm64\bin\winapk.exe dex .\tests\fixtures\hello-java.apk .\build\classes.jar
```

`-Component Java` builds Mac.apk's Java side into `build/windows-arm64/lib` (see
[port-audit.md](port-audit.md)). `-Component FrameworkRes` builds the platform resource package
`bin/tools/framework-res.apk` once from AOSP's `frameworks/base/core/res` at the tag
`build-lock.json` names, with the SDK's `aapt2` (the recipe of upstream's
`scripts/build_framework_res.sh`). `-Component Fixtures` builds the fixture APKs of
`tests/windows-arm64/apk-fixtures` into `tests/fixtures` (never committed) with the SDK's `aapt2`,
`d8`, `zipalign` and `apksigner`, for `src/**/*.kt` kotlinc (the Kotlin standard library is
dexed in) and, for `jni/*.c`, the NDK's clang; each gets a
`<name>.json` with its build parameters and hashes, and every library is checked to be ELF64
AArch64. `-Component Host` builds `winapk.exe`, `winapk-bridge.dll`, `native-probe.exe`, `jni-probe.exe` and
`winapk-probe.jar` into `build/windows-arm64/bin` with clang-cl in Visual Studio's ARM64 developer
environment, fetching the Vulkan headers, zlib, SQLite and Capstone through vcpkg the first time,
and fails if any PE it makes is not ARM64. `-Component NativeProbe` builds `native-probe.exe` alone
and the guest ELF fixtures of `tests/windows-arm64/guest-elf/fixtures.psd1` into `tests/fixtures/elf`
with the NDK's clang (never committed; hashes in `fixtures.json` there).

`test.ps1 -Suite Host` is fixture T01. `test.ps1 -Suite Java` is G1 (T02, T03, T04 and the `hello-jni`
negative case): it runs the fixtures with `winapk run`, posts clicks, keys, characters, a resize,
minimise, restore and deactivation to the app's window, suspends the UI thread once to check the
UI-call timeout (with upstream's `MACAPK_INJECT_FILE` input driver sending Back), runs a copy from
a Unicode path, and reads the runtime's log; it needs a desktop session and writes its data and logs
to `build/windows-arm64/test-data`. `doctor --present` also opens a small test window to check
Vulkan presentation; without it that check is `not-run`. `test.ps1 -Suite Native` is G2 (T05, T06):
no JVM and no window; it runs `native-probe.exe suite` on each T05 build, `load` on each T06 file
(checking the refusal against `fixtures.psd1`) and `stress`.

`test.ps1 -Suite Bionic` runs the P3 thread foundation (T08) through `native-probe.exe threads`,
not G3: 8/32 simultaneous workers, errno/key/emulated-TLS isolation and destructors, explicit
unsupported cases, then 200 cycles (1600 workers) with empty-registry and handle checks. Each
probe has a 60-second deadline. It also checks the unchanged refusals for ELF TLS, raw syscalls
and platform-reserved registers using the P3 import table. The fixture deliberately uses
`-nostdlib -femulated-tls`; dependencies, other Bionic groups and JNI are not proved
by it. Sources, expected results and binary hashes remain in the fixture recipe/manifest.

The same suite runs `native-probe.exe sync` on `libsync.so`: normal/recursive/error-checking
mutexes, eight-thread contention, signal and broadcast with predicate loops, wait/relock and
busy-destroy protection, stale/copied markers and unsupported attributes. It also runs 200
sync cycles (1600 workers) and checks that timed-wait imports are refused and owned-object
teardown terminates diagnostically. This no-dependency fixture does not enable N1 or JNI;
timed waits/locks and attribute APIs remain unsupported ([native-profiles.md](native-profiles.md)).

`-Suite Modules` runs only T07's compact graph matrix (`native-probe modules`), also included in
`-Suite Bionic`. It validates fixture hashes and ten semantic cases in a single process with a
30-second deadline: rollback, constructor order, weak imports, guest dl API/errors, lookup, reference/pinning and
initializer reentry/concurrency. Use this targeted suite while developing module loading; run
the full runtime regression once the capability is stable, not after every edit.

`-Suite Memory` selects only T08's `memory` command (also included in `Bionic`): four semantic
cases plus a host-state check, and two fatal ownership cases from the same source. It uses
`-fno-builtin` so the tests call the imported wrappers rather than compiler-folded operations;
there is no additional long stress loop.

`-Suite Io` selects only T08's `io` command (also in `Bionic`): four file/time semantic cases,
host-state and manifest checks, plus confirmation of the Unicode-mapped bytes and unchanged
out-of-root junction target. It prepares only ignored test directories below `build/windows-arm64/test-data`.
The probe accepts `io <elf> <android-root>`; no host working-directory or environment change is made.

`-Suite Faults` selects T08's `faults` command (also in `Bionic`): a host SEH-chain case and two
separately terminating guest processes (access violation and trap). It requires a module and
guest virtual address in each fatal diagnostic, exit 3, and no fabricated successful result.
The N0 `Native` regression also exercises the new executable-range registry over its existing
load/unload cycles; no duplicate fault stress suite is added.

`-Suite Format` selects the bounded T08 formatter/log matrix (also in `Bionic`).
`NativeProbe` and `All` build the Android-ABI helper into `bin/guest/libwinapk-bionic.so` and
record its source, exact NDK arguments and hash in `bin/guest/bionic.json`. The suite checks that
hash as well as the fixture's, five cases and exact UTF-8 log bytes. The helper is product code,
not a system libc or generated test fixture; no raw host variadic export is used.

`-Suite Libraries` selects T07's N1 system-library/version/CRT matrix (also in `Bionic`).
Its positive library uses the NDK driver's normal shared-library link, including CRT start files
and libc/libdl/liblog dependencies: no `-nostdlib`, `-nostartfiles`, `-femulated-tls` or disabled
stack protector. This verifies that recorded command and source, not all Android Studio/CMake
configurations or arbitrary NDK output. Three small refusal variants cover an unsupported fork
import, a valid but unknown version name/hash, and a malformed version chain, before constructors.
The single process also checks logical lookup scopes, real finalization and pinned callbacks at
shutdown. `fixtures.json` records the exact normal-link flags; copies staged under ignored
`test-data/n1-libraries` keep each variant's original SONAME without modifying source fixtures.

`-Component JniProbe` builds only `jni-probe.exe`, its Java probe jar, the Android-ABI
JNI/Bionic helpers and the ELF fixture recipes. `-Suite Jni` runs T09's foundation,
not G3: one real ARM64 JVM with `-Xcheck:jni` checks constructor/static/instance/nonvirtual
.../V/A calls, references/strings/arrays, pending exceptions, and normal/daemon worker attachment.
Nine semantic groups share one process, including actual JVM native registration/calls,
persistent/nested entry state and teardown;
five short fatal processes check unsupported JNI, cross-thread environment misuse, guest trap,
access violation and entry into a method registered by a failed JNI hook. Each has a 15-second deadline.
Seven fixture hashes, two helper hashes and the semantic/summary/fatal results make 24 checks.
Warnings from CheckJNI fail the positive run.
The positive run uses `-Xbatch` and checks `T09-compilation.xml` for an installed compilation
of the Java implicit-fault method, rather than assuming a hot loop necessarily ran JIT code.
The JNI helper and core fixture use `-O2 -nostdlib -fno-builtin -fno-stack-protector`;
the core fixture also uses `-femulated-tls` to test the declared TLS model. The separate
library family uses normal NDK API-30 shared-library links, CRT and libc/libdl dependencies
without those special flags. This proves those recipes, not arbitrary NDK APK loading.
The APK JNI fixture also links normally at its manifest's API 30 baseline. Native APK fixture
compilation now takes its API from the manifest rather than assuming API 24 for all fixtures.
The separate `native-probe` remains
JVM-free. Logs and JVM error files stay under ignored `test-data/logs/T09-*` and `hs_err_pid*.log`.
The native group registers six fixed-signature methods and rejects a data address, a non-native
Java method, an oversized slot reservation and a foreign class loader. The JVM then calls the
PE adapters directly, not a host-controlled guest test entry. Teardown verifies real unregistration.
Two additional semantic groups cover explicit JNI library lifecycle and export binding:
dependency-first constructors, root-only/once-only JNI_OnLoad, a root without a hook,
short-name precedence, overloaded-long/Unicode/array-object descriptor names, dependency exports
and explicit-registration precedence. A JVM-only `1bad` declaration remains unresolved despite
a matching export, as required by JNI's ambiguous-escape rule.
Wrong versions, pending exceptions, data-valued hooks and repeated failed loads are refused.
The separate fatal hook case proves that an invalid-version hook cannot leave a usable native binding.

`-Suite ApkJni` runs T09's G3 Activity matrix: off/restricted normal-close launches and one
necessary unsupported-JNI termination launch, plus a ZIP traversal refusal, eight checks total.
The Activity asserts the native result 43, explicit registration, once-only System/Runtime
loads, Android naming, late-class exports and missing/outside-directory refusal. It additionally
checks strings/arrays, long/float/double and instance results, exception propagation/clearing,
dependency-only exports, dependency hook separation, cached bad-version refusal and an attached
guest worker's nested callback. The positive matrix runs with CheckJNI; all launches wait for the
actual first bitmap frame, not a window-focus notification (which need not fire when unfocused).
Use the existing 30-second log deadline: CheckJNI/font drawing can put the first frame near
the old 15-second limit. There is no fixed startup sleep or automatic retry.
The refusal button calls MonitorEnter and must exit 3 with slot 217,
not return a stand-in result. The screenshot is `T09-apk-activity.png`, not a substitute for values.
ZIP traversal is refused before DEX conversion or boot. `hello-jni.json` records source/links,
SONAME arguments and hashes; no broad app sweep or duplicate pressure loop is added.

For the CFG experiment, repeat `Build-Host`'s CMake configure/build arguments in the separate
`build/windows-arm64/native/CFGDebug` directory, keeping Debug/ARM64 and the same output bin,
and add these CMake flags:

```powershell
'-DCMAKE_CXX_FLAGS=/DWIN32 /D_WINDOWS /EHsc /guard:cf'
'-DCMAKE_EXE_LINKER_FLAGS=/guard:cf'
'-DCMAKE_SHARED_LINKER_FLAGS=/guard:cf'
```

Run Host, Java, Native, Bionic, Jni and ApkJni before restoring the ordinary build with
`cmake --build build/windows-arm64/native/Debug --clean-first` in the same ARM64 developer shell.
Because the two build trees share bin, an incremental Host build alone can keep newer CFG
outputs; require CFG=0 in both running-process queries after restoration.
Jni and ApkJni query the running process's CFG policy; the native
registration group also verifies every static PE entry against its live GFID table when CFG is
enabled. Check PE load configuration, not merely the requested compiler flags. This is CFG
verification of the declared profile, not arbitrary guest control-flow safety or a security sandbox.

`winapk run` keeps each app's data under `%LOCALAPPDATA%\Windows-ARM64.apk\Apps` (`--data-dir`
moves it): `android\` is the Android root the app sees (`data\data\<package>`, `sdcard`),
`code\<package>\` its converted code with the cache stamp, `logs\` the JVM's crash logs. The
runtime's log goes to standard error. `winapk dex` is `run`'s conversion step on its own: it
converts an APK's DEX to a jar in a JVM of its own process.

## Running the interface

```bash
go run ./cmd/windows-arm64-apk
WINAPK_FRESH=1 go run ./cmd/windows-arm64-apk
SCREENSHOT_DIR=/path/to/dir go test ./internal/gui -run TestScreensRender
```

The first runs against the sample apps of `design/`; `WINAPK_FRESH=1` starts with an empty
library and no Java runtime, as on a first run. The test writes every screen in both languages and
appearances as PNG, to compare with `design/`. Strings live in `locales/en.json` and
`locales/zh-Hans.json`; after changing them run `go tool mygo generate`.

## Updating Mac.apk

Mac.apk is a git subtree under `macapk/` that follows the latest upstream `main`:

```bash
git ls-remote https://github.com/kksimp/Mac.apk main
git subtree pull --prefix=macapk https://github.com/kksimp/Mac.apk main --squash
```

The first import uses `git subtree add` with the same arguments. After each merge, re-check the
entry points listed in [upstream.md](upstream.md) and re-run the gates the merged files touch.

## Testing principles

Tests protect product contracts and error-prone logic, preferably table-driven; every fixed bug
gets a regression test. Fixtures are self-built and publishable, each with source, build
parameters, a hash and expected output, and each capability has a positive and a negative case.
Each test names the fixture ID (`T01`…) or gate it covers. Execution, JVM and GPU tests count only
on real ARM64 hardware; results are reported as `PASS`, `FAIL`, `BLOCKED` or `NOT_RUN`.
