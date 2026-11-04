# Third-party code

Windows-ARM64.apk is licensed under AGPL-3.0. This inventory lists every upstream it reuses or
embeds, what it uses, and what that obliges. `THIRD_PARTY_NOTICES.md` holds the notices
themselves.

## Inventory

| Component | Repository and version | Used for | License | Modifications | Obligations |
|---|---|---|---|---|---|
| MyGo | `egoist/mygo`, latest tag | UI framework, packaging | MIT | none | keep the copyright notice; its embedded Kage compiler and MingCute icon are Apache-2.0 |
| Kyokai, Easytier-MyGo and Nextcloud-MyGo files | `YuanZhe-99/Kyokai`, `YuanZhe-99/Easytier-MyGo`, `YuanZhe-99/Nextcloud-MyGo` | `tools/checkdocs`, the `internal/version` pattern, the MyGo maintenance skill, the conventions; Kyokai is the design reference | AGPL-3.0 / GPL-3.0, relicensed under AGPL-3.0 by their author | adapted names and comments | none beyond Windows-ARM64.apk's own license |
| Microsoft Build of OpenJDK 21 | 21.0.13, `build-lock.json` | building the Java side; the JVM `winapk` embeds in development | GPL-2.0 with the Classpath Exception | none | not shipped yet; the private runtime of P7 ships with its license and notices |
| Vulkan-Headers | 1.4.363.0 via vcpkg, `build-lock.json` | compiling `winapk`'s Vulkan probe | Apache-2.0 | none | build time only; nothing ships |
| zlib | 1.3.2 via vcpkg, `build-lock.json` | inflating APK entries in `winapk` (`apk.cpp`) | zlib | none | ships as a DLL beside `winapk.exe`; keep its notice |
| AOSP `frameworks/base/core/res` | tag `android-16.0.0_r2`, `build-lock.json` | compiled into `bin/tools/framework-res.apk`, the platform resources apps resolve `@android:` against | Apache-2.0 | none (compiled with every feature flag read-only true, as upstream) | ships in the runtime; keep the license and NOTICE |
| Android SDK build-tools, platform and NDK | build-tools 36.0.0, android-36, NDK r30, `build-lock.json` | `aapt2` for the platform resources; building the fixture APKs and their libraries | Android Software Development Kit License | none | build time only; nothing ships, and the fixtures are generated, never committed |
| Mac.apk | `kksimp/Mac.apk`, latest `main`, git subtree under `macapk/` (at `ff2afbf`) | DEX translation, Android Framework, APK/resource parsing, loader and bridge code | AGPL-3.0 (upstream also offers a commercial license, not used here) | edited in place for Windows, recorded per commit | keep headers and notices; ship the complete corresponding source of `macapk/` with every release; its bundled components keep their own licenses (`macapk/THIRD_PARTY_NOTICES.txt`) |

## Trademarks

"Mac.apk" and its logo identify the official Mac.apk project and are not licensed with the code
(`TRADEMARKS.md` upstream): a fork must use a different name and icon, may say it is "based on
Mac.apk", and must not present itself as official or endorsed. Windows-ARM64.apk uses its own name
and icon accordingly. Android is a trademark of Google LLC.

## Planned components

ANGLE and Skia enter this inventory when a phase first builds with them. The Vulkan loader is Windows' own and never ships. Mac.apk's macOS prebuilts (ANGLE, MoltenVK, Skia `.dylib`) are never shipped in
a Windows package.
