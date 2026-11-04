# 平台说明

| 平台 | 状态 |
|---|---|
| Windows 11 ARM64 | G0 通过；G1 的检查在本机通过（T02、T03）：`winapk run` 在 Win32 窗口中运行纯 Java APK 的真实 DEX，支持触摸、按键、滚动、改变尺寸、返回和保存的状态。界面仍对接示例数据（`winapk.Mock`）。 |
| Windows 11 x64 | 不是目标平台；只可承载构建工具和解析测试。 |
| macOS（Apple Silicon） | 上游 Mac.apk 的平台；已在 `367fd1a` 引入为 `macapk/`；尚未在此运行 macOS 构建或 smoke（`NOT_RUN`）。 |

## Windows

在 Windows 11 ARM64 上验证（2026-11-03）：build 26300.9711，Snapdragon Compute Platform（8cx Gen 3），32 GB，Qualcomm Adreno 8cx Gen 3（驱动 30.0.4122.4000），Vulkan loader 1.3.301，只有 Mesa Dozen 设备；Go 1.27.3、Microsoft OpenJDK 21.0.13 ARM64、clang-cl 22.1.3。

| 检查 | 结果 |
|---|---|
| Go 模块 | `go vet ./...` 与 `go test ./...` 通过 |
| 文档镜像 | `go run ./tools/checkdocs` 通过 |
| 环境要求 | `scripts/windows/check-requirements.ps1`：每个锁定的工具均为 OK，包括 Android SDK（build-tools 36.0.0、android-36）和 NDK r30 |
| 平台资源 | `build.ps1 -Component FrameworkRes`：由 AOSP `android-16.0.0_r2`（API 36）构建的 `framework-res.apk`，66.4 MB，sha256 `62eb212e…6d90`；它的 14,530 个公开资源 ID 与 android.jar（android-36）一致，一个资源（`string/user_switched`）因没有默认值被 aapt2 去掉 |
| 夹具 | `build.ps1 -Component Fixtures`：`hello-java.apk` 和 `hello-jni.apk` 已签名，apksigner 校验通过；`libhello.so` 为 ELF64 AArch64（NDK r30）；哈希在 `tests/fixtures/*.json` |
| G1 Java 路径 | `test.ps1 -Suite Java`：24 项检查 PASS（见下）；`winapk.exe`、`winapk-bridge.dll` 和 `zd.dll` 为 ARM64；窗口在 100% 缩放下（density 160） |
| Java 侧（Mac.apk `ff2afbf`） | `build.ps1 -Component Java`：不经修改即可编译，78 秒，没有错误（[port-audit.md](port-audit.md)） |
| T01 宿主和 JVM | `test.ps1 -Suite Host`：20 项检查 PASS——`winapk.exe`、`winapk-bridge.dll` 和 zlib 为 ARM64；`jvm.dll` 为 ARM64，在进程内以上游的模块选项创建 JVM；JNI 宿主 → Java → 宿主；Java2D；260 个字体族；补丁模块；以及负例（找不到 JVM、无效的 `--java-home`） |
| Vulkan | ARM64 loader 可用；两个 Mesa Dozen 设备（基于 Direct3D 12 的 Vulkan 1.2）；`doctor --present`：Win32 surface 测试通过。没有厂商 Vulkan 驱动 |
| 界面 | `go tool mygo vet .` 与 `go tool mygo i18n check` 通过；`design/` 的每个界面在 `ui.Tester` 中以两种语言和两种外观渲染；在 `ui.Tester` 中用指针拖动的窗格会拆分目标窗格；原生窗口能打开并保持运行（smoke 运行，示例数据） |

## 关卡

| 关卡 | 状态 |
|---|---|
| G0 宿主就绪 | PASS（2026-11-03，本机：T01，工具链已锁定） |
| G1 Java MVP | 本机 PASS（2026-11-03），G1.6 与其他 DPI 缩放为 NOT_RUN（见下） |
| G2 Native 最小执行 | NOT_RUN |
| G3 Native 集成 | NOT_RUN |
| G4 图形 | NOT_RUN |
| G5 实验版 | NOT_RUN |

## G1 检查

`test.ps1 -Suite Java`，2026-11-03，在上述机器上。

| 项（`PLAN.md` §8） | 结果 | 证据 |
|---|---|---|
| G1.1 真实 APK 的 DEX 启动 Activity | PASS | 冷启动：com.macapk.dex 输出 `stage dex-converted (… ms)`，`stage jvm-created`，`T03: lifecycle=onCreate`、`onResume` |
| G1.2 文字、按钮和输入框；点击、输入、滚动、返回 | PASS | 两次点击得到 `taps=2`；按 H、I 得到 `text=hi`；拖动把列表首行移到 12；Escape 得到 `back=pressed`、`onPause`、`onDestroy` |
| G1.3 尺寸与坐标；不残留进程 | 100% 缩放下 PASS | 加宽到 600×800 px 后布局随之变化，点击新位置得到 `taps=3`；关闭窗口后进程以 0 退出且不再存在。其他 DPI 缩放：NOT_RUN（只有一台 100% 的显示器） |
| G1.4 重启后保留状态，冷缓存与热缓存 | PASS | 第二次运行：`stage dex-converted (cached)`、`restored.prefs=3`、`restored.file=3` |
| G1.5 guest native 关闭时的 APK `.so` | PASS | `hello-jni`：`native.error=GUEST_NATIVE_DISABLED: libhello.so …`，应用继续运行 |
| G1.6 Kotlin 夹具 | NOT_RUN | 尚未构建 T04 |
| T02 呈现器 | PASS | Android 视图就是客户区（400×800 px）；客户区像素等于主题的 `windowBackground` #EFEDF1 |
| 负例 | PASS | 不是 APK 的文件：退出码 65，`INSTALL_FAILED_INVALID_APK`；`--native=restricted`：退出码 64，`GUEST_NATIVE_NOT_IMPLEMENTED` |

## 已知限制

- Mac.apk 的 Windows 改动：`runtime/CMakeLists.txt` 的 `if(WIN32)` 分支，以及 `AppClassLoader.findLibrary` 的
  `winapk.guestNative` 检查；其余都是 `runtime/platform/windows` 下的新代码。Windows 实现了 `NativeCocoa` 和其他宿主
  native 中的哪些、哪些仍未绑定，见 [port-audit.md](port-audit.md)。
- 界面还没有启动 `winapk run`；它仍使用 `winapk.Mock`。
- 这台电脑没有厂商 Vulkan 驱动：Vulkan 是基于 Direct3D 12 的 Mesa Dozen（[decisions.md](decisions.md)）。
- `CMakeLists.txt` 与 `AppClassLoader` 改动后的 macOS 构建和 smoke：NOT_RUN（这里没有 Mac）；该分支只在 `WIN32` 下返回，Java 检查在未设置 `winapk.guestNative` 时不起作用。
- 界面只显示示例数据。在运行时契约提供之前尚未完成：窗格或窗口中的应用画面、移动应用数据文件夹的内容、数据和转换后代码的大小，以及更新私有 Java 运行时（[gui.md](gui.md)）。
- 在运行时于 ARM64 真机上运行之前，不能声明原生 ARM64 支持。
