# 函数索引

每个 Go 包与运行时模块一行：其职责及应先阅读的声明。声明的文档注释中带有函数说明层（第一句：用途；非平凡时再写输入、
返回值、副作用和说明）。

| 包 / 模块 | 职责 | 从这里开始 |
|---|---|---|
| `cmd/windows-arm64-apk` | 界面的入口，对接 `winapk.Mock` | `main` |
| `internal/gui` | 界面（[gui.md](../gui.md)）：主窗口、窗格和组、应用窗口、面板、设置、诊断 | `App`、`New`、`view`、`launch`、`group`、`showTab` |
| `internal/winapk` | 界面对运行时的期望（契约草案）及 Mock | `Client`、`Process`、`Report`、`APK`、`State`、`Mock` |
| `locales` | 界面的 ARB 目录（en、zh-Hans）及生成的访问函数 | `en.json` |
| `macapk/runtime/platform/windows` | Windows ARM64 宿主：`winapk.exe`（`doctor`、`inspect`、`run`）、它的 APK 读取器、JVM 和 Vulkan 探测、`windows.cmake`、`vcpkg.json` | `wmain`、`Run`、`StageCode`、`InspectApk`、`FindJvm`、`CreateJvm`、`ProbeVulkan` |
| `macapk/runtime/platform/windows/src/bridge` | `winapk-bridge.dll`：Java 侧宿主 native 的 Windows 实现——`NativeLog`（`log.cpp`），以及 `NativeCocoa` 的窗口、呈现和输入（`window.cpp`） | `winapk_bridge_register`、`Bind`、`Init`、`RunLoop`、`Tick`、`PresentBitmap`、`Touch`、`Key` |
| `scripts/windows` | 环境检查、构建（Java、FrameworkRes、Fixtures、Host）和测试套件（Host = T01，Java = G1） | `build.ps1`、`test.ps1`、`check-requirements.ps1`、`common.ps1` |
| `tests/windows-arm64/apk-fixtures` | 夹具 APK 的源码：`hello-java`（T03）、`hello-jni`（G1.5，之后用于 T09） | 各自的 `MainActivity` |
| `internal/version` | 产品版本 | `Version`、`Older` |
| `tools/checkdocs` | 文档镜像检查 | `main` |
