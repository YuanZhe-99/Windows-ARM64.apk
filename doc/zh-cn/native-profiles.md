# 原生执行 profile

客体原生代码默认关闭，只在已声明、已测试的 profile 内运行（`PLAN.md` §5.2）。加载器是
`macapk/runtime/guest/loader/elf_loader.cpp`；目前只有 `native-probe.exe` 使用它。`winapk run --native=restricted`
仍然返回 `GUEST_NATIVE_NOT_IMPLEMENTED`，APK 中的库不会被加载（P4）。

| Profile | 状态 |
|---|---|
| N0，可审计的 leaf ELF | 已实现；G2 在本机通过（[platform-notes.md](platform-notes.md)） |
| N1，受限的 C/JNI ELF（依赖、导入、构造函数、JNI） | P3 线程/同步基础（T08）和模块事务（T07）；其余 Bionic 功能组与 JNI 未完成，G3 NOT_RUN |
| N2，图形样例 | 未开始（P5、P6） |

## N0

由经审计的源码、以已知参数构建的共享库：无依赖、无 libc、无 TLS、无构造与析构函数、无 IFUNC、无系统调用、不使用
`x18` 或 `TPIDR_EL0`。它的导出按 [abi-compatibility.md](abi-compatibility.md) 的签名调用；它的导入只绑定到调用方
指定的宿主符号。

模块按以下顺序加载；其代码一行都不运行，任一拒绝都会释放全部资源：

1. 读取文件并检查 ELF 头：ELF64、小端、版本 1、AArch64、ET_DYN。
2. 对照文件检查 program header：每个偏移和长度、`p_filesz` ≤ `p_memsz`、对齐及其同余关系、PT_LOAD 有序且不重叠、
   只有一个 PT_DYNAMIC。从不读取 section header。
3. 页：宿主页大小（此处为 4 KiB）和分配粒度取自 `GetSystemInfo`。只有权限相同的段才能共用一个宿主页；为整个范围
   保留一段地址（`VirtualAlloc`，由系统选择地址），每个段以读写方式提交并复制；BSS 即新页的零值。
4. PT_DYNAMIC：先读字符串表，再逐个检查标签；未知标签被拒绝。
5. 符号：符号数取自 DT_GNU_HASH 或 DT_HASH，每个已定义符号都必须位于映像之内。
6. 对每个可执行字进行筛查（见下）。
7. 重定位分两遍：第一遍检查每个类型、目标和符号，并列出所有缺失的导入；第二遍写入。导入立即绑定，没有延迟绑定。
8. 为每个段设定最终保护（R、RX、RW；绝无 W+X），把 PT_GNU_RELRO 覆盖的完整页设为只读，然后执行
   `FlushInstructionCache`。

| 重定位或表 | N0 |
|---|---|
| `R_AARCH64_RELATIVE`、`ABS64`、`GLOB_DAT`、`JUMP_SLOT` | 应用 |
| RELR（`DT_RELR` 与 `DT_ANDROID_RELR`，NDK r30 自 API 28 起的默认） | 应用 |
| `R_AARCH64_NONE` | 跳过 |
| TLS（`TLS_DTPMOD64`、`TLS_DTPREL64`、`TLS_TPREL64`、`TLSDESC`） | 拒绝 |
| `R_AARCH64_IRELATIVE` 与 IFUNC 符号 | 拒绝：加载时会运行解析函数 |
| `R_AARCH64_COPY` | 拒绝：仅用于可执行文件 |
| Android 压缩重定位（`DT_ANDROID_RELA`，APS2） | 拒绝：尚未实现 |
| REL 表（`DT_REL`、`DT_ANDROID_REL`） | 拒绝：AArch64 使用 RELA |
| 其他任何类型 | 拒绝，并给出其编号 |
| DT_GNU_HASH、DT_HASH | 两者都可读取；两者都有时优先 GNU |
| 符号版本 | 拒绝 `DT_VERNEED`（带版本的导入需要库）；不检查 `DT_VERSYM` 和 `DT_VERDEF` |

