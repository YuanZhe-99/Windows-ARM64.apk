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
| `macapk/runtime/platform/windows` | The Windows ARM64 host: `winapk.exe` (`doctor`, `inspect`, `run`), its APK reader, JVM and Vulkan probes, `windows.cmake`, `vcpkg.json` | `wmain`, `Run`, `StageCode`, `InspectApk`, `FindJvm`, `CreateJvm`, `ProbeVulkan` |
| `macapk/runtime/platform/windows/src/bridge` | `winapk-bridge.dll`: the Windows implementations of the Java side's host natives — `NativeLog` (`log.cpp`) and `NativeCocoa`'s window, presentation and input (`window.cpp`) | `winapk_bridge_register`, `Bind`, `Init`, `RunLoop`, `Tick`, `PresentBitmap`, `Touch`, `Key` |
| `scripts/windows` | Requirements check, build (Java, FrameworkRes, Fixtures, Host) and test suites (Host = T01, Java = G1) | `build.ps1`, `test.ps1`, `check-requirements.ps1`, `common.ps1` |
| `tests/windows-arm64/apk-fixtures` | Fixture APK sources: `hello-java` (T03), `hello-jni` (G1.5, later T09) | `MainActivity` of each |
| `internal/version` | Product version | `Version`, `Older` |
| `tools/checkdocs` | Documentation mirror check | `main` |
