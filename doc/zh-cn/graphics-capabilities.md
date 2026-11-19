# 图形能力

G3 之后 P5/P6 进行中，尚无图形关卡通过。Java 位图路径保持独立；
启用客体原生不等于启用 GLES 或客体 Vulkan。

## 验证路径

| 路径 | 状态 | 必需证据 |
|---|---|---|
| 宿主 ANGLE/D3D11 | PASS | Adreno 8cx Gen 3 硬件，驱动 `30.0.4122.4000`，EGL 1.5 / GLES 3.0；绘制/回读、resize 及销毁 |
| 宿主 ANGLE/Vulkan 经 Dozen | PASS；用户批准的实验（ADR-0004） | 同一 Adreno GPU 经 D3D12，Mesa `26.3.0-devel (git-e3fc7749c6)`、Vulkan `1.2.359`、EGL 1.5 / GLES 2.0；`translated=yes software=no` |
| Java GLES 进入 ANGLE | 下述受限 GLES20 profile PASS | 真实 APK `GLSurfaceView` 颜色、resize、恢复、销毁及显式拒绝 |
| 受管理的 native window | 下述引用 profile PASS | 真实受限 APK、客户区尺寸、换挂载及引用拒绝；不绘制 |
| 客体 GLES/N2 | 受限 T11 profile PASS | 普通 NDK APK；纹理/FBO、混合 shader、resize、surface 替换与显式 Vulkan 拒绝 |
| 客体 Vulkan 实例/WSI | T13.wsi 在 Dozen 上 PASS | 真实 APK、Android→Win32 surface、实际支持与 capabilities、resize、换挂载及拒绝；不绘制 |
| 客体 Vulkan 交换链清屏 | Dozen 上受限 T13 profile PASS | 真实客体设备/队列、命令缓冲区及交换链，resize 前后共三次呈现 |
| 客体 Vulkan 传输缓冲区 | Dozen 上受限 T13 profile PASS | 一致内存分配及 GPU 填充，16 个字精确回读及生命周期拒绝 |
| 客体 Vulkan 图形管线、图像与回调 | Dozen 上受限 T13 profile PASS | SPIR-V 顶点/片元 `VkGraphicsPipelineCreateInfo`、`4x4` `VkImage` 渲染通道绘制与图像到缓冲区回读（`0xFFCC8822`），以及客体 `VkAllocationCallbacks` 与 `VK_EXT_debug_report` 回调桥 |
| 客体 Vulkan 计算实验 | 本机 Dozen 上 FAIL，未纳入交付 | Adreno SM 6.2（`WaveLaneMin=64, Max=128`）上 `vkCreateComputePipelines` 返回 `VK_ERROR_OUT_OF_HOST_MEMORY`，而 WARP SM 6.6+ 成功 |
| 客体 Vulkan 验证层与 G4 | NOT_RUN / BLOCKED | 未安装 `VK_LAYER_KHRONOS_validation`（`NOT_RUN`）；缺少厂商原生 ICD（`BLOCKED`） |
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
宿主探测不覆盖 `auto` 选择及打包。

## Java GLES 增量

Windows Java GLES 路径使用私有 ARM64 ANGLE（`bin\angle\libEGL.dll`、`libGLESv2.dll`、
`d3dcompiler_47.dll`），挂载原子落盘的 `EGL_ANDROID_blob_cache`（`angle_runtime.cpp`）并接入
Direct3D 11 多层硬件合成器（`compositor.cpp`，ADR-0005）。CLI 默认 `--graphics=off`；`vulkan` 与
`d3d11` 显式请求对应后端且不回退；`auto` 在存在原生 GPU 厂商 Vulkan ICD 时优先 `vulkan`，在宿主
Vulkan 为转换层（如 Mesa Dozen）时优先原生硬件 `d3d11`（GLES 3.0）。

先构建 `Angle` 再构建 `Host` 或 `All` 以暂存 ARM64 DLL（`stage-app-resources.ps1` 与
`package.ps1` 自动将其打入发布包）。`winapk doctor` 通过无窗口 `1x1` Pbuffer 实测探测 `bin\angle`，
在 `angleBackends` 中报告各后端及实际最高 GLES 版本（Dozen 机器上 `d3d11` 为 ES 3.0，`vulkan` 为
ES 2.0）。`graphics.cpp` 将上游全部 `GLES10`、`GLES11`、`GLES20`、`GLUtils`、`GLES30`、`GLES31`、
`GLES31Ext`、`GLES32`、`EGL14`、`EGL15` 与 `EGLExt` JNI 方法动态绑定到私有 ARM64 ANGLE，支持
ES 1.1/2.0/3.x `GLSurfaceView` 以及独立线程的 `EGL14` / `MacApkEGL10` / `SurfaceTexture`
（`MacApkGlWindow`）纹理生产者。`test.ps1 -Suite GraphicsJava` 运行 `hello-gles.apk` 与
`hello-gles3.apk` 共 8 项检查：显式 Vulkan 经 Dozen、转换层机器上 `auto` 优选硬件 D3D11、隐藏 Vulkan
时 `auto` 回退 D3D11、显式 Vulkan 不可用拒绝、D3D11 上的 GLES 3.0 `#version 300 es`
VAO/UBO/`glTexStorage2D`/FBO-blit + 工作线程 `EGL14` `SurfaceTexture` 呈现，以及 Dozen ES 2.0
Vulkan 后端对 ES 3.0 请求的显式拒绝。

