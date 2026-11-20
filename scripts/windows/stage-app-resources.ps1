<#
.SYNOPSIS
Stages (or cleans) the Release winapk runtime under resources/windows-arm64 for `go tool mygo build`.
.DESCRIPTION
MyGo installs `resources/windows-arm64/*` beside `Windows-ARM64.apk.exe` in both the NSIS installer
(`windows-arm64-apk-setup-<version>-arm64.exe`) and the Ed25519-signed update archive
(`windows-arm64-apk-<version>-windows-arm64.tar.gz`). Every staged `.exe` and `.dll` is verified as
native ARM64 (0xAA64) and audited against Debug CRT imports.
#>
[CmdletBinding()]
param(
    [ValidateSet('Release')][string]$Configuration = 'Release',
    [switch]$Clean
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
. "$PSScriptRoot\common.ps1"

$repo = Get-RepoRoot
$stage = Join-Path $repo 'resources\windows-arm64'
if (Test-Path -LiteralPath $stage) {
    Remove-Item -LiteralPath $stage -Recurse -Force
}
if ($Clean) {
    Write-Step 'Cleaned resources/windows-arm64'
    return
}

$build = Join-Path $repo 'build\windows-arm64'
$native = Join-Path $build "native\$Configuration\bin"
$lock = Get-Content (Join-Path $repo 'build-lock.json') -Raw | ConvertFrom-Json
New-Item -ItemType Directory -Force $stage | Out-Null

function Copy-Staged([string]$Base, [string]$Source, [string]$Relative) {
    $inFile = Join-Path $Base $Source
    if (-not (Test-Path -LiteralPath $inFile -PathType Leaf)) { throw "Missing runtime artifact: $Source" }
    if ($Relative -match '\.(exe|dll)$' -and (Get-PeMachine $inFile) -ne 'ARM64') {
        throw "Not an ARM64 PE: $Source"
    }
    $dest = Join-Path $stage $Relative
    New-Item -ItemType Directory -Path (Split-Path -Parent $dest) -Force | Out-Null
    Copy-Item -LiteralPath $inFile -Destination $dest -Force
}

foreach ($f in Get-ChildItem $native -File | Where-Object { $_.Name -in @('winapk.exe','winapk-bridge.dll','winapk-probe.jar','z.dll','sqlite3.dll','capstone.dll','liblzma.dll') }) {
    Copy-Staged $native $f.Name "bin\$($f.Name)"
}
foreach ($req in 'winapk.exe','winapk-bridge.dll','winapk-probe.jar','z.dll','sqlite3.dll','capstone.dll','liblzma.dll') {
    if (-not (Test-Path -LiteralPath (Join-Path $stage "bin\$req"))) { throw "Missing required Release binary: $req" }
}
$angleDir = Join-Path $build 'bin\angle'
if (Test-Path -LiteralPath (Join-Path $angleDir 'libEGL.dll')) {
    foreach ($dll in 'libEGL.dll','libGLESv2.dll','d3dcompiler_47.dll') {
        if (Test-Path -LiteralPath (Join-Path $angleDir $dll)) {
            Copy-Staged $angleDir $dll "bin\angle\$dll"
        }
    }
    if (Test-Path -LiteralPath (Join-Path $angleDir 'angle.json')) {
        Copy-Staged $angleDir 'angle.json' 'bin\angle\angle.json'
    }
}
$skikoDir = Join-Path $build 'bin\skia'
if (Test-Path -LiteralPath (Join-Path $skikoDir 'skiko-windows-arm64.dll')) {
    Copy-Staged $skikoDir 'skiko-windows-arm64.dll' 'bin\skia\skiko-windows-arm64.dll'
    if (Test-Path -LiteralPath (Join-Path $skikoDir 'icudtl.dat')) {
        Copy-Staged $skikoDir 'icudtl.dat' 'bin\skia\icudtl.dat'
    }
}

$readobj = Join-Path (Split-Path (Find-AndroidNdk)) 'llvm-readobj.exe'
foreach ($pe in Get-ChildItem $stage -File -Recurse | Where-Object Extension -in '.dll','.exe') {
    $imports = & $readobj --coff-imports $pe.FullName
    if ($LASTEXITCODE -or $imports -match '(?i)Name: (?:ucrtbased|(?:msvcp|vcruntime)\d+(?:_\d+)?d)\.dll') {
        throw "Debug CRT import detected in staged PE: $($pe.Name)"
    }
}

foreach ($jar in 'android-framework.jar','macapk-loader.jar','bcprov-jdk18on-1.79.jar') {
    Copy-Staged $build "lib\$jar" "lib\$jar"
}
foreach ($module in 'java_base_patch','jdk_unsupported_patch') {
    $root = Join-Path $build "lib\$module"
    $classes = @(Get-ChildItem -LiteralPath $root -Filter '*.class' -File -Recurse)
    if (-not $classes.Count) { throw "Missing patch module: $module" }
    foreach ($c in $classes) {
        $rel = [IO.Path]::GetRelativePath($root, $c.FullName)
        Copy-Staged $root $rel "lib\$module\$rel"
    }
}

$fw = 'bin\tools\framework-res.apk'
Copy-Staged $build $fw $fw
foreach ($helper in 'bionic','bionic-n2','jni') {
    $so = "bin\guest\libwinapk-$helper.so"
    Copy-Staged $build $so $so
}

foreach ($doc in 'README.md','LICENSE','THIRD_PARTY_NOTICES.md','build-lock.json') {
    Copy-Staged $repo $doc $doc
}
Copy-Staged $repo 'macapk\LICENSE' 'licenses\Mac.apk-LICENSE'
Copy-Staged $repo 'macapk\THIRD_PARTY_NOTICES.txt' 'licenses\Mac.apk-NOTICES.txt'
foreach ($dep in @('zlib','sqlite3','capstone','vulkan-headers','liblzma')) {
    $cp = Join-Path $build "vcpkg_installed\arm64-windows\share\$dep\copyright"
    if (Test-Path -LiteralPath $cp) {
        Copy-Staged $build "vcpkg_installed\arm64-windows\share\$dep\copyright" "licenses\$dep.txt"
    }
}
if (Test-Path -LiteralPath (Join-Path $build 'deps\angle\LICENSE')) {
    Copy-Staged $build 'deps\angle\LICENSE' 'licenses\ANGLE-LICENSE'
}
if ($lock.guest.frameworkRes.source -match 'tag (\S+),') {
    $notice = "framework-res\work-$($Matches[1])\src\NOTICE"
    if (Test-Path -LiteralPath (Join-Path $build $notice)) {
        Copy-Staged $build $notice 'licenses\AOSP-framework-res-NOTICE'
    }
}
Write-Step "Staged Release runtime into resources/windows-arm64 ($((Get-ChildItem $stage -File -Recurse).Count) files)"
