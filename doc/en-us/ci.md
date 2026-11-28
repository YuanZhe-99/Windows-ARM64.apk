# Continuous integration and releases

GitHub Actions workflows under `.github/workflows/` build, verify and package
**Windows-ARM64.apk** natively on GitHub's Windows 11 ARM64 runner (`windows-11-arm`).

## Workflows

| Workflow | Trigger | Runner | Purpose |
|---|---|---|---|
| `ci.yml` | Every `push`, `pull_request`, `workflow_dispatch` | `windows-11-arm` | Runs Go/MyGo/i18n/docs checks, builds the native ARM64 runtime + Java/Skia/ELF targets, and runs the headless native ARM64 suites |
| `build.yml` | Reusable (`workflow_call`) | `windows-11-arm` | Shared build, verification and optional release-artifact staging |
| `release.yml` | `v*` tags | `ubuntu-latest` + `windows-11-arm` | Verifies tag/version parity, builds and signs release packages on `windows-11-arm`, computes `SHA256SUMS`, and publishes the GitHub Release |

## Verification tiers

GitHub's `windows-11-arm` runner executes native ARM64 processes without an interactive desktop GPU
session, so verification splits cleanly between CI and physical Windows 11 ARM64 hardware:

| Check / Suite | `windows-11-arm` CI | Physical ARM64 desktop + GPU |
|---|---|---|
| `gofmt`, `go vet`, `go test ./internal/version ./internal/gui`, `CGO_ENABLED=0`, `mygo vet`, `mygo i18n check`, `checkdocs` | Runs on every push/PR | Runs before release |
| `Build-Java`, `Build-FrameworkRes`, `Build-Angle`, `Build-Skia`, `Build-Fixtures`, `Build-Host` (`Debug` & `Release`), `Build-GuestBionic`, `Build-GuestJni`, `Build-ElfFixtures` | Runs on every push/PR | Runs before release |
| `Host`, `Native`, `Bionic`, `Jni`, `DexJob` | Runs natively on `windows-11-arm` | Runs before release |
| `HostServices`, `Audio` (hardware WASAPI), `Agsl` (`T15` D3D12), `Java`, `ApkJni`, `Surface`, `GraphicsHost`, `GraphicsJava`, `GraphicsGuest`, `VulkanWsi`, `Package`, `Stability` (`T14`) | Artifact build & PE audit (`Package` inventory) | Full interactive window, GPU & 30-min stability gates ([platform-notes.md](platform-notes.md)) |

## Release artifacts and Ed25519 update signing

When a tag `vX.Y.Z` (matching `internal/version/version.go` and `mygo.json`) is pushed to
`YuanZhe-99/Windows-ARM64.apk`, `release.yml` publishes:

- `windows-arm64-apk-setup-<version>-arm64.exe` — per-user NSIS installer containing both
  `Windows-ARM64.apk.exe` and the complete Release `winapk` runtime (`bin/`, `lib/`, `licenses/`,
  `LICENSE`, `THIRD_PARTY_NOTICES.md`), staged through `scripts/windows/stage-app-resources.ps1`.
- `Windows-ARM64.apk-<version>-arm64.zip` — portable unpacked Windows 11 ARM64 bundle from
  `scripts/windows/package.ps1`.
- `Windows-ARM64.apk-<version>-arm64-source.zip` — complete corresponding AGPL-3.0 source snapshot
  (excluding upstream macOS `.dylib` prebuilts).
- `windows-arm64-apk-<version>-windows-arm64.tar.gz` and `update-windows-arm64.json` —
  Ed25519-signed update archive and manifest produced by `go tool mygo build` when the repository
  secret `MYGO_UPDATER_PRIVATE_KEY` is configured. Installed copies verify signatures against
  `updates.publicKey` in `mygo.json` when the user checks for updates in **Settings → About →
  Updates**.
- `SHA256SUMS` — SHA-256 digests of every published asset.
