# 第三方代码

Windows-ARM64.apk 以 AGPL-3.0 授权。本清单列出它复用或嵌入的每个上游、用途及相应义务。声明文本本身见
`THIRD_PARTY_NOTICES.md`。

## 清单

| 组件 | 仓库与版本 | 用途 | 许可证 | 修改 | 义务 |
|---|---|---|---|---|---|
| MyGo | `egoist/mygo`，最新标签 | 界面框架、打包 | MIT | 无 | 保留版权声明；其内嵌的 Kage 编译器和 MingCute 图标为 Apache-2.0 |
| Kyokai、Easytier-MyGo 与 Nextcloud-MyGo 的文件 | `YuanZhe-99/Kyokai`、`YuanZhe-99/Easytier-MyGo`、`YuanZhe-99/Nextcloud-MyGo` | `tools/checkdocs`、`internal/version` 模式、MyGo 维护 skill、工作规范；Kyokai 是设计参考 | AGPL-3.0 / GPL-3.0，由其作者以 AGPL-3.0 重新授权 | 调整了名称和注释 | 除 Windows-ARM64.apk 自身许可证外无其他义务 |
| Microsoft Build of OpenJDK 21 | 21.0.13，`build-lock.json` | 构建 Java 侧；开发时 `winapk` 嵌入的 JVM | GPL-2.0 附 Classpath Exception | 无 | 尚不随包发布；P7 的私有运行时随其许可证和声明一起发布 |
| Vulkan-Headers | 1.4.363.0，经 vcpkg，`build-lock.json` | 编译 `winapk` 的 Vulkan 探测 | Apache-2.0 | 无 | 仅构建时使用；不随包发布 |
| zlib | 1.3.2，经 vcpkg，`build-lock.json` | 在 `winapk` 中解压 APK 条目（`apk.cpp`） | zlib | 无 | 以 DLL 形式随 `winapk.exe` 发布；保留其声明 |
| SQLite | 3.54.1，经 vcpkg，`build-lock.json` | 经上游的 `NativeSqlite`（`winapk-bridge.dll` 中的 `native_sqlite.cpp`）提供 `android.database.sqlite` | 公有领域 | 无 | 以 DLL 形式随 `winapk.exe` 发布；无须声明 |
| AOSP `frameworks/base/core/res` | 标签 `android-16.0.0_r2`，`build-lock.json` | 编译为 `bin/tools/framework-res.apk`，即应用解析 `@android:` 所用的平台资源 | Apache-2.0 | 无（与上游一样，所有 feature flag 编译为只读 true） | 随运行时发布；保留许可证和 NOTICE |
| Android SDK build-tools、平台和 NDK | build-tools 36.0.0、android-36、NDK r30，`build-lock.json` | 平台资源所用的 `aapt2`；构建夹具 APK、其中的库以及客体 ELF 夹具 | Android Software Development Kit License | 无 | 仅构建时使用；不随包发布，夹具是生成的，从不提交 |
| Capstone | 5.0.9 via vcpkg，`build-lock.json` | 客体加载器对 ARM64 代码的筛查（`elf_loader.cpp`，`native-probe.exe` 中） | BSD-3-Clause | 无 | 随 `native-probe.exe` 以 `capstone.dll` 提供，这是一个开发工具，尚未进入任何包；加载器进入运行时后随之发布，保留其声明 |
| Android 的 `org.apache.http.legacy` | 经 Mac.apk（`macapk/thirdparty/apache-http-legacy`，来自 Android 14 系统镜像、AOSP `platform/external/apache-http`） | 打包进 `android-framework.jar` 的 Apache HttpClient 及 Android 的旧 HTTP 客户端，如同设备上的共享库 | Apache-2.0 | 无（上游已用其 DEX 转换器转换） | 随运行时发布；保留许可证和 Apache HttpComponents 的 NOTICE |
| Kotlin 编译器与标准库 | 2.4.0，Android Studio 自带，`build-lock.json` | 编译 Kotlin 夹具 T04；`kotlin-stdlib.jar` 转为 DEX 放入该 APK | Apache-2.0 | 无 | 仅构建时使用；夹具是生成的，从不提交，也不随包发布 |
| ANGLE 及公开 DEPS | `google/angle`、`801a1a5`、`build-lock.json` | 可选 P5a ARM64 EGL/GLES 源码构建，含 Vulkan 与 D3D11 | BSD-3-Clause；依赖保留各自许可证（含 Apache-2.0/LLVM 例外、MIT、zlib） | 固定 Chromium build 依赖适配锁定 SDK/自定义 VS，不改 renderer | 保留 ANGLE LICENSE 与依赖声明；P7 分发这些 DLL 前必须枚举实际链接的 GN 依赖并附完整文本；尚未打包 |
| Mac.apk | `kksimp/Mac.apk`，最新 `main`，`macapk/` 下的 git subtree（在 `3f35611`） | DEX 转换、Android Framework、APK/资源解析、加载器与桥代码 | AGPL-3.0（上游另提供商业许可，本项目不使用） | 为 Windows 原地修改，按提交记录 | 保留文件头和声明；每次发行附带 `macapk/` 的完整对应源码；其内含组件保留各自许可证（`macapk/THIRD_PARTY_NOTICES.txt`） |

## 商标

“Mac.apk”及其标志用于标识官方 Mac.apk 项目，不随代码授权（上游 `TRADEMARKS.md`）：分支必须使用不同的名称和图标，
可以说明“基于 Mac.apk”，不得自称官方或暗示获得认可。Windows-ARM64.apk 据此使用自己的名称和图标。Android 是 Google LLC 的商标。

## 计划中的组件

Skia 在某个阶段首次用它构建时加入本清单。
Vulkan loader 是 Windows 自带的，绝不随包发布。Mac.apk 的 macOS 预编译产物（ANGLE、MoltenVK、Skia `.dylib`）绝不进入 Windows 包。
