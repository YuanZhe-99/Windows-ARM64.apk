# 版本历史

## 0.1.0（进行中）

- 仓库脚手架：工作规范（`AGENTS.md`）、计划（`PLAN.md`）、镜像文档、文档镜像检查和 Go 模块。
- Go 1.27.3、MyGo v0.7.4、Mac.apk `main` 位于 `367fd1a`（2026-11-03 检查），原样引入为 `macapk/` subtree。
- 2026-11-04：Mac.apk `main` 合并到 `8b45e48`（上游 v1.0.2377 同步）；Go 1.27.3 和 MyGo v0.7.4 已检查，未变。
- 2026-11-05：G2 之后停止开发；0.1.0 从未发布或打标签（[README.md](README.md#项目状态)）。
- 2026-11-16：恢复开发；升到 Go 1.27.4、MyGo v0.9.2 和 Mac.apk `main` 的 `3f35611`（上游 v1.0.2379 同步），重新验证 G0–G2（[platform-notes.md](platform-notes.md)）。
- 2026-11-16：开始后端 P3 线程基础（T08）；Go 1.27.4、MyGo v0.9.2 与 Mac.apk `3f35611` 已检查、未变；版本仍为未发布的 0.1.0。
- 2026-11-16：新增 P3 进程私有 mutex/condition 同步（T08 `sync`）；Go 1.27.4、MyGo v0.9.2 与 Mac.apk `3f35611` 已检查、未变；未完成新关卡或发布。
- 2026-11-16：新增 P3 事务式模块依赖图与客体 dl API（T07）；完整 ARM64 回归通过（Host 22、Java 43、Native 58、Bionic 56）。Go 1.27.4、MyGo v0.9.2 与 Mac.apk `3f35611` 已检查、未变；G3 仍为 NOT_RUN，不发布。
