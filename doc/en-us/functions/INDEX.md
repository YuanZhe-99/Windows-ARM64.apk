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
| `macapk/runtime/guest/loader` | N0 ELF loader and P3 transactional dependency namespace ([native-profiles.md](../native-profiles.md)); eager linking, protected images, dependency-first constructors and pinned handles | `Load`, `Module`, `ModuleSet`, `Loader::Prepare`, `Loader::Finish`, `ModuleSet::Open`, `ModuleSet::Find` |
| `macapk/runtime/guest/bionic` | P3's managed guest thread/errno/TLS-key/emutls, mutex/condition and dl API subset ([native-profiles.md](../native-profiles.md)); no APK native admission yet | `ThreadRuntime`, `GuestThreadContext`, `Create`, `Join`, `EmutlsAddress`, `ConditionWait`, `DlOpen`, `DlError` |
| `macapk/runtime/platform/windows/src/native_probe` | `native-probe.exe`: `load`, `suite` (T05), `stress`, `modules` (T07), `threads` and `sync` (T08), ABI/lifecycle assertions | `wmain`, `RunSuite`, `CallPreserved`, `Stress`, `ProbeThreads`, `ProbeModules` |
| `scripts/windows` | Requirements check, build (Java, FrameworkRes, Fixtures, Host, NativeProbe) and test suites (Host = T01, Java = G1, Native = G2, Bionic = T07/T08, Modules = T07 only) | `build.ps1`, `test.ps1`, `Test-Bionic`, `Test-Modules`, `check-requirements.ps1`, `common.ps1` |
| `tests/windows-arm64/apk-fixtures` | Fixture APK sources: `hello-java` (T03), `hello-kotlin` (T04, G1.6), `hello-jni` (G1.5, later T09) | `MainActivity` of each |
| `tests/windows-arm64/guest-elf` | Guest ELF sources: `n0` (T05), `negative` (T06), `modules` (T07), `threads` (T08); build parameters, link dependencies and expected results/refusals | `fixtures.psd1`, `root_value`, `t08_case` |
| `internal/version` | Product version | `Version`, `Older` |
| `tools/checkdocs` | Documentation mirror check | `main` |
