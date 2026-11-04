# Function index

One row per Go package and runtime module: its role and the declarations to read first.
Declarations carry the Function Explanation Layer in their doc comments (first sentence: purpose;
then inputs, returns, side effects and notes where non-trivial).

| Package / module | Role | Start with |
|---|---|---|
| `cmd/windows-arm64-apk` | The interface's entry point, against `winapk.Mock` | `main` |
| `internal/gui` | The interface ([gui.md](../gui.md)): main window, panes and groups, app windows, sheets, Settings, Diagnostics | `App`, `New`, `view`, `launch`, `group`, `showTab` |
| `internal/winapk` | What the interface expects of the runtime (contract draft), and the Mock | `Client`, `Process`, `Report`, `APK`, `State`, `Mock` |
| `locales` | The interface's ARB catalogs (en, zh-Hans) and their generated accessors | `en.json` |
| `macapk/runtime/platform/windows` | The Windows ARM64 host: `winapk.exe` (`doctor`, `inspect`, `run`, `dex`), its APK reader, JVM and Vulkan probes, `windows.cmake`, `vcpkg.json`, `winapk.manifest` | `wmain`, `Run`, `ConvertDex`, `StageCode`, `BuildId`, `InspectApk`, `FindJvm`, `CreateJvm`, `ProbeVulkan` |
| `macapk/runtime/platform/windows/src/bridge` | `winapk-bridge.dll`: the Windows implementations of the Java side's host natives — `NativeLog` (`log.cpp`), `NativeCocoa`'s window, presentation and input (`window.cpp`) and upstream's `NativeSqlite` (`runtime/src/native_sqlite.cpp`) | `winapk_bridge_register`, `Bind`, `RunOnUi`, `Init`, `RunLoop`, `Tick`, `PresentBitmap`, `Touch`, `Key`, `Text`, `TaskToFront` |
| `macapk/runtime/guest/loader` | The N0 guest ELF loader ([native-profiles.md](../native-profiles.md)): checks, maps, screens, relocates and protects an Android AArch64 shared object without running it; host code | `Load`, `Module::Find`, `LoadError`, `CategoryName`, `Loader::Screen`, `Loader::Relocate`, `Loader::Protect` |
| `macapk/runtime/platform/windows/src/native_probe` | `native-probe.exe` (`PLAN.md` §9.1): `load`, `suite` (T05) and `stress`, the ABI cases, the callee-saved register harness | `wmain`, `Cases`, `RunSuite`, `CallPreserved`, `CheckHostState`, `Stress` |
| `scripts/windows` | Requirements check, build (Java, FrameworkRes, Fixtures, Host, NativeProbe) and test suites (Host = T01, Java = G1, Native = G2) | `build.ps1`, `test.ps1`, `check-requirements.ps1`, `common.ps1` |
| `tests/windows-arm64/apk-fixtures` | Fixture APK sources: `hello-java` (T03), `hello-kotlin` (T04, G1.6), `hello-jni` (G1.5, later T09) | `MainActivity` of each |
| `tests/windows-arm64/guest-elf` | Guest ELF fixture sources: `n0/abi.c` (T05), `negative/negative.c` (T06), and `fixtures.psd1`, every fixture's build and expected refusal | `fixtures.psd1` |
| `internal/version` | Product version | `Version`, `Older` |
| `tools/checkdocs` | Documentation mirror check | `main` |