## 受管理的 Android native window 与多层硬件合成

`surface.cpp`（`libandroid.so`）对根 `SurfaceView` 与嵌在 `ViewGroup` 内的子 `SurfaceView` 提供
`ANativeWindow_fromSurface`、`acquire`、`release`、`getWidth`、`getHeight`、`getFormat`、
`setBuffersGeometry`、`lock` 与 `unlockAndPost`。获取 `SurfaceView` 原生窗口时回调
`Surface.macapkNoteNativeWindowConsumer` 使 `SurfaceView.draw` 在 `ViewCanvasDriver` 中透明挖孔，
由 `compositor.cpp` 在硬件 D3D11 上按 `[底层 SurfaceView] → [保留直通 Alpha 挖孔的主 UI 层] →
[zOrderOnTop 顶层 SurfaceView 与弹窗覆盖层]` Z 序合成，并通过 3 槽异步 `D3D11_USAGE_STAGING`
环形缓冲为 `--embed` 窗格输出帧。

`test.ps1 -Suite Surface`：Windows ARM64 上 **4 项 PASS，3.48 秒**。普通 NDK API-30
夹具声明 `DT_NEEDED libandroid.so`，同一引用追踪 `360x720` 到 `424x752`；
在一个启用 CheckJNI 的 APK 进程中验证无关/空对象、Java release、旧挂载失效、新身份及过期/未知令牌。

## 客体 EGL/GLES profile

启用 `--native=restricted` 及 `--graphics` 后端后，普通 `SurfaceView` 可承载一组客体 EGL
display/config/context/window surface。客体看到的是桥接令牌；驱动句柄留在 Win32 消息线程。
客体线程持有逻辑绑定，命令同步在消息线程执行，不持有注册表锁。resize 更新宿主 drawable
而不更换客体 context；失效挂载不能交换或再次绑定。客体 EGL 与 Java EGL 不能同时占用
ANGLE；客体 Vulkan 使用独立 dispatch 与所有权。

逻辑 `libEGL.so` 与 `libGLESv2.so` 只暴露固定的 ES2 和窗口 EGL 命令。`eglGetProcAddress`
从同一张表返回桥入口或空值。不公布扩展；不支持 ES3 context、pbuffer / 无 surface context、
多个 display、自定义 native display 或共享 context。报告的 1.4/ES2 字符串标识 profile，
不是一致性声明。调用要求窗口消息循环已运行，display 归属不能在客体线程间转移。
受管理的 surface 最小化且尺寸为零时，桥拒绝 swap 并返回 `EGL_BAD_SURFACE`。
恢复后，夹具用原客体 context、surface 和 program 绘制并回读，再测试 resize 和替换。
这不等于真实 context loss 或长期稳定性验证。
T11 仍只启动原有三个进程。首次扩展测试观察到一次 Vulkan 在 resize/替换后的 swap 失败；
复测通过。驱动 swap 失败现会记录 EGL 错误码，不再只留下夹具步骤编号。

`test.ps1 -Suite GraphicsGuest` 以一个 API-30 APK 分别测试显式硬件 D3D11 和 Vulkan/Dozen，
并在仅隐藏子进程 Vulkan ICD 时额外启动一次。检查 RGBA8 纹理上传经 FBO 精确回读、
混合三角形回读（容差 1–2 字节）、resize 和 surface 替换保留同一个 context、不公布扩展、
proc 地址身份，以及无效 native window、ES3、过期/当前 surface 和后端不可用错误。
显式 Vulkan 失败时应用收到 `EGL_NOT_INITIALIZED`，可以正常关闭。

## 客体 Vulkan 实例与 WSI 增量

选择 `--native=restricted --graphics=vulkan` 时，受限 `libvulkan.so` 仅使用 System32
Vulkan loader。桥筛选实例扩展，把 Android surface 创建翻译为 Win32，私有保存实例、
物理设备和 surface 句柄，并查询实际呈现支持和 capabilities。消息线程负责 surface
创建、查询及销毁；ANGLE 不参与。

仅通过 WSI 测试不等于呈现一帧，也不等于通过 G4。

`test.ps1 -Suite VulkanWsi` 启动同一个普通 NDK API-30 APK 两次：一次系统 ICD 可用，
一次仅对子进程屏蔽 ICD。验证扩展和 proc 作用域，拒绝 AHardwareBuffer、分配回调、
非空 `pNext` 及过期句柄。窗口从 `360x720` 改到 `424x752`；替换视图后旧 surface
返回 `VK_ERROR_SURFACE_LOST_KHR`。不安装或复制驱动。

