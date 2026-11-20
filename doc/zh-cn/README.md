# Windows-ARM64.apk 文档

**这些文档是代码的权威描述。** `doc/en-us/` 是源；`doc/zh-cn/` 逐文件镜像它。计划与未完成的工作不写在这里；
计划进行期间只存在于仓库的 `PLAN.md` 中。

Windows-ARM64.apk 以原生 Windows 11 ARM64 进程运行选定的 Android APK。它移植自 Mac.apk：DEX → JVM 转换与
Android Framework 运行在原生 ARM64 JVM 上，受限的 ELF 加载器配合 Bionic、JNI 和图形桥运行应用的 `arm64-v8a` 库。
MyGo 前端在手机式窗格或独立窗口中打开每个应用，每个 APK 一个运行时进程。

| 文档 | 内容 |
|---|---|
| [gui.md](gui.md) | 界面：窗口、窗格和组、面板、它的文件、运行时契约草案、示例数据 |
| [abi-compatibility.md](abi-compatibility.md) | 宿主与客体代码之间哪些调用直通、哪些需要 thunk、哪些不受支持，以及证据 |
| [native-profiles.md](native-profiles.md) | N0 及已声明的 P3 C/Bionic 子集（T07/T08）、准入、ABI 限制与拒绝 |
| [graphics-capabilities.md](graphics-capabilities.md) | 图形验证路径、已选 ANGLE 源码构建及显式后端契约 |
| [port-audit.md](port-audit.md) | P0：Mac.apk 中哪些部分与 macOS 绑定，以及在 Windows 上如何处理 |
| [decisions.md](decisions.md) | 架构决策（ADR） |
| [upstream.md](upstream.md) | 上游契约与特殊行为：Mac.apk、Windows ARM64 ABI、Android NDK、MyGo |
| [third-party.md](third-party.md) | 复用的代码与依赖、许可证与义务 |
| [platform-notes.md](platform-notes.md) | 各平台已实现与已验证的内容、关卡状态 |
| [development.md](development.md) | 环境、命令、开发流程 |
| [ci.md](ci.md) | GitHub Actions ARM64 持续集成、发布打包与 Ed25519 更新签名 |
| [translation-guide.md](translation-guide.md) | 镜像规则与术语表 |
| [version-history.md](version-history.md) | 各版本及其使用的上游版本 |
| [functions/INDEX.md](functions/INDEX.md) | 各包与各模块的职责和关键声明 |

## 项目状态

版本 0.1.1 为当前 Windows 11 ARM64 实验版构建。在 [platform-notes.md](platform-notes.md) 记录的 Windows 11 ARM64 机器上：
G0–G3 已对 Java/Kotlin APK、N0 ELF/ABI、已声明 C/Bionic 子集与 `hello-jni` Activity 矩阵（含独立启用 CFG
的构建）验证通过；T14 完成 20 次冷/热启动、100 次 resize/最小化恢复循环及完整 30 分钟连续稳定性运行；MyGo
前端已按全部 17 张 `design/` 设计稿完成并接入真实 `winapk` 运行时（`--embed` 共享内存帧与双工 IPC）；P8
宿主服务（`NativeOs`、剪贴板、DirectWrite 中日韩字体、`/proc`、动态 DPI）、WASAPI `NativeAudio` 与 Skia
Direct3D 12 AGSL/RenderEffect（`T15`）均通过契约测试。受限 GLES（`T10.java`、`T11`）与 Vulkan（`T13`
WSI、SPIR-V 图形管线、离屏 `VkImage` 回读、客体回调与设备丢失恢复）在本机 Adreno 8cx Gen 3 + Mesa Dozen
（`translated=yes`，ADR-0004）上通过；8cx Gen 3 上的厂商原生 ARM64 Vulkan ICD 仍为 `BLOCKED`。当前为
Mac.apk `154f64e`、Go 1.27.4、MyGo v0.10.0。

## 上游版本

Go、MyGo 与 Mac.apk 不固定版本：始终跟随各自的最新版本。下表为最近一次检查（2026-11-19）时的最新版本；
其余依赖锁定在 `build-lock.json` 中，它也记录上次构建实际使用的这三者的版本。

| 组件 | 策略 | 最近检查时的最新版本 |
|---|---|---|
| Go | 最新发行版 | 1.27.4 |
| MyGo | 最新标签 | v0.10.0 |
| Mac.apk | 最新 `main`，作为 `macapk/` subtree | `154f64e`，已合并 |
| OpenJDK（ARM64）、clang-cl、MSVC、Windows SDK、CMake、Ninja、vcpkg、Vulkan 头文件 | 锁定在 `build-lock.json` | Microsoft OpenJDK 21.0.13；clang-cl 22.1.3；MSVC 14.51；SDK 10.0.26100；CMake 4.3.1；Ninja 1.13.2；vcpkg 基线 `afa249f`；Vulkan 头文件 1.4.363；liblzma 5.8.4；Capstone 5.0.9 |
| Android SDK/NDK、kotlinc、AOSP 框架资源 | 锁定在 `build-lock.json` | build-tools 36.0.0、android-36；NDK r30（30.0.16248370）；kotlinc 2.4.0；`android-16.0.0_r2` |
| ANGLE | 在 P5a 锁定 | `801a1a5`，ARM64 源码构建及宿主/客体 D3D11 + Vulkan 经 Dozen 受限 profile 验证通过 |
| Skia | 在 P8 锁定 | JetBrains Skiko `0.150.2`（`skiko-windows-arm64.dll`，ARM64 Ganesh Direct3D 12，`T15` 11/11 通过）；Vulkan loader 使用 Windows 自带的 |