宿主未提供的未定义弱导入绑定为 0，与 Android 一致。模块自己定义的符号绑定到模块自身的定义，绝不绑定到同名的宿主
符号。

## P3 线程基础（T08）

这是独立探测，不是 APK 的 N1 准入。`native-probe threads` 将 `ThreadRuntime::Imports()` 加仅供测试的宿主状态回调绑定给经审计、
无依赖的 ELF，沿用未改变的 N0 加载检查。其 `-nostdlib -femulated-tls -fno-stack-protector` 构建为特制夹具，
不能证明常规 NDK 产物可运行。N0 原有 ABI 测试与导入保持不变。

- `__errno` 返回每个受管理线程上下文中独立的 32 位客体 errno，绝非 UCRT errno 或 `GetLastError`。
  pthread 函数返回 Linux 错误码，不修改它。
- 编译器需要的 `memset` 导入有显式固定签名字节包装（64 位大小、32 位填充值）；T08 检查范围、返回指针、
  零长度和字节转换。
- `pthread_create` 只接受空属性指针（可 join、默认宿主栈）。`pthread_join`、`pthread_self`、
  `pthread_equal`、`pthread_key_create`、`pthread_key_delete`、`pthread_getspecific` 和
  `pthread_setspecific` 使用显式 Android LP64 类型：`pthread_t` 为 64 位，`pthread_key_t` 为有符号 32 位。
  标识不是 Windows HANDLE 或 CRT 对象。非空属性返回 `ENOTSUP`；设置或删除无效/已删除 key 返回 `EINVAL`。
- 每个宿主 worker 先进入新的客体上下文，再调用固定签名的客体函数。最多同时分配 128 个 key，不复用旧标识；
  析构回调前清空值，最多执行四轮析构。删除 key 不运行析构函数。仿真 TLS 在所有 key 析构期间保持有效，之后释放。
- `__emutls_get_address` 读取 LLVM compiler-rt 的 AArch64 描述符布局（四个 64 位字段：大小、对齐、
  object、初始值）。每个上下文按描述符地址拥有独立初始化、正确对齐的存储，不修改描述符。
  目前只覆盖 C `__thread` 数据；C++ TLS 析构和客体 ELF TLS（GD/LD、IE/LE、TLSDESC、PT_TLS）仍不支持。
  resolver 必须是绑定到本 shim 的未定义导入，不能是客体库内嵌的另一份 compiler-rt。
- 调用方必须保持所有模块映射，直到上下文与回调结束。运行时销毁时仍有未 join 的 worker、从未管理线程调用有状态 shim、
  emutls 描述符损坏或编译器 TLS resolver 分配失败时，探测进程带诊断终止，绝不伪造 TLS 地址。
  不允许异常经客体代码展开。下述客体 PC 故障边界终止执行，绝不尝试恢复。
  `pthread_exit`、detach/取消、格式化日志 shim 和完整原生 profile 集成仍是 P3 后续工作；JNI 属于 P4。

P3 同步子集通过独立的 `native-probe sync`（T08）测试：

- Android LP64 mutex/condition 存储为 40/48 字节，按 4 字节对齐。shim 只在其中写入私有代际标记；
  所有者、递归次数、等待者与宿主条件变量均保留在运行时的地址表中。将存活标记复制到另一个地址、使用旧代际或已销毁
  对象返回 `EINVAL`，绝不跨 ABI 传递宿主锁布局。
- `pthread_mutex_init`（空属性）、`lock`、`trylock`、`unlock`、`destroy` 支持默认 normal mutex。
  识别 NDK 的 normal、recursive、error-checking 静态初始化值；递归所有权计数，error-checking 重锁返回
  `EDEADLK`，递归次数限制为 Bionic 的 2048 次获取，忙碌的 trylock 返回 `EBUSY`。
  normal mutex 的阻塞式自身重锁仍会死锁。
  非所有者 unlock 返回 `EPERM`；销毁已持有、有竞争等待者或绑定条件等待的 mutex 返回 `EBUSY`。
  销毁后必须显式重新初始化。