### 交换链清屏 profile

一个逻辑设备及呈现队列、一个 FIFO 交换链和一个主命令缓冲区构成独立受限 profile。
客体对已获取图像的令牌记录布局转换与清屏，向所属队列提交并呈现。驱动设备、队列、
交换链和图像句柄保持私有。本 profile 不实现 shader pipeline、通用图像、
自定义分配器或调试回调。

`VulkanWsi` 套件在同一个正例进程中验证三次清屏呈现：启动时、从最小化恢复后及 resize 后。
最小化至零尺寸时 acquire 返回 `VK_ERROR_OUT_OF_DATE_KHR`，恢复到原尺寸后旧交换链仍过期。
客体在恢复及 resize 后分别销毁并重建。测试断言真实 present 返回值，不进行交换链像素回读；
不代表厂商 Vulkan 或 G4 验收。

### 传输缓冲区回读

同一 APK 进程还验证一个受限传输目标缓冲区和一份宿主可见且一致的设备内存分配。
桥翻译两个令牌，记录真实填充与 transfer→host barrier，仅在队列完成后允许映射。
APK 逐字核对已知值。这是传输测试，不是 shader 执行、通用资源支持或交换链像素回读。

计算实验未通过 pipeline 创建阶段：本机 Adreno 8cx Gen 3 的 D3D12 驱动仅报告
`HighestShaderModel=0x62` 且 `WaveLaneMin=64, WaveLaneMax=128`，而 Mesa Dozen 编译计算着色器时生成针对
Shader Model 6.6+ 的 subgroup 要求。已验证的桥不导出计算命令。

### 图形管线、离屏图像与客体回调

在同一单次启动的 `hello-vulkan-wsi.apk` 夹具（Adreno 8cx Gen 3 + Dozen）中，受限 `libvulkan.so`
提供方还验证了：

- **离屏 `VkImage` 与渲染通道：** 一个 `4x4` `VK_FORMAT_R8G8B8A8_UNORM` 最优平铺图像
  （`COLOR_ATTACHMENT_BIT | TRANSFER_SRC_BIT`）、独立 `VkDeviceMemory` 绑定、`VkImageView`、
  单子通道 `VkRenderPass`（`CLEAR` 到 `STORE`，`UNDEFINED` 到 `TRANSFER_SRC_OPTIMAL`）及 `VkFramebuffer`。
- **SPIR-V 图形管线：** 由 NDK `glslc` 编译的顶点与片元 `VkShaderModule`、空 `VkPipelineLayout`、
  `vkCreateGraphicsPipelines`、`vkCmdBeginRenderPass`、`vkCmdBindPipeline`、`vkCmdDraw`、
  `vkCmdEndRenderPass` 及 `vkCmdCopyImageToBuffer` 拷入一致回读缓冲区，逐字断言全部 16 个像素等于片元着色器输出 `0xFFCC8822`。
- **客体回调：** 着色器模块创建/销毁上的筛查后 `VkAllocationCallbacks`（在调用方客体线程执行并与宿主 ICD 堆隔离，验证返回 `NULL` 时转为 `VK_ERROR_OUT_OF_HOST_MEMORY` 且分配与释放次数平衡），以及 `VK_EXT_debug_report`（`vkCreateDebugReportCallbackEXT`、非可执行回调地址拒绝、`vkDebugReportMessageEXT` 与 `vkDestroyDebugReportCallbackEXT`）。

## P8 基于 Direct3D 12 的 Skia / AGSL / RenderEffect（`T15`）

`NativeSkiaD3D12`（`macapk/runtime/platform/windows/src/bridge/skia_d3d12.cpp`）创建独立的硬件
`IDXGIAdapter1`、`ID3D12Device` 与直接 `ID3D12CommandQueue`（默认拒绝软件 WARP 适配器），并通过
`DirectContextKt._nMakeDirect3D` 接入 Skiko `0.150.2`（`skiko-windows-arm64.dll`，`ARM64` PE）。
`MacApkSkia` 保持 Java2D Canvas + GPU 着色器/滤镜 + 显式直通 alpha 回读的分工（`PLAN.md` §15），在本机报告
`Direct3D12: Qualcomm(R) Adreno(TM) 8cx Gen 3`，并通过 `AgslSmokeWindows`（`T15`，`test.ps1 -Suite Agsl`）
的全部 11 项像素契约。当 `skiko-windows-arm64.dll` 或 Direct3D 12 不可用（`WINAPK_DISABLE_SKIA=1`）时，
`MacApkSkia.ensureLoaded()` 显式抛出 `GRAPHICS_CAPABILITY_MISSING`，绝不静默回退为无效果的 CPU 渲染。
