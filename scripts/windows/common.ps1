# Helpers shared by the scripts in scripts/windows. Dot-source it.
Set-StrictMode -Version Latest

<# Returns the repository's root. #>
function Get-RepoRoot { (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path }

<# Prints a step of a script. #>
function Write-Step([string]$Message) { Write-Host "[winapk] $Message" }

<#
Runs an external program and stops the script when it fails.
Inputs: the program and its arguments, passed as an array (never a string
a shell would split).
#>
function Invoke-Checked([string]$Program, [string[]]$Arguments) {
    & $Program @Arguments
    if ($LASTEXITCODE -ne 0) { throw "$([IO.Path]::GetFileName($Program)) exited with $LASTEXITCODE" }
}

<#
Empties a directory under build/ and makes it again.
Notes: refuses any path outside the repository's build/ folder, so a wrong
argument can never delete source or user data (PLAN.md §18).
#>
function Reset-Dir([string]$Path) {
    $build = Join-Path (Get-RepoRoot) 'build'
    $full = [IO.Path]::GetFullPath($Path)
    if (-not $full.StartsWith($build + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) {
        throw "refusing to clear ${full}: not under ${build}"
    }
    if (Test-Path -LiteralPath $full) { Remove-Item -LiteralPath $full -Recurse -Force }
    New-Item -ItemType Directory -Force $full | Out-Null
}

<# Quotes a path for a javac/jar @argfile: forward slashes, in quotes. #>
function Quote-ArgFile([string]$Path) { '"' + $Path.Replace('\', '/') + '"' }

<#
Returns the machine type of a PE file: ARM64, ARM64EC/ARM64X (hybrid), x64,
x86 or the raw value.
#>
function Get-PeMachine([string]$Path) {
    $b = [IO.File]::ReadAllBytes($Path)
    $pe = [BitConverter]::ToInt32($b, 0x3c)
    switch ([BitConverter]::ToUInt16($b, $pe + 4)) {
        0xAA64 { 'ARM64' }
        0xA641 { 'ARM64EC' }
        0x8664 { 'x64' }
        0x014C { 'x86' }
        default { '0x{0:X4}' -f $_ }
    }
}

<#
Finds the JDK to build and run with: WINAPK_JAVA_HOME, else JAVA_HOME.
Returns: its folder. Notes: it must be a JDK 21 whose jvm.dll is ARM64; an
emulated JVM is never used (AGENTS.md, "Native means native").
#>
function Find-Jdk {
    foreach ($jh in @($env:WINAPK_JAVA_HOME, $env:JAVA_HOME, [Environment]::GetEnvironmentVariable('JAVA_HOME', 'Machine'))) {
        if (-not $jh) { continue }
        $jh = $jh.TrimEnd('\')
        $jvm = Join-Path $jh 'bin\server\jvm.dll'
        $release = Join-Path $jh 'release'
        if (-not ((Test-Path $jvm) -and (Test-Path $release) -and (Test-Path (Join-Path $jh 'bin\javac.exe')))) { continue }
        $version = (Select-String -Path $release -Pattern '^JAVA_VERSION="(\d+)' | Select-Object -First 1).Matches.Groups[1].Value
        if ($version -ne '21') { throw "$jh is JDK $version; the build needs JDK 21 (build-lock.json)" }
        if ((Get-PeMachine $jvm) -ne 'ARM64') { throw "$jvm is not ARM64" }
        return $jh
    }
    throw 'No JDK 21 found: set WINAPK_JAVA_HOME or JAVA_HOME (see scripts/windows/check-requirements.ps1)'
}

<#
Finds Visual Studio with the ARM64 C++ tools.
Returns: its root and the folders of its CMake and Ninja.
#>
function Find-VisualStudio {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (-not (Test-Path $vswhere)) { throw 'Visual Studio Installer (vswhere.exe) not found' }
    $root = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.ARM64 -property installationPath
    if (-not $root) { throw 'No Visual Studio with the ARM64 C++ tools (VC.Tools.ARM64)' }
    $cm = Join-Path $root 'Common7\IDE\CommonExtensions\Microsoft\CMake'
    [pscustomobject]@{ Root = $root; CMakeDir = (Join-Path $cm 'CMake\bin'); NinjaDir = (Join-Path $cm 'Ninja') }
}

<#
Runs a program in Visual Studio's ARM64 developer environment: vcvarsall arm64
(clang-cl, the linker and the Windows SDK), with VS's ARM64 LLVM first on PATH.
Side effects: the environment changes for the program only; this shell's is
put back afterwards, and the system environment is never touched.
#>
function Invoke-InDevShell($Vs, [string[]]$Command) {
    if (-not (Get-Variable -Scope Script -Name devEnv -ErrorAction SilentlyContinue)) {
        $vcvars = Join-Path $Vs.Root 'VC\Auxiliary\Build\vcvarsall.bat'
        $lines = & cmd.exe /d /c "`"$vcvars`" arm64 >nul && set"
        if ($LASTEXITCODE -ne 0) { throw 'vcvarsall.bat arm64 failed' }
        $script:devEnv = @{}
        foreach ($l in $lines) { if ($l -match '^([^=]+)=(.*)$') { $script:devEnv[$Matches[1]] = $Matches[2] } }
        $script:devEnv['PATH'] = (Join-Path $Vs.Root 'VC\Tools\Llvm\ARM64\bin') + ';' + $script:devEnv['PATH']
    }
    $saved = @{}
    foreach ($k in $script:devEnv.Keys) {
        $saved[$k] = [Environment]::GetEnvironmentVariable($k)
        [Environment]::SetEnvironmentVariable($k, $script:devEnv[$k])
    }
    try {
        $program, $rest = $Command
        & $program @rest
        if ($LASTEXITCODE -ne 0) { throw "$([IO.Path]::GetFileName($program)) exited with $LASTEXITCODE" }
    } finally {
        foreach ($k in $saved.Keys) { [Environment]::SetEnvironmentVariable($k, $saved[$k]) }
    }
}
