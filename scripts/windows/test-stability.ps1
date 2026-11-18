<#
.SYNOPSIS
Runs T14 stability and baseline measurements on Windows 11 ARM64 (PLAN.md §16).
.DESCRIPTION
Executes:
1. Cold + warm launch loop (default 20 total launches: 1 cold DEX conversion + 19 warm cached launches),
   recording cold launch ms, DEX conversion ms, and warm launch p50/p95 ms.
2. 100 resize / minimize-restore cycles on a live APK window, measuring post-warmup vs final RSS,
   commit (PrivateMemorySize64), handle count and thread count, asserting zero deadlock/crash and
   bounded post-warmup resource growth.
3. Continuous endurance run (default 60s for smoke; pass -EnduranceSeconds 1800 for the full 30-minute
   T14 gate), sampling live frame cadence and process resources and writing stability-baseline.json.
#>
[CmdletBinding()]
param(
    [string]$WinApkExe = '',
    [int]$LaunchCount = 20,
    [int]$ResizeMinimizeCycles = 100,
    [int]$EnduranceSeconds = 60,
    [string]$ReportPath = ''
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
. "$PSScriptRoot\common.ps1"

$repo = Get-RepoRoot
$out = Join-Path $repo 'build\windows-arm64'
if (-not $WinApkExe) { $WinApkExe = Join-Path $out 'bin\winapk.exe' }
if (-not (Test-Path -LiteralPath $WinApkExe)) { throw "Missing winapk.exe at $WinApkExe" }
$apk = Join-Path $repo 'tests\fixtures\hello-java.apk'
if (-not (Test-Path -LiteralPath $apk)) { throw "Missing fixture $apk; run build.ps1 -Component Fixtures" }
$jdk = Find-Jdk

Add-Type @"
using System;
using System.Runtime.InteropServices;
public static class WinApkStabWin {
    [DllImport("user32.dll", CharSet = CharSet.Unicode)] public static extern IntPtr FindWindowW(string cls, string title);
    [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
    [DllImport("user32.dll")] public static extern bool PostMessageW(IntPtr h, uint msg, IntPtr w, IntPtr l);
    [DllImport("user32.dll")] public static extern bool SetWindowPos(IntPtr h, IntPtr after, int x, int y, int cx, int cy, uint flags);
}
"@

function Get-Percentile([double[]]$Values, [double]$P) {
    if (-not $Values -or $Values.Count -eq 0) { return 0.0 }
    $sorted = @($Values | Sort-Object)
    $idx = [Math]::Min($sorted.Count - 1, [Math]::Max(0, [int][Math]::Ceiling($sorted.Count * $P) - 1))
    return [Math]::Round([double]$sorted[$idx], 1)
}

function Read-SharedLog([string]$LogFile) {
    if (-not (Test-Path -LiteralPath $LogFile)) { return '' }
    $fs = [IO.File]::Open($LogFile, 'Open', 'Read', 'ReadWrite')
    try { (New-Object IO.StreamReader $fs).ReadToEnd() } finally { $fs.Dispose() }
}

function Find-WindowByPid([int]$TargetPid, [string]$Title, [int]$TimeoutSec, $Proc) {
    $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSec)
    while ([DateTime]::UtcNow -lt $deadline) {
        if ($Proc -and $Proc.HasExited) { break }
        $h = [WinApkStabWin]::FindWindowW('WinApkAppWindow', $Title)
        if ($h -ne [IntPtr]::Zero) {
            [uint32]$ownerPid = 0
            [void][WinApkStabWin]::GetWindowThreadProcessId($h, [ref]$ownerPid)
            if ([int]$ownerPid -eq $TargetPid) { return $h }
        }
        Start-Sleep -Milliseconds 40
    }
    return [IntPtr]::Zero
}

function Wait-LogMatch([string]$LogFile, [string]$Pattern, [int]$TimeoutSec, $Proc) {
    $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSec)
    while ([DateTime]::UtcNow -lt $deadline) {
        $txt = Read-SharedLog $LogFile
        if ($txt -and $txt -match $Pattern) { return $Matches }
        if ($Proc -and $Proc.HasExited) { break }
        Start-Sleep -Milliseconds 40
    }
    return $null
}

