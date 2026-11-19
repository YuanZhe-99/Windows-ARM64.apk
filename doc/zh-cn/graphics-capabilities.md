# 图形能力

G3 之后 P5/P6 进行中，尚无图形关卡通过。Java 位图路径保持独立；
启用客体原生不等于启用 GLES 或客体 Vulkan。

## 验证路径

| 路径 | 状态 | 必需证据 |
|---|---|---|
| 宿主 ANGLE/D3D11 | PASS | Adreno 8cx Gen 3 硬件，驱动 `30.0.4122.4000`，EGL 1.5 / GLES 3.0；绘制/回读、resize 及销毁 |
| 宿主 ANGLE/Vulkan 经 Dozen | PASS；用户批准的实验（ADR-0004） | 同一 Adreno GPU 经 D3D12，Mesa `26.3.0-devel (git-e3fc7749c6)`、Vulkan `1.2.359`、EGL 1.5 / GLES 2.0；`translated=yes software=no` |
| Java GLES 进入 ANGLE | PASS（`GLES10/11/20/30/31/32` + `EGL14/15/EGLExt`） | 真实 APK `GLSurfaceView` ES 2.0 与 ES 3.0（VAO/UBO/`glTexStorage2D`/FBO MRT/实例化/blit）、工作线程 `EGL14` `SurfaceTexture`（`MacApkGlWindow`）、resize、恢复、销毁及 Dozen 下 ES3 显式拒绝 |
| 受管理的 native window | 下述引用 profile PASS | 真实受限 APK、客户区尺寸、换挂载及引用拒绝；不绘制 |
| 客体 GLES/N2 | PASS（`libEGL.so` + `libGLESv1_CM/v2/v3.so`） | 普通 NDK APK（`DT_NEEDED` ES2 + ES3）；多线程 `pthread` `share_context`、`Pbuffer`、`KHR_surfaceless_context`、`EGLImageKHR`（`glEGLImageTargetTexture2DOES`）、D3D11 ES 3.0 VAO + Dozen ES 3.0 拒绝、resize、最小化/恢复、表面替换与显式 Vulkan 拒绝 |
| 客体 Vulkan 实例/WSI | T13.wsi 在 Dozen 上 PASS | 真实 APK、Android→Win32 surface、实际支持与 capabilities、resize、换挂载及拒绝；不绘制 |
| 客体 Vulkan 交换链清屏 | Dozen 上受限 T13 profile PASS | 真实客体设备/队列、命令缓冲区及交换链，resize 前后共三次呈现 |
| 客体 Vulkan 传输缓冲区 | Dozen 上受限 T13 profile PASS | 一致内存分配及 GPU 填充，16 个字精确回读及生命周期拒绝 |
| 客体 Vulkan 图形管线、图像与回调 | Dozen 上 PASS（`libvulkan.so` 1.0–1.3，290+ 命令） | SPIR-V `VkGraphicsPipelineCreateInfo`、`VkPipelineCache`、描述符/采样器/Push Constants、`4x4` `VkImage` 渲染通道绘制与回读（`0xFFCC8822`）、`--embed` WSI 回读，以及豁免 CFG 的 `VkAllocationCallbacks` + `VK_EXT_debug_report`/`debug_utils` 回调桥 |
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

启用 `--native=restricted` 及 `--graphics` 后端后，逻辑系统库 `libEGL.so`、
`libGLESv1_CM.so`、`libGLESv2.so` 与 `libGLESv3.so`（`guest_graphics.cpp`、
`guest_gles_autogen.inc`）暴露 EGL 1.5（`EGL_KHR_create_context`、
`EGL_KHR_surfaceless_context`、`EGL_KHR_image_base`、`EGL_KHR_gl_texture_2D_image`、
`EGL_KHR_gl_renderbuffer_image`、`EGL_KHR_fence_sync`、`EGL_KHR_wait_sync`）及 505 个
GLES 1.0/1.1/2.0/3.0/3.1/3.2 与 OES/EXT/KHR 入口。客体线程直接在当前调用线程上驱动 ANGLE，
不再串行转发至 Win32 消息线程，支持多 `EGLContext`、跨渲染线程与 IO 工作 `pthread` 的
`share_context`、`eglCreatePbufferSurface`、无表面 `eglMakeCurrent` 以及 `EGLImageKHR` 跨线程纹理传递。

