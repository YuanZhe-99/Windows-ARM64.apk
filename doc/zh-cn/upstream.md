# 上游契约

Windows-ARM64.apk 依赖上游的哪些行为，以及每一点核实到了什么程度。Mac.apk、Go 与 MyGo 跟随各自最新版本，因此上游变化时
每一点都要重新核对。下列 Mac.apk 要点在 `ff2afbf`（2026-11-03 合并）上成立；“已阅读”表示读过源码，“已构建”或“已验证”表示 P0 在
Windows 11 ARM64 上构建或运行过（[port-audit.md](port-audit.md)）。

## Mac.apk

| 要点 | 影响 | 核实程度 |
|---|---|---|
| Framework 与 DEX 转换器是 Java（`runtime/java/`、`runtime/java/com/macapk/dex/`） | 保持原样；只隔离平台调用和路径假设 | 已构建：在 Windows ARM64 JDK 上不经修改即可编译 |
| `Boot.run(apk, classesJar, appDir, manifest)` 是启动入口；调用线程运行 `NativeCocoa.runLoop()`，Activity 在自己的线程上运行；应用的数据目录以 `appDir` 的文件夹名命名；`appDir` 中不属于它的文件（`Boot.isCodeDirArtifact`，或以 `.macapk` 开头）会被移到数据目录 | `winapk run` 以包名命名 `appDir`，缓存戳命名为 `.macapk-winapk-stamp` | 已验证（G1） |
| `NativeCocoa.java` 承载窗口、Surface、呈现和宿主服务的 JNI 方法；帧由 UI 线程调用渲染器的 `macapkDrawFrame()` 拉取，它用 `presentBitmap` 呈现 ARGB 像素；触摸以点为单位、y 从底部起算交给 `TouchDispatcher.dispatch` | Windows 把同样的声明绑定到 Win32 代码（ADR-0002） | 已验证（G1） |
| `AppClassLoader.findLibrary` 转换 APK 的库，失败时退回空操作的 stub | guest native 关闭时，Windows 先抛出 `GUEST_NATIVE_DISABLED`（按平台条件生效的检查） | 已验证（G1.5） |
| `MACAPK_FW_RES_APK` 指定平台资源包；`-Dmacapk.android.root` 指定 Android 根目录；`-Dmacapk.display.density` 指定 density | 由 `winapk run` 设置 | 已验证（G1） |
| `MACAPK_BUILD_JAVA_ID`（来自 `build-id.txt`，由 `main.cpp` 导出）是 `dalvik.system.MacApkDexConverter` 缓存的键；没有它该缓存不会保留 | 由 `winapk run` 导出（ADR-0003） | 已设置（G1）；动态 DEX 路径尚无夹具覆盖 |
| `ActivityHost.macapkTaskToFront()` 把运行时移到后台的任务带回前台；其他情况下不做任何事 | 窗口从任务栏恢复时调用 | 已验证（G1.3） |
| `MACAPK_INJECT_FILE` 启动 `TouchDispatcher` 的输入驱动，它从该文件读取 `key`、`tap` 等行 | `test.ps1` 用它在 UI 线程被挂起时发送返回 | 已验证（G1.3） |
| `runtime/src/native_sqlite.cpp` 除 CoreFoundation 的大小写映射和排序外都是可移植的 C++ | 在 Windows 上构建，用 `_WIN32` 分支替换这两部分 | 已验证（T03 SQLite） |
| `runtime/src/jvm_loader.cpp` 以 macOS 加载假设嵌入 JVM | 需要 Windows `jvm.dll` 加载器及其自己的选项 | 已验证：Windows 加载器在 `platform/windows` 中，JDK 接受上游的模块选项（T01） |
| `runtime/CMakeLists.txt` 在顶层启用 OBJCXX，并查找 Apple 与 Homebrew 依赖 | 语言、源文件、链接和依赖发现改为按平台条件化 | 已构建：`if(WIN32)` 分支在 macOS 项目之前引入 `platform/windows/windows.cmake` |
| `runtime/translator/src/arm64_native.cpp` 将 ELF 映射/重定位与 macOS 内存、寄存器和补丁机制交织 | 复用解析逻辑；分离宿主内存与执行策略 | 已阅读 |
| `arm64_pack.cpp` 与 `arm64_macho_emit.cpp` 把客体代码包装为 Mach-O | Windows 改用一个宿主 PE 桥和虚拟客体库注册表 | 已阅读 |
| `bionic_shim.cpp` 将 Bionic、系统调用和线程绑定到 Darwin 机制 | 按能力拆分，绝不整文件移植 | 已阅读 |
| `bionic_shim_vulkan.cpp` 将客体 Vulkan 适配到 MoltenVK 与 Metal Surface | 复用其扩展处理；新增 Win32 后端 | 已阅读 |
| `angle_glue.cpp` 与 `gles_bridge.cpp` 是 GLES/ANGLE 桥入口 | 分离宿主表面、后端选择和客体 ABI | 已阅读 |
| `NativeLoaderShim.java` 依赖与 ClassLoader 关联的宿主动态库；应用自己调用的 `System.load(path)` 直接到达 JVM | 把 `.dylib` 改名为 `.dll` 不够；客体库经客体加载器处理，APK 库的 `System.load` 也必须转到那里（未完成，[port-audit.md](port-audit.md)） | 已阅读 |
| 基础 Canvas 是 Java2D；AGSL 与 RenderEffect 在 Skia 的 Metal 后端上执行并回读（`BUILDING.md`） | Canvas、GLES、Vulkan、AGSL 是独立工作项，不是一次 Metal 替换 | 已阅读 |
| 构建需要 macOS 26.4 的 `x18` ABI entitlement，并提交了 macOS 的 ANGLE、MoltenVK、Skia `.dylib` 预编译产物 | 这些在 Windows 上都不适用、不分发；依赖为 ARM64 Windows 构建或获取 | 已阅读 |
| 启动器是 Swift（`launcher/`），构建与发行脚本是 bash（`scripts/`、`runtime/scripts/`） | Windows 上由 MyGo 前端取代启动器；脚本步骤重新表达，而非改名 | 已阅读 |
| 代码以 AGPL-3.0 或商业许可授权；名称与标志保留（`COMMERCIAL.md`、`TRADEMARKS.md`） | 见 [third-party.md](third-party.md) | 已阅读 |

