<#
.SYNOPSIS
Runs Windows-ARM64.apk's runtime test suites on Windows 11 ARM64.

.DESCRIPTION
Suite Host is fixture T01 (PLAN.md §16): winapk doctor in a native ARM64
process, the JVM embedded and a JNI call each way, Java2D and fonts, the
upstream JVM options and patch modules, the machine type of every built PE;
and its negative case, doctor with no JVM to find, which must say so instead
of reporting success. Each check prints PASS or FAIL with its evidence; the
script exits non-zero when any fails. Build first:
scripts/windows/build.ps1 -Component All.

.PARAMETER Suite
Host.
#>
[CmdletBinding()]
param(
    [ValidateSet('Host')]
    [string]$Suite = 'Host'
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
. "$PSScriptRoot\common.ps1"

$repo = Get-RepoRoot
$out = Join-Path $repo 'build\windows-arm64'
$winapk = Join-Path $out 'bin\winapk.exe'
$script:failed = 0

<# Records one check: PASS or FAIL, with what was seen. #>
function Check([string]$Id, [bool]$Ok, [string]$Evidence) {
    if ($Ok) { Write-Host "PASS  $Id  $Evidence" } else { Write-Host "FAIL  $Id  $Evidence"; $script:failed++ }
}

<# Runs doctor --json with an environment of its own and returns the report. #>
function Invoke-Doctor([hashtable]$Env, [string[]]$Arguments = @()) {
    $saved = @{}
    foreach ($k in $Env.Keys) { $saved[$k] = [Environment]::GetEnvironmentVariable($k); [Environment]::SetEnvironmentVariable($k, $Env[$k]) }
    try {
        $json = & $winapk doctor --json @Arguments
        if ($LASTEXITCODE -ne 0) { throw "winapk doctor exited with $LASTEXITCODE" }
        ($json -join "`n") | ConvertFrom-Json
    } finally {
        foreach ($k in $saved.Keys) { [Environment]::SetEnvironmentVariable($k, $saved[$k]) }
    }
}

function Test-Host {
    if (-not (Test-Path $winapk)) { throw "$winapk is missing: run scripts/windows/build.ps1 -Component Host" }
    $jdk = Find-Jdk

    # T01: every built PE is ARM64 (AGENTS.md: no ARM64EC or x64 components).
    foreach ($f in Get-ChildItem (Join-Path $out 'bin') -Include *.exe, *.dll -Recurse) {
        $m = Get-PeMachine $f.FullName
        Check "T01.pe.$($f.Name)" ($m -eq 'ARM64') $m
    }

    # T01 positive: the JDK the build uses, embedded in winapk.exe.
    $r = Invoke-Doctor @{ WINAPK_JAVA_HOME = $jdk }
    Check 'T01.schema' ($r.schemaVersion -eq 1) "schemaVersion=$($r.schemaVersion)"
    Check 'T01.version' ($r.version -eq (& $winapk --version).Split(' ')[1]) "version=$($r.version)"
    Check 'T01.host.native' ($r.host.osArch -eq 'arm64' -and $r.host.processArch -eq 'arm64') "os=$($r.host.osArch) process=$($r.host.processArch)"
    Check 'T01.jvm.arch' ($r.jvm.arch -eq 'arm64') "jvm.dll=$($r.jvm.arch) ($($r.jvm.vendor) $($r.jvm.version))"
    Check 'T01.jvm.started' ($r.jvm.started -eq 'ok') $r.jvm.started
    $p = $r.jvm.probe
    Check 'T01.jvm.osArch' ($p.osArch -eq 'aarch64') "os.arch=$($p.osArch)"
    Check 'T01.jni.roundtrip' ($p.jni -eq 'ok') "jni=$($p.jni)"
    Check 'T01.java2d' ($p.java2d -eq 'ok') "java2d=$($p.java2d)"
    Check 'T01.fonts' ([int]$p.fontFamilies -gt 0) "fontFamilies=$($p.fontFamilies)"
    Check 'T01.jvm.desktop' ($r.jvm.javaDesktop -and $r.jvm.jniHeaders) "java.desktop=$($r.jvm.javaDesktop) jniHeaders=$($r.jvm.jniHeaders)"
    Check 'T01.vulkan.loader' ($r.graphics.vulkanLoader -eq 'available' -and $r.graphics.vulkanLoaderArch -eq 'arm64') "loader=$($r.graphics.vulkanLoader) arch=$($r.graphics.vulkanLoaderArch)"
    Check 'T01.guestVulkan.off' ($r.runtime.guestVulkan -eq $false) 'guest Vulkan is not reported as available'

    # T01: upstream's patch modules load on this JVM (built by -Component Java).
    $lib = Join-Path $out 'lib'
    $patches = @()
    foreach ($m in @(@{ D = 'java_base_patch'; M = 'java.base' }, @{ D = 'jdk_unsupported_patch'; M = 'jdk.unsupported' })) {
        $dir = Join-Path $lib $m.D
        if (Test-Path $dir) { $patches += "--patch-module=$($m.M)=$dir" }
    }
    if ($patches.Count -eq 2) {
        $v = & (Join-Path $jdk 'bin\java.exe') @patches --add-opens java.base/jdk.internal.misc=jdk.unsupported -version 2>&1
        Check 'T01.patch-module' ($LASTEXITCODE -eq 0) (($v | Select-Object -First 1) -as [string])
    } else {
        Check 'T01.patch-module' $false 'patch modules not built: run build.ps1 -Component Java'
    }

    # T01 negative: no JVM anywhere it looks; doctor must say so, not report success.
    $none = Join-Path $out 'no-jvm'
    New-Item -ItemType Directory -Force $none | Out-Null
    $n = Invoke-Doctor @{ WINAPK_JAVA_HOME = $null; JAVA_HOME = $null; LOCALAPPDATA = $none }
    Check 'T01.neg.jvm-missing' ($n.jvm.started -like 'not-run:*' -and -not $n.jvm.home) "started=$($n.jvm.started)"
    $ids = @($n.limitations | ForEach-Object id)
    Check 'T01.neg.limitation' ($ids -contains 'DEPENDENCY_MISSING') "limitations=$($ids -join ',')"

    # T01 negative: a --java-home that holds no JVM is not used.
    $bad = Invoke-Doctor @{ WINAPK_JAVA_HOME = $jdk } @('--java-home', (Join-Path $out 'not-a-jdk'))
    Check 'T01.neg.java-home-invalid' ($bad.jvm.source -ne 'argument') "an invalid --java-home is not used (source=$($bad.jvm.source))"
}

switch ($Suite) { 'Host' { Test-Host } }
if ($script:failed -gt 0) { Write-Host "$script:failed check(s) failed"; exit 1 }
Write-Host 'all checks passed'