$work = Join-Path $out ('test-data\stability-' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff') + '-' + [guid]::NewGuid().ToString('N').Substring(0,6))
New-Item -ItemType Directory -Force $work | Out-Null
if (-not $ReportPath) { $ReportPath = Join-Path $work 'stability-baseline.json' }
$dataDir = Join-Path $work 'data'
$failed = 0
$samples = [System.Collections.Generic.List[object]]::new()

function Check-Stab([string]$Id, [bool]$Ok, [string]$Evidence) {
    if ($Ok) { Write-Host "PASS  $Id  $Evidence" }
    else { Write-Host "FAIL  $Id  $Evidence"; $script:failed++ }
}

function Start-StabApp([string]$LogFile) {
    $argList = @('run', "`"$apk`"", '--native=off', '--graphics=off', '--data-dir', "`"$dataDir`"", '--java-home', "`"$jdk`"", '--size', '360x720')
    Start-Process -FilePath $WinApkExe -ArgumentList $argList `
        -RedirectStandardError $LogFile -RedirectStandardOutput "$LogFile.out" -PassThru -NoNewWindow
}

# 1. Cold + warm launch loop (default 20 launches)
Write-Step "T14.relaunch: running $LaunchCount launches (1 cold + $($LaunchCount - 1) warm)"
$coldMs = 0.0
$dexMs = 0.0
$warmTimes = [System.Collections.Generic.List[double]]::new()
$launchOk = $true

for ($i = 0; $i -lt $LaunchCount; $i++) {
    $log = Join-Path $work "launch-$i.log"
    $sw = [Diagnostics.Stopwatch]::StartNew()
    $p = Start-StabApp $log
    try {
        $hwnd = Find-WindowByPid $p.Id 'Hello Java' 30 $p
        $hit = Wait-LogMatch $log 'ViewCanvasDriver: macapkDrawFrame fire #1 present \d+x\d+ nonzero=[1-9]\d*/' 30 $p
        $sw.Stop()
        if ($hwnd -eq [IntPtr]::Zero -or -not $hit) {
            $launchOk = $false
            break
        }
        if ($i -eq 0) {
            $coldMs = [Math]::Round($sw.Elapsed.TotalMilliseconds, 1)
        } else {
            $warmTimes.Add($sw.Elapsed.TotalMilliseconds)
        }
        [void][WinApkStabWin]::PostMessageW($hwnd, 0x0010, [IntPtr]::Zero, [IntPtr]::Zero) # WM_CLOSE
        if (-not $p.WaitForExit(15000) -or $p.ExitCode -ne 0) {
            $launchOk = $false
            break
        }
    } finally {
        if (-not $p.HasExited) { $p.Kill(); $p.WaitForExit() }
        $p.Dispose()
    }
    if ($i -eq 0) {
        $txt = Read-SharedLog $log
        if ($txt -match 'stage dex-converted \((\d+) ms\)') { $dexMs = [double]$Matches[1] }
    }
}
$warmP50 = Get-Percentile $warmTimes.ToArray() 0.50
$warmP95 = Get-Percentile $warmTimes.ToArray() 0.95
Check-Stab 'T14.relaunch' $launchOk "launches=$LaunchCount cold=${coldMs}ms dex=${dexMs}ms warm_p50=${warmP50}ms warm_p95=${warmP95}ms"

# 2. 100 resize + minimize/restore cycles + endurance sampling on a live runtime session
$cycleLog = Join-Path $work 'cycles-endurance.log'
$proc = Start-StabApp $cycleLog

$warmupRssKB = 0L; $warmupCommitKB = 0L; $warmupHandles = 0; $warmupThreads = 0
$finalRssKB = 0L; $finalCommitKB = 0L; $finalHandles = 0; $finalThreads = 0
$frameIntervals = [System.Collections.Generic.List[double]]::new()
$cyclesOk = $false
$enduranceOk = $false

try {
    $hwnd = Find-WindowByPid $proc.Id 'Hello Java' 30 $proc
    $firstHit = Wait-LogMatch $cycleLog 'ViewCanvasDriver: macapkDrawFrame fire #1 present' 30 $proc
    if ($hwnd -ne [IntPtr]::Zero -and $firstHit) {
        # Warm-up 10 transitions before baseline snapshot (PLAN.md §16: compare after warm-up)
        for ($w = 0; $w -lt 10; $w++) {
            [void][WinApkStabWin]::SetWindowPos($hwnd, [IntPtr]::Zero, 0, 0, 380 + ($w % 2) * 40, 740 + ($w % 2) * 30, 0x0016)
            Start-Sleep -Milliseconds 25
        }
        $proc.Refresh()
        $warmupRssKB = [long]($proc.WorkingSet64 / 1KB)
        $warmupCommitKB = [long]($proc.PrivateMemorySize64 / 1KB)
        $warmupHandles = $proc.HandleCount
        $warmupThreads = $proc.Threads.Count

        Write-Step "T14.resizeMinimize: running $ResizeMinimizeCycles resize and minimize/restore cycles"
        $cyclesOk = $true
        for ($c = 0; $c -lt $ResizeMinimizeCycles; $c++) {
            if ($proc.HasExited) { $cyclesOk = $false; break }
            $wPx = 360 + (($c % 5) * 24)
            $hPx = 700 + (($c % 4) * 20)
            [void][WinApkStabWin]::SetWindowPos($hwnd, [IntPtr]::Zero, 0, 0, $wPx, $hPx, 0x0016)
            if (($c % 4) -eq 0) {
                [void][WinApkStabWin]::PostMessageW($hwnd, 0x0112, [IntPtr]0xF020, [IntPtr]0) # SC_MINIMIZE
                Start-Sleep -Milliseconds 20
                [void][WinApkStabWin]::PostMessageW($hwnd, 0x0112, [IntPtr]0xF120, [IntPtr]0) # SC_RESTORE
            }
            $t0 = [Diagnostics.Stopwatch]::GetTimestamp()
            $lp = [IntPtr]((120 -band 0xFFFF) -bor ((120 -band 0xFFFF) -shl 16))
            [void][WinApkStabWin]::PostMessageW($hwnd, 0x0201, [IntPtr]1, $lp)
            [void][WinApkStabWin]::PostMessageW($hwnd, 0x0202, [IntPtr]0, $lp)
            Start-Sleep -Milliseconds 16
            $dtMs = ([Diagnostics.Stopwatch]::GetTimestamp() - $t0) * 1000.0 / [Diagnostics.Stopwatch]::Frequency
            $frameIntervals.Add($dtMs)
        }
        $proc.Refresh()
        $afterCyclesCommitKB = [long]($proc.PrivateMemorySize64 / 1KB)
        $afterCyclesHandles = $proc.HandleCount
        $commitDeltaKB = $afterCyclesCommitKB - $warmupCommitKB
        $handleDelta = $afterCyclesHandles - $warmupHandles
        $cyclesOk = $cyclesOk -and (-not $proc.HasExited) -and ($commitDeltaKB -lt 98304) -and ($handleDelta -lt 250)
        Check-Stab 'T14.resizeMinimize' $cyclesOk "cycles=$ResizeMinimizeCycles warmupCommit=${warmupCommitKB}KB afterCommit=${afterCyclesCommitKB}KB (delta=${commitDeltaKB}KB) handles=${warmupHandles}->${afterCyclesHandles}"

        Write-Step "T14.endurance: running continuous session for ${EnduranceSeconds}s"
        $endSw = [Diagnostics.Stopwatch]::StartNew()
        $enduranceOk = $true
        $tick = 0
        while ($endSw.Elapsed.TotalSeconds -lt $EnduranceSeconds) {
            if ($proc.HasExited) { $enduranceOk = $false; break }
            $lp = [IntPtr]((140 -band 0xFFFF) -bor ((160 -band 0xFFFF) -shl 16))
            [void][WinApkStabWin]::PostMessageW($hwnd, 0x0201, [IntPtr]1, $lp)
            [void][WinApkStabWin]::PostMessageW($hwnd, 0x0202, [IntPtr]0, $lp)
            if (($tick % 10) -eq 0) {
                $proc.Refresh()
                $samples.Add([ordered]@{
                    elapsedSec = [Math]::Round($endSw.Elapsed.TotalSeconds, 1)
                    rssKB = [long]($proc.WorkingSet64 / 1KB)
                    commitKB = [long]($proc.PrivateMemorySize64 / 1KB)
                    handles = $proc.HandleCount
                    threads = $proc.Threads.Count
                })
            }
            $tick++
            $sleepSec = [Math]::Min(1.0, [Math]::Max(0.1, $EnduranceSeconds - $endSw.Elapsed.TotalSeconds))
            Start-Sleep -Milliseconds ([int]($sleepSec * 1000))
        }
        $proc.Refresh()
        $finalRssKB = [long]($proc.WorkingSet64 / 1KB)
        $finalCommitKB = [long]($proc.PrivateMemorySize64 / 1KB)
        $finalHandles = $proc.HandleCount
        $finalThreads = $proc.Threads.Count

        [void][WinApkStabWin]::PostMessageW($hwnd, 0x0010, [IntPtr]::Zero, [IntPtr]::Zero)
        $cleanExit = $proc.WaitForExit(15000) -and ($proc.ExitCode -eq 0)
        $enduranceCommitDeltaKB = $finalCommitKB - $afterCyclesCommitKB
        $enduranceHandleDelta = $finalHandles - $afterCyclesHandles
        $enduranceOk = $enduranceOk -and $cleanExit -and ($enduranceCommitDeltaKB -lt 98304) -and ($enduranceHandleDelta -lt 250)
        Check-Stab 'T14.endurance' $enduranceOk "duration=${EnduranceSeconds}s commit=${afterCyclesCommitKB}->${finalCommitKB}KB handles=${afterCyclesHandles}->${finalHandles} threads=${warmupThreads}->${finalThreads} cleanExit=$cleanExit"
    } else {
        Check-Stab 'T14.resizeMinimize' $false 'window or initial frame did not appear'
        Check-Stab 'T14.endurance' $false 'skipped because window did not start'
    }
} finally {
    if (-not $proc.HasExited) { $proc.Kill(); $proc.WaitForExit() }
    $proc.Dispose()
}

$frameP50 = Get-Percentile $frameIntervals.ToArray() 0.50
$frameP95 = Get-Percentile $frameIntervals.ToArray() 0.95
$report = [ordered]@{
    timestampUtc = [DateTime]::UtcNow.ToString('o')
    winapk = $WinApkExe
    fixture = 'hello-java.apk'
    launches = [ordered]@{
        count = $LaunchCount
        coldMs = $coldMs
        dexConvertMs = $dexMs
        warmP50Ms = $warmP50
        warmP95Ms = $warmP95
    }
    resizeMinimizeCycles = $ResizeMinimizeCycles
    enduranceSeconds = $EnduranceSeconds
    cadence = [ordered]@{
        dispatchCycleP50Ms = $frameP50
        dispatchCycleP95Ms = $frameP95
    }
    resources = [ordered]@{
        warmup = [ordered]@{ rssKB = $warmupRssKB; commitKB = $warmupCommitKB; handles = $warmupHandles; threads = $warmupThreads }
        final = [ordered]@{ rssKB = $finalRssKB; commitKB = $finalCommitKB; handles = $finalHandles; threads = $finalThreads }
    }
    samples = $samples
    passed = ($failed -eq 0)
}
$report | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $ReportPath -Encoding utf8NoBOM
Write-Step "Stability baseline written to $ReportPath"
if ($failed -gt 0) { exit 1 }
exit 0
