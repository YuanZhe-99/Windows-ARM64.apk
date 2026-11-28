# 持续集成与发布

`.github/workflows/` 下的 GitHub Actions 工作流在 GitHub 的 Windows 11 ARM64 运行器
（`windows-11-arm`）上原生构建、验证并打包 **Windows-ARM64.apk**。

## 工作流

| 工作流 | 触发条件 | 运行器 | 用途 |
|---|---|---|---|
| `ci.yml` | 每次 `push`、`pull_request`、`workflow_dispatch` | `windows-11-arm` | 运行 Go/MyGo/i18n/文档检查，构建原生 ARM64 运行时与 Java/Skia/ELF 目标，并执行无头原生 ARM64 测试套件 |
| `build.yml` | 可复用（`workflow_call`） | `windows-11-arm` | 共享的构建、校验与可选发布产物打包逻辑 |
| `release.yml` | `v*` 标签 | `ubuntu-latest` + `windows-11-arm` | 校验标签与版本号一致，在 `windows-11-arm` 上构建并签名发布包，生成 `SHA256SUMS` 并创建 GitHub Release |

## 验证分层

GitHub 的 `windows-11-arm` 运行器以原生 ARM64 进程运行，但没有交互式桌面 GPU 会话；因此验证在 CI 与物理
Windows 11 ARM64 硬件之间明确分工：

| 检查 / 套件 | `windows-11-arm` CI | 物理 ARM64 桌面 + GPU |
|---|---|---|
| `gofmt`、`go vet`、`go test ./internal/version ./internal/gui`、`CGO_ENABLED=0`、`mygo vet`、`mygo i18n check`、`checkdocs` | 每次推送 / PR 执行 | 发布前执行 |
| `Build-Java`、`Build-FrameworkRes`、`Build-Angle`、`Build-Skia`、`Build-Fixtures`、`Build-Host`（`Debug` 与 `Release`）、`Build-GuestBionic`、`Build-GuestJni`、`Build-ElfFixtures` | 每次推送 / PR 执行 | 发布前执行 |
| `Host`、`Native`、`Bionic`、`Jni`、`DexJob` | 在 `windows-11-arm` 上原生执行 | 发布前执行 |
| `HostServices`、`Audio`（硬件 WASAPI）、`Agsl`（`T15` D3D12）、`Java`、`ApkJni`、`Surface`、`GraphicsHost`、`GraphicsJava`、`GraphicsGuest`、`VulkanWsi`、`Package`、`Stability`（`T14`） | 产物构建与 PE 架构审计（`Package` 清单） | 完整交互式窗口、GPU 与 30 分钟稳定性门禁（[platform-notes.md](platform-notes.md)） |

## 发布产物与 Ed25519 更新签名

当向 `YuanZhe-99/Windows-ARM64.apk` 推送与 `internal/version/version.go` 及 `mygo.json` 一致的
`vX.Y.Z` 标签时，`release.yml` 发布以下产物：

- `windows-arm64-apk-setup-<version>-arm64.exe` —— 单用户 NSIS 安装程序，经
  `scripts/windows/stage-app-resources.ps1` 一并打包 `Windows-ARM64.apk.exe` 与完整 Release
  `winapk` 运行时（`bin/`、`lib/`、`licenses/`、`LICENSE`、`THIRD_PARTY_NOTICES.md`）。
- `Windows-ARM64.apk-<version>-arm64.zip` —— 由 `scripts/windows/package.ps1` 生成的免安装便携包。
- `Windows-ARM64.apk-<version>-arm64-source.zip` —— 完整的 AGPL-3.0 对应源码快照（不含上游 macOS
  `.dylib` 预编译文件）。
- `windows-arm64-apk-<version>-windows-arm64.tar.gz` 与 `update-windows-arm64.json` —— 配置仓库密钥
  `MYGO_UPDATER_PRIVATE_KEY` 时由 `go tool mygo build` 生成的 Ed25519 签名更新归档与清单。已安装副本在用户于
  **设置 → 关于 → 更新** 中手动检查更新时，使用 `mygo.json` 中的 `updates.publicKey` 校验签名。
- `SHA256SUMS` —— 所有发布文件的 SHA-256 校验和。
