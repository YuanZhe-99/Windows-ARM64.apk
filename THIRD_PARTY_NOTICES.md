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
| Mac.apk (git subtree under `macapk/`, at `154f64e`) | AGPL-3.0 (dual-licensed upstream) | DEX translation, Android Framework, loader and bridges; its own bundled components are listed in `macapk/THIRD_PARTY_NOTICES.txt` |
| zlib 1.3.2 (vcpkg) | zlib License (Jean-loup Gailly and Mark Adler; the notice ships with `z.dll`) | inflating APK entries in `winapk.exe`; ships as `z.dll` |
| liblzma / XZ Utils 5.8.4 (vcpkg) | BSD Zero Clause (`0BSD`) | `NativeOs.xzDecompress` in `winapk-bridge.dll`; ships as `liblzma.dll` |
| SQLite 3.54.1 (vcpkg) | public domain | `android.database.sqlite` in `winapk-bridge.dll`; ships as `sqlite3.dll` |
| Capstone 5.0.9 (vcpkg) | BSD-3-Clause (Nguyen Anh Quynh and contributors; the notice ships with `capstone.dll`) | screening guest ARM64 code in the ELF loader (`native-probe.exe`, a development tool) |
| Android's `org.apache.http.legacy` (through Mac.apk) | Apache-2.0 (AOSP `platform/external/apache-http`, Apache HttpComponents' NOTICE) | Apache HttpClient for apps, inside `android-framework.jar` |
| ANGLE `801a1a5` | BSD-3-Clause (The ANGLE Project Authors); public DEPS retain their licenses | private ARM64 EGL/GLES runtime (`bin\angle\libEGL.dll`, `libGLESv2.dll`, `d3dcompiler_47.dll`); linked-dependency notices below |
| OpenJDK and AOSP resources | see `doc/en-us/third-party.md` | private JVM and framework resources |
| JetBrains Skiko 0.150.2 (`skiko-windows-arm64.dll` + `icudtl.dat`) | Apache-2.0 (JetBrains s.r.o.), BSD-3-Clause (Google LLC Skia), Unicode/ICU | AGSL `RuntimeShader` and `RenderEffect` GPU post-processing over Direct3D 12 (`T15`) |

The Vulkan loader and GPU drivers are system components, never copied into the package.

## ANGLE and linked GN dependencies

ANGLE (`801a1a5f6e00ed0028937067b7663fd7053bff31`, built with `scripts/windows/angle-args.gn`:
`angle_enable_d3d11=true`, `angle_enable_vulkan=true`, `angle_enable_cl=false`,
`angle_enable_swiftshader=false`, `angle_use_custom_libvulkan=false`, `is_component_build=false`)
links only the permissive libraries below:

| Linked target / directory | Revision | License |
|---|---|---|
| ANGLE (`src/libANGLE`, `src/libEGL`, `src/libGLESv2`, `src/common/base`) | `801a1a5f6e00ed0028937067b7663fd7053bff31` | BSD-3-Clause |
| `src/common/third_party/xxhash` | bundled in ANGLE `801a1a5` | BSD-2-Clause (Yann Collet) |
| `src/third_party/volk` & `src/third_party/ceval` | bundled in ANGLE `801a1a5` | MIT (Arseny Kapoulkine / ANGLE contributors) |
| `third_party/abseil-cpp` (`angle_abseil`) | `ecb00cc74a6251b273c78789f947d3eb280dd093` | Apache-2.0 |
| `third_party/vulkan_memory_allocator` (`angle_vk_mem_alloc_wrapper`) | `7e55b011e16182fc349149abbd3aaf3b1db46421` | MIT (Advanced Micro Devices, Inc.) |
| `third_party/astc-encoder/src` (`angle_image_util`) | `2319d9c4d4af53a7fc7c52985e264ce6e8a02a9b` | Apache-2.0 (ARM Limited) |
| `third_party/vulkan-headers/src`, `spirv-headers/src`, `EGL-Registry`, `OpenGL-Registry` | `b26454f3` / `5c6004e7` / `7dea2ed7` / `9cb90ca4` | Apache-2.0 / MIT (The Khronos Group Inc.) |
| `third_party/zlib` (`angle_compression`) | `ec453dc9abac1ddf39ced752e72a7a80dcda1d91` | Zlib (Jean-loup Gailly and Mark Adler) + Chromium BSD-3-Clause |
| `third_party/libc++/src` & `libc++abi/src` (statically linked C++ runtime) | `64f64c14` / `8e42446d` | Apache-2.0 WITH LLVM-exception |

SwiftShader, the Khronos Vulkan Loader, Vulkan Validation Layers, Dawn, and `clspv` are disabled in
`scripts/windows/angle-args.gn` and are never linked into `libEGL.dll` or `libGLESv2.dll`.

### ANGLE BSD-3-Clause notice

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

### VulkanMemoryAllocator MIT notice

Copyright (c) 2017-2026 Advanced Micro Devices, Inc. All rights reserved.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.

### volk MIT notice

Copyright (c) 2018-2024 Arseny Kapoulkine. Permission is hereby granted, free of charge,
to any person obtaining a copy of this software and associated documentation files to deal
in the Software without restriction, subject to inclusion of the copyright and permission notice.

### xxHash BSD-2-Clause notice

Copyright (c) 2012-2021 Yann Collet. All rights reserved. Redistribution and use in source and
binary forms, with or without modification, are permitted provided that source and binary
redistributions retain the above copyright notice, conditions and disclaimer.
