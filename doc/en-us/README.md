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
| [native-profiles.md](native-profiles.md) | N0 and the declared P3 C/Bionic subset (T07/T08), admission, ABI limits and refusals |
| [graphics-capabilities.md](graphics-capabilities.md) | Graphics verification paths, selected ANGLE source build and explicit backend contracts |
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

Version 0.1.0 is unreleased. G0–G3 pass on the Windows 11 ARM64 machine recorded in
[platform-notes.md](platform-notes.md): Java/Kotlin APKs, N0 ELF/ABI, the declared C/Bionic
subset and the real hello-jni Activity matrix. `--native=restricted` is explicit; native is
still off by default. T09 covers both JNI directions, typed values, exceptions, dependencies,
load hooks, worker/nested callbacks and explicit unsupported-entry failure. A separate
CFG-enabled build and the restored ordinary build pass the full regression.

This is not arbitrary NDK/C++/ELF-TLS or graphics compatibility. P5/P6 graphics and G4/G5 remain
unverified. The user approved experiments on this PC's Mesa Dozen translated Vulkan
(ADR-0004), never as vendor-driver verification. The MyGo interface still uses `winapk.Mock` and
front-end integration is deferred. Mac.apk `3f35611`, Go 1.27.4 and MyGo v0.9.2 are current.

## Upstream versions

Go, MyGo and Mac.apk are not pinned: they always follow their latest version. The table shows the
latest version at the last check (2026-11-17); everything else is locked in `build-lock.json`,
which also records what the last build used of the three.

| Component | Policy | Latest at last check |
|---|---|---|
| Go | latest release | 1.27.4 |
| MyGo | latest tag | v0.9.2 |
| Mac.apk | latest `main`, as the `macapk/` subtree | `3f35611`, merged |
| OpenJDK (ARM64), clang-cl, MSVC, Windows SDK, CMake, Ninja, vcpkg, Vulkan headers | locked in `build-lock.json` | Microsoft OpenJDK 21.0.13; clang-cl 22.1.3; MSVC 14.51; SDK 10.0.26100; CMake 4.3.1; Ninja 1.13.2; vcpkg baseline `afa249f`; Vulkan headers 1.4.363; Capstone 5.0.9 |
| Android SDK/NDK, kotlinc, AOSP framework resources | locked in `build-lock.json` | build-tools 36.0.0, android-36; NDK r30 (30.0.16248370); kotlinc 2.4.0; `android-16.0.0_r2` |
| ANGLE | locked in P5a | `801a1a5`, ARM64 source build and host D3D11 / Vulkan-on-Dozen pass; not a guest graphics gate |
| Skia | chosen in P8 | not chosen yet; the Vulkan loader is Windows' own |
