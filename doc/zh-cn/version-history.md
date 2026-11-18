# 版本历史

## 0.1.0（进行中）

- 2026-11-18：按 `design/` 全部 17 张设计稿完成 MyGo 前端（中英双语、亮暗外观、Mica 材质）并将 `cmd/windows-arm64-apk` 接入真实 `winapk.Runtime`（`--embed` 共享内存 RGBA8 帧与重叠双工管道 IPC、零重启 `Reattach`、静态 ARSC/ELF `inspect` 与 `--profile=N0|N1|N2` 准入、经校验的私有 ARM64 OpenJDK 21 安装器与存储管理）；查明 Adreno 8cx Gen 3 SM 6.2 在 Dozen 下的计算管线限制，并在 Dozen 上验证客体 Vulkan SPIR-V 图形管线、离屏 `VkImage` 回读（`0xFFCC8822`）、`VkAllocationCallbacks` 与 `VK_EXT_debug_report` 回调。Go 1.27.4、MyGo v0.10.0 和已合并的 Mac.apk `3f35611` 经检查未变；版本 0.1.0 仍未发布。
- 2026-11-17：为已开放的 Vulkan 桥表增加编译期签名检查，保留已验证的传输 profile。Dozen 上计算实验和原生对照均在 pipeline 创建阶段失败，不交付计算入口。Go 1.27.4、MyGo v0.10.0 和已合并的 Mac.apk `3f35611` 经检查未变。
- 2026-11-17：在现有 T13 启动中验证 Vulkan GPU 传输填充和一致内存精确回读，以及归属和同步拒绝。Go 1.27.4、MyGo v0.10.0 和已合并的 Mac.apk `3f35611` 经检查未变。
- 2026-11-17：在 Dozen 上验证 T13 零尺寸交换链失效及恢复后重建呈现，未增加夹具启动次数。Go 1.27.4、MyGo v0.10.0 和已合并的 Mac.apk `3f35611` 经检查未变。
- 2026-11-17：将 DEX 转换纳入随父进程退出的 Windows Job Object，并验证取消时回收正在运行的子进程。Go 1.27.4、MyGo v0.10.0 和已合并的 Mac.apk `3f35611` 经检查未变。
- 2026-11-17：在 D3D11 与 Vulkan/Dozen 上验证 T11 客体 GLES 最小化与恢复，未增加 APK 启动次数。Go 1.27.4、MyGo v0.10.0 和已合并的 Mac.apk `3f35611` 经检查未变。
- 2026-11-17：生成并验证本地 ARM64 Release CLI ZIP，使用外部 Java/VC++ 依赖，另附源码快照。未发布，不等于 G5 通过。Go 1.27.4、MyGo v0.10.0 和已合并的 Mac.apk `3f35611` 经检查未变。
- 2026-11-17：以真实 T13 APK 验证受限客体 Vulkan 清屏：经 Dozen 完成三次 FIFO 呈现，包括 resize 后重建交换链。shader/缓冲区、回调和 G4/G5 仍未验收。Go 1.27.4、MyGo v0.10.0 和已合并的 Mac.apk `3f35611` 经检查未变。
- 2026-11-17：新增独立客体 Vulkan 实例/WSI profile（T13.wsi），在 Dozen 上验证窗口 resize/替换及仅对子进程屏蔽 ICD。客体 Vulkan 绘制和 G4/G5 仍为 NOT_RUN。Go 1.27.4、MyGo v0.10.0 和已合并的 Mac.apk `3f35611` 经检查未变。
- 2026-11-17：接入受限客体 EGL/GLES（T11）。真实 NDK API-30 APK 验证 ANGLE 的显式 D3D11 与 Vulkan/Dozen，包括纹理/FBO 和混合 shader 回读、重建 surface、显式 Vulkan 不可用时不回退。客体 Vulkan、G4/G5 仍为 NOT_RUN。Go 1.27.4、MyGo v0.10.0 和已合并的 Mac.apk `3f35611` 经检查未变。
- 2026-11-17：在真实 APK 中验证受限 Android native window 引用 profile；客体 EGL/Vulkan 渲染仍为 NOT_RUN。Go 1.27.4、MyGo v0.10.0 和已合并的 Mac.apk `3f35611` 已检查、未变。
- 2026-11-17：通过 ARM64 ANGLE 验证真实 `GLSurfaceView` APK 的受限 Java GLES20，包括 resize、恢复及 Vulkan/Dozen 与 D3D11 选择。客体 GLES/Vulkan 与 G4/G5 仍为 NOT_RUN；未修改前端源码。Go 1.27.4、MyGo v0.10.0 及已合并的 Mac.apk `3f35611` 已检查。
- 2026-11-17：跟随 MyGo 至 v0.10.0（仅依赖更新，迁移预检无需改源码）。Go 1.27.4 与已合并的 Mac.apk `3f35611` 经检查未变。Go vet、常规及关闭 CGO 的测试、MyGo vet 与 i18n 检查通过。
- 2026-11-17：构建固定的 Windows ARM64 ANGLE，验证宿主 D3D11 及用户批准的 Vulkan/Dozen（ADR-0004）。GraphicsHost 6 项通过，包括显式 Vulkan 不可用时不回退。厂商 Vulkan BLOCKED，G4/G5 NOT_RUN。准备阶段基线通过 Host 24、Java 43、Native 58、Bionic 104、Jni 24、ApkJni 8；APK 就绪改用真实首帧，Go 通配符排除生成的 build 下载目录。该基线使用 Go 1.27.4、MyGo v0.9.2 和已合并 Mac.apk `3f35611`；0.1.0 仍未发布。
- 2026-11-17：以真实 hello-jni Activity 矩阵完成所声明 C/Bionic/JNI profile 的 G3：双向类型数值/异常、依赖 hook、无效版本缓存、worker/嵌套回调及窗口驱动的不支持 JNI 终止。默认及真实启用 CFG 的完整 ARM64 回归通过（Host 23、Java 43、Native 58、Bionic 104、Jni 24、ApkJni 8）；全部 64 个 PE JNI 入口具有对齐 GFID。默认 bin 已 clean 重建并复验 CFG=0。Go 1.27.4、MyGo v0.9.2 及已合并 Mac.apk `3f35611` 已检查、未变；G4/G5 尚未验证，版本 0.1.0 未发布。
- 2026-11-17：将受限 APK 库提取、仅客体 DEX 重定向及应用类导出绑定接入 Windows 宿主桥。API-30 hello-jni Activity 通过六项精简检查（4.77 秒）；完整 ARM64 requirements/build All、Host 23、Java 43、Native 58、Bionic 104、Jni 24 通过。API-24 紧凑 RX 元数据仍被拒绝；完整 APK/G3 矩阵未完成。Go 1.27.4、MyGo v0.9.2 与已合并 Mac.apk `3f35611` 已检查、未变；版本 0.1.0 未发布。
- 2026-11-17：新增显式根库专属/仅一次 JNI_OnLoad、失败缓存及失败 hook native 入口守卫；short/long/Unicode/描述符导出查询、注册优先及歧义转义拒绝在既有 T09 语义组通过。requirements/build All、Host 23、Java 43、Native 58、Bionic 104、Jni 24 通过；最终定向 Jni 运行 1.73 秒。Go 1.27.4、MyGo v0.9.2 与 Mac.apk `3f35611` 已检查、未变；APK 集成及 G3 仍未完成，0.1.0 未发布。
- 仓库脚手架：工作规范（`AGENTS.md`）、计划（`PLAN.md`）、镜像文档、文档镜像检查和 Go 模块。
- Go 1.27.3、MyGo v0.7.4、Mac.apk `main` 位于 `367fd1a`（2026-11-03 检查），原样引入为 `macapk/` subtree。
- 2026-11-04：Mac.apk `main` 合并到 `8b45e48`（上游 v1.0.2377 同步）；Go 1.27.3 和 MyGo v0.7.4 已检查，未变。
- 2026-11-05：G2 之后停止开发；0.1.0 从未发布或打标签（[README.md](README.md#项目状态)）。
- 2026-11-16：恢复开发；升到 Go 1.27.4、MyGo v0.9.2 和 Mac.apk `main` 的 `3f35611`（上游 v1.0.2379 同步），重新验证 G0–G2（[platform-notes.md](platform-notes.md)）。
- 2026-11-16：开始后端 P3 线程基础（T08）；Go 1.27.4、MyGo v0.9.2 与 Mac.apk `3f35611` 已检查、未变；版本仍为未发布的 0.1.0。
- 2026-11-16：新增 P3 进程私有 mutex/condition 同步（T08 `sync`）；Go 1.27.4、MyGo v0.9.2 与 Mac.apk `3f35611` 已检查、未变；未完成新关卡或发布。
- 2026-11-16：新增 P3 事务式模块依赖图与客体 dl API（T07）；完整 ARM64 回归通过（Host 22、Java 43、Native 58、Bionic 56）。Go 1.27.4、MyGo v0.9.2 与 Mac.apk `3f35611` 已检查、未变；G3 仍为 NOT_RUN，不发布。
- 2026-11-16：新增 P3 私有堆分配与字节/字符串包装（T08 内存）；完整 ARM64 回归通过（Host 22、Java 43、Native 58、Bionic 67）。Go 1.27.4、MyGo v0.9.2 与 Mac.apk `3f35611` 已检查、未变；未完成新关卡或发布。
- 2026-11-16：新增 P3 映射常规文件与时间 API（T08 io）；完整 ARM64 回归通过（Host 22、Java 43、Native 58、Bionic 75）。Go 1.27.4、MyGo v0.9.2 与 Mac.apk `3f35611` 已检查、未变；未完成新关卡或发布。
- 2026-11-16：新增 P3 可执行范围客体故障诊断（T08 faults）；完整 ARM64 回归通过（Host 22、Java 43、Native 58、Bionic 83）。Go 1.27.4、MyGo v0.9.2 与 Mac.apk `3f35611` 已检查、未变；JVM/客体混合故障仍留在 P4，未完成新关卡或发布。
- 2026-11-16：新增客体 ABI 受限格式化器及固定宿主字节日志入口（T08 format）；完整 ARM64 回归通过（Host 22、Java 43、Native 58、Bionic 92）。Go 1.27.4、MyGo v0.9.2 与 Mac.apk `3f35611` 已检查、未变；完整 N1/APK/JNI 准入尚未完成，不发布。
- 2026-11-17：以逻辑系统库、经验证的 `LIBC` 导入及真实 CRT 生命周期记录完成已声明的 P3 C/Bionic 准入子集；普通 NDK 链接的 C 夹具及完整 ARM64 回归通过（Host 22、Java 43、Native 58、Bionic 104）。Go 1.27.4、MyGo v0.9.2 与 Mac.apk `3f35611` 已检查、未变；P4/APK/JNI 与 G3 尚未完成，不发布。
- 2026-11-17：新增 P4 真实 JVM 的 JNI 基础：每线程代理、客体 .../V/A 调用、引用/异常、loader 上下文及 worker attach/detach；通过 JVM/JIT 边界回归修复客体 PC 故障诊断优先级。requirements/build All、Host 23、Java 43、Native 58、Bionic 104、Jni 11 通过。Go 1.27.4、MyGo v0.9.2 与 Mac.apk `3f35611` 已检查、未变；Java→guest native 绑定/APK 集成及 G3 尚未完成，0.1.0 未发布。
- 2026-11-17：跨 JNI 宿主入口及同绑定重入保留 Bionic 线程状态，在最后代理失效前关闭 key/TLS 状态。requirements/build All、Host 23、Java 43、Native 58、Bionic 104、Jni 13 通过。Go 1.27.4、MyGo v0.9.2 与 Mac.apk `3f35611` 已检查、未变；实际 Java→guest native thunk/APK 集成及 G3 仍未完成，不发布。
- 2026-11-17：新增有界静态 ARM64 PE JNI 入口与真实 JVM native 注册、混合固定签名参数/结果、嵌套及 worker 回调、异常传播和真实注销。requirements/build All、Host 23、Java 43、Native 58、Bionic 104、Jni 14 通过；最终 Jni 运行 1.36 秒。Go 1.27.4、MyGo v0.9.2 与 Mac.apk `3f35611` 已检查、未变；导出绑定/JNI_OnLoad/APK 集成及 G3 仍未完成，不发布。
