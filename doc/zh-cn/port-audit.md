# 移植审计

Mac.apk 中哪些部分与 macOS 绑定、在 Windows ARM64 上如何处理、在哪个阶段处理。审计基于 Mac.apk
`ff2afbf`（2026-11-03）并在 `3f35611`（2026-11-16，上游 v1.0.2379 同步）上复核，以 `macapk/` subtree 合入，在 Windows 11 ARM64（build 26300）上进行。每次合并
Mac.apk 后重新执行下面的搜索并更新各表。

## 方法

- 阅读：`BUILDING.md`、`runtime/CMakeLists.txt`、`runtime/translator/CMakeLists.txt`、`runtime/scripts/build.sh`、
  `scripts/build_framework_res.sh`、`debug/build_java_only.sh`、
  `runtime/src/jvm_loader.cpp` 的 JVM 选项、`runtime/java` 的 native 方法声明，以及 `runtime/src` 和
  `runtime/translator/src` 的文件列表与 include。
- 在 `macapk/` 中搜索 `PLAN.md` §7 列出的平台耦合（计数见下）。
- 构建：用 Windows ARM64 JDK 构建整个 Java 侧（`scripts/windows/build.ps1 -Component Java`）；嵌入 JVM
  的 Windows 宿主（`-Component Host`，夹具 T01）。
- P1：AOSP 框架资源（用 SDK 的 `aapt2` 构建），以及纯 Java APK 的 Java 路径（见下）。
- P2：为客体加载器阅读了 `runtime/translator/include/macapk/elf64_reader.h`、`elf_reader.h`、`elf64_reader.cpp` 以及新增的
  `guest_rwx.h` 和 `guest_jit_pages.h`（见下）。

## 各部分

| 部分 | 上游路径 | 规模 | macOS 耦合 | Windows 处理 | 阶段 |
|---|---|---|---|---|---|
| Android 框架 | `runtime/java/android`、`androidx`、`dalvik`、`javax`、`libcore`、`org` | 3,017 个 Java 文件，59.3 万行（含 `runtime/java` 其余部分） | 26 个文件调用 `NativeCocoa`；字体和存储代码写有 macOS 路径 | 原样复用；调用逐步改到平台中立的宿主接口 | P1 |
| DEX 转换器 | `runtime/java/com/macapk/dex` | 含在上面 | `AndroidSdk.java` 在 macOS 和 Linux 的位置查找 SDK | 复用；SDK 查找增加 Windows 位置 | P1 |
| 启动与加载 | `runtime/java/com/macapk/loader`（`Boot.java`、`NativeCocoa.java` 等） | `NativeCocoa` 有 57 个 native | 窗口、Surface、输入、宿主服务经 Cocoa JNI；macOS 路径 | 宿主启动服务和 `HostWindow`/`HostEventLoop`/`HostFramePresenter`（`PLAN.md` §4.3） | P1 |
| JNI 宿主 | `runtime/src/*.cpp` | 19 个文件，2.2 万行 | `dlfcn`、`mach`、`sys/*`（见下） | 逐文件：POSIX 调用在平台接口后换成 Windows 实现 | P1–P4 |
| macOS 宿主服务 | `runtime/src/*.mm` | 26 个文件，3.2 万行（`cocoa_bridge.mm` 1.4 万） | Objective-C++、AppKit、AVFoundation、Metal、VideoToolbox | 不在 Windows 上构建；只在某个阶段需要该服务时替换 | P1、P8 |
| 客体原生 | `runtime/translator` | 34 个文件，15.2 万行（`bionic_shim.cpp` 6.9 万、`arm64_native.cpp` 1.6 万） | Mach 内存、`x18`/`TPIDR_EL0` 补丁、Mach-O 包装、Darwin 信号 | 复用 ELF 记录布局；N0 加载器依据 program header 映射（P2，见下）；内存、执行、TLS 和 Bionic 按能力重建（`PLAN.md` §5） | P2（N0 已完成）–P4 |
| 图形 | `runtime/src/angle_glue.cpp`、`gles_bridge.cpp`、`egl14/gles*_bridges.inc`、`translator/src/bionic_shim_vulkan.cpp` | 6 千行 | Metal 上的 ANGLE、MoltenVK | Windows ARM64 的 ANGLE（Vulkan、Direct3D 11）；客体 Vulkan 到 Windows loader | P5、P6 |
| Skia | `thirdparty/skia-prebuilt`、`skia_metal_bridge.mm` | 预编译 `.dylib` | Metal | 以后：Windows ARM64 的 Skia GPU 桥 | P8 |
| 构建 | `runtime/CMakeLists.txt`、`runtime/scripts/build.sh`、`scripts/*.sh` | 835 行的构建脚本 | OBJCXX、Homebrew、`codesign`、entitlement、`.dylib` 暂存、`python3` | `scripts/windows/*.ps1`；`CMakeLists.txt` 分支到 `platform/windows/windows.cmake` | P0（Java 和 Host 已完成） |
| Translator 构建 | `runtime/translator/CMakeLists.txt` | 2 个目标 | `macapk-translator` 把 ELF `.so` 转成 Mach-O `.dylib`；`libmacapk-bionic-shim.dylib` 链接 capstone、zlib、libpng、ANGLE 以及 Security 和 CoreFoundation 框架 | 不在 Windows 上构建：P2 的客体加载器自己映射 ELF（不做 Mach-O 或 PE 包装）；加载器的筛查使用 vcpkg 提供的 Capstone 5.0.9 | P2 |
| 框架资源 | `scripts/build_framework_res.sh` | 一次性 | 需要 `aapt2`（SDK build-tools 或 apktool 的 macOS 二进制）和约 20 MB 的 AOSP 源码包；生成约 48 MB 的 `framework-res-api36.apk`，从不提交 | 已完成：在 `build.ps1 -Component FrameworkRes` 中用 SDK 的 `aapt2`（build-tools 36.0.0）执行同样的步骤；`resources.arsc` 保持 stored、不再压缩，所以包为 66 MB | P1 |
| 启动器 | `launcher/*.swift` | 8 个文件 | SwiftUI 应用包 | 由 MyGo 前端取代 | 已完成（前端） |
| 预编译产物 | `thirdparty/angle-prebuilt`、`skia-prebuilt`、MoltenVK | macOS 二进制 | 仅 macOS | 绝不随 Windows 包发布 | — |

