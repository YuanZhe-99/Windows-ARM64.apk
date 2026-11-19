# 决策

计划要求记录的架构决策（`PLAN.md` §1.9、§4.1、§11.1），按时间顺序排列。每项说明决定了什么、为什么，
以及什么情况下重新讨论。

## ADR-0001：宿主工具链、JDK、进程结构和 JNI 方向

2026-11-03，P0。状态：已采纳。

**工具链。** Windows 宿主在 ARM64 上用 Visual Studio 2026 自带的工具原生构建：目标为
`aarch64-pc-windows-msvc` 的 clang-cl 22.1.3、MSVC 14.51 的链接器和库、Windows SDK 10.0.26100、CMake
4.3.1 和 Ninja 1.13.2。第三方 C/C++ 代码以 manifest 模式、锁定的基线从 Visual Studio 内置的 vcpkg 获取
（`macapk/runtime/platform/windows/vcpkg.json`）。产物只有 ARM64 PE；`windows.cmake` 拒绝其他目标，构建会
检查它生成的每个 PE。上游的 `runtime/CMakeLists.txt` 增加一个 `if(WIN32)` 分支，引入
`platform/windows/windows.cmake` 并在 macOS 项目之前返回，因此 macOS 构建读到的内容与之前完全相同。理由：
所有工具都已在构建机上且是原生的；clang-cl 符合计划（§4.1）和上游代码的 clang 方言。

**JDK。** 适用于 ARM64 的 Microsoft Build of OpenJDK 21（21.0.13），开发用，通过 winget 安装。Java 侧不经
修改即可用它编译；T01 证明它的 `jvm.dll`（ARM64）能嵌入原生宿主，并接受上游的模块选项、双向 JNI 调用、
Java2D 和字体。JDK 21 是上游的默认版本；界面提供安装的私有运行时默认也是这个发行版（`PLAN.md` §2.4）。如果
后续阶段发现不兼容，则重新讨论（计划提到上游可选迁移到 JDK 25）。

**进程结构。** `winapk.exe` 是一个原生 ARM64 程序：`doctor` 和 `inspect` 是短暂运行；`run`（P1）是每个 APK
一个的运行时进程，由前端启动，并通过 `internal/winapk` 的契约与之通信。JVM 从运行时选定的文件夹加载
（`LoadLibraryEx` 加载 `bin\server\jvm.dll`，从不链接 `jvm.lib`），因此 Windows-ARM64.apk 自己的运行时、
用户选择的文件夹或开发用 JDK 都一样可用；模拟运行的 JVM 会被拒绝。

**JNI 方向。** 宿主 native 显式注册（`RegisterNatives`），如 T01 探针所做：在 Windows 上，JVM 只在
`System.loadLibrary` 加载的库中查找 `Java_*` 名称，从不在嵌入它的程序中查找。P1 把宿主 native 放进一个带
注册表的宿主桥 DLL（计划的默认方案，§4.1），使应用的运行时和由 `java.exe` 启动的测试 JVM 都能用到同样的
native。客体 `.so` 绝不经过 `LoadLibrary`：它们交给客体加载器和按 ClassLoader 隔离的客体库注册表（§4.1，
P2–P4）。

**P0 中发现的影响。** 这台电脑的 Vulkan 是基于 Direct3D 12 的 Mesa Dozen，不是厂商驱动。P0–P4 都不依赖
它。在 P5/P6 之前，项目需要一台有厂商 Vulkan 驱动的电脑来做图形关卡，或者记录一项决定：在 Dozen 上测量
客体 Vulkan 并如实报告（绝不当作原生 Vulkan）。

## ADR-0002：NativeCocoa 作为过渡期的宿主窗口接口

2026-11-03，P1。状态：已采纳。

**决定。** 在 Windows 上，Java 侧继续调用 `com.macapk.loader.NativeCocoa`；同样的 Java 声明通过 `RegisterNatives`
绑定到 `winapk-bridge.dll` 中的 Win32 代码（`window.cpp`）。`PLAN.md` §8.2 允许等价的适配层，并要求说明保留的
`NativeCocoa` 名称是过渡期接口：它就是，当出现第二个调用方或第二种窗口后端需要时，由平台中立的 `HostWindow` 接口（§4.3）
取代。没有 Windows 实现的 native 保持未绑定，在被调用处以 `UnsatisfiedLinkError` 失败。

