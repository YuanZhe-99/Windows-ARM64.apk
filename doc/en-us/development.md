# Development

## Requirements

- The latest Go release, named in `go.mod` (an older Go downloads it automatically). MyGo is a Go
  tool dependency (`go tool mygo …`) at its latest tag.
- A Windows 11 ARM64 machine for every run of the runtime; graphics work needs a desktop session
  and a GPU with an ARM64 Vulkan driver.
- For the runtime (from P0): an ARM64 OpenJDK, the Android SDK and NDK, clang-cl with the Windows
  SDK, CMake and Ninja. P0 chooses their versions and locks them in `build-lock.json`;
  `scripts/windows/check-requirements.ps1` will check them without installing anything.

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

The runtime build, `winapk doctor` and the fixture suites are added here with the first runtime
code (`PLAN.md` §18 lists the intended commands).

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