## Windows ARM64 ABI

| 要点 | 影响 | 核实程度 |
|---|---|---|
| `x18` 指向 TEB，`TPIDR_EL0` 保留 | 客体 TLS 和影子调用栈方案绝不使用它们；进入时不做交换 | 已阅读 |
| 数据模型为 LLP64：`long` 为 32 位 | 跨边界的客体 `long` 使用独立的 64 位类型 | 已阅读 |
| 展开使用专门的 `.pdata`/`.xdata` 元数据与 SEH | 客体 DWARF 展开与宿主 thunk 是两个不同的问题 | 已阅读 |

| JVM 只在 `System.loadLibrary` 加载的库中查找 `Java_*` native，不在嵌入它的程序中查找 | 宿主 native 用 `RegisterNatives` 注册（[decisions.md](decisions.md)） | 已验证（T01） |
| 在这台电脑上（Adreno 8cx Gen 3），Vulkan loader 只提供 Mesa Dozen，即转换到 Direct3D 12 的 Vulkan 1.2 | `doctor` 报告 `VULKAN_DRIVER_TRANSLATED`；图形关卡需要厂商驱动或一项已记录的决定 | 已验证（`doctor`） |

## Android NDK

| 要点 | 影响 | 核实程度 |
|---|---|---|
| Android ARM64 上 `x18` 保留给平台 | 仍逐个检查二进制；某个二进制未使用不证明普遍成立 | 已阅读 |
| ELF TLS 有多种访问 model 及 TLSDESC（AOSP `elf-tls.md`） | 每种 model 要么明确支持，要么明确拒绝 | 已阅读 |

## MyGo

| 要点 | 影响 | 核实程度 |
|---|---|---|
| MyGo 的发行说明支持 `windows/arm64` | 前端可以是原生 ARM64 | 已阅读；Go 模块在 Windows 11 ARM64 上构建并通过测试 |
| MyGo 是 Go 工具依赖（`go tool mygo …`） | 其版本随 `go.mod` 变化 | 已构建 |