**理由。** Framework 在 26 个文件中调用 `NativeCocoa`；重命名或包装这些调用会是一次大范围的上游改动，却不带来任何行为上
的收益，违背 `AGENTS.md` 保持上游改动小的规则。在 JNI 层绑定可以让上游的 Java 保持不变。

**线程和帧。** JVM 主线程调用 `init` 和 `runLoop`，成为拥有所有窗口的 UI 线程；其他线程创建或改变窗口的调用会投递给它
并等待：每等待 2 秒记录一次 `UI_THREAD_BLOCKED`；`runLoop` 运行后，10 秒内仍未开始执行的调用以 `UI_CALL_TIMEOUT`
放弃，因此持有 UI 线程所等待的锁的 JNI 调用方不会让应用挂起（`runLoop` 开始之前等待不设上限；它结束之后调用立即失败）。
最小化的窗口以 2 Hz 绘制，即上游对隐藏窗口的节流；恢复窗口时调用上游取消隐藏时的钩子 `ActivityHost.macapkTaskToFront`。每个窗口在 UI 线程上有一个 16 ms 的帧定时器：尺寸改变时调用渲染器的
`macapkSurfaceChanged`，然后调用 `macapkDrawFrame`，与 macOS 的 display link 一样；`presentBitmap` 在锁内复制 ARGB
帧并使窗口失效，`WM_PAINT` 以 32 位自上而下的 DIB 绘制它（ARGB 整数在内存中就是 BGRA 字节，所以无需转换；不做 alpha
合成）。输入在 UI 线程上调用 `TouchDispatcher`。

**单位。** 一个点是一个 Windows DIP（1/96 英寸），也是一个 dp：`winapk run` 以 density = 160 × 系统 DPI 缩放启动运行时，
所以应用的像素就是窗口的像素。

**重新讨论**：当窗格交接（界面在自己的窗口中显示应用）或 GLES surface 需要不止一个呈现目标时，或帧节奏应当跟随显示器
（DWM）而不是定时器时。

## ADR-0003：Windows 上的构建标识与转换器进程

2026-11-04，P1。状态：已采纳。

**决定。** `winapk run` 以 APK 的 SHA-256，以及转换器 jar（`macapk-loader.jar`）和框架 jar
（`android-framework.jar`）整个文件的 SHA-256，作为应用转换后代码（`classes.jar` 及其缓存戳）的键，并导出
`MACAPK_BUILD_JAVA_ID`（这两者摘要的 16 位十六进制），与上游 `main.cpp` 从 `build-id.txt` 导出它一样：它是应用自己的
动态 DEX 缓存（`dalvik.system.MacApkDexConverter`）的键。DEX 转换器在子进程 `winapk dex` 中运行，该进程用 Invocation API
创建 JVM，以 UTF-16 字符串传递路径，而不是在子进程 `java.exe` 中运行；`winapk.exe` 在其 manifest 中声明 UTF-8 活动代码页。

**理由。** 上游的 `build_ids.py` 把 `JAVA_ID` 收窄为转换器自身的字节码和框架的类形状，因此框架方法体的改动不会使已转换的
应用失效；移植它需要 Python，或者再实现一遍它的摘要算法。以整个 jar 为基础（上游以前的 `JAVA_ID`，现在用于诊断的
`JARS_ID`）永远不会过期，只是重新转换得更频繁。子进程 `java.exe` 以 ANSI 代码页接收参数，HotSpot 也以它读取 JVM 选项字符串：
路径中含有该电脑旧代码页之外的字符（在中文系统上的韩文）时，两者都会出错。使用 UTF-8 活动代码页后，选项字符串可以携带任何路径。

**重新讨论**：当只改动框架后的重新转换代价大到值得优化时（届时移植收窄的摘要），或当 P7 的私有运行时改变转换器的启动方式时。

## ADR-0004：明确标注的 Dozen Vulkan 实验

2026-11-17，P5/P6。状态：G3 之后经用户接受。

**决定。** 使用本机已有 ARM64 Mesa Dozen Vulkan→Direct3D 12 实现进行图形实验。
报告为 Vulkan 转换层路径，绝不称为 GPU 厂商 Vulkan ICD 验证。
不安装/复制驱动，不改变系统 ICD 选择。硬件与软件设备分开；软件结果不能证明 Adreno GPU 路径。

