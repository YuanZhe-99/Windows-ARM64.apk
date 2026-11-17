# 平台说明

| 平台 | 状态 |
|---|---|
| Windows 11 ARM64 | G0 通过；G1 的检查在本机通过（T02、T03）：`winapk run` 在 Win32 窗口中运行纯 Java APK 的真实 DEX，支持触摸、按键和输入的 Unicode 文字、滚动、改变尺寸、最小化与恢复、返回、保存的状态和 SQLite，路径可含 Unicode 字符；Kotlin 夹具 T04 也能运行。G2 同样通过：`native-probe.exe` 加载 N0 客体 ELF 库，在其上运行 ABI 测试集 T05，并拒绝格式错误和超出 profile 的库（T06）；`winapk run` 尚不加载 APK 的库。界面仍对接示例数据（`winapk.Mock`）。 |
| Windows 11 x64 | 不是目标平台；只可承载构建工具和解析测试。 |
| macOS（Apple Silicon） | 上游 Mac.apk 的平台；已引入为 `macapk/`，当前在 `3f35611`；尚未在此运行 macOS 构建或 smoke（`NOT_RUN`）。 |

## Windows

在 Windows 11 ARM64 上验证（2026-11-16）：build 26300.9974，Snapdragon Compute Platform（8cx Gen 3），32 GB，Qualcomm Adreno 8cx Gen 3（驱动 30.0.4122.4000），Vulkan loader 1.3.301，只有 Mesa Dozen 设备；Go 1.27.4、MyGo v0.9.2、Microsoft OpenJDK 21.0.13 ARM64、clang-cl 22.1.3。

| 检查 | 结果 |
|---|---|
| Go 模块 | `go vet ./...`、`go test ./...` 与 `CGO_ENABLED=0 go test ./...` 通过 |
| 文档镜像 | `go run ./tools/checkdocs` 通过 |
| 环境要求 | `scripts/windows/check-requirements.ps1`：每个锁定的工具均为 OK，包括 Android SDK（build-tools 36.0.0、android-36）、NDK r30 和 kotlinc 2.4.0（Android Studio 自带） |
| 平台资源 | `build.ps1 -Component FrameworkRes`：由 AOSP `android-16.0.0_r2`（API 36）构建的 `framework-res.apk`，66.4 MB，sha256 `62eb212e…6d90`；它的 14,530 个公开资源 ID 与 android.jar（android-36）一致，一个资源（`string/user_switched`）因没有默认值被 aapt2 去掉 |
| 夹具 | `build.ps1 -Component Fixtures`：`hello-java.apk`、`hello-jni.apk` 和 `hello-kotlin.apk`（内含转为 DEX 的 kotlin-stdlib 2.4.0）已签名，apksigner 校验通过；`libhello.so` 为 ELF64 AArch64（NDK r30）；哈希在 `tests/fixtures/*.json` |
| G1 Java 路径 | `test.ps1 -Suite Java`：100% 缩放（density 160，2026-11-16，含按下途中失去激活释放鼠标捕获的修复）下 43 项检查 PASS，3840×2160 显示器 200% 缩放（density 320，2026-11-04）下 43 项 PASS；`winapk.exe`、`winapk-bridge.dll`、`sqlite3.dll` 和 `zd.dll` 为 ARM64 |
| Java 侧（Mac.apk `3f35611`） | `build.ps1 -Component Java`：不经修改即可编译，没有错误，并像上游一样把 Android 的 `org.apache.http.legacy` 解包进框架 jar（[port-audit.md](port-audit.md)） |
| 客体 ELF 夹具 | `build.ps1 -Component NativeProbe`：用 NDK r30 的 clang 21 构建 T05 和 T06 的 24 个夹具，哈希在 `tests/fixtures/elf/fixtures.json`（[native-profiles.md](native-profiles.md)） |
| G2 原生路径 | `test.ps1 -Suite Native`：58 项检查 PASS（见下）；`native-probe.exe` 和 `capstone.dll` 为 ARM64 |
| T01 宿主和 JVM | `test.ps1 -Suite Host`：23 项检查 PASS（2026-11-17）——`winapk.exe`、`winapk-bridge.dll`、`native-probe.exe`、`jni-probe.exe`、zlib、SQLite 和 Capstone 为 ARM64；`jvm.dll` 为 ARM64，在进程内以上游的模块选项创建 JVM；JNI 宿主 → Java → 宿主；Java2D；259 个字体族；补丁模块；以及负例（找不到 JVM、无效的 `--java-home`） |
| Vulkan | ARM64 loader 可用；两个 Mesa Dozen 设备（基于 Direct3D 12 的 Vulkan 1.2）；`doctor --present`：Win32 surface 测试通过。没有厂商 Vulkan 驱动 |
| 界面 | `go tool mygo vet .` 与 `go tool mygo i18n check` 通过；`design/` 的每个界面在 `ui.Tester` 中以两种语言和两种外观渲染；在 `ui.Tester` 中用指针拖动的窗格会拆分目标窗格（已适配 MyGo v0.9.2）；原生窗口能打开并保持运行（smoke 运行，示例数据） |

