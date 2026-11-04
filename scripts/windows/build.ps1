<#
.SYNOPSIS
Builds Windows-ARM64.apk's runtime parts on Windows 11 ARM64.

.DESCRIPTION
Component Java builds Mac.apk's Java side (the Android framework, the boot
loader and the DEX converter) the way macapk/runtime/scripts/build.sh does:
one javac pass over macapk/runtime/java, android-framework.jar (everything but
com/macapk) and macapk-loader.jar (com/macapk), the bundled fonts, the
java.base and jdk.unsupported patch modules, the net_spi resolver provider and
bcprov. Component Host builds the native host (winapk.exe) and its Java probe
with CMake, Ninja and clang-cl from Visual Studio, for ARM64.

Output goes to build/windows-arm64/ (lib/ and bin/). Nothing outside the
repository is changed, and nothing is installed.

.PARAMETER Component
Java, Host, or All.

.PARAMETER Configuration
Debug or Release (Host only).
#>
[CmdletBinding()]
param(
    [ValidateSet('Java', 'FrameworkRes', 'Fixtures', 'Host', 'All')]
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
    Invoke-Checked $javac @('-d', $classes, '--release', '21', '-encoding', 'UTF-8', '-Xlint:-options', "@$list")

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
        $classFiles = @(Get-ChildItem $classes -Recurse -Filter *.class | ForEach-Object FullName)
        Invoke-Checked (Join-Path $bt 'd8.bat') (@('--release', '--min-api', '24', '--lib', $sdk.AndroidJar, '--output', $dex) + $classFiles)
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
            foreach ($c in Get-ChildItem $jni -Filter *.c) {
                $so = Join-Path $work "lib$($c.BaseName).so"
                Invoke-Checked $clang @('--target=aarch64-linux-android24', '-shared', '-fPIC', '-O2', '-Wl,-z,max-page-size=16384', '-o', $so, $c.FullName)
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
        if ($libs) {
            $info.ndk = $lock.guest.androidNdk.version
            $info.clang = '--target=aarch64-linux-android24 -shared -fPIC -O2 -Wl,-z,max-page-size=16384'
            $info.libs = [ordered]@{}
            foreach ($so in $libs) { $info.libs["lib/arm64-v8a/$([IO.Path]::GetFileName($so))"] = (Get-FileHash $so -Algorithm SHA256).Hash.ToLower() }
        }
        $info | ConvertTo-Json | Set-Content (Join-Path $fixtures "$name.json") -Encoding utf8NoBOM
        Write-Step "$name.apk: dex sha256 $($info.dexSha256)"
    }
}

<# Builds winapk.exe and its Java probe into $out\bin. #>
function Build-Host {
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
    Write-Step 'Building winapk.exe'
    Invoke-InDevShell $vs @($cmake, '--build', $native)
    foreach ($f in Get-ChildItem $bin -Include *.exe, *.dll -Recurse) {
        $m = Get-PeMachine $f.FullName
        if ($m -ne 'ARM64') { throw "$($f.Name) is $m, not ARM64 (AGENTS.md: native means native)" }
        Write-Step "$($f.Name): $m"
    }
}

switch ($Component) {
    'Java' { Build-Java }
    'FrameworkRes' { Build-FrameworkRes }
    'Fixtures' { Build-Fixtures }
    'Host' { Build-Host }
    'All' { Build-Java; Build-FrameworkRes; Build-Fixtures; Build-Host }
}
