# 图形能力

G3 之后 P5/P6 进行中，尚无图形关卡通过。Java 位图路径保持独立；
启用客体原生不等于启用 GLES 或客体 Vulkan。

## 验证路径

| 路径 | 状态 | 必需证据 |
|---|---|---|
| 宿主 ANGLE/D3D11 | PASS | Adreno 8cx Gen 3 硬件，驱动 `30.0.4122.4000`，EGL 1.5 / GLES 3.0；绘制/回读、resize 及销毁 |
| 宿主 ANGLE/Vulkan 经 Dozen | PASS；用户批准的实验（ADR-0004） | 同一 Adreno GPU 经 D3D12，Mesa `26.3.0-devel (git-e3fc7749c6)`、Vulkan `1.2.359`、EGL 1.5 / GLES 2.0；`translated=yes software=no` |
| Java GLES 进入 ANGLE | NOT_RUN | 真实 APK GLSurfaceView 的值/生命周期，不是宿主探测 |
| 客体 GLES/N2 | NOT_RUN | 受管理 Surface/ANativeWindow、审计后的 EGL/GLES 调用与函数指针、正负 APK 契约 |
| 客体 Vulkan/N2 | NOT_RUN | 独立 Android/Win32 WSI 及 dispatch/handle/callback 契约，不是 ANGLE 私有 device |
| 厂商 ARM64 Vulkan ICD | BLOCKED | 具有该驱动的机器；Dozen 结果不能填入此行 |

## ANGLE 源码构建

`build-lock.json` 选定 ANGLE `801a1a5f6e00ed0028937067b7663fd7053bff31`。
对其固定 Chromium build 依赖应用一个经审计的补丁：把自定义工具链 runtime 目录的错误列表类型，
改为普通 Python 路径同样输出的 `"None"` 字符串。同一补丁在两个 Python 工具链脚本中选择锁定的
SDK 10.0.26100.0；否则它们会忽略 GN 覆盖而强制使用 SDK 28000。不静默安装 SDK。
还省略这些 EGL/GLES DLL 目标不用的 Chromium 调试器/符号化 DLL 复制；CRT 复制不变。
不因缺少可选 x64 SDK 调试器而安装组件。
NTDDI 构建目标设为 SDK 26100 定义的 `NTDDI_WIN11_GE`，不使用此 SDK 未定义的较新
`NTDDI_WIN11_BR`，否则必需的头文件声明会被隐藏。
ANGLE renderer 代码不变，不复制 Mac.apk/Metal 补丁。
补丁在固定 Git 原始文件上的正向应用及反向检查均通过；
构建也接受 CRLF checkout 上的同一补丁，不修改全局 Git 设置。
源码构建和 ARM64 PE 暂存通过。`scripts/windows/angle-args.gn` 请求 ARM64 EGL/GLES DLL，
仅包含 Vulkan 与 D3D11，不捆绑 Vulkan loader、SwiftShader、OpenCL 或 WebGPU 后端。
使用系统已有 Vulkan loader/ICD，不作修改，不复制或安装驱动。
源码和工具位于忽略的 `build/windows-arm64/deps`，不在 Mac.apk 子树中。
记录 depot_tools 与 ANGLE DEPS；构建工具可以仿真执行，发货的 PE 产物不能仿真。
排除上游受限/私有 trace 资产及未用到的 OpenCL/Dawn 依赖。
这些 C++ DLL 目标禁用 Rust 和 Siso。固定资源编译器对象通过公开 HTTPS 获取，
校验上游哈希和记录的 SHA-256；绕开 gsutil 的 MAX_PATH 解压失败，不修改 Windows 设置。

上游 `doc/DevSetup.md` 描述独立 GN/Ninja 构建及后端选择。
其 `gni/angle.gni` 仍有 Windows ARM64 缺少 Vulkan 的注释，但真实默认条件允许 Windows Vulkan 后端。
因此必须验证真实构建及 renderer，不能仅凭注释或构建推定支持。

## 宿主探测契约

P5a 从受控私有 DLL 目录加载 libEGL/libGLESv2，显式选择 Vulkan 或 D3D11，
创建 Win32 EGL 窗口 surface/context 并验证确定性 shader 像素。
报告必须包含请求/实际后端、GL/EGL 版本、renderer、扩展、device/驱动标识及转换层/软件状态。
请求后端缺失就是失败，绝不静默切换。Vulkan 与 D3D11 分别运行短进程。

最小负例使显式 Vulkan 路径不可用，不能借 D3D11 成功。宿主渲染不证明 Java GLES、
客体 GLES、客体 Vulkan 或 G4。resize/context/surface 生命周期覆盖随真实调用方的实现增长，
不建立空的推测性接口。

可选 `build.ps1 -Component AngleProbe` 和 `test.ps1 -Suite GraphicsHost` 实现该宿主增量：
两个正例进程和一个 Vulkan 驱动不可用的负例进程。负例仅覆盖子进程 loader 环境，不改变系统 ICD。
测试将子进程 PATH 缩减到 Windows 目录，检查 DLL 哈希/机器类型，并在真实窗口 resize 前后各断言
两个像素；必须正常销毁 EGL 对象。
`bin/angle.json` 记录 ANGLE 配方/补丁及 DLL 哈希；`bin/angle-probe.json` 记录宿主夹具的源码哈希、
编译器/配置、可执行文件哈希及期望输出。

`GraphicsHost`：Windows ARM64 上 **6 项 PASS，2.38 秒**（`P5-GraphicsHost.log`）。
两个正例都在客户区从 `304x201` 变到 `400x265` 前后各绘制并回读两点，RGBA 均精确为
`64,128,191,255`，销毁后正常退出。负例在显式 Vulkan 不可用时退出 2，未改用 D3D11。
不覆盖产品 `auto` 选择及打包。
