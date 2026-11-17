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
| [port-audit.md](port-audit.md) | P0：Mac.apk 中哪些部分与 macOS 绑定，以及在 Windows 上如何处理 |
| [decisions.md](decisions.md) | 架构决策（ADR） |
| [upstream.md](upstream.md) | 上游契约与特殊行为：Mac.apk、Windows ARM64 ABI、Android NDK、MyGo |
| [third-party.md](third-party.md) | 复用的代码与依赖、许可证与义务 |
| [platform-notes.md](platform-notes.md) | 各平台已实现与已验证的内容、关卡状态 |
| [development.md](development.md) | 环境、命令、开发流程 |
| [translation-guide.md](translation-guide.md) | 镜像规则与术语表 |
| [version-history.md](version-history.md) | 各版本及其使用的上游版本 |
| [functions/INDEX.md](functions/INDEX.md) | 各包与各模块的职责和关键声明 |

## 项目状态

开发于 2026-11-16 在未发布的 0.1.0 版本恢复（2026-11-05 起曾暂停）。G0、G1 和 G2 在
[platform-notes.md](platform-notes.md) 记录的 Windows 11 ARM64 机器上通过；G3、G4 和 G5 尚未运行。已有内容：
`winapk run` 在 Win32 窗口中运行纯 Java 和 Kotlin APK；`native-probe.exe` 加载 profile N0 的客体 ELF 库并在其上运行
ABI 测试集；MyGo 界面对接示例数据（`winapk.Mock`），尚不启动运行时。当前构建使用 Mac.apk `3f35611`、
Go 1.27.4 和 MyGo v0.9.2。

P3 已声明子集已完成；后续工作按 `PLAN.md` §11（P4：JNI，以及在 `winapk run` 中加载 APK 的库）推进。
独立 T08 线程基础现已覆盖受管理 pthread、errno、key/析构、显式仿真 C TLS 与无超时 mutex/condition 同步；
T07 新增事务式依赖图、构造函数与客体 dl API；两者均不开放完整 N1 或 APK 原生库加载。
T08 还覆盖私有堆分配、字节/字符串及映射常规文件/时间 API。
P3 已声明的 C/Bionic 准入现已通过普通 NDK 链接的 C 夹具，包含逻辑系统库、版本与真实 CRT 生命周期回调。
完整 N1/JNI 及 APK 原生库加载仍属于 P4/G3。
独立的真实 JVM `jni-probe.exe` 已通过 T09 双向 JNI 调用，包含显式注册的静态 PE 入口、
引用/异常、嵌套回调及 worker 附着。导出名绑定、JNI_OnLoad 及 APK 集成仍未完成。
优先推进后端，暂缓前端集成。

## 上游版本

Go、MyGo 与 Mac.apk 不固定版本：始终跟随各自的最新版本。下表为最近一次检查（2026-11-17）时的最新版本；
其余依赖锁定在 `build-lock.json` 中，它也记录上次构建实际使用的这三者的版本。

| 组件 | 策略 | 最近检查时的最新版本 |
|---|---|---|
| Go | 最新发行版 | 1.27.4 |
| MyGo | 最新标签 | v0.9.2 |
| Mac.apk | 最新 `main`，作为 `macapk/` subtree | `3f35611`，已合并 |
| OpenJDK（ARM64）、clang-cl、MSVC、Windows SDK、CMake、Ninja、vcpkg、Vulkan 头文件 | 锁定在 `build-lock.json` | Microsoft OpenJDK 21.0.13；clang-cl 22.1.3；MSVC 14.51；SDK 10.0.26100；CMake 4.3.1；Ninja 1.13.2；vcpkg 基线 `afa249f`；Vulkan 头文件 1.4.363；Capstone 5.0.9 |
| Android SDK/NDK、kotlinc、AOSP 框架资源 | 锁定在 `build-lock.json` | build-tools 36.0.0、android-36；NDK r30（30.0.16248370）；kotlinc 2.4.0；`android-16.0.0_r2` |
| ANGLE、Skia | 在 P5a 和 P8 选定 | 尚未选定；Vulkan loader 使用 Windows 自带的 |
