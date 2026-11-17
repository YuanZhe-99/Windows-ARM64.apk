# Native profiles

Guest native code is off by default and runs only within a declared, tested profile
(`PLAN.md` §5.2). The loader is `macapk/runtime/guest/loader/elf_loader.cpp`; so far only
`native-probe.exe` uses it. `winapk run --native=restricted` still answers
`GUEST_NATIVE_NOT_IMPLEMENTED`, and an APK's libraries are not loaded (P4).

| Profile | State |
|---|---|
| N0, an auditable leaf ELF | implemented; G2 passes on this PC ([platform-notes.md](platform-notes.md)) |
| N1, a restricted C/JNI ELF (dependencies, imports, constructors, JNI) | separate P3 Bionic groups (T08) and module transactions (T07); complete profile admission and JNI incomplete, G3 NOT_RUN |
| N2, graphics samples | not started (P5, P6) |

## N0

A shared library built from audited source with known parameters: no dependency, no libc, no TLS,
no constructor or destructor, no IFUNC, no system call, no `x18` or `TPIDR_EL0`. Its exports are
called with the signatures of [abi-compatibility.md](abi-compatibility.md); its imports bind only
to the host symbols the caller names.

How a module loads, in this order; none of its code runs, and a refusal releases everything:

1. The file is read and the ELF header checked: ELF64, little-endian, version 1, AArch64, ET_DYN.
2. The program headers are checked against the file: every offset and length, `p_filesz` ≤
   `p_memsz`, the alignment and its congruence, PT_LOAD in order without overlap, one PT_DYNAMIC.
   Section headers are never read.
3. Pages: the host page (4 KiB here) and allocation granularity come from `GetSystemInfo`.
   Segments may share a host page only with the same permissions; one range of the whole span is
   reserved (`VirtualAlloc`, the system choosing the address), each segment committed read-write
   and copied; BSS is the zero of fresh pages.
4. PT_DYNAMIC: the string table first, then every tag; an unknown tag is refused.
5. Symbols: the count from DT_GNU_HASH or DT_HASH, every defined symbol inside the image.
6. Screening of every executable word (below).
7. Relocations, twice: a pass that checks every type, target and symbol and lists all missing
   imports, then a pass that writes them. Imports bind eagerly; there is no lazy binding.
8. Final protections per segment (R, RX, RW; never W+X), PT_GNU_RELRO read-only over its whole
   pages, then `FlushInstructionCache`.