## 耦合计数

各模式匹配的行数（`grep -c` 求和），基于 `3f35611`。

| 模式 | `runtime/src` | `runtime/translator` | `runtime/java` | `launcher` | `scripts` |
|---|---|---|---|---|---|
| `Cocoa` | 581 | 33 | 275 | 4 | 0 |
| `AppKit` | 161 | 7 | 141 | 14 | 3 |
| `Metal` | 264 | 77 | 124 | 1 | 38 |
| Mach（`mach_`、`<mach/`、Mach-O） | 132 | 234 | 18 | 3 | 21 |
| `dyld` | 34 | 133 | 4 | 4 | 5 |
| `.dylib` | 22 | 140 | 63 | 21 | 158 |
| `/opt/homebrew` | 2 | 16 | 3 | 1 | 32 |
| `/Library` | 22 | 11 | 35 | 37 | 16 |
| `mmap` | 9 | 226 | 29 | 0 | 1 |
| `pthread_` | 42 | 963 | 2 | 0 | 0 |
| `sigaction` | 1 | 218 | 0 | 0 | 0 |
| `x18` | 6 | 643 | 25 | 9 | 22 |
| `TPIDR_EL0` | 0 | 83 | 0 | 1 | 1 |
| `svc` | 0 | 149 | 128 | 1 | 0 |

寄存器、信号、线程和内存方面的耦合几乎都在客体原生代码中；Java 侧的耦合主要是 `NativeCocoa` 调用和
宿主路径。

## JNI 接口面

| 项目 | 数量 | 处理 |
|---|---|---|
| `runtime/java` 中的 `native` 声明 | 54 个文件共 1,073 个；GLES/EGL 667、`NativeCocoa` 57、`NativeAudio` 43、`NativeMedia` 38、`CameraBridge` 30、`NativeSqlite` 26 | 按阶段注册；没有 Windows 实现的 native 以稳定错误失败，绝不返回替身结果 |
| `runtime/src` 中的 `Java_*` 导出 | 155 | 在 Windows 上显式注册（`RegisterNatives`）：JVM 只在 `System.loadLibrary` 加载的库中查找 `Java_*` 名称 |
| `runtime/src` 中的 `RegisterNatives` 调用 | 66 | 保留 |
| `runtime/java` 中的 `System.loadLibrary`/`System.load` | 42 | 在 P1 逐个审查；客体 `.so` 交给客体加载器，绝不用 `LoadLibrary` |

