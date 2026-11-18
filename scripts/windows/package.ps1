<#
.SYNOPSIS
Stages a local Windows ARM64 experimental bundle (front end + Release runtime) or CLI-only ZIP from existing Release artifacts.
.DESCRIPTION
No downloads, installs, signing or publishing. The source ZIP omits upstream macOS prebuilt libraries.
Stage all intended source changes, or run from a clean checkout. Generated files remain ignored.
#>
[CmdletBinding()]
param(
    [ValidateSet('Release')][string]$Configuration = 'Release',
    [ValidateSet('Bundle', 'Cli')][string]$Target = 'Bundle',
    [switch]$IncludeAngle
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
. "$PSScriptRoot\common.ps1"

$repo = Get-RepoRoot
$build = Join-Path $repo 'build\windows-arm64'
$appOut = Join-Path $repo 'build\app\windows-arm64'
$native = Join-Path $build "native\$Configuration\bin"
$lock = Get-Content (Join-Path $repo 'build-lock.json') -Raw | ConvertFrom-Json
$rest = @(git -C $repo diff --name-only)
if ($LASTEXITCODE -or $rest.Count) { throw 'Stage intended source changes before packaging' }
$rest = @(git -C $repo ls-files --others --exclude-standard)
if ($LASTEXITCODE -or $rest.Count) { throw 'Untracked source files must be resolved before packaging' }
$tree = git -C $repo write-tree
if ($LASTEXITCODE) { throw 'Could not record the Git source tree' }
$head = git -C $repo rev-parse HEAD
if ($LASTEXITCODE) { throw 'Could not record the Git revision' }
$version = (& (Join-Path $native 'winapk.exe') --version) -replace '^winapk ', ''
if ($LASTEXITCODE -or $version -notmatch '^\d+\.\d+\.\d+$') { throw 'Missing or invalid Release winapk.exe; build Release Host first' }

$name = if ($Target -eq 'Bundle') { "Windows-ARM64.apk-$version-arm64" } else { "Windows-ARM64.apk-$version-cli-arm64" }
$output = Join-Path $build ('package\' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff') + '-' + $tree.Substring(0,8))
$stage = Join-Path $output $name
New-Item -ItemType Directory $output | Out-Null
New-Item -ItemType Directory $stage | Out-Null

<# Copies one allowlisted artifact and, for a PE, verifies ARM64. #>
function Copy-Payload([string]$Base, [string]$Source, [string]$Relative) {
    $inputFile = Join-Path $Base $Source
    if (!(Test-Path -LiteralPath $inputFile -PathType Leaf)) { throw "Missing package input: $Source" }
    if ($Relative -match '\.(exe|dll)$' -and (Get-PeMachine $inputFile) -ne 'ARM64') {
        throw "Not an ARM64 PE: $Source"
    }
    $dest = Join-Path $stage $Relative
    New-Item -ItemType Directory -Path (Split-Path -Parent $dest) -Force | Out-Null
    Copy-Item -LiteralPath $inputFile -Destination $dest
}

if ($Target -eq 'Bundle') {
    Copy-Payload $appOut 'Windows-ARM64.apk.exe' 'Windows-ARM64.apk.exe'
}
foreach ($file in Get-ChildItem $native -File | Where-Object { $_.Name -in @('winapk.exe','winapk-bridge.dll','winapk-probe.jar','z.dll','sqlite3.dll','capstone.dll','lzma.dll') }) {
    Copy-Payload $native $file.Name "bin\$($file.Name)"
}
foreach ($req in 'winapk.exe','winapk-bridge.dll','winapk-probe.jar','z.dll','sqlite3.dll','capstone.dll') {
    if (!(Test-Path -LiteralPath (Join-Path $stage "bin\$req"))) { throw "Missing required runtime file: $req" }
}
if ($IncludeAngle) {
    $angleBin = Join-Path $build 'bin\angle'
    foreach ($dll in 'libEGL.dll','libGLESv2.dll') {
        Copy-Payload $angleBin $dll "bin\angle\$dll"
    }
}
# Reject a mixed Debug/Release build tree, including Debug CRT imports in dependency DLLs.
$readobj = Join-Path (Split-Path (Find-AndroidNdk)) 'llvm-readobj.exe'
foreach ($file in Get-ChildItem $stage -File -Recurse | Where-Object Extension -in '.dll','.exe') {
    $imports = & $readobj --coff-imports $file.FullName
    if ($LASTEXITCODE -or $imports -match '(?i)Name: (?:ucrtbased|(?:msvcp|vcruntime)\d+(?:_\d+)?d)\.dll') {
        throw "Package PE import audit failed: $($file.Name)"
    }
}

