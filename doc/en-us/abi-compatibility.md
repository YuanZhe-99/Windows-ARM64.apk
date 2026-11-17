# ABI compatibility

Which calls between host code (Windows ARM64) and guest code (Android AArch64, AAPCS64 with the
LP64 data model) may go directly, which need a thunk, and which are not supported, with the
evidence for each. The evidence is fixture T05, run by `native-probe.exe` (`test.ps1 -Suite
Native`); the case names below are its own. Verified 2026-11-04 on the machine of
[platform-notes.md](platform-notes.md); guest code built with the NDK r30 clang 21 for API 30
([native-profiles.md](native-profiles.md) has the build parameters).

## Conventions compared

| Point | Android AArch64 | Windows ARM64 | Consequence | Evidence |
|---|---|---|---|---|
| Integer and pointer arguments | `x0`–`x7`, then the stack | the same | direct | `int.i32`, `ptr.store`, `stack.spill` |
| Floating-point arguments | `v0`–`v7`, then the stack | the same | direct | `fp.basic`, `fp.mixed`, `stack.spill` |
| Arguments past the registers | an 8-byte stack slot each, in order | the same | direct (Apple's arm64 packs small ones; Android does not) | `stack.small` |
| Narrow integers (`int8_t` … `uint16_t`) | the upper bits are unspecified; the callee extends | the same | the host need not extend what it passes | `int.narrow`, `int.narrow-dirty` |
| `long` | 64-bit | 32-bit (LLP64) | a guest `long` is `int64_t` on the host, never `long` | `int.long64` |
| Homogeneous floating-point aggregates (≤ 4 members) | `v0`–`v3`, both ways | the same | direct | `hfa.vec4f`, `hfa.complex` |
| Structures ≤ 16 bytes | `x0`/`x1`, both ways | the same | direct | `struct.pair32`, `struct.pair64` |
| Larger structures | passed by reference to a copy; returned through `x8` | the same, for C structures | direct | `struct.byref`, `struct.indirect` |
| Callee-saved registers | `x19`–`x28`, `x29` (the frame pointer), `d8`–`d15` | the same | direct | `regs.callee-saved` checks `x19`–`x27`, `d8`–`d15` and sp (in `x28`) around a guest function that needs them all; `regs.harness-detects` shows the check sees a broken one |
| Stack alignment | 16 bytes at every call | the same | direct | `regs.sp-entry`, `host.state` |
| `x18` | reserved; Android's clang never allocates it | the thread's TEB | guest code that names `x18` is refused; the host only reads it | `host.state` (x18 is the TEB in every callback); T06 `neg-x18` |
| `TPIDR_EL0` | Android's thread pointer | reserved for Windows | guest code that reads or writes it is refused | T06 `neg-tpidr` |
| Variadic functions | variadic arguments go like fixed ones (`v` registers for floating point) | every variadic argument goes in `x` registers and the stack, floating point too | a variadic prototype of one side never calls the other's variadic function | `va.fixed-prototype` |
| `long double` | 128-bit IEEE quad, in `q` registers | 64-bit, the same as `double` | not supported in any signature between host and guest | none: refused by design |
| Unwinding | DWARF `.eh_frame` | `.pdata`/`.xdata` and SEH | no exception crosses; registered guest-PC fatal diagnostics do not unwind or recover | T08 faults; T09 mixed JVM/guest trap |

## Signatures

| Signature | Host → guest | Guest → host | Evidence |
|---|---|---|---|
| Fixed parameters of integers up to 64 bits (guest `long` as `int64_t`), pointers, `float`, `double`, HFAs and C structures of those | direct | direct (an import bound to the host function) | every T05 case |
| Function pointers handed across | direct, by the rule above | direct | `callback.pointer` |
| Reentry: guest → host → guest, five levels | direct | direct | `callback.reenter` |
| A guest variadic function | direct, through a fixed host prototype of the promoted argument types | — | `va.fixed-prototype` |
| A host variadic function (`printf`, etc.) | — | never bind a guest variadic import directly to it | no host va_list is shared |
| Restricted guest formatter/log varargs | — | Android-ABI helper executes guest `...`/V calls; only fixed errno/bytes/log signatures enter the host | T08 `format`: integer register/stack spill, LP64, guest va_copy, truncation, refusals and exact UTF-8 logs |
| `va_list` across ABI domains | never | never | T08 format keeps it inside the guest helper |
| Guest JNI .../V/A calls | Registered fixed-signature native methods use static PE entries: descriptor-sized stack copies, unchanged compatible integer/FP argument locations, substituted guest environment and scalar/reference results | Android helper decodes .../V by recorded descriptor into 8-byte jvalue slots; fixed proxy wrappers call host A entries with a thread-private host environment | T09: constructors and .../V/A; actual JVM native registration, mixed 25-argument stack spills, long/float/double/object/void results, nested callbacks and Java exceptions |
| Data shared through an import (a host variable a guest GLOB_DAT reads and writes) | — | direct, for a type of the same layout on both sides | `import.call` |
| `stat`, `timespec` | explicit 128/16-byte guest layouts and conversion, never host CRT objects | the same | T08 `io`: NDK layout assertions, stat sentinels/values, wall-clock epoch/units and monotonic deadline |
| Other differing layouts (`sockaddr`, etc.) | unsupported | unsupported | — |
| Android mutex/condition objects | opaque guest storage, 40/48 bytes, alignment 4; no host lock stored there | fixed pointer signatures; generation/address tables own host state | T08 (`sync`): kinds, contention, wait/relock, lifecycle and refusal cases |
| Thread/key shims and C emutls | fixed integer/pointer signatures; Android `pthread_t` as `int64_t`, key as `int32_t` | explicit wrappers; no host pthread/CRT object passed to the guest | T08 (`threads`); see [native-profiles.md](native-profiles.md) |
| Guest dl APIs | fixed pointer/int signatures, guest opaque handle tokens | module namespace only; never Windows loader handles or arbitrary host symbols | T07: roundtrip, error consumption, flags and invalid/closed handles |
| Guest CRT exit/atfork registration | fixed callback/argument/DSO pointers, 32-bit status; no host CRT registry | runtime-owned records; exit callbacks run guest-side under a managed context; fork is unavailable | T07 N1: reverse order, DSO selection, reentry/once-only, registration limits and shutdown after close |
| Memory, bytes and strings | explicit 64-bit guest sizes/lengths, fixed pointers/ints | runtime-owned heap and ownership checks; byte wrappers only, no shared allocator/CRT objects | T08 `memory`: zero/overflow/failure, overlap/unsigned bytes, UTF-8, cross-thread ownership and invalid frees |
| `open`/`open64` optional mode | — | specialized fixed `(pointer, int32, uint32)` entry reads x2 only for creation; no host varargs or shared va_list | T08 `io`: two-argument open, three-argument create, fortified `__open_2` and unsupported flags/modes |
| `long double`, C++ classes, exceptions | not supported | not supported | — |

Direct means the host calls through a pointer of the matching host prototype, or the guest's
import is bound to the host function's address, with nothing in between. A signature is direct only
when this table lists it; the rest of a module's imports and exports are not called.

## Host-side limits

- N0 binds imports to the host's own functions; it generates no host thunks, so none needs unwind
  data or a valid call target of its own. `native-probe.exe` is built with `winapk.exe`'s flags;
  both run with dynamic code allowed and Control Flow Guard off (`native-probe` prints the policy).
  A build with CFG or generated thunks is NOT_RUN.
- Host state is only read: x18 against `NtCurrentTeb()` and sp in every callback, never a sentinel
  written to x18 (`PLAN.md` §9.3).
- T09's native entry bank is statically linked PE code, not runtime-generated code. Its two
  non-leaf assembly functions have Windows `.pdata/.xdata`; `llvm-readobj --unwind` decoded
  the 160/512-byte stack regions, frame/LR and x19 save/restore pairs. This inspection and
  real JNI calls do not establish guest exception unwinding or a CFG-enabled build.
