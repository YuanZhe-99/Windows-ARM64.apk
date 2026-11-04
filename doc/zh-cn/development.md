# 开发

## 环境要求

- `go.mod` 中写明的最新 Go 发行版（较旧的 Go 会自动下载它）。MyGo 是 Go 工具依赖（`go tool mygo …`），使用最新标签。
- 运行时的每次运行都需要一台 Windows 11 ARM64 机器；图形工作还需要桌面会话和带 ARM64 Vulkan 驱动的 GPU。
- 运行时：`build-lock.json` 锁定的内容。带 ARM64 C++ 工具、Clang、CMake 和 vcpkg 组件的 Visual Studio 2026
  提供 clang-cl、MSVC、Windows SDK、CMake、Ninja 和 vcpkg（都不在 PATH 上）。JDK 是适用于 ARM64 的
  Microsoft OpenJDK 21，用 `winget install --id Microsoft.OpenJDK.21 -e --architecture arm64` 安装（其 MSI
  设置系统 `JAVA_HOME` 和 PATH；构建时可用 `WINAPK_JAVA_HOME` 覆盖）。Android SDK（build-tools 36.0.0、
  android-36）和 NDK r30 从 P1 和 P2 起需要。`scripts/windows/check-requirements.ps1` 检查全部内容，不安装任何东西。

## 命令

```bash
gofmt -l cmd internal tools
go vet ./...
go test ./...
CGO_ENABLED=0 go test ./...
go tool mygo vet .
go tool mygo i18n check
go run ./tools/checkdocs           # doc/en-us and doc/zh-cn mirror each other
```

运行时的命令见下一节。

## 构建和测试运行时

```powershell
.\scripts\windows\check-requirements.ps1
.\scripts\windows\build.ps1 -Component Java
.\scripts\windows\build.ps1 -Component Host -Configuration Debug
.\scripts\windows\test.ps1 -Suite Host
.\build\windows-arm64\bin\winapk.exe doctor --json
.\build\windows-arm64\bin\winapk.exe doctor --present
```

`-Component Java` 把 Mac.apk 的 Java 侧构建到 `build/windows-arm64/lib`（见 [port-audit.md](port-audit.md)）；
`-Component Host` 在 Visual Studio 的 ARM64 开发者环境中用 clang-cl 把 `winapk.exe` 和 `winapk-probe.jar` 构建到
`build/windows-arm64/bin`，首次构建时通过 vcpkg 获取 Vulkan 头文件，生成的任何 PE 不是 ARM64 时即失败。
`test.ps1 -Suite Host` 就是夹具 T01。`doctor --present` 还会打开一个小测试窗口检查 Vulkan 呈现；不加时该项为 `not-run`。

## 运行界面

```bash
go run ./cmd/windows-arm64-apk
WINAPK_FRESH=1 go run ./cmd/windows-arm64-apk
SCREENSHOT_DIR=/path/to/dir go test ./internal/gui -run TestScreensRender
```

第一条对接 `design/` 的示例应用运行；`WINAPK_FRESH=1` 以空的应用库、没有 Java 运行时启动，如同首次
运行。测试把每个界面以两种语言和两种外观写成 PNG，用于与 `design/` 对比。字符串在 `locales/en.json`
和 `locales/zh-Hans.json` 中；修改后运行 `go tool mygo generate`。

## 更新 Mac.apk

Mac.apk 是 `macapk/` 下的 git subtree，跟随上游最新 `main`：

```bash
git ls-remote https://github.com/kksimp/Mac.apk main
git subtree pull --prefix=macapk https://github.com/kksimp/Mac.apk main --squash
```

首次引入使用参数相同的 `git subtree add`。每次合入后，重新核对 [upstream.md](upstream.md) 列出的入口，并重跑合入文件所涉及的关卡。

## 测试原则

测试保护产品契约和易错逻辑，优先使用表驱动；每个修复的缺陷都有回归测试。夹具自建且可公开，每个都有源码、构建参数、
哈希和期望输出，每项能力都有正例和负例。每个测试注明它覆盖的夹具编号（`T01`…）或关卡。执行、JVM 与 GPU 测试只在
ARM64 真机上计数；结果报告为 `PASS`、`FAIL`、`BLOCKED` 或 `NOT_RUN`。
