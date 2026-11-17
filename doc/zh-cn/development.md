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

Go 模块展开包通配符时忽略 `./build`。该生成/下载目录可能包含大型第三方 checkout，
不是产品 Go 源码。根目录的 `cmd`、`internal`、`locales` 和 `tools` 仍由标准 Go 命令覆盖。

```powershell
.\scripts\windows\check-requirements.ps1
.\scripts\windows\build.ps1 -Component All          # or Java, FrameworkRes, Fixtures, Host, NativeProbe, JniProbe
.\scripts\windows\build.ps1 -Component Angle        # optional P5a source build; separate from Java
.\scripts\windows\build.ps1 -Component AngleProbe
.\scripts\windows\test.ps1 -Suite GraphicsHost      # host-only; not G4
.\scripts\windows\build.ps1 -Component Host         # after Angle, to enable the Java GLES bridge
.\scripts\windows\test.ps1 -Suite GraphicsJava      # real APK; not guest GLES or G4
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
.\scripts\windows\test.ps1 -Suite Jni
.\scripts\windows\test.ps1 -Suite ApkJni
.\build\windows-arm64\bin\native-probe.exe suite .\tests\fixtures\elf\libn0abi.so
.\build\windows-arm64\bin\native-probe.exe sync .\tests\fixtures\elf\libsync.so
.\build\windows-arm64\bin\winapk.exe doctor --json
.\build\windows-arm64\bin\winapk.exe inspect .\tests\fixtures\hello-java.apk --json
.\build\windows-arm64\bin\winapk.exe run .\tests\fixtures\hello-java.apk --native=off
.\build\windows-arm64\bin\winapk.exe run .\tests\fixtures\hello-jni.apk --native=restricted
.\build\windows-arm64\bin\winapk.exe dex .\tests\fixtures\hello-java.apk .\build\classes.jar
```

`-Component Java` 把 Mac.apk 的 Java 侧构建到 `build/windows-arm64/lib`（见 [port-audit.md](port-audit.md)）。
`-Component FrameworkRes` 用 SDK 的 `aapt2`，从 `build-lock.json` 指定标签的 AOSP `frameworks/base/core/res`
一次性构建平台资源包 `bin/tools/framework-res.apk`（上游 `scripts/build_framework_res.sh` 的步骤）。
`-Component Fixtures` 用 SDK 的 `aapt2`、`d8`、`zipalign`、`apksigner`，对 `src/**/*.kt` 用 kotlinc（Kotlin 标准库一并转为 DEX），以及（对 `jni/*.c`）NDK 的 clang，把
`tests/windows-arm64/apk-fixtures` 的夹具 APK 构建到 `tests/fixtures`（从不提交）；每个夹具附带记录构建参数和
哈希的 `<name>.json`，每个库都检查是否为 ELF64 AArch64。`-Component Host` 在 Visual Studio 的 ARM64 开发者环境中用
clang-cl 把 `winapk.exe`、`winapk-bridge.dll`、`native-probe.exe`、`jni-probe.exe` 和 `winapk-probe.jar` 构建到 `build/windows-arm64/bin`，首次构建时
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

