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
| `macapk/runtime/platform/windows/src/bridge` | `winapk-bridge.dll`: Java host natives, window/input/presentation, SQLite and the process-private restricted guest namespace | `winapk_bridge_register`, `RegisterGuest`, `GuestRuntime`, `Open`, `BindExports`, `RunOnUi`, `PresentBitmap` |
| `macapk/runtime/java/com/macapk/loader/GuestNativeLoader.java` | Windows-only guest call-site service and app-class export-binding hook; host/JDK loading unchanged | `loadLibrary`, `load`, `defined`, `bind`, `openNative` |
| `macapk/runtime/guest/loader` | N0/P3 ELF graphs, logical providers, import versions and fault ranges; own-image hook lookup and namespace execution admission for JNI ([native-profiles.md](../native-profiles.md)) | `Load`, `ModuleSet::Open`, `ModuleSet::FindOwn`, `ModuleSet::Executable`, `BuiltinLibrary`, `GuestFault` |
| `macapk/runtime/guest/bionic` | Host thread/TLS, persistent host-thread bindings, sync, memory, file/time/dl/log and guest CRT lifetime; Android `format.c` and helper/provider ownership ([native-profiles.md](../native-profiles.md)) | `ThreadRuntime`, `ThreadBinding`, `GuestThreadContext`, `CxaFinalize`, `BionicLibraries::Load`, `vsnprintf` |
| `macapk/runtime/guest/jni` | P4 private tables, static PE entries with unwind/GFID metadata, persistent contexts, JNI library lifecycle and export binding for probes and restricted APKs ([native-profiles.md](../native-profiles.md)) | `JniBridge::OpenLibrary`, `InitializeLibrary`, `JniBridge::BindExports`, `Mangle`, `Register`, `winapk_jni_dispatch` |
| `macapk/runtime/platform/windows/src/native_probe` | `native-probe.exe`: `load`, `suite` (T05), `stress`, `modules`/`libraries` (T07), `threads`, `sync`, `memory`, `io`, `faults`, `format` (T08), ABI/lifecycle assertions | `wmain`, `RunSuite`, `CallPreserved`, `Stress`, `ProbeBionic`, `ProbeModules`, `ProbeLibraries` |
| `macapk/runtime/platform/windows/src/jni_probe.cpp` | Separate real-JVM T09 foundation and fatal boundaries (not G3 by itself); isolated loader, CheckJNI, live CFG/GFID admission and proxy cleanup | `wmain`, `Run`, `HostState`, `NativeEntryTargets`, `GuestJniProbe` |
| `scripts/windows` | Host/Java/G2/P3/T09/APK checks, native APK dependency recipes and running-process CFG evidence; targeted Bionic subsets available | `build.ps1`, `Build-Fixtures`, `Build-GuestBionic`, `Build-GuestJni`, `Test-Jni`, `Test-ApkJni`, `check-requirements.ps1` |
| `tests/windows-arm64/apk-fixtures` | Source-owned T03 Java, T04 Kotlin and T09/G3 JNI Activity matrix, dependency/version libraries, nested/worker callbacks and unsupported-JNI button | `MainActivity`, `WorkerCallbacks`, `JNI_OnLoad`, `nativeMatrix`, `nativeWorker`, `jni/fixtures.psd1` |
| `tests/windows-arm64/guest-elf` | Guest ELF sources: `n0` (T05), `negative` (T06), `modules` (T07), `threads` (T08), `jni` (T09 foundation); build parameters, dependencies and expected results/refusals | `fixtures.psd1`, `root_value`, `t08_case`, `t09_case` |
| `internal/version` | Product version | `Version`, `Older` |
| `tools/checkdocs` | Documentation mirror check | `main` |