## Windows 上的 Java 侧

`build.ps1 -Component Java` 重复 `runtime/scripts/build.sh` 的 Java 步骤：一次 `javac --release 21`，
`android-framework.jar`（除 `com/macapk` 外的全部类）及内置字体，`macapk-loader.jar`（`com/macapk` 及解析器的
`META-INF/services`），`java_base_patch` 与 `jdk_unsupported_patch` 模块，`net_spi` 和 `bcprov`；自 `8b45e48` 起，还把 Android 的
`org.apache.http.legacy`（上游转换好的 jar）放到 classpath 上并解包进 `android-framework.jar`，绝不覆盖已编译的类。

| 检查 | 结果 |
|---|---|
| Java 侧不经修改即可用 Microsoft OpenJDK 21.0.13 ARM64 编译 | 在 `3f35611` 上 PASS：达到 javac 的 100 个警告上限（内部 `Unsafe` API 和弃用），没有错误 |
| `android-framework.jar` | 14,979 KB，6,415 个条目 |
| `macapk-loader.jar` | 1,556 KB，530 个条目 |
| 补丁模块能在 JVM 上加载（`--patch-module java.base=… --patch-module jdk.unsupported=…`） | PASS（T01） |
| `build-id.txt`（上游的 DEX 缓存标识，`build_ids.py`） | NOT_RUN：需要 Python；在 P1 随读取它的运行时一起重写 |
| `pairip-refs` | 公开仓库中没有（如上游所述）；不暂存 |

## JVM 选项

上游 `jvm_loader.cpp` 的选项及其在 Windows JVM 上的状态。

| 选项 | Windows |
|---|---|
| `--enable-native-access=ALL-UNNAMED`、`java.io`、`java.util`、`java.lang`、`jdk.internal.loader` 的 `--add-opens`、`sun.net.www.protocol.http(s)` 的 `--add-exports` | 在拒绝未知选项的情况下被 `JNI_CreateJavaVM` 接受（T01） |
| `-Xrs`、`-Xss16m` | 接受（T01） |
| `-XX:+UseSerialGC`、堆大小、`-Djava.net.preferIPv4Stack`、校验开关、AOT 缓存 | 标准 HotSpot 选项；P1 用它们启动应用时检查 |
| `java.base` 与 `jdk.unsupported` 的 `--patch-module` | 接受（T01，用 `java.exe`） |

## 宿主源码

按平台依赖列出的 `runtime/src/*.cpp`。

| 文件 | 行数 | POSIX/Apple include | Windows 处理 |
|---|---|---|---|
| `main.cpp` | 3,938 | `dlfcn`、`mach`、`sys/sysctl`、`sys/wait`、`unistd` | 由 Windows 宿主入口取代（`winapk run`，P1）；其步骤重写 |
| `native_methods.cpp` | 4,525 | `dlfcn`、`mach_time`、`sys/proc`、`sys/sysctl` | 在 `HostClock`、`HostFilesystem` 后逐个方法移植 |
| `apk_unpacker.cpp` | 4,520 | `dlfcn`、`sys/attr`、`sys/qos`、`sys/wait` | 复用 APK 解析；替换文件和进程调用（P1） |
| `window_policy.cpp`、`window_traits.cpp` | 3,017 | `sys/stat` | 复用窗口规则；呈现交给 `HostWindow` |
| `jvm_loader.cpp` | 818 | `dlfcn`、`mach`、`sys/mman` | Windows 加载器已在 `platform/windows` 中（`LoadLibraryEx` 加载 `jvm.dll`） |
| `native_sqlite.cpp` | 783 | CoreFoundation | 编入 `winapk-bridge.dll`，链接 vcpkg 的 SQLite（P1）；`_WIN32` 分支以 Win32 NLS 取代其 CoreFoundation 大小写映射和排序 |
| `native_os.cpp`、`icu_host_bridge.cpp`、`native_linker.cpp`、`jit_wx_guard.cpp` | 1,291 | `dlfcn`、`pthread`、`sys/statvfs` | 按能力处理；`jit_wx_guard` 遵守 W^X 规则（`AGENTS.md`） |
| `angle_glue.cpp`、`gles_bridge.cpp` 和 `.inc` 桥 | 5,963 | `pthread` | P5 |
| `apk_native_exec.cpp`、`app_process_group.cpp`、`app_native_stub.cpp`、`host_fonts.cpp` | 650 | `signal` | P1/P2 |