**范围。** ANGLE/Vulkan、ANGLE/D3D11 与客体 Vulkan 的结果保持独立。
显式 Vulkan 选择不能悄悄变成 D3D11。每个图形关卡仍须满足真实渲染/回读、生命周期及错误契约；
允许该驱动路径不等于 G4 通过、Khronos 一致性或允许宣告未实现的扩展。
ANGLE 与客体 Vulkan 仍保持独立 device/资源所有权。

**重新讨论**：当可使用厂商 ARM64 Vulkan 硬件、缺少所需 Dozen 能力或出现未解释的驱动/桥错误时。
Java/G3 保持独立，受影响的图形路径报告阻塞，不替换为其他后端。

## ADR-0005：硬件感知的 `--graphics=auto` 策略与 D3D11/DXGI 多层 GPU 合成架构

2026-11-18，GPU-P1..GPU-P6。状态：用户已批准。

**决定。**
1. **随包内置 ANGLE：** 每个 Windows 11 ARM64 构建、便携包与单用户 NSIS 安装包均内置私有原生
   ARM64 ANGLE（`bin\angle\libEGL.dll`、`libGLESv2.dll`，以及供运行时 HLSL 编译使用的
   `d3dcompiler_47.dll`），同时启用 Direct3D 11 与 Vulkan 双后端，并挂载原子落盘的
   `EGL_ANDROID_blob_cache`。
2. **后端选择（`--graphics`）：** 显式指定 `d3d11` 或 `vulkan` 时仅绑定对应硬件后端，失败绝不静默切换。
   `auto` 采用硬件感知择优：当存在厂商原生 ARM64 Vulkan ICD（`vulkanTranslated == false`）时，
   `auto` 优先尝试 `vulkan`，失败回退 `d3d11`；当系统 Vulkan 仅为转换层（`vulkanTranslated == true`，
   如基于 Direct3D 12 的 Mesa Dozen）时，`auto` 优先使用原生硬件 `d3d11`（直接跑在厂商驱动上的
   GLES 3.0，避免 GLES→Vulkan→D3D12 双重转换与 SM 6.2 计算管线限制）。同理，在转换层机器上，
   `auto` 模式下的客体 `libvulkan.so` 按上游 Manifest 强依赖策略暴露，使同时支持 Vulkan 与 GLES
   的双栈引擎（如 Flutter、`libmpv`）自动采用满血原生 D3D11 GLES 3.0，除非用户显式指定
   `--graphics=vulkan` 或应用声明必需 `android.hardware.vulkan.*`。
3. **多层呈现与 `--embed` 跨进程传输：** 触发 ADR-0002 的多呈现目标重审条件，每个 Activity 窗口在宿主
   Direct3D 11 / DXGI 设备上按 SurfaceFlinger Z 序合成多层画面（`[底层 SurfaceView] → [保留预乘
   Alpha 透明挖孔与 TextureView 纹理的主 UI 层] → [zOrderOnTop 顶层 SurfaceView]`）。独立窗口通过
   DXGI Flip 交换链硬件直出；`--embed` 窗格模式通过 3 槽异步 `D3D11_USAGE_STAGING` 环形缓冲回读合成帧并写入既有
   `Local\winapk-frame-<token>` 共享内存协议，彻底消除隐藏 `HWND` 上 `PrintWindow` 黑屏与同步
   `glReadPixels` 停顿，同时保留零重启 `Reattach`。

**原因。** 真实 Android GPU 应用普遍将 `SurfaceView` 与 `TextureView` 嵌在 `ViewGroup` 内部并与
Java UI 叠加，跨渲染/IO 线程创建共享 `EGLContext`，且在 `--embed` 窗格模式下运行时 `HWND` 从不显示。
此前单表面夹具虽可通过，但 UI 线程同步转发 GL、全不透明 `presentBitmap` 覆盖以及隐藏交换链上的
`PrintWindow` 导致真实应用无法出图。

**重新讨论：** 若未来 MyGo `ui` 原生开放跨进程 DXGI 共享 `HANDLE` 纹理合成接口，`--embed` 可在不改动
`WinApkCompositor` 之上任何 Java 或客体渲染层的前提下，进一步升级为零 CPU Staging 拷贝的共享纹理直传。
