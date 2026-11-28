# Windows-ARM64.apk

**Android apps as native Windows 11 ARM64 processes**

Windows-ARM64.apk runs selected Android APKs natively on Windows 11 ARM64 — no Android emulator,
no virtual machine, no WSA or WSL. It is based on [Mac.apk](https://github.com/kksimp/Mac.apk):
the app's Java/Kotlin code is translated from DEX and runs on a native ARM64 JVM with Mac.apk's
Android Framework, and the app's own `arm64-v8a` native libraries are loaded by a restricted ELF
loader with a Bionic, JNI and graphics bridge. An app opens either in a phone-style pane inside the
main window or in a standalone window, one runtime process per APK.

> **Status:** experimental (`0.1.6`; single-machine verification on
> Snapdragon 8cx Gen 3 / Windows 11 ARM64 build 26300).
> Gates G0–G3 pass for the declared Java/Kotlin and restricted N0/N1 C/Bionic/JNI profiles, the
> MyGo front end is wired to the real `winapk` runtime (`--embed` shared-memory frames and duplex
> IPC, zero-restart pane ↔ window switching), and bounded GLES (ANGLE D3D11 & Vulkan/Dozen) and
> Vulkan (`T13` WSI, SPIR-V graphics pipeline, offscreen `VkImage` readback and guest callbacks)
> profiles pass within their documented limits. Vendor native ARM64 Vulkan ICDs on 8cx Gen 3 remain
> unavailable (`BLOCKED`, Mesa Dozen `translated=yes` only). See
> [project status](doc/en-us/README.md#project-status), [PLAN.md](PLAN.md) (in Chinese) and
> [platform notes](doc/en-us/platform-notes.md).

## Compatibility & Default Switches

| Capability | Default | Status on Windows 11 ARM64 |
|---|---|---|
| Pure Java / Kotlin APKs (Activities, resources, Canvas Views, input, SQLite, local storage) | Enabled (`--native=off`) | `PASS` (G1 / Layer A) |
| MyGo desktop front end (phone-style pane, standalone window, zero-restart `Reattach`, private ARM64 OpenJDK 21 installer, diagnostics) | Enabled (`Windows-ARM64.apk.exe`) | `PASS` (all 17 `design/` screens, live `--embed` RGBA8 surface & IPC) |
| `arm64-v8a` guest native libraries (`N0` leaf, `N1` restricted C/Bionic/JNI, `N2` bounded graphics) | Off (`--native=restricted --profile=N0\|N1\|N2` required) | `PASS` for declared NDK API-30 profiles (G2 / G3 / Layer B); arbitrary C++ unwinding, ELF `PT_TLS` and inline syscalls refused |
| OpenGL ES 2.0 / EGL 1.4 via private ARM64 ANGLE (`--graphics=auto\|vulkan\|d3d11`) | `auto` (Vulkan first, then D3D11; explicit `vulkan` never falls back) | Bounded Java `GLSurfaceView` (`T10.java`) and guest GLES (`T11`) `PASS` on hardware D3D11 and Vulkan/Dozen |
| Guest Android Vulkan (`libvulkan.so`, `--graphics=vulkan`) | Off unless `--native=restricted --profile=N2 --graphics=vulkan` | Bounded `T13` WSI, transfer buffer, SPIR-V graphics pipeline, `VkImage` readback & callbacks `PASS` on Dozen (`translated=yes`); vendor ICD `BLOCKED` on 8cx Gen 3 |
| `winapk doctor --json` & `winapk inspect <apk> --json` | Static / non-executing | `PASS` (host/JVM/GPU/Vulkan probe, static ARSC label/icon, permissions, ELF `DT_NEEDED` profile & verdict) |

ARM32 and x86 app code, Google Play Services, DRM, WebView and arbitrary unscreened native binaries
are unsupported and fail with explicit error categories (`GUEST_NATIVE_DISABLED`,
`GUEST_ABI_UNSUPPORTED`, `GRAPHICS_CAPABILITY_MISSING`, `DEPENDENCY_MISSING`).

## Architecture

- **The runtime (`bin/winapk.exe`, `bin/winapk-bridge.dll`)** — one native Windows ARM64 process
  per APK: the Mac.apk DEX translator and Android Framework on an ARM64 OpenJDK 21, a Win32 host
  backend, and restricted guest ELF, Bionic, JNI, ANGLE GLES and Vulkan WSI bridges.
- **The front end (`Windows-ARM64.apk.exe`)** — a native ARM64 Go +
  [MyGo](https://github.com/egoist/mygo) desktop application that inspects, launches and hosts apps
  in a phone-style pane or standalone window without cross-process `HWND` parenting.

## Building & Packaging from Source

Requirements, build steps, test suites and local bundle packaging (`scripts/windows/package.ps1`):
[doc/en-us/development.md](doc/en-us/development.md).

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