## 关卡

| 关卡 | 状态 |
|---|---|
| G0 宿主就绪 | PASS（2026-11-03，2026-11-16 在本机复验：T01，工具链已锁定） |
| G1 Java MVP | 本机 PASS（2026-11-04 200% 缩放，2026-11-16 100% 缩放复验），含 G1.6；应用运行中改变缩放为 NOT_RUN（见下） |
| G2 Native 最小执行 | 本机 PASS（2026-11-04，2026-11-16 复验）：`native-probe.exe` 中的 profile N0；`winapk run` 中客体原生仍关闭 |
| G3 Native 集成 | NOT_RUN |
| G4 图形 | NOT_RUN |
| G5 实验版 | NOT_RUN |

## G1 检查

`test.ps1 -Suite Java`，2026-11-04（200% 缩放）与 2026-11-16（100% 缩放），在上述机器上。

| 项（`PLAN.md` §8） | 结果 | 证据 |
|---|---|---|
| G1.1 真实 APK 的 DEX 启动 Activity | PASS | 冷启动：com.macapk.dex 输出 `stage dex-converted (… ms)`，`stage jvm-created`，`T03: lifecycle=onCreate`、`onResume`；APK 与数据文件夹的路径含空格及匈牙利文、韩文和中文字符时同样通过 |
| G1.2 文字、按钮和输入框；点击、输入、滚动、返回 | PASS | 两次点击得到 `taps=2`；按 H、I 得到 `text=hi`，之后以 WM_CHAR 发送的 é、中文和一个代理对（emoji）接在后面；拖动把列表首行移到 12；Escape 得到 `back=pressed`、`onPause`、`onDestroy` |
| G1.3 尺寸与坐标；不残留进程 | 100% 与 200% 缩放下 PASS | 加宽到 600×800 px 后布局随之变化，点击新位置得到 `taps=3`；按下途中窗口失去激活会取消触摸（不产生点击）；在任务根部返回会最小化窗口，恢复窗口会重新启动 Activity；最小化的窗口记录 2 Hz 节流，恢复后回到全速；UI 线程被挂起时，应用循环发出的窗口调用记录 `UI_THREAD_BLOCKED: moveTaskToBack`，随后以 `UI_CALL_TIMEOUT` 放弃，应用继续运行；关闭窗口后进程以 0 退出且不再存在。200%（4K）下：`scale 2.000000, density 320 dpi`，400×800 点即 800×1600 px 的客户区，Android 视图将其填满，48 dp 的按钮高 96 px；截图显示了该缩放下的布局。缩放取自显示器，而不是 `GetDpiForSystem`——后者在用户注销之前一直保持登录时的缩放 |
| G1.4 重启后保留状态，冷缓存与热缓存 | PASS | 第二次运行：`stage dex-converted (cached)`、`restored.prefs=3`、`restored.file=3`，`databases/t03.db` 保留了三次启动的行 |
| G1.5 guest native 关闭时的 APK `.so` | PASS | `hello-jni`：`native.error=GUEST_NATIVE_DISABLED: libhello.so …`，应用继续运行 |
| G1.6 Kotlin 夹具 | PASS | T04（`hello-kotlin`，kotlin-stdlib 2.4.0）：`result=top=the:3,brown:1,dog:1 squares=140 copy=Count(word=fox, n=2)`、`onResume`、退出码 0。仅限此夹具：不推定其他 Kotlin 或 Compose 应用兼容 |
| SQLite（T03） | PASS | `upper('café 中文')` 为 `CAFÉ 中文`，`LIKE 'CAFÉ%'` 匹配，`COLLATE LOCALIZED` 排序为 `apple|Banana|café 中文`；查询不存在的表抛出 `SQLiteException` |
| T02 呈现器 | PASS | Android 视图就是客户区（400×800 px）；客户区像素等于主题的 `windowBackground` #EFEDF1 |
| 负例 | PASS | 不是 APK 的文件：退出码 65，`INSTALL_FAILED_INVALID_APK`；`--native=restricted`：退出码 64，`GUEST_NATIVE_NOT_IMPLEMENTED`；正常运行不记录 `UI_THREAD_BLOCKED` |

