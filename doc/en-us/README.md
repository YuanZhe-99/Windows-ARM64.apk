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
| [upstream.md](upstream.md) | Upstream contracts and quirks: Mac.apk, Windows ARM64 ABI, Android NDK, MyGo |
| [third-party.md](third-party.md) | Reused code and dependencies, licenses and obligations |
| [platform-notes.md](platform-notes.md) | What is implemented and verified on each platform, gate status |
| [development.md](development.md) | Environment, commands, the dev loop |
| [translation-guide.md](translation-guide.md) | Mirror rules and the glossary |
| [version-history.md](version-history.md) | Releases and the upstream versions they used |
| [functions/INDEX.md](functions/INDEX.md) | Per-package and per-module roles and key declarations |

## Upstream versions

Go, MyGo and Mac.apk are not pinned: they always follow their latest version. The table shows the
latest version at the last check (2026-11-03); `build-lock.json` records what each build actually
used once P0 creates it.

| Component | Policy | Latest at last check |
|---|---|---|
| Go | latest release | 1.27.3 |
| MyGo | latest tag | v0.7.4 |
| Mac.apk | latest `main`, as the `macapk/` subtree | `367fd1a` (code unchanged since `f87be03`; `VERSION` 1.0.2303) |
| OpenJDK (ARM64), Android SDK/NDK, ANGLE, Vulkan Loader, Skia | chosen and locked in P0 | not chosen yet |
| clang-cl, Windows SDK, CMake, Ninja | chosen and locked in P0 | not chosen yet |
