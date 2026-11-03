# 第三方代码

Windows-ARM64.apk 以 AGPL-3.0 授权。本清单列出它复用或嵌入的每个上游、用途及相应义务。声明文本本身见
`THIRD_PARTY_NOTICES.md`。

## 清单

| 组件 | 仓库与版本 | 用途 | 许可证 | 修改 | 义务 |
|---|---|---|---|---|---|
| MyGo | `egoist/mygo`，最新标签 | 界面框架、打包 | MIT | 无 | 保留版权声明；其内嵌的 Kage 编译器和 MingCute 图标为 Apache-2.0 |
| Kyokai、Easytier-MyGo 与 Nextcloud-MyGo 的文件 | `YuanZhe-99/Kyokai`、`YuanZhe-99/Easytier-MyGo`、`YuanZhe-99/Nextcloud-MyGo` | `tools/checkdocs`、`internal/version` 模式、MyGo 维护 skill、工作规范；Kyokai 是设计参考 | AGPL-3.0 / GPL-3.0，由其作者以 AGPL-3.0 重新授权 | 调整了名称和注释 | 除 Windows-ARM64.apk 自身许可证外无其他义务 |
| Mac.apk | `kksimp/Mac.apk`，最新 `main`，`macapk/` 下的 git subtree（P0 引入） | DEX 转换、Android Framework、APK/资源解析、加载器与桥代码 | AGPL-3.0（上游另提供商业许可，本项目不使用） | 为 Windows 原地修改，按提交记录 | 保留文件头和声明；每次发行附带 `macapk/` 的完整对应源码；其内含组件保留各自许可证（`macapk/THIRD_PARTY_NOTICES.txt`） |

## 商标

“Mac.apk”及其标志用于标识官方 Mac.apk 项目，不随代码授权（上游 `TRADEMARKS.md`）：分支必须使用不同的名称和图标，
可以说明“基于 Mac.apk”，不得自称官方或暗示获得认可。Windows-ARM64.apk 据此使用自己的名称和图标。Android 是 Google LLC 的商标。

## 计划中的组件

OpenJDK、Android SDK/NDK 与 AOSP 资源、ANGLE、Vulkan Loader 和 Skia 在 P0 选定并锁定到 `build-lock.json` 时，连同确切版本和
许可证加入本清单。Mac.apk 的 macOS 预编译产物（ANGLE、MoltenVK、Skia `.dylib`）绝不进入 Windows 包。
