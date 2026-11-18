# 函数索引

每个 Go 包与运行时模块一行：其职责及应先阅读的声明。声明的文档注释中带有函数说明层（第一句：用途；非平凡时再写输入、
返回值、副作用和说明）。

| 包 / 模块 | 职责 | 从这里开始 |
|---|---|---|
| `cmd/windows-arm64-apk` | 界面的入口，对接 `winapk.Mock` | `main` |
| `internal/gui` | 界面（[gui.md](../gui.md)）：主窗口、窗格和组、应用窗口、面板、设置、诊断 | `App`、`New`、`view`、`launch`、`group`、`showTab` |
| `internal/winapk` | 界面对运行时的期望（契约草案）及 Mock | `Client`、`Process`、`Report`、`APK`、`State`、`Mock` |
| `locales` | 界面的 ARB 目录（en、zh-Hans）及生成的访问函数 | `en.json` |
| `macapk/runtime/platform/windows` | Windows ARM64 宿主：`winapk.exe`（`doctor`、`inspect`、`run`、`dex`）、APK/ARSC/ELF 静态读取器、JVM 和 Vulkan 探测、`windows.cmake`、`vcpkg.json`、`winapk.manifest` | `wmain`、`Run`、`ConvertDex`、`StageCode`、`BuildId`、`InspectApk`、`ResolveResources`、`InspectArm64Elf`、`FindJvm`、`CreateJvm`、`ProbeVulkan` |
| `macapk/runtime/platform/windows/src/bridge` | `winapk-bridge.dll`：Java 宿主 native、窗口/输入/呈现、SQLite 及进程私有受限客体命名空间 | `winapk_bridge_register`、`RegisterGuest`、`GuestRuntime`、`Open`、`BindExports`、`RunOnUi`、`PresentBitmap` |
| `macapk/runtime/java/com/macapk/loader/GuestNativeLoader.java` | 仅 Windows 的客体调用点服务及应用类导出绑定 hook；宿主/JDK 加载不变 | `loadLibrary`、`load`、`defined`、`bind`、`openNative` |
| `macapk/runtime/guest/loader` | N0/P3 ELF 依赖图、逻辑提供方、导入版本及故障范围；JNI 自身映像 hook 查询及命名空间执行准入（[native-profiles.md](../native-profiles.md)） | `Load`、`ModuleSet::Open`、`ModuleSet::FindOwn`、`ModuleSet::Executable`、`BuiltinLibrary`、`GuestFault` |
| `macapk/runtime/guest/bionic` | 宿主线程/TLS、持久宿主线程绑定、同步、内存、文件/时间/dl/日志与客体 CRT 生命周期；Android `format.c` 及辅助库/提供方所有权（[native-profiles.md](../native-profiles.md)） | `ThreadRuntime`、`ThreadBinding`、`GuestThreadContext`、`CxaFinalize`、`BionicLibraries::Load`、`vsnprintf` |
| `macapk/runtime/guest/jni` | P4 私有表、含展开/GFID 元数据的静态 PE 入口、持久上下文、供探测及受限 APK 使用的 JNI 库生命周期和导出绑定（[native-profiles.md](../native-profiles.md)） | `JniBridge::OpenLibrary`、`InitializeLibrary`、`JniBridge::BindExports`、`Mangle`、`Register`、`winapk_jni_dispatch` |
| `macapk/runtime/platform/windows/src/native_probe` | `native-probe.exe`：`load`、`suite`（T05）、`stress`、`modules`/`libraries`（T07）、`threads`、`sync`、`memory`、`io`、`faults`、`format`（T08），ABI/生命周期断言 | `wmain`、`RunSuite`、`CallPreserved`、`Stress`、`ProbeBionic`、`ProbeModules`、`ProbeLibraries` |
| `macapk/runtime/platform/windows/src/jni_probe.cpp` | 独立真实 JVM 的 T09 基础及致命边界（单独不是 G3）；隔离 loader、CheckJNI、运行中 CFG/GFID 准入及代理清理 | `wmain`、`Run`、`HostState`、`NativeEntryTargets`、`GuestJniProbe` |
| `scripts/windows` | 宿主/Java/G2/P3/T09/APK 检查、原生 APK 依赖配方及运行中进程 CFG 证据；提供 Bionic 定向子集 | `build.ps1`、`Build-Fixtures`、`Build-GuestBionic`、`Build-GuestJni`、`Test-Jni`、`Test-ApkJni`、`check-requirements.ps1` |
| `tests/windows-arm64/apk-fixtures` | 自有源码的 T03 Java、T04 Kotlin 及 T09/G3 JNI Activity 矩阵、依赖/版本库、嵌套/worker 回调及不支持 JNI 的按钮 | `MainActivity`、`WorkerCallbacks`、`JNI_OnLoad`、`nativeMatrix`、`nativeWorker`、`jni/fixtures.psd1` |
| `tests/windows-arm64/guest-elf` | 客体 ELF 源码：`n0`（T05）、`negative`（T06）、`modules`（T07）、`threads`（T08）、`jni`（T09 基础）；构建参数、依赖及期望结果/拒绝 | `fixtures.psd1`、`root_value`、`t08_case`、`t09_case` |
| `internal/version` | 产品版本 | `Version`、`Older` |
| `tools/checkdocs` | 文档镜像检查 | `main` |
| `build-lock.json` | 跟随的上游记录及锁定的后端依赖；ANGLE P5a 宿主路径分别验证 | `followed`、`host.angle.hostVerification`、`deferred` |
| `go.mod` | 前端/工具依赖；从 Go 通配符包发现中排除生成的后端下载目录 | `ignore ./build`、`tool`、`require` |
| `scripts/windows/angle-args.gn` | P5a ARM64 ANGLE DLL 配方：Vulkan 与 D3D11、系统 loader，不启用可选软件/WebGPU/OpenCL 后端 | `Build-Angle`、`target_cpu`、`angle_enable_vulkan`、`angle_use_custom_libvulkan` |
| `macapk/runtime/platform/windows/src/angle_probe.cpp` | 可选 T10 宿主 ANGLE 硬件实验、类型化设备标识、shader 回读/resize 及显式后端拒绝；不是 Java/客体 GLES | `Probe`、`Device`、`Shader`、`Draw`、`Test-GraphicsHost` |
| `macapk/runtime/platform/windows/src/bridge/graphics.cpp`、`android/opengl/GLSurfaceView.java` | T10 Java GLES20：私有 ANGLE DLL、宿主管理窗口 context、受限于当前帧的 renderer 回调及有界回读 | `JavaGraphics::Open`、`Begin`、`End`、`RegisterJavaGraphics`、`macapkWindowsDrawFrame`、`Test-GraphicsJava` |
| `macapk/runtime/platform/windows/src/bridge/surface.cpp` | 受限 `libandroid.so` 的窗口身份、挂载寿命和客户区尺寸 | `PublishSurface`、`ResizeSurface`、`RetireSurface`、`NativeWindowLibrary` |
| `macapk/runtime/platform/windows/src/bridge/guest_graphics.cpp` | T11 固定 Android EGL/GLES 入口、客体句柄归属、最小化/恢复及消息线程驱动分发 | `AddGuestGraphicsLibraries`、`MakeCurrent`、`EnsureSurface`、`Swap`、`GetProc`、`Test-GraphicsGuest` |
| `macapk/runtime/platform/windows/src/bridge/guest_vulkan.cpp` | T13 WSI、受限呈现、一致内存传输缓冲区回读及桥入口 PFN 类型检查 | `CreateSwapchain`、`CreateBuffer`、`FillBuffer`、`MapMemory`、`Present`、`CheckedEntry`、`Test-VulkanWsi` |
| `scripts/windows/package.ps1` | 从已有 ARM64 Release 产物离线打包，生成运行时与源码 ZIP 及本地依赖清单 | `Copy-Payload`、`Test-Package` |
| `macapk/runtime/platform/windows/src/run.cpp` | APK 启动、按内容寻址的 DEX 缓存及用于回收转换器的 Job Object | `Spawn`、`StageCode`、`ConvertDex`、`Test-DexJob` |
