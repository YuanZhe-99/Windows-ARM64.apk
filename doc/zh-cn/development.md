# 开发

## 环境要求

- `go.mod` 中写明的最新 Go 发行版（较旧的 Go 会自动下载它）。MyGo 是 Go 工具依赖（`go tool mygo …`），使用最新标签。
- 运行时的每次运行都需要一台 Windows 11 ARM64 机器；图形工作还需要桌面会话和带 ARM64 Vulkan 驱动的 GPU。
- 运行时（自 P0 起）：ARM64 OpenJDK、Android SDK 与 NDK、clang-cl 与 Windows SDK、CMake 和 Ninja。P0 选定其版本并锁定在
  `build-lock.json` 中；`scripts/windows/check-requirements.ps1` 只检查，不安装任何东西。

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

运行时构建、`winapk doctor` 与夹具测试套件随第一批运行时代码加入此处（预期命令见 `PLAN.md` §18）。

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