| Relocation or table | N0 |
|---|---|
| `R_AARCH64_RELATIVE`, `ABS64`, `GLOB_DAT`, `JUMP_SLOT` | applied |
| RELR (`DT_RELR` and `DT_ANDROID_RELR`, NDK r30's default from API 28) | applied |
| `R_AARCH64_NONE` | skipped |
| TLS (`TLS_DTPMOD64`, `TLS_DTPREL64`, `TLS_TPREL64`, `TLSDESC`) | refused |
| `R_AARCH64_IRELATIVE` and IFUNC symbols | refused: a resolver would run while loading |
| `R_AARCH64_COPY` | refused: executables only |
| Android packed relocations (`DT_ANDROID_RELA`, APS2) | refused: not implemented |
| REL tables (`DT_REL`, `DT_ANDROID_REL`) | refused: AArch64 uses RELA |
| Any other type | refused, with its number |
| DT_GNU_HASH, DT_HASH | both read; GNU first when there are both |
| Symbol versions | `DT_VERNEED` refused (versioned imports need libraries); `DT_VERSYM` and `DT_VERDEF` not checked |

An undefined weak import the host does not provide binds to 0, as on Android. A symbol the module
defines binds to the module's own definition, never to a host symbol of the same name.

## P3 thread foundation (T08)

This is an independent probe, not N1 admission for APKs. `native-probe threads` binds
`ThreadRuntime::Imports()` plus a test-only host-state callback to an audited, dependency-free ELF under the unchanged N0 loader
checks. Its `-nostdlib -femulated-tls -fno-stack-protector` build is special, not evidence that
ordinary NDK output works. N0's original ABI suite and imports are unchanged.

- `__errno` returns a separate 32-bit guest errno in each managed thread context, never UCRT
  errno or `GetLastError`. The pthread functions return Linux error numbers without changing it.
- The compiler-required `memset` import has an explicit fixed-signature byte wrapper (64-bit
  size, 32-bit fill value); T08 checks bounds, return pointer, zero length and byte conversion.
- `pthread_create` accepts only a null attributes pointer (joinable, default host stack).
  `pthread_join`, `pthread_self`, `pthread_equal`, `pthread_key_create`, `pthread_key_delete`,
  `pthread_getspecific` and `pthread_setspecific` use explicit Android LP64 types: `pthread_t`
  is 64-bit, `pthread_key_t` is signed 32-bit. Tokens are not Windows handles or CRT objects.
  Non-null attributes return `ENOTSUP`; invalid/deleted keys return `EINVAL` on set/delete.
- Each host worker enters a fresh guest context before calling the fixed-signature guest
  function. Keys support 128 concurrent allocations, no stale-token reuse, clearing before
  destructor callbacks, and at most four destructor rounds. Key deletion does not run a
  destructor. Emulated TLS remains alive during all key destructors and is freed afterwards.
- `__emutls_get_address` reads the LLVM compiler-rt AArch64 descriptor layout (four 64-bit
  fields: size, alignment, object, initializer). Each context owns aligned, independently
  initialized storage keyed by descriptor address; the descriptor is not mutated.
  Only C `__thread` data is covered; C++ TLS destructors and guest ELF TLS
  (GD/LD, IE/LE, TLSDESC and PT_TLS) remain unsupported. The resolver must be an undefined
  import bound to this shim, not a separate compiler-rt copy embedded in the guest library.
- The caller must keep every module mapped until its contexts and callbacks have ended.
  An unjoined worker at runtime destruction, an unmanaged stateful shim entry, a malformed emutls
  descriptor or an allocation failure in the compiler TLS resolver terminates the probe with
  a diagnostic, never a fabricated TLS address. No exception is unwound through guest code.
  The guest-PC fault boundary below terminates rather than recovers execution.
  `pthread_exit` and detach/cancellation remain unsupported. Complete native-profile admission
  remains unfinished; APK/JNI integration is P4. These probes alone never establish G3.

The P3 synchronization subset is exercised separately by `native-probe sync` (T08):

- Android LP64 mutex/condition storage is 40/48 bytes, aligned to 4 bytes. The shim stores only
  a private generation marker in it; owner, recursion, waiters and host condition variables
  stay in runtime-owned address tables. Copying a live marker to another address, using an old
  generation or using a destroyed object returns `EINVAL`. No host lock layout crosses the ABI.
- `pthread_mutex_init` (null attributes), `lock`, `trylock`, `unlock` and `destroy` support
  default normal mutexes. The NDK's normal, recursive and error-checking static initializers
  are recognized; recursive ownership is counted, error-checking relock returns `EDEADLK`,
  recursion is limited to Bionic's 2048 acquisitions, and a busy trylock returns `EBUSY`.
  A normal mutex's blocking self-lock still deadlocks.
  Unlock without ownership returns `EPERM`; destroying an owned, contended or condition-bound
  mutex returns `EBUSY`. Explicit reinitialization is required after destruction.
- `pthread_cond_init` (null attributes), `wait`, `signal`, `broadcast` and `destroy` support
  the zero static initializer and untimed, process-private waits. Wait atomically releases
  logical mutex ownership and registers with the condition under the runtime registry lock;
  it reacquires the mutex before returning. Spurious wakeups are allowed: use a predicate loop.
  Simultaneous waiters must use the same mutex; a different binding returns `EINVAL`.
  Destroy with waiters returns `EBUSY`; waiting without owning the mutex returns `EPERM`.
- Non-null mutex/condition attributes and the monotonic condition static initializer return
  `ENOTSUP`; timed waits/locks, attribute APIs, process sharing, robust/PI mutexes and futex
  syscalls are not implemented. A condition wait with recursive depth greater than one returns
  `ENOTSUP` without releasing any ownership. Lazy allocation can return Linux `ENOMEM`.
  All these pthread calls leave guest errno unchanged.
- Waiter pins prevent destroying a mutex while a blocked lock or condition wait still uses it.
  Tests destroy their objects before the runtime's idle check; runtime teardown must not free
  owned or waited-on synchronization objects.

T08 checks fatal refusal of malformed descriptors and unjoined-worker teardown, isolation in
8 and 32 simultaneous guest threads, exact join pointers, main-thread
state, initialized/zero/aligned TLS, destructor re-arming and limits, deleted keys, key exhaustion,
unsupported attributes, invalid/self join, and read-only x18/sp host-state checks.

## P3 memory, bytes and strings (T08)

The `native-probe memory` subset uses fixed-signature wrappers, not raw UCRT export lookup.
Its audited imports are `malloc`, `calloc`, `realloc`, `free`; `memcpy`, `memmove`, `memset`,
`memcmp`, `memchr`; `strlen`, `strnlen`, `strcmp`, `strncmp`, `strcpy`, `strncpy`, `strchr`,
`strrchr`, `strstr`, `strdup`. Sizes and length returns are explicit unsigned 64-bit guest types.

- Each runtime owns a private Windows heap and an allocation ownership table shared by its
  managed threads. Bytes may cross a proven pointer signature; no allocator or CRT object does.
  Allocations are at least 16-byte aligned; outstanding allocations are released at runtime teardown.
- Zero-size malloc/calloc returns a freeable minimum allocation. Realloc with null acts as
  malloc; realloc with a non-null pointer and size zero frees it and returns null. A failed
  realloc leaves the old bytes owned and unchanged.
- Sizes above `PTRDIFF_MAX`, multiplication overflow and allocation failures return null and set
  guest `ENOMEM`; successful allocation and free preserve guest errno. Invalid, interior,
  host-owned or already-freed allocation pointers terminate with a guest ABI diagnostic.
- Byte/string functions preserve guest errno, compare unsigned bytes and perform no locale or
  encoding conversion. UTF-8 is copied unchanged. Their normal C buffer/termination requirements
  still apply; this is not a memory-safety sandbox. `strdup` uses the same private heap.

T08 memory combines allocation/overflow/reallocation, byte overlap/bounds, string/UTF-8 and
cross-thread allocation/free checks in one small positive fixture; an invalid free and a double
free use the same source in separate terminating probes. No stdio, formatted varargs or log API
is implied by the memory subset; the separate formatting helper below has a narrower declared API.

## P3 restricted formatting and logging (T08)

`libwinapk-bionic.so` is a small, project-built Android AArch64 helper, not a system libc or a
host PE library. The NDK builds it from `runtime/guest/bionic/format.c`; the same screened loader
maps it. Its `snprintf`, `vsnprintf`, `__android_log_print` and `__android_log_vprint` execute in
the guest ABI, so compiler-generated argument handling and va_list stay entirely in that domain.
Only fixed-signature errno/byte/log imports cross into the host. The helper stays mapped until
all dependent modules, guest threads and callbacks have ended.

- Formatting supports `%%`, `d/i/u/x/X/p/s/c`; integer `hh/h/l/ll/z` lengths where appropriate;
  `-`, `+`, space, `0`, `#`, width/precision and `*`. Guest long and size types remain 64-bit.
  Pointer text is `0x` followed by hexadecimal digits; strings are unchanged bytes, with precision
  measured in bytes. This is not a locale-sensitive formatter.
- Output is terminated when capacity is nonzero, truncated with a full would-have-written return
  count like snprintf. A null output is valid only with zero capacity. Supported calls preserve
  guest errno; unsupported conversion/length combinations (including floats, wide text, `%n` and
  positional arguments) return -1/`ENOTSUP`, with an empty output when capacity permits.
  Width/precision and format text are bounded at 4096; total result at 1 MiB. Limit failures use
  `EOVERFLOW`, never a fabricated zero result.
- Fixed `__android_log_write` writes a diagnostic record through the host, without passing a
  format or va_list to the host formatter. It accepts priorities 2–7, tags up to 127 bytes and
  messages up to 4095 bytes. Formatted logs refuse overflow rather than silently truncating.
  A completed/flushed record returns 1; invalid input, overflow or host I/O failure returns -1
  with the appropriate guest errno. There is no Android log daemon or stdio object emulation.

T08 format tests register/stack integer arguments, guest va_copy/V calls, bounds/truncation,
explicit unsupported conversions and literal/UTF-8 log bytes. It does not prove floating-point
varargs or ordinary NDK libc admission; full native-profile/APK integration remains P4.

## P3 files and time (T08)

The independent `native-probe io` subset receives a host Android root from its caller; it does
not change the process working directory or expose host descriptors. APK integration remains P4.

- Guest absolute `/data/...` and `/sdcard/...` map below that root. Relative/Windows paths,
  traversal, reserved device names, alternate streams, malformed UTF-8, trailing dots/spaces and
  reparse-point components are refused. Directories are prepared by the host, not created by open.
  The host must not replace the directory tree concurrently. This restricted path mapping is not
  race-resistant isolation, a security sandbox or a POSIX filesystem.
- `open`/`open64` accept read-only, write-only or read/write with `O_CREAT`, `O_EXCL`, `O_TRUNC`,
  `O_APPEND`, `O_CLOEXEC`, `O_LARGEFILE`. Creation supports mode `0600` only; unsupported modes,
  flags and directory/device opens fail explicitly. The optional integer mode is read only for
  creation through a fixed three-register signature, never a shared va_list.
- `read`, `write`, `close`, `lseek`/`lseek64`, `fstat`/`fstat64` use a runtime-owned fd table
  starting at 3, with non-inheritable Windows handles kept private. Standard streams, dup, pipes,
  sockets, directory iteration, permissions and arbitrary Linux file semantics are not implemented.
  Operations serialize per runtime; reads/writes may return a short count. Append writes target
  the file's current end, independently of the seek position.
- `fstat` writes an explicit 128-byte Android AArch64 layout: host file identity, size, link
  count, allocation blocks and access/write/change times; regular-file mode with synthetic guest
  owner permissions, uid/gid zero, 4096-byte preferred I/O size. It never exposes a host stat object.
  API errors return -1 and a converted Linux errno; successful operations preserve guest errno.
- `clock_gettime` supports realtime (Unix epoch, precise Windows wall clock) and monotonic
  (precise unbiased interrupt time, excluding suspend). Timespec is two signed 64-bit fields.
  Other clocks fail; `nanosleep` supports relative durations up to 60 seconds, rounding up to
  100 ns and waiting until the monotonic deadline. No guest signals/cancellation are delivered;
  no EINTR is synthesized, and the remainder is untouched on success.

T08 io combines roundtrip/seek/stat, open/append flags, mapping/error refusals and clock/sleep
checks. A host-side check confirms the Unicode-mapped file and that a refused junction path
did not truncate a file outside the mapped root.

## P3 module transactions (T07)

`ModuleSet` extends the loader for audited C-library graphs; the original N0 `Load` API still
refuses dependencies and constructors. This is not APK native admission or G3 completion.

- Names are portable `.so` basenames under one supplied library directory, never search paths,
  Windows DLL names or arbitrary host exports. A declared SONAME must equal the requested name.
  Cycles, versioned symbols and guest ELF TLS are explicitly refused in this subset.
- Prepare every new image and dependency, screen instructions, resolve eager imports, apply
  relocations and protections, then validate every initializer before running any guest code.
  A failure rolls back all new images; already committed images are unchanged.
- Definitions bind to their own image; undefined imports search direct dependencies breadth-first,
  then the explicitly audited host imports. Missing weak imports bind to zero. Guest code never
  runs to discover an IFUNC address.
- Constructors run dependency-first, once per mapped image, using DT_INIT then DT_INIT_ARRAY.
  The caller enters a managed guest context before opening a graph. Lookups and reopen from an
  initializing thread are allowed; a competing open during initialization fails explicitly
  rather than exposing incomplete state or blocking across a guest callback.
- Opaque 64-bit handles carry explicit open references. Close decrements a reference; the graph
  remains pinned until the ModuleSet is destroyed after all guest execution has ended.
  Finalizers and JNI_OnUnload are not invoked; this is the documented process-lifetime policy,
  not full dynamic unload. Initialization does not call JNI_OnLoad.
- `ThreadRuntime` can receive the namespace and bind guest `dlopen`, `dlsym`, `dlclose`,
  `dlerror` to it. Only RTLD_NOW|RTLD_LOCAL and explicit handles are supported; lazy/global/default
  scopes fail. A pending error is per guest context and consumed by dlerror, separate from errno.

T07 uses one positive dependency pair and a small refusal matrix to assert initialization
order/values, guest dl API/error consumption, lookup scope, repeat-open/close, unresolved weak imports, cycle/name rejection,
invalid constructor targets, and no constructor execution on a failed transaction.

## P3 guest fault boundary (T08)

The loader registers each fully linked/protected image before any initializer or exported call,
and removes it before unmapping. A last-priority vectored handler considers access violations,
illegal/privileged instructions and breakpoints only when the fault PC belongs to an executable
segment of a currently registered guest image. It writes `GUEST_EXECUTION_FAULT`, the module's
basename, guest virtual address and Windows exception code, then terminates the process with 3.
It never resumes corrupted guest state, runs guest cleanup or unwinds guest C++ into the host.

All other exceptions continue through the normal Windows chain: host/JVM/JIT code, faults
inside a host import and jumps to an address outside the registered executable segments are not
claimed by this boundary. It does not replace HotSpot's handlers, implement signals, recover
stack overflow or supply ELF unwind metadata. Mixed JVM/guest validation belongs to P4.
The registry lock is never held across guest execution or callbacks; fault reporting uses
bounded stack buffers and a direct Windows write, not an allocation or guest callback.

T08 faults uses a normal host-exception-chain case and isolated guest access-violation/trap
processes. The existing N0 stress suite checks registration/unregistration alongside mappings.

## Screening

Every 4-byte word of each executable segment is checked: a raw `svc`, `hvc` or `smc` and any
`mrs`/`msr` of `TPIDR_EL0` by encoding, and every instruction Capstone decodes for `x18`/`w18`
among its operands and implicit registers. A word Capstone does not decode counts as data.
Screening is static: it cannot prove that a library does not jump into data, decrypt code or
build it at run time. N0's admission rests on the audited source and build parameters; the screen
refuses what it can see (`PLAN.md` §5.2).

## Refusals

Each refusal prints `reject CATEGORY file where: reason`, where `where` is a file offset, a
virtual address or a symbol, before any guest code runs.

| Category | Meaning | T06 fixtures |
|---|---|---|
| `ELF_MALFORMED` | not a well-formed ELF: magic, sizes, offsets, overlap, tables outside the image | `neg-bad-magic`, `neg-truncated`, `neg-phdr-range`, `neg-overlap` |
| `ELF_UNSUPPORTED` | well formed, but not a kind this host loads: 32-bit, another machine, not ET_DYN, a W+X or write-only segment, an executable stack, segments sharing a host page with different permissions, text relocations, an unknown program header or dynamic tag | `neg-arm32`, `neg-x86_64`, `neg-et-exec`, `neg-rwx`, `neg-execstack`, `neg-1k-pages` |
| `ELF_RELOCATION_UNSUPPORTED` | a relocation type or table N0 does not apply | `neg-ifunc`, `neg-android-packed`, `neg-unknown-reloc` |
| `GUEST_SYMBOL_UNRESOLVED` | imports the host does not provide, all listed | `neg-missing` |
| `GUEST_ABI_UNSUPPORTED` | outside the profile: dependencies, constructors, destructors, TLS, versioned imports, a variant calling convention, IFUNC symbols, `svc`, `TPIDR_EL0`, `x18` | `neg-needed`, `neg-ctor`, `neg-tls`, `neg-svc`, `neg-tpidr`, `neg-x18` |
| `HOST_MEMORY_FAILED` | reserving, committing or protecting memory failed | none: the host has not refused yet |

`ELF_MALFORMED`, `ELF_UNSUPPORTED` and `HOST_MEMORY_FAILED` are added to the categories of
`PLAN.md` §14.2, which names the others.

## Fixtures

`tests/windows-arm64/guest-elf/fixtures.psd1` lists every fixture with its source, target, flags
and expected refusal; `build.ps1 -Component NativeProbe` builds them into `tests/fixtures/elf`
(generated, never committed) and records each hash in `fixtures.json` there.

| Fixture | What | Build |
|---|---|---|
| T05 `libn0abi.so` | the ABI suite ([abi-compatibility.md](abi-compatibility.md)) | `--target=aarch64-linux-android30 -shared -fPIC -fno-stack-protector -O2 -nostdlib -ffp-contract=off`: no stack protector (it imports from libc), no floating-point contraction (exact results) |
| T05 `libn0abi-rela.so`, `-sysv.so`, `-4k.so` | the same, with RELATIVE entries instead of RELR, a SysV hash table only, 4 KiB segment alignment | `-Wl,--pack-dyn-relocs=none`, `-Wl,--hash-style=sysv`, `-Wl,-z,max-page-size=4096` |
| T06 built (`negative/negative.c`) | one defect each, chosen by a `NEG_*` define, a target or a link option | the IFUNC case at `-O0`: at `-O2` clang folds its resolver away |
| T06 derived | `libn0abi.so` with one field patched (`build.ps1`, `Write-ElfMutation`) | — |
| T08 `libthreads.so` | `threads/threads.c`: managed pthreads, errno, keys/destructors, C emutls and compiler-required `memset`; nine probe cases must PASS | API 30, `-O2 -nostdlib -femulated-tls`, common flags as T05 |
| T08 negative files | same source, missing `pthread_detach` (load refusal), malformed emutls descriptor and unjoined-worker teardown (diagnostic, exit 3) | `T08_MISSING`, `T08_BAD_EMUTLS`, `T08_UNJOINED`; exact recipes/results in `fixtures.psd1` |
| T08 `libsync.so` | `threads/sync.c`: six mutex/condition cases, stress and host-state checks | API 30, `-O2 -nostdlib`, common flags as T05; NDK header layout assertions |
| T08 sync negative files | same source: missing `pthread_cond_timedwait` (load refusal), owned-mutex teardown (diagnostic, exit 3) | `T08_SYNC_MISSING`, `T08_SYNC_OWNED`; exact recipes/results in `fixtures.psd1` |
| T08 memory | `threads/memory.c`: allocation/byte/string/thread-sharing cases, invalid and double free | API 30, `-O2 -nostdlib -fno-builtin`, common flags as T05; `T08_MEMORY_INVALID`, `T08_MEMORY_DOUBLE` terminate with exit 3 |
| T08 io | `threads/io.c`: mapped regular files, explicit stat/time layouts, flags/errors and bounded sleep | API 30, `-O2 -nostdlib -fno-builtin`, common flags as T05; four semantic cases and host-state check |
| T08 faults | `threads/faults.c`: host exception chain, guest access violation and trap | API 30, `-O2 -nostdlib`, common flags as T05; `T08_GUEST_FAULT` / `T08_GUEST_TRAP` require a module/offset diagnostic and exit 3 |
| T08 format | `threads/format.c`: integer/guest-V formatting, bounds/refusals and UTF-8 byte logs | API 30, `-O2 -nostdlib -fno-builtin`, common flags as T05; maps the separately built product formatting helper |
| T07 module graph | `modules/dependency.c`, `root.c`, `cycle.c`: positive pair, missing import, invalid initializer, SONAME/cycle seed | API 30, `-O2 -nostdlib`, common flags as T05; explicit linked fixture dependencies and expected values in `fixtures.psd1` |

Every fixture is built by the NDK's own clang and linker; none is a special build that would stand
for ordinary NDK output beyond what its flags say (`PLAN.md` §5.3).
