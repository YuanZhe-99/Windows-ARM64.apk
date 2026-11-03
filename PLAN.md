# PLAN：Windows-ARM64.apk — 将 Mac.apk 移植到 Windows ARM64

> 交付对象：负责实施的 Coding Agent。本文是一份实施计划，不代表移植已完成或已验证。
>
> 目标：在 Windows 11 ARM64 上，以 Windows 原生进程运行选定的 Android APK；复用 Mac.apk 的 DEX → JVM 与 Android Framework 实现，逐步建立 Android ARM64 ELF、Bionic、JNI 和图形兼容层。
>
> 图形方向：Android Vulkan → ABI/WSI 适配 → Windows 原生 Vulkan；Android OpenGL ES/EGL → ANGLE → Vulkan。ANGLE/D3D11 是独立、可配置的备选后端。

- 编写日期：2026-11-03。
- 上游仓库：[kksimp/Mac.apk](https://github.com/kksimp/Mac.apk)。
- 本计划核查时的上游提交：[`f87be03575a79c7eeed578f421ee852bf99a814b`](https://github.com/kksimp/Mac.apk/tree/f87be03575a79c7eeed578f421ee852bf99a814b)，提交时间 2026-10-29 UTC。
- 上游版本不固定：Mac.apk 始终跟随上游最新 `main`（以 git subtree 引入 `macapk/`），Go 与 MyGo 始终跟随最新发行版；文中的上游路径与审计表按实际合入的上游提交核对和更新。
- 核查范围：上游构建文档、文件目录及关键构建/加载/桥接源码的定向阅读；未实际编译或执行 Windows 移植，也未完成整个仓库审计。
- 产品名称确定为 **`Windows-ARM64.apk`**。本文中的 `winapk` 仅作为内部 CLI/构建标识；面向用户的名称使用 `Windows-ARM64.apk`，无需因此批量重命名上游 Java 包或覆盖现有 macOS 产品。

## 1. 给 Coding Agent 的执行要求

1. 先完成 P0 的代码与环境盘点，再以小范围提交推进；每一步必须能说明修改位置、行为变化和验证证据。
2. 复用已有 DEX 转换器、Framework 和 APK/资源解析逻辑。优先抽取平台接口，避免另写一套 Android Framework。
3. 将“宿主 Windows ARM64 代码”和“客体 Android ARM64 代码”视为两个 ABI 域。相同指令集不等于相同运行环境。
4. 将原生执行验证提前到 P2。P2 可以在 P0 后独立开展，不必等基础界面全部完成；不让 UI 投入掩盖原生路线的不确定性。
5. 保留纯 Java APK 的独立交付路径。原生执行尚未通过时，该路径仍应能构建、运行、测试和发布实验包。
6. 不以加载成功、窗口出现、空实现返回成功代替功能完成。每个阶段都要有可复现的功能样例和失败样例。
7. 运行时不依赖 WSA、WSL、Android 虚拟机、Android 系统镜像或完整 Linux 用户空间。使用 JVM 是本方案既定设计，与“不启动 Android 虚拟机”并不矛盾。
8. 第一阶段支持自建、可审计的测试 APK/ELF；不承诺任意 APK、商业游戏、保护壳、ART 私有接口或完整 Android 系统兼容性。
9. 默认不实现 ARM32 重编译、通用二进制翻译或通用指令模拟。若原生路线必须依赖这些能力，先提交架构决策和独立实验结果，不把它们悄悄加入 MVP。
10. 实现普通适配和修复不需要反复确认。确实受硬件、依赖或架构选择阻塞时，记录证据、完成不受影响的工作，再提出具体问题。

## 2. 目标、范围和交付层级

### 2.1 成功定义

| 交付层级 | 可观察结果 | 不代表什么 |
| --- | --- | --- |
| A：Java MVP | 原生 Windows ARM64 JVM 运行由 APK 转换得到的代码；基础 Activity、资源、窗口、输入和本地存储可用 | 不代表 APK 内的 Android `.so` 可运行 |
| B：Native PoC | 受支持的 `arm64-v8a` ELF 经自定义加载器运行；JNI 双向调用、有限 libc/线程/TLS 场景通过 | 不代表任意 Bionic、Linux syscall 或 C++ 库兼容 |
| C：Graphics PoC | GLES 样例经 ANGLE/Vulkan 呈现；Android Vulkan 样例经 Windows 原生 Vulkan 呈现 | 不代表 Android 图形扩展、硬件缓冲和跨 API 共享全部支持 |
| D：可交付实验版本 | A、B、C 的测试资产、依赖、日志和启动入口可在干净 ARM64 机器上复现 | 不代表生产级 APK 平台或安全沙箱 |

不要把 A 标注为 B/C/D 已完成。最终报告按层级分别列出 `PASS / FAIL / BLOCKED / NOT_RUN`。

### 2.2 第一轮必须覆盖

- Windows 11 ARM64 原生进程、原生 ARM64 JVM 和原生 ARM64 宿主 DLL。
- 先以单 APK 验证，每个 APK 一个独立运行时进程；支持在程序主窗口内的手机式窗格中打开，也支持独立窗口打开。
- Java/Kotlin 经 DEX → JVM 的既有路线；先用无应用原生库的简单 Java APK建立基线。
- 基础 View/Canvas 呈现、点击、键盘、返回、窗口尺寸变化和数据持久化。
- ELF64/AArch64 `ET_DYN` 的受限加载、重定位、依赖解析和入口调用。
- 有明确能力清单的 Bionic/JNI 桥。
- Vulkan 驱动探测、Win32 WSI、ANGLE/Vulkan GLES，以及独立的客体 Vulkan 桥。
- 可重复构建、测试夹具、结构化能力报告和兼容性报告。

这里的“纯 Java APK”指应用不携带或依赖客体 Android 原生库，并不表示宿主不需要 JNI、JDK 本地库、字体或位图呈现组件。

### 2.3 明确后置

- ARM32/Thumb、x86/x86_64 Android 应用原生代码。
- 任意 ELF 可执行程序、`fork/exec`、完整 Binder 服务、Linux 进程模型。
- ART 内部对象布局、ART JIT、应用自生成机器码、通用自修改代码。
- 通用 POSIX 信号兼容、跨 Windows/Android C++ 运行库异常展开。
- AHardwareBuffer 完整互操作、视频零拷贝、完整多 Surface 合成。
- Google Play Services、DRM、设备完整性认证和反作弊兼容。
- 摄像头、麦克风、定位、传感器、WebView、媒体编解码的完整实现。
- 安装商店、自动更新、MSIX、文件关联和高级管理功能。

### 2.4 产品前端与设计交接

- **名称与前端：** 本程序名为 `Windows-ARM64.apk`，程序本身的前端使用 **MyGo**。
- **两种打开方式：** App 可以在主窗口内一个类似手机模拟器的窗格中打开，也可以作为独立窗口打开。手机式窗格是呈现形式，底层仍沿用本文的原生兼容运行时。
- **设计交接：** 由设计 Agent **参考 Kyokai**，具体设计主窗口、App 展示窗格、独立窗口以及打开方式的选择交互；本 PLAN 不预设详细布局和视觉样式。
- **实现边界：** MyGo 负责产品前端，运行时保留独立进程及窗口/Surface 接口；嵌入呈现的具体接入方式由 Coding Agent 配合设计方案验证，不预设必须使用某一种窗口嵌入技术。

## 3. 已核实事实与待验证假设

### 3.1 上游代码定位

以下是本计划核查时上游提交中实际存在的入口，不是建议新建的目录。

| 上游路径 | 本次核查所得 | 移植动作 |
| --- | --- | --- |
| `runtime/java/`、`runtime/java/com/macapk/dex/` | Framework 与 DEX 转换逻辑在 Java 侧 | 保留主体，隔离平台调用和路径假设 |
| `runtime/java/com/macapk/loader/Boot.java` | 启动流程包含 `NativeCocoa`、本地库和 macOS 路径耦合 | 拆出宿主启动服务，保留应用生命周期 |
| `runtime/java/com/macapk/loader/NativeCocoa.java` | 大量窗口、Surface、呈现和宿主服务 JNI 方法 | 新增平台中立入口，渐进替换调用 |
| `runtime/src/jvm_loader.cpp` | JVM 嵌入和运行时选项依赖 macOS 加载环境 | 实现 Windows JVM 加载与选项选择 |
| `runtime/CMakeLists.txt` | 顶层直接启用 OBJCXX，包含 Apple 依赖和 Homebrew 路径 | 平台条件化语言、源文件、链接和依赖发现 |
| `runtime/translator/src/arm64_native.cpp` | ELF 映射/重定位与 macOS 内存、寄存器、补丁机制交织 | 复用解析算法，分离宿主内存与执行策略 |
| `runtime/translator/src/arm64_pack.cpp`、`arm64_macho_emit.cpp` | 现有链路包含 Mach-O 包装产物 | Windows 改用宿主 PE 桥和虚拟客体库注册 |
| `runtime/translator/src/bionic_shim.cpp` | Bionic/系统调用/线程等桥接与 Darwin 机制耦合 | 按能力拆分，不整文件机械替换 |
| `runtime/translator/src/bionic_shim_vulkan.cpp` | 已有面向 MoltenVK/Metal Surface 的客体 Vulkan 适配 | 保留可复用的扩展处理思路，新增 Win32 后端 |
| `runtime/src/angle_glue.cpp`、`gles_bridge.cpp` | 已有 GLES/ANGLE 桥入口 | 分离宿主表面、后端选择和客体 ABI |

入口依据：[构建定义](https://github.com/kksimp/Mac.apk/blob/f87be03575a79c7eeed578f421ee852bf99a814b/runtime/CMakeLists.txt)、[原生加载器](https://github.com/kksimp/Mac.apk/blob/f87be03575a79c7eeed578f421ee852bf99a814b/runtime/translator/src/arm64_native.cpp)、[客体 Vulkan 桥](https://github.com/kksimp/Mac.apk/blob/f87be03575a79c7eeed578f421ee852bf99a814b/runtime/translator/src/bionic_shim_vulkan.cpp)。

上游构建说明明确区分：基础 Canvas 仍使用 Java2D，AGSL/RenderEffect 通过 Skia 的 Metal GPU 后端执行并回读。因此 Windows 计划不能把“基础 Canvas”“GLES”“Vulkan”“AGSL”视作一个替换 Metal 的任务。[上游 BUILDING.md](https://github.com/kksimp/Mac.apk/blob/f87be03575a79c7eeed578f421ee852bf99a814b/BUILDING.md)

### 3.2 不应继承为结论的假设

- “Windows ARM64 与 Android ARM64 都是 AArch64，因此 `.so` 可以直接调用”：必须经过 ABI、加载器、TLS 和系统调用验证。
- “某款 Qualcomm GPU 已认证 Vulkan，因此当前设备可用”：以当前机器的 ARM64 Vulkan Loader、ICD、功能与呈现探测结果为准。
- “Android `x18` 一定被所有应用使用”：NDK 通常保留它，具体二进制必须检查；也不能因为没发现使用就宣布整个程序兼容。
- “上游注释中的 TLS/x18 描述就是 Android 平台规范”：上游可能描述自己的重写后状态或特定应用行为；标准行为按 Android/AOSP 文档与实测确认。
- “Vulkan 原生支持意味着不用做桥接”：仍有 ABI、函数指针、回调、扩展筛选、Surface、生命周期和同步适配。
- “ANGLE 与客体都使用 Vulkan 就能共享所有资源”：独立 instance/device、分配器和同步对象仍然需要设计，MVP 不做隐式共享。

## 4. 架构决策

### 4.1 默认技术选择

| 项目 | 默认方案 | 采用条件/限制 |
| --- | --- | --- |
| 宿主架构 | 原生 ARM64 PE/COFF | 第一轮不用 ARM64EC 混合 ABI |
| 宿主语言 | 沿用 C/C++ 和 Java | Objective-C++ 仅留在 macOS 目标中 |
| 产品前端 | MyGo | 名称为 `Windows-ARM64.apk`；由设计 Agent 参考 Kyokai 设计双模式打开体验 |
| 运行时窗口后端 | Win32 `HWND` 与消息循环 | 服务于主窗口内窗格或独立窗口呈现，与 MyGo 产品前端分层 |
| JVM | 可获得、可再分发的 Windows ARM64 OpenJDK 21 构建作为首选 | P0 核实版本、模块、JNI 和 AWT/Java2D；确有不兼容再通过 ADR 调整 |
| 构建 | CMake + Ninja + Windows SDK；优先 clang-cl 工具链 | 宿主与 NDK 客体工具链分开；汇编按实际目标格式编译 |
| 基础 Canvas | 复用 Java2D 离屏渲染，Win32 位图呈现 | 明确是基础路径，不宣称 GPU Canvas 完成 |
| GLES/EGL | Windows ARM64 ANGLE，Vulkan 优先 | D3D11 备选需独立测量与能力记录 |
| 客体 Vulkan | Windows Vulkan Loader/ICD | 不引入 MoltenVK，不把 Android 扩展直接透传给 Windows |
| Skia/AGSL | 后期构建 Windows ARM64 Skia GPU 桥 | 优先验证 Vulkan；不可直接使用 macOS `.dylib` |
| 客体原生库 | 自定义 ELF 加载器 + 受限执行 profile | 不使用 `LoadLibrary` 加载 ELF |
| JNI 接入 | 一个宿主桥 DLL + 按 ClassLoader 隔离的客体库注册表 | 默认不为每个 `.so` 生成完整 PE 包装；实际困难记录 ADR |
| 应用隔离 | 每个 APK 一个运行时子进程 | 进程隔离用于故障管理，不声称提供 Android 安全沙箱 |

ANGLE 提供 Vulkan 和 D3D11 后端，但某一固定版本的 Windows ARM64 构建与目标驱动组合仍须验证。[ANGLE 官方仓库](https://github.com/google/angle)

### 4.2 运行路径

```text
Windows-ARM64.apk（MyGo 前端，窗格内 / 独立窗口）或 winapk CLI
    └─ 每个 APK 启动一个 Windows ARM64 runtime 进程
         ├─ APK / Manifest / Resources / Assets
         ├─ DEX → JVM bytecode → ARM64 OpenJDK
         │    └─ Android Framework → Host Platform API
         │         ├─ Java2D bitmap → Win32 基础呈现
         │         └─ Java GLES JNI → ANGLE → Vulkan（备选 D3D11）
         └─ arm64-v8a ELF → Guest Loader / ABI / Bionic / JNI
              ├─ GLES / EGL → 客体 ABI 桥 → ANGLE
              └─ Vulkan → 客体 ABI + Android WSI 桥
                              └─ vulkan-1.dll → ARM64 ICD → GPU

Host Platform API
    ├─ Windows：窗口、事件、内存、线程、路径、时钟、动态库
    └─ macOS：继续使用原有平台实现
```

### 4.3 平台接口边界

接口以调用需求驱动，不先做“大而全”的平台框架。以下名称可调整，但职责必须保持清楚。

| 接口 | 职责 | 关键约束 |
| --- | --- | --- |
| `HostWindow` | 创建、显示、关闭、尺寸/DPI、原生窗口句柄 | `HWND` 由宿主拥有；窗口操作归消息线程 |
| `HostEventLoop` | 消息泵、任务投递、退出与事件队列 | 不把 JVM 主 Looper、渲染线程和 Win32 消息线程随意合并 |
| `HostFramePresenter` | 呈现位图、dirty rect 和 Surface 内容 | 像素格式、stride、alpha、所有权显式定义 |
| `HostMemory` | reserve/commit/protect/free、指令缓存刷新 | 区分 Windows 分配粒度、页大小、ELF 对齐 |
| `HostLibrary` | 加载宿主 DLL、查找宿主导出 | 不兼任 ELF 解析或客体符号搜索 |
| `GuestModuleRegistry` | ELF 模块、导入、符号、引用和依赖状态 | 宿主 DLL handle 与客体 handle 不得混用 |
| `GuestThreadContext` | 客体 errno、pthread/TLS、JNI 绑定 | 不替换 Windows TEB，不借用宿主保留寄存器 |
| `HostFilesystem` | 应用数据、缓存、资源与路径映射 | UTF-8/UTF-16、大小写冲突和文件句柄语义明确 |
| `HostClock` | 单调时钟、wall clock、deadline | 不把不同纪元或单位混为同一个时间值 |
| `GraphicsCapabilities` | 后端、扩展、格式和可用功能 | 只报告已实现且验证的能力 |

跨平台 C 接口尽量使用固定宽度整数、显式长度和 opaque handle；不让 `std::string`、Windows `long`、CRT `FILE*` 或平台 pthread 对象跨客体边界。

### 4.4 建议新增的目录

以下目录是建议新增项，不是已存在的上游结构。P0 应核对仓库约定后再创建。

```text
docs/windows-arm64/
  port-audit.md
  abi-compatibility.md
  native-profiles.md
  graphics-capabilities.md
  compatibility.md
  adr/
runtime/platform/
  common/
  windows/
  macos/                 # 按需迁入，不做一次性大搬家
runtime/guest/
  abi/
  loader/
  bionic/
  jni/
  graphics/
cmake/platforms/
scripts/windows/
tests/windows-arm64/
  host/
  guest-elf/
  apk-fixtures/
  graphics/
  negative/
```

已有 `runtime/translator/` 不必立即删除或改名。先提取可测试组件、增加 Windows 目标，再逐步替换重复实现。

## 5. ABI 与原生执行的硬约束

### 5.1 ABI 检查表

| 风险 | 必须采取的措施 | 验证样例 |
| --- | --- | --- |
| `x18` | Windows 中始终保留真实宿主用途；不将其临时改为客体 TLS/SCS 指针 | 入口/回调前后读取并比对，线程切换与嵌套回调压力测试 |
| `TPIDR_EL0` | 视为宿主保留状态；不用写寄存器的方式安装 Android TLS | TLS 指令扫描、不同 TLS model 的正/负样例 |
| 数据模型 | 显式描述 Android LP64 与 Windows LLP64 的接口类型 | `long`、指针、结构体大小、偏移、返回值测试 |
| 可变参数 | 不跨平台复用 `va_list`；解析 guest 参数后调用类型明确的 host 函数 | 混合整数/浮点、寄存器溢出到栈、JNI `...`/`V`/`A` |
| 固定参数 | 按签名证明兼容；不以一个整数加法推广到所有接口 | float/double、HFA、结构体、间接返回、栈参数 |
| 类型布局 | guest/host 结构体分别定义、显式转换 | `stat`、`timespec`、`sockaddr`、pthread 相关类型 |
| 浮点扩展 | `long double` 等不得按宿主类型盲转 | 明确实现转换或将对应 ABI 标记不支持 |
| 异常/展开 | 区分 Android C++/DWARF 与 Windows SEH | 客体内 throw/catch、桥处拒绝跨域展开、错误诊断 |
| Linux syscall | 导入 shim 与内联 `svc` 分别处理 | 导入成功但含内联 syscall 的拒绝样例 |
| JIT/自修改 | MVP 不允许客体动态生成可执行代码绕过检查 | executable allocation / protection change 负样例 |

依据：Windows ARM64 的 `x18` 用于 TEB，`TPIDR_EL0` 保留；Android NDK 的 `x18` 保留给平台机制。不要实现“进入客体时换 x18，返回时恢复”作为通用方案，因为异步异常和回调也可能发生。[Windows ARM64 ABI](https://learn.microsoft.com/en-us/cpp/build/arm64-windows-abi-conventions?view=msvc-170)、[Android ABI](https://developer.android.com/ndk/guides/abis)

Windows 的 LLP64 数据模型使 `long` 仍为 32 位。客体接口里的 64 位 Android `long` 应使用独立类型表达，避免地址截断。[Windows 数据模型](https://learn.microsoft.com/en-us/windows/win32/winprog64/abstract-data-models)

### 5.2 受限原生执行 profiles

**N0：可审计的 leaf ELF。** 仅自建夹具、已知编译参数和哈希；无 TLS、无 libc/动态依赖、无内联 syscall、无 x18 访问、无异常和自修改代码。用于证明映射、重定位和最小 ABI。

**N1：受限 C/JNI ELF。** 允许已实现的显式依赖、导入、构造函数和 JNI；仅允许通过 shim 管理的线程状态。具体 TLS model 以测试矩阵为准，不把“支持 `thread_local`”写成无条件承诺。

**N2：图形样例 ELF。** 在 N1 基础上开放经验证的 EGL/GLES/Vulkan/ANativeWindow 子集和回调。

**扩展 profile：** 仅在单独 ADR 与测试通过后加入复杂 TLS、客体内部 C++ 异常或二进制重写。

静态反汇编扫描只是筛查手段，不能证明任意 ELF 没有隐藏代码、跳入数据或运行时解密行为。因此初期准入依赖可审计夹具及其构建来源，不是“扫描无命中即可安全运行所有 `.so`”。

### 5.3 `x18`、TLS 与重写策略

1. 将上游 x18、TPIDR 和 syscall 补丁机制列入复用审计，默认不在 Windows 启用。
2. 不复制 macOS entitlement、TPIDR 位开关或 Mach signal handler。
3. Guest TLS 通过线程上下文、模块 TLS 表和受控 resolver 实现；分别处理 emulated TLS、GD/LD、IE/LE、TLSDESC。
4. 对要求直接读取 Android thread pointer 的代码，若尚无可靠适配，拒绝该 profile，不返回伪造地址或全零结果。
5. 如研究二进制重写，必须覆盖无符号代码、指令/数据混排、PC-relative 寻址、分支距离、活跃寄存器、异常元数据和新生成代码；单独报告覆盖边界。
6. 测试夹具可以调整编译参数帮助定位问题，但报告必须区分“特制兼容夹具”和“常规 NDK 默认构建”。不得用前者证明普遍兼容。

TLS 的加载和访问不仅是一个 `TlsAlloc` 调用；它涉及模块布局、访问 model 和线程初始化。[AOSP ELF TLS 设计](https://android.googlesource.com/platform/bionic/+/HEAD/docs/elf-tls.md)

## 6. 阶段依赖和验收关卡

```text
P0 盘点/依赖/探测
 ├─ P1 纯 Java 启动与 Win32 基础界面 ───────────────┐
 ├─ P2 ELF + ABI 最小实验 → P3 Bionic/TLS → P4 JNI ┤
 └─ P5a 宿主 ANGLE/Vulkan 构建与呈现 ─────────────┤
                                                  ↓
                                    P5b 客体 GLES/EGL 集成
                                                  ↓
                                    P6 客体 Vulkan/WSI 集成
                                                  ↓
                                    P7 打包、回归、实验版交付

P8：P7 后的可选扩展（Skia/AGSL、媒体、ARM32 等）
```

依赖图不要求创建多个 Agent；单个 Agent 也按这个依赖顺序推进。P2 失败时继续交付 P1，并给出原生路线的最小失败复现。

| 关卡 | 必须证明 | 未通过的处理 |
| --- | --- | --- |
| G0：宿主就绪 | ARM64 JVM/桥 DLL可启动；工具链与依赖可追溯 | 只完成可做的静态构建/审计，不能声称运行验证通过 |
| G1：Java MVP | APK 中的真实 DEX 经转换、加载、显示并交互 | 修复平台调用/资源问题，不绕过 APK 直接运行桌面 Demo |
| G2：Native 最小执行 | N0 正确返回、ABI 测试通过、无破坏宿主状态 | 暂停广泛 native 移植，保留 Java 路线 |
| G3：Native 集成 | N1 的依赖、TLS 子集和 JNI 往返正确 | 报告具体 profile 不支持，不能用空 shim 掩盖 |
| G4：图形 | GLES 与 Vulkan 两条客体路径各自通过 | 后端分别报告，不互相代替 |
| G5：实验版 | 干净机器构建/运行/错误报告可复现 | 标记未验证项，不把打包成功当兼容性通过 |

## 7. P0 — 仓库盘点、工具链与能力探测

### 输入

- 上游源码、当前分支、适用的仓库工作规范。
- 一台真实 Windows 11 ARM64 设备；图形阶段需要可访问 GPU 和桌面会话。
- Windows ARM64 JDK、C/C++ 工具链、Windows SDK、CMake/Ninja、Android SDK/NDK。

### 工作项

- [ ] 记录当前工作树状态、合入的上游 commit 和已有变更；不覆盖用户改动。
- [ ] 对比本计划核查时的上游提交与最新上游 `main`；路径变化时更新审计表，不盲套文件名。
- [ ] 搜索平台耦合：`Cocoa`、`AppKit`、`Metal`、`Mach`、`dyld`、`.dylib`、`/opt/homebrew`、`/Library`、`mmap`、`pthread`、`sigaction`、`x18`、`TPIDR_EL0`、`svc`。
- [ ] 阅读 `runtime/CMakeLists.txt`、`runtime/translator/CMakeLists.txt`、`runtime/scripts/build.sh` 和 `scripts/build_framework_res.sh`。
- [ ] 从既有构建脚本提取 Java 编译、资源打包、module patches 和启动选项；不只把 `.sh` 改名为 `.ps1`。
- [ ] 为宿主、客体和 Java 工具链分别记录版本、target triple、来源、校验值、许可证与用途。
- [ ] 检查生成 PE 的机器类型、JVM/DLL 架构、NDK ELF 的架构。构建期工具可以通过 x64 仿真运行，但不得与原生运行时验证混淆。
- [ ] 核实 JDK 的 `java.desktop`、JNI headers、`jvm.dll`、Java2D/字体支持，以及上游需要的 `--patch-module`、`--add-opens`。
- [ ] 编写 `doctor`：检查运行时、JVM、图形后端及可选依赖，输出机器可读 JSON。
- [ ] 锁定 Mac.apk、Go、MyGo 以外的依赖版本（这三者跟随最新，只记录每次构建实际使用的版本）；不得依赖 macOS 预编译产物。ANGLE/Skia 若没有合适 ARM64 二进制，建立独立构建任务。
- [ ] 建立公开、自建的 APK/ELF 测试集；上游私人回归集不可作为验收前提。

### `doctor` 最低字段

```json
{
  "schemaVersion": 1,
  "host": { "osBuild": "measured", "osArch": "arm64", "processArch": "arm64" },
  "jvm": { "vendor": "measured", "version": "measured", "arch": "arm64" },
  "graphics": {
    "vulkanLoader": "available-or-reason",
    "devices": [],
    "win32Present": "not-run-or-result",
    "angleBackends": []
  },
  "runtime": { "guestNativeProfiles": [], "guestVulkan": false },
  "limitations": []
}
```

GPU 记录至少包含 vendor/device ID、驱动版本、API 版本、queue families、WSI 支持；区分“驱动支持”与“兼容层已开放”。没有 Vulkan 时，Java MVP 仍可运行，客体 Vulkan 明确不可用。

### 产物与验收

- `port-audit.md`：文件、依赖、平台耦合、处理方式、优先级。
- 依赖锁定清单、工具链说明、基线构建结果。
- `doctor` 输出及 G0 状态。
- 第一个 ADR：宿主工具链/JDK、运行时进程结构和 JNI 加载方向。

## 8. P1 — 纯 Java APK 与 Win32 基础界面

### 8.1 拆分构建

- [ ] Windows 配置只启用 C/C++；macOS 才启用 OBJCXX 与 Apple frameworks。
- [ ] 分离公共 runtime、macOS backend、Windows backend；用目标级源文件和链接设置控制平台。
- [ ] 给 Java Framework/DEX 转换增加独立构建入口，不要求先构建 Bionic shim、ANGLE 或 Skia。
- [ ] 支持 Windows classpath 分隔符、Unicode/空格路径、参数文件和长命令行。
- [ ] 保留上游资源、字体、module patches、build IDs 与缓存失效语义。
- [ ] 按锁定 AOSP/SDK 配方提供 `framework-res.apk`；记录来源/API，缺失时明确失败，不以空资源包通过。

### 8.2 平台中立 Java 接口

- [ ] 对 `Boot`、`ActivityHost`、`ViewCanvasDriver` 与 `NativeCocoa` 的调用关系做最小梳理。
- [ ] 新增 `HostPlatform` 或等价适配层；macOS 后端可继续调用 `NativeCocoa`，Windows 后端调用新的 JNI 桥。
- [ ] 基础窗口/输入/呈现路径先切换，其他服务按能力返回明确的“不支持”。
- [ ] 避免一次性全局替换类名；任何暂时保留的 `NativeCocoa` 名称都要说明是过渡兼容接口。
- [ ] 不全局伪造宿主 `os.name` 等系统属性来影响整个 JDK；若上游依赖这类机制，在宿主与客体视图间隔离并回归验证。

### 8.3 JVM 与线程

- [ ] Windows runtime 从确定的 JDK/JRE 路径加载正确的 `jvm.dll`，使用 Invocation API 创建 JVM。
- [ ] 验证 JDK 选项实际有效，不照搬 macOS 的信号/主线程/JIT 参数。
- [ ] 明确 Win32 消息线程、Framework 主 Looper 和渲染任务归属；跨线程使用任务队列。
- [ ] 避免 JNI 调用持锁等待 UI，而 UI 又同步等待 JVM 的循环依赖。
- [ ] 正确处理关闭、焦点、最小化、恢复和进程退出；阻塞任务有超时与诊断。

### 8.4 基础图形和输入

- [ ] Java2D 渲染到离屏像素缓冲；Windows 使用 DIB/等价最小位图路径呈现。
- [ ] 显式定义 ARGB/BGRA、premultiplied alpha、stride、尺寸和缓冲所有权；先解决正确性，再优化拷贝。
- [ ] 映射鼠标点击/拖动为触摸，键盘/滚轮为相应 Android 事件；首轮先支持常见按键和返回。
- [ ] DPI 缩放后使用一致的逻辑坐标；resize 更新 Surface/Activity 的尺寸与生命周期。
- [ ] 简单测试页面使用 TextView、Button、EditText 和可滚动列表；字体回退和输入结果要可检查。
- [ ] 为不支持的高级输入/多触点/IME 场景记录范围；不让基础编辑框静默丢字。

### 8.5 数据与缓存

- [ ] 将每应用数据、缓存和日志放入 `%LOCALAPPDATA%` 下明确的运行时目录，可由测试参数覆盖。
- [ ] 处理 APK 文件项路径穿越、绝对路径、重名/大小写冲突、非法 Windows 名称和解压大小限制。
- [ ] 在原始 ZIP 中保留大小写敏感的资源查询语义；不能简单将所有 APK 项直接映射成 Windows 文件名。
- [ ] 缓存 key 包含 APK 内容哈希、转换器/Framework 版本、平台和必要配置；写入使用临时文件后原子替换。

### G1 验收

1. 从真实 `hello-java.apk` 的 DEX 启动 Activity，日志包含转换、类加载和生命周期节点。
2. 页面可显示文字、按钮和输入框；点击计数、文本输入、滚动、返回均有断言。
3. 改变窗口尺寸/DPI 后坐标和内容正确；关闭后无残留运行时进程。
4. 保存首选项/应用文件并重启验证；冷缓存与热缓存两次均成功。
5. 未启用 guest native 时，尝试加载 APK `.so` 给出明确错误。
6. 可选 Kotlin 夹具通过同一 DEX 路径运行，包含必要标准库；不以纯 Java 结果推定全部 Kotlin/Compose 兼容。

## 9. P2 — 提前验证 ELF 加载与 ARM64 ABI

### 9.1 独立实验程序

建立不依赖 JVM/窗口/ANGLE 的 `native-probe.exe`。只加载受控测试 ELF，输出检查结果和失败阶段。保持该实验可独立运行，便于区分 JVM 问题与加载器问题。

### 9.2 解析、内存和重定位

- [ ] 先接受 little-endian ELF64、`EM_AARCH64`、`ET_DYN`；验证所有偏移、长度、溢出和 segment 重叠。
- [ ] 按 program headers 处理 `PT_LOAD`、`PT_DYNAMIC`、BSS 和对齐，不把完整 section headers 作为正常加载前提。
- [ ] 查询 Windows 页大小与 allocation granularity；计算 load bias、reserve/commit 范围和最终权限。
- [ ] 使用 `VirtualAlloc`/`VirtualProtect` 等宿主实现，并在代码修改后 `FlushInstructionCache`。
- [ ] 保持构造期间可写、执行前设定最终权限；不以长期 RWX 规避问题。不能可靠表达的 page 权限组合应报告并拒绝。
- [ ] 第一批实现 `RELATIVE`、`ABS64`、`GLOB_DAT`、`JUMP_SLOT`；按用例加入 GNU hash/symbol versions 等。
- [ ] 明确记录 RELR、Android packed relocations、TLS relocations、IFUNC/IRELATIVE 的支持状态；未知类型不得跳过。
- [ ] 解析阶段完成不等于可执行：依赖、重定位、执行 profile 检查完成前，不运行构造函数或 IFUNC resolver。
- [ ] 支持失败回滚，避免半初始化模块进入全局可查找状态。

### 9.3 ABI 测试

- [ ] NDK 生成 N0 fixtures，并记录 API level、编译器/链接器版本、优化和安全选项。
- [ ] 测试整数、指针、浮点、混合参数、超过寄存器容量的参数、HFA、小/大结构体返回。
- [ ] 验证保存寄存器、栈对齐、返回地址和回调重入；`x18` 检测只读，不主动给宿主 x18 写 sentinel。
- [ ] 对 thunk 使用明确的 guest/host 参数模型；有证明的固定签名才允许直接调用。
- [ ] 动态产生的宿主 thunk 按 Windows 要求处理可调用目标和展开信息；用目标进程实际缓解策略进行测试。
- [ ] 扫描 x18、TPIDR、svc 等风险并结合构建来源决定准入；仅报告支持的 N0 语义。
- [ ] 加入损坏 ELF、错误架构、未知重定位、缺失符号和禁止指令的负例。

### G2 验收

- N0 所有正例返回精确期望值；至少包含双向回调和浮点/结构体场景。
- 负例在执行入口/构造函数之前拒绝，并输出模块、偏移与原因。
- 循环加载/执行无崩溃，无随循环增长的资源泄漏；地址随机化下可重复。
- 在真实 ARM64 上执行；x64 机器仅能报告交叉编译或解析测试通过。
- 形成 `abi-compatibility.md`：哪些签名可直通、哪些必须 thunk、哪些不支持，以及证据位置。

若 G2 未通过，保持 guest native 默认关闭，继续 Java MVP；不要进入大规模 Bionic 或游戏兼容开发。

## 10. P3 — Bionic、线程、TLS 与客体模块语义

### 10.1 按样例需求实现的第一批功能

| 功能组 | 首批范围 | 必须注意 |
| --- | --- | --- |
| 内存/字节 | malloc/calloc/realloc/free、memcpy/memmove/memset/memcmp | 分配与释放归属，大小/溢出，guest errno |
| 字符串/日志 | 常用字符串、Android log、受限格式化 | Android `long`、`va_list`、编码 |
| 文件 | open/read/write/close/lseek/fstat、基础目录映射 | 独立 guest fd 表，Windows HANDLE 不是 fd |
| 时间 | monotonic/realtime、有限 sleep | 单位、epoch、超时返回和中断语义 |
| 线程 | create/join、mutex、cond、TLS keys | pthread 对象不可 memcpy 成 SRWLOCK/CRITICAL_SECTION |
| 动态加载 | dlopen/dlsym/dlerror、依赖引用 | guest handle、lookup scope、构造顺序和重复加载 |
| TLS | 先实现夹具实际使用且可验证的 model | 模块 TLS 初始化、线程销毁和析构 |

### 10.2 行为约束

- [ ] 为 guest `errno` 建立每线程存储；错误码显式转换，不使用 `GetLastError` 原值充当 Linux errno。
- [ ] 对 guest opaque 对象采用独立布局/对象表，管理初始化、销毁和无效 handle。
- [ ] `pthread_create` 经宿主线程入口创建 GuestThreadContext，再调用客体入口；退出时清理 TLS/JNI 状态。
- [ ] 处理 static initializer、依赖环、弱符号、重复 SONAME 和 symbol version 的已支持子集。
- [ ] guest `dlclose` 初期可以将模块保留到进程结束，但必须文档化并保持引用语义一致；不能释放仍被线程/JNI 使用的代码。
- [ ] 不按名称把 Bionic 导入直接绑定到任意 UCRT 导出；每个导入经过 ABI/语义审计。
- [ ] `stdio`、`va_list`、pthread、`stat` 等不透明/布局不同对象不能直接跨域传递。
- [ ] 原始 Linux `svc` 永远不传入 Windows 内核。当前不支持的内联 syscall 在已声明 profile 中拒绝。
- [ ] WaitOnAddress/条件变量等只能作为实现组件；不得声称它们天然等价于所有 futex 操作。
- [ ] 缺失符号在初始化前列出；已实现函数的错误遵循其 API 语义，不用万能“返回 0”补洞。

### 10.3 异常与故障边界

- MVP 不允许 Android C++ 异常穿越宿主桥或 JVM 边界。
- 客体内部 C++ throw/catch 若需要支持，单独验证 guest libc++/unwind 和 TLS 依赖；不因为 Windows 有 SEH 就认为自动可用。
- Windows 宿主 thunk 的展开元数据与 ELF 的 `.eh_frame` 是不同问题，不直接复用。
- 诊断处理不得抢占 HotSpot 自己的异常处理。只识别本运行时管理的故障范围；未知故障交给正常处理链。
- 遇到无法恢复的客体执行故障，记录诊断并终止该 APK 进程；不在损坏状态下继续运行。

Windows ARM64 的异常展开有专用元数据和约定，应作为独立移植工作处理。[Windows ARM64 异常处理](https://learn.microsoft.com/en-us/cpp/build/arm64-exception-handling?view=msvc-170)

### 验收

- 多个 guest 线程拥有不同 errno/TLS 值，join 与析构结果正确；增加至少 8 线程的压力夹具。
- 文件读写/seek/stat 与夹具预期一致；中文/空格路径可用，非法映射有明确失败。
- 两个 ELF 的依赖、构造、导出解析和重复加载行为可重复。
- 缺失导入、未知 TLS model 和禁止 syscall 均产生可定位错误。
- 更新 `native-profiles.md`，列出支持函数、TLS model、限制及对应测试编号。

## 11. P4 — JNI 与 APK 原生库集成

### 11.1 不能省略的加载适配

Windows JVM 的 `System.load()` 不能直接加载 ELF。上游 `NativeLoaderShim` 借助宿主动态库与 ClassLoader 的关联工作，因此不能只将 `.dylib` 字符串改成 `.dll`。[上游 NativeLoaderShim](https://github.com/kksimp/Mac.apk/blob/f87be03575a79c7eeed578f421ee852bf99a814b/runtime/java/com/macapk/loader/NativeLoaderShim.java)

默认实现方向：

1. 宿主桥 DLL 由受控宿主 ClassLoader 加载一次。
2. 客体 `System.loadLibrary`/`System.load` 通过既有 DEX/类加载重写入口进入 GuestNativeLoader，维护虚拟库状态。
3. 客体 ELF 的 `DT_NEEDED` 依赖加载与 Java 的显式库加载分开处理；依赖加载不自动调用每个依赖的 `JNI_OnLoad`。
4. 显式 Java 加载触发正确的 `JNI_OnLoad`、版本检查、异常处理与 ClassLoader 上下文。
5. `RegisterNatives` 注册可由 Windows JVM 调用的宿主 ABI thunk，thunk 再进入客体 ABI。
6. 对使用 `Java_package_Class_method` 导出的普通 JNI 方法，实现 short/long mangled name、重载签名和适当的绑定时机；不能只支持测试里的手工 `RegisterNatives`。
7. 将宿主桥的加载归属与每个 guest ClassLoader 的模块状态隔离；不反复对同一 DLL 做跨 ClassLoader `System.load`。

上述重定向只作用于客体应用的库加载路径；JDK、宿主 Framework 依赖和 Windows 后端仍使用宿主加载机制，不能全局拦截所有 `System.loadLibrary` 并把它们当作 ELF。

如果该方向与现有转换器语义发生冲突，先以小实验比较 PE facade 方案，再通过 ADR 调整。不要同时维护两套未经验证的默认加载路径。

### 11.2 JNI 双向桥

- [ ] 宿主 `JavaVM*` / `JNIEnv*` 与客体可见的表/代理进行映射；不能凭结构相似直接复制整个函数表。
- [ ] 正确处理 `GetEnv`、Attach/Detach、guest 线程附着与每线程 `JNIEnv`，禁止跨线程复用 `JNIEnv*`。
- [ ] 支持基础引用、字符串、数组、方法查找和回调；正确传播 Java pending exception。
- [ ] 对 `Call*Method`、`Call*MethodV`、`Call*MethodA` 与对象构造路径分别测试。
- [ ] 对 `...` 和 `va_list` 路径，按方法签名解码 guest 参数，转换为 `jvalue[]` 后调用宿主 `A` 版本。
- [ ] Java→guest native entry 也需按签名封送；guest→Java 的成功不能证明反方向成功。
- [ ] `FindClass` 使用正确的 ClassLoader 上下文，覆盖依赖库与新建 guest 线程的场景。
- [ ] 保留引用生命周期，处理局部/全局引用、数组 pin/release、字符串 UTF 编码和异常清理。
- [ ] 静态/实例、重载、64 位整数、float/double、数组、对象及多层回调均有 fixture。
- [ ] `JNI_OnUnload`/模块卸载能力按实现范围明确标注；MVP 可选择进程级生命周期，不宣称完整动态卸载。

JNI 的表布局、可变参数入口与引用规则以规范为准。[JNI Functions](https://docs.oracle.com/en/java/javase/21/docs/specs/jni/functions.html)

### G3 验收

`hello-jni.apk` 必须覆盖：

- Java 调用客体函数，结果显示在 Activity 上。
- 客体调用 Java，包含字符串/数组、浮点/长整数、异常。
- 显式注册和导出名解析两种绑定方式。
- `JNI_OnLoad` 一次性语义、错误版本、依赖库中的 native 方法。
- guest 新建线程 attach/callback/detach，以及至少一层嵌套回调。
- 对尚不支持的 JNI 入口明确失败；不把宿主函数指针直接塞进客体表蒙混通过。

## 12. P5 — ANGLE、EGL 与 GLES

### 12.1 P5a：先证明宿主后端可用

- [ ] 固定 ANGLE commit、上游补丁及 GN 参数，构建 Windows ARM64 `libEGL.dll`/`libGLESv2.dll` 或该版本对应产物。
- [ ] 只应用经审计且仍适用的上游补丁；Metal 专用补丁不机械迁移。
- [ ] 原生 Windows 测试程序创建 Win32 窗口、EGL display/context/surface，通过 ANGLE/Vulkan 绘制图案。
- [ ] 显式记录实际使用的 renderer/backend、GPU、驱动和 EGL/GLES capabilities。
- [ ] Vulkan 不可用时，可在 `auto` 模式选择经测试的 D3D11；用户明确选择 Vulkan 时不得悄悄切换。
- [ ] Windows DLL 搜索路径与包内依赖确定，不依赖开发机 PATH 中偶然存在的 DLL。

### 12.2 P5b：接通 Framework 和客体 ABI

- [ ] 先验证 Java GLES 调用进入宿主 ANGLE，再接入客体 ELF 的 `libEGL.so`/`libGLESv2.so` 虚拟模块。
- [ ] 将 guest `EGLNativeWindowType`/`ANativeWindow` 映射到受管理的 Windows Surface 对象；不是裸指针强转。
- [ ] 管理 display/context/surface 生命周期、线程绑定、context current 状态与销毁顺序。
- [ ] `eglGetProcAddress` 返回 guest 可调用的桥入口；不对未知扩展返回裸宿主函数地址。
- [ ] 筛选 EGL/GLES 扩展和版本：只报告 ANGLE 支持且桥接层可用的交集。
- [ ] 验证 context loss、resize、最小化/恢复、surface 销毁后重建。
- [ ] 最初仅支持单个明确归属的渲染 Surface；View/SurfaceView 混合合成单独列为能力，不用多次提交到同一个窗口凑出效果。

首个客体图形 APK 采用普通 Java Activity + SurfaceView + JNI 的结构，由 Java 将 `Surface` 交给 native。实现受限的 `ANativeWindow_fromSurface`、acquire/release、尺寸查询和对象注册；`jobject`、guest `ANativeWindow*` 与 `HWND` 是不同对象，不能相互强转。首轮不依赖 `NativeActivity`/`android_native_app_glue`；它们需要额外的生命周期、ALooper/AInputQueue 适配，另开任务。

### 验收

- 宿主 ANGLE 三角形、Java GLSurfaceView 样例和 N2 客体 GLES 样例分别通过。
- 纹理上传、alpha blending、FBO 和简单 shader 得到确定结果；至少一次回读校验。
- 连续 resize/最小化/恢复无死锁、丢失上下文未处理或资源持续增长。
- 后端选择与日志一致；D3D11 通过不代表 Vulkan 路径通过。

## 13. P6 — Android Vulkan 到 Windows 原生 Vulkan

### 13.1 桥的职责

Vulkan GPU 命令和 SPIR-V 原则上交给 Windows 原生驱动；本层负责客体 ABI、Android API 视图与宿主 WSI。不能只替换 `vkCreateAndroidSurfaceKHR` 的函数名。

### 13.2 Loader 和 dispatch

- [ ] 使用可获得的 Windows ARM64 Vulkan Loader，通过正常 ICD 发现加载驱动，不硬编码某个 Qualcomm DLL。
- [ ] 为虚拟 `libvulkan.so` 提供 bootstrap、instance/device dispatch 和版本协商。
- [ ] 基于固定 `vk.xml` 生成签名/桥表或采用经过验证的等价方式；首轮可以只开放完成的命令集合。
- [ ] 若只实现 Vulkan 核心命令的受限子集，产品能力报告标为 fixture/PoC profile；不能据此宣称符合某个完整 Vulkan 版本或通过 Khronos 一致性认证。
- [ ] 审计所有返回函数指针的路径：`vkGetInstanceProcAddr`、`vkGetDeviceProcAddr`、扩展入口和回调。
- [ ] 仅当固定签名、结构布局和 ABI 证据齐全时允许直通；其余通过 thunk。
- [ ] 处理 guest `VkAllocationCallbacks` 和 debug callback 的回调 ABI、user data、寿命与线程上下文。
- [ ] 保持 dispatchable/non-dispatchable handles 的表示、映射和所有权一致，不在不同 device/instance 中串用。

### 13.3 Android Surface → Win32 Surface

- [ ] guest 枚举 `VK_KHR_android_surface`，host 请求 `VK_KHR_win32_surface`；实例创建时转换扩展列表并验证依赖。
- [ ] 只有 Surface 桥已经可用时才向 guest 宣告 Android Surface 扩展。
- [ ] 从 guest `ANativeWindow` 查找受管理的 `HWND`/`HINSTANCE`，正确构造 `VkWin32SurfaceCreateInfoKHR`。
- [ ] 保持 `ANativeWindow` acquire/release、HWND 与 `VkSurfaceKHR` 的独立生命周期。
- [ ] 查询 host queue family 的实际 present 支持；不沿用 Android 的默认呈现假设。
- [ ] 处理 surface formats、present modes、extent、DPI、最小化的零尺寸和窗口重新创建。
- [ ] 正确处理 `VK_ERROR_OUT_OF_DATE_KHR`、`VK_SUBOPTIMAL_KHR`、`VK_ERROR_SURFACE_LOST_KHR` 和 device lost。
- [ ] 重建交换链时等待正确的在途资源，避免销毁仍被 GPU 使用的对象。
- [ ] 消息泵保持响应；不在持有 JVM/渲染关键锁时阻塞等待窗口线程。

Win32 Vulkan WSI 可能通过窗口消息进行同步，消息线程被阻塞会导致呈现相关死锁。[vkCreateWin32SurfaceKHR 规范](https://docs.vulkan.org/refpages/latest/refpages/source/vkCreateWin32SurfaceKHR.html)

### 13.4 扩展与 AHardwareBuffer

- [ ] 分开维护 host 扩展、guest 已实现扩展和可翻译扩展表。
- [ ] 按需转换 `pNext` 链；对兼容层无法处理的已请求结构明确失败，不随意截断链。
- [ ] MVP 不暴露 `VK_ANDROID_external_memory_android_hardware_buffer`，也不伪造 AHardwareBuffer 成功。
- [ ] Android native fence fd、sync fd、DMA-BUF 等不能当作 Windows HANDLE 直接传递。
- [ ] 后续 AHardwareBuffer 互操作需要独立设计格式、usage、外部格式、handle 所有权、同步和进程范围。
- [ ] 不将 ANGLE 的私有 VkDevice 与应用 VkDevice 默认视为同一资源域。

Android Surface 是 `ANativeWindow` 对应的图形端点；AHardwareBuffer 扩展还涉及导入/导出与格式语义，两者不能等同于普通 Win32 窗口句柄。[Android Surface](https://docs.vulkan.org/refpages/latest/refpages/source/VK_KHR_android_surface.html)、[AHardwareBuffer 扩展](https://docs.vulkan.org/refpages/latest/refpages/source/VK_ANDROID_external_memory_android_hardware_buffer.html)

### G4 验收

1. 宿主 Win32 Vulkan 样例通过，用于确认 loader/ICD/WSI 环境。
2. 使用 Android NDK 构建的 N2 Vulkan 样例，经 guest `libvulkan.so` 和 Android Surface 入口完成呈现；不可用宿主样例代替。
3. 必测 instance/device 创建、command buffer、buffer/image、shader、swapchain 与多帧同步。
4. resize、最小化/恢复、surface 生命周期变化和窗口关闭后无悬挂任务。
5. 不支持的 Android 扩展在枚举/创建路径给出一致结果；请求后不能伪装成功。
6. guest 获取的函数指针、allocation/debug callbacks 有专门正例。
7. 可用的 ARM64 validation layers 下无未解决的测试路径错误；若该环境没有 layer，记录 `NOT_RUN`，不可写成“零错误”。

## 14. P7 — 打包、诊断与回归

### 14.1 运行入口与部署

- [ ] 提供 `doctor`、`inspect`、`run` 和 fixture test 入口；早期用 CLI 验证，实验版接入 MyGo 前端，实现主窗口内手机式窗格与独立窗口两种打开方式。
- [ ] 打包 runtime、JRE 或清晰的外部 JRE 配置、Framework JAR、module patches、资源、宿主 DLL 和依赖说明。
- [ ] 检查所有进程内 DLL 都是目标 ARM64 架构；不打包 `.dylib`/Mach-O 产物充当 Windows 依赖。
- [ ] GPU ICD 由驱动安装提供，不随意复制开发机驱动 DLL；Vulkan Loader 等可分发组件遵循其实际分发方式。
- [ ] 运行时子进程交给启动器管理；需要时用 Job Object 清理子进程，退出码明确。
- [ ] 干净用户配置、离线已有依赖、空格/中文安装路径下验证。
- [ ] 保存上游许可证、第三方 notices、源码与补丁来源；核对新增分发组件要求，不推断商业授权结论。
- [ ] 不把本次实验包注册为系统默认 APK 打开程序，不自动修改全局环境。

### 14.2 日志与错误模型

统一记录：时间、进程/线程、APK/package、阶段、模块、guest symbol/offset、backend、错误码与说明。

建议稳定错误类别：

| 错误类别 | 示例 |
| --- | --- |
| `HOST_ARCH_UNSUPPORTED` | 当前 runtime/JVM 不是 ARM64 |
| `DEPENDENCY_MISSING` | 缺少 JRE、资源包或已请求的 ANGLE 后端 |
| `APK_UNSUPPORTED` | 请求了尚未支持的包结构/ABI |
| `ELF_RELOCATION_UNSUPPORTED` | 指定模块的某个重定位类型未实现 |
| `GUEST_SYMBOL_UNRESOLVED` | 所需 Bionic/JNI 符号缺失 |
| `GUEST_ABI_UNSUPPORTED` | x18/TLS/可变参数或执行 profile 不满足 |
| `GRAPHICS_CAPABILITY_MISSING` | Vulkan ICD、present 或已请求扩展不可用 |
| `GUEST_EXECUTION_FAULT` | guest 模块偏移对应的运行故障 |

未知导入默认严格报错；能力缺失不能自动变成空实现。Minidump/详细内存诊断按需生成并保存在本地，默认日志不输出应用数据、令牌或完整环境变量。

### 14.3 macOS 回归

- [ ] 公共 Java/DEX 改动跑已有公开测试；Windows 分支不要污染 macOS 编译目标。
- [ ] 可用时执行 macOS 构建和代表性 smoke；没有 Mac 时明确标记未运行。
- [ ] 不以“Windows 通过”推断 macOS 无回归；列出公共代码变化与剩余验证任务。

### G5 验收

- 从干净源码与锁定依赖构建，或按文档取得可追溯二进制依赖；不使用开发机私有目录。
- A/B/C 每个层级的结果有测试日志、版本信息和样例哈希。
- 失败测试能够输出具体缺失能力，而不是黑窗、卡死或无说明退出。
- 有独立 Windows ARM64 机器的复现结果；若缺少第二台设备，明确实验覆盖只有单机。
- README/compatibility 报告准确说明支持范围、后端、默认开关和未完成项。

## 15. P8 — 可选后续扩展

按应用需求选择，不在前置关卡未通过时全面展开。

### Skia / AGSL / RenderEffect

- 移植 `NativeSkiaMetal`/`skia_metal_bridge.mm` 所代表的设备/上下文契约，建立平台中立的 GPU bridge。
- 核实可用 Skia/Skiko Windows ARM64 产物、导出 ABI 和对应源码；缺失时从固定版本构建。
- 先保留 Java2D Canvas + GPU shader/effect + 显式回读的分工，再测量是否值得重做整个 Canvas GPU 路线。
- 移植上游 `debug/AgslRenderingSmoke.java` 的像素契约；只替换 Metal-specific 断言，不删除颜色、矩阵、缓存失效和 RenderEffect 的语义检查。
- 记录真实 Skia backend；不可用时将 AGSL 能力标记不支持，不默默用无效果/空结果通过。

### 音频、媒体与其他宿主服务

- 根据目标 APK 优先级实现 WASAPI 音频输出；设备变化、格式和缓冲延迟单独测试。
- Media Foundation/其他解码组件先做 capability probe，避免承诺所有 Android codec。
- 剪贴板、文件选择器、IME、摄像头、传感器、WebView 各自建立平台接口与能力边界。

### 更广的 Native 和 ARM32

- 将 C++ guest runtime、复杂 TLS、信号和受控重写作为单独里程碑。
- ARM32 重编译除指令生成外，还涉及 Windows 产物、shim、地址宽度、JNI 和展开，不能仅重新编译上游 translator。
- 仅当 ARM64 路线稳定且存在明确样例需求时启动。

## 16. 测试资产与结果矩阵

所有 fixture 都应提供源码、构建参数、哈希、期望输出；不依赖私人 APK 集或无法获得的二进制。

| ID | 测试 | 验证重点 | 阶段 |
| --- | --- | --- | --- |
| T01 | `doctor` / 宿主 JNI hello | 进程/JVM/DLL 架构，环境诊断 | P0 |
| T02 | Java2D bitmap presenter | 像素格式、DPI、消息泵 | P1 |
| T03 | `hello-java.apk` | DEX、资源、Activity、输入、持久化 | P1 |
| T04 | `hello-kotlin.apk`（后加） | Kotlin 生成 DEX 与所需标准库 | P1 |
| T05 | N0 ELF leaf suite | 映射、重定位、整数/浮点/结构体 ABI | P2 |
| T06 | ELF malformed/unsupported suite | 提前拒绝和可定位错误 | P2 |
| T07 | N1 dependency/constructor suite | DT_NEEDED、符号、构造与重复加载 | P3 |
| T08 | pthread/errno/TLS suite | 线程隔离、析构、支持 model | P3 |
| T09 | JNI matrix APK | 动态注册、导出名、双向/多线程回调 | P4 |
| T10 | host ANGLE + Java GLES APK | 后端构建与 Java 图形路径 | P5 |
| T11 | guest GLES APK | N2 ABI、EGL 生命周期、函数指针 | P5 |
| T12 | host Vulkan sample | Loader/ICD/Win32 WSI 环境 | P6 |
| T13 | guest Vulkan APK | Android WSI 桥、dispatch、callbacks | P6 |
| T14 | resize/relaunch/cache stress | 长运行稳定性、生命周期、资源 | P7 |
| T15 | AGSL pixel contracts | Skia GPU/effect 语义 | P8 |

### 验证方法

- 解析/资源/Java 测试可在普通 CI 上运行；ARM64 执行、JVM 集成和 GPU 验证必须有对应原生环境。
- 使用自托管 ARM64 runner 或可用的 ARM64 CI；无桌面/GPU 的 runner 不承担当次窗口呈现验收。
- 用固定输入和结果断言证明行为；截图用于界面验证，不能代替数值、生命周期和错误断言。
- 图形读回测试使用确定性图案，声明像素容差；字体渲染不依赖不同机器上完全相同的像素哈希。
- 初始稳定性门槛：代表样例至少 20 次冷/热启动、100 次 resize/最小化恢复、一次 30 分钟运行；执行前可根据已观察到的问题增加针对性用例。
- 记录冷/热启动时间、转换时间、RSS/commit、handle/thread count、帧时间 p50/p95；先建立同机基线，不编造统一 FPS 保证。
- 在 warm-up 后比较重复循环资源曲线，检查持续增长；缓存保留要有明确上限，不能把所有内存增长都解释为正常。
- GLES/Vulkan 分别测量。选择同一 GPU API 不足以证明某个后端更快。

### 每次验收报告模板

```text
Build / upstream commit:
Windows build / CPU / RAM:
JDK vendor / version / architecture:
GPU / driver / Vulkan Loader / ANGLE commit:
Fixture ID / source commit / binary hash:
Native profile / graphics backend:
Command:
Expected result:
Actual result:
Status: PASS | FAIL | BLOCKED | NOT_RUN
Evidence:
Known limitations:
Next blocking issue:
```

## 17. 风险、决策与停止条件

| 风险 | 优先级 | 缩小风险的行动 | 触发后的处理 |
| --- | --- | --- | --- |
| x18/TPIDR/TLS 不能在目标二进制上可靠适配 | 最高 | P2 提前做受控实验，按 model 分层 | 保留 Java 交付；缩小 native profile 或提出独立重写研究 |
| JNI varargs/ClassLoader 语义错误 | 最高 | 双向参数矩阵、依赖库与线程测试 | 阻止扩展真实应用，修复桥契约 |
| 当前 Loader/Shim 深度绑定 Darwin | 高 | 先抽取 parser/HostMemory，小块替换 | 不整块移植；保留最小可运行分支 |
| ARM64 JDK/ANGLE/Skia 构建不可得 | 高 | P0/P5a 单独验证，锁定可复现源码构建 | 记录具体阻塞；Skia 可后置，JDK 不能伪造通过 |
| 当前设备 Vulkan 驱动/WSI 不可用 | 高 | 原生宿主 probe + guest probe 分开 | Java 可继续；GLES 可用已验证 D3D11，guest Vulkan 仍不支持 |
| Linux syscall/信号/C++ 展开需求扩大 | 高 | 显式符号/指令/用例清单 | 拒绝超出 profile 的应用，不用万能 stub |
| 多线程 WSI/JVM 死锁 | 高 | 线程归属、超时日志、resize/回调压力用例 | 保留最小复现，停止扩大图形范围 |
| “统一 Vulkan”引入错误资源共享 | 中 | 独立 device/resource 所有权 | MVP 不做共享，互操作单独设计 |
| 公共 Framework 改动破坏 macOS | 中 | 小提交、平台边界、双平台验证 | 回退隔离问题提交，不删除原实现 |

### 必须停止该依赖链的情形

- 需要破坏 Windows 保留寄存器或关闭全局安全机制才能运行样例。
- 数据布局、参数或返回值仍有未解释的偶发错误。
- 缺少真实 ARM64/GPU 运行证据却准备宣布对应关卡通过。
- 使用未实现函数的“假成功”让测试通过。
- 样例依赖未被证明的异常/TLS/系统调用语义。

停止的是受影响的依赖链，不是整个项目。继续可独立完成的构建、Java MVP、文档和宿主图形实验，并写出最小阻塞报告。

## 18. 构建与命令接口契约

下面是**待实现的目标接口**，不是上游当前已有命令。可以根据仓库风格调整名称，但文档、脚本和 CI 必须一致。

```powershell
# 盘点环境；只检查，不默默安装或修改系统配置。
.\scripts\windows\check-requirements.ps1

# 独立构建 Java / 宿主 / 原生实验 / 图形。
.\scripts\windows\build.ps1 -Configuration Debug -Component Java
.\scripts\windows\build.ps1 -Configuration Debug -Component Host
.\scripts\windows\build.ps1 -Configuration Debug -Component NativeProbe
.\scripts\windows\build.ps1 -Configuration Debug -Component Graphics

# 运行探测和包分析；inspect 不执行 APK 代码。
.\build\windows-arm64\bin\winapk.exe doctor --json
.\build\windows-arm64\bin\winapk.exe inspect .\tests\fixtures\hello-java.apk --json

# guest native 与 graphics 后端必须可显式选择。
.\build\windows-arm64\bin\winapk.exe run .\tests\fixtures\hello-java.apk --native=off
.\build\windows-arm64\bin\winapk.exe run .\tests\fixtures\hello-jni.apk --native=restricted
.\build\windows-arm64\bin\winapk.exe run .\tests\fixtures\hello-gles.apk --native=restricted --gles-backend=vulkan
.\build\windows-arm64\bin\winapk.exe run .\tests\fixtures\hello-vulkan.apk --native=restricted --guest-vulkan=on

# 按能力分组验收；脚本生成报告和复现命令。
.\scripts\windows\test.ps1 -Suite Java
.\scripts\windows\test.ps1 -Suite Native
.\scripts\windows\test.ps1 -Suite Graphics
.\scripts\windows\package.ps1 -Configuration Release
```

脚本要求：

- 所有外部程序返回码显式检查，失败时非零退出；路径用参数传递，不拼接不受控 shell 字符串。
- 构建输出与 fixture APK 统一 staging；`tests/fixtures/` 是建议的生成产物目录，源码仍在测试源码目录。
- 清理只针对已确认的构建目录，绝不通过宽泛通配符递归删除仓库或用户数据。
- 依赖下载须固定版本与校验；已具备依赖时支持离线构建。
- `doctor` 的 GPU window/present 测试若需要可见桌面，应明确显示测试模式；无桌面时返回未运行原因。
- `inspect` 仅解析 Manifest、DEX/native ABI 和已知依赖信息，不调用 ELF 构造函数、JNI_OnLoad 或 APK 初始化代码。

## 19. 提交粒度与最终交付清单

### 建议提交顺序

1. `docs: record Windows ARM64 port audit and acceptance gates`
2. `build: split Java and platform-specific native targets`
3. `runtime: add Windows host bootstrap and capability probe`
4. `runtime: add Win32 bitmap presentation and input`
5. `native: add constrained AArch64 ELF loader probe`
6. `native: add explicit guest ABI and basic Bionic contracts`
7. `jni: connect guest library lifecycle and bidirectional calls`
8. `graphics: add ARM64 ANGLE backend and GLES bridges`
9. `graphics: add guest Vulkan dispatch and Win32 WSI`
10. `packaging: add reproducible Windows experimental bundle and reports`

每个提交附对应测试结果；允许为修复和实验拆成更小提交。不在一个“Windows port”提交中混入整体重构、品牌改名和兼容性扩展。

### 最终完成清单

- [ ] P0 审计与依赖锁定完整，实际合入的上游 commit 可追溯。
- [ ] Windows ARM64 宿主构建与 Java 独立构建入口可用。
- [ ] G1：真实纯 Java APK 的界面、输入与持久化通过。
- [ ] G2：N0 ELF 与 ABI 测试在 ARM64 真机通过。
- [ ] G3：受支持 N1 的 Bionic/TLS/JNI 测试通过，限制明确。
- [ ] G4：guest GLES 与 guest Vulkan 路径分别通过，不用宿主样例代替。
- [ ] G5：干净环境实验包、文档、日志和兼容性报告完成。
- [ ] 产品显示名称为 `Windows-ARM64.apk`；MyGo 前端按设计 Agent 参考 Kyokai 的方案实现，App 在主窗口窗格内和独立窗口中均可打开、交互及关闭。
- [ ] 所有未实现能力有明确错误或准确的 capability 值。
- [ ] macOS 回归状态如实记录；未跑的检查标为 `NOT_RUN`。
- [ ] 源码、测试、构建说明、第三方 notices 和实验限制一并交付。

如果只完成部分关卡，按层级交付可用成果和阻塞报告，不能勾选整项“Windows ARM64 移植完成”。

## 20. 首次工作会话应立即完成的事情

1. 阅读仓库规范，记录工作树状态，定位本计划列出的上游入口。
2. 生成 P0 审计初稿和锁定工具链清单，核实 ARM64 JDK/JNI 与 Vulkan 环境。
3. 将 Java-only 构建从 macOS 原生构建前置依赖中拆出，先验证 JAR 与资源产物。
4. 创建最小 Windows ARM64 宿主和 `doctor`，证明 JVM 可嵌入。
5. 建立 N0 fixture 与独立 ELF/ABI probe 的最小闭环，尽早给出 native 路线的第一份证据。
6. 汇报实际改动、执行命令、通过/失败项及下一项具体阻塞；随后按依赖图继续。

**第一轮优先交付两个独立证据：Java APK 路线确实能在 Windows ARM64 JVM 上启动，以及受限 Android ARM64 ELF 路线确实能在 Windows ARM64 进程中正确执行。图形集成建立在这些证据之上。**
