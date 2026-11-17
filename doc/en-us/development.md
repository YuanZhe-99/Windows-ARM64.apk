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

```powershell
.\scripts\windows\check-requirements.ps1
.\scripts\windows\build.ps1 -Component All          # or Java, FrameworkRes, Fixtures, Host, NativeProbe
.\scripts\windows\test.ps1 -Suite Host
.\scripts\windows\test.ps1 -Suite Java
.\scripts\windows\test.ps1 -Suite Native
.\scripts\windows\test.ps1 -Suite Bionic
.\scripts\windows\test.ps1 -Suite Modules
.\scripts\windows\test.ps1 -Suite Memory
.\scripts\windows\test.ps1 -Suite Io
.\build\windows-arm64\bin\native-probe.exe suite .\tests\fixtures\elf\libn0abi.so
.\build\windows-arm64\bin\native-probe.exe sync .\tests\fixtures\elf\libsync.so
.\build\windows-arm64\bin\winapk.exe doctor --json
.\build\windows-arm64\bin\winapk.exe inspect .\tests\fixtures\hello-java.apk --json
.\build\windows-arm64\bin\winapk.exe run .\tests\fixtures\hello-java.apk --native=off
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
AArch64. `-Component Host` builds `winapk.exe`, `winapk-bridge.dll`, `native-probe.exe` and
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