## Windows 上的 Java 路径（P1）

纯 Java APK 需要宿主提供的东西，以及 `winapk run` 如何提供（G1，[platform-notes.md](platform-notes.md)）。

| 上游部分 | Windows |
|---|---|
| `main.cpp` 的启动流程（安装、解包、DEX 转换、JVM 选项、native、`Boot.run`） | 在 `run.cpp` 中重新表达：原生读取 APK（`apk.cpp`：ZIP、ZIP64、解压、二进制 manifest、条目名筛查），与上游一样在子进程中用 com.macapk.dex 转换 DEX，但子进程是以 UTF-16 字符串向 JVM 传参的 `winapk dex`，而不是 `java.exe`（ADR-0003），按 APK、转换器和框架的哈希缓存，然后在进程内以上游的选项（去掉 macOS 专用的 hosts 文件路径和 AOT 缓存）创建 JVM；`winapk.exe` 使用 UTF-8 活动代码页，所以选项字符串能携带 Unicode 路径 |
| `build-id.txt` 与 `MACAPK_BUILD_JAVA_ID`（`build_ids.py`，由 `main.cpp` 读取） | `winapk run` 以两个 jar 的完整摘要导出 `MACAPK_BUILD_JAVA_ID`，因此应用自己的动态 DEX 缓存得以保留，并在转换器或框架有任何改动时失效（ADR-0003）；`MACAPK_BUILD_NATIVE_ID` 等客体加载器（P2–P4）实现后再提供 |
| `NativeLog`（5 个 native 中的 4 个） | 绑定 `print`、`nativeBoostCurrentThreadQos`（线程优先级高于正常）、`nativeBootTimeNanos`（性能计数器，即 HotSpot `nanoTime` 的来源）；`nativeInstallJniTrace` 和 `nativeRegisterPairipGate` 未绑定 |
| `NativeCocoa` 的窗口、呈现、输入（57 个 native 中的 29 个） | 在 `window.cpp` 中绑定（ADR-0002）：`init`、`runLoop`、尺寸和缩放、窗口和视图、渲染器、`presentBitmap`、触摸注册、方向和宽高比上限、`setSecureContent`（`WDA_EXCLUDEFROMCAPTURE`）、任务移到后台/前台、数据和库目录、`exitProcess`。窗口状态与上游一致：最小化的窗口以 2 Hz 绘制（上游对隐藏窗口的节流），恢复窗口时调用 `ActivityHost.macapkTaskToFront`（上游取消隐藏时的钩子），记录焦点变化，按下途中失去焦点或鼠标捕获时释放捕获并发送 `ACTION_CANCEL`。文字：没有映射按键送达的字符（输入法提交、死键组合、未映射的按键）以 KEYCODE_UNKNOWN 加该字符交给 `dispatchKey`，每个 UTF-16 码元一对 DOWN/UP，即上游处理组合文字的方式。未绑定，在被调用处以 `UnsatisfiedLinkError` 失败的：AOSP 窗口策略会话（`activityWindowTransition`、`windowOverrideOrientation`、`applyWindowOrientationFor`、`setTopActivityConfigChanges`）、URL 和 scheme 处理、IOSurface 环（`presentFrame`）、GL surface 和覆盖层、`hostFontFile`、`setSystemFontsDir`、`setSystemProperty`、翻译覆盖层、触控板和退出手势调试 |
| `NativeSqlite`（26 个 native） | 已绑定：就是上游的 `native_sqlite.cpp`，编入 `winapk-bridge.dll`，链接 SQLite 3.54.1，带有每个 Android 连接都有的 Unicode `upper`/`lower`/`LIKE` 和 LOCALIZED、UNICODE、PHONEBOOK 排序规则。保留的上游特性：它按 modified UTF-8 转换字符串，所以 SQL 文本或列中 BMP 之外的字符与设备上不同（macOS 亦然） |
| Boot 会用到的其他宿主 native（`NativeOs.setenv`、`GamepadBridge`、Monet 壁纸） | 未绑定；Boot 逐一记录日志并继续 |
| `AppClassLoader.findLibrary` 退回 stub | guest native 关闭时改为 `GUEST_NATIVE_DISABLED`（G1.5） |
| `ClassFileRewriter` 把 `/proc/meminfo` 别名为 `/tmp/mcmeminf` | Windows 上没有这个文件夹：读取 `/proc/meminfo` 的应用什么也得不到（`Boot` 会记录）；需要同样长度的 Windows 路径或另一种重定向 |
| 应用自己调用的 `System.load(absolutePath)` | 会到达 JVM，JVM 会把 ELF 交给 `LoadLibrary`；必须转到客体加载器（P4）——未完成 |
| `dir_naming.h`（`8b45e48` 新增：从应用文件夹名中去掉版本号里的点） | 不需要：`winapk run` 只用包名命名该文件夹 |

