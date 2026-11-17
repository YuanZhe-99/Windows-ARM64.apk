# 开发

## 环境要求

- `go.mod` 中写明的最新 Go 发行版（较旧的 Go 会自动下载它）。MyGo 是 Go 工具依赖（`go tool mygo …`），使用最新标签。
- 运行时的每次运行都需要一台 Windows 11 ARM64 机器；图形工作还需要桌面会话和带 ARM64 Vulkan 驱动的 GPU。
- 运行时：`build-lock.json` 锁定的内容。带 ARM64 C++ 工具、Clang、CMake 和 vcpkg 组件的 Visual Studio 2026
  提供 clang-cl、MSVC、Windows SDK、CMake、Ninja 和 vcpkg（都不在 PATH 上）。JDK 是适用于 ARM64 的
  Microsoft OpenJDK 21，用 `winget install --id Microsoft.OpenJDK.21 -e --architecture arm64` 安装（其 MSI
  设置系统 `JAVA_HOME` 和 PATH；构建时可用 `WINAPK_JAVA_HOME` 覆盖）。Android SDK（build-tools 36.0.0、
  android-36、cmdline-tools）和 NDK r30 用于构建平台资源和夹具，Kotlin 夹具使用 Android Studio 自带的 kotlinc 2.4.0（或 `KOTLIN_HOME`）；Android Studio 的默认位置 `%LOCALAPPDATA%\Android\Sdk`
  无需 `ANDROID_HOME` 即可找到（用 Android Studio 或 `android sdk install` 安装）。`scripts/windows/check-requirements.ps1`（与 `scripts/windows/` 下的所有脚本一样在 PowerShell 7 `pwsh` 中运行）检查全部内容，不安装任何东西。

## 命令

```bash
gofmt -l cmd internal tools
go vet ./...
go test ./...
CGO_ENABLED=0 go test ./...
go tool mygo vet .
go tool mygo i18n check
go run ./tools/checkdocs           # doc/en-us and doc/zh-cn mirror each other
```

运行时的命令见下一节。

## 构建和测试运行时

```powershell
.\scripts\windows\check-requirements.ps1
.\scripts\windows\build.ps1 -Component All          # or Java, FrameworkRes, Fixtures, Host, NativeProbe
.\scripts\windows\test.ps1 -Suite Host
.\scripts\windows\test.ps1 -Suite Java
.\scripts\windows\test.ps1 -Suite Native
.\scripts\windows\test.ps1 -Suite Bionic
.\scripts\windows\test.ps1 -Suite Modules
.\scripts\windows\test.ps1 -Suite Memory
.\scripts\windows\test.ps1 -Suite Io
.\scripts\windows\test.ps1 -Suite Faults
.\scripts\windows\test.ps1 -Suite Format
.\scripts\windows\test.ps1 -Suite Libraries
.\build\windows-arm64\bin\native-probe.exe suite .\tests\fixtures\elf\libn0abi.so
.\build\windows-arm64\bin\native-probe.exe sync .\tests\fixtures\elf\libsync.so
.\build\windows-arm64\bin\winapk.exe doctor --json
.\build\windows-arm64\bin\winapk.exe inspect .\tests\fixtures\hello-java.apk --json
.\build\windows-arm64\bin\winapk.exe run .\tests\fixtures\hello-java.apk --native=off
.\build\windows-arm64\bin\winapk.exe dex .\tests\fixtures\hello-java.apk .\build\classes.jar
```