## G2 检查

`test.ps1 -Suite Native`，2026-11-16，在上述机器上。

| 条目（`PLAN.md` §9） | 结果 | 证据 |
|---|---|---|
| N0 正例返回精确值，包括双向回调、浮点和结构体 | PASS | T05：四个构建（带 RELR 的 `libn0abi.so`、`-rela`、`-sysv`、`-4k`）上各 27 个用例：整数、客体 `long`、高位带垃圾值的窄参数、指针、浮点、经栈传递的 20 个参数、HFA、经寄存器/引用/`x8` 传递的结构体、客体可变参数函数、RELATIVE/RELR、ABS64、GLOB_DAT 和 JUMP_SLOT 数据、BSS、宿主导入、客体调用的宿主函数指针、五层客体 → 宿主 → 客体重入、被调用方保存的寄存器（以及能发现被破坏寄存器的检查）、sp 对齐；每次回调中 x18 都是 TEB |
| 负例在任何入口或构造函数之前被拒绝，并给出模块、偏移和原因 | PASS | T06：20 个文件（13 个构建所得，7 个修补一个字段派生），每个都以预期的类别和原因输出 `reject CATEGORY file where: reason`，退出码 2，未运行任何代码（[native-profiles.md](native-profiles.md)） |
| 循环加载/执行无崩溃、无资源增长；地址随机化下可重复 | PASS | 2000 次加载/运行/卸载循环，每次都附带一次被拒绝的加载：两半中堆分配均 +0 字节，私有区域 +0，句柄 +0；同时加载 32 份副本，位于 32 个不同基址，全部通过；两个进程位于两个基址，全部通过 |
| 在真实 ARM64 上 | PASS | 上述机器；`native-probe.exe` 和 `capstone.dll` 为 ARM64 |
| `abi-compatibility.md` | PASS | [abi-compatibility.md](abi-compatibility.md)：直通、需 thunk 和不支持的签名及其用例 |

## P3 Bionic 与模块检查

`test.ps1 -Suite Bionic`，2026-11-17，在上述 Windows ARM64 机器上 104/104 项检查 PASS（T08：75，T07：29），不是 G3；
线程夹具用 NDK r30、API 30、`-O2 -nostdlib -femulated-tls -fno-stack-protector` 构建；
独立同步夹具使用 `-O2 -nostdlib -fno-stack-protector`，没有 TLS。
哈希与完整选项在 `tests/fixtures/elf/fixtures.json`（生成产物，不提交）；
输出在 `build/windows-arm64/test-data/logs/T08-*`。
线程探测包含九个用例，同步探测包含八个；拒绝/终止文件不证明 APK 原生库或 JNI 集成已可用。
T07 依赖图使用六个 `-O2 -nostdlib` 库；`-Suite Modules` 独立通过十项语义、六项哈希及汇总检查。
其输出在 `build/windows-arm64/test-data/logs/T07-modules.*`。
T08 内存用同一源码的三个 `-O2 -nostdlib -fno-builtin` 构建新增 11 项检查；
正例进程含四项语义及一次宿主状态检查，不另加压力循环。
T08 io 用一个 `-O2 -nostdlib -fno-builtin` 构建新增八项检查，含四项语义、
宿主状态、哈希/汇总及一次宿主侧映射/哨兵检查。
T08 faults 用同一源码的三个构建新增八项检查：宿主 SEH 链正常处理，
客体访问违规与 trap 进程带模块/虚拟地址诊断终止。
T08 format 新增九项检查：辅助库/夹具哈希、四项语义及宿主状态、汇总和精确日志字节。
产品辅助库是 NDK 构建、经过筛查的 AArch64 ELF，不是 PE DLL。
T07 N1 新增十二项检查：四项夹具哈希、辅助库哈希、六项语义及汇总/日志字节。
正例源码使用 NDK 驱动的普通 API-30 共享库链接及 CRT 启动文件、libc/libdl/liblog，
不使用 `-nostdlib` 或特殊 TLS/栈保护参数。