## 客体 ELF 加载器（P2）

N0 加载器（`runtime/guest/loader`，[native-profiles.md](native-profiles.md)）从上游取用的内容。

| 上游部分 | Windows |
|---|---|
| `elf64_reader.h` 的记录布局（`Elf64Ehdr`、`Elf64Phdr`、`Elf64Sym`、`Elf64Rela`）和 `elf_reader.h` 的常量 | 原样使用 |
| `elf64_reader.cpp` 的解析器 | 不使用：它通过 section header 查找符号，用 POSIX `open`/`pread` 读取，并为转换复制段；加载器依据 program header 工作（`PLAN.md` §9.2） |
| `arm64_native.cpp` 的内存与补丁（改写 `x18` 和 `TPIDR_EL0` 的使用、Mach VM） | 不使用：N0 拒绝这类代码而不是给它打补丁（`PLAN.md` §5.3） |
| `guest_rwx.h`、`guest_jit_pages.h`（`8b45e48` 新增：供客体 JIT 使用的可执行页） | 不使用：N0 不允许任何可执行分配 |
| translator 链接的 Capstone | 加载器的筛查使用 vcpkg 提供的 Capstone 5.0.9 |

## 改变计划假设的发现

- **这台电脑上的 Vulkan 是转换层。** Adreno 8cx Gen 3 驱动没有安装 Vulkan ICD；loader 列出两个 Mesa
  Dozen 设备（基于 Direct3D 12 的 Vulkan 1.2：Adreno 和软件 Basic Render Driver）。在它们上面 Win32
  呈现测试通过。客体 Vulkan（P6）和 ANGLE 的 Vulkan 后端（P5）在这里会运行在转换层上；`doctor` 会报告
  （`VULKAN_DRIVER_TRANSLATED`），G4 需要一台有厂商 Vulkan 驱动的电脑，或一项明确的决定
  （[decisions.md](decisions.md)）。
- **Windows 和 SDK 都不带 Vulkan 头文件**：它们按锁定的基线从 vcpkg 获取（仅构建时使用）。
- **Java 侧不需修改即可编译**于 Windows ARM64 JDK；它的平台耦合在运行时。

## 下一步

P2 的 profile N0 已完成（G2）。P3 现有独立 T08 线程基础（受管理 pthread、errno、key/析构、C 仿真 TLS 与无超时 mutex/condition 同步），
实现于 `runtime/guest/bionic`，不移植 Darwin shim。T07 在 `runtime/guest/loader` 和 `runtime/guest/bionic`
新增依赖/构造事务与客体 dl API；T08 现已覆盖私有堆内存/字符串、映射文件/时间和客体 PC 故障诊断。
格式化日志及完整 N1 集成仍未完成
（[native-profiles.md](native-profiles.md)）；之后 P4 把 APK 的库和 `System.load` 转到加载器。P1 尚未完成的是
应用运行中的 density 变化，以及应用接下来可能需要的宿主 native（`NativeOs`、宿主字体）；界面从 `winapk.Mock` 切换到
`winapk run` 属于 P7（`PLAN.md` §14）。
