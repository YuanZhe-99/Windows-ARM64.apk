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
| 栈展开 | DWARF `.eh_frame` | `.pdata`/`.xdata` 与 SEH | 异常不跨域；已注册客体 PC 的致命诊断不展开、不恢复 | T08 faults；T09 JVM/客体混合 trap |

## 签名

| 签名 | 宿主 → 客体 | 客体 → 宿主 | 证据 |
|---|---|---|---|
| 固定参数：64 位以内的整数（客体 `long` 作 `int64_t`）、指针、`float`、`double`、HFA 以及由这些组成的 C 结构体 | 直通 | 直通（导入绑定到宿主函数） | 所有 T05 用例 |
| 跨边界传递的函数指针 | 按上一行的规则直通 | 直通 | `callback.pointer` |
| 重入：客体 → 宿主 → 客体，五层 | 直通 | 直通 | `callback.reenter` |
| 客体的可变参数函数 | 通过由提升后参数类型组成的固定宿主原型直通 | — | `va.fixed-prototype` |
| 宿主的可变参数函数（`printf` 等） | — | 绝不把客体变参导入直接绑定到它 | 不共享宿主 va_list |
| 受限客体格式化/日志变参 | — | Android ABI 辅助库执行客体 `...`/V 调用；只有固定 errno/字节/日志签名进入宿主 | T08 `format`：整数寄存器/栈溢出参数、LP64、客体 va_copy、截断、拒绝及精确 UTF-8 日志 |
| 跨 ABI 域的 `va_list` | 绝不 | 绝不 | T08 format 把它保持在客体辅助库中 |
| 客体 JNI .../V/A 调用 | 已注册固定签名 native 方法使用静态 PE 入口：按描述符复制栈槽、保持兼容的整数/浮点参数位置、替换客体环境，返回标量/引用 | Android 辅助库按记录的描述符将 .../V 解码为八字节 jvalue 槽；固定代理包装器使用线程私有宿主环境调用宿主 A 入口 | T09：构造及 .../V/A；真实 JVM native 注册、混合 25 参数栈溢出、long/float/double/对象/void 结果、嵌套回调与 Java 异常 |
| 通过导入共享的数据（客体经 GLOB_DAT 读写的宿主变量） | — | 两侧布局相同的类型可直通 | `import.call` |
| `stat`、`timespec` | 显式 128/16 字节客体布局及转换，绝不使用宿主 CRT 对象 | 同左 | T08 `io`：NDK 布局断言、stat 哨兵/值、墙钟 epoch/单位及 monotonic 期限 |
| 其他不同布局（`sockaddr` 等） | 不支持 | 不支持 | — |
| Android mutex/condition 对象 | 不透明客体存储，40/48 字节，按 4 字节对齐；绝不在其中存放宿主锁 | 固定指针签名；代际/地址表拥有宿主状态 | T08（`sync`）：类型、竞争、等待/重锁、生命周期与拒绝用例 |
| 线程/key shim 与 C 仿真 TLS | 固定整数/指针签名；Android `pthread_t` 用 `int64_t`，key 用 `int32_t` | 显式包装；不把宿主 pthread/CRT 对象交给客体 | T08（`threads`）；见 [native-profiles.md](native-profiles.md) |
| 客体 dl API | 固定指针/int 签名，客体不透明句柄标识 | 仅限模块命名空间；绝不传递 Windows 加载器句柄或任意宿主符号 | T07：往返、错误读取清除、标志及无效/已关闭句柄 |
| 客体 CRT 退出/atfork 注册 | 固定回调/参数/DSO 指针、32 位状态；不使用宿主 CRT 注册表 | 运行时所有的记录；退出回调在受管理上下文中执行客体代码；fork 不可用 | T07 N1：逆序、DSO 选择、重入/仅一次、注册上限及关闭后的运行时清理 |
| 内存、字节与字符串 | 显式 64 位客体大小/长度，固定指针/int | 运行时所有的堆及归属检查；仅字节包装，不共享分配器/CRT 对象 | T08 `memory`：零值/溢出/失败、重叠/无符号字节、UTF-8、跨线程归属及无效释放 |
| `open`/`open64` 可选 mode | — | 专用固定 `(pointer, int32, uint32)` 入口仅在创建时读取 x2；不使用宿主变参或共享 va_list | T08 `io`：双参数 open、三参数 create、fortified `__open_2` 及不支持的标志/模式 |
| `long double`、C++ 类、异常 | 不受支持 | 不受支持 | — |

直通指宿主通过匹配的宿主原型的指针调用，或客体的导入被绑定到宿主函数的地址，中间没有任何东西。只有本表列出的签名
才算直通；模块的其余导入与导出不会被调用。

## 宿主侧的限制

- N0 把导入绑定到宿主自己的函数；它不生成宿主 thunk，因此没有哪个 thunk 需要自己的展开数据或有效调用目标。
  `native-probe.exe` 以 `winapk.exe` 的编译选项构建；二者都在允许动态代码、关闭 Control Flow Guard 的条件下运行
  （`native-probe` 会打印该策略）。启用 CFG 或生成 thunk 的构建为 NOT_RUN。
- 宿主状态只读：每次回调中将 x18 与 `NtCurrentTeb()` 比对并检查 sp，从不向 x18 写入哨兵值（`PLAN.md` §9.3）。
- T09 的 native 入口表是静态链接 PE 代码，不是运行时生成代码。两个非叶汇编函数带有 Windows
  `.pdata/.xdata`；`llvm-readobj --unwind` 已解码 160/512 字节栈区域、frame/LR 及 x19 保存/恢复对。
  此检查和真实 JNI 调用不证明客体异常展开或启用 CFG 的构建。