| 条目（`PLAN.md` §10） | 结果 | 证据 |
|---|---|---|
| 客体 pthread/errno/key 隔离 | PASS | 8 与 32 个同时存在的 worker；标识、errno/TLS 地址和值独立；join 返回精确指针；主上下文不变 |
| TLS 初始值、对齐与析构 | PASS | 初始值 41、零初始化、64 字节对齐；析构回调前 key 值已清空；重新设置 key 后回调两轮或受四轮上限约束；每次回调均可使用仿真 TLS |
| 明确错误与字节 shim | PASS | 已删除/无效 key → `EINVAL`，第 129 个存活 key → `EAGAIN`，非空线程属性 → `ENOTSUP`，自身 join → `EDEADLK`，未知/已 join 线程 → `ESRCH`；客体 errno 不变；`memset` 返回值/范围/零长度/字节转换 |
| 线程资源清理 | PASS | 预热后 200 轮 / 1600 个 worker：句柄增量 +0；每次调用后线程/上下文/key/仿真 TLS 注册表均清空；5413 次宿主状态回调，没有 x18/sp/errno 违规 |
| Mutex 类型、布局与生命周期 | PASS | 40 字节、四字节对齐存储带哨兵；normal、recursive（2048 上限）、error-checking；八 worker × 256 次递增 = 2048；诊断复制/过期/已销毁对象和非所有者；显式重新初始化；errno 不变 |
| 条件等待/signal/broadcast | PASS | 48 字节存储带哨兵；256 次请求/应答 signal 握手；八等待者 broadcast；每次返回均持有 mutex；拒绝销毁忙碌 condition/mutex，包括被条件等待引用的未锁定 mutex；拒绝第二个 mutex 绑定和递归深度 >1 |
| 同步资源清理与拒绝 | PASS | 200 轮 / 1600 个 worker，句柄增量 +0，同步注册表清空；1916 次宿主状态回调无违规；超时等待导入在入口前被拒绝，仍持有 mutex 时销毁运行时带诊断退出 3 |
| 范围外原生行为 | PASS | 缺失的 `pthread_detach` 导入在入口前拒绝；使用 P3 导入表时 ELF PT_TLS、裸 `svc`、TPIDR_EL0 和 x18 仍被拒绝；损坏的 emutls 描述符与仍有未 join worker 时销毁运行时均带诊断终止，退出码 3 |
| 模块事务与查询（T07） | PASS | 依赖优先 trace `12`、根库结果 `49`、自身定义、未解析弱导入、广度优先依赖导出查询、重复打开/引用计数及关闭后重新打开而不重复初始化 |
| 事务拒绝与回调（T07） | PASS | 缺失导入和无效初始化入口在任何构造执行前回滚全部新映射；拒绝依赖环、基名、SONAME 和缺失文件；既有依赖图不变；同线程构造重入及其他线程竞争时的拒绝，不跨回调持有注册表锁 |
| 客体 dl API（T07） | PASS | `dlopen` NOW/LOCAL、`dlsym`、`dlclose`、`dlerror` 读取清除；无效标志、缺失导出及已关闭句柄明确失败；运行时上下文清理 |
| 内存/字节/字符串子集（T08） | PASS | 零大小分配、16 字节对齐、calloc 清零/溢出、realloc 增长/缩小/失败及内容/errno 保留；重叠/范围/无符号字节操作、UTF-8/终止/填充/查询；八线程归属移交；无效和重复释放在调用宿主分配器前退出 3；分配表清空；12 次宿主状态回调无违规 |
| 文件与 errno（T08 io） | PASS | Unicode/空格根目录与文件名；字节 `410042FF5A`、EOF 及零长度 I/O；seek 超过 8 GiB 后返回而不增大文件；128 字节 stat 带哨兵；create/exclusive/append/只读/只写/fortified-open 行为；转换后的 ENOENT/EEXIST/EINVAL/EBADF；拒绝不支持的标志/模式及无效 UTF-8/设备/逃逸/数据流/junction 路径；外部哨兵未变，fd 表清空 |
| 时间（T08 io） | PASS | realtime Unix epoch 与宿主相差两秒以内，100 ns 转为规范纳秒单位；monotonic 间隔不少于请求的 2 ms；零时长 sleep、成功时 remainder 不变、无效时间和不支持的时钟/时长；四次宿主状态回调无违规。挂起与客体信号 NOT_RUN/不支持 |
| 客体 PC 故障边界（T08 faults） | PASS | 宿主发起的访问违规到达宿主 SEH 处理器，客体 errno/x18/sp 不变；`neg-guest-fault.so` 虚拟地址 `0x42d0` 的访问违规（Windows `0xc0000005`）与 `neg-guest-trap.so` 虚拟地址 `0x42b4` 的 trap（`0xc000001d`）均输出 `GUEST_EXECUTION_FAULT` 后退出 3；不恢复/展开；N0 的 2000 轮循环仍保持堆分配/私有区域/句柄增量 +0 |
| 受限格式化/日志（T08 format） | PASS | LP64 最小/最大值及溢出到栈的整数参数；标志/宽度/精度、客体 va_copy 与 V 变体；本应写入长度、保留哨兵的截断及内嵌 NUL；带 errno 拒绝 `%n`、浮点/宽文本/位置格式及超限；经固定宿主日志入口输出精确的字面百分号/UTF-8 记录；四次宿主状态回调无违规。不支持浮点变参和通用 stdio |
| NDK 系统库准入（T07 N1） | PASS | 普通 NDK 链接执行构造函数并返回精确 `N1:4294967297` 结果；`LIBC` 需求仅绑定具名逻辑提供方；libc/log 查询隔离；fork、未知版本和损坏链在构造前失败，不保留映像；原有 N0 拒绝不变 |
| 客体 CRT 生命周期（T07 N1） | PASS | 逆序/DSO 选择/重入顺序 `3412`，回调不重复；128 份 atfork 注册、溢出及 DSO 移除，不调用 hook；空退出回调与 256 项容量错误，保留一项时精确执行另外 255 个选定回调；关闭保持代码映射；最终回调具有新 errno、分配/释放客体堆、拒绝新 worker、保持 x18/sp，构造/退出 trace 为 `19` |
| APK 原生库与 JNI 集成 | NOT_RUN | 独立 T09 JNI 基础在下文通过；`winapk run` 的原生库加载仍关闭 |