客体窗口表面（`ANativeWindow*`）底层由动态缩放的 ANGLE Pbuffer 承载，并交由 `WinApkCompositor`
按 Z 序合成（D3D11 下采用 `EGL_D3D_TEXTURE_2D_SHARE_HANDLE_ANGLE` 零拷贝共享纹理，Vulkan 下采用
FBO-0 RGBA8 传输），在独立窗口与 `--embed` 窗格中表现完全一致。宿主 WSI 扩展
（`eglCreatePlatformWindowSurface`）与未知扩展仍返回空指针。最小化（`0x0`）或已失效表面调用
`eglSwapBuffers` 时返回 `EGL_BAD_SURFACE`。

`test.ps1 -Suite GraphicsGuest` 以一个 API-30 NDK APK（`DT_NEEDED` 含 `libandroid.so`、
`libEGL.so`、`libGLESv2.so`、`libGLESv3.so`）分别测试显式硬件 D3D11、显式 Vulkan/Dozen，以及屏蔽子进程
Vulkan ICD 的拒绝用例。验证后台 `pthread_create` IO 线程经 `share_context` + Pbuffer + 无表面绑定 +
`EGLImageKHR` / `glEGLImageTargetTexture2DOES` 上传纹理并由主渲染线程采样回读、D3D11 下的 GLES 3.0
（`#version 300 es` / VAO）与 Dozen ES 2.0 后端下对 ES 3.0 的诚实拒绝（`EGL_BAD_MATCH`）、混合三角形回读、
最小化/恢复、窗口缩放（`360x720` → `424x752`）、表面替换及显式不可用拒绝（`EGL_NOT_INITIALIZED`）。

## 客体 Vulkan 1.0–1.3 与 WSI（`guest_vulkan.cpp`）

`guest_vulkan.cpp` 与基于锁定版 `Vulkan-Headers` 1.4.363 生成的 `guest_vulkan_autogen.inc` 经
System32 `vulkan-1.dll` 暴露 290+ 个 Vulkan 1.0–1.3 核心、晋升及 KHR/EXT 实例、物理设备、逻辑设备、
队列与命令缓冲入口，支持多 `VkInstance`、多 `VkDevice` 与多 `VkQueue`。依据 ADR-0005，客体 Vulkan 在
`--graphics=vulkan`、`--graphics=auto` 且存在原生 GPU 厂商 Vulkan ICD（`HasNativeVendorVulkan() == true`），
或 `--graphics=auto` 且 APK 清单要求 `android.hardware.vulkan.*` 时启用。

宿主专属 Win32 表面/互操作扩展（`VK_KHR_win32_surface`、`VK_KHR_external_*_win32`、
`VK_KHR_win32_keyed_mutex`）、Android `AHardwareBuffer` 扩展（`VK_ANDROID_*`）与 POSIX `fd` 外部内存/同步
扩展（`VK_KHR_external_*_fd`）在扩展枚举、`ppEnabledExtensionNames` 与 `pNext` 中均被过滤并以
`VK_ERROR_EXTENSION_NOT_PRESENT` 拒绝。标准 Vulkan 1.1–1.3 `pNext` 链（`VkPhysicalDeviceProperties2`、
`VkPhysicalDeviceDriverProperties`、`VkPhysicalDeviceFeatures2`、动态渲染、synchronization2、时间线信号量、
描述符更新模板）直接透传。

### 交换链 WSI 与 `--embed` 合成

