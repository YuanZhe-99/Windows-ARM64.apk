# 上游契约

Windows-ARM64.apk 依赖上游的哪些行为，以及每一点核实到了什么程度。Mac.apk、Go 与 MyGo 跟随各自最新版本，因此上游变化时
每一点都要重新核对。下列 Mac.apk 要点读自提交 `f87be03`（代码与 2026-11-03 的 `main` `367fd1a` 相同），未构建或运行任何东西。
subtree 已在 `367fd1a` 引入，下文提到的每个路径都存在；行为要点在 P0 的审计和构建给出结果前仍为“已阅读”。

## Mac.apk

| 要点 | 影响 | 核实程度 |
|---|---|---|
| Framework 与 DEX 转换器是 Java（`runtime/java/`、`runtime/java/com/macapk/dex/`） | 保持原样；只隔离平台调用和路径假设 | 已阅读 |
| `Boot.java` 的启动流程耦合 `NativeCocoa`、本地库和 macOS 路径 | 拆出宿主启动服务；保留应用生命周期 | 已阅读 |
| `NativeCocoa.java` 承载窗口、Surface、呈现和宿主服务的 JNI 方法 | 新增平台中立入口，调用逐步迁移过去 | 已阅读 |
| `runtime/src/jvm_loader.cpp` 以 macOS 加载假设嵌入 JVM | 需要 Windows `jvm.dll` 加载器及其自己的选项 | 已阅读 |
| `runtime/CMakeLists.txt` 在顶层启用 OBJCXX，并查找 Apple 与 Homebrew 依赖 | 语言、源文件、链接和依赖发现改为按平台条件化 | 已阅读 |
| `runtime/translator/src/arm64_native.cpp` 将 ELF 映射/重定位与 macOS 内存、寄存器和补丁机制交织 | 复用解析逻辑；分离宿主内存与执行策略 | 已阅读 |
| `arm64_pack.cpp` 与 `arm64_macho_emit.cpp` 把客体代码包装为 Mach-O | Windows 改用一个宿主 PE 桥和虚拟客体库注册表 | 已阅读 |
| `bionic_shim.cpp` 将 Bionic、系统调用和线程绑定到 Darwin 机制 | 按能力拆分，绝不整文件移植 | 已阅读 |
| `bionic_shim_vulkan.cpp` 将客体 Vulkan 适配到 MoltenVK 与 Metal Surface | 复用其扩展处理；新增 Win32 后端 | 已阅读 |
| `angle_glue.cpp` 与 `gles_bridge.cpp` 是 GLES/ANGLE 桥入口 | 分离宿主表面、后端选择和客体 ABI | 已阅读 |
| `NativeLoaderShim.java` 依赖与 ClassLoader 关联的宿主动态库 | 把 `.dylib` 改名为 `.dll` 不够；客体库经客体加载器处理 | 已阅读 |
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