- `pthread_cond_init`（空属性）、`wait`、`signal`、`broadcast`、`destroy` 支持全零静态初始化值和进程私有、
  无超时等待。wait 在运行时注册表锁保护下原子释放逻辑 mutex 所有权并注册条件等待，返回前重新取得 mutex。
  允许伪唤醒，调用方须使用谓词循环。同时存在的等待者必须使用同一个 mutex，否则返回 `EINVAL`。
  有等待者时销毁返回 `EBUSY`；未持有 mutex 时等待返回 `EPERM`。
- 非空 mutex/condition 属性和 monotonic 条件变量静态初始化值返回 `ENOTSUP`；超时等待/加锁、属性 API、
  进程共享、robust/PI mutex 与 futex 系统调用尚未实现。递归深度大于一时条件等待返回 `ENOTSUP`，
  不释放任何所有权。延迟分配可能返回 Linux `ENOMEM`。这些 pthread 调用均不修改客体 errno。
- 等待者引用阻止销毁仍被阻塞加锁或条件等待使用的 mutex。测试在运行时 idle 检查前销毁对象；
  运行时销毁不得释放仍被持有或等待的同步对象。

T08 检查损坏描述符与仍有未 join worker 时销毁运行时导致的终止、8 与 32 个同时存在的客体线程的隔离、精确 join
指针、主线程状态、初始值/零值/对齐 TLS、析构重设值与轮数
上限、已删除 key、key 耗尽、不支持的属性、无效/自身 join，以及只读 x18/sp 宿主状态检查。

## P3 内存、字节与字符串（T08）

`native-probe memory` 子集使用固定签名包装，不直接查找 UCRT 导出。
经审计的导入为 `malloc`、`calloc`、`realloc`、`free`；`memcpy`、`memmove`、`memset`、
`memcmp`、`memchr`；`strlen`、`strnlen`、`strcmp`、`strncmp`、`strcpy`、`strncpy`、`strchr`、
`strrchr`、`strstr`、`strdup`。大小及长度返回值显式使用客体无符号 64 位类型。

- 每个运行时拥有私有 Windows 堆及由其受管理线程共享的分配归属表。字节可经已验证指针签名跨域，
  分配器或 CRT 对象不可跨域。分配至少按 16 字节对齐；未释放分配在运行时销毁时释放。
- 零大小 malloc/calloc 返回可释放的最小分配。空指针 realloc 等同 malloc；
  非空指针、零大小 realloc 释放原分配并返回空。realloc 失败后原字节的归属与内容不变。
- 大小超过 `PTRDIFF_MAX`、乘法溢出及分配失败均返回空并设置客体 `ENOMEM`；
  成功分配和 free 保持客体 errno。无效、内部、宿主所有或已释放的分配指针带客体 ABI 诊断终止。
- 字节/字符串函数保持客体 errno，比较无符号字节，不转换区域设置或编码。UTF-8 原样复制。
  普通 C 缓冲区/终止要求仍适用；这不是内存安全沙箱。`strdup` 使用同一私有堆。

T08 内存把分配/溢出/重新分配、字节重叠/范围、字符串/UTF-8 和跨线程分配/释放合并在一个小正例夹具中；
无效释放与重复释放使用同一源码、在独立终止探测中检查。不据此宣称支持 stdio、格式化变参或日志 API。

## P3 文件与时间（T08）

独立 `native-probe io` 子集由调用方传入宿主 Android 根目录；
不改变进程工作目录，也不暴露宿主描述符。APK 集成仍属于 P4。

- 客体绝对路径 `/data/...` 与 `/sdcard/...` 映射到该根目录下。拒绝相对/Windows 路径、
  路径逃逸、保留设备名、备用数据流、损坏的 UTF-8、结尾点/空格及重解析点路径组件。
  目录由宿主准备，open 不创建目录。宿主不得并发替换目录树。
  这是受限路径映射，不是抗竞态隔离、安全沙箱或 POSIX 文件系统。
