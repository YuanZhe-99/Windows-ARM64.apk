# ABI 兼容性

宿主代码（Windows ARM64）与客体代码（Android AArch64，AAPCS64，LP64 数据模型）之间，哪些调用可以直通，
哪些需要 thunk，哪些不受支持，以及各自的证据。证据是由 `native-probe.exe` 运行的夹具 T05（`test.ps1 -Suite
Native`）；下文的用例名即其本身的名称。2026-11-04 在 [platform-notes.md](platform-notes.md) 所列的机器上验证；
客体代码由 NDK r30 的 clang 21 针对 API 30 构建（构建参数见 [native-profiles.md](native-profiles.md)）。

## 调用约定对照

| 要点 | Android AArch64 | Windows ARM64 | 结论 | 证据 |
|---|---|---|---|---|
| 整数与指针参数 | `x0`–`x7`，之后入栈 | 相同 | 直通 | `int.i32`、`ptr.store`、`stack.spill` |
| 浮点参数 | `v0`–`v7`，之后入栈 | 相同 | 直通 | `fp.basic`、`fp.mixed`、`stack.spill` |
| 超出寄存器的参数 | 按顺序各占一个 8 字节栈槽 | 相同 | 直通（Apple 的 arm64 会紧排小参数；Android 不会） | `stack.small` |
| 窄整数（`int8_t` … `uint16_t`） | 高位未定义；由被调用方扩展 | 相同 | 宿主传参时无需扩展 | `int.narrow`、`int.narrow-dirty` |
| `long` | 64 位 | 32 位（LLP64） | 客体的 `long` 在宿主上是 `int64_t`，绝不是 `long` | `int.long64` |
| 同质浮点聚合（≤ 4 个成员） | 传参与返回都用 `v0`–`v3` | 相同 | 直通 | `hfa.vec4f`、`hfa.complex` |
| ≤ 16 字节的结构体 | 传参与返回都用 `x0`/`x1` | 相同 | 直通 | `struct.pair32`、`struct.pair64` |
| 更大的结构体 | 以引用方式传递其副本；通过 `x8` 返回 | 对 C 结构体相同 | 直通 | `struct.byref`、`struct.indirect` |
| 被调用方保存的寄存器 | `x19`–`x28`、`x29`（帧指针）、`d8`–`d15` | 相同 | 直通 | `regs.callee-saved` 在一个需要全部这些寄存器的客体函数前后检查 `x19`–`x27`、`d8`–`d15` 和 sp（存于 `x28`）；`regs.harness-detects` 表明该检查能发现被破坏的寄存器 |
| 栈对齐 | 每次调用时 16 字节 | 相同 | 直通 | `regs.sp-entry`、`host.state` |
| `x18` | 保留；Android 的 clang 从不分配它 | 线程的 TEB | 引用 `x18` 的客体代码被拒绝；宿主只读取它 | `host.state`（每次回调中 x18 都是 TEB）；T06 `neg-x18` |
| `TPIDR_EL0` | Android 的线程指针 | 为 Windows 保留 | 读写它的客体代码被拒绝 | T06 `neg-tpidr` |
| 可变参数函数 | 可变参数与固定参数传法相同（浮点用 `v` 寄存器） | 所有可变参数都走 `x` 寄存器和栈，浮点也是 | 一方的可变参数原型绝不用来调用另一方的可变参数函数 | `va.fixed-prototype` |
| `long double` | 128 位 IEEE 四精度，用 `q` 寄存器 | 64 位，与 `double` 相同 | 在宿主与客体之间的任何签名中都不受支持 | 无：按设计拒绝 |
| 栈展开 | DWARF `.eh_frame` | `.pdata`/`.xdata` 与 SEH | 任何异常都不跨越；客体代码中的故障尚未处理（P3） | 尚无 |

## 签名

| 签名 | 宿主 → 客体 | 客体 → 宿主 | 证据 |
|---|---|---|---|
| 固定参数：64 位以内的整数（客体 `long` 作 `int64_t`）、指针、`float`、`double`、HFA 以及由这些组成的 C 结构体 | 直通 | 直通（导入绑定到宿主函数） | 所有 T05 用例 |
| 跨边界传递的函数指针 | 按上一行的规则直通 | 直通 | `callback.pointer` |
| 重入：客体 → 宿主 → 客体，五层 | 直通 | 直通 | `callback.reenter` |
| 客体的可变参数函数 | 通过由提升后参数类型组成的固定宿主原型直通 | — | `va.fixed-prototype` |
| 宿主的可变参数函数（`printf`、`__android_log_print`） | — | 需要一个读取客体参数、再调用类型明确的宿主函数的 thunk；不在 N0 中 | 尚无（P3） |
| `va_list` | 绝不 | 绝不 | — |
| 通过导入共享的数据（客体经 GLOB_DAT 读写的宿主变量） | — | 两侧布局相同的类型可直通 | `import.call` |
| 布局不同的结构体（`stat`、`timespec`、`sockaddr`、pthread 类型） | 显式的客体布局与转换（P3） | 同左 | 尚无 |
| `long double`、C++ 类、异常 | 不受支持 | 不受支持 | — |

直通指宿主通过匹配的宿主原型的指针调用，或客体的导入被绑定到宿主函数的地址，中间没有任何东西。只有本表列出的签名
才算直通；模块的其余导入与导出不会被调用。

## 宿主侧的限制

- N0 把导入绑定到宿主自己的函数；它不生成宿主 thunk，因此没有哪个 thunk 需要自己的展开数据或有效调用目标。
  `native-probe.exe` 以 `winapk.exe` 的编译选项构建；二者都在允许动态代码、关闭 Control Flow Guard 的条件下运行
  （`native-probe` 会打印该策略）。启用 CFG 或生成 thunk 的构建为 NOT_RUN。
- 宿主状态只读：每次回调中将 x18 与 `NtCurrentTeb()` 比对并检查 sp，从不向 x18 写入哨兵值（`PLAN.md` §9.3）。
