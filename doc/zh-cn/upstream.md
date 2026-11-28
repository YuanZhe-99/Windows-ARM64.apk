# 上游契约

Windows-ARM64.apk 依赖上游的哪些行为，以及每一点核实到了什么程度。Mac.apk、Go 与 MyGo 跟随各自最新版本，因此上游变化时
每一点都要重新核对。下列 Mac.apk 要点在 `93c0351`（2026-11-27 合并）上成立；“已阅读”表示读过源码，“已构建”或“已验证”表示 P0 在
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
| `runtime/translator/src/arm64_native.cpp` 将 ELF 映射/重定位与 macOS 内存、寄存器和补丁机制交织 | N0 加载器只复用 ELF 记录布局（`elf64_reader.h`）；使用 `x18`、`TPIDR_EL0` 或 `svc` 的代码被拒绝而不是打补丁 | 已阅读；布局已构建（G2） |
| `runtime/scripts/build.sh` 把 `thirdparty/apache-http-legacy/org.apache.http.legacy.jvm.jar` 放到框架的 classpath 上，并解包进 `android-framework.jar`，绝不覆盖已编译的类 | `build.ps1 -Component Java` 同样处理 | 已构建（`3f35611`） |
| `main.cpp` 把应用的文件夹命名为 `<package>_<version>`，并去掉版本号里的点（`dir_naming.h`） | `winapk run` 只用包名命名，因此应用路径中不出现版本号的点 | 已阅读 |
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
| `x18` 指向 TEB，`TPIDR_EL0` 保留 | 客体 TLS 和影子调用栈方案绝不使用它们；进入时不做交换 | 已验证：每次客体回调中 x18 都是 TEB（T05 `host.state`） |
| 固定签名调用遵循 AAPCS64（寄存器、栈槽、HFA、`x8`、被调用方保存的寄存器）；Windows 可变参数调用把浮点放在整数寄存器中 | 固定签名直通；绝不共享宿主变参/va_list，经审计的可选参数需要显式入口（[abi-compatibility.md](abi-compatibility.md)） | 固定签名（T05）、纯整数可选 mode（T08 io）已验证；可变参数的差异已阅读 |
| 数据模型为 LLP64：`long` 为 32 位 | 跨边界的客体 `long` 使用独立的 64 位类型 | 已阅读 |
| 展开使用专门的 `.pdata`/`.xdata` 元数据与 SEH | 客体 DWARF 展开与宿主 thunk 是两个不同的问题 | 已阅读 |
| [CFG](https://learn.microsoft.com/en-us/windows/win32/secbp/control-flow-guard) 需要编译器/链接器插桩与有效目标元数据；VirtualAlloc/VirtualProtect 的可执行页默认是有效目标 | PE JNI 入口提供显式 GFID；检查 Guard/CF Instrumented/FID table 标志及真实进程策略；客体跳转仍显式筛查，不是沙箱 | Debug `/guard:cf` Host/Java/Native/Bionic/Jni/ApkJni PASS；运行策略启用，已加载 GFID 表包含全部 64 个 PE 入口 |
| [精确 unbiased interrupt time](https://learn.microsoft.com/en-us/windows/win32/api/realtimeapiset/nf-realtimeapiset-queryunbiasedinterrupttimeprecise) 单位为 100 ns，不计挂起；SDK 导入库为 mincore | 显式 timespec 转换及相对 monotonic 期限；不使用墙钟 sleep | 已阅读；T08 io 检查单位、墙钟 epoch 及最短 sleep 期限；挂起 NOT_RUN |
| [WriteFile](https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-writefile) 的两个 OVERLAPPED offset 均为 `0xFFFFFFFF` 时写入 EOF；同步句柄等待并更新文件指针 | 追加不使用先 seek 后 write，不向客体暴露 OVERLAPPED 对象 | 已阅读；T08 io 验证 seek 到开头后追加 |
| [AddVectoredExceptionHandler](https://learn.microsoft.com/en-us/windows/win32/api/errhandlingapi/nf-errhandlingapi-addvectoredexceptionhandler) 的 First=1 先于现有处理器；未知故障必须继续搜索 | 只处理已注册客体可执行 PC；JVM/JIT 及所有其他 PC 仍走正常链；不替换 HotSpot 处理器 | T08 宿主 SEH 及 T09 Java/JIT 异常、客体 trap：PASS；First=0 在锁定的 Microsoft JVM 中未收到客体 trap，由 JVM 退出 1 |

| JVM 只在 `System.loadLibrary` 加载的库中查找 `Java_*` native，不在嵌入它的程序中查找 | 宿主 native 用 `RegisterNatives` 注册（[decisions.md](decisions.md)） | 已验证（T01） |
| 在这台电脑上（Adreno 8cx Gen 3），Vulkan loader 只提供 Mesa Dozen，即转换到 Direct3D 12 的 Vulkan 1.2 | `doctor` 报告 `VULKAN_DRIVER_TRANSLATED`；图形关卡需要厂商驱动或一项已记录的决定 | 已验证（`doctor`） |

## Android NDK

| 要点 | 影响 | 核实程度 |
|---|---|---|
| Android ARM64 上 `x18` 保留给平台 | 仍逐个检查二进制；某个二进制未使用不证明普遍成立 | 已对 T05 的构建验证：筛查未在其中发现 `x18` |
| NDK r30 以 16 KiB 的最大页大小链接，并在 API 30 下把相对重定位压缩为 `DT_ANDROID_RELR` | 段只有在权限相同时才共用 4 KiB 的宿主页；RELR 会被应用 | 已验证（T05 构建） |
| ELF TLS 有多种访问 model 及 TLSDESC（AOSP `elf-tls.md`） | 每种 model 要么明确支持，要么明确拒绝 | 已阅读 |
| Bionic LP64 `pthread_t` 是 64 位 `long`，key 为有符号 32 位；LLVM emutls 描述符包含大小/对齐/object/初始值字段 | 使用显式客体类型和受管理 resolver，绝不用 Windows `long`、CRT pthread 对象或硬件线程指针切换 | 已阅读 NDK r30 头文件与 compiler-rt `emutls.c`；T08 验证 C 仿真 TLS 初始值/对齐及线程/key 语义 |
| NDK LP64 mutex/condition 布局为 40/48 字节，对齐 4；静态类型/时钟位在 `pthread.h` 中；Bionic non-PI 递归计数为 11 位的深度减一 | 客体大小标记与宿主对象表，绝不复制宿主锁；不支持的属性/时钟明确失败 | 已阅读 NDK r30 头文件及 AOSP `pthread_mutex.cpp` / `pthread_cond.cpp`；T08 `sync` 覆盖已声明子集 |
| AArch64 stat 为 128 字节，timespec 为两个有符号 64 位字段；open/open64 有可选整数 mode | 显式客体布局及三寄存器可选 mode 入口；绝不传递宿主 stat、long 或 va_list | 已阅读 NDK r30 `sys/stat.h`、`bits/timespec.h`、`fcntl.h`；T08 io 检查真实声明、哨兵范围、数值及带/不带 mode 调用 |
| 普通链接的 API-30 NDK C 共享库需要 libc/libdl/liblog 及 `LIBC` 版本；CRT 启动文件还导入 `__cxa_atexit`、`__cxa_finalize`、`__register_atfork` | 声明显式逻辑提供方，验证版本需求，实现真实生命周期记录，绝不使用返回成功的空桩 | 已检查 NDK r30 普通链接及 llvm-readelf（2026-11-17）；T07 N1 运行所记录的源码/链接及回调 |
| [DSO 对象析构 ABI](https://itanium-cxx-abi.github.io/cxx-abi/abi.html#dso-dtor) 注册函数/参数/DSO，逆序 finalization，可选单个 DSO 或全部，同一记录不得调用两次 | 独立客体注册表，在回调前移除记录，正常销毁运行时时在代码/堆仍有效时处理剩余记录 | 已阅读规范；T07 N1 验证选择、重入/仅一次及退出清理；不证明客体 C++ 异常支持 |

## JNI

| 要点 | 影响 | 已检查 |
|---|---|---|
| [JNI 函数](https://docs.oracle.com/en/java/javase/21/docs/specs/jni/functions.html)：稳定槽号、每线程环境、不透明引用/ID；jvalue 是 JNI 原生类型/引用的 union | 显式私有表、固定签名包装器及线程归属检查；绝不复制 JVM 表 | 已阅读规范；NDK/宿主静态布局断言及 T09 真实 JVM 调用、引用和误用用例 |
| FindClass 使用原生库的 loader 并初始化类；查询缺失报告 NoClassDefFoundError | 使用带初始化的 Class.forName 及配置的 loader，仅转换 ClassNotFoundException；worker 使用同一 loader | T09 隔离 URLClassLoader、FindClass 副作用及缺失类 |
| .../V 方法调用使用 C 默认参数提升；A 调用接受 jvalue 数组 | 客体按记录的方法签名 va_copy/va_arg，然后进入固定宿主 A 入口 | T09 混合寄存器/栈值、重复 V、构造及虚/非虚分派 |
| JNI native 符号查询先尝试 short name，再尝试带参数描述符编码的 long name；名称转义 UTF-16 单元，歧义转义失败 | 编码下划线、数组/对象描述符及 Unicode；拒绝开头或映射的斜杠后未经转义的 `0`–`3`；不替换已记录的注册 | T09 short/int-long 重载/Unicode/依赖导出、显式注册优先及 JVM 专属 `1bad` 查询拒绝 |
| JNI_OnLoad 是 Java 库初始化 hook，不是 ELF 依赖构造 | 只调用显式根库自身导出一次，检查返回版本及 pending exception；保留失败映像但拒绝其注册的 native 入口 | T09 普通 NDK 根/依赖、无 hook 根库、错误版本、pending exception、数据 hook 及失败后入口拒绝 |

## MyGo

| 要点 | 影响 | 核实程度 |
|---|---|---|
| MyGo 的发行说明支持 `windows/arm64` | 前端可以是原生 ARM64 | 已阅读；Go 模块在 Windows 11 ARM64 上构建并通过测试 |
| MyGo 是 Go 工具依赖（`go tool mygo …`） | 其版本随 `go.mod` 变化 | 已构建 |