同一变更集还运行了 `check-requirements.ps1`、`build.ps1 -Component All`、
`test.ps1 -Suite Host`（22 项 PASS）、`-Suite Java`（43 项 PASS）与 `-Suite Native`（58 项 PASS）。
Go vet/测试（含 `CGO_ENABLED=0`）、MyGo vet/i18n 与文档镜像检查均通过。
格式化增量的完整输出保存在 `build/windows-arm64/test-data/logs/P3-format-*.log`。
2026-11-17 的 N1 增量运行 requirements/build All 及 Host 22、Native 58、Bionic 104、Java 43；
日志在同目录的 `P3-n1-*.log`。最终另跑 Bionic 104，覆盖加强后的退出上限/堆断言。
这些 N1 C 库结果不包含 JNI/Java native 调用。
构建的宿主 PE/DLL 全为 ARM64；构建脚本检查正例 AArch64 客体夹具的架构。
不宣称完成新关卡，不升级版本，无计划偏离。

## P4 JNI 基础检查

2026-11-17，在上述 ARM64 机器及 Microsoft JVM `21.0.13.0.2+1-LTS` 上：
`build.ps1 -Component All`，然后 `test.ps1 -Suite Jni`：13/13 PASS，启用 `-Xcheck:jni`，没有警告；
最终另跑 `-Xbatch`，在 `T09-compilation.xml` 中验证已安装的 Java 隐式故障方法编译。
一个 NDK 构建的夹具、一个产品客体辅助库、六组语义及四个短终止进程；不另加压力循环。
最终 13 项套件在本机耗时 1.47 秒（单次测量，不是性能保证）。这不是 G3。

| 条目 | 结果 | 证据 |
|---|---|---|
| 客体 → Java 参数/结果 | PASS | .../V/A 构造、静态、实例及非虚调用；混合 25 参数签名溢出整数/浮点寄存器；窄值、64 位 long、提升的 float/double、对象及原生类型数组描述符；同一 va_list 重复 V；虚调用重写相差 100；全部标量结果类别及对象/void 调用 |
| 引用与异常 | PASS | UTF-16 内嵌 NUL 以 modified UTF-8 往返；数组 element commit/release 及 region 数值；局部 frame 提升、全局/弱/局部引用类别；真实 pending IllegalArgumentException、缺失方法、无效描述符/ID/类别及缺失类 NoClassDefFoundError；配置的隔离 loader 及 FindClass 初始化 |
| JVM worker 生命周期 | PASS | detached GetEnv 及版本拒绝；普通/daemon Attach、重复 Attach 身份、显式 Detach 或宿主线程退出自动 Detach，后者晚于仍可调用 JNI 的 Bionic key 析构；不 detach 借用的主线程；join 后精确保留一个主代理 |
| 持久宿主线程绑定 | PASS | 客体 pthread 身份、key 值、emutls 地址/值在返回宿主后保留；宿主控制的嵌套客体入口改变同一 errno/TLS；最后绑定的 key 析构仍可调用 JNI 并重入该绑定，然后释放 TLS 并使运行时 idle；不证明 Java native 入口 thunk |
| 故障及误用边界 | PASS | 不支持的槽 217 及跨线程 JNIEnv：带明确类别退出 3；客体 trap/访问违规：已注册模块/偏移及退出 3；热 Java 空指针/除零异常仍由 JVM 所有并正常完成；主线程/worker 只读检查 x18/sp |
| Java → guest native 绑定及 APK 集成 | NOT_RUN | 尚无 RegisterNatives/mangled-name 绑定或 JNI_OnLoad；winapk run 原生准入仍关闭 |

