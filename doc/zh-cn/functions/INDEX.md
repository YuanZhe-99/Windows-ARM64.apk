# 函数索引

每个 Go 包与运行时模块一行：其职责及应先阅读的声明。声明的文档注释中带有函数说明层（第一句：用途；非平凡时再写输入、
返回值、副作用和说明）。

| 包 / 模块 | 职责 | 从这里开始 |
|---|---|---|
| `cmd/windows-arm64-apk` | 界面的入口，对接 `winapk.Mock` | `main` |
| `internal/gui` | 界面（[gui.md](../gui.md)）：主窗口、窗格和组、应用窗口、面板、设置、诊断 | `App`、`New`、`view`、`launch`、`group`、`showTab` |
| `internal/winapk` | 界面对运行时的期望（契约草案）及 Mock | `Client`、`Process`、`Report`、`APK`、`State`、`Mock` |
| `locales` | 界面的 ARB 目录（en、zh-Hans）及生成的访问函数 | `en.json` |
| `macapk/runtime/platform/windows` | Windows ARM64 宿主：`winapk.exe`（`doctor`、`inspect`、`run`、`dex`）、它的 APK 读取器、JVM 和 Vulkan 探测、`windows.cmake`、`vcpkg.json`、`winapk.manifest` | `wmain`、`Run`、`ConvertDex`、`StageCode`、`BuildId`、`InspectApk`、`FindJvm`、`CreateJvm`、`ProbeVulkan` |
| `macapk/runtime/platform/windows/src/bridge` | `winapk-bridge.dll`：Java 侧宿主 native 的 Windows 实现——`NativeLog`（`log.cpp`）、`NativeCocoa` 的窗口、呈现和输入（`window.cpp`），以及上游的 `NativeSqlite`（`runtime/src/native_sqlite.cpp`） | `winapk_bridge_register`、`Bind`、`RunOnUi`、`Init`、`RunLoop`、`Tick`、`PresentBitmap`、`Touch`、`Key`、`Text`、`TaskToFront` |
| `macapk/runtime/guest/loader` | N0 客体 ELF 加载器（[native-profiles.md](../native-profiles.md)）：检查、映射、筛查、重定位并保护 Android AArch64 共享库，而不运行它；宿主代码 | `Load`、`Module::Find`、`LoadError`、`CategoryName`、`Loader::Screen`、`Loader::Relocate`、`Loader::Protect` |
| `macapk/runtime/platform/windows/src/native_probe` | `native-probe.exe`（`PLAN.md` §9.1）：`load`、`suite`（T05）和 `stress`，ABI 用例，被调用方保存寄存器的检查 | `wmain`、`Cases`、`RunSuite`、`CallPreserved`、`CheckHostState`、`Stress` |
| `scripts/windows` | 环境检查、构建（Java、FrameworkRes、Fixtures、Host、NativeProbe）和测试套件（Host = T01，Java = G1，Native = G2） | `build.ps1`、`test.ps1`、`check-requirements.ps1`、`common.ps1` |
| `tests/windows-arm64/apk-fixtures` | 夹具 APK 的源码：`hello-java`（T03）、`hello-kotlin`（T04，G1.6）、`hello-jni`（G1.5，之后用于 T09） | 各自的 `MainActivity` |
| `tests/windows-arm64/guest-elf` | 客体 ELF 夹具的源码：`n0/abi.c`（T05）、`negative/negative.c`（T06），以及记录每个夹具构建方式和预期拒绝的 `fixtures.psd1` | `fixtures.psd1` |
| `internal/version` | 产品版本 | `Version`、`Older` |
| `tools/checkdocs` | 文档镜像检查 | `main` |
