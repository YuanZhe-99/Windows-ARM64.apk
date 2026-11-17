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

版本 0.1.0 尚未发布。G0–G3 在 [platform-notes.md](platform-notes.md) 记录的 Windows 11 ARM64
机器上通过：Java/Kotlin APK、N0 ELF/ABI、已声明 C/Bionic 子集及真实 hello-jni Activity 矩阵。
`--native=restricted` 必须显式指定，原生默认仍关闭。T09 覆盖双向 JNI、类型数值、异常、
依赖、加载 hook、worker/嵌套回调及不支持入口的明确失败。
独立启用 CFG 的构建与恢复后的普通构建均通过完整回归。

这不表示任意 NDK/C++/ELF-TLS 或图形兼容。P5/P6 图形及 G4/G5 仍未验证；
本机仅有 Mesa Dozen 转换层 Vulkan、没有厂商驱动，该路径需要硬件或明确决定。
MyGo 界面仍使用 `winapk.Mock`，暂缓前端集成。当前为 Mac.apk `3f35611`、Go 1.27.4、MyGo v0.9.2。

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