foreach ($file in 'android-framework.jar','macapk-loader.jar','bcprov-jdk18on-1.79.jar') {
    Copy-Payload $build "lib\$file" "lib\$file"
}
$skikoDir = Join-Path $build 'bin\skia'
if (Test-Path -LiteralPath (Join-Path $skikoDir 'skiko-windows-arm64.dll')) {
    Copy-Payload $skikoDir 'skiko-windows-arm64.dll' 'bin\skiko-windows-arm64.dll'
    if (Test-Path -LiteralPath (Join-Path $skikoDir 'icudtl.dat')) {
        Copy-Payload $skikoDir 'icudtl.dat' 'bin\icudtl.dat'
    }
}
foreach ($module in 'java_base_patch','jdk_unsupported_patch') {
    $root = Join-Path $build "lib\$module"
    $classes = @(Get-ChildItem -LiteralPath $root -Filter '*.class' -File -Recurse)
    if (!$classes.Count) { throw "Missing compiled patch module: $module" }
    foreach ($file in $classes) {
        $relative = [IO.Path]::GetRelativePath($root, $file.FullName)
        Copy-Payload $root $relative "lib\$module\$relative"
    }
}
$framework = 'bin\tools\framework-res.apk'
if ($lock.guest.frameworkRes.built -notmatch 'sha256 ([a-f0-9]{64})') { throw 'Missing locked framework resource hash' }
if ((Get-FileHash (Join-Path $build $framework) -Algorithm SHA256).Hash.ToLowerInvariant() -ne $Matches[1]) {
    throw 'Framework resource hash differs from build-lock.json'
}
Copy-Payload $build $framework $framework
foreach ($helper in 'bionic','jni') {
    $file = "bin\guest\libwinapk-$helper.so"
    $record = Get-Content (Join-Path $build "bin\guest\$helper.json") -Raw | ConvertFrom-Json
    $inputFile = Join-Path $build $file
    $bytes = [IO.File]::ReadAllBytes($inputFile)
    if ($bytes.Length -lt 64 -or [BitConverter]::ToUInt32($bytes,0) -ne 0x464C457F -or
        $bytes[4] -ne 2 -or $bytes[5] -ne 1 -or [BitConverter]::ToUInt16($bytes,18) -ne 183 -or
        (Get-FileHash $inputFile -Algorithm SHA256).Hash.ToLowerInvariant() -ne $record.sha256) {
        throw "Guest helper architecture or hash mismatch: $helper"
    }
    Copy-Payload $build $file $file
}

foreach ($file in 'README.md','LICENSE','THIRD_PARTY_NOTICES.md','build-lock.json') { Copy-Payload $repo $file $file }
Copy-Payload $repo 'macapk\LICENSE' 'licenses\Mac.apk-LICENSE'
Copy-Payload $repo 'macapk\THIRD_PARTY_NOTICES.txt' 'licenses\Mac.apk-NOTICES.txt'
foreach ($dep in @('zlib','sqlite3','capstone','vulkan-headers','liblzma')) {
    $cp = Join-Path $build "vcpkg_installed\arm64-windows\share\$dep\copyright"
    if (Test-Path -LiteralPath $cp) {
        Copy-Payload $build "vcpkg_installed\arm64-windows\share\$dep\copyright" "licenses\$dep.txt"
    }
}
if (Test-Path -LiteralPath (Join-Path $build 'deps\angle\LICENSE')) {
    Copy-Payload $build 'deps\angle\LICENSE' 'licenses\ANGLE-LICENSE'
}
if ($lock.guest.frameworkRes.source -notmatch 'tag (\S+),') { throw 'Missing framework resource source tag' }
Copy-Payload $build "framework-res\work-$($Matches[1])\src\NOTICE" 'licenses\AOSP-framework-res-NOTICE'
foreach ($lang in 'en-us','zh-cn') {
    foreach ($doc in 'README.md','development.md','graphics-capabilities.md','platform-notes.md','gui.md') {
        Copy-Payload $repo "doc\$lang\$doc" "doc\$lang\$doc"
    }
}

$sourceName = "$name-source.zip"
$sourceArchive = Join-Path $output $sourceName
Invoke-Checked 'git' @('-C', $repo, 'archive', '--format=zip', "--prefix=$name-source/", "--output=$sourceArchive", $tree,
    '--', '.', ':(exclude,glob)macapk/**/*.dylib')
$contents = @(Get-ChildItem -LiteralPath $stage -File -Recurse | Sort-Object FullName | ForEach-Object {
    [ordered]@{
        path = [IO.Path]::GetRelativePath($stage, $_.FullName).Replace('\','/')
        sha256 = (Get-FileHash $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
    }
})
$omitted = [System.Collections.Generic.List[string]]::new()
if (-not $IncludeAngle) { $omitted.Add('ANGLE') }
$omitted.AddRange([string[]]@('JVM (auto-installed privately by front end or supplied externally)', 'Vulkan loader and GPU drivers', 'fixture APKs'))
if ($Target -eq 'Cli') { $omitted.Add('frontend') }

[ordered]@{
    schemaVersion = 1; version = $version; configuration = $Configuration; arch = 'arm64'; target = $Target.ToLowerInvariant()
    profile = if ($Target -eq 'Bundle') { 'local Windows ARM64 experimental bundle (front end + runtime); not a public release' } else { 'local CLI experiment; not a release' }
    source = [ordered]@{ baseCommit = $head; tree = $tree; archive = $sourceName
        excludes = @('macapk/**/*.dylib')
        sha256 = (Get-FileHash $sourceArchive -Algorithm SHA256).Hash.ToLowerInvariant() }
    prerequisites = @('Windows 11 ARM64', 'ARM64 OpenJDK 21 with java.desktop (installed privately by front end or external)',
        'ARM64 Microsoft Visual C++ v14 Redistributable (external)')
    omitted = @($omitted)
    files = $contents
} | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $stage 'package.json') -Encoding utf8NoBOM

$archive = Join-Path $output "$name.zip"
[IO.Compression.ZipFile]::CreateFromDirectory($stage, $archive, [IO.Compression.CompressionLevel]::Fastest, $false)
Write-Step "Staged local $Target package: $archive"
Write-Output $archive
