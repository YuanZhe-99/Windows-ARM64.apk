# 平台说明

| 平台 | 状态 |
|---|---|
| Windows 11 ARM64 | 仅脚手架：Go 检查通过（见下）。尚无运行时和前端。 |
| Windows 11 x64 | 不是目标平台；只可承载构建工具和解析测试。 |
| macOS（Apple Silicon） | 上游 Mac.apk 的平台；已在 `367fd1a` 引入为 `macapk/`；尚未在此运行 macOS 构建或 smoke（`NOT_RUN`）。 |

## Windows

在 Windows 11 ARM64 上验证（2026-11-03；Go 1.27.3）：

| 检查 | 结果 |
|---|---|
| Go 模块 | `go vet ./...` 与 `go test ./...` 通过 |
| 文档镜像 | `go run ./tools/checkdocs` 通过 |

## 关卡

| 关卡 | 状态 |
|---|---|
| G0 宿主就绪 | NOT_RUN |
| G1 Java MVP | NOT_RUN |
| G2 Native 最小执行 | NOT_RUN |
| G3 Native 集成 | NOT_RUN |
| G4 图形 | NOT_RUN |
| G5 实验版 | NOT_RUN |

## 已知限制

- Mac.apk 已原样引入 `macapk/`；尚无 Windows 运行时代码。
- 在运行时于 ARM64 真机上运行之前，不能声明原生 ARM64 支持。
