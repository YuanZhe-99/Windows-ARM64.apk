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
| Mac.apk (git subtree under `macapk/`, at `3f35611`) | AGPL-3.0 (dual-licensed upstream) | DEX translation, Android Framework, loader and bridges; its own bundled components are listed in `macapk/THIRD_PARTY_NOTICES.txt` |
| zlib 1.3.2 (vcpkg) | zlib License (Jean-loup Gailly and Mark Adler; the notice ships with `z.dll`) | inflating APK entries in `winapk.exe`; ships as `z.dll` |
| SQLite 3.54.1 (vcpkg) | public domain | `android.database.sqlite` in `winapk-bridge.dll`; ships as `sqlite3.dll` |
| Capstone 5.0.9 (vcpkg) | BSD-3-Clause (Nguyen Anh Quynh and contributors; the notice ships with `capstone.dll`) | screening guest ARM64 code in the ELF loader (`native-probe.exe`, a development tool) |
| Android's `org.apache.http.legacy` (through Mac.apk) | Apache-2.0 (AOSP `platform/external/apache-http`, Apache HttpComponents' NOTICE) | Apache HttpClient for apps, inside `android-framework.jar` |
| ANGLE `801a1a5` | BSD-3-Clause (The ANGLE Project Authors); public DEPS retain their licenses | optional ARM64 host experiment; no released package yet; packaging must supply full linked-dependency notices |
| OpenJDK and AOSP resources | see `doc/en-us/third-party.md` | private JVM and framework resources |
| Skia | deferred to P8 | not built or shipped |

The Vulkan loader and GPU drivers are system components, never copied into the package.

## ANGLE license

Copyright 2018 The ANGLE Project Authors.
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:

* Redistributions of source code must retain the above copyright
  notice, this list of conditions and the following disclaimer.
* Redistributions in binary form must reproduce the above
  copyright notice, this list of conditions and the following
  disclaimer in the documentation and/or other materials provided
  with the distribution.
* Neither the name of TransGaming Inc., Google Inc., 3DLabs Inc.
  Ltd., nor the names of their contributors may be used to endorse
  or promote products derived from this software without specific
  prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.
