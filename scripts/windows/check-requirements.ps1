<#
.SYNOPSIS
Checks this machine against build-lock.json. Installs nothing and changes no
setting (PLAN.md §18).

.DESCRIPTION
Each requirement prints OK, MISSING or DIFFERENT with what was found. A
locked requirement that is missing or different makes the script exit 1; a
chosen one not installed yet (the Android SDK and NDK, needed from P1/P2) is
reported as LATER and does not. How to install each is in
doc/en-us/development.md.
#>
[CmdletBinding()]
param()
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
. "$PSScriptRoot\common.ps1"

$lock = Get-Content (Join-Path (Get-RepoRoot) 'build-lock.json') -Raw | ConvertFrom-Json
$script:bad = 0

<# Prints one requirement's result; a failed locked one counts against the exit code. #>
function Report([string]$Name, [string]$State, [string]$Found) {
    Write-Host ('{0,-10} {1,-16} {2}' -f $State, $Name, $Found)
    if ($State -in 'MISSING', 'DIFFERENT') { $script:bad++ }
}

$arch = $env:PROCESSOR_ARCHITECTURE
Report 'Windows' ($(if ($arch -eq 'ARM64') { 'OK' } else { 'DIFFERENT' })) "$([Environment]::OSVersion.Version) $arch"

try {
    $jdk = Find-Jdk
    $v = (Select-String -Path (Join-Path $jdk 'release') -Pattern '^JAVA_VERSION="([^"]+)"').Matches.Groups[1].Value
    $want = ($lock.host.jdk.version -split ' ')[0]
    Report 'JDK' ($(if ($v.StartsWith($want)) { 'OK' } else { 'DIFFERENT' })) "$v arm64 at $jdk (locked $want)"
} catch { Report 'JDK' 'MISSING' $_.Exception.Message }

try {
    $vs = Find-VisualStudio
    Report 'VisualStudio' 'OK' $vs.Root
    $clang = Join-Path $vs.Root 'VC\Tools\Llvm\ARM64\bin\clang-cl.exe'
    if (Test-Path $clang) {
        $cv = ((& $clang --version) | Select-Object -First 1) -replace '^clang version (\S+).*', '$1'
        Report 'clang-cl' ($(if ($cv -eq $lock.host.'clang-cl'.version) { 'OK' } else { 'DIFFERENT' })) "$cv (locked $($lock.host.'clang-cl'.version))"
    } else { Report 'clang-cl' 'MISSING' 'VS component VC.Llvm.Clang' }
    $msvc = Get-ChildItem (Join-Path $vs.Root 'VC\Tools\MSVC') -Directory | Sort-Object Name | Select-Object -Last 1
    Report 'MSVC' ($(if ($msvc -and $msvc.Name -eq $lock.host.msvc.version) { 'OK' } else { 'DIFFERENT' })) "$($msvc.Name) (locked $($lock.host.msvc.version))"
    $cmakeV = ((& (Join-Path $vs.CMakeDir 'cmake.exe') --version) | Select-Object -First 1) -replace '^cmake version ', ''
    Report 'CMake' ($(if ($cmakeV -eq $lock.host.cmake.version) { 'OK' } else { 'DIFFERENT' })) "$cmakeV (locked $($lock.host.cmake.version))"
    $ninjaV = & (Join-Path $vs.NinjaDir 'ninja.exe') --version
    Report 'Ninja' ($(if ($ninjaV -eq $lock.host.ninja.version) { 'OK' } else { 'DIFFERENT' })) "$ninjaV (locked $($lock.host.ninja.version))"
    $vcpkg = Join-Path $vs.Root 'VC\vcpkg\vcpkg.exe'
    if (Test-Path $vcpkg) {
        $kv = ((& $vcpkg version) | Select-Object -First 1) -replace '.*version (\d{4}-\d{2}-\d{2}).*', '$1'
        Report 'vcpkg' ($(if ($kv -eq $lock.host.vcpkg.version) { 'OK' } else { 'DIFFERENT' })) "$kv (locked $($lock.host.vcpkg.version))"
    } else { Report 'vcpkg' 'MISSING' 'VS component Vcpkg' }
} catch { Report 'VisualStudio' 'MISSING' $_.Exception.Message }

$sdk = 'C:\Program Files (x86)\Windows Kits\10\Include\' + $lock.host.windowsSdk.version
Report 'WindowsSDK' ($(if (Test-Path $sdk) { 'OK' } else { 'MISSING' })) $sdk

$vk = Join-Path $env:SystemRoot 'System32\vulkan-1.dll'
if (Test-Path $vk) { Report 'VulkanLoader' 'OK' "$((Get-Item $vk).VersionInfo.FileVersion) $(Get-PeMachine $vk) (system; winapk doctor reports devices)" }
else { Report 'VulkanLoader' 'LATER' 'no vulkan-1.dll: graphics gates cannot run here' }

# Chosen, needed from P1 (SDK) and P2 (NDK): reported, never required yet.
$sdkRoot = @($env:ANDROID_HOME, $env:ANDROID_SDK_ROOT, (Join-Path $env:LOCALAPPDATA 'Android\Sdk')) | Where-Object { $_ -and (Test-Path $_) } | Select-Object -First 1
if ($sdkRoot) { Report 'AndroidSDK' 'OK' $sdkRoot } else { Report 'AndroidSDK' 'LATER' "not installed; needed from P1 ($($lock.guest.androidSdk.packages -join ', '))" }
$ndk = if ($sdkRoot) { Join-Path $sdkRoot ('ndk\' + ($lock.guest.androidNdk.version -replace '.*\((.*)\)', '$1')) }
if ($ndk -and (Test-Path $ndk)) { Report 'AndroidNDK' 'OK' $ndk } else { Report 'AndroidNDK' 'LATER' "not installed; needed from P2 ($($lock.guest.androidNdk.version))" }

if ($script:bad -gt 0) { Write-Host "$script:bad requirement(s) missing or different"; exit 1 }
Write-Host 'requirements met for the Java and Host components'
