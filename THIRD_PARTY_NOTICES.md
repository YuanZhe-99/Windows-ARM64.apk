# Third-party notices

Windows-ARM64.apk is licensed under AGPL-3.0 (`LICENSE`). It includes or links the components
below. The inventory of reused code (repository, version, files, modifications, obligations) is
[doc/en-us/third-party.md](doc/en-us/third-party.md); release builds regenerate the full list of
Go, Java and native dependencies with their license texts.

## Go modules (front end)

| Component | License | Source |
|---|---|---|
| MyGo | MIT | https://github.com/egoist/mygo |
| purego (through MyGo) | Apache-2.0 | https://github.com/ebitengine/purego |
| go-text/typesetting (through MyGo) | Unlicense OR BSD-3-Clause | https://github.com/go-text/typesetting |
| golang.org/x/image, x/net, x/text | BSD-3-Clause | https://go.googlesource.com |
| Kage shader compiler, inside MyGo | Apache-2.0 | https://github.com/hajimehoshi/ebiten |
| MingCute icon, inside MyGo | Apache-2.0 | https://github.com/Richard9394/MingCute |

## Runtime

| Component | License | Used for |
|---|---|---|
| Mac.apk (git subtree under `macapk/`, imported in P0) | AGPL-3.0 (dual-licensed upstream) | DEX translation, Android Framework, loader and bridges; its own bundled components are listed in `macapk/THIRD_PARTY_NOTICES.txt` |
| OpenJDK, ANGLE, Vulkan Loader, Skia, AOSP resources | recorded when chosen in P0 | runtime of the APK process |
