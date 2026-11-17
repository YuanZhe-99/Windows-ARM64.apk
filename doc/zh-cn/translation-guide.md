# 翻译指南

## 1. 范围

`doc/en-us/` 为权威版本。`doc/zh-cn/` 镜像它；界面字符串位于 ARB 目录中。`PLAN.md` 以中文撰写，不做镜像。

## 2. 结构一致

两棵目录树包含相同的文件，标题、表格行和代码块数量相同；代码块逐字节一致。由
`go run ./tools/checkdocs` 强制检查。

## 3. 不翻译的内容

代码、命令、路径、配置键、标识符、错误类别（`GUEST_ABI_UNSUPPORTED` 等）、夹具/阶段/关卡/profile 编号（`T01`、`P0`、`G1`、`N0`）、
状态值（`PASS`、`FAIL`、`BLOCKED`、`NOT_RUN`）、版本号、产品与公司名称（Windows-ARM64.apk、Mac.apk、MyGo、Kyokai、Android、
Microsoft），以及 API 与格式名称（JNI、ELF、PE、DEX、Bionic、EGL、GLES、Vulkan、ANGLE、WSI、TLS）。

## 4. 风格

句子平实简短。中文使用全角标点，中英文之间仅在有助阅读时加空格。

## 5. 术语表

| English | 简体中文 |
|---|---|
| host | 宿主 |
| guest | 客体 |
| ABI domain | ABI 域 |
| runtime (process) | 运行时（进程） |
| front end | 前端 |
| phone-style pane | 手机式窗格 |
| standalone window | 独立窗口 |
| main window | 主窗口 |
| loader | 加载器 |
| relocation | 重定位 |
| thunk | thunk |
| shim | shim |
| bridge | 桥 |
| native profile | 原生执行 profile |
| fixture | 夹具 |
| gate | 关卡 |
| phase | 阶段 |
| capability report | 能力报告 |
| compatibility report | 兼容性报告 |
| backend | 后端 |
| subtree | subtree |
| upstream | 上游 |
| port audit | 移植审计 |
| library (of apps) | 应用库 |
| group (of panes) | 组 |
| sample data | 示例数据 |
| sheet | 面板 |
| translated (Vulkan driver) | 转换层（Vulkan 驱动） |
| baseline (vcpkg) | 基线 |
| requirement | 环境要求 |
| active code page | 活动代码页 |
| build identity | 构建标识 |
| direct (call) | 直通 |
| screen, screening (of code) | 筛查 |
| refusal, refuse (a module) | 拒绝 |
| callee-saved register | 被调用方保存的寄存器 |
| homogeneous floating-point aggregate (HFA) | 同质浮点聚合（HFA） |
| variant calling convention | 变体调用约定 |
| emulated TLS | 仿真 TLS |

## 6. 审阅清单

- 两棵树的文件、标题、表格和代码块一致。
- 术语使用一致。
- 链接在各自目录树中指向相同的文档。
