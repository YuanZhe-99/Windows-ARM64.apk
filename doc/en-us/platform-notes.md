# Platform notes

| Platform | Status |
|---|---|
| Windows 11 ARM64 | G0 passes: the Java side builds, and `winapk.exe` embeds the ARM64 JVM (T01). The interface runs against sample data (`winapk.Mock`); no APK runs yet. |
| Windows 11 x64 | Not a target; may host build tools and parsing tests only. |
| macOS (Apple Silicon) | Upstream Mac.apk's platform; imported as `macapk/` at `367fd1a`; no macOS build or smoke run here yet (`NOT_RUN`). |

## Windows

Verified on Windows 11 ARM64 (2026-11-03): build 26300.9711, Snapdragon Compute Platform (8cx Gen 3), 32 GB, Qualcomm Adreno 8cx Gen 3 (driver 30.0.4122.4000), Vulkan loader 1.3.301 with Mesa Dozen devices only; Go 1.27.3, Microsoft OpenJDK 21.0.13 ARM64, clang-cl 22.1.3.

| Check | Result |
|---|---|
| Go module | `go vet ./...` and `go test ./...` pass |
| Documentation mirror | `go run ./tools/checkdocs` passes |
| Requirements | `scripts/windows/check-requirements.ps1`: every locked tool OK; Android SDK and NDK not installed (needed from P1/P2) |
| Java side (Mac.apk `ff2afbf`) | `build.ps1 -Component Java`: compiles unchanged, 78 s, no error ([port-audit.md](port-audit.md)) |
| T01 host and JVM | `test.ps1 -Suite Host`: 17 checks PASS — `winapk.exe` ARM64; JVM `jvm.dll` ARM64 created in-process with upstream's module options; JNI host → Java → host; Java2D; 260 font families; patch modules; and the negative cases (no JVM found, an invalid `--java-home`) |
| Vulkan | loader ARM64 available; two Mesa Dozen devices (Vulkan 1.2 on Direct3D 12); `doctor --present`: Win32 surface test passed. No vendor Vulkan driver |
| Interface | `go tool mygo vet .` and `go tool mygo i18n check` pass; every screen of `design/` renders in `ui.Tester` in both languages and appearances; a pane dragged with the pointer in `ui.Tester` splits its target; the native window opens and stays up (smoke run, sample data) |

## Gates

| Gate | Status |
|---|---|
| G0 host ready | PASS (2026-11-03, this machine: T01, toolchain locked) |
| G1 Java MVP | NOT_RUN |
| G2 native minimal execution | NOT_RUN |
| G3 native integration | NOT_RUN |
| G4 graphics | NOT_RUN |
| G5 experimental release | NOT_RUN |

## Known limitations

- Mac.apk's only Windows change so far is the `if(WIN32)` branch of `runtime/CMakeLists.txt`;
  the Windows host is `doctor` alone (no APK runs before P1).
- This PC has no vendor Vulkan driver: Vulkan is Mesa Dozen on Direct3D 12 ([decisions.md](decisions.md)).
- macOS build after the `CMakeLists.txt` change: NOT_RUN (no Mac here); the branch returns only under `WIN32`.
- The interface shows sample data only. Not done until the runtime contract has them: the app's surface in a pane or window, moving the App data folder's contents, data and converted-code sizes, and updating the private Java runtime ([gui.md](gui.md)).
- No native ARM64 claim can be made before the runtime runs on ARM64 hardware.
