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
  (build-tools 36.0.0, android-36) and NDK r30 are needed from P1 and P2.
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
.\scripts\windows\build.ps1 -Component Java
.\scripts\windows\build.ps1 -Component Host -Configuration Debug
.\scripts\windows\test.ps1 -Suite Host
.\build\windows-arm64\bin\winapk.exe doctor --json
.\build\windows-arm64\bin\winapk.exe doctor --present
```

`-Component Java` builds Mac.apk's Java side into `build/windows-arm64/lib` (see
[port-audit.md](port-audit.md)); `-Component Host` builds `winapk.exe` and `winapk-probe.jar` into
`build/windows-arm64/bin` with clang-cl in Visual Studio's ARM64 developer environment, fetching
the Vulkan headers through vcpkg the first time, and fails if any PE it makes is not ARM64.
`test.ps1 -Suite Host` is fixture T01. `doctor --present` also opens a small test window to check
Vulkan presentation; without it that check is `not-run`.

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
