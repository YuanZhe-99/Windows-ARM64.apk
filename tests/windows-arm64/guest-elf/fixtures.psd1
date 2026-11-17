# The guest ELF fixtures (PLAN.md §9.3, §10, §16): T05's N0 ABI suite, T06's refused ELF files,
# T07 module graphs and T08 Bionic groups. scripts/windows/build.ps1 -Component NativeProbe builds each
# entry with the NDK of build-lock.json into tests/fixtures/elf/ (generated, never committed) and
# records its hash in tests/fixtures/elf/fixtures.json; test.ps1 -Suite Native runs T05 with
# native-probe suite/stress and checks T06 refusals; Suite Bionic runs T07 and T08.
#
# Name: the file. Source: under tests/windows-arm64/guest-elf. Target: clang --target.
# Flags: added to Common. From + Mutation: a copy of another fixture with one field patched
# (build.ps1, Write-ElfMutation). Expect: the error category. Reason: text the refusal contains.
# Result: expected success/fatal output in the generated manifest. Exit: a runtime fatal's code.
# Command/Cases: T08 probe command and required positive case count (threads/9 by default).
@{
    Common   = @('-shared', '-fPIC', '-fno-stack-protector', '-Wall', '-Wextra', '-Werror')
    Fixtures = @(
        @{ Name = 'libio.so'; Id = 'T08'; Source = 'threads/io.c'; Target = 'aarch64-linux-android30'
           Flags = @('-O2', '-nostdlib', '-fno-builtin'); Command = 'io'; Cases = 5; Result = 'T08 PASS' }
        @{ Name = 'libmemory.so'; Id = 'T08'; Source = 'threads/memory.c'; Target = 'aarch64-linux-android30'
           Flags = @('-O2', '-nostdlib', '-fno-builtin'); Command = 'memory'; Cases = 5; Result = 'T08 PASS' }
        @{ Name = 'neg-memory-invalid.so'; Id = 'T08'; Source = 'threads/memory.c'; Target = 'aarch64-linux-android30'
           Flags = @('-O2', '-nostdlib', '-fno-builtin', '-DT08_MEMORY_INVALID', '-Wno-unused-function'); Command = 'memory'
           Exit = 3; Reason = 'free of invalid or already freed guest allocation'; Result = 'exit 3: invalid guest free' }
        @{ Name = 'neg-memory-double.so'; Id = 'T08'; Source = 'threads/memory.c'; Target = 'aarch64-linux-android30'
           Flags = @('-O2', '-nostdlib', '-fno-builtin', '-DT08_MEMORY_DOUBLE', '-Wno-unused-function'); Command = 'memory'
           Exit = 3; Reason = 'free of invalid or already freed guest allocation'; Result = 'exit 3: duplicate guest free' }
        @{ Name = 'libdep.so'; Id = 'T07'; Source = 'modules/dependency.c'; Target = 'aarch64-linux-android30'
           Flags = @('-O2', '-nostdlib'); Result = 'constructor trace 1, dependency_value=7' }
        @{ Name = 'libroot.so'; Id = 'T07'; Source = 'modules/root.c'; Target = 'aarch64-linux-android30'
           Flags = @('-O2', '-nostdlib'); Links = @('libdep.so'); Result = 'trace 12, root_value=49, repeat initialized once' }
        @{ Name = 'libbadroot.so'; Id = 'T07'; Source = 'modules/root.c'; Target = 'aarch64-linux-android30'
           Flags = @('-O2', '-nostdlib', '-DT07_MISSING'); Links = @('libdep.so'); Result = 'GUEST_SYMBOL_UNRESOLVED required_missing; no constructor' }
        @{ Name = 'libbadinit.so'; Id = 'T07'; Source = 'modules/root.c'; Target = 'aarch64-linux-android30'
           Flags = @('-O2', '-nostdlib', '-DT07_BAD_INIT'); Links = @('libdep.so'); Result = 'GUEST_ABI_UNSUPPORTED initializer; no constructor' }
        @{ Name = 'libcycle-seed.so'; Id = 'T07'; Source = 'modules/dependency.c'; Target = 'aarch64-linux-android30'
           Flags = @('-O2', '-nostdlib'); Soname = 'libcycle.so'; Result = 'GUEST_ABI_UNSUPPORTED SONAME mismatch' }
        @{ Name = 'libcycle.so'; Id = 'T07'; Source = 'modules/cycle.c'; Target = 'aarch64-linux-android30'
           Flags = @('-O2', '-nostdlib'); Links = @('libcycle-seed.so'); Result = 'GUEST_ABI_UNSUPPORTED dependency cycle; no constructor' }
        @{ Name = 'libsync.so'; Id = 'T08'; Source = 'threads/sync.c'; Target = 'aarch64-linux-android30'
           Flags = @('-O2', '-nostdlib'); Command = 'sync'; Cases = 8; Result = 'T08 PASS' }
        @{ Name = 'neg-sync-timedwait.so'; Id = 'T08'; Source = 'threads/sync.c'; Target = 'aarch64-linux-android30'
           Flags = @('-O2', '-nostdlib', '-DT08_SYNC_MISSING', '-Wno-unused-function'); Command = 'sync'
           Expect = 'GUEST_SYMBOL_UNRESOLVED'; Reason = 'pthread_cond_timedwait' }
        @{ Name = 'neg-sync-owned.so'; Id = 'T08'; Source = 'threads/sync.c'; Target = 'aarch64-linux-android30'
           Flags = @('-O2', '-nostdlib', '-DT08_SYNC_OWNED', '-Wno-unused-function'); Command = 'sync'
           Exit = 3; Reason = 'GUEST_ABI_UNSUPPORTED thread runtime: runtime destroyed with owned or waited-on synchronization objects'
           Result = 'exit 3: GUEST_ABI_UNSUPPORTED thread runtime: runtime destroyed with owned or waited-on synchronization objects' }
        # T08: no ELF TLS or dependencies; only the audited P3 thread imports.
        @{ Name = 'libthreads.so'; Id = 'T08'; Source = 'threads/threads.c'; Target = 'aarch64-linux-android30'
           Flags = @('-O2', '-nostdlib', '-femulated-tls'); Result = 'T08 PASS' }
        @{ Name = 'neg-threads-import.so'; Id = 'T08'; Source = 'threads/threads.c'; Target = 'aarch64-linux-android30'
           Flags = @('-O2', '-nostdlib', '-femulated-tls', '-DT08_MISSING', '-Wno-unused-function')
           Expect = 'GUEST_SYMBOL_UNRESOLVED'; Reason = 'pthread_detach' }
        @{ Name = 'neg-emutls-descriptor.so'; Id = 'T08'; Source = 'threads/threads.c'; Target = 'aarch64-linux-android30'
           Flags = @('-O2', '-nostdlib', '-femulated-tls', '-DT08_BAD_EMUTLS', '-Wno-unused-function')
           Exit = 3; Reason = 'GUEST_ABI_UNSUPPORTED thread runtime: invalid emulated TLS descriptor'
           Result = 'exit 3: GUEST_ABI_UNSUPPORTED thread runtime: invalid emulated TLS descriptor' }
        @{ Name = 'neg-threads-unjoined.so'; Id = 'T08'; Source = 'threads/threads.c'; Target = 'aarch64-linux-android30'
           Flags = @('-O2', '-nostdlib', '-femulated-tls', '-DT08_UNJOINED', '-Wno-unused-function')
           Exit = 3; Reason = 'GUEST_ABI_UNSUPPORTED thread runtime: runtime destroyed with unjoined workers or active contexts'
           Result = 'exit 3: GUEST_ABI_UNSUPPORTED thread runtime: runtime destroyed with unjoined workers or active contexts' }
        # T05: the same source four ways; every case must pass on each.
        @{ Name = 'libn0abi.so'; Id = 'T05'; Source = 'n0/abi.c'; Target = 'aarch64-linux-android30'
           Flags = @('-O2', '-nostdlib', '-ffp-contract=off') }
        @{ Name = 'libn0abi-rela.so'; Id = 'T05'; Source = 'n0/abi.c'; Target = 'aarch64-linux-android30'
           Flags = @('-O2', '-nostdlib', '-ffp-contract=off', '-Wl,--pack-dyn-relocs=none') }
        @{ Name = 'libn0abi-sysv.so'; Id = 'T05'; Source = 'n0/abi.c'; Target = 'aarch64-linux-android30'
           Flags = @('-O2', '-nostdlib', '-ffp-contract=off', '-Wl,--hash-style=sysv') }
        @{ Name = 'libn0abi-4k.so'; Id = 'T05'; Source = 'n0/abi.c'; Target = 'aarch64-linux-android30'
           Flags = @('-O2', '-nostdlib', '-ffp-contract=off', '-Wl,-z,max-page-size=4096') }

        # T06, built: one defect each.
        @{ Name = 'neg-svc.so'; Id = 'T06'; Source = 'negative/negative.c'; Target = 'aarch64-linux-android30'
           Flags = @('-O2', '-nostdlib', '-DNEG_SVC'); Expect = 'GUEST_ABI_UNSUPPORTED'; Reason = 'a raw system call' }
        @{ Name = 'neg-tpidr.so'; Id = 'T06'; Source = 'negative/negative.c'; Target = 'aarch64-linux-android30'
           Flags = @('-O2', '-nostdlib', '-DNEG_TPIDR'); Expect = 'GUEST_ABI_UNSUPPORTED'; Reason = 'TPIDR_EL0 is the host' }
        @{ Name = 'neg-x18.so'; Id = 'T06'; Source = 'negative/negative.c'; Target = 'aarch64-linux-android30'
           Flags = @('-O2', '-nostdlib', '-DNEG_X18'); Expect = 'GUEST_ABI_UNSUPPORTED'; Reason = 'x18 holds the Windows TEB' }
        @{ Name = 'neg-tls.so'; Id = 'T06'; Source = 'negative/negative.c'; Target = 'aarch64-linux-android30'
           Flags = @('-O2', '-nostdlib', '-DNEG_TLS'); Expect = 'GUEST_ABI_UNSUPPORTED'; Reason = 'PT_TLS' }
        @{ Name = 'neg-ctor.so'; Id = 'T06'; Source = 'negative/negative.c'; Target = 'aarch64-linux-android30'
           Flags = @('-O2', '-nostdlib', '-DNEG_CTOR'); Expect = 'GUEST_ABI_UNSUPPORTED'; Reason = 'constructors' }
        @{ Name = 'neg-missing.so'; Id = 'T06'; Source = 'negative/negative.c'; Target = 'aarch64-linux-android30'
           Flags = @('-O2', '-nostdlib', '-DNEG_MISSING'); Expect = 'GUEST_SYMBOL_UNRESOLVED'
           Reason = '2 imports the host does not provide: host_also_missing, host_missing' }
        # -O0: at -O2 clang folds the resolver and no IRELATIVE is left.
        @{ Name = 'neg-ifunc.so'; Id = 'T06'; Source = 'negative/negative.c'; Target = 'aarch64-linux-android30'
           Flags = @('-O0', '-nostdlib', '-DNEG_IFUNC'); Expect = 'ELF_RELOCATION_UNSUPPORTED'; Reason = 'R_AARCH64_IRELATIVE' }
        @{ Name = 'neg-needed.so'; Id = 'T06'; Source = 'negative/negative.c'; Target = 'aarch64-linux-android30'
           Flags = @('-O2'); Expect = 'GUEST_ABI_UNSUPPORTED'; Reason = 'DT_NEEDED' }
        @{ Name = 'neg-arm32.so'; Id = 'T06'; Source = 'negative/negative.c'; Target = 'armv7a-linux-androideabi30'
           Flags = @('-O2', '-nostdlib'); Expect = 'ELF_UNSUPPORTED'; Reason = '32-bit ELF' }
        @{ Name = 'neg-x86_64.so'; Id = 'T06'; Source = 'negative/negative.c'; Target = 'x86_64-linux-android30'
           Flags = @('-O2', '-nostdlib'); Expect = 'ELF_UNSUPPORTED'; Reason = 'machine 62 (x86-64)' }
        @{ Name = 'neg-execstack.so'; Id = 'T06'; Source = 'negative/negative.c'; Target = 'aarch64-linux-android30'
           Flags = @('-O2', '-nostdlib', '-Wl,-z,execstack'); Expect = 'ELF_UNSUPPORTED'; Reason = 'executable stack' }
        @{ Name = 'neg-1k-pages.so'; Id = 'T06'; Source = 'negative/negative.c'; Target = 'aarch64-linux-android30'
           Flags = @('-O2', '-nostdlib', '-Wl,-z,max-page-size=1024', '-Wl,-z,common-page-size=1024')
           Expect = 'ELF_UNSUPPORTED'; Reason = 'share a host page' }
        @{ Name = 'neg-android-packed.so'; Id = 'T06'; Source = 'n0/abi.c'; Target = 'aarch64-linux-android30'
           Flags = @('-O2', '-nostdlib', '-Wl,--pack-dyn-relocs=android'); Expect = 'ELF_RELOCATION_UNSUPPORTED'; Reason = 'APS2' }

        # T06, derived from libn0abi.so: the only difference is the named field.
        @{ Name = 'neg-bad-magic.so'; Id = 'T06'; From = 'libn0abi.so'; Mutation = 'bad-magic'
           Expect = 'ELF_MALFORMED'; Reason = 'bad magic' }
        @{ Name = 'neg-truncated.so'; Id = 'T06'; From = 'libn0abi.so'; Mutation = 'truncated'
           Expect = 'ELF_MALFORMED'; Reason = 'past the end of the file' }
        @{ Name = 'neg-phdr-range.so'; Id = 'T06'; From = 'libn0abi.so'; Mutation = 'phdr-range'
           Expect = 'ELF_MALFORMED'; Reason = 'program headers' }
        @{ Name = 'neg-overlap.so'; Id = 'T06'; From = 'libn0abi.so'; Mutation = 'overlap'
           Expect = 'ELF_MALFORMED'; Reason = 'overlaps PT_LOAD' }
        @{ Name = 'neg-rwx.so'; Id = 'T06'; From = 'libn0abi.so'; Mutation = 'rwx'
           Expect = 'ELF_UNSUPPORTED'; Reason = 'writable-and-executable' }
        @{ Name = 'neg-et-exec.so'; Id = 'T06'; From = 'libn0abi.so'; Mutation = 'et-exec'
           Expect = 'ELF_UNSUPPORTED'; Reason = 'only shared objects' }
        @{ Name = 'neg-unknown-reloc.so'; Id = 'T06'; From = 'libn0abi.so'; Mutation = 'unknown-reloc'
           Expect = 'ELF_RELOCATION_UNSUPPORTED'; Reason = 'relocation type 32767 unknown' }
    )
}
