# 原生执行 profile

客体原生代码默认关闭，只在已声明、已测试的 profile 内运行（`PLAN.md` §5.2）。加载器是
`macapk/runtime/guest/loader/elf_loader.cpp`；目前只有 `native-probe.exe` 使用它。`winapk run --native=restricted`
仍然返回 `GUEST_NATIVE_NOT_IMPLEMENTED`，APK 中的库不会被加载（P4）。

| Profile | 状态 |
|---|---|
| N0，可审计的 leaf ELF | 已实现；G2 在本机通过（[platform-notes.md](platform-notes.md)） |
| N1，受限的 C/JNI ELF（依赖、导入、构造函数、JNI） | 已独立实现 P3 线程基础（T08）；依赖与 JNI 尚未实现，G3 NOT_RUN |
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
  不允许异常经客体代码展开。客体故障范围处理、`pthread_exit`、detach/取消、文件/时间/日志 shim
  和依赖加载仍是 P3 后续工作；JNI 属于 P4。

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

每个夹具都由 NDK 自带的 clang 和链接器构建；除其选项所表明的以外，没有哪个是可以代表普通 NDK 产物的特制构建
（`PLAN.md` §5.3）。
