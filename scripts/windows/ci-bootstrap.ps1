<#
.SYNOPSIS
Bootstraps locked build-time dependencies into .dev/deps on a Windows 11 ARM64 CI runner.
.DESCRIPTION
Prepares (and caches across GitHub Actions runs):
1. Native ARM64 Microsoft Build of OpenJDK 21 (SHA-256 and 0xAA64 PE verified).
2. Visual Studio ARM64 C++ / clang-cl toolchain check (installs VC.Llvm.Clang only in CI if absent).
3. Android SDK build-tools;36.0.0, platforms;android-36 and ndk;30.0.16248370.
4. Kotlin compiler 2.4.0 for the Kotlin fixture T04.
Never modifies global system PATH or machine environment variables; exports paths to $env:GITHUB_ENV when set.
#>
[CmdletBinding()]
param(
    [string]$Prefix = ''
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
. "$PSScriptRoot\common.ps1"

$repo = Get-RepoRoot
if (-not $Prefix) { $Prefix = Join-Path $repo '.dev\deps' }
New-Item -ItemType Directory -Force $Prefix | Out-Null
$downloads = Join-Path $Prefix 'downloads'
New-Item -ItemType Directory -Force $downloads | Out-Null
$lock = Get-Content (Join-Path $repo 'build-lock.json') -Raw | ConvertFrom-Json

function Export-CiVar([string]$Name, [string]$Value) {
    [Environment]::SetEnvironmentVariable($Name, $Value)
    if ($env:GITHUB_ENV) {
        "$Name=$Value" | Add-Content -LiteralPath $env:GITHUB_ENV -Encoding utf8NoBOM
    }
}

# 1. Visual Studio ARM64 + clang-cl check
$vs = Find-VisualStudio
$vsClang = Join-Path $vs.Root 'VC\Tools\Llvm\ARM64\bin\clang-cl.exe'
$sysClang = Join-Path $env:ProgramFiles 'LLVM\bin\clang-cl.exe'
if (-not (Test-Path $vsClang) -and -not (Test-Path $sysClang)) {
    if (-not $env:GITHUB_ACTIONS) {
        throw 'clang-cl.exe not found in Visual Studio VC\Tools\Llvm\ARM64\bin or Program Files\LLVM\bin'
    }
    Write-Step 'Installing Microsoft.VisualStudio.Component.VC.Llvm.Clang on CI runner'
    $setupExe = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\setup.exe'
    Invoke-Checked $setupExe @('modify', '--installPath', $vs.Root, '--add', 'Microsoft.VisualStudio.Component.VC.Llvm.Clang', '--quiet', '--norestart', '--wait')
}

# 2. ARM64 Microsoft Build of OpenJDK 21
$jdkHome = ''
try { $jdkHome = Find-Jdk } catch { $jdkHome = '' }
if (-not $jdkHome) {
    $jdkRoot = Join-Path $Prefix 'jdk-21'
    $existingJvm = Get-ChildItem $jdkRoot -Filter 'jvm.dll' -Recurse -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($existingJvm) {
        $jdkHome = Split-Path (Split-Path (Split-Path $existingJvm.FullName))
    } else {
        $jdkZipName = 'microsoft-jdk-21.0.13.0.2-windows-aarch64.zip'
        $jdkSha = '9c1f36b1660d97ab228787294433eefaaad3e3e8963bdd3cf975b02171137ecc'
        $jdkZip = Join-Path $downloads $jdkZipName
        if (-not (Test-Path $jdkZip) -or (Get-FileHash $jdkZip -Algorithm SHA256).Hash.ToLower() -ne $jdkSha) {
            Write-Step "Downloading ARM64 OpenJDK 21 ($jdkZipName)"
            Invoke-WebRequest -Uri "https://aka.ms/download-jdk/$jdkZipName" -OutFile "$jdkZip.tmp" -TimeoutSec 300
            $got = (Get-FileHash "$jdkZip.tmp" -Algorithm SHA256).Hash.ToLower()
            if ($got -ne $jdkSha) {
                Remove-Item "$jdkZip.tmp" -Force -ErrorAction SilentlyContinue
                throw "OpenJDK 21 SHA-256 mismatch: expected $jdkSha, got $got"
            }
            Move-Item "$jdkZip.tmp" $jdkZip -Force
        }
        Remove-Item $jdkRoot -Recurse -Force -ErrorAction SilentlyContinue
        New-Item -ItemType Directory -Force $jdkRoot | Out-Null
        Expand-Archive -LiteralPath $jdkZip -DestinationPath $jdkRoot -Force
        $jvmDll = Get-ChildItem $jdkRoot -Filter 'jvm.dll' -Recurse | Select-Object -First 1
        if (-not $jvmDll -or (Get-PeMachine $jvmDll.FullName) -ne 'ARM64') {
            throw 'Extracted OpenJDK 21 jvm.dll is missing or not ARM64'
        }
        $jdkHome = Split-Path (Split-Path (Split-Path $jvmDll.FullName))
    }
}
Export-CiVar 'WINAPK_JAVA_HOME' $jdkHome
Export-CiVar 'JAVA_HOME' $jdkHome
Write-Step "ARM64 JDK 21 ready: $jdkHome"

# 3. Android SDK + NDK
$sdkReady = $false
try {
    $null = Find-AndroidSdk
    $null = Find-AndroidNdk
    $sdkReady = $true
} catch { $sdkReady = $false }

if (-not $sdkReady) {
    $sdkRoot = Join-Path $Prefix 'android-sdk'
    New-Item -ItemType Directory -Force $sdkRoot | Out-Null
    Export-CiVar 'ANDROID_HOME' $sdkRoot
    Export-CiVar 'ANDROID_SDK_ROOT' $sdkRoot
    $sdkMgr = Join-Path $sdkRoot 'cmdline-tools\latest\bin\sdkmanager.bat'
    if (-not (Test-Path $sdkMgr)) {
        $cmdZip = Join-Path $downloads 'commandlinetools-win-13114758_latest.zip'
        if (-not (Test-Path $cmdZip)) {
            Write-Step 'Downloading Android SDK command-line tools'
            Invoke-WebRequest -Uri 'https://dl.google.com/android/repository/commandlinetools-win-13114758_latest.zip' -OutFile "$cmdZip.tmp" -TimeoutSec 300
            Move-Item "$cmdZip.tmp" $cmdZip -Force
        }
        $tmpUnpack = Join-Path $sdkRoot 'cmdline-tools-tmp'
        Remove-Item $tmpUnpack -Recurse -Force -ErrorAction SilentlyContinue
        Expand-Archive -LiteralPath $cmdZip -DestinationPath $tmpUnpack -Force
        $latestDir = Join-Path $sdkRoot 'cmdline-tools\latest'
        New-Item -ItemType Directory -Force (Split-Path $latestDir) | Out-Null
        if (Test-Path $latestDir) { Remove-Item $latestDir -Recurse -Force }
        Move-Item (Join-Path $tmpUnpack 'cmdline-tools') $latestDir
        Remove-Item $tmpUnpack -Recurse -Force
    }
    $ndkVer = $lock.guest.androidNdk.version -replace '.*\((.*)\).*', '$1'
    $pkgs = @($lock.guest.androidSdk.packages) + @("ndk;$ndkVer")
    Write-Step "Installing locked Android SDK/NDK packages: $($pkgs -join ', ')"
    $yes = ('y' + [Environment]::NewLine) * 30
    $yes | & $sdkMgr "--sdk_root=$sdkRoot" --licenses | Out-Null
    Invoke-Checked $sdkMgr (@("--sdk_root=$sdkRoot") + $pkgs)
} else {
    $foundSdk = (Find-AndroidSdk).Root
    Export-CiVar 'ANDROID_HOME' $foundSdk
    Export-CiVar 'ANDROID_SDK_ROOT' $foundSdk
}
Write-Step "Android SDK/NDK ready: $env:ANDROID_HOME"

# 4. Kotlin compiler (for T04 fixture)
$ktDir = ''
try { $ktDir = Find-Kotlinc } catch { $ktDir = '' }
if (-not $ktDir) {
    $ktVer = $lock.guest.kotlinc.version -replace '\s*\(.*\)$', ''
    $ktRoot = Join-Path $Prefix "kotlin-$ktVer"
    $ktCandidate = Join-Path $ktRoot 'kotlinc'
    if (-not (Test-Path (Join-Path $ktCandidate 'bin\kotlinc.bat'))) {
        $ktZip = Join-Path $downloads "kotlin-compiler-$ktVer.zip"
        if (-not (Test-Path $ktZip)) {
            Write-Step "Downloading Kotlin compiler $ktVer"
            Invoke-WebRequest -Uri "https://github.com/JetBrains/kotlin/releases/download/v$ktVer/kotlin-compiler-$ktVer.zip" -OutFile "$ktZip.tmp" -TimeoutSec 300
            Move-Item "$ktZip.tmp" $ktZip -Force
        }
        Remove-Item $ktRoot -Recurse -Force -ErrorAction SilentlyContinue
        Expand-Archive -LiteralPath $ktZip -DestinationPath $ktRoot -Force
    }
    $ktDir = $ktCandidate
}
Export-CiVar 'KOTLIN_HOME' $ktDir
Write-Step "Kotlinc ready: $ktDir"
