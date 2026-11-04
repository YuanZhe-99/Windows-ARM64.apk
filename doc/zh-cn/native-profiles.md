# 原生执行 profile

客体原生代码默认关闭，只在已声明、已测试的 profile 内运行（`PLAN.md` §5.2）。加载器是
`macapk/runtime/guest/loader/elf_loader.cpp`；目前只有 `native-probe.exe` 使用它。`winapk run --native=restricted`
仍然返回 `GUEST_NATIVE_NOT_IMPLEMENTED`，APK 中的库不会被加载（P4）。

| Profile | 状态 |
|---|---|
| N0，可审计的 leaf ELF | 已实现；G2 在本机通过（[platform-notes.md](platform-notes.md)） |
| N1，受限的 C/JNI ELF（依赖、导入、构造函数、JNI） | 未开始（P3、P4） |
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

每个夹具都由 NDK 自带的 clang 和链接器构建；除其选项所表明的以外，没有哪个是可以代表普通 NDK 产物的特制构建
（`PLAN.md` §5.3）。
