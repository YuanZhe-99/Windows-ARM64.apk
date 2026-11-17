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
| `macapk/runtime/guest/loader` | N0 ELF loader, P3 transactional dependencies/logical providers, validated import versions and executable-range faults ([native-profiles.md](../native-profiles.md)); no APK/JNI admission yet | `Load`, `ModuleSet`, `BuiltinLibrary`, `Loader::CheckVersions`, `ModuleSet::Open`, `TrackModule`, `GuestFault` |
| `macapk/runtime/guest/bionic` | Host thread/TLS, persistent host-thread bindings, sync, memory, file/time/dl/log and guest CRT lifetime; Android `format.c` and helper/provider ownership ([native-profiles.md](../native-profiles.md)) | `ThreadRuntime`, `ThreadBinding`, `GuestThreadContext`, `CxaFinalize`, `BionicLibraries::Load`, `vsnprintf` |
| `macapk/runtime/guest/jni` | P4 per-thread JNI proxies and persistent/reentrant Bionic bindings, method descriptors and Android-side varargs; standalone T09 only ([native-profiles.md](../native-profiles.md)) | `JniBridge`, `JniBridge::Call`, `ThreadBinding`, `Signature`, `winapk_jni_install` |
| `macapk/runtime/platform/windows/src/native_probe` | `native-probe.exe`: `load`, `suite` (T05), `stress`, `modules`/`libraries` (T07), `threads`, `sync`, `memory`, `io`, `faults`, `format` (T08), ABI/lifecycle assertions | `wmain`, `RunSuite`, `CallPreserved`, `Stress`, `ProbeBionic`, `ProbeModules`, `ProbeLibraries` |
| `macapk/runtime/platform/windows/src/jni_probe.cpp` | Separate real-JVM T09 JNI foundation and fatal boundary probe, not G3; isolated Java loader, CheckJNI, host-state and proxy cleanup assertions | `wmain`, `Run`, `HostState`, `GuestJniProbe` |
| `scripts/windows` | Requirements check, Java/host/guest-helper/fixture builds and tests (Host = T01, Java = G1, Native = G2, Bionic = T07/T08, Jni = T09 foundation; targeted Bionic subsets available) | `build.ps1`, `Build-GuestBionic`, `Build-GuestJni`, `test.ps1`, `Test-Bionic`, `Test-Jni`, `check-requirements.ps1` |
| `tests/windows-arm64/apk-fixtures` | Fixture APK sources: `hello-java` (T03), `hello-kotlin` (T04, G1.6), `hello-jni` (G1.5, later T09) | `MainActivity` of each |
| `tests/windows-arm64/guest-elf` | Guest ELF sources: `n0` (T05), `negative` (T06), `modules` (T07), `threads` (T08), `jni` (T09 foundation); build parameters, dependencies and expected results/refusals | `fixtures.psd1`, `root_value`, `t08_case`, `t09_case` |
| `internal/version` | Product version | `Version`, `Older` |
| `tools/checkdocs` | Documentation mirror check | `main` |