- `open`/`open64` 接受只读、只写或读写，及 `O_CREAT`、`O_EXCL`、`O_TRUNC`、
  `O_APPEND`、`O_CLOEXEC`、`O_LARGEFILE`。创建模式仅支持 `0600`；
  不支持的模式、标志和目录/设备打开明确失败。可选整数模式仅在创建时通过固定三寄存器签名读取，绝不共享 va_list。
- `read`、`write`、`close`、`lseek`/`lseek64`、`fstat`/`fstat64` 使用运行时所有、从 3 开始的 fd 表，
  非继承 Windows 句柄保持私有。未实现标准流、dup、管道、套接字、目录枚举、权限或任意 Linux 文件语义。
  操作按运行时串行；读写可能返回短计数。追加写入使用文件当前末尾，不受 seek 位置影响。
- `fstat` 写入显式的 128 字节 Android AArch64 布局：宿主文件标识、大小、链接数、分配块数和访问/写入/更改时间；
  常规文件模式及合成的客体所有者权限，uid/gid 为零，首选 I/O 大小 4096 字节。绝不暴露宿主 stat 对象。
  API 错误返回 -1 及转换后的 Linux errno；成功操作保持客体 errno。
- `clock_gettime` 支持 realtime（Unix epoch，精确 Windows 墙钟）与 monotonic
  （精确 unbiased interrupt time，不计挂起）。Timespec 为两个有符号 64 位字段。
  其他时钟失败；`nanosleep` 支持最多 60 秒的相对时长，向上取整至 100 ns 并等待到 monotonic 期限。
  不投递客体信号/取消，不伪造 EINTR，成功时不改动 remainder。

T08 io 合并读写/seek/stat、open/追加标志、映射/错误拒绝与时钟/sleep 检查。
宿主侧检查确认 Unicode 映射文件，以及拒绝 junction 路径时没有截断映射根目录之外的文件。

## P3 模块事务（T07）

`ModuleSet` 为经审计的 C 库依赖图扩展加载器；原有 N0 `Load` API 仍拒绝依赖与构造函数。
这不开放 APK 原生准入，也不表示 G3 完成。

- 名称限于指定库目录下可移植的 `.so` 基名，绝不使用搜索路径、Windows DLL 名称或任意宿主导出。
  声明的 SONAME 必须等于请求名称。本子集明确拒绝依赖环、带版本的符号及客体 ELF TLS。
- 准备所有新映像和依赖，筛查指令、立即解析导入、应用重定位及保护，最后验证全部初始化入口，
  然后才运行任何客体代码。失败会回滚所有新映像，已提交映像保持不变。
- 定义绑定到自身映像；未定义导入先按广度优先搜索直接依赖，再搜索显式审计的宿主导入。
  缺失的弱导入绑定为零，绝不通过执行客体代码获取 IFUNC 地址。
- 构造函数按依赖优先顺序运行，每个映像仅一次，先 DT_INIT 后 DT_INIT_ARRAY。
  调用方须在打开依赖图前进入受管理客体上下文。允许初始化线程查询和重复打开；
  初始化期间其他线程竞争打开会明确失败，既不暴露半初始化状态，也不跨客体回调阻塞等待。
- 不透明的 64 位句柄维护显式打开引用。Close 减少引用，依赖图保持映射，直到所有客体执行结束后销毁 ModuleSet。
  不调用 finalizer 或 JNI_OnUnload；这是已声明的进程级生命周期策略，并非完整动态卸载。
  初始化不会调用 JNI_OnLoad。
- `ThreadRuntime` 可接收该命名空间，将客体 `dlopen`、`dlsym`、`dlclose`、`dlerror` 绑定到它。
  只支持 RTLD_NOW|RTLD_LOCAL 和显式句柄；lazy/global/default 范围明确失败。
  待取出的错误属于各客体上下文，由 dlerror 读取并清除，与 errno 分离。

