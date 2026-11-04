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
  `ANDROID_HOME`.
  `scripts/windows/check-requirements.ps1` checks all of it and installs nothing.

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
.\scripts\windows\build.ps1 -Component All          # or Java, FrameworkRes, Fixtures, Host
.\scripts\windows\test.ps1 -Suite Host
.\scripts\windows\test.ps1 -Suite Java
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
AArch64. `-Component Host` builds `winapk.exe`, `winapk-bridge.dll` and `winapk-probe.jar` into
`build/windows-arm64/bin` with clang-cl in Visual Studio's ARM64 developer environment, fetching
the Vulkan headers, zlib and SQLite through vcpkg the first time, and fails if any PE it makes is not
ARM64.

`test.ps1 -Suite Host` is fixture T01. `test.ps1 -Suite Java` is G1 (T02, T03, T04 and the `hello-jni`
negative case): it runs the fixtures with `winapk run`, posts clicks, keys, characters, a resize,
minimise, restore and deactivation to the app's window, suspends the UI thread once to check the
UI-call timeout (with upstream's `MACAPK_INJECT_FILE` input driver sending Back), runs a copy from
a Unicode path, and reads the runtime's log; it needs a desktop session and writes its data and logs
to `build/windows-arm64/test-data`. `doctor --present` also opens a small test window to check
Vulkan presentation; without it that check is `not-run`.

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
