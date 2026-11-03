# Platform notes

| Platform | Status |
|---|---|
| Windows 11 ARM64 | Scaffold only: the Go gate passes (below). No runtime, no front end yet. |
| Windows 11 x64 | Not a target; may host build tools and parsing tests only. |
| macOS (Apple Silicon) | Upstream Mac.apk's platform; its regression status is recorded once the subtree is imported. |

## Windows

Verified on Windows 11 ARM64 (2026-11-03; Go 1.27.3):

| Check | Result |
|---|---|
| Go module | `go vet ./...` and `go test ./...` pass |
| Documentation mirror | `go run ./tools/checkdocs` passes |

## Gates

| Gate | Status |
|---|---|
| G0 host ready | NOT_RUN |
| G1 Java MVP | NOT_RUN |
| G2 native minimal execution | NOT_RUN |
| G3 native integration | NOT_RUN |
| G4 graphics | NOT_RUN |
| G5 experimental release | NOT_RUN |

## Known limitations

- Mac.apk is not imported yet; no runtime code exists.
- No native ARM64 claim can be made before the runtime runs on ARM64 hardware.
