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
| Mac.apk (git subtree under `macapk/`, at `8b45e48`) | AGPL-3.0 (dual-licensed upstream) | DEX translation, Android Framework, loader and bridges; its own bundled components are listed in `macapk/THIRD_PARTY_NOTICES.txt` |
| zlib 1.3.2 (vcpkg) | zlib License (Jean-loup Gailly and Mark Adler; the notice ships with `z.dll`) | inflating APK entries in `winapk.exe`; ships as `z.dll` |
| SQLite 3.54.1 (vcpkg) | public domain | `android.database.sqlite` in `winapk-bridge.dll`; ships as `sqlite3.dll` |
| Capstone 5.0.9 (vcpkg) | BSD-3-Clause (Nguyen Anh Quynh and contributors; the notice ships with `capstone.dll`) | screening guest ARM64 code in the ELF loader (`native-probe.exe`, a development tool) |
| Android's `org.apache.http.legacy` (through Mac.apk) | Apache-2.0 (AOSP `platform/external/apache-http`, Apache HttpComponents' NOTICE) | Apache HttpClient for apps, inside `android-framework.jar` |
| OpenJDK, ANGLE, Vulkan Loader, Skia, AOSP resources | recorded when chosen in P0 | runtime of the APK process |