T07 使用一对正例依赖库和少量拒绝样例，断言初始化顺序/值、客体 dl API/错误读取清除、查询范围、重复打开/关闭、未解析弱导入、
依赖环/名称拒绝、无效构造入口，以及失败事务绝不运行构造函数。

## P3 客体故障边界（T08）

加载器在任何初始化或导出调用前注册已完成链接/保护的映像，并在取消映射前移除。
末尾优先级的 vectored handler 仅在故障 PC 属于当前注册客体映像的可执行段时，
处理访问违规、非法/特权指令及断点。它写出 `GUEST_EXECUTION_FAULT`、
模块基名、客体虚拟地址与 Windows 异常码，然后以退出码 3 终止进程。
绝不恢复已损坏的客体状态、运行客体清理或让客体 C++ 展开到宿主。

所有其他异常继续走正常 Windows 处理链：宿主/JVM/JIT 代码、宿主导入内部的故障，
以及跳转到注册可执行段以外地址的故障，均不由本边界认领。
它不替换 HotSpot 处理器，不实现信号、栈溢出恢复或 ELF 展开元数据。JVM/客体混合验证属于 P4。
注册表锁绝不跨客体执行或回调持有；故障报告使用有界栈缓冲及直接 Windows 写入，
不分配内存、不回调客体。

T08 faults 使用正常的宿主异常链用例，及隔离进程中的客体访问违规/trap。
既有 N0 压力套件随映射一同检查注册/注销。

## 筛查

检查每个可执行段中的每个 4 字节字：按编码识别裸 `svc`、`hvc`、`smc` 以及任何对 `TPIDR_EL0` 的 `mrs`/`msr`；对
Capstone 能解码的每条指令，检查其操作数和隐式寄存器中是否有 `x18`/`w18`。Capstone 无法解码的字计为数据。筛查是静态
的：它无法证明一个库不会跳入数据、解密代码或在运行时生成代码。N0 的准入依据是经审计的源码和构建参数；筛查拒绝它能
看到的问题（`PLAN.md` §5.2）。

## 拒绝

每次拒绝都在任何客体代码运行之前打印 `reject CATEGORY file where: reason`，其中 `where` 是文件偏移、虚拟地址或符号。

| 类别 | 含义 | T06 夹具 |
|---|---|---|
| `ELF_MALFORMED` | 不是格式正确的 ELF：魔数、大小、偏移、重叠、表位于映像之外 | `neg-bad-magic`、`neg-truncated`、`neg-phdr-range`、`neg-overlap` |
| `ELF_UNSUPPORTED` | 格式正确，但不是本宿主加载的种类：32 位、其他机器、非 ET_DYN、W+X 或只写的段、可执行栈、权限不同的段共用宿主页、文本重定位、未知的 program header 或动态标签 | `neg-arm32`、`neg-x86_64`、`neg-et-exec`、`neg-rwx`、`neg-execstack`、`neg-1k-pages` |
| `ELF_RELOCATION_UNSUPPORTED` | N0 不应用的重定位类型或表 | `neg-ifunc`、`neg-android-packed`、`neg-unknown-reloc` |
| `GUEST_SYMBOL_UNRESOLVED` | 宿主未提供的导入，全部列出 | `neg-missing` |
| `GUEST_ABI_UNSUPPORTED` | 超出 profile：依赖、构造函数、析构函数、TLS、带版本的导入、变体调用约定、IFUNC 符号、`svc`、`TPIDR_EL0`、`x18` | `neg-needed`、`neg-ctor`、`neg-tls`、`neg-svc`、`neg-tpidr`、`neg-x18` |
| `HOST_MEMORY_FAILED` | 保留、提交或保护内存失败 | 无：宿主尚未拒绝过 |

`ELF_MALFORMED`、`ELF_UNSUPPORTED` 和 `HOST_MEMORY_FAILED` 是在 `PLAN.md` §14.2 所列类别之外新增的，其余类别由该节
给出。

## 夹具

