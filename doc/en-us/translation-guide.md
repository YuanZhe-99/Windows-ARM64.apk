# Translation guide

## 1. Scope

`doc/en-us/` is authoritative. `doc/zh-cn/` mirrors it; interface strings live in the ARB
catalogs. `PLAN.md` is written in Chinese and is not mirrored.

## 2. Structural parity

Both trees contain the same files with the same number of headings, table rows and code blocks;
code blocks are byte-identical. `go run ./tools/checkdocs` enforces this.

## 3. Never translated

Code, commands, paths, config keys, identifiers, error categories (`GUEST_ABI_UNSUPPORTED`, …),
fixture, phase, gate and profile IDs (`T01`, `P0`, `G1`, `N0`), status values (`PASS`, `FAIL`,
`BLOCKED`, `NOT_RUN`), version numbers, product and company names (Windows-ARM64.apk, Mac.apk,
MyGo, Kyokai, Android, Microsoft) and API and format names (JNI, ELF, PE, DEX, Bionic, EGL, GLES,
Vulkan, ANGLE, WSI, TLS).

## 4. Style

Plain, short sentences. Chinese text uses full-width punctuation, with a space between Chinese
and Latin words only where it helps readability.

## 5. Glossary

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

## 6. Review checklist

- Same files, headings, tables and code blocks in both trees.
- Glossary terms used consistently.
- Links point to the same documents in each tree.
