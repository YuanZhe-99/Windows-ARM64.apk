# Decisions

Architecture decisions the plan asks to record (`PLAN.md` §1.9, §4.1, §11.1), newest last. Each
says what was decided, why, and what would reopen it.

## ADR-0001: host toolchain, JDK, process structure and JNI direction

2026-11-03, P0. Status: accepted.

**Toolchain.** The Windows host builds natively on ARM64 with Visual Studio 2026's own tools:
clang-cl 22.1.3 targeting `aarch64-pc-windows-msvc`, MSVC 14.51's linker and libraries, the
Windows SDK 10.0.26100, CMake 4.3.1 and Ninja 1.13.2. Third-party C/C++ code comes from Visual
Studio's built-in vcpkg in manifest mode at a locked baseline
(`macapk/runtime/platform/windows/vcpkg.json`). Output is ARM64 PE only; `windows.cmake` refuses
other targets and the build checks every PE it makes. Upstream's `runtime/CMakeLists.txt` gains one
`if(WIN32)` branch that includes `platform/windows/windows.cmake` and returns before the macOS
project, so the macOS build reads exactly what it read before. Why: everything is already on the
build machine and native; clang-cl matches the plan (§4.1) and the upstream tree's clang dialect.

**JDK.** Microsoft Build of OpenJDK 21 for ARM64 (21.0.13), installed with winget for development.
The Java side compiles with it unchanged, and T01 shows its `jvm.dll` (ARM64) embeds in a native
host with upstream's module options, JNI calls both ways, Java2D and fonts. JDK 21 is upstream's
default; the same distribution is the default of the private runtime the interface offers to
install (`PLAN.md` §2.4). Reopen if a later phase finds an incompatibility (the plan names JDK 25
as upstream's optional move).

**Process structure.** `winapk.exe` is one native ARM64 program: `doctor` and `inspect` are short
runs; `run` (P1) is one runtime process per APK, which the front end starts and talks to through
the contract of `internal/winapk`. The JVM is loaded from a folder chosen at run time
(`LoadLibraryEx` of `bin\server\jvm.dll`, never linked against `jvm.lib`), so Windows-ARM64.apk's
own runtime, a folder the person chooses or a development JDK serve alike; an emulated JVM is
refused.

**JNI direction.** Host natives are registered explicitly (`RegisterNatives`), as the T01 probe
does: on Windows the JVM looks up `Java_*` names only in libraries loaded through
`System.loadLibrary`, never in the program that embeds it. P1 puts the host natives in one host
bridge DLL with a registration table (the plan's default, §4.1), so an app's runtime and test JVMs
started by `java.exe` reach the same natives. Guest `.so` files never go through `LoadLibrary`:
they go to the guest loader and a per-ClassLoader guest library registry (§4.1, P2–P4).

**Consequences found in P0.** This PC's Vulkan is Mesa Dozen on Direct3D 12, not a vendor
driver. Nothing in P0–P4 depends on it. Before P5/P6 the project needs either a PC with a vendor
Vulkan driver for the graphics gates, or a recorded decision to measure guest Vulkan on Dozen and
report it as such (never as native Vulkan).