`tests/windows-arm64/guest-elf/fixtures.psd1` 列出每个夹具的源码、目标、选项和预期的拒绝；`build.ps1 -Component
NativeProbe` 把它们构建到 `tests/fixtures/elf`（生成产物，从不提交），并把每个哈希记录在该目录的 `fixtures.json` 中。

| 夹具 | 内容 | 构建 |
|---|---|---|
| T05 `libn0abi.so` | ABI 测试集（[abi-compatibility.md](abi-compatibility.md)） | `--target=aarch64-linux-android30 -shared -fPIC -fno-stack-protector -O2 -nostdlib -ffp-contract=off`：不用栈保护（它从 libc 导入），不做浮点收缩（结果精确） |
| T05 `libn0abi-rela.so`、`-sysv.so`、`-4k.so` | 同一源码，分别用 RELATIVE 项代替 RELR、只有 SysV 哈希表、4 KiB 段对齐 | `-Wl,--pack-dyn-relocs=none`、`-Wl,--hash-style=sysv`、`-Wl,-z,max-page-size=4096` |
| T06 构建所得（`negative/negative.c`） | 每个一种缺陷，由 `NEG_*` 定义、目标或链接选项选定 | IFUNC 用例用 `-O0`：在 `-O2` 下 clang 会把其解析函数折叠掉 |
| T06 派生所得 | 修补了一个字段的 `libn0abi.so`（`build.ps1`，`Write-ElfMutation`） | — |
| T08 `libthreads.so` | `threads/threads.c`：受管理 pthread、errno、key/析构、C 仿真 TLS 与编译器需要的 `memset`；九个探测用例必须 PASS | API 30、`-O2 -nostdlib -femulated-tls`，公共选项与 T05 相同 |
| T08 负例文件 | 同一源码：缺失 `pthread_detach`（加载时拒绝）、损坏的仿真 TLS 描述符和仍有未 join worker 时销毁运行时（诊断，退出码 3） | `T08_MISSING`、`T08_BAD_EMUTLS`、`T08_UNJOINED`；完整配方/结果在 `fixtures.psd1` |
| T08 `libsync.so` | `threads/sync.c`：六个 mutex/condition 用例、压力测试与宿主状态检查 | API 30、`-O2 -nostdlib`，公共选项与 T05 相同；断言 NDK 头文件布局 |
| T08 同步负例文件 | 同一源码：缺失 `pthread_cond_timedwait`（加载时拒绝），仍持有 mutex 时销毁运行时（诊断，退出码 3） | `T08_SYNC_MISSING`、`T08_SYNC_OWNED`；完整配方/结果在 `fixtures.psd1` |
| T08 内存 | `threads/memory.c`：分配/字节/字符串/线程共享用例、无效和重复释放 | API 30、`-O2 -nostdlib -fno-builtin`，公共选项与 T05 相同；`T08_MEMORY_INVALID`、`T08_MEMORY_DOUBLE` 以退出码 3 终止 |
| T08 io | `threads/io.c`：映射常规文件、显式 stat/时间布局、标志/错误及有限 sleep | API 30、`-O2 -nostdlib -fno-builtin`，公共选项与 T05 相同；四项语义及宿主状态检查 |
| T08 faults | `threads/faults.c`：宿主异常链、客体访问违规与 trap | API 30、`-O2 -nostdlib`，公共选项与 T05 相同；`T08_GUEST_FAULT` / `T08_GUEST_TRAP` 要求模块/偏移诊断及退出码 3 |
| T07 模块依赖图 | `modules/dependency.c`、`root.c`、`cycle.c`：正例库对、缺失导入、无效初始化入口、SONAME/依赖环种子 | API 30、`-O2 -nostdlib`，公共选项与 T05 相同；显式链接的夹具依赖及期望值在 `fixtures.psd1` |

每个夹具都由 NDK 自带的 clang 和链接器构建；除其选项所表明的以外，没有哪个是可以代表普通 NDK 产物的特制构建
（`PLAN.md` §5.3）。
