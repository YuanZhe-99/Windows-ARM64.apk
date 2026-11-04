# Native profiles

Guest native code is off by default and runs only within a declared, tested profile
(`PLAN.md` §5.2). The loader is `macapk/runtime/guest/loader/elf_loader.cpp`; so far only
`native-probe.exe` uses it. `winapk run --native=restricted` still answers
`GUEST_NATIVE_NOT_IMPLEMENTED`, and an APK's libraries are not loaded (P4).

| Profile | State |
|---|---|
| N0, an auditable leaf ELF | implemented; G2 passes on this PC ([platform-notes.md](platform-notes.md)) |
| N1, a restricted C/JNI ELF (dependencies, imports, constructors, JNI) | not started (P3, P4) |
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

Every fixture is built by the NDK's own clang and linker; none is a special build that would stand
for ordinary NDK output beyond what its flags say (`PLAN.md` §5.3).
