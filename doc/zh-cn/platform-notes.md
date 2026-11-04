# 平台说明

| 平台 | 状态 |
|---|---|
| Windows 11 ARM64 | G0 通过：Java 侧可以构建，`winapk.exe` 能嵌入 ARM64 JVM（T01）。界面对接示例数据（`winapk.Mock`）运行；还没有 APK 能运行。 |
| Windows 11 x64 | 不是目标平台；只可承载构建工具和解析测试。 |
| macOS（Apple Silicon） | 上游 Mac.apk 的平台；已在 `367fd1a` 引入为 `macapk/`；尚未在此运行 macOS 构建或 smoke（`NOT_RUN`）。 |

## Windows

在 Windows 11 ARM64 上验证（2026-11-03）：build 26300.9711，Snapdragon Compute Platform（8cx Gen 3），32 GB，Qualcomm Adreno 8cx Gen 3（驱动 30.0.4122.4000），Vulkan loader 1.3.301，只有 Mesa Dozen 设备；Go 1.27.3、Microsoft OpenJDK 21.0.13 ARM64、clang-cl 22.1.3。

| 检查 | 结果 |
|---|---|
| Go 模块 | `go vet ./...` 与 `go test ./...` 通过 |
| 文档镜像 | `go run ./tools/checkdocs` 通过 |
| 环境要求 | `scripts/windows/check-requirements.ps1`：每个锁定的工具均为 OK；Android SDK 和 NDK 尚未安装（P1/P2 起需要） |
| Java 侧（Mac.apk `ff2afbf`） | `build.ps1 -Component Java`：不经修改即可编译，78 秒，没有错误（[port-audit.md](port-audit.md)） |
| T01 宿主和 JVM | `test.ps1 -Suite Host`：17 项检查 PASS——`winapk.exe` 为 ARM64；`jvm.dll` 为 ARM64，在进程内以上游的模块选项创建 JVM；JNI 宿主 → Java → 宿主；Java2D；260 个字体族；补丁模块；以及负例（找不到 JVM、无效的 `--java-home`） |
| Vulkan | ARM64 loader 可用；两个 Mesa Dozen 设备（基于 Direct3D 12 的 Vulkan 1.2）；`doctor --present`：Win32 surface 测试通过。没有厂商 Vulkan 驱动 |
| 界面 | `go tool mygo vet .` 与 `go tool mygo i18n check` 通过；`design/` 的每个界面在 `ui.Tester` 中以两种语言和两种外观渲染；在 `ui.Tester` 中用指针拖动的窗格会拆分目标窗格；原生窗口能打开并保持运行（smoke 运行，示例数据） |

## 关卡

| 关卡 | 状态 |
|---|---|
| G0 宿主就绪 | PASS（2026-11-03，本机：T01，工具链已锁定） |
| G1 Java MVP | NOT_RUN |
| G2 Native 最小执行 | NOT_RUN |
| G3 Native 集成 | NOT_RUN |
| G4 图形 | NOT_RUN |
| G5 实验版 | NOT_RUN |

## 已知限制

- 到目前为止，Mac.apk 唯一的 Windows 改动是 `runtime/CMakeLists.txt` 的 `if(WIN32)` 分支；Windows 宿主只有
  `doctor`（P1 之前没有 APK 能运行）。
- 这台电脑没有厂商 Vulkan 驱动：Vulkan 是基于 Direct3D 12 的 Mesa Dozen（[decisions.md](decisions.md)）。
- `CMakeLists.txt` 改动后的 macOS 构建：NOT_RUN（这里没有 Mac）；该分支只在 `WIN32` 下返回。
- 界面只显示示例数据。在运行时契约提供之前尚未完成：窗格或窗口中的应用画面、移动应用数据文件夹的内容、数据和转换后代码的大小，以及更新私有 Java 运行时（[gui.md](gui.md)）。
- 在运行时于 ARM64 真机上运行之前，不能声明原生 ARM64 支持。
