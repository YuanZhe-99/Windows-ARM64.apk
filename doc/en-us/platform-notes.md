# Platform notes

| Platform | Status |
|---|---|
| Windows 11 ARM64 | G0 passes; G1's checks pass on this PC (T02, T03): `winapk run` runs a pure-Java APK's real DEX in a Win32 window with touch, keys and typed Unicode text, scrolling, resizing, minimising and restoring, Back, saved state and SQLite, from Unicode paths; the Kotlin fixture T04 runs too. G2 passes as well: `native-probe.exe` loads N0 guest ELF libraries, runs the ABI suite T05 on them and refuses the malformed and out-of-profile ones (T06); `winapk run` does not load an APK's libraries yet. The interface still runs against sample data (`winapk.Mock`). |
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
| T01 host and JVM | `test.ps1 -Suite Host`: 22 checks PASS — `winapk.exe`, `winapk-bridge.dll`, `native-probe.exe`, zlib, SQLite and Capstone ARM64; JVM `jvm.dll` ARM64 created in-process with upstream's module options; JNI host → Java → host; Java2D; 259 font families; patch modules; and the negative cases (no JVM found, an invalid `--java-home`) |
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
| Negative cases | PASS | a file that is not an APK: exit 65, `INSTALL_FAILED_INVALID_APK`; `--native=restricted`: exit 64, `GUEST_NATIVE_NOT_IMPLEMENTED`; a normal run logs no `UI_THREAD_BLOCKED` |

## G2 checks

`test.ps1 -Suite Native`, 2026-11-16, on the machine above.

| Item (`PLAN.md` §9) | Result | Evidence |
|---|---|---|
| N0 positive cases return exact values, callbacks both ways and floating point and structures included | PASS | T05: 27 cases on each of four builds (`libn0abi.so` with RELR, `-rela`, `-sysv`, `-4k`): integers, guest `long`, narrow arguments with dirty upper bits, pointers, floating point, 20 arguments over the stack, HFAs, structures in registers, by reference and through `x8`, a variadic guest function, RELATIVE/RELR, ABS64, GLOB_DAT and JUMP_SLOT data, BSS, host imports, a host function pointer called by the guest, five levels of guest → host → guest reentry, callee-saved registers (and the harness catching a broken one), sp alignment; x18 is the TEB in every callback |
| Negative cases refused before any entry or constructor, with module, offset and reason | PASS | T06: 20 files (13 built, 7 derived by patching one field), each `reject CATEGORY file where: reason` with its expected category and reason, exit 2, nothing run ([native-profiles.md](native-profiles.md)) |
| Repeated load/execute without crash or growing resources; repeatable under address randomization | PASS | 2000 load/run/unload cycles, each with a refused load: heap allocations +0 bytes in both halves, private regions +0, handles +0; 32 copies at once at 32 distinct bases, all passing; two processes at two bases, all passing |
| On real ARM64 | PASS | the machine above; `native-probe.exe` and `capstone.dll` ARM64 |
| `abi-compatibility.md` | PASS | [abi-compatibility.md](abi-compatibility.md): direct, thunk and unsupported signatures with their cases |

## P3 thread and synchronization checks

`test.ps1 -Suite Bionic`, 2026-11-16: 39/39 checks PASS on the Windows ARM64 machine above. This is T08, not G3;
the thread fixture is built with NDK r30, API 30, `-O2 -nostdlib -femulated-tls -fno-stack-protector`;
the separate sync fixture uses `-O2 -nostdlib -fno-stack-protector` and no TLS.
Its hash and exact flags are in `tests/fixtures/elf/fixtures.json`
(generated, not committed); output is in `build/windows-arm64/test-data/logs/T08-*`.
The thread probe has nine cases, the sync probe eight; their refusal/fatal files are not evidence
that dependency or JNI integration works.

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
| N1 dependency loading and JNI integration | NOT_RUN | T07 and T09 not implemented; `winapk run` native loading remains disabled |

The same change set ran `check-requirements.ps1`, `build.ps1 -Component All` and
`test.ps1 -Suite Host` (22 PASS), `-Suite Java` (43 PASS), `-Suite Native` (58 PASS).
Go vet/tests (including `CGO_ENABLED=0`), MyGo vet/i18n and the documentation mirror check pass.
All built host PE/DLL files are ARM64; positive AArch64 guest fixture architecture is checked
by the build script. No new gate is claimed, no version bump, and no plan deviation.

## Known limitations

- Mac.apk's Windows changes: the `if(WIN32)` branch of `runtime/CMakeLists.txt`, the
  `winapk.guestNative` check of `AppClassLoader.findLibrary` and the `_WIN32` Unicode helpers of
  `runtime/src/native_sqlite.cpp`; everything else is new code under
  `runtime/platform/windows` and `runtime/guest`. What of `NativeCocoa` and the other host natives Windows implements,
  and what stays unbound, is in [port-audit.md](port-audit.md).
- The interface does not launch `winapk run` yet; it still uses `winapk.Mock`.
- Guest native: profile N0 plus the separate P3 T08 thread foundation, only in `native-probe.exe`; `winapk run` loads no APK library (P4). A fault inside guest code is not caught yet (P3, `PLAN.md` §10.3): it ends the probe. Built with `winapk.exe`'s flags, Control Flow Guard off; a CFG build is NOT_RUN ([abi-compatibility.md](abi-compatibility.md)).
- This PC has no vendor Vulkan driver: Vulkan is Mesa Dozen on Direct3D 12 ([decisions.md](decisions.md)).
- macOS build and smoke after the `CMakeLists.txt`, `AppClassLoader` and `native_sqlite.cpp` changes: NOT_RUN (no Mac here); the branch returns only under `WIN32`, the Java check is inert unless `winapk.guestNative` is set, and the CoreFoundation helpers are unchanged outside `_WIN32`.
- DPI: the app's density is fixed when it starts. Moving its window to a display of another scale, or changing the scale while it runs, resizes the window to keep its size in points (`WM_DPICHANGED`) but the app keeps its first density: NOT_RUN, and not yet handled as Android's configuration change.
- Input scope (`PLAN.md` §8.4): one pointer (the mouse; no multi-touch or pinch), the mapped keys plus any character through WM_CHAR. An input method's composition is not shown in the field while it is built, only committed, as on macOS; its candidate window sits where Windows puts it. A real IME session was not run (the test sends WM_CHAR as an IME commit arrives).
- The interface shows sample data only. Not done until the runtime contract has them: the app's surface in a pane or window, moving the App data folder's contents, data and converted-code sizes, and updating the private Java runtime ([gui.md](gui.md)).
- No native ARM64 claim can be made before the runtime runs on ARM64 hardware.
