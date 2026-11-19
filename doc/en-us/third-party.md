# Third-party code

Windows-ARM64.apk is licensed under AGPL-3.0. This inventory lists every upstream it reuses or
embeds, what it uses, and what that obliges. `THIRD_PARTY_NOTICES.md` holds the notices
themselves.

## Inventory

| Component | Repository and version | Used for | License | Modifications | Obligations |
|---|---|---|---|---|---|
| MyGo | `egoist/mygo`, latest tag | UI framework, packaging | MIT | none | keep the copyright notice; its embedded Kage compiler and MingCute icon are Apache-2.0 |
| Kyokai, Easytier-MyGo and Nextcloud-MyGo files | `YuanZhe-99/Kyokai`, `YuanZhe-99/Easytier-MyGo`, `YuanZhe-99/Nextcloud-MyGo` | `tools/checkdocs`, the `internal/version` pattern, the MyGo maintenance skill, the conventions; Kyokai is the design reference | AGPL-3.0 / GPL-3.0, relicensed under AGPL-3.0 by their author | adapted names and comments | none beyond Windows-ARM64.apk's own license |
| Microsoft Build of OpenJDK 21 | 21.0.13, `build-lock.json` | building the Java side; the JVM `winapk` embeds | GPL-2.0 with the Classpath Exception | none | external prerequisite for the local CLI package, not bundled |
| Vulkan-Headers | 1.4.363.0 via vcpkg, `build-lock.json` | compiling `winapk`'s Vulkan probe | Apache-2.0 | none | build time only; nothing ships |
| zlib | 1.3.2 via vcpkg, `build-lock.json` | inflating APK entries in `winapk` (`apk.cpp`) | zlib | none | ships as a DLL beside `winapk.exe`; keep its notice |
| liblzma (XZ Utils) | 5.8.4 via vcpkg, `build-lock.json` | `NativeOs.xzDecompress` for `.xz` resource streams (`host_services.cpp` in `winapk-bridge.dll`) | 0BSD | none | ships as `liblzma.dll` beside `winapk.exe`; keep its notice |
| SQLite | 3.54.1 via vcpkg, `build-lock.json` | `android.database.sqlite` through upstream's `NativeSqlite` (`native_sqlite.cpp` in `winapk-bridge.dll`) | public domain | none | ships as a DLL beside `winapk.exe`; no notice required |
| AOSP `frameworks/base/core/res` | tag `android-16.0.0_r2`, `build-lock.json` | compiled into `bin/tools/framework-res.apk`, the platform resources apps resolve `@android:` against | Apache-2.0 | none (compiled with every feature flag read-only true, as upstream) | ships in the runtime; keep the license and NOTICE |
| Android SDK build-tools, platform and NDK | build-tools 36.0.0, android-36, NDK r30, `build-lock.json` | `aapt2` for the platform resources; building the fixture APKs, their libraries and the guest ELF fixtures | Android Software Development Kit License | none | build time only; nothing ships, and the fixtures are generated, never committed |
| Capstone | 5.0.9 via vcpkg, `build-lock.json` | the guest loader's screen of ARM64 code (`elf_loader.cpp`) | BSD-3-Clause | none | local runtime ZIP contains `capstone.dll` and its notice, not the native probe |
| Android's `org.apache.http.legacy` | through Mac.apk (`macapk/thirdparty/apache-http-legacy`, from an Android 14 system image, AOSP `platform/external/apache-http`) | Apache HttpClient and Android's legacy HTTP client, packed into `android-framework.jar` as the shared library on a device | Apache-2.0 | none (converted by upstream's DEX converter) | ships in the runtime; keep the license and Apache HttpComponents' NOTICE |
| Kotlin compiler and standard library | 2.4.0, bundled with Android Studio, `build-lock.json` | compiling the Kotlin fixture T04; `kotlin-stdlib.jar` is dexed into that APK | Apache-2.0 | none | build time only; the fixture is generated, never committed or shipped |
| ANGLE and its public DEPS | `google/angle`, `801a1a5`, `build-lock.json` | optional P5a ARM64 EGL/GLES source build with Vulkan and D3D11 | BSD-3-Clause; linked GN targets (`abseil-cpp` Apache-2.0, `vulkan_memory_allocator` MIT, `volk`/`ceval` MIT, `xxhash` BSD-2-Clause, `astc-encoder` Apache-2.0, Khronos headers Apache-2.0/MIT, `zlib` Zlib, LLVM `libc++`/`libc++abi` Apache-2.0 WITH LLVM-exception) | pinned Chromium build dependency adapted to the locked SDK/custom VS, not renderer changes | linked GN dependency audit recorded in `THIRD_PARTY_NOTICES.md`; preserve notices whenever private ANGLE DLLs are staged |
| JetBrains Skiko / Skia | `org.jetbrains.skiko:skiko-awt-runtime-windows-arm64:0.150.2`, `build-lock.json` | P8 AGSL `RuntimeShader` and `RenderEffect` GPU post-processing over Direct3D 12 (`NativeSkiaD3D12` + `skiko-windows-arm64.dll`, T15) | Apache-2.0 (Skiko), BSD-3-Clause (Skia), Unicode-DFS-2016 (`icudtl.dat`) | none (locked Maven Central artifact verified by SHA-256 and `0xAA64` PE check) | keep Apache-2.0, Skia BSD-3-Clause and Unicode notices when `skiko-windows-arm64.dll` ships |
| Mac.apk | `kksimp/Mac.apk`, latest `main`, git subtree under `macapk/` (at `3f35611`) | DEX translation, Android Framework, APK/resource parsing, loader and bridge code | AGPL-3.0 (upstream also offers a commercial license, not used here) | edited in place for Windows, recorded per commit | keep headers and notices; ship the complete corresponding source of `macapk/` with every release; its bundled components keep their own licenses (`macapk/THIRD_PARTY_NOTICES.txt`) |

## Trademarks

"Mac.apk" and its logo identify the official Mac.apk project and are not licensed with the code
(`TRADEMARKS.md` upstream): a fork must use a different name and icon, may say it is "based on
Mac.apk", and must not present itself as official or endorsed. Windows-ARM64.apk uses its own name
and icon accordingly. Android is a trademark of Google LLC.

## Planned components

The local CLI package depends on the external ARM64 Microsoft Visual C++ v14 Redistributable.
It includes the checked-in notices, vcpkg notices, the AOSP resource notice and a separate
source snapshot. It is not a public redistribution audit or a release.

Skia enters this inventory when a phase first builds with it. The Vulkan loader is Windows' own and never ships. Mac.apk's macOS prebuilts (ANGLE, MoltenVK, Skia `.dylib`) are never shipped in
a Windows package.
