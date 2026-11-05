# Windows-ARM64.apk

**Android apps as native Windows 11 ARM64 processes**

Windows-ARM64.apk runs selected Android APKs natively on Windows 11 ARM64 — no Android emulator,
no virtual machine, no WSA or WSL. It is based on [Mac.apk](https://github.com/kksimp/Mac.apk):
the app's Java/Kotlin code is translated from DEX and runs on a native ARM64 JVM with Mac.apk's
Android Framework, and the app's own `arm64-v8a` native libraries are loaded by a restricted ELF
loader with a Bionic and JNI bridge. An app opens either in a phone-style pane inside the main
window or in a window of its own.

> **Status:** discontinued (2026-11-05). Development stopped at 0.1.0, before any release.
> The pure-Java path (gate G1) and a probe of the restricted ELF loader (gate G2) work on the test
> machine; nothing here is ready for use. See the [project status](doc/en-us/README.md#project-status),
> [PLAN.md](PLAN.md) (in Chinese) and the [platform notes](doc/en-us/platform-notes.md).

## Features

| Feature | Status |
|---|---|
| Pure Java/Kotlin APKs: Activities, resources, Views, input, local storage | works on the test machine (G1), standalone window only |
| Open an app in a phone-style pane or in a standalone window, one process per app | interface built on sample data, not connected to the runtime |
| `arm64-v8a` native libraries within declared, tested profiles (ELF loader, Bionic, JNI) | profile N0 loader in a test probe (G2); not in `winapk run` |
| OpenGL ES / EGL through ANGLE on Vulkan (D3D11 as a separate backend) | not started |
| Android Vulkan on the Windows Vulkan driver | not started |
| `doctor` and compatibility reports that state exactly what works on this machine | `doctor` checks the host, JVM and Vulkan; no compatibility report |

ARM32 and x86 app code, Google Play Services, DRM, WebView and full media support are out of scope
for the first versions.

## Architecture

- **The runtime** — one Windows ARM64 process per APK: the Mac.apk DEX translator and Framework on
  an ARM64 OpenJDK, a Win32 host backend, and the guest native and graphics bridges.
- **The front end** — a Go + [MyGo](https://github.com/egoist/mygo) application that lists, opens
  and hosts apps, in its own pane or in separate windows.

## Building from Source

No builds were released. Requirements and commands: [doc/en-us/development.md](doc/en-us/development.md).

## Documentation

- [English documentation](doc/en-us/README.md) · [中文文档](doc/zh-cn/README.md)

## Acknowledgements

- **[Mac.apk](https://github.com/kksimp/Mac.apk)** — for the DEX translation, the Android Framework
  and the native bridges this port builds on.
- **[MyGo](https://github.com/egoist/mygo)** — for its cross-platform desktop application framework.

## Open Source Licenses

Windows-ARM64.apk is licensed under [AGPL-3.0](LICENSE). Third-party components and their licenses
are listed in [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) and
[doc/en-us/third-party.md](doc/en-us/third-party.md). "Mac.apk" and the Mac.apk logo identify the
official Mac.apk project; Windows-ARM64.apk is an independent port, not affiliated with or endorsed
by it. Android is a trademark of Google LLC.