回归同时通过 requirements、Host 23、Native 58、Bionic 104、Java 43；
Go 格式/vet/测试（含禁用 CGO）、MyGo vet/i18n 及文档镜像通过。
日志在 `build/windows-arm64/test-data/logs/P4-jni-*.log` 和 `T09-*`。
后续绑定增量的完整回归在 `P4-context-*.log`；只向既有正例进程添加持久性及销毁语义检查，不另加循环。
首次混合 trap 实验失败：JVM 在末尾客体处理器运行前退出 1。
修复安装严格限定客体 PC 的首位处理器，绝不替换 HotSpot 处理器，
每个非客体 PC 仍走原有链。保留的 T09 用例保护这条边界。
没有新关卡、版本升级或计划偏离；启用 CFG 和 macOS 的运行仍为 NOT_RUN。

## 已知限制

- Mac.apk 的 Windows 改动：`runtime/CMakeLists.txt` 的 `if(WIN32)` 分支、`AppClassLoader.findLibrary` 的
  `winapk.guestNative` 检查，以及 `runtime/src/native_sqlite.cpp` 的 `_WIN32` Unicode 辅助函数；其余都是 `runtime/platform/windows` 和 `runtime/guest` 下的新代码。Windows 实现了 `NativeCocoa` 和其他宿主
  native 中的哪些、哪些仍未绑定，见 [port-audit.md](port-audit.md)。
- 界面还没有启动 `winapk run`；它仍使用 `winapk.Mock`。
- 客体原生：profile N0、`native-probe.exe` 中的独立 P3 T07/T08 子集，以及 `jni-probe.exe` 中的 P4 JNI 基础；`winapk run` 不加载任何 APK 库（P4）。客体 PC 故障边界诊断识别出的客体执行故障，不处理宿主导入、JIT/JVM 故障、范围外分支目标或栈溢出。T09 覆盖已声明的混合 JVM 边界，不证明任意 JVM/客体失败。以 `winapk.exe` 的编译选项构建，关闭 Control Flow Guard；启用 CFG 的构建为 NOT_RUN（[abi-compatibility.md](abi-compatibility.md)）。
- 这台电脑没有厂商 Vulkan 驱动：Vulkan 是基于 Direct3D 12 的 Mesa Dozen（[decisions.md](decisions.md)）。
- `CMakeLists.txt`、`AppClassLoader` 与 `native_sqlite.cpp` 改动后的 macOS 构建和 smoke：NOT_RUN（这里没有 Mac）；该分支只在 `WIN32` 下返回，Java 检查在未设置 `winapk.guestNative` 时不起作用，`_WIN32` 之外的 CoreFoundation 辅助函数没有改变。
- DPI：应用的 density 在启动时固定。把窗口移到另一缩放的显示器，或在运行中改变缩放，窗口会调整尺寸以保持其点数（`WM_DPICHANGED`），但应用保持最初的 density：NOT_RUN，也尚未作为 Android 的配置变更处理。
- 输入范围（`PLAN.md` §8.4）：一个指针（鼠标；没有多点触控或捏合），映射的按键，以及经 WM_CHAR 到达的任何字符。输入法的组字在输入过程中不显示在输入框里，只在提交时落入，与 macOS 相同；候选窗口在 Windows 默认的位置。未运行真实的输入法会话（测试以输入法提交时的方式发送 WM_CHAR）。
- 界面只显示示例数据。在运行时契约提供之前尚未完成：窗格或窗口中的应用画面、移动应用数据文件夹的内容、数据和转换后代码的大小，以及更新私有 Java 运行时（[gui.md](gui.md)）。
- 在运行时于 ARM64 真机上运行之前，不能声明原生 ARM64 支持。
