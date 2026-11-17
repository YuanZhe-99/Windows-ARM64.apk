<#
.SYNOPSIS
Builds Windows-ARM64.apk's runtime parts on Windows 11 ARM64.

.DESCRIPTION
Component Java builds Mac.apk's Java side (the Android framework, the boot
loader and the DEX converter) the way macapk/runtime/scripts/build.sh does:
one javac pass over macapk/runtime/java, android-framework.jar (everything but
com/macapk) and macapk-loader.jar (com/macapk), the bundled fonts, the
java.base and jdk.unsupported patch modules, the net_spi resolver provider and
bcprov. Component Host builds the native host (winapk.exe, native-probe.exe) and its
Java probe with CMake, Ninja and clang-cl from Visual Studio, for ARM64. Component
NativeProbe builds native-probe.exe, the guest formatting helper and ELF fixtures (T05–T08)
with the NDK.
JniProbe builds the real-JVM JNI probe, Android JNI/Bionic helpers and T09 fixture recipes.
Angle builds pinned ARM64 ANGLE DLLs; AngleProbe builds only the optional host experiment.

Output goes to build/windows-arm64/ (lib/ and bin/). Nothing outside the
repository is changed, and nothing is installed.

.PARAMETER Component
Java, FrameworkRes, Fixtures (the APKs), Host, NativeProbe, JniProbe, Angle, AngleProbe, or All.