`-Component JniProbe` 只构建 `jni-probe.exe`、其 Java 探测 jar、Android ABI 的
JNI/Bionic 辅助库及 ELF 夹具配方。`-Suite Jni` 运行 T09 基础，不是 G3：
一个带 `-Xcheck:jni` 的真实 ARM64 JVM 检查构造/静态/实例/非虚 .../V/A 调用、
引用/字符串/数组、pending exception，以及普通/daemon worker 附着。
九组语义共用一个进程，包含真实 JVM native 注册/调用、持久/嵌套入口状态及销毁；
五个短终止进程检查不支持的 JNI、跨线程环境误用、客体 trap、访问违规及失败 JNI hook 注册的方法入口。
每个进程限时 15 秒。七项夹具哈希、两项辅助库哈希及语义/汇总/终止结果构成 24 项检查。
CheckJNI 警告会使正例失败。
正例使用 `-Xbatch`，并检查 `T09-compilation.xml` 中是否安装了 Java 隐式故障方法的编译，
而不假定热循环就一定运行了 JIT 代码。
JNI 辅助库和核心夹具使用 `-O2 -nostdlib -fno-builtin -fno-stack-protector`；
核心夹具另用 `-femulated-tls` 测试已声明的 TLS model。独立库家族使用普通 NDK API-30
共享库链接、CRT 及 libc/libdl 依赖，不带这些特殊参数。只证明这些配方，不证明任意 NDK APK 加载。
独立的 `native-probe` 仍不含 JVM。
APK JNI 夹具也按 manifest 的 API 30 基线普通链接。原生 APK 夹具编译现在从 manifest 读取 API，
不再对全部夹具固定假设 API 24。
日志及 JVM 错误文件保存在被忽略的 `test-data/logs/T09-*` 和 `hs_err_pid*.log`。
native 组注册六个固定签名方法，拒绝数据地址、非 native Java 方法、超额槽位保留及其他类加载器。
随后 JVM 直接调用 PE 适配入口，不使用宿主控制的客体测试入口。销毁检查真实注销。
另两组语义覆盖显式 JNI 库生命周期及导出绑定：依赖优先构造、根库专属/仅一次 JNI_OnLoad、
无 hook 的根库、short name 优先、重载 long/Unicode/数组对象描述符名称、依赖导出及显式注册优先。
JVM 专属 `1bad` 声明即使有匹配导出也保持未解析，符合 JNI 歧义转义规则。
拒绝错误版本、pending exception、数据型 hook 及重复失败加载。
独立终止 hook 用例证明无效版本 hook 不能留下可用的 native 绑定。

`-Suite ApkJni` 运行 T09 的 G3 Activity 矩阵：off/restricted 正常关闭启动、一个必要的不支持 JNI
终止启动，以及一个 ZIP 路径穿越拒绝，共八项检查。Activity 断言真实 native 结果 43、显式注册、
仅一次 System/Runtime 加载、Android 命名、后加载类导出及缺失/目录外拒绝。
还检查字符串/数组、long/float/double 和实例结果、异常传播/清除、仅由依赖导出的方法、
依赖 hook 分离、错误版本拒绝缓存及附着 guest worker 的嵌套回调。
正例矩阵启用 CheckJNI；全部启动等待真实首个位图帧，不等待窗口焦点通知（无焦点时不一定触发）。
使用既有的 30 秒日志截止时间：CheckJNI/字体绘制可能使首帧接近此前的 15 秒上限。
不添加固定启动休眠或自动重试。
拒绝按钮调用 MonitorEnter，必须退出 3 并报告槽位 217，不返回替代结果。
截图为 `T09-apk-activity.png`，不替代数值检查。ZIP 路径穿越在 DEX 转换或 boot 之前拒绝。
`hello-jni.json` 记录源码/链接、SONAME 参数及哈希；不增加大范围应用扫描或重复压力循环。

CFG 实验在独立的 `build/windows-arm64/native/CFGDebug` 目录重复 `Build-Host` 的 CMake
配置/构建参数，保持 Debug/ARM64 及相同的输出 bin，并添加以下 CMake 参数：

```powershell
'-DCMAKE_CXX_FLAGS=/DWIN32 /D_WINDOWS /EHsc /guard:cf'
'-DCMAKE_EXE_LINKER_FLAGS=/guard:cf'
'-DCMAKE_SHARED_LINKER_FLAGS=/guard:cf'
```

运行 Host、Java、Native、Bionic、Jni 和 ApkJni 后，在相同 ARM64 开发 shell 中执行
`cmake --build build/windows-arm64/native/Debug --clean-first` 恢复普通构建。
两个构建树共享 bin，单独增量构建 Host 可能保留较新的 CFG 产物；恢复后要求两种运行中进程查询均为 CFG=0。
Jni 和 ApkJni 查询运行中进程的 CFG 策略；native 注册组还在 CFG 启用时，对照运行中 GFID 表验证
每个静态 PE 入口。检查 PE 加载配置，不仅检查请求的编译参数。这只验证已声明 profile 的 CFG，
不证明任意客体控制流安全，也不是安全沙箱。

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
