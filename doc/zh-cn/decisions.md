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
