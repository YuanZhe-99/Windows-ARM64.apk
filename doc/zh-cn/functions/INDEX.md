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
| `macapk/runtime/platform/windows/src/bridge` | `winapk-bridge.dll`：Java 宿主 native、窗口/输入/呈现、SQLite 及进程私有受限客体命名空间（P4 集成进行中） | `winapk_bridge_register`、`RegisterGuest`、`GuestRuntime`、`Open`、`BindExports`、`RunOnUi`、`PresentBitmap` |
| `macapk/runtime/java/com/macapk/loader/GuestNativeLoader.java` | 仅 Windows 的客体调用点服务及应用类导出绑定 hook；宿主/JDK 加载不变 | `loadLibrary`、`load`、`defined`、`bind`、`openNative` |
| `macapk/runtime/guest/loader` | N0/P3 ELF 依赖图、逻辑提供方、导入版本及故障范围；JNI 自身映像 hook 查询及命名空间执行准入（[native-profiles.md](../native-profiles.md)） | `Load`、`ModuleSet::Open`、`ModuleSet::FindOwn`、`ModuleSet::Executable`、`BuiltinLibrary`、`GuestFault` |
| `macapk/runtime/guest/bionic` | 宿主线程/TLS、持久宿主线程绑定、同步、内存、文件/时间/dl/日志与客体 CRT 生命周期；Android `format.c` 及辅助库/提供方所有权（[native-profiles.md](../native-profiles.md)） | `ThreadRuntime`、`ThreadBinding`、`GuestThreadContext`、`CxaFinalize`、`BionicLibraries::Load`、`vsnprintf` |
| `macapk/runtime/guest/jni` | P4 私有表、静态 PE native 入口、持久上下文、显式 JNI 库生命周期及 short/long 导出绑定；T09，不是 APK 准入（[native-profiles.md](../native-profiles.md)） | `JniBridge::OpenLibrary`、`InitializeLibrary`、`JniBridge::BindExports`、`Mangle`、`Register`、`winapk_jni_dispatch` |
| `macapk/runtime/platform/windows/src/native_probe` | `native-probe.exe`：`load`、`suite`（T05）、`stress`、`modules`/`libraries`（T07）、`threads`、`sync`、`memory`、`io`、`faults`、`format`（T08），ABI/生命周期断言 | `wmain`、`RunSuite`、`CallPreserved`、`Stress`、`ProbeBionic`、`ProbeModules`、`ProbeLibraries` |
| `macapk/runtime/platform/windows/src/jni_probe.cpp` | 独立真实 JVM 的 T09 JNI 基础及致命边界探测，不是 G3；隔离 Java loader、CheckJNI、宿主状态及代理清理断言 | `wmain`、`Run`、`HostState`、`GuestJniProbe` |
| `scripts/windows` | 环境检查、Java/宿主/客体辅助库/夹具构建及测试（Host = T01，Java = G1，Native = G2，Bionic = T07/T08，Jni = T09 基础；提供 Bionic 定向子集） | `build.ps1`、`Build-GuestBionic`、`Build-GuestJni`、`test.ps1`、`Test-Bionic`、`Test-Jni`、`check-requirements.ps1` |
| `tests/windows-arm64/apk-fixtures` | 夹具 APK 的源码：`hello-java`（T03）、`hello-kotlin`（T04，G1.6）、`hello-jni`（G1.5，之后用于 T09） | 各自的 `MainActivity` |
| `tests/windows-arm64/guest-elf` | 客体 ELF 源码：`n0`（T05）、`negative`（T06）、`modules`（T07）、`threads`（T08）、`jni`（T09 基础）；构建参数、依赖及期望结果/拒绝 | `fixtures.psd1`、`root_value`、`t08_case`、`t09_case` |
| `internal/version` | 产品版本 | `Version`、`Older` |
| `tools/checkdocs` | 文档镜像检查 | `main` |
