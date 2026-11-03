# Third-party code

Windows-ARM64.apk is licensed under AGPL-3.0. This inventory lists every upstream it reuses or
embeds, what it uses, and what that obliges. `THIRD_PARTY_NOTICES.md` holds the notices
themselves.

## Inventory

| Component | Repository and version | Used for | License | Modifications | Obligations |
|---|---|---|---|---|---|
| MyGo | `egoist/mygo`, latest tag | UI framework, packaging | MIT | none | keep the copyright notice; its embedded Kage compiler and MingCute icon are Apache-2.0 |
| Kyokai, Easytier-MyGo and Nextcloud-MyGo files | `YuanZhe-99/Kyokai`, `YuanZhe-99/Easytier-MyGo`, `YuanZhe-99/Nextcloud-MyGo` | `tools/checkdocs`, the `internal/version` pattern, the MyGo maintenance skill, the conventions; Kyokai is the design reference | AGPL-3.0 / GPL-3.0, relicensed under AGPL-3.0 by their author | adapted names and comments | none beyond Windows-ARM64.apk's own license |
| Mac.apk | `kksimp/Mac.apk`, latest `main`, git subtree under `macapk/` (imported at `367fd1a`) | DEX translation, Android Framework, APK/resource parsing, loader and bridge code | AGPL-3.0 (upstream also offers a commercial license, not used here) | edited in place for Windows, recorded per commit | keep headers and notices; ship the complete corresponding source of `macapk/` with every release; its bundled components keep their own licenses (`macapk/THIRD_PARTY_NOTICES.txt`) |

## Trademarks

"Mac.apk" and its logo identify the official Mac.apk project and are not licensed with the code
(`TRADEMARKS.md` upstream): a fork must use a different name and icon, may say it is "based on
Mac.apk", and must not present itself as official or endorsed. Windows-ARM64.apk uses its own name
and icon accordingly. Android is a trademark of Google LLC.

## Planned components

OpenJDK, the Android SDK/NDK and AOSP resources, ANGLE, the Vulkan Loader and Skia enter this
inventory with their exact versions and licenses when P0 chooses them and locks them in
`build-lock.json`. Mac.apk's macOS prebuilts (ANGLE, MoltenVK, Skia `.dylib`) are never shipped in
a Windows package.