`-Component Java` 把 Mac.apk 的 Java 侧构建到 `build/windows-arm64/lib`（见 [port-audit.md](port-audit.md)）。
`-Component FrameworkRes` 用 SDK 的 `aapt2`，从 `build-lock.json` 指定标签的 AOSP `frameworks/base/core/res`
一次性构建平台资源包 `bin/tools/framework-res.apk`（上游 `scripts/build_framework_res.sh` 的步骤）。
`-Component Fixtures` 用 SDK 的 `aapt2`、`d8`、`zipalign`、`apksigner`，对 `src/**/*.kt` 用 kotlinc（Kotlin 标准库一并转为 DEX），以及（对 `jni/*.c`）NDK 的 clang，把
`tests/windows-arm64/apk-fixtures` 的夹具 APK 构建到 `tests/fixtures`（从不提交）；每个夹具附带记录构建参数和
哈希的 `<name>.json`，每个库都检查是否为 ELF64 AArch64。`-Component Host` 在 Visual Studio 的 ARM64 开发者环境中用
clang-cl 把 `winapk.exe`、`winapk-bridge.dll`、`native-probe.exe` 和 `winapk-probe.jar` 构建到 `build/windows-arm64/bin`，首次构建时
通过 vcpkg 获取 Vulkan 头文件、zlib、SQLite 和 Capstone，生成的任何 PE 不是 ARM64 时即失败。`-Component NativeProbe` 只构建
`native-probe.exe`，并用 NDK 的 clang 把 `tests/windows-arm64/guest-elf/fixtures.psd1` 中的客体 ELF 夹具构建到 `tests/fixtures/elf`
（从不提交；哈希在该目录的 `fixtures.json` 中）。

`test.ps1 -Suite Host` 就是夹具 T01。`test.ps1 -Suite Java` 是 G1（T02、T03、T04 和 `hello-jni` 负例）：它用
`winapk run` 运行夹具，向应用窗口发送点击、按键、字符、改变尺寸、最小化、恢复和失去激活，挂起一次 UI 线程以检查 UI 调用超时（由上游的
`MACAPK_INJECT_FILE` 输入驱动发送返回），从 Unicode 路径运行一份副本，并读取运行时日志；需要桌面会话，数据和日志写到
`build/windows-arm64/test-data`。`doctor --present` 还会打开一个小测试窗口检查 Vulkan 呈现；不加时该项为 `not-run`。`test.ps1 -Suite Native` 是 G2（T05、T06）：
不需要 JVM 和窗口；它对每个 T05 构建运行 `native-probe.exe suite`，对每个 T06 文件运行 `load`（对照 `fixtures.psd1` 检查拒绝），
并运行 `stress`。

`test.ps1 -Suite Bionic` 通过 `native-probe.exe threads` 运行 P3 线程基础（T08），并非 G3：8/32 个同时存在
的 worker、errno/key/仿真 TLS 隔离及析构、明确不支持的用例，再执行 200 轮（1600 个 worker），检查注册表清空和句柄。
每个探测进程有 60 秒期限。还使用 P3 导入表检查 ELF TLS、裸系统调用、平台保留寄存器仍被拒绝。夹具刻意使用
`-nostdlib -femulated-tls`；不能据此证明依赖加载、其他 Bionic 功能组或 JNI。
源码、预期结果与二进制哈希仍记录在夹具配方/清单中。

同一套件通过 `native-probe.exe sync` 运行 `libsync.so`：normal/recursive/error-checking mutex、
八线程竞争、带谓词循环的 signal/broadcast、等待/重锁与忙碌销毁保护、过期/复制标记和不支持的属性。
还运行 200 轮同步循环（1600 个 worker），检查超时等待导入被拒绝、仍持有对象时销毁运行时会带诊断终止。
这个无依赖夹具不开放 N1 或 JNI；超时等待/加锁及属性 API 仍不支持（[native-profiles.md](native-profiles.md)）。

`-Suite Modules` 只运行精简的 T07 依赖图矩阵（`native-probe modules`），也包含在 `-Suite Bionic` 中。
它验证夹具哈希，并在单进程、30 秒期限内检查十项语义：回滚、构造顺序、弱导入、客体 dl API/错误、查询、引用/保留映射及初始化重入/并发。
开发模块加载时使用该定向套件，能力稳定后再执行完整运行时回归，不在每次编辑后重复全量测试。

`-Suite Memory` 只选择 T08 的 `memory` 命令（也包含在 `Bionic` 中）：四项语义、一次宿主状态检查，
以及来自同一源码的两个归属违规终止用例。使用 `-fno-builtin`，确保测试调用导入包装而不是编译器折叠的操作；
不另加耗时压力循环。

