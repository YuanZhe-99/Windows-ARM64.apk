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
    [ValidateSet('Java', 'Host', 'All')]
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
    'Host' { Build-Host }
    'All' { Build-Java; Build-Host }
}