.PARAMETER Configuration
Debug or Release (Host only).
#>
[CmdletBinding()]
param(
    [ValidateSet('Java', 'FrameworkRes', 'Fixtures', 'Host', 'NativeProbe', 'JniProbe', 'Angle', 'AngleProbe', 'All')]
    [string]$Component = 'All',
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Debug'
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
. "$PSScriptRoot\common.ps1"

$repo = Get-RepoRoot
$out = Join-Path $repo 'build\windows-arm64'
$rt = Join-Path $repo 'macapk\runtime'

<# Builds the Java side into $out\lib. #>
function Build-Java {
    $jdk = Find-Jdk
    $javac = Join-Path $jdk 'bin\javac.exe'
    $jar = Join-Path $jdk 'bin\jar.exe'
    $lib = Join-Path $out 'lib'
    $classes = Join-Path $out 'classes'
    # Wiped every time, as upstream does: a class whose source went away must
    # not stay in the jars.
    Reset-Dir $classes
    New-Item -ItemType Directory -Force $lib | Out-Null

    Write-Step "Compiling Mac.apk's Java sources with $jdk"
    # Upstream skips sync-conflict copies ("Foo 2.java").
    $sources = Get-ChildItem (Join-Path $rt 'java') -Recurse -Filter *.java |
        Where-Object { $_.Name -notmatch ' [2-9]\.java$' } |
        Sort-Object FullName | ForEach-Object { Quote-ArgFile $_.FullName }
    $list = Join-Path $out '.java.list'
    Set-Content -Path $list -Value $sources -Encoding utf8NoBOM
    # Android's org.apache.http.legacy (macapk/thirdparty/apache-http-legacy): on
    # the classpath so the framework's own org.apache.http slice compiles against
    # it, then unpacked beside the classes (never over one javac wrote, nor
    # META-INF) so it lands in android-framework.jar, as build.sh does.
    $legacy = Join-Path $repo 'macapk\thirdparty\apache-http-legacy\org.apache.http.legacy.jvm.jar'
    $cp = if (Test-Path $legacy) { @('-cp', $legacy) } else { @() }
    Invoke-Checked $javac (@('-d', $classes, '--release', '21', '-encoding', 'UTF-8', '-Xlint:-options') + $cp + @("@$list"))
    if (Test-Path $legacy) {
        Add-Type -AssemblyName System.IO.Compression.FileSystem
        $zip = [IO.Compression.ZipFile]::OpenRead($legacy)
        try {
            foreach ($e in $zip.Entries) {
                if ($e.FullName.StartsWith('META-INF/') -or $e.FullName.EndsWith('/')) { continue }
                $dest = Join-Path $classes $e.FullName
                if (Test-Path $dest) { continue }
                New-Item -ItemType Directory -Force (Split-Path $dest) | Out-Null
                [IO.Compression.ZipFileExtensions]::ExtractToFile($e, $dest)
            }
        } finally { $zip.Dispose() }
    }

    Write-Step 'Packaging android-framework.jar'
    Push-Location $classes
    try {
        $fw = Get-ChildItem -Recurse -File -Filter *.class |
            ForEach-Object { [IO.Path]::GetRelativePath($classes, $_.FullName) } |
            Where-Object { -not $_.StartsWith('com\macapk\') } | ForEach-Object { Quote-ArgFile $_ }
        $fwList = Join-Path $out '.framework.list'
        Set-Content -Path $fwList -Value $fw -Encoding utf8NoBOM
        Invoke-Checked $jar @('--create', '--file', (Join-Path $lib 'android-framework.jar'), "@$fwList")
        # Every .ttf in runtime/fonts goes into the framework jar under fonts/.
        $fonts = Get-ChildItem (Join-Path $rt 'fonts') -Filter *.ttf -ErrorAction SilentlyContinue
        if ($fonts) {
            $fontDir = Join-Path $classes 'fonts'
            New-Item -ItemType Directory -Force $fontDir | Out-Null
            $fonts | Copy-Item -Destination $fontDir -Force
            Invoke-Checked $jar @('--update', '--file', (Join-Path $lib 'android-framework.jar'), 'fonts')
        }
    } finally { Pop-Location }

    # Copies of JDK classes, valid only on the JDK they came from: compiled
    # beside and swapped in, so a failed compile leaves the old ones.
    foreach ($patch in @(@{ Dir = 'java_base_patch'; Module = 'java.base' }, @{ Dir = 'jdk_unsupported_patch'; Module = 'jdk.unsupported' })) {
        $src = Join-Path $rt $patch.Dir
        if (-not (Test-Path $src)) { continue }
        Write-Step "Compiling $($patch.Dir) (--patch-module $($patch.Module))"
        $next = Join-Path $lib ".$($patch.Dir).new"
        Reset-Dir $next
        $files = Get-ChildItem $src -Recurse -Filter *.java | ForEach-Object FullName
        Invoke-Checked $javac (@('--patch-module', "$($patch.Module)=$src", '-encoding', 'UTF-8', '-d', $next) + $files)
        $dest = Join-Path $lib $patch.Dir
        if (Test-Path $dest) { Remove-Item -LiteralPath $dest -Recurse -Force }
        Move-Item -LiteralPath $next -Destination $dest
    }

    # The DNS resolver provider (JDK 18+ API), compiled apart from the
    # --release 21 pass, with its ServiceLoader manifest.
    $netSpi = Join-Path $rt 'net_spi'
    if (Test-Path $netSpi) {
        $files = Get-ChildItem $netSpi -Recurse -Filter *.java | Where-Object { $_.Name -notmatch ' [2-9]\.java$' } | ForEach-Object FullName
        if ($files) {
            Write-Step 'Compiling net_spi'
            Invoke-Checked $javac (@('-d', $classes, '-encoding', 'UTF-8', '-Xlint:-options') + $files)
        }
        $services = Join-Path $netSpi 'META-INF\services'
        if (Test-Path $services) {
            $dest = Join-Path $classes 'META-INF\services'
            New-Item -ItemType Directory -Force $dest | Out-Null
            Copy-Item (Join-Path $services '*') $dest -Force
        }
    }
    $keybinds = Join-Path $rt 'java\com\macapk\loader\keybinds'
    if (Test-Path $keybinds) {
        $dest = Join-Path $classes 'com\macapk\loader\keybinds'
        New-Item -ItemType Directory -Force $dest | Out-Null
        Copy-Item (Join-Path $keybinds '*') $dest -Force
    }

    Write-Step 'Packaging macapk-loader.jar'
    Push-Location $classes
    try {
        $loader = Join-Path $lib 'macapk-loader.jar'
        Invoke-Checked $jar @('--create', '--file', $loader, 'com/macapk')
        if (Test-Path 'META-INF\services') { Invoke-Checked $jar @('--update', '--file', $loader, 'META-INF/services') }
    } finally { Pop-Location }

    $bc = Join-Path $rt 'thirdparty\bcprov-jdk18on-1.79.jar'
    if (Test-Path $bc) { Copy-Item $bc $lib -Force }
    Write-Step "Java side built: $lib"
}

<#
Builds the platform resource package, bin\tools\framework-res.apk, from AOSP's
frameworks/base/core/res at the tag build-lock.json names: the recipe of
macapk/scripts/build_framework_res.sh (aapt2 compile with every feature flag
read-only true, then link with the flags of core/res/Android.bp).
Notes: built once and kept; the source archive and the compiled resources are
cached under build\windows-arm64\framework-res. The archive comes from
android.googlesource.com, whose archives are generated on request, so it is
checked by its content (the Apache-2.0 marker and the manifest), and the
resulting package is hashed and recorded in platform-notes.md.
#>
function Build-FrameworkRes {
    $tools = Join-Path $out 'bin\tools'
    $apk = Join-Path $tools 'framework-res.apk'
    if (Test-Path $apk) { Write-Step "framework-res.apk already built: $apk"; return }
    $sdk = Find-AndroidSdk
    $aapt2 = Join-Path $sdk.BuildTools 'aapt2.exe'
    $lock = Get-Content (Join-Path $repo 'build-lock.json') -Raw | ConvertFrom-Json
    if ($lock.guest.frameworkRes.source -notmatch 'tag (\S+), API (\d+)') { throw 'build-lock.json: guest.frameworkRes.source names no tag and API' }
    $tag, $api = $Matches[1], $Matches[2]
    $versionName = if ($tag -match '^android-(\d+)\.') { $Matches[1] } else { $api }
    $work = Join-Path $out "framework-res\work-$tag"
    $src = Join-Path $work 'src'
    New-Item -ItemType Directory -Force $work, $tools | Out-Null

    if (-not (Test-Path (Join-Path $src 'AndroidManifest.xml'))) {
        Write-Step "Fetching frameworks/base/core/res at $tag"
        $tarball = Join-Path $work 'core-res.tar.gz'
        Invoke-Checked 'curl.exe' @('-sSfL', '-o', $tarball, "https://android.googlesource.com/platform/frameworks/base/+archive/refs/tags/$tag/core/res.tar.gz")
        Reset-Dir $src
        Invoke-Checked "$env:SystemRoot\System32\tar.exe" @('-xzf', $tarball, '-C', $src)
        if (-not (Test-Path (Join-Path $src 'MODULE_LICENSE_APACHE2'))) { throw 'the archive did not unpack to a core/res tree (no MODULE_LICENSE_APACHE2)' }
    }

    # Every featureFlag the sources name, all read-only true, as upstream.
    $flags = Get-ChildItem (Join-Path $src 'res') -Recurse -Filter *.xml | Select-String -Pattern 'featureFlag="!?([^"]+)"' -AllMatches |
        ForEach-Object { $_.Matches } | ForEach-Object { $_.Groups[1].Value }
    $flags += Select-String -Path (Join-Path $src 'AndroidManifest.xml') -Pattern 'featureFlag="!?([^"]+)"' -AllMatches |
        ForEach-Object { $_.Matches } | ForEach-Object { $_.Groups[1].Value }
    $flagArg = (($flags | Sort-Object -Unique) | ForEach-Object { "${_}:ro=true" }) -join ','
    $flagArgs = if ($flagArg) { @('--feature-flags', $flagArg) } else { @() }
    Write-Step "$(($flags | Sort-Object -Unique).Count) feature flags (all :ro=true)"

    $compiled = Join-Path $work 'compiled.zip'
    if (-not (Test-Path $compiled)) {
        Write-Step 'aapt2 compile'
        Invoke-Checked $aapt2 (@('compile', '--dir', (Join-Path $src 'res'), '--pseudo-localize') + $flagArgs + @('-o', $compiled))
    }
    Write-Step 'aapt2 link'
    $next = "$apk.new"
    Invoke-Checked $aapt2 (@('link', '-o', $next) + $flagArgs + @(
        '--manifest', (Join-Path $src 'AndroidManifest.xml'), '-A', (Join-Path $src 'assets'),
        '--private-symbols', 'com.android.internal', '--no-auto-version', '--auto-add-overlay',
        '--min-sdk-version', $api, '--target-sdk-version', $api,
        '--version-code', $api, '--version-name', $versionName, $compiled))
    Move-Item -LiteralPath $next -Destination $apk -Force
    $hash = (Get-FileHash $apk -Algorithm SHA256).Hash.ToLower()
    Write-Step "framework-res.apk built from AOSP $tag (API $api, Apache-2.0): $([math]::Round((Get-Item $apk).Length / 1MB, 1)) MB, sha256 $hash"
}

<#
Builds the fixture APKs from tests\windows-arm64\apk-fixtures into tests\fixtures
(generated, never committed): aapt2 compile and link against the locked
android.jar, javac, d8, zipalign and apksigner.
Notes: each APK is signed with a debug key made under build\ on first use (never
committed); its classes.dex is reproducible from the source and is hashed into
tests\fixtures\<name>.json with the build parameters (AGENTS.md: every fixture
has source, build parameters, a hash and expected output).
#>
function Build-Fixtures {
    $sdk = Find-AndroidSdk
    $jdk = Find-Jdk
    $env:JAVA_HOME = $jdk
    $bt = $sdk.BuildTools
    $lock = Get-Content (Join-Path $repo 'build-lock.json') -Raw | ConvertFrom-Json
    $fixtures = Join-Path $repo 'tests\fixtures'
    New-Item -ItemType Directory -Force $fixtures | Out-Null
    $key = Join-Path $out 'fixtures\debug.keystore'
    if (-not (Test-Path $key)) {
        New-Item -ItemType Directory -Force (Split-Path $key) | Out-Null
        Write-Step 'Making the fixture debug key'
        Invoke-Checked (Join-Path $jdk 'bin\keytool.exe') @('-genkeypair', '-keystore', $key, '-storepass', 'android', '-keypass', 'android',
            '-alias', 'fixture', '-keyalg', 'RSA', '-keysize', '2048', '-validity', '10000', '-dname', 'CN=Windows-ARM64.apk fixture')
    }
    foreach ($dir in Get-ChildItem (Join-Path $repo 'tests\windows-arm64\apk-fixtures') -Directory) {
        $name = $dir.Name
        Write-Step "Building fixture $name.apk"
        $work = Join-Path $out "fixtures\$name"
        Reset-Dir $work
        $gen = Join-Path $work 'gen'; $classes = Join-Path $work 'classes'; $dex = Join-Path $work 'dex'
        New-Item -ItemType Directory -Force $gen, $classes, $dex | Out-Null
        $res = Join-Path $work 'res.zip'
        Invoke-Checked (Join-Path $bt 'aapt2.exe') @('compile', '--dir', (Join-Path $dir.FullName 'res'), '-o', $res)
        $unsigned = Join-Path $work 'unsigned.apk'
        Invoke-Checked (Join-Path $bt 'aapt2.exe') @('link', '-o', $unsigned, '-I', $sdk.AndroidJar,
            '--manifest', (Join-Path $dir.FullName 'AndroidManifest.xml'), '--java', $gen, $res)
        $sources = @(Get-ChildItem (Join-Path $dir.FullName 'src'), $gen -Recurse -Filter *.java | ForEach-Object FullName)
        Invoke-Checked (Join-Path $jdk 'bin\javac.exe') (@('-d', $classes, '-source', '8', '-target', '8', '-encoding', 'UTF-8',
            '-bootclasspath', $sdk.AndroidJar, '-Xlint:-options') + $sources)
        # src/**/*.kt: compiled after the Java (R included) against it; the Kotlin standard
        # library goes into the DEX with the app's classes, as a Gradle build does.
        $kt = @(Get-ChildItem (Join-Path $dir.FullName 'src') -Recurse -Filter *.kt | ForEach-Object FullName)
        $stdlib = @()
        if ($kt) {
            $kotlinc = Find-Kotlinc
            Invoke-Checked (Join-Path $kotlinc 'bin\kotlinc.bat') (@('-jvm-target', '1.8', '-no-reflect',
                '-classpath', "$($sdk.AndroidJar);$classes", '-d', $classes) + $kt)
            $stdlib = @(Join-Path $kotlinc 'lib\kotlin-stdlib.jar')
        }
        $classFiles = @(Get-ChildItem $classes -Recurse -Filter *.class | ForEach-Object FullName)
        $d8 = & (Join-Path $bt 'd8.bat') @(@('--release', '--min-api', '24', '--lib', $sdk.AndroidJar, '--output', $dex) + $classFiles + $stdlib) 2>&1
        if ($LASTEXITCODE -ne 0) { $d8 | Write-Host; throw "d8.bat exited with $LASTEXITCODE" }
        # d8 36.0.0 rewrites Kotlin metadata up to 2.2 and reports every newer kotlin-stdlib 2.4
        # class it leaves as it is, with stack traces (about a megabyte); the DEX code is
        # unaffected. So a Kotlin fixture's d8 output is shown only when d8 fails.
        if ($kt) { Write-Step "d8: $(@($d8 | Where-Object { "$_" -match 'kotlin metadata' }).Count) notes on Kotlin metadata newer than it rewrites (code unaffected)" }
        else { $d8 | Write-Host }
        # classes.dex goes in beside the resources, stored as the platform expects it.
        Add-Type -AssemblyName System.IO.Compression, System.IO.Compression.FileSystem
        $zip = [IO.Compression.ZipFile]::Open($unsigned, 'Update')
        try { [IO.Compression.ZipFileExtensions]::CreateEntryFromFile($zip, (Join-Path $dex 'classes.dex'), 'classes.dex', 'Optimal') | Out-Null }
        finally { $zip.Dispose() }
        # jni/*.c: one arm64-v8a library each, built with the locked NDK for the APK's minSdk.
        $jni = Join-Path $dir.FullName 'jni'
        $libs = @()
        if (Test-Path $jni) {
            $clang = Find-AndroidNdk
            [xml]$manifest = Get-Content (Join-Path $dir.FullName 'AndroidManifest.xml') -Raw
            $api = $manifest.manifest.'uses-sdk'.GetAttribute('minSdkVersion', 'http://schemas.android.com/apk/res/android')
            $recipes = if (Test-Path (Join-Path $jni 'fixtures.psd1')) {
                (Import-PowerShellDataFile (Join-Path $jni 'fixtures.psd1')).Fixtures
            } else {
                @(Get-ChildItem $jni -Filter *.c | ForEach-Object {
                    @{ Source = $_.Name; Name = "lib$($_.BaseName).so"; Links = @() }
                })
            }
            foreach ($recipe in $recipes) {
                $so = Join-Path $work $recipe.Name
                $links = @($recipe.Links | ForEach-Object { Join-Path $work $_ })
                Invoke-Checked $clang (@("--target=aarch64-linux-android$api", '-shared', '-fPIC', '-O2',
                    '-Wl,-z,max-page-size=16384', "-Wl,-soname,$($recipe.Name)", '-o', $so,
                    (Join-Path $jni $recipe.Source)) + $links)
                # PLAN.md §7: every built ELF is checked to be ELF64 for AArch64 (e_machine 183).
                $b = [IO.File]::ReadAllBytes($so)
                if (-not ($b[0] -eq 0x7F -and $b[1] -eq 0x45 -and $b[4] -eq 2 -and [BitConverter]::ToUInt16($b, 18) -eq 183)) { throw "$so is not an ELF64 AArch64 library" }
                Write-Step "$([IO.Path]::GetFileName($so)): ELF64 AArch64"
                $libs += $so
            }
        }
        if ($libs) {
            $zip = [IO.Compression.ZipFile]::Open($unsigned, 'Update')
            try { foreach ($so in $libs) { [IO.Compression.ZipFileExtensions]::CreateEntryFromFile($zip, $so, "lib/arm64-v8a/$([IO.Path]::GetFileName($so))", 'Optimal') | Out-Null } }
            finally { $zip.Dispose() }
        }
        $aligned = Join-Path $work 'aligned.apk'
        Invoke-Checked (Join-Path $bt 'zipalign.exe') @('-f', '-p', '4', $unsigned, $aligned)
        $apk = Join-Path $fixtures "$name.apk"
        Invoke-Checked (Join-Path $bt 'apksigner.bat') @('sign', '--ks', $key, '--ks-pass', 'pass:android', '--ks-key-alias', 'fixture', '--out', $apk, $aligned)
        Remove-Item "$apk.idsig" -ErrorAction SilentlyContinue
        $info = [ordered]@{
            fixture = $name
            source = "tests/windows-arm64/apk-fixtures/$name"
            buildTools = Split-Path $bt -Leaf
            platform = Split-Path (Split-Path $sdk.AndroidJar) -Leaf
            javac = '-source 8 -target 8 -bootclasspath android.jar'
            d8 = '--release --min-api 24'
            dexSha256 = (Get-FileHash (Join-Path $dex 'classes.dex') -Algorithm SHA256).Hash.ToLower()
            apkSha256 = (Get-FileHash $apk -Algorithm SHA256).Hash.ToLower()
        }
        if ($kt) {
            $info.kotlinc = "$($lock.guest.kotlinc.version) -jvm-target 1.8 -no-reflect"
            $info.kotlinStdlibSha256 = (Get-FileHash $stdlib[0] -Algorithm SHA256).Hash.ToLower()
        }
        if ($libs) {
            $info.ndk = $lock.guest.androidNdk.version
            $info.clang = "--target=aarch64-linux-android$api -shared -fPIC -O2 -Wl,-z,max-page-size=16384 -Wl,-soname,<recipe.Name>"
            $info.nativeRecipes = $recipes
            $info.libs = [ordered]@{}
            foreach ($so in $libs) { $info.libs["lib/arm64-v8a/$([IO.Path]::GetFileName($so))"] = (Get-FileHash $so -Algorithm SHA256).Hash.ToLower() }
        }
        $info | ConvertTo-Json -Depth 6 | Set-Content (Join-Path $fixtures "$name.json") -Encoding utf8NoBOM
        Write-Step "$name.apk: dex sha256 $($info.dexSha256)"
    }
}

<#
Writes $dest with one controlled ELF defect (tests/windows-arm64/guest-elf/fixtures.psd1, Mutation).
T06 patches one structural field; T07 changes a version name and its matching hash, or one chain offset.
Notes: little-endian ELF64 offsets; program header i is at e_phoff + 56 * i.
#>
function Write-ElfMutation([string]$src, [string]$dest, [string]$mutation) {
    $b = [IO.File]::ReadAllBytes($src)
    $u16 = { param($o) [BitConverter]::ToUInt16($b, $o) }
    $u32 = { param($o) [BitConverter]::ToUInt32($b, $o) }
    $u64 = { param($o) [BitConverter]::ToUInt64($b, $o) }
    $put = { param($o, [byte[]]$v) [Array]::Copy($v, 0, $b, $o, $v.Length) }
    $phoff = & $u64 0x20
    $phnum = & $u16 0x38
    $ph = 0..($phnum - 1) | ForEach-Object { $at = $phoff + 56 * $_; [pscustomobject]@{ At = $at; Type = (& $u32 $at); Flags = (& $u32 ($at + 4)); Offset = (& $u64 ($at + 8)); Vaddr = (& $u64 ($at + 16)) } }
    $loads = @($ph | Where-Object Type -EQ 1)
    switch ($mutation) {
        'bad-magic' { $b[3] = [byte][char]'X' }
        'truncated' { $b = $b[0..0xFFF] }
        'phdr-range' { & $put 0x38 ([BitConverter]::GetBytes([uint16]0xFFFF)) }
        # PT_LOAD 1's p_memsz grows 16 bytes into PT_LOAD 2 (moving a vaddr would break alignment first).
        'overlap' { & $put ($loads[1].At + 40) ([BitConverter]::GetBytes([uint64]($loads[2].Vaddr - $loads[1].Vaddr + 16))) }
        'rwx' { $x = $loads | Where-Object { $_.Flags -band 1 } | Select-Object -First 1; & $put ($x.At + 4) ([BitConverter]::GetBytes([uint32]7)) }
        'et-exec' { & $put 0x10 ([BitConverter]::GetBytes([uint16]2)) }
        'unknown-reloc' {
            # The first DT_RELA entry's type (the low half of r_info) becomes 0x7fff.
            $dyn = $ph | Where-Object Type -EQ 2
            for ($e = $dyn.Offset; ; $e += 16) {
                $tag = & $u64 $e
                if ($tag -eq 0) { throw "$src has no DT_RELA" }
                if ($tag -eq 7) { $rela = & $u64 ($e + 8); break }
            }
            $seg = $loads | Where-Object { $_.Vaddr -le $rela } | Select-Object -Last 1
            & $put ($rela - $seg.Vaddr + $seg.Offset + 8) ([BitConverter]::GetBytes([uint32]0x7FFF))
        }
        { $_ -in 'unknown-version', 'bad-version-chain' } {
            $dyn = $ph | Where-Object Type -EQ 2
            $tags = @{}
            for ($e = $dyn.Offset; (& $u64 $e) -ne 0; $e += 16) { $tags[(& $u64 $e)] = & $u64 ($e + 8) }
            $offset = {
                param([uint64]$address)
                $segment = $loads | Where-Object { $_.Vaddr -le $address } | Select-Object -Last 1
                $address - $segment.Vaddr + $segment.Offset
            }
            $need = & $offset $tags[[uint64]0x6ffffffe]
            if ($mutation -eq 'bad-version-chain') {
                & $put ($need + 12) ([BitConverter]::GetBytes([uint32]1))
            } else {
                # Change the version name and matching ELF hash, keeping the version table valid.
                $strings = & $offset $tags[[uint64]5]
                [long]$hash = 0
                foreach ($character in [Text.Encoding]::ASCII.GetBytes('NOPE')) {
                    $hash = ($hash -shl 4) + $character
                    $high = $hash -band 0xf0000000
                    if ($high) { $hash = $hash -bxor ($high -shr 24) }
                    $hash = $hash -band (-bnot $high)
                }
                for (;;) {
                    $aux = $need + (& $u32 ($need + 8))
                    for ($n = 0; $n -lt (& $u16 ($need + 2)); $n++) {
                        & $put ($strings + (& $u32 ($aux + 8))) ([Text.Encoding]::ASCII.GetBytes('NOPE'))
                        & $put $aux ([BitConverter]::GetBytes([uint32]$hash))
                        $aux += & $u32 ($aux + 12)
                    }
                    $next = & $u32 ($need + 12)
                    if (-not $next) { break }
                    $need += $next
                }
            }
        }
        default { throw "unknown mutation $mutation" }
    }
    [IO.File]::WriteAllBytes($dest, $b)
}

<#
Builds the guest ELF fixtures (T05, T06, T07, T08) that tests/windows-arm64/guest-elf/fixtures.psd1
lists into tests\fixtures\elf with the NDK, and records their build parameters and hashes in
fixtures.json there. Notes: generated, never committed (AGENTS.md).
#>
function Build-ElfFixtures {
    $clang = Find-AndroidNdk
    $lock = Get-Content (Join-Path $repo 'build-lock.json') -Raw | ConvertFrom-Json
    $src = Join-Path $repo 'tests\windows-arm64\guest-elf'
    $table = Import-PowerShellDataFile (Join-Path $src 'fixtures.psd1')
    $dir = Join-Path $repo 'tests\fixtures\elf'
    New-Item -ItemType Directory -Force $dir | Out-Null
    # Only the files this table names are replaced (Reset-Dir clears under build\ alone).
    foreach ($n in @($table.Fixtures.Name) + 'fixtures.json') { Remove-Item (Join-Path $dir $n) -ErrorAction SilentlyContinue }
    $clangVersion = (& $clang --version | Select-Object -First 1)
    $records = foreach ($f in $table.Fixtures) {
        $dest = Join-Path $dir $f.Name
        $rec = [ordered]@{ name = $f.Name; fixture = $f.Id }
        if ($f.ContainsKey('Mutation')) {
            Write-ElfMutation (Join-Path $dir $f.From) $dest $f.Mutation
            $rec.from = $f.From
            $rec.mutation = $f.Mutation
        } else {
            $soname = if ($f.ContainsKey('Soname')) { $f.Soname } else { $f.Name }
            $common = if ($f.ContainsKey('Common')) { $f.Common } else { $table.Common }
            $cargs = @("--target=$($f.Target)") + $common + $f.Flags + @("-Wl,-soname,$soname", '-o', $dest, (Join-Path $src $f.Source))
            if ($f.ContainsKey('Links')) { $cargs += @($f.Links | ForEach-Object { Join-Path $dir $_ }) }
            if ($f.ContainsKey('Libraries')) { $cargs += @($f.Libraries | ForEach-Object { "-l$_" }) }
            Invoke-Checked $clang $cargs
            $rec.source = "tests/windows-arm64/guest-elf/$($f.Source)"
            $rec.clang = ($cargs -join ' ')
        }
        if ($f.ContainsKey('Expect')) { $rec.expect = "$($f.Expect): $($f.Reason)" }
        if ($f.ContainsKey('Result')) { $rec.expect = $f.Result }
        $bytes = [IO.File]::ReadAllBytes($dest)
        if ($f.ContainsKey('Target') -and $f.Target -eq 'aarch64-linux-android30' -and
            ($bytes[4] -ne 2 -or [BitConverter]::ToUInt16($bytes, 18) -ne 183)) {
            throw "$($f.Name) is not ELF64 AArch64"
        }
        $rec.sha256 = (Get-FileHash $dest -Algorithm SHA256).Hash.ToLower()
        Write-Step "$($f.Id) $($f.Name): $($rec.sha256.Substring(0, 16))"
        $rec
    }
    [ordered]@{ ndk = $lock.guest.androidNdk.version; compiler = $clangVersion; fixtures = @($records) } |
        ConvertTo-Json -Depth 4 | Set-Content (Join-Path $dir 'fixtures.json') -Encoding utf8NoBOM
}

<# Builds the audited Android-ABI formatting helper, not a Windows DLL or a system libc. #>
function Build-GuestBionic {
    $clang = Find-AndroidNdk
    $lock = Get-Content (Join-Path $repo 'build-lock.json') -Raw | ConvertFrom-Json
    $directory = Join-Path $out 'bin\guest'
    New-Item -ItemType Directory -Force $directory | Out-Null
    $library = Join-Path $directory 'libwinapk-bionic.so'
    $source = 'macapk/runtime/guest/bionic/format.c'
    $arguments = @('--target=aarch64-linux-android30', '-shared', '-fPIC', '-fno-stack-protector',
                   '-O2', '-nostdlib', '-fno-builtin', '-Wall', '-Wextra', '-Werror',
                   '-Wl,-soname,libwinapk-bionic.so', '-o', $library, (Join-Path $repo $source))
    Invoke-Checked $clang $arguments
    $bytes = [IO.File]::ReadAllBytes($library)
    if ($bytes[0] -ne 0x7f -or $bytes[4] -ne 2 -or [BitConverter]::ToUInt16($bytes, 18) -ne 183) {
        throw 'guest formatting helper is not ELF64 AArch64'
    }
    $record = [ordered]@{
        source = $source; ndk = $lock.guest.androidNdk.version
        compiler = (& $clang --version | Select-Object -First 1)
        arguments = $arguments
        sha256 = (Get-FileHash $library -Algorithm SHA256).Hash.ToLower()
    }
    $record | ConvertTo-Json -Depth 4 | Set-Content (Join-Path $directory 'bionic.json') -Encoding utf8NoBOM
    Write-Step "libwinapk-bionic.so: ELF64 AArch64, $($record.sha256.Substring(0, 16))"
}

<# Builds the Android-only JNI varargs helper and records its toolchain, flags and content hash. #>
function Build-GuestJni {
    $clang = Find-AndroidNdk
    $lock = Get-Content (Join-Path $repo 'build-lock.json') -Raw | ConvertFrom-Json
    $directory = Join-Path $out 'bin\guest'
    New-Item -ItemType Directory -Force $directory | Out-Null
    $library = Join-Path $directory 'libwinapk-jni.so'
    $source = 'macapk/runtime/guest/jni/calls.c'
    $arguments = @('--target=aarch64-linux-android30', '-shared', '-fPIC', '-fno-stack-protector',
                   '-O2', '-nostdlib', '-fno-builtin', '-Wall', '-Wextra', '-Werror',
                   '-Wl,-soname,libwinapk-jni.so', '-o', $library, (Join-Path $repo $source))
    Invoke-Checked $clang $arguments
    $bytes = [IO.File]::ReadAllBytes($library)
    if ($bytes[0] -ne 0x7f -or $bytes[4] -ne 2 -or [BitConverter]::ToUInt16($bytes, 18) -ne 183) {
        throw 'guest JNI helper is not ELF64 AArch64'
    }
    $record = [ordered]@{
        source = $source; ndk = $lock.guest.androidNdk.version
        compiler = (& $clang --version | Select-Object -First 1)
        arguments = $arguments
        sha256 = (Get-FileHash $library -Algorithm SHA256).Hash.ToLower()
    }
    $record | ConvertTo-Json -Depth 4 | Set-Content (Join-Path $directory 'jni.json') -Encoding utf8NoBOM
    Write-Step "libwinapk-jni.so: ELF64 AArch64, $($record.sha256.Substring(0, 16))"
}

<# Builds pinned ANGLE ARM64 DLLs privately with its public DEPS and no system driver/toolchain installation. #>
function Build-Angle {
    $lock = Get-Content (Join-Path $repo 'build-lock.json') -Raw | ConvertFrom-Json
    $record = $lock.host.angle
    $deps = Join-Path $out 'deps'
    New-Item -ItemType Directory -Force $deps | Out-Null
    foreach ($source in @(
        @{ Name = 'depot_tools'; Url = 'https://chromium.googlesource.com/chromium/tools/depot_tools.git'; Commit = $record.depotToolsCommit },
        @{ Name = 'angle'; Url = $record.source; Commit = $record.commit }
    )) {
        $path = Join-Path $deps $source.Name
        if (-not (Test-Path $path)) { Invoke-Checked 'git' @('clone', '--depth', '1', $source.Url, $path) }
        $head = & git -C $path rev-parse HEAD
        if ($LASTEXITCODE -ne 0) { throw "Not a source checkout: $path" }
        $changes = & git -C $path status --porcelain --untracked-files=no
        if ($LASTEXITCODE -ne 0 -or $changes) { throw "Inspect local source changes before building $($source.Name)" }
        if ($head.Trim() -ne $source.Commit) {
            Invoke-Checked 'git' @('-C', $path, 'fetch', '--depth', '1', 'origin', $source.Commit)
            Invoke-Checked 'git' @('-C', $path, 'checkout', '--detach', $source.Commit)
        }
    }
    $client = @'
solutions = [
  {
    "name": "angle",
    "url": "https://chromium.googlesource.com/angle/angle",
    "managed": False,
    "custom_deps": {},
    "custom_vars": {
      "checkout_angle_cl_deps": False,
      "checkout_angle_dawn_deps": False,
      "checkout_angle_internal": False,
      "checkout_angle_restricted_traces": False,
      "checkout_extra_traces": False,
    },
  },
]
'@
    $clientPath = Join-Path $deps '.gclient'
    if (Test-Path $clientPath) {
        $existing = Get-Content $clientPath -Raw
        if ($existing.Replace("`r", '').Trim() -ne $client.Replace("`r", '').Trim()) { throw 'Inspect the existing ANGLE .gclient configuration before changing it' }
    } else { $client | Set-Content $clientPath -Encoding utf8NoBOM }
    $depot = Join-Path $deps 'depot_tools'
    $angle = Join-Path $deps 'angle'
    $env:DEPOT_TOOLS_UPDATE = '0'
    $env:DEPOT_TOOLS_WIN_TOOLCHAIN = '0'
    $env:CIPD_CACHE_DIR = Join-Path $deps 'cache\cipd'
    $env:VPYTHON_VIRTUALENV_ROOT = Join-Path $deps 'cache\vpython'
    $env:PATH = "$depot;$env:PATH"
    Invoke-Checked (Join-Path $depot 'bootstrap\win_tools.bat') @()
    Push-Location $deps
    try {
        Invoke-Checked (Join-Path $depot 'gclient.bat') @('sync', '--no-history', '--nohooks', '--revision', "angle@$($record.commit)", '-j4')
    } finally { Pop-Location }
    foreach ($relative in $record.patches) {
        $patch = Join-Path $repo $relative
        & git -C (Join-Path $angle 'build') apply --reverse --check --ignore-space-change $patch 2> $null
        if ($LASTEXITCODE -ne 0) {
            Invoke-Checked 'git' @('-C', (Join-Path $angle 'build'), 'apply', '--check', '--ignore-space-change', $patch)
            Invoke-Checked 'git' @('-C', (Join-Path $angle 'build'), 'apply', '--ignore-space-change', $patch)
        }
    }
    $vs = Find-VisualStudio
    $sdk = (Get-ItemPropertyValue 'HKLM:\SOFTWARE\Microsoft\Windows Kits\Installed Roots' 'KitsRoot10').TrimEnd('\')
    $env:GYP_MSVS_OVERRIDE_PATH = $vs.Root
    $env:GYP_MSVS_VERSION = '2026'
    Push-Location $angle
    try {
        # Run only the metadata hook needed by these DLL targets; no Rust/test/remote toolchain setup.
        Invoke-Checked (Join-Path $depot 'python3.bat') @('build/util/lastchange.py', '-o', 'build/util/LASTCHANGE')
    } finally { Pop-Location }
    # Fetch the exact public hook object directly: gsutil's own archive exceeds MAX_PATH here.
    $rc = Join-Path $angle 'build\toolchain\win\rc\win\rc.exe'
    $rcHash = (Get-Content "$rc.sha1" -Raw).Trim()
    if (-not (Test-Path $rc) -or (Get-FileHash $rc -Algorithm SHA1).Hash.ToLower() -ne $rcHash) {
        Invoke-WebRequest "https://storage.googleapis.com/chromium-browser-clang/rc/$rcHash" -OutFile "$rc.download"
        if ((Get-FileHash "$rc.download" -Algorithm SHA1).Hash.ToLower() -ne $rcHash) { throw 'ANGLE resource compiler hook hash mismatch' }
        Move-Item "$rc.download" $rc -Force
    }
    if ((Get-FileHash $rc -Algorithm SHA256).Hash.ToLower() -ne $record.resourceCompilerSha256) {
        throw 'ANGLE resource compiler SHA-256 mismatch'
    }
    $compiler = Join-Path $angle 'third_party\llvm-build\Release+Asserts\bin\clang-cl.exe'
    Invoke-Checked $compiler @('--version')
    $native = Join-Path $angle 'out\WindowsArm64'
    New-Item -ItemType Directory -Force $native | Out-Null
    $gnArgs = (Get-Content (Join-Path $repo $record.gnArgs) -Raw) + "`n" +
        "visual_studio_path = `"$($vs.Root.Replace('\', '/'))`"`nvisual_studio_version = `"2026`"`n" +
        "windows_sdk_path = `"$($sdk.Replace('\', '/'))`"`nwindows_sdk_version = `"$($lock.host.windowsSdk.version)`"`n" +
        "wdk_path = `"$($sdk.Replace('\', '/'))`"`n"
    $gnArgs | Set-Content (Join-Path $native 'args.gn') -Encoding utf8NoBOM
    Push-Location $angle
    try {
        Invoke-InDevShell $vs @((Join-Path $angle 'buildtools\win\gn.exe'), 'gen', $native, '--fail-on-unused-args')
        Invoke-InDevShell $vs @((Join-Path $vs.NinjaDir 'ninja.exe'), '-C', $native, '-j4', 'libEGL', 'libGLESv2')
    } finally { Pop-Location }
    $dest = Join-Path $out 'bin\angle'
    New-Item -ItemType Directory -Force $dest | Out-Null
    $hashes = [ordered]@{}
    foreach ($name in 'libEGL.dll', 'libGLESv2.dll') {
        $dll = Join-Path $native $name
        if ((Get-PeMachine $dll) -ne 'ARM64') { throw "$name is not ARM64; not staging it" }
        Copy-Item $dll (Join-Path $dest $name) -Force
        $hashes[$name] = (Get-FileHash $dll -Algorithm SHA256).Hash.ToLower()
    }
    $patches = @($record.patches | ForEach-Object {
        [ordered]@{ path = $_; sha256 = (Get-FileHash (Join-Path $repo $_) -Algorithm SHA256).Hash.ToLower() }
    })
    [ordered]@{ commit = $record.commit; depotToolsCommit = $record.depotToolsCommit; patches = $patches
        compiler = $record.compiler; resourceCompilerSha256 = $record.resourceCompilerSha256
        args = Get-Content (Join-Path $repo $record.gnArgs) -Raw
        argsSha256 = (Get-FileHash (Join-Path $repo $record.gnArgs) -Algorithm SHA256).Hash.ToLower()
        windowsSdk = $lock.host.windowsSdk.version; sha256 = $hashes } |
        ConvertTo-Json -Depth 5 | Set-Content (Join-Path $dest 'angle.json') -Encoding utf8NoBOM
    Write-Step 'ANGLE DLLs staged as ARM64; no rendering capability is claimed until the host probes run'
}

<# Builds winapk.exe, native-probe.exe and the Java probe into $out\bin (all targets, or only $Target). #>
function Build-Host([string]$Target = '') {
    $vs = Find-VisualStudio
    $jdk = Find-Jdk
    $bin = Join-Path $out 'bin'
    $native = Join-Path $out "native\$Configuration"
    New-Item -ItemType Directory -Force $bin, $native | Out-Null

    Write-Step 'Compiling the Java probe (winapk-probe.jar)'
    $probeSrc = Join-Path $rt 'platform\windows\java'
    $probeClasses = Join-Path $out 'probe-classes'
    Reset-Dir $probeClasses
    $files = Get-ChildItem $probeSrc -Recurse -Filter *.java | ForEach-Object FullName
    Invoke-Checked (Join-Path $jdk 'bin\javac.exe') (@('-d', $probeClasses, '--release', '21', '-encoding', 'UTF-8') + $files)
    Invoke-Checked (Join-Path $jdk 'bin\jar.exe') @('--create', '--file', (Join-Path $bin 'winapk-probe.jar'), '-C', $probeClasses, '.')

    Write-Step "Configuring the native host ($Configuration, ARM64, clang-cl)"
    $cmake = Join-Path $vs.CMakeDir 'cmake.exe'
    $toolchain = Join-Path $vs.Root 'VC\vcpkg\scripts\buildsystems\vcpkg.cmake'
    $env:VCPKG_ROOT = $null
    Invoke-InDevShell $vs @(
        $cmake, '-S', $rt, '-B', $native, '-G', 'Ninja',
        "-DCMAKE_MAKE_PROGRAM=$(Join-Path $vs.NinjaDir 'ninja.exe')",
        "-DCMAKE_BUILD_TYPE=$Configuration",
        '-DCMAKE_C_COMPILER=clang-cl', '-DCMAKE_CXX_COMPILER=clang-cl',
        "-DCMAKE_TOOLCHAIN_FILE=$toolchain",
        "-DVCPKG_MANIFEST_DIR=$(Join-Path $rt 'platform\windows')",
        "-DVCPKG_INSTALLED_DIR=$(Join-Path $out 'vcpkg_installed')",
        '-DVCPKG_TARGET_TRIPLET=arm64-windows',
        "-DJAVA_HOME=$jdk",
        "-DWINAPK_BIN_DIR=$bin"
    )
    Write-Step "Building $(if ($Target) { "$Target.exe" } else { 'winapk.exe and native-probe.exe' })"
    Invoke-InDevShell $vs (@($cmake, '--build', $native) + $(if ($Target) { @('--target', $Target) } else { @() }))
    foreach ($f in Get-ChildItem $bin -Include *.exe, *.dll -Recurse) {
        $m = Get-PeMachine $f.FullName
        if ($m -ne 'ARM64') { throw "$($f.Name) is $m, not ARM64 (AGENTS.md: native means native)" }
        Write-Step "$($f.Name): $m"
    }
    if ($Target -eq 'angle-probe') {
        $lock = Get-Content (Join-Path $repo 'build-lock.json') -Raw | ConvertFrom-Json
        $source = 'macapk/runtime/platform/windows/src/angle_probe.cpp'
        [ordered]@{
            fixture = 'T10.host'; source = $source
            sourceSha256 = (Get-FileHash (Join-Path $repo $source) -Algorithm SHA256).Hash.ToLower()
            compiler = $lock.host.'clang-cl'.version; configuration = $Configuration
            cmake = 'macapk/runtime/platform/windows/windows.cmake; angle-probe target, C++20, ARM64'
            sha256 = (Get-FileHash (Join-Path $bin 'angle-probe.exe') -Algorithm SHA256).Hash.ToLower()
            expected = 'vulkan/d3d11: exit 0, four RGBA 64/128/191/255 reads (tolerance 1), resize and teardown; unavailable Vulkan: exit 2, no fallback'
        } | ConvertTo-Json | Set-Content (Join-Path $bin 'angle-probe.json') -Encoding utf8NoBOM
    }
}

switch ($Component) {
    'Java' { Build-Java }
    'FrameworkRes' { Build-FrameworkRes }
    'Fixtures' { Build-Fixtures }
    'Host' { Build-Host }
    'NativeProbe' { Build-Host 'native-probe'; Build-GuestBionic; Build-ElfFixtures }
    'JniProbe' { Build-Host 'jni-probe'; Build-GuestBionic; Build-GuestJni; Build-ElfFixtures }
    'Angle' { Build-Angle }
    'AngleProbe' { Build-Host 'angle-probe' }
    'All' { Build-Java; Build-FrameworkRes; Build-Fixtures; Build-Host; Build-GuestBionic; Build-GuestJni; Build-ElfFixtures }
}
