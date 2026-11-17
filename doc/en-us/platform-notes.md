# Platform notes

| Platform | Status |
|---|---|
| Windows 11 ARM64 | G0–G2 pass on this PC: Java/Kotlin APKs, input/lifecycle/storage/SQLite and standalone N0 ELF/ABI checks. The initial explicit `--native=restricted` APK path passes hello-jni's Activity value and loading-boundary checks; full G3 remains NOT_RUN. The interface still uses sample data (`winapk.Mock`). |
| Windows 11 x64 | Not a target; may host build tools and parsing tests only. |
| macOS (Apple Silicon) | Upstream Mac.apk's platform; imported as `macapk/`, now at `3f35611`; no macOS build or smoke run here yet (`NOT_RUN`). |

## Windows

Verified on Windows 11 ARM64 (2026-11-16): build 26300.9974, Snapdragon Compute Platform (8cx Gen 3), 32 GB, Qualcomm Adreno 8cx Gen 3 (driver 30.0.4122.4000), Vulkan loader 1.3.301 with Mesa Dozen devices only; Go 1.27.4, MyGo v0.9.2, Microsoft OpenJDK 21.0.13 ARM64, clang-cl 22.1.3.

| Check | Result |
|---|---|
| Go module | `go vet ./...`, `go test ./...` and `CGO_ENABLED=0 go test ./...` pass |
| Documentation mirror | `go run ./tools/checkdocs` passes |
| Requirements | `scripts/windows/check-requirements.ps1`: every locked tool OK, including the Android SDK (build-tools 36.0.0, android-36), NDK r30 and kotlinc 2.4.0 (Android Studio's) |
| Platform resources | `build.ps1 -Component FrameworkRes`: `framework-res.apk` from AOSP `android-16.0.0_r2` (API 36), 66.4 MB, sha256 `62eb212e…6d90`; its 14,530 public resource IDs equal android.jar's (android-36), one resource (`string/user_switched`) dropped by aapt2 for lacking a default |
| Fixtures | `build.ps1 -Component Fixtures`: `hello-java.apk`, `hello-jni.apk` and `hello-kotlin.apk` (with kotlin-stdlib 2.4.0 dexed in) signed and verified by apksigner; `libhello.so` ELF64 AArch64 (NDK r30); hashes in `tests/fixtures/*.json` |
| G1 Java path | `test.ps1 -Suite Java`: 43 checks PASS at 100% scale (density 160, 2026-11-16, including the `ReleaseCapture` fix on deactivation mid-press) and 43 PASS at 200% (density 320, 3840×2160 display, 2026-11-04); `winapk.exe`, `winapk-bridge.dll`, `sqlite3.dll` and `zd.dll` ARM64 |
| Java side (Mac.apk `3f35611`) | `build.ps1 -Component Java`: compiles unchanged, no error, Android's `org.apache.http.legacy` unpacked into the framework jar as upstream does ([port-audit.md](port-audit.md)) |
| Guest ELF fixtures | `build.ps1 -Component NativeProbe`: the 24 fixtures of T05 and T06 with NDK r30's clang 21, hashes in `tests/fixtures/elf/fixtures.json` ([native-profiles.md](native-profiles.md)) |
| G2 native path | `test.ps1 -Suite Native`: 58 checks PASS (below); `native-probe.exe` and `capstone.dll` ARM64 |
| T01 host and JVM | `test.ps1 -Suite Host`: 23 checks PASS (2026-11-17) — `winapk.exe`, `winapk-bridge.dll`, `native-probe.exe`, `jni-probe.exe`, zlib, SQLite and Capstone ARM64; JVM `jvm.dll` ARM64 created in-process with upstream's module options; JNI host → Java → host; Java2D; 259 font families; patch modules; and the negative cases (no JVM found, an invalid `--java-home`) |
| Vulkan | loader ARM64 available; two Mesa Dozen devices (Vulkan 1.2 on Direct3D 12); `doctor --present`: Win32 surface test passed. No vendor Vulkan driver |
| Interface | `go tool mygo vet .` and `go tool mygo i18n check` pass; every screen of `design/` renders in `ui.Tester` in both languages and appearances; a pane dragged with the pointer in `ui.Tester` splits its target (updated for MyGo v0.9.2); the native window opens and stays up (smoke run, sample data) |

## Gates

| Gate | Status |
|---|---|
| G0 host ready | PASS (2026-11-03, re-verified 2026-11-16 on this machine: T01, toolchain locked) |
| G1 Java MVP | PASS on this PC (2026-11-04 at 200% scale, re-verified 2026-11-16 at 100% scale), G1.6 included; a scale change while an app runs NOT_RUN (below) |
| G2 native minimal execution | PASS on this PC (2026-11-04, re-verified 2026-11-16): profile N0 in `native-probe.exe`; guest native stays off in `winapk run` |
| G3 native integration | NOT_RUN |
| G4 graphics | NOT_RUN |
| G5 experimental release | NOT_RUN |

## G1 checks

`test.ps1 -Suite Java`, 2026-11-04 (200% scale) and 2026-11-16 (100% scale), on the machine above.

| Item (`PLAN.md` §8) | Result | Evidence |
|---|---|---|
| G1.1 a real APK's DEX starts an Activity | PASS | cold run: `stage dex-converted (… ms)` by com.macapk.dex, `stage jvm-created`, `T03: lifecycle=onCreate`, `onResume`; the same from an APK and a data folder whose paths hold spaces, Hungarian, Korean and Chinese characters |
| G1.2 text, button and field; taps, typing, scrolling, Back | PASS | two clicks give `taps=2`; keys H, I give `text=hi`, then WM_CHAR é, 中文 and a surrogate pair (an emoji) append to it; a drag moves the list's first row to 12; Escape gives `back=pressed`, `onPause`, `onDestroy` |
| G1.3 size and coordinates; no process left | PASS at 100% and 200% scale | widened to 600×800 px the layout follows and a click at the new bounds gives `taps=3`; deactivating the window mid-press cancels the touch (no click); Back at the task root minimises the window and restoring it relaunches the activity; a minimised window logs the 2 Hz throttle and full rate on restore; with the UI thread suspended, a window call from the app's loop logs `UI_THREAD_BLOCKED: moveTaskToBack` and is abandoned with `UI_CALL_TIMEOUT`, and the app runs on; closing the window ends the process with 0 and it is gone. At 200% (4K): `scale 2.000000, density 320 dpi`, 400×800 points are an 800×1600 px client the Android view fills, and the 48 dp button is 96 px tall; a screenshot shows the layout at that scale. The scale is read from the display, not from `GetDpiForSystem`, which keeps the sign-in scale until the person signs out |
| G1.4 saved state across restarts, cold and hot cache | PASS | second run: `stage dex-converted (cached)`, `restored.prefs=3`, `restored.file=3`, and `databases/t03.db` holds the rows of all three launches |
| G1.5 an APK `.so` with guest native off | PASS | `hello-jni`: `native.error=GUEST_NATIVE_DISABLED: libhello.so …`, and the app runs on |
| G1.6 Kotlin fixture | PASS | T04 (`hello-kotlin`, kotlin-stdlib 2.4.0): `result=top=the:3,brown:1,dog:1 squares=140 copy=Count(word=fox, n=2)`, `onResume`, exit 0. This fixture only: no claim for other Kotlin or Compose apps |
| SQLite (T03) | PASS | `upper('café 中文')` is `CAFÉ 中文`, `LIKE 'CAFÉ%'` matches, `COLLATE LOCALIZED` orders `apple|Banana|café 中文`; a query of a missing table throws `SQLiteException` |
| T02 presenter | PASS | the Android view is the client area (400×800 px); the client pixel equals the theme's `windowBackground` #EFEDF1 |
| Negative cases | PASS | a file that is not an APK: exit 65, `INSTALL_FAILED_INVALID_APK`; an unknown native mode: exit 64, `GUEST_ABI_UNSUPPORTED`; a normal run logs no `UI_THREAD_BLOCKED` |

## G2 checks

`test.ps1 -Suite Native`, 2026-11-16, on the machine above.

| Item (`PLAN.md` §9) | Result | Evidence |
|---|---|---|
| N0 positive cases return exact values, callbacks both ways and floating point and structures included | PASS | T05: 27 cases on each of four builds (`libn0abi.so` with RELR, `-rela`, `-sysv`, `-4k`): integers, guest `long`, narrow arguments with dirty upper bits, pointers, floating point, 20 arguments over the stack, HFAs, structures in registers, by reference and through `x8`, a variadic guest function, RELATIVE/RELR, ABS64, GLOB_DAT and JUMP_SLOT data, BSS, host imports, a host function pointer called by the guest, five levels of guest → host → guest reentry, callee-saved registers (and the harness catching a broken one), sp alignment; x18 is the TEB in every callback |
| Negative cases refused before any entry or constructor, with module, offset and reason | PASS | T06: 20 files (13 built, 7 derived by patching one field), each `reject CATEGORY file where: reason` with its expected category and reason, exit 2, nothing run ([native-profiles.md](native-profiles.md)) |
| Repeated load/execute without crash or growing resources; repeatable under address randomization | PASS | 2000 load/run/unload cycles, each with a refused load: heap allocations +0 bytes in both halves, private regions +0, handles +0; 32 copies at once at 32 distinct bases, all passing; two processes at two bases, all passing |
| On real ARM64 | PASS | the machine above; `native-probe.exe` and `capstone.dll` ARM64 |
| `abi-compatibility.md` | PASS | [abi-compatibility.md](abi-compatibility.md): direct, thunk and unsupported signatures with their cases |

## P3 Bionic and module checks

`test.ps1 -Suite Bionic`, 2026-11-17: 104/104 checks PASS on the Windows ARM64 machine above (T08: 75, T07: 29), not G3;
the thread fixture is built with NDK r30, API 30, `-O2 -nostdlib -femulated-tls -fno-stack-protector`;
the separate sync fixture uses `-O2 -nostdlib -fno-stack-protector` and no TLS.
Its hash and exact flags are in `tests/fixtures/elf/fixtures.json`
(generated, not committed); output is in `build/windows-arm64/test-data/logs/T08-*`.
The thread probe has nine cases, the sync probe eight; their refusal/fatal files are not evidence
that APK native or JNI integration works. The T07 graph uses six `-O2 -nostdlib` libraries;
`-Suite Modules` independently passes its ten semantic cases, six hashes and summary check.
Its output is in `build/windows-arm64/test-data/logs/T07-modules.*`.
T08 memory adds 11 checks from three `-O2 -nostdlib -fno-builtin` builds of one source;
the positive process has four semantic cases and a host-state check, without another stress loop.
T08 io adds eight checks from one `-O2 -nostdlib -fno-builtin` build, with four semantic cases,
host-state, hash/summary and a host-side mapping/sentinel check.
T08 faults adds eight checks from three builds of one source: the host SEH chain survives,
while guest access violation and trap processes terminate with their module/virtual address.
T08 format adds nine checks: helper/fixture hashes, four semantic cases and host-state, summary
and exact log bytes. The product helper is an NDK-built, screened AArch64 ELF, not a PE DLL.
T07 N1 adds twelve checks: four fixture hashes, helper hash, six semantics and summary/log bytes.
Its positive source uses the NDK driver's normal API-30 shared-library link with CRT start files
and libc/libdl/liblog, not `-nostdlib` or special TLS/stack-protector flags.

| Item (`PLAN.md` §10) | Result | Evidence |
|---|---|---|
| Guest pthread/errno/key isolation | PASS | 8 and 32 simultaneous workers; distinct tokens, errno/TLS addresses and values; exact pointer return on join; main context unchanged |
| TLS initialization, alignment and destruction | PASS | initialized value 41, zero-filled values, 64-byte alignment; cleared keys before destructor callbacks; re-armed keys called twice or limited to four rounds; emutls still usable in every callback |
| Explicit errors and byte shim | PASS | deleted/invalid keys → `EINVAL`, 129th live key → `EAGAIN`, non-null thread attributes → `ENOTSUP`, self join → `EDEADLK`, unknown/already-joined thread → `ESRCH`; guest errno unchanged; `memset` return/bounds/zero length/byte conversion |
| Thread resource cleanup | PASS | 200 cycles / 1600 workers after warm-up: handle delta +0; thread/context/key/emutls registries empty after every call; 5413 host-state callbacks, no x18/sp/errno violation |
| Mutex kinds, layout and lifecycle | PASS | 40-byte, four-byte-aligned storage with sentinels; normal, recursive (2048 limit), error-checking; eight workers × 256 increments = 2048; copied/stale/destroyed objects and wrong owner diagnosed; explicit reinit; errno unchanged |
| Condition wait/signal/broadcast | PASS | 48-byte storage with sentinels; 256 request/ack signal handshakes; eight waiters broadcast; mutex owned on every return; busy condition/mutex destruction refused, including an unlocked mutex pinned by condition waits; a second mutex binding and recursive depth >1 refused |
| Sync resource cleanup and refusals | PASS | 200 cycles / 1600 workers, handle delta +0, sync registries empty; 1916 host-state callbacks without violations; timed-wait import refused before entry, owned-mutex runtime teardown exits 3 with a diagnostic |
| Out-of-scope native behavior | PASS | missing `pthread_detach` import refused before entry; ELF PT_TLS, raw `svc`, TPIDR_EL0 and x18 still refused with the P3 import table; malformed emutls descriptor and unjoined-worker teardown both terminate with a diagnostic and exit 3 |
| Module transactions and lookup (T07) | PASS | dependency-first trace `12`, root result `49`, own definitions, unresolved weak import, breadth-first dependency export lookup, repeat opens/refcounts and close/reopen without reinitialization |
| Transaction refusal and callbacks (T07) | PASS | missing import and invalid initializer roll back all new mappings before any constructor; cycle, basename, SONAME and missing-file refusals; existing graph unchanged; same-thread constructor reentry and competing-thread refusal without holding the registry lock across callbacks |
| Guest dl API (T07) | PASS | `dlopen` NOW/LOCAL, `dlsym`, `dlclose`, `dlerror` consumption; invalid flags, missing export and already-closed handle fail; runtime context cleanup |
| Memory/byte/string subset (T08) | PASS | zero-size allocations, 16-byte alignment, calloc zeroing/overflow, grow/shrink/failed realloc and preserved contents/errno; overlap/bounds/unsigned-byte operations, UTF-8/termination/padding/search; eight-thread ownership transfer; invalid and double free exit 3 before the host allocator; empty allocation table; 12 host-state callbacks without violations |
| Files and errno (T08 io) | PASS | Unicode/space root and filename; bytes `410042FF5A`, EOF and zero-length I/O; seek above 8 GiB and back without file growth; 128-byte stat with sentinels; create/exclusive/append/read-only/write-only/fortified-open behavior; converted ENOENT/EEXIST/EINVAL/EBADF; unsupported flags/modes and invalid UTF-8/device/traversal/stream/junction paths refused; outside sentinel unchanged, fd table empty |
| Time (T08 io) | PASS | realtime Unix epoch within two seconds of the host, 100-ns to normalized nanosecond units; monotonic interval at least the requested 2 ms; zero sleep, untouched successful remainder, invalid times and unsupported clocks/durations; four host-state callbacks without violations. Suspend and guest signals NOT_RUN/not supported |
| Guest-PC fault boundary (T08 faults) | PASS | host-raised access violation reaches its host SEH handler, guest errno/x18/sp unchanged; guest access violation at `neg-guest-fault.so` vaddr `0x42d0` (Windows `0xc0000005`) and trap at `neg-guest-trap.so` vaddr `0x42b4` (`0xc000001d`) each produce `GUEST_EXECUTION_FAULT` then exit 3; no resume/unwind; N0's 2000 cycles still have +0 heap allocations/private regions/handles |
| Restricted formatting/logging (T08 format) | PASS | LP64 minima/maxima and integer arguments spilling to the stack; flags/width/precision, guest va_copy and V variants; would-have-written counts, sentinel-preserving truncation and embedded NUL; `%n`, float/wide/positional formats and limits refused with errno; exact literal-percent/UTF-8 records through fixed host log entry; four host-state callbacks without violations. Floating-point varargs and general stdio are not supported |
| NDK system-library admission (T07 N1) | PASS | normal NDK link runs its constructor and exact `N1:4294967297` result; `LIBC` requirements bind only to named logical providers; libc/log lookup isolation; fork, unknown version and malformed chain fail with no constructor or retained image; original N0 refusals unchanged |
| Guest CRT lifetime (T07 N1) | PASS | reverse/DSO-selected/reentrant order `3412`, no duplicate callback; 128 atfork records, overflow and DSO removal without invoking hooks; null exit callback and 256-entry capacity errors, exactly 255 selected callbacks while one remains pending; close keeps code mapped; final callback runs with fresh errno, allocates/frees guest heap bytes, refuses new workers, preserves x18/sp and produces constructor/shutdown trace `19` |
| APK native and JNI integration | NOT_RUN | standalone T09 JNI foundation passes below; `winapk run` native loading remains disabled |

The same change set ran `check-requirements.ps1`, `build.ps1 -Component All` and
`test.ps1 -Suite Host` (22 PASS), `-Suite Java` (43 PASS), `-Suite Native` (58 PASS).
Go vet/tests (including `CGO_ENABLED=0`), MyGo vet/i18n and the documentation mirror check pass.
The formatter increment's complete output is saved under `build/windows-arm64/test-data/logs/P3-format-*.log`.
The 2026-11-17 N1 increment ran requirements/build All and Host 22, Native 58, Bionic 104, Java 43;
logs are `P3-n1-*.log` in the same directory. A final Bionic 104 rerun covers the strengthened exit
quota/heap assertions. JNI/Java native calls are not part of these N1 C-library results.
All built host PE/DLL files are ARM64; positive AArch64 guest fixture architecture is checked
by the build script. No new gate is claimed, no version bump, and no plan deviation.

## P4 JNI foundation checks

2026-11-17 on the ARM64 machine above, Microsoft JVM `21.0.13.0.2+1-LTS`:
`build.ps1 -Component All`, then `test.ps1 -Suite Jni`: 24/24 PASS, with `-Xcheck:jni` and no
warnings; a final rerun uses `-Xbatch` and verifies the installed Java implicit-fault method
in `T09-compilation.xml`. Seven NDK-built fixtures, two product guest helpers, nine semantic groups and five short
fatal processes; no additional pressure loop. The 24-check suite took 1.53 seconds on this
machine (one run, not a performance guarantee). This is not G3.

| Item | Result | Evidence |
|---|---|---|
| Guest → Java arguments/results | PASS | .../V/A constructor, static, instance and nonvirtual calls; mixed 25-argument signature spills integer/FP registers; narrow values, 64-bit long, promoted float/double, object and primitive-array descriptors; repeated V with the same va_list; virtual override differs by 100; all scalar result categories and object/void calls |
| References and exceptions | PASS | UTF-16 embedded NUL roundtrips as modified UTF-8; array element commit/release and region values; local frame promotion, global/weak/local ref kinds; real pending IllegalArgumentException, missing methods, invalid descriptor/ID/category and missing-class NoClassDefFoundError; configured isolated loader and FindClass initialization |
| JVM worker lifecycle | PASS | detached GetEnv and refused version; normal/daemon Attach, repeated Attach identity, explicit Detach or automatic host-thread-exit Detach after a Bionic key destructor that still calls JNI; borrowed main thread is not detached; exactly one main proxy after joins |
| Persistent host-thread binding | PASS | guest pthread identity, key value and emutls address/value survive returning to the host; nested host-controlled guest entry changes the same errno/TLS; the last binding's key destructor still calls JNI and reenters that binding, then TLS is freed and the runtime is idle; this does not prove Java native entry thunks |
| JVM → guest native registration | PASS | six fixed-signature methods entered through actual JVM RegisterNatives PE entries: mixed 25 arguments, long/float/double/object/void results, instance/static calls, object/array mutation and Java exception propagation; depth-five Java↔guest callbacks and normal/daemon worker native calls; data target, non-native Java method, slot quota and foreign loader refused; native reentry during key cleanup and UnsatisfiedLinkError after real unregistration |
| Explicit JNI library lifecycle | PASS | normal NDK CRT/libc/libdl links; dependency/root/hook trace `2 1 3`, repeated root load unchanged, root without a hook appends only constructor `1` and never calls dependency hook `9`; bad version and pending IllegalStateException return failure once, retries do not rerun; data-valued JNI_OnLoad refused; a method registered during the bad-version hook works inside that hook but terminates explicitly if invoked after its failure |
| JNI export lookup | PASS | short name wins over a conflicting long name; int/long overloads, array/object descriptors, underscore/Chinese UTF-16 escaping and a DT_NEEDED dependency export return exact values through the JVM; explicit registration wins; repeated binding allocates no duplicate entries; JVM-only `1bad` throws UnsatisfiedLinkError despite a matching export |
| Fault and misuse boundary | PASS | unsupported slot 217 and cross-thread JNIEnv: exit 3 with explicit category; guest trap/access violation: registered module/offset and exit 3; hot Java null-pointer/division exceptions remain JVM-owned and complete normally; x18/sp checked read-only on main/workers |
| Initial APK call-site integration | PASS | `ApkJni` six checks: off/restricted launches with CheckJNI, real Activity value 43, explicit registration and once-only hook across System/Runtime loads, late-defined class export, missing/outside-directory refusal and pre-DEX ZIP traversal refusal; both launches exit 0. Full Activity/G3 matrix remains unfinished |

The regression also passes requirements, Host 23, Native 58, Bionic 104 and Java 43;
Go formatting/vet/tests (including CGO-disabled), MyGo vet/i18n and docs mirror pass.
Logs are `build/windows-arm64/test-data/logs/P4-jni-*.log` and `T09-*`.
The subsequent binding increment's full regression is in `P4-context-*.log`; it adds only the
persistence and teardown semantic checks to the existing positive process, not another loop.
The static-native-entry increment's regression is in `P4-native-*.log`, adding one semantic group
to that same process. `llvm-readobj --unwind` also decodes both assembly functions' Windows
save/restore records; this is not a guest-unwinding or CFG-enabled test.
The explicit-load increment's complete requirements/build All and Host 23, Java 43, Native 58,
Bionic 104, Jni 24 regression is in `P4-load-*.log`. Its six library variants use normal NDK
shared-library links, not the core probe's `-nostdlib`/emulated-TLS flags; no arbitrary APK claim.
The initial APK increment's requirements/build All and full regression are in `P4-apk-*.log`:
Host 23, Java 43, Native 58, Bionic 104, Jni 24 and ApkJni 6 PASS. ApkJni took 4.77 s and Jni
0.96 s (single measurements). The APK fixture uses ordinary NDK API 30; the earlier API-24
compact RX metadata layout was refused rather than exempted from executable-byte scanning.
No new gate/layer completion or release follows; G3 and complete layer B remain NOT_RUN.
The first mixed trap experiment failed: the JVM exited 1 before the tail guest handler ran.
The repair installs a strictly guest-PC-scoped first handler, never replaces HotSpot's handlers
and leaves every non-guest PC in its original chain. The retained T09 cases guard that boundary.
No new gate, version bump or plan deviation; CFG-enabled and macOS runs remain NOT_RUN.

## Known limitations

- Mac.apk's Windows changes: the `if(WIN32)` branch of `runtime/CMakeLists.txt`, the
  `winapk.guestNative` check of `AppClassLoader.findLibrary` and the `_WIN32` Unicode helpers of
  `runtime/src/native_sqlite.cpp`; everything else is new code under
  `runtime/platform/windows` and `runtime/guest`. What of `NativeCocoa` and the other host natives Windows implements,
  and what stays unbound, is in [port-audit.md](port-audit.md).
- The interface does not launch `winapk run` yet; it still uses `winapk.Mock`.
- Guest native: standalone N0/P3 and P4 T09 probes plus the first restricted APK Activity increment. Full G3/N1 remains unfinished. The guest-PC boundary diagnoses recognized guest faults, not host imports, JIT/JVM faults, out-of-range branches or stack overflow. T09 proves the stated mixed-JVM cases only. Control Flow Guard is off; a CFG build is NOT_RUN ([abi-compatibility.md](abi-compatibility.md)).
- This PC has no vendor Vulkan driver: Vulkan is Mesa Dozen on Direct3D 12 ([decisions.md](decisions.md)).
- macOS build and smoke after the `CMakeLists.txt`, `AppClassLoader` and `native_sqlite.cpp` changes: NOT_RUN (no Mac here); the branch returns only under `WIN32`, the Java check is inert unless `winapk.guestNative` is set, and the CoreFoundation helpers are unchanged outside `_WIN32`.
- DPI: the app's density is fixed when it starts. Moving its window to a display of another scale, or changing the scale while it runs, resizes the window to keep its size in points (`WM_DPICHANGED`) but the app keeps its first density: NOT_RUN, and not yet handled as Android's configuration change.
- Input scope (`PLAN.md` §8.4): one pointer (the mouse; no multi-touch or pinch), the mapped keys plus any character through WM_CHAR. An input method's composition is not shown in the field while it is built, only committed, as on macOS; its candidate window sits where Windows puts it. A real IME session was not run (the test sends WM_CHAR as an IME commit arrives).
- The interface shows sample data only. Not done until the runtime contract has them: the app's surface in a pane or window, moving the App data folder's contents, data and converted-code sizes, and updating the private Java runtime ([gui.md](gui.md)).
- No native ARM64 claim can be made before the runtime runs on ARM64 hardware.
