# PLAN：Windows-ARM64.apk — 将 Mac.apk 移植到 Windows ARM64

> 交付对象：负责实施的 Coding Agent。本文是一份实施计划，不代表移植已完成或已验证。
>
> 目标：在 Windows 11 ARM64 上，以 Windows 原生进程运行选定的 Android APK；复用 Mac.apk 的 DEX → JVM 与 Android Framework 实现，逐步建立 Android ARM64 ELF、Bionic、JNI 和图形兼容层。
>
> 图形方向：Android Vulkan → ABI/WSI 适配 → Windows 原生 Vulkan；Android OpenGL ES/EGL → ANGLE → Vulkan。ANGLE/D3D11 是独立、可配置的备选后端。

> **状态（2026-11-17）：开发进行中（版本 0.1.0，尚未发布）。** G0–G3 已通过所声明的 profile；P3/P4 的契约与限制见 `doc/en-us/native-profiles.md`，当前按依赖推进 P5/P6；厂商 Vulkan 驱动仍缺失。
> 当前进度、上游版本与已验证项见 `doc/zh-cn/README.md` 与 `doc/zh-cn/platform-notes.md`。

- 编写日期：2026-11-03。
- 上游仓库：[kksimp/Mac.apk](https://github.com/kksimp/Mac.apk)。
- 初稿核查时的上游提交：[`f87be03575a79c7eeed578f421ee852bf99a814b`](https://github.com/kksimp/Mac.apk/tree/f87be03575a79c7eeed578f421ee852bf99a814b)（2026-10-29 UTC）；当前实际合入的上游提交与复核结果见 `build-lock.json` 与 `doc/en-us/port-audit.md`。
- 上游版本不固定：Mac.apk 始终跟随上游最新 `main`（以 git subtree 引入 `macapk/`），Go 与 MyGo 始终跟随最新发行版；每次合入后按 `doc/en-us/upstream.md` 与 `doc/en-us/port-audit.md` 复核并重跑受影响关卡。
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
- **用户已确定的设计决定（2026-11-03，设计稿见 `design/`）：**
  1. 与 Kyokai 一致：Library 是主页（Home）窗格；每个打开的 App 是主窗口内的独立小窗格，可同时打开多个、分组并排/上下拆分，也可移到独立窗口。
  2. App 在窗格与独立窗口之间移动时尽量不重启，具体取决于运行时能否转移窗口/Surface；做不到时改为“在…中重新打开”。
  3. 关闭主窗口时，窗格中的 App 随之停止，独立窗口中的 App 默认继续运行；窗格中仍有运行的 App 时先提醒确认。
  4. 应用数据目录允许用户移动，具体取决于运行时支持。
  5. 原生执行 profile（N0/N1/N2）在高级设置中可调整（单个 App 与默认值）。
  6. 找不到 Java 时，程序自行安装一个仅供本程序使用的独立 Java 运行时（JRE 或 JDK，放在自己的目录），不修改系统 Java、PATH 或环境变量；APK 检查是否需要 JVM 取决于运行时实现。
  7. 诊断与截图中可以显示真实 GPU 名称。
  8. 诊断（Diagnostics）与 Kyokai 的状态窗格一致，作为窗格出现在“Open”列表中，而不是 Settings 那样的整页面板。
  9. 独立 Java 运行时的来源不限（设计稿默认 Microsoft Build of OpenJDK 21，任一合适的 ARM64 构建均可），可以自动更新（默认开启，校验同首次下载）；程序本身仍不自动更新。

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

### 4.4 仓库目录映射

Mac.apk 以 git subtree 位于 `macapk/`，文档统一维护在 `doc/en-us/` 并镜像到 `doc/zh-cn/`（不另建 `docs/windows-arm64/`）：

```text
doc/en-us/（及镜像 doc/zh-cn/）
  port-audit.md
  abi-compatibility.md
  native-profiles.md
  decisions.md
  platform-notes.md
  gui.md
macapk/runtime/platform/
  windows/               # winapk CLI、JVM/Vulkan 探测、Win32 桥、native-probe
macapk/runtime/guest/
  loader/                # 受限 ELF 加载器（已实现 N0）
  abi/                   # 按阶段新增（P3+）
  bionic/                # 按阶段新增（P3）
  jni/                   # 按阶段新增（P4）
  graphics/              # 按阶段新增（P5b/P6）
scripts/windows/         # 环境检查、构建、测试（PowerShell 7）
tests/windows-arm64/     # 自建 APK 与客体 ELF 夹具源码
tests/fixtures/          # 生成的测试夹具与哈希记录（不提交）
build/windows-arm64/     # 构建与测试输出（不提交）
```

已有 `macapk/runtime/translator/` 不删除或改名；Windows 目标仅复用经审计的数据结构，逐步建设独立客体组件。

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
P0 盘点/依赖/探测（已完成，G0 PASS）
 ├─ P1 纯 Java 启动与 Win32 基础界面（已完成，G1 PASS） ─────────┐
 ├─ P2 ELF + ABI 最小实验（已完成，G2 PASS） → P3 Bionic/TLS → P4 JNI ┤
 └─ P5a 宿主 ANGLE/Vulkan 构建与呈现 ─────────────────────────────┤
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

## 7. P0 — 仓库盘点、工具链与能力探测（已完成，G0 通过）

P0 与 G0 已完成并通过真机验证；长期契约已迁入概念文档：

- **移植审计与上游耦合：** `doc/en-us/port-audit.md`、`doc/en-us/upstream.md`。
- **依赖锁定与环境检查：** `build-lock.json`、`scripts/windows/check-requirements.ps1`、`doc/en-us/development.md`。
- **宿主工具链、JDK、进程结构与 JNI 方向：** `doc/en-us/decisions.md`（ADR-0001）。
- **`winapk doctor` 与 T01 验证证据：** `doc/en-us/platform-notes.md`（含本机仅具 Mesa Dozen 转换层 Vulkan 驱动的事实及 P5/P6 前置要求）。
- **后续阶段延续项：** 自建夹具 T01–T06 已完成，T07–T15 按 §16 随 P3–P8 建立；ANGLE 与 Skia 分别在 P5a 与 P8 锁定构建。

## 8. P1 — 纯 Java APK 与 Win32 基础界面（已完成，G1 通过）

P1 与 G1（含 G1.6 Kotlin 夹具，100% 与 200% 缩放）已完成并通过真机验证；长期契约已迁入概念文档：

- **独立构建与平台资源：** `scripts/windows/build.ps1`（`Java`、`FrameworkRes`、`Fixtures`、`Host`）与 `doc/en-us/development.md`。
- **宿主桥、窗口/消息线程与构建标识：** `doc/en-us/port-audit.md`、`doc/en-us/decisions.md`（ADR-0002 `NativeCocoa` 过渡绑定与 UI 线程超时机制、ADR-0003 `winapk dex` UTF-16 参数与完整 JAR 摘要标识）。
- **G1 逐项验收证据与输入/DPI 边界：** `doc/en-us/platform-notes.md`。
- **后续阶段延续项：** MyGo 前端对接 `winapk` 运行时进程与窗格/独立窗口呈现归属 P7（§14.1，契约草案见 `doc/en-us/gui.md`）；运行中动态改变显示器缩放时的 Android 配置变更与真实输入法组字会话仍为 `NOT_RUN`（见 `doc/en-us/platform-notes.md`）；其余未绑定宿主 native（如 `NativeOs`、宿主字体）随目标样例按需实现。

## 9. P2 — 提前验证 ELF 加载与 ARM64 ABI（已完成，G2 通过）

P2 与 G2（独立 `native-probe.exe` 上的 profile N0、ABI 正例 T05、提前拒绝负例 T06 及 2000 次循环/32 副本压力测试）已完成并通过真机验证；长期契约已迁入概念文档：

- **N0 准入条件、加载顺序、重定位支持表、静态筛查与拒绝类别：** `doc/en-us/native-profiles.md`。
- **宿主 ↔ 客体调用约定对比及直通 / 需 thunk / 不支持签名表：** `doc/en-us/abi-compatibility.md`。
- **G2 验收证据：** `doc/en-us/platform-notes.md`。
- **P3/P4 延续项已落实：** 使用静态 PE JNI 入口而非动态生成 thunk；Windows 展开信息、16 字节对齐及显式 GFID 均已核实，启用 CFG 的真实进程回归通过（`doc/en-us/abi-compatibility.md`、`platform-notes.md`）。`winapk run` 中客体原生仍默认关闭；不宣称客体 C++ 展开或任意控制流安全。

## 10. P3 — Bionic、线程、TLS 与客体模块语义

P3 首批已声明 C/Bionic 子集完成并于 2026-11-17 真机验证。长期契约移至
`doc/en-us/native-profiles.md`（线程/errno/TLS、同步、私有堆/字符串、文件/时间、受限格式化/日志、
依赖/版本/引用/CRT 生命周期与故障边界），ABI 证据见 `doc/en-us/abi-compatibility.md`。

验收记录见 `doc/en-us/platform-notes.md`：T07/T08 覆盖关键正负路径，包括 8/32 线程隔离及清理、
中文/空格文件路径、模块事务、普通 NDK 驱动链接的 C 库、限定系统库及 `LIBC` 版本需求、
构造前失败回滚与只识别客体 PC 的致命故障。完整回归 Host 22、Java 43、Native 58、Bionic 104 项通过，
不另加重复压力套件。

这只证明文档声明的 C/Bionic 范围及所记录的源码/构建参数；不表示任意 NDK/CMake 输出、完整 N1 或 G3 通过。
P4 继续验证 JNI/APK 接入、线程 JNI 状态清理及 JVM/客体混合故障；客体 C++ 展开、复杂 TLS 与完整动态卸载不在该子集中。
前端继续暂缓，不改变 §6 的运行时依赖顺序。

## 11. P4 — JNI 与 APK 原生库集成

完成（2026-11-17）：G3 在已声明 C/Bionic/JNI 子集通过，不扩张为任意 NDK APK 兼容。
长期契约与排除项见 `doc/en-us/native-profiles.md`，ABI/CFG 见 `abi-compatibility.md`，
完整报告及源码/哈希证据见 `platform-notes.md`。独立 Jni 24 与真实 APK Activity 矩阵 ApkJni 8
组合覆盖双向值/异常、显式/导出绑定、依赖/版本/一次性 hook、worker/嵌套回调及不支持 JNI 的真实终止。
requirements/build All、Host 23、Java 43、Native 58、Bionic 104、Jni 24、ApkJni 8 均通过；
独立 CFG Debug 构建也通过同一回归，运行中策略开启，64 个 PE 入口全部在有效目标表。
普通构建已 clean-first 恢复并核对实际 CFG=0。客体 C++、ELF TLS、动态卸载及额外加载器仍明确不支持；
API-24 紧凑 RX 元数据布局仍拒绝，APK 夹具使用普通 NDK API-30 链接。未发布、未擅自推送。

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

- [x] 宿主 `JavaVM*` / `JNIEnv*` 与客体可见的表/代理进行映射；不能凭结构相似直接复制整个函数表。
- [x] 正确处理 `GetEnv`、Attach/Detach、guest 线程附着与每线程 `JNIEnv`，禁止跨线程复用 `JNIEnv*`。
- [x] 支持基础引用、字符串、数组、方法查找和回调；正确传播 Java pending exception。
- [x] 对 `Call*Method`、`Call*MethodV`、`Call*MethodA` 与对象构造路径分别测试。
- [x] 对 `...` 和 `va_list` 路径，按方法签名解码 guest 参数，转换为 `jvalue[]` 后调用宿主 `A` 版本。
- [x] Java→guest native entry 也需按签名封送；guest→Java 的成功不能证明反方向成功。
- [x] `FindClass` 使用正确的 ClassLoader 上下文，覆盖依赖库与新建 guest 线程的场景。
- [x] 保留引用生命周期，处理局部/全局引用、数组 pin/release、字符串 UTF 编码和异常清理。
- [x] 静态/实例、重载、64 位整数、float/double、数组、对象及多层回调均有 fixture。
- [x] `JNI_OnUnload`/模块卸载能力按实现范围明确标注；MVP 可选择进程级生命周期，不宣称完整动态卸载。

JNI 的表布局、可变参数入口与引用规则以规范为准。[JNI Functions](https://docs.oracle.com/en/java/javase/21/docs/specs/jni/functions.html)

### G3 验收

PASS（2026-11-17，所声明的 N1 C/JNI 子集）；以下全部由真实 APK/窗口路径与独立 ABI/JNI 回归共同覆盖。

`hello-jni.apk` 必须覆盖：

- Java 调用客体函数，结果显示在 Activity 上。
- 客体调用 Java，包含字符串/数组、浮点/长整数、异常。
- 显式注册和导出名解析两种绑定方式。
- `JNI_OnLoad` 一次性语义、错误版本、依赖库中的 native 方法。
- guest 新建线程 attach/callback/detach，以及至少一层嵌套回调。
- 对尚不支持的 JNI 入口明确失败；不把宿主函数指针直接塞进客体表蒙混通过。

## 12. P5 — ANGLE、EGL 与 GLES

2026-11-17：用户明确允许现有 ARM64 Mesa Dozen（Vulkan→D3D12）实验，见 ADR-0004。
该路径只记为转换层，不记为厂商 Vulkan 验证；后端与硬件/软件设备结果分别记录，G4 仍未验收。
P5a 固定 ANGLE `801a1a5f6e00ed0028937067b7663fd7053bff31`，ARM64 源码构建通过。
`GraphicsHost` 6 项 PASS（2.38 秒）：Adreno 8cx Gen 3 上分别验证 Vulkan/Dozen 和 D3D11，
两路均完成真实窗口 resize、shader 回读和 EGL 销毁；屏蔽子进程 Vulkan ICD 后显式 Vulkan 退出 2，
未回退到 D3D11。厂商 Vulkan 仍 BLOCKED，打包和 G4 均未验收；`auto` 的 Java 路径另由 T10.java 验证。

### 12.1 P5a：先证明宿主后端可用

- [x] 固定 ANGLE commit、上游补丁及 GN 参数，构建 Windows ARM64 `libEGL.dll`/`libGLESv2.dll`。
- [x] 只应用经审计的 Chromium 构建补丁；不迁移 Metal 补丁。
- [x] 原生 Windows 测试程序创建 Win32 窗口、EGL display/context/surface，通过 ANGLE/Vulkan（Dozen）及 D3D11 绘制图案。
- [x] 宿主验证记录实际 renderer/backend、GPU、驱动和 EGL/GLES capabilities，见双语图形能力文档。
- [x] Java `GLSurfaceView` 在 Vulkan 不可用时，`auto` 选择经测试的 D3D11；显式 Vulkan 不回退。
- [ ] Windows DLL 搜索路径与包内依赖确定，不依赖开发机 PATH 中偶然存在的 DLL。

### 12.2 P5b：接通 Framework 和客体 ABI

- [x] T10.java：真实 APK 的受限 Java GLES20 已验证两色回读、resize、最小化/恢复、不支持调用拒绝及销毁。
- [x] T10.surface：真实 APK 已验证受限 `libandroid.so`、holder 身份、引用、resize 和换挂载。
- [x] T11：客体 `libEGL.so`/`libGLESv2.so` 已接入，D3D11 与 Vulkan/Dozen 分别验证。
- [x] 将 guest `EGLNativeWindowType`/`ANativeWindow` 映射到受管理的 Windows Surface 对象；不是裸指针强转。
- [x] T11 的单 display/context/surface profile 管理归属、current 和销毁顺序。
- [x] `eglGetProcAddress` 仅返回 guest 可调用的固定桥入口；未知入口返回空。
- [x] T11 不暴露硬件私有 EGL/GLES 扩展，仅报告受限 EGL 1.4 / ES2 profile，不作一致性声明。
- [x] T11 验证 resize、surface 销毁后重建及原 context 保留。
- [ ] 客体 GLES 真实 context loss、最小化/恢复及长期稳定性未验收。
- [x] 只支持单个明确归属的渲染 Surface；不提供 View/SurfaceView 混合合成。

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

- [x] T13.wsi 使用 System32 ARM64 Vulkan Loader 正常发现 ICD；不硬编码厂商驱动。
- [x] T13.wsi 验证实例与物理设备令牌、实例 proc 分发、Android→Win32 surface、实际呈现支持查询及 resize/换挂载；不呈现帧。
- [x] T13 清屏增量验证客体设备/队列、获取 fence、命令缓冲区、交换链三帧呈现及 resize 后重建；不覆盖 shader、通用缓冲区/图像或回调。
- [x] 为虚拟 `libvulkan.so` 提供上述受限 profile 的 bootstrap、instance/device dispatch 及版本拒绝。
- [ ] 基于固定 `vk.xml` 生成签名/桥表或采用经过验证的等价方式；首轮可以只开放完成的命令集合。
- [ ] 若只实现 Vulkan 核心命令的受限子集，产品能力报告标为 fixture/PoC profile；不能据此宣称符合某个完整 Vulkan 版本或通过 Khronos 一致性认证。
- [ ] 审计所有返回函数指针的路径：`vkGetInstanceProcAddr`、`vkGetDeviceProcAddr`、扩展入口和回调。
- [ ] 仅当固定签名、结构布局和 ABI 证据齐全时允许直通；其余通过 thunk。
- [ ] 处理 guest `VkAllocationCallbacks` 和 debug callback 的回调 ABI、user data、寿命与线程上下文。
- [ ] 保持 dispatchable/non-dispatchable handles 的表示、映射和所有权一致，不在不同 device/instance 中串用。

### 13.3 Android Surface → Win32 Surface

- [ ] guest 枚举 `VK_KHR_android_surface`，host 请求 `VK_KHR_win32_surface`；实例创建时转换扩展列表并验证依赖。
- [ ] 只有 Surface 桥已经可用时才向 guest 宣告 Android Surface 扩展。
- [x] 从 guest `ANativeWindow` 查找受管理的 `HWND`/`HINSTANCE`，构造 `VkWin32SurfaceCreateInfoKHR`。
- [x] 保持 `ANativeWindow` acquire/release、HWND 与 `VkSurfaceKHR` 的独立生命周期，验证换挂载后失效。
- [x] 查询实际 surface/queue family 的 present 支持，不沿用 Android 默认呈现假设。
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
- [x] 本地 CLI 包包含 Release runtime、Framework JAR、module patches、资源、宿主 DLL 和外部 ARM64 JRE/VC++ 依赖说明；不含 ANGLE。
- [x] 验证包内 PE 为 ARM64，并拒绝 Debug CRT 导入；源码 ZIP 也排除 macOS `.dylib` 预编译件。
- [x] 本地 CLI 包不携带 GPU ICD 或 Vulkan loader。
- [ ] 运行时子进程交给启动器管理；需要时用 Job Object 清理子进程，退出码明确。
- [x] 在含空格/中文的解压路径和全新数据/配置根目录用已有依赖执行 T14.package；打包不下载。
- [ ] 公开分发前完成许可审核，含 ANGLE 实际链接依赖；本地包已有许可证、notices、源码快照和哈希。
- [x] 不注册默认 APK 打开程序，不自动修改全局环境。

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
| T10 | host ANGLE + Java GLES APK + Surface APK | 后端构建、Java 图形、客体 native window 寿命（分别验收） | P5 |
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

已实现的命令以 `doc/en-us/development.md` 为准。下面列出后端的构建、运行和定向验证示例；受限图形及本地 CLI 包不等于 G4/G5 验收：

```powershell
# 盘点环境；只检查，不默默安装或修改系统配置。
.\scripts\windows\check-requirements.ps1

# 独立构建 Java / 宿主 / 原生实验 / 图形。
.\scripts\windows\build.ps1 -Configuration Debug -Component Java
.\scripts\windows\build.ps1 -Configuration Debug -Component Host
.\scripts\windows\build.ps1 -Configuration Debug -Component NativeProbe
.\scripts\windows\build.ps1 -Component Angle
.\scripts\windows\build.ps1 -Component AngleProbe

# 运行探测和包分析；inspect 不执行 APK 代码。
.\build\windows-arm64\bin\winapk.exe doctor --json
.\build\windows-arm64\bin\winapk.exe inspect .\tests\fixtures\hello-java.apk --json

# guest native 与 graphics 后端必须可显式选择。
.\build\windows-arm64\bin\winapk.exe run .\tests\fixtures\hello-java.apk --native=off
.\build\windows-arm64\bin\winapk.exe run .\tests\fixtures\hello-jni.apk --native=restricted
.\build\windows-arm64\bin\winapk.exe run .\tests\fixtures\hello-gles.apk --native=off --graphics=vulkan
.\build\windows-arm64\bin\winapk.exe run .\tests\fixtures\hello-vulkan-wsi.apk --native=restricted --graphics=vulkan

# 按能力分组验证。
.\scripts\windows\test.ps1 -Suite Java
.\scripts\windows\test.ps1 -Suite Native
.\scripts\windows\test.ps1 -Suite GraphicsHost
.\scripts\windows\test.ps1 -Suite GraphicsJava
.\scripts\windows\test.ps1 -Suite Surface
.\scripts\windows\test.ps1 -Suite GraphicsGuest
.\scripts\windows\test.ps1 -Suite VulkanWsi

# 本地 CLI 包；先暂存或提交预期源码，包不发布。
.\scripts\windows\build.ps1 -Component All
.\scripts\windows\build.ps1 -Configuration Release -Component Host
$zip = .\scripts\windows\package.ps1
.\scripts\windows\test.ps1 -Suite Package -PackageZip $zip
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

- [x] P0 审计与依赖锁定完整，实际合入的上游 commit 可追溯。
- [x] Windows ARM64 宿主构建与 Java 独立构建入口可用。
- [x] G1：真实纯 Java APK 的界面、输入与持久化通过。
- [x] G2：N0 ELF 与 ABI 测试在 ARM64 真机通过。
- [x] G3：受支持 N1 的 Bionic/TLS/JNI 测试通过，限制明确。
- [ ] G4：guest GLES 与 guest Vulkan 路径分别通过，不用宿主样例代替。
- [ ] G5：干净环境实验包、文档、日志和兼容性报告完成。
- [ ] 产品显示名称为 `Windows-ARM64.apk`；MyGo 前端按设计 Agent 参考 Kyokai 的方案实现，App 在主窗口窗格内和独立窗口中均可打开、交互及关闭。
- [ ] 所有未实现能力有明确错误或准确的 capability 值。
- [ ] macOS 回归状态如实记录；未跑的检查标为 `NOT_RUN`。
- [ ] 源码、测试、构建说明、第三方 notices 和实验限制一并交付。

如果只完成部分关卡，按层级交付可用成果和阻塞报告，不能勾选整项“Windows ARM64 移植完成”。

## 20. 基线里程碑状态与当前起点

P0 审计/锁定、Java MVP（G1）、N0 ELF/ABI（G2）及已声明 C/Bionic/JNI 子集与真实 APK 集成（G3）均已完成，证据见 `doc/en-us/platform-notes.md`。层 A、B 在该范围 PASS；不是整个后端完成。P5 的受限 T11 客体 EGL/GLES 已通过，真实 context loss、客体最小化/恢复及长期稳定性未验收。接下来推进 §13 客体 Vulkan，再做 §14 后端交付；本机只有 Mesa Dozen 转换层 Vulkan，厂商 Vulkan 路径需设备。前端暂缓；不提前勾选 G4/G5 或发布。