`-Suite Io` 只选择 T08 的 `io` 命令（也在 `Bionic` 中）：四项文件/时间语义、宿主状态及清单检查，
并确认 Unicode 映射字节和根目录外 junction 目标未变。
只在 `build/windows-arm64/test-data` 下准备被忽略的测试目录。
探测命令为 `io <elf> <android-root>`；不改变宿主工作目录或环境。

`-Suite Faults` 选择 T08 的 `faults` 命令（也在 `Bionic` 中）：一次宿主 SEH 链用例，
以及两个单独终止的客体进程（访问违规与 trap）。每份终止诊断必须包含模块与客体虚拟地址，
退出码为 3，不能伪造成功结果。
N0 的 `Native` 回归也在已有加载/卸载循环中覆盖新的可执行范围注册表，不另加重复故障压力套件。

`-Suite Format` 选择有限的 T08 格式化/日志矩阵（也在 `Bionic` 中）。
`NativeProbe` 与 `All` 把 Android ABI 辅助库构建到 `bin/guest/libwinapk-bionic.so`，
在 `bin/guest/bionic.json` 记录其源码、完整 NDK 参数及哈希。
套件核对辅助库和夹具哈希、五项用例及精确 UTF-8 日志字节。
辅助库是产品代码，不是系统 libc 或生成的测试夹具；不直接使用宿主可变参数导出。

`-Suite Libraries` 选择 T07 的 N1 系统库/版本/CRT 矩阵（也在 `Bionic` 中）。
正例库使用 NDK 驱动的普通共享库链接，包含 CRT 启动文件及 libc/libdl/liblog 依赖：
不使用 `-nostdlib`、`-nostartfiles`、`-femulated-tls` 或禁用栈保护的参数。
它只验证所记录的命令及源码，不表示所有 Android Studio/CMake 配置或任意 NDK 输出均可用。
三个小拒绝变体在构造前覆盖不支持的 fork 导入、有效但未知的版本名称/哈希，以及损坏的版本链。
同一进程还检查逻辑查询范围、真实 finalization 及保持映射的退出回调。
`fixtures.json` 记录完整普通链接参数；被忽略的 `test-data/n1-libraries` 中的暂存副本保持各变体原有 SONAME，
不修改源夹具。

`winapk run` 把每个应用的数据放在 `%LOCALAPPDATA%\Windows-ARM64.apk\Apps` 下（`--data-dir` 可改）：`android\`
是应用看到的 Android 根目录（`data\data\<package>`、`sdcard`），`code\<package>\` 是转换后的代码及缓存戳，
`logs\` 是 JVM 的崩溃日志。运行时日志写到标准错误。`winapk dex` 就是单独执行的 `run` 转换步骤：在自身进程的 JVM 中把 APK 的 DEX 转换为 jar。

## 运行界面

```bash
go run ./cmd/windows-arm64-apk
WINAPK_FRESH=1 go run ./cmd/windows-arm64-apk
SCREENSHOT_DIR=/path/to/dir go test ./internal/gui -run TestScreensRender
```

第一条对接 `design/` 的示例应用运行；`WINAPK_FRESH=1` 以空的应用库、没有 Java 运行时启动，如同首次
运行。测试把每个界面以两种语言和两种外观写成 PNG，用于与 `design/` 对比。字符串在 `locales/en.json`
和 `locales/zh-Hans.json` 中；修改后运行 `go tool mygo generate`。

## 更新 Mac.apk

Mac.apk 是 `macapk/` 下的 git subtree，跟随上游最新 `main`：

```bash
git ls-remote https://github.com/kksimp/Mac.apk main
git subtree pull --prefix=macapk https://github.com/kksimp/Mac.apk main --squash
```

首次引入使用参数相同的 `git subtree add`。每次合入后，重新核对 [upstream.md](upstream.md) 列出的入口，并重跑合入文件所涉及的关卡。

## 测试原则

测试保护产品契约和易错逻辑，优先使用表驱动；每个修复的缺陷都有回归测试。夹具自建且可公开，每个都有源码、构建参数、
哈希和期望输出，每项能力都有正例和负例。每个测试注明它覆盖的夹具编号（`T01`…）或关卡。执行、JVM 与 GPU 测试只在
ARM64 真机上计数；结果报告为 `PASS`、`FAIL`、`BLOCKED` 或 `NOT_RUN`。
