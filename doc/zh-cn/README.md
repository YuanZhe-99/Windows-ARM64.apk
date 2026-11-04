# Windows-ARM64.apk 文档

**这些文档是代码的权威描述。** `doc/en-us/` 是源；`doc/zh-cn/` 逐文件镜像它。计划与未完成的工作不写在这里；
计划进行期间只存在于仓库的 `PLAN.md` 中。

Windows-ARM64.apk 以原生 Windows 11 ARM64 进程运行选定的 Android APK。它移植自 Mac.apk：DEX → JVM 转换与
Android Framework 运行在原生 ARM64 JVM 上，受限的 ELF 加载器配合 Bionic、JNI 和图形桥运行应用的 `arm64-v8a` 库。
MyGo 前端在手机式窗格或独立窗口中打开每个应用，每个 APK 一个运行时进程。

| 文档 | 内容 |
|---|---|
| [gui.md](gui.md) | 界面：窗口、窗格和组、面板、它的文件、运行时契约草案、示例数据 |
| [upstream.md](upstream.md) | 上游契约与特殊行为：Mac.apk、Windows ARM64 ABI、Android NDK、MyGo |
| [third-party.md](third-party.md) | 复用的代码与依赖、许可证与义务 |
| [platform-notes.md](platform-notes.md) | 各平台已实现与已验证的内容、关卡状态 |
| [development.md](development.md) | 环境、命令、开发流程 |
| [translation-guide.md](translation-guide.md) | 镜像规则与术语表 |
| [version-history.md](version-history.md) | 各版本及其使用的上游版本 |
| [functions/INDEX.md](functions/INDEX.md) | 各包与各模块的职责和关键声明 |

## 上游版本

Go、MyGo 与 Mac.apk 不固定版本：始终跟随各自的最新版本。下表为最近一次检查（2026-11-03）时的最新版本；
P0 创建 `build-lock.json` 后，由它记录每次构建实际使用的版本。

| 组件 | 策略 | 最近检查时的最新版本 |
|---|---|---|
| Go | 最新发行版 | 1.27.3 |
| MyGo | 最新标签 | v0.7.4 |
| Mac.apk | 最新 `main`，作为 `macapk/` subtree | `ff2afbf`（自 subtree 的 `367fd1a` 起只改了上游的 `README.md`；尚未合并，由运行时 Agent 下次合并） |
| OpenJDK（ARM64）、Android SDK/NDK、ANGLE、Vulkan Loader、Skia | 在 P0 选定并锁定 | 尚未选定 |
| clang-cl、Windows SDK、CMake、Ninja | 在 P0 选定并锁定 | 尚未选定 |
