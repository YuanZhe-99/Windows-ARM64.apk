# Windows-ARM64.apk documentation

**These docs are the authoritative description of the code.** `doc/en-us/` is the source;
`doc/zh-cn/` mirrors it file for file. Plans and open work are not documented here; while the plan
is active it lives only in the repository's `PLAN.md`.

Windows-ARM64.apk runs selected Android APKs as native Windows 11 ARM64 processes. It is a port of
Mac.apk: the DEX → JVM translation and Android Framework run on a native ARM64 JVM, and a
restricted ELF loader with Bionic, JNI and graphics bridges runs the app's `arm64-v8a` libraries.
A MyGo front end opens each app in a phone-style pane or a standalone window, one runtime process
per APK.

| Document | Covers |
|---|---|
| [gui.md](gui.md) | The interface: windows, panes and groups, sheets, its files, the runtime contract draft, sample data |
| [abi-compatibility.md](abi-compatibility.md) | Which calls between host and guest code go directly, which need a thunk, which are unsupported, with evidence |
| [native-profiles.md](native-profiles.md) | The guest native profiles (N0): what the ELF loader admits, how it loads, and its refusals |
| [port-audit.md](port-audit.md) | P0: what in Mac.apk is tied to macOS and how each part is handled on Windows |
| [decisions.md](decisions.md) | Architecture decisions (ADRs) |
| [upstream.md](upstream.md) | Upstream contracts and quirks: Mac.apk, Windows ARM64 ABI, Android NDK, MyGo |
| [third-party.md](third-party.md) | Reused code and dependencies, licenses and obligations |
| [platform-notes.md](platform-notes.md) | What is implemented and verified on each platform, gate status |
| [development.md](development.md) | Environment, commands, the dev loop |
| [translation-guide.md](translation-guide.md) | Mirror rules and the glossary |
| [version-history.md](version-history.md) | Releases and the upstream versions they used |
| [functions/INDEX.md](functions/INDEX.md) | Per-package and per-module roles and key declarations |

## Project status

Development stopped on 2026-11-05 at version 0.1.0, which was never released or tagged. G0, G1 and
G2 pass on the Windows 11 ARM64 machine recorded in [platform-notes.md](platform-notes.md); G3, G4
and G5 were never run. What exists: `winapk run` runs pure-Java and Kotlin APKs in a Win32
window; `native-probe.exe` loads profile-N0 guest ELF libraries and runs the ABI suite on them;
the MyGo interface runs against sample data (`winapk.Mock`) and does not start the runtime. The
last build used Mac.apk `8b45e48`, Go 1.27.3 and MyGo v0.7.4.

Work resuming starts by moving Go, MyGo and Mac.apk to their latest versions and re-running the
G0–G2 checks, then continues with `PLAN.md` §10 (P3: Bionic, threads, errno, TLS) and §11 (P4:
JNI and loading an APK's libraries in `winapk run`).

## Upstream versions

Go, MyGo and Mac.apk are not pinned: they always follow their latest version. The table shows the
latest version at the last check (2026-11-04); everything else is locked in `build-lock.json`,
which also records what the last build used of the three.

| Component | Policy | Latest at last check |
|---|---|---|
| Go | latest release | 1.27.3 |
| MyGo | latest tag | v0.7.4 |
| Mac.apk | latest `main`, as the `macapk/` subtree | `8b45e48`, merged |
| OpenJDK (ARM64), clang-cl, MSVC, Windows SDK, CMake, Ninja, vcpkg, Vulkan headers | locked in `build-lock.json` | Microsoft OpenJDK 21.0.13; clang-cl 22.1.3; MSVC 14.51; SDK 10.0.26100; CMake 4.3.1; Ninja 1.13.2; vcpkg baseline `afa249f`; Vulkan headers 1.4.363; Capstone 5.0.9 |
| Android SDK/NDK, AOSP framework resources | chosen in `build-lock.json`, not installed yet | build-tools 36.0.0, android-36; NDK r30 (30.0.16248370); `android-16.0.0_r2` |
| ANGLE, Skia | chosen in P5a and P8 | not chosen yet; the Vulkan loader is Windows' own |