`vkCreateAndroidSurfaceKHR` 将受管理的 `ANativeWindow*` 令牌翻译为 `VkSurfaceKHR`，并将非 CPU 物理设备排在
软件适配器之前。在 `--embed` 窗格模式下，交换链呈现接入 `WinApkCompositor`——在原生厂商 ICD 支持时优先走
`VK_KHR_external_memory_win32` DXGI 零拷贝共享纹理，在转换层 ICD（Mesa Dozen）上自动回退为宿主一致
`VkBuffer` `vkCmdCopyImageToBuffer` 传输回读。最小化（`0x0`）与窗口缩放会锁存 `VK_ERROR_OUT_OF_DATE_KHR`
直至重建交换链；已失效表面返回 `VK_ERROR_SURFACE_LOST_KHR`。本机 Dozen 验证不替代原生厂商 Vulkan ICD 验收。

### 传输缓冲区回读与计算说明

`hello-vulkan-wsi.apk` 验证一致宿主可见缓冲区映射、GPU `vkCmdFillBuffer` + transfer→host 屏障回读，以及对
`VK_ANDROID_*` 和 `VK_KHR_external_*_fd` `pNext` 链的显式拒绝。在 Adreno 8cx Gen 3（`HighestShaderModel=0x62`、
`WaveLaneMin=64..128`）上，Mesa Dozen 编译计算着色器会生成针对 Shader Model 6.6+ 的 subgroup 指令，计算管线受宿主驱动着色器模型限制。

### 图形管线、描述符、管线缓存与客体回调

在 `hello-vulkan-wsi.apk`（`test.ps1 -Suite VulkanWsi` 与 `TestRealRuntimeEmbed`）中，`libvulkan.so` 验证了：

- **离屏 `VkImage`、描述符、采样器、Push Constants 与 `VkPipelineCache`：** `4x4`
  `VK_FORMAT_R8G8B8A8_UNORM` 最优平铺图像、`VkSampler`、`VkDescriptorSetLayout` + `VkDescriptorPool` +
  `VkDescriptorSet`（`vkUpdateDescriptorSets` UBO）、`VkPushConstantRange`（`vkCmdPushConstants`）、
  `VkPipelineCache`（`vkGetPipelineCacheData`）、SPIR-V 图形管线绘制与图像到缓冲区回读（逐字断言全部 16 个字等于 `0xFFCC8822`），以及 `--embed` 窗格 WSI 像素回读。
- **豁免 CFG 的客体回调（`__declspec(guard(nocf))`）：** 堆驻留 `VkAllocationCallbacks` 跳板（验证返回 `NULL` 时转为 `VK_ERROR_OUT_OF_HOST_MEMORY` 且 `allocs == frees`）、宿主/桥双通路 `VK_EXT_debug_report` 与 `VK_EXT_debug_utils` 消息回调（含非可执行回调地址拒绝），以及 `VK_EXT_device_memory_report` `pNext` 回调筛查。

## P8 基于 Direct3D 12 的 Skia / AGSL / RenderEffect（`T15`）

`NativeSkiaD3D12`（`macapk/runtime/platform/windows/src/bridge/skia_d3d12.cpp`）创建独立的硬件
`IDXGIAdapter1`、`ID3D12Device` 与直接 `ID3D12CommandQueue`（默认拒绝软件 WARP 适配器），并通过
`DirectContextKt._nMakeDirect3D` 接入 Skiko `0.150.2`（`skiko-windows-arm64.dll`，`ARM64` PE）。
`MacApkSkia` 保持 Java2D Canvas + GPU 着色器/滤镜 + 显式直通 alpha 回读的分工（`PLAN.md` §15），在本机报告
`Direct3D12: Qualcomm(R) Adreno(TM) 8cx Gen 3`，并通过 `AgslSmokeWindows`（`T15`，`test.ps1 -Suite Agsl`）
的全部 11 项像素契约。当 `skiko-windows-arm64.dll` 或 Direct3D 12 不可用（`WINAPK_DISABLE_SKIA=1`）时，
`MacApkSkia.ensureLoaded()` 显式抛出 `GRAPHICS_CAPABILITY_MISSING`，绝不静默回退为无效果的 CPU 渲染。
