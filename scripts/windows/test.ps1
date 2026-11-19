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

Suite Java is G1 (PLAN.md §8): fixtures T02 (the Win32 bitmap presenter: pixel
format, size, message pump), T03 (hello-java.apk: DEX conversion, the
Activity's lifecycle, taps, typing keys and Unicode characters, scrolling, a
resize, a press cancelled by losing focus, Back, minimise and restore, SQLite,
closing, then a second run that restores the saved state from the cache and
checks the UI-call timeout with the UI thread suspended, then a run from a
Unicode path) and T04 (hello-kotlin.apk, G1.6), driven by posting window
messages to the app's window and reading the runtime's log; and its negative
cases, a file that is not an APK, an unsupported --native mode, an APK library
with guest native off and a query of a missing SQLite table. It
needs a desktop session. Build first: build.ps1 -Component All.

Suite Native is G2 (PLAN.md §9), without a JVM or a window: fixture T05 (the
N0 ABI suite of native-probe.exe on four builds of one guest library: integers,
pointers, floating point, the stack, HFAs and structures, imports, callbacks
both ways, reentry, callee-saved registers, x18 and sp, then 2000 load/run/
unload cycles and 32 copies at once) and T06 (malformed, foreign and out-of-
profile ELF files, each refused with its category and reason). Build first:
build.ps1 -Component NativeProbe.

Suite Bionic is the P3 thread foundation (T08), not G3: managed pthreads, guest
errno, TLS keys/destructors, explicitly emulated C TLS and untimed mutex/condition
synchronization; unsupported imports
and hardware TLS/syscall/register access remain refused. Each probe has a
60-second deadline. Build first: build.ps1 -Component NativeProbe.

.PARAMETER Suite
Host, Java, Native, Bionic, Jni, ApkJni, GraphicsHost, GraphicsJava, Surface, GraphicsGuest,
VulkanWsi, DexJob, Package, or a targeted Bionic subset.
.PARAMETER PackageZip
Local runtime ZIP from package.ps1, required for Package.
#>
[CmdletBinding()]
param(
    [ValidateSet('Host', 'HostServices', 'Audio', 'Agsl', 'Java', 'Native', 'Bionic', 'Modules', 'Memory', 'Io', 'Faults', 'Format', 'Libraries', 'Jni', 'ApkJni', 'GraphicsHost', 'GraphicsJava', 'Surface', 'GraphicsGuest', 'VulkanWsi', 'DexJob', 'Package', 'Stability')]
    [string]$Suite = 'Host',
    [string]$PackageZip
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

<# Sets process-local test variables, preserving the distinction between unset and empty. #>
function Set-TestEnv([hashtable]$Entries) {
    foreach ($key in $Entries.Keys) {
        if ($null -eq $Entries[$key]) { Remove-Item -LiteralPath "Env:$key" -ErrorAction SilentlyContinue }
        else { [Environment]::SetEnvironmentVariable($key, $Entries[$key]) }
    }
}

<# Runs doctor --json with an environment of its own and returns the report. #>
function Invoke-Doctor([hashtable]$Env, [string[]]$Arguments = @()) {
    $saved = @{}
    foreach ($k in $Env.Keys) { $saved[$k] = [Environment]::GetEnvironmentVariable($k) }
    Set-TestEnv $Env
    try {
        $json = & $winapk doctor --json @Arguments
        if ($LASTEXITCODE -ne 0) { throw "winapk doctor exited with $LASTEXITCODE" }
        ($json -join "`n") | ConvertFrom-Json
    } finally {
        Set-TestEnv $saved
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
    $vkLimIds = @($r.limitations | ForEach-Object id)
    $vkHeadlessCi = [bool]$env:GITHUB_ACTIONS -and $r.graphics.vulkanLoader -eq 'instance-failed: -9' -and ($vkLimIds -contains 'GRAPHICS_CAPABILITY_MISSING')
    Check 'T01.vulkan.loader' (($r.graphics.vulkanLoader -eq 'available' -or $vkHeadlessCi) -and $r.graphics.vulkanLoaderArch -eq 'arm64') "loader=$($r.graphics.vulkanLoader) arch=$($r.graphics.vulkanLoaderArch)"
    $privateAngle = (Test-Path (Join-Path $out 'bin\angle\libEGL.dll')) -and (Test-Path (Join-Path $out 'bin\angle\libGLESv2.dll'))
    $probedNames = @($r.graphics.angleBackends | ForEach-Object backend)
    $angleProbeOk = if (-not $privateAngle) { $probedNames.Count -eq 0 }
                    elseif ($env:GITHUB_ACTIONS) { $true }
                    else { $probedNames -contains 'd3d11' }
    Check 'T01.angle.inventory' ($r.graphics.angleFilesArm64 -eq $privateAngle -and $angleProbeOk) "files=$privateAngle backends=$($probedNames -join ',')"
    Check 'T01.guestVulkan.off' ($r.runtime.guestVulkan -eq $false) 'guest Vulkan is not reported as available'
    Check 'T01.runtime.inventory' ($r.runtime.frameworkCommit -match '^[0-9a-f]{7}$' -and @($r.runtime.guestNativeProfiles) -contains 'N0') "macapk=$($r.runtime.frameworkCommit) profiles=$(@($r.runtime.guestNativeProfiles) -join ',')"

    # Static APK inspection (ARSC label, feature/profile/verdict) and invalid --profile refusal.
    $helloApk = Join-Path $repo 'tests\fixtures\hello-java.apk'
    if (Test-Path $helloApk) {
        $ins = (& $winapk inspect $helloApk --json | Out-String) | ConvertFrom-Json
        Check 'T01.inspect.java' ($ins.ok -and $ins.label -eq 'Hello Java' -and $ins.verdict -eq 'can-run' -and $ins.profile -eq '' -and $ins.size -gt 0) "label=$($ins.label) verdict=$($ins.verdict)"
        $badProf = & $winapk run $helloApk --native=restricted --profile=N9 2>&1
        Check 'T01.neg.profile-invalid' ($LASTEXITCODE -eq 64 -and ($badProf -join "`n") -match 'GUEST_ABI_UNSUPPORTED: --profile') "exit=$LASTEXITCODE"
    }

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

Add-Type @"
using System; using System.Runtime.InteropServices;
public static class WinApkTestWin {
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern IntPtr FindWindowW(string c, string t);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern IntPtr FindWindowExW(IntPtr parent, IntPtr after, string c, string t);
  [DllImport("user32.dll")] public static extern bool PostMessageW(IntPtr h, uint m, IntPtr w, IntPtr l);
  [DllImport("user32.dll")] public static extern IntPtr SendMessageW(IntPtr h, uint m, IntPtr w, ref RECT l);
  [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool SetWindowPos(IntPtr h, IntPtr a, int x, int y, int cx, int cy, uint f);
  [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h, IntPtr dc, uint f);
  [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
  [DllImport("user32.dll")] public static extern bool SetProcessDpiAwarenessContext(IntPtr c);
  [DllImport("user32.dll")] public static extern bool IsIconic(IntPtr h);
  [DllImport("user32.dll")] public static extern uint GetDpiForWindow(IntPtr h);
  // PROCESS_MITIGATION_CONTROL_FLOW_GUARD_POLICY is one DWORD; policy index 7.
  [DllImport("kernel32.dll")] public static extern bool GetProcessMitigationPolicy(IntPtr p, int policy, out uint flags, UIntPtr size);
  [DllImport("user32.dll")] static extern IntPtr MonitorFromPoint(long pointXY, uint flags);  // POINT by value: (0, 0) is 0
  [DllImport("shcore.dll")] static extern int GetDpiForMonitor(IntPtr m, int type, out uint x, out uint y);
  // The primary display's current (effective) DPI, as winapk run reads it.
  public static uint PrimaryDpi() { uint x, y; return GetDpiForMonitor(MonitorFromPoint(0, 1), 0, out x, out y) == 0 ? x : 96; }
  [DllImport("kernel32.dll")] static extern IntPtr OpenThread(uint access, bool inherit, uint id);
  [DllImport("kernel32.dll")] static extern uint SuspendThread(IntPtr h);
  [DllImport("kernel32.dll")] public static extern uint ResumeThread(IntPtr h);
  [DllImport("kernel32.dll")] public static extern bool CloseHandle(IntPtr h);
  [DllImport("kernel32.dll")] static extern bool GetThreadContext(IntPtr h, IntPtr ctx);
  [DllImport("kernel32.dll", CharSet=CharSet.Unicode)] static extern IntPtr GetModuleHandleW(string n);
  [DllImport("kernel32.dll")] static extern IntPtr GetCurrentProcess();
  [DllImport("psapi.dll")] static extern bool GetModuleInformation(IntPtr p, IntPtr m, out MODULEINFO mi, uint cb);
  struct MODULEINFO { public IntPtr Base; public uint Size; public IntPtr Entry; }
  public struct RECT { public int L, T, R, B; }
  // Suspends thread `tid` while it waits in win32u.dll (GetMessage), never in Java code, so the
  // JVM's safepoints still complete. ARM64 CONTEXT: ContextFlags at 0, Pc at 264, 16-aligned.
  // Returns the thread handle (ResumeThread, CloseHandle it), or zero.
  public static IntPtr SuspendWhileWaiting(uint tid) {
    IntPtr h = OpenThread(0x0002 | 0x0008 | 0x0040, false, tid);  // SUSPEND_RESUME | GET_CONTEXT | QUERY_INFORMATION
    if (h == IntPtr.Zero) return h;
    MODULEINFO mi;
    GetModuleInformation(GetCurrentProcess(), GetModuleHandleW("win32u.dll"), out mi, (uint)Marshal.SizeOf(typeof(MODULEINFO)));
    long lo = mi.Base.ToInt64(), hi = lo + mi.Size;
    IntPtr raw = Marshal.AllocHGlobal(0x390 + 16);
    IntPtr ctx = new IntPtr((raw.ToInt64() + 15) & ~15L);
    try {
      for (int i = 0; i < 400; i++) {
        SuspendThread(h);
        Marshal.WriteInt32(ctx, 0, 0x00400001);  // CONTEXT_ARM64 | CONTEXT_CONTROL
        if (GetThreadContext(h, ctx)) { long pc = Marshal.ReadInt64(ctx, 264); if (pc >= lo && pc < hi) return h; }
        ResumeThread(h);
        System.Threading.Thread.Sleep(5);
      }
    } finally { Marshal.FreeHGlobal(raw); }
    CloseHandle(h);
    return IntPtr.Zero;
  }
}
"@

<# Starts winapk run on an APK at $script:appSize (points) with its log in $Log; returns the process. #>
function Start-App([string]$Apk, [string]$Data, [string]$Log, [string]$Native = 'off', [string]$Graphics = 'off', [string]$Profile = '') {
    $args = @('run', "`"$Apk`"", '--data-dir', "`"$Data`"", '--size', $script:appSize, "--native=$Native", "--graphics=$Graphics")
    if ($Profile) { $args += "--profile=$Profile" }
    Start-Process -FilePath $winapk -ArgumentList $args `
        -RedirectStandardError $Log -RedirectStandardOutput "$Log.out" -PassThru -NoNewWindow
}

<# Returns the log's text so far (the runtime keeps writing it). #>
function Read-Log([string]$Log) {
    $fs = [IO.File]::Open($Log, 'Open', 'Read', 'ReadWrite')
    try { (New-Object IO.StreamReader $fs).ReadToEnd() } finally { $fs.Dispose() }
}

<# Waits until the log matches $Pattern; returns the match, or $null after $Seconds. #>
function Wait-Log([string]$Log, [string]$Pattern, [int]$Seconds = 30, $Process = $null) {
    $until = (Get-Date).AddSeconds($Seconds)
    while ((Get-Date) -lt $until) {
        $m = [regex]::Matches((Read-Log $Log), $Pattern)
        if ($m.Count -gt 0) { return $m[$m.Count - 1] }
        if ($Process -and $Process.HasExited) { break }
        Start-Sleep -Milliseconds 200
    }
    $null
}

<# Finds the app window of process $Id titled $Title (safe across concurrent app instances). #>
function Find-AppWindow([int]$Id, [string]$Title, [int]$Seconds = 60, $Process = $null) {
    $until = (Get-Date).AddSeconds($Seconds)
    while ((Get-Date) -lt $until) {
        if ($Process -and $Process.HasExited) { break }
        $after = [IntPtr]::Zero
        for (;;) {
            $h = [WinApkTestWin]::FindWindowExW([IntPtr]::Zero, $after, 'WinApkAppWindow', $Title)
            if ($h -eq [IntPtr]::Zero) { break }
            $p = 0
            [WinApkTestWin]::GetWindowThreadProcessId($h, [ref]$p) | Out-Null
            if ($p -eq $Id) { return $h }
            $after = $h
        }
        Start-Sleep -Milliseconds 250
    }
    [IntPtr]::Zero
}

function Lp([int]$X, [int]$Y) { [IntPtr](($Y -shl 16) -bor ($X -band 0xffff)) }

<# Taps the centre of "l,t,r,b" (window pixels) with a left click. #>
function Send-Tap($H, [string]$Bounds) {
    $b = $Bounds.Split(',') | ForEach-Object { [int]$_ }
    $x = [int](($b[0] + $b[2]) / 2); $y = [int](($b[1] + $b[3]) / 2)
    [WinApkTestWin]::PostMessageW($H, 0x201, [IntPtr]1, (Lp $x $y)) | Out-Null
    Start-Sleep -Milliseconds 60
    [WinApkTestWin]::PostMessageW($H, 0x202, [IntPtr]0, (Lp $x $y)) | Out-Null
}

<# Drags from (x1,y1) to (x2,y2) in ten moves. #>
function Send-Drag($H, [int]$X1, [int]$Y1, [int]$X2, [int]$Y2) {
    [WinApkTestWin]::PostMessageW($H, 0x201, [IntPtr]1, (Lp $X1 $Y1)) | Out-Null
    for ($i = 1; $i -le 10; $i++) {
        Start-Sleep -Milliseconds 20
        [WinApkTestWin]::PostMessageW($H, 0x200, [IntPtr]1, (Lp ($X1 + ($X2 - $X1) * $i / 10) ($Y1 + ($Y2 - $Y1) * $i / 10))) | Out-Null
    }
    [WinApkTestWin]::PostMessageW($H, 0x202, [IntPtr]0, (Lp $X2 $Y2)) | Out-Null
}

<# Types text as WM_CHAR UTF-16 code units, the way an input method's commit arrives. #>
function Send-Chars($H, [string]$Text) {
    foreach ($c in $Text.ToCharArray()) { [WinApkTestWin]::PostMessageW($H, 0x102, [IntPtr][int]$c, [IntPtr]1) | Out-Null }
    Start-Sleep -Milliseconds 40
}

<# Posts a window-menu command: 0xF020 minimise, 0xF120 restore. #>
function Send-SysCommand($H, [int]$Command) { [WinApkTestWin]::PostMessageW($H, 0x112, [IntPtr]$Command, [IntPtr]0) | Out-Null }

<# Presses and releases a virtual key. #>
function Send-Key($H, [int]$Vk) {
    [WinApkTestWin]::PostMessageW($H, 0x100, [IntPtr]$Vk, [IntPtr]1) | Out-Null
    [WinApkTestWin]::PostMessageW($H, 0x101, [IntPtr]$Vk, [IntPtr]0xC0000001) | Out-Null
    Start-Sleep -Milliseconds 40
}

<# Returns the colour of the window's client pixel (x, y) as "RRGGBB", read with PrintWindow. #>
function Get-ClientPixel($H, [int]$X, [int]$Y) {
    Add-Type -AssemblyName System.Drawing
    $r = New-Object WinApkTestWin+RECT
    [WinApkTestWin]::GetClientRect($H, [ref]$r) | Out-Null
    $bmp = New-Object System.Drawing.Bitmap ([Math]::Max(1, $r.R)), ([Math]::Max(1, $r.B))
    $g = [System.Drawing.Graphics]::FromImage($bmp); $dc = $g.GetHdc()
    [WinApkTestWin]::PrintWindow($H, $dc, 3) | Out-Null  # PW_CLIENTONLY | PW_RENDERFULLCONTENT
    $g.ReleaseHdc($dc); $g.Dispose()
    $p = $bmp.GetPixel($X, $Y); $bmp.Dispose()
    '{0:X2}{1:X2}{2:X2}' -f $p.R, $p.G, $p.B
}

<# Closes the window and waits for the process to end; returns its exit code or $null. #>
function Close-App($H, $P, [int]$Seconds = 20) {
    [WinApkTestWin]::PostMessageW($H, 0x10, [IntPtr]0, [IntPtr]0) | Out-Null
    if ($P.WaitForExit($Seconds * 1000)) { return $P.ExitCode }
    $P.Kill(); $null
}

function Test-Java {
    foreach ($need in @($winapk, (Join-Path $out 'bin\winapk-bridge.dll'), (Join-Path $out 'bin\tools\framework-res.apk'), (Join-Path $out 'lib\macapk-loader.jar'))) {
        if (-not (Test-Path $need)) { throw "$need is missing: run scripts/windows/build.ps1 -Component All" }
    }
    $apk = Join-Path $repo 'tests\fixtures\hello-java.apk'
    if (-not (Test-Path $apk)) { throw "$apk is missing: run scripts/windows/build.ps1 -Component Fixtures" }
    $env:WINAPK_JAVA_HOME = Find-Jdk
    [WinApkTestWin]::SetProcessDpiAwarenessContext([IntPtr]-4) | Out-Null  # per-monitor v2, as the app
    # A phone shape of 400x800 points, smaller when that would not fit the screen at this scale.
    $scale = [WinApkTestWin]::PrimaryDpi() / 96.0
    Add-Type -AssemblyName System.Windows.Forms
    $fitH = [int][math]::Floor([System.Windows.Forms.Screen]::PrimaryScreen.WorkingArea.Height / $scale * 0.85)
    $ptH = [math]::Min(800, $fitH - ($fitH % 2))
    $script:appSize = "$($ptH / 2)x$ptH"
    $data = Join-Path $out 'test-data\java'
    Reset-Dir $data
    $logs = Join-Path $out 'test-data\logs'
    New-Item -ItemType Directory -Force $logs | Out-Null

    # ---- run 1: cold cache ----
    $log = Join-Path $logs 'T03-run1.log'
    $p = Start-App $apk $data $log
    $h = Find-AppWindow $p.Id 'Hello Java'
    Check 'T03.window' ($h -ne [IntPtr]::Zero) "window of process $($p.Id)"
    if ($h -eq [IntPtr]::Zero) { if (-not $p.HasExited) { $p.Kill() }; return }
    $layout = Wait-Log $log 'layout\.size=(\d+),(\d+) tap=([\d,]+) input=([\d,]+) list=([\d,]+)'
    $text = Read-Log $log
    Check 'T03.G1.1.convert' ($text -match '\[winapk\] stage dex-converted \(\d+ ms\)') 'cold: DEX converted by com.macapk.dex'
    Check 'T03.G1.1.jvm' ($text -match '\[winapk\] stage jvm-created') 'JVM created in winapk.exe'
    Check 'T03.G1.1.lifecycle' ((Wait-Log $log 'T03: lifecycle=onResume') -and $text -match 'T03: lifecycle=onCreate') 'onCreate, onResume'
    Check 'T03.G1.4.fresh' ($text -match 'T03: restored\.prefs=0' -and $text -match 'T03: restored\.file=none') 'nothing saved before the first run'
    # Non-ASCII test strings are built from code points so this script stays ASCII.
    $e = [char]0xE9; $zh = "$([char]0x4E2D)$([char]0x6587)"; $emoji = "$([char]0xD83D)$([char]0xDE00)"
    $sql = { param($n) "T03: sqlite=rows=$n upper=CAF$([char]0xC9) $zh like=1 order=apple|Banana|caf$e $zh missing=SQLiteException" }
    Check 'T03.sqlite' ($text.Contains((& $sql 1))) "android.database.sqlite: $([regex]::Match($text, 'T03: sqlite=[^\r\n]*').Value)"
    if (-not $layout) { Check 'T03.layout' $false 'no layout line'; Close-App $h $p | Out-Null; return }
    $r = New-Object WinApkTestWin+RECT
    [WinApkTestWin]::GetClientRect($h, [ref]$r) | Out-Null
    Check 'T02.size' ([int]$layout.Groups[1].Value -eq $r.R -and [int]$layout.Groups[2].Value -eq $r.B) "Android view $($layout.Groups[1].Value)x$($layout.Groups[2].Value) px = client $($r.R)x$($r.B) px"
    # G1.3 at this PC's scale: a point is a DIP and a dp, so the client is points x scale, the
    # density is 160 x scale, and a 48 dp button is 48 x scale pixels tall.
    $win = [regex]::Match($text, 'window (\d+)x(\d+) points, scale ([\d.]+), density (\d+) dpi')
    $wdpi = [WinApkTestWin]::GetDpiForWindow($h); $ws = $wdpi / 96.0
    $tb = $layout.Groups[3].Value.Split(',') | ForEach-Object { [int]$_ }
    $btnDp = ($tb[3] - $tb[1]) / $ws
    Check 'T03.G1.3.dpi' ($win.Success -and [math]::Abs([double]$win.Groups[3].Value - $ws) -lt 0.01 -and [int]$win.Groups[4].Value -eq [math]::Round(160 * $ws) -and [math]::Abs($r.R - [int]$win.Groups[1].Value * $ws) -le 1 -and [math]::Abs($r.B - [int]$win.Groups[2].Value * $ws) -le 1) "window DPI $wdpi (scale $ws): $($win.Value); client $($r.R)x$($r.B) px"
    Check 'T03.G1.3.dpi-layout' ([math]::Abs($btnDp - 48) -le 2) "the button is $($tb[3] - $tb[1]) px = $([math]::Round($btnDp, 1)) dp tall (48 dp expected)"
    $bg = ([regex]::Match($text, 'starting-window background #([0-9A-F]{6})')).Groups[1].Value
    Start-Sleep -Milliseconds 500
    $px = Get-ClientPixel $h 2 ($r.B - 2)
    Check 'T02.pixels' ($bg -and $px -eq $bg) "client pixel #$px, theme windowBackground #$bg (ARGB ints shown as BGRA DIB rows)"

    Send-Tap $h $layout.Groups[3].Value
    Send-Tap $h $layout.Groups[3].Value
    Check 'T03.G1.2.tap' ($null -ne (Wait-Log $log 'T03: taps=2' 10)) 'two clicks on the button counted 2'
    Send-Tap $h $layout.Groups[4].Value
    Start-Sleep -Milliseconds 300
    Send-Key $h 0x48; Send-Key $h 0x49  # H, I
    Check 'T03.G1.2.text' ($null -ne (Wait-Log $log 'T03: text=hi' 10)) 'typed "hi" into the EditText'
    # PLAN.md §8.4: characters no mapped key types (an IME commit, a surrogate pair) reach the field.
    Send-Chars $h "$e$zh$emoji"
    Check 'T03.G1.2.text-unicode' ($null -ne (Wait-Log $log ([regex]::Escape("T03: text=hi$e$zh$emoji") + '\r?\n') 10)) 'WM_CHAR e-acute, two CJK characters and a surrogate pair landed in the EditText'
    $lb = $layout.Groups[5].Value.Split(',') | ForEach-Object { [int]$_ }
    $cx = [int](($lb[0] + $lb[2]) / 2)
    Send-Drag $h $cx ($lb[3] - 20) $cx ($lb[1] + 20)
    $first = Wait-Log $log 'T03: list\.first=([1-9]\d*)' 10
    Check 'T03.G1.2.scroll' ($null -ne $first) "a drag scrolled the list (first visible row $(if ($first) { $first.Groups[1].Value }))"

    # G1.3: a wider window relays out and taps still land on the button.
    $wr = New-Object WinApkTestWin+RECT
    [WinApkTestWin]::GetWindowRect($h, [ref]$wr) | Out-Null
    $newW = ($wr.R - $wr.L) + [int]($r.R / 2)
    [WinApkTestWin]::SetWindowPos($h, [IntPtr]::Zero, 0, 0, $newW, ($wr.B - $wr.T), 0x16) | Out-Null  # NOMOVE|NOZORDER|NOACTIVATE
    [WinApkTestWin]::GetClientRect($h, [ref]$r) | Out-Null
    $relaid = Wait-Log $log "layout\.size=$($r.R),$($r.B) tap=([\d,]+)" 10
    Check 'T03.G1.3.resize' ($null -ne $relaid) "relaid out at client $($r.R)x$($r.B) px"
    if ($relaid) {
        Send-Tap $h $relaid.Groups[1].Value
        Check 'T03.G1.3.tap-after-resize' ($null -ne (Wait-Log $log 'T03: taps=3' 10)) "the button at $($relaid.Groups[1].Value) took the click"
        # Losing focus mid-press cancels the touch: the button must not click.
        $b = $relaid.Groups[1].Value.Split(',') | ForEach-Object { [int]$_ }
        $bx = [int](($b[0] + $b[2]) / 2); $by = [int](($b[1] + $b[3]) / 2)
        [WinApkTestWin]::PostMessageW($h, 0x201, [IntPtr]1, (Lp $bx $by)) | Out-Null
        Start-Sleep -Milliseconds 100
        [WinApkTestWin]::PostMessageW($h, 0x006, [IntPtr]0, [IntPtr]::Zero) | Out-Null  # WM_ACTIVATE, WA_INACTIVE
        Start-Sleep -Milliseconds 100
        [WinApkTestWin]::PostMessageW($h, 0x202, [IntPtr]0, (Lp $bx $by)) | Out-Null
        $lost = Wait-Log $log '\[winapk\] window: focus lost' 5
        Start-Sleep -Milliseconds 1500
        Check 'T03.G1.3.focus-cancel' ($lost -and (Read-Log $log) -notmatch 'T03: taps=4') 'deactivated mid-press: ACTION_CANCEL, no click'
    }

    Send-Key $h 0x1B  # Escape is Back
    Check 'T03.G1.2.back' ($null -ne (Wait-Log $log 'T03: lifecycle=onDestroy' 10) -and (Read-Log $log) -match 'T03: back=pressed') 'Back finished the activity'
    # PLAN.md §8.3: the emptied task goes to the taskbar; restoring the window starts it again.
    $iconic = $false
    for ($i = 0; $i -lt 40 -and -not $iconic; $i++) { Start-Sleep -Milliseconds 250; $iconic = [WinApkTestWin]::IsIconic($h) }
    Check 'T03.G1.3.back-hides' $iconic 'the emptied task minimised the window (moveTaskToBack)'
    Send-SysCommand $h 0xF120
    $until = (Get-Date).AddSeconds(20)
    while ((Get-Date) -lt $until -and [regex]::Matches((Read-Log $log), 'T03: lifecycle=onResume').Count -lt 2) { Start-Sleep -Milliseconds 200 }
    $text = Read-Log $log
    Check 'T03.G1.3.restore' ([regex]::Matches($text, 'T03: lifecycle=onResume').Count -ge 2 -and $text -match 'task brought back empty -> launching') 'restoring the window relaunched the activity (ActivityHost.macapkTaskToFront)'
    Check 'T03.sqlite.persist' ($text.Contains((& $sql 2))) 'the relaunch found the first launch''s rows'
    Send-SysCommand $h 0xF020
    $min = Wait-Log $log '\[winapk\] window: minimised; drawing at 2 Hz' 10
    Start-Sleep -Milliseconds 1500
    Send-SysCommand $h 0xF120
    $until = (Get-Date).AddSeconds(10)
    while ((Get-Date) -lt $until -and [regex]::Matches((Read-Log $log), 'window: restored; drawing at full rate').Count -lt 2) { Start-Sleep -Milliseconds 200 }
    Check 'T03.G1.3.minimize' ($min -and [regex]::Matches((Read-Log $log), 'window: restored; drawing at full rate').Count -ge 2 -and -not $p.HasExited) 'minimised and restored by the person: throttled, then full rate, app still running'
    Check 'T03.neg.ui-blocked' ((Read-Log $log) -notmatch 'UI_THREAD_BLOCKED|UI_CALL_TIMEOUT') 'no window call waited on the UI thread in a normal run'
    $code = Close-App $h $p
    Check 'T03.G1.3.exit' ($code -eq 0) "closing the window ended the process (exit $code)"
    Check 'T03.G1.3.no-leftover' ($null -eq (Get-Process -Id $p.Id -ErrorAction SilentlyContinue)) "process $($p.Id) is gone"

    # ---- run 2: hot cache, restored state ----
    $log2 = Join-Path $logs 'T03-run2.log'
    $inject = Join-Path $logs 'T03-inject.txt'
    Remove-Item $inject -ErrorAction SilentlyContinue
    $env:MACAPK_INJECT_FILE = $inject  # upstream's input driver (TouchDispatcher.macapkStartInjectDriver)
    $p2 = Start-App $apk $data $log2
    $env:MACAPK_INJECT_FILE = $null
    $h2 = Find-AppWindow $p2.Id 'Hello Java'
    $ok = $null -ne (Wait-Log $log2 'T03: lifecycle=onResume')
    $text2 = Read-Log $log2
    Check 'T03.G1.4.cache' ($text2 -match '\[winapk\] stage dex-converted \(cached\)') 'hot: classes.jar reused from the cache'
    Check 'T03.G1.4.prefs' ($ok -and $text2 -match 'T03: restored\.prefs=3') 'SharedPreferences restored 3 taps'
    Check 'T03.G1.4.file' ($text2 -match 'T03: restored\.file=3') 'files/taps.txt restored 3'
    Check 'T03.G1.4.sqlite' ($text2.Contains((& $sql 3))) 'databases/t03.db kept both launches of run 1'
    # PLAN.md §8.3: a window call from the app's loop while the UI thread cannot run it is
    # reported, then abandoned, instead of hanging. Back (sent by the input driver, not the
    # window) ends the task, whose moveTaskToBack must reach the suspended UI thread.
    $tid = 0
    if ($h2 -ne [IntPtr]::Zero -and (Wait-Log $log2 'inject-driver watching' 10)) { $tid = [WinApkTestWin]::GetWindowThreadProcessId($h2, [ref]0) }
    $th = if ($tid) { [WinApkTestWin]::SuspendWhileWaiting($tid) } else { [IntPtr]::Zero }
    if ($th -ne [IntPtr]::Zero) {
        try {
            Set-Content $inject 'key 4' -Encoding ascii
            $blocked = Wait-Log $log2 'UI_THREAD_BLOCKED: (\w+) has waited' 20
            $abandoned = Wait-Log $log2 'UI_CALL_TIMEOUT: (\w+) was not run' 30
        } finally { [WinApkTestWin]::ResumeThread($th) | Out-Null; [WinApkTestWin]::CloseHandle($th) | Out-Null }
        Check 'T03.G1.3.ui-blocked' ($null -ne $blocked) "reported: $(if ($blocked) { $blocked.Value })"
        Check 'T03.G1.3.ui-timeout' ($null -ne $abandoned) "abandoned: $(if ($abandoned) { $abandoned.Value })"
        Start-Sleep -Milliseconds 500
        Check 'T03.G1.3.ui-recovered' (-not $p2.HasExited) 'the UI thread ran again and the app kept running'
    } else {
        Check 'T03.G1.3.ui-blocked' $false "could not suspend the UI thread while it waited (thread $tid)"
    }
    if ($h2 -ne [IntPtr]::Zero) { $code2 = Close-App $h2 $p2 } else { $p2.Kill(); $code2 = $null }
    Check 'T03.G1.4.exit' ($code2 -eq 0) "second run ended (exit $code2)"

    # PLAN.md §8.1: an APK and a data folder whose paths have spaces and characters outside
    # the PC's legacy code page (Hungarian, Korean).
    $uni = Join-Path $out "test-data\uni $([char]0x0151)$([char]0xD55C) $zh"
    Reset-Dir $uni
    $uniApk = Join-Path $uni "hello $([char]0x0151)$([char]0xD55C).apk"
    Copy-Item $apk $uniApk
    $log4 = Join-Path $logs 'T03-unicode.log'
    $p4 = Start-App $uniApk (Join-Path $uni "data $e") $log4
    $h4 = Find-AppWindow $p4.Id 'Hello Java'
    $ok4 = $null -ne (Wait-Log $log4 'T03: lifecycle=onResume')
    Check 'T03.G1.1.unicode-path' ($ok4 -and (Read-Log $log4) -match 'stage dex-converted \(\d+ ms\)' -and (Read-Log $log4).Contains((& $sql 1))) 'converted, booted and stored data under a Unicode path with spaces'
    if ($h4 -ne [IntPtr]::Zero) { $code4 = Close-App $h4 $p4 } else { if (-not $p4.HasExited) { $p4.Kill() }; $code4 = $null }
    Check 'T03.G1.1.unicode-exit' ($code4 -eq 0) "exit $code4"

    # ---- negative cases ----
    & $winapk run (Join-Path $repo 'README.md') --data-dir $data 2> (Join-Path $logs 'neg-not-apk.log')
    $neg = Get-Content (Join-Path $logs 'neg-not-apk.log') -Raw
    Check 'T03.neg.not-apk' ($LASTEXITCODE -eq 65 -and $neg -match 'INSTALL_FAILED_INVALID_APK') "exit $LASTEXITCODE"
    & $winapk run $apk --native=unknown --data-dir $data 2> (Join-Path $logs 'neg-native.log')
    $neg = Get-Content (Join-Path $logs 'neg-native.log') -Raw
    Check 'T03.neg.native-mode' ($LASTEXITCODE -eq 64 -and $neg -match 'GUEST_ABI_UNSUPPORTED') "exit $LASTEXITCODE"

    # G1.5: guest native off, an APK library load fails clearly (never a stub, never LoadLibrary).
    $jniApk = Join-Path $repo 'tests\fixtures\hello-jni.apk'
    $log3 = Join-Path $logs 'G1.5-hello-jni.log'
    $p3 = Start-App $jniApk $data $log3
    $h3 = Find-AppWindow $p3.Id 'Hello JNI'
    $res = Wait-Log $log3 'JNI: native\.(value|error)=(.*)' 30
    $text3 = Read-Log $log3
    Check 'G1.5.inspect' ($text3 -match 'guest native is off: the APK''s 3 native libraries \(arm64-v8a\)') 'winapk run reports the APK''s native libraries'
    Check 'G1.5.load-refused' ($res -and $res.Groups[1].Value -eq 'error' -and $res.Groups[2].Value -match '^GUEST_NATIVE_DISABLED: libhello\.so') "System.loadLibrary: $(if ($res) { $res.Value })"
    if ($h3 -ne [IntPtr]::Zero) { $code3 = Close-App $h3 $p3 } else { if (-not $p3.HasExited) { $p3.Kill() }; $code3 = $null }
    Check 'G1.5.exit' ($code3 -eq 0) "the app kept running and closed normally (exit $code3)"

    # G1.6: the Kotlin fixture T04 through the same DEX path, its standard library included.
    $ktApk = Join-Path $repo 'tests\fixtures\hello-kotlin.apk'
    $log5 = Join-Path $logs 'T04-hello-kotlin.log'
    $p5 = Start-App $ktApk $data $log5
    $h5 = Find-AppWindow $p5.Id 'Hello Kotlin'
    $kt = Wait-Log $log5 'T04: result=([^\r\n]*)' 60
    Check 'T04.G1.6.result' ($kt -and $kt.Groups[1].Value -eq 'top=the:3,brown:1,dog:1 squares=140 copy=Count(word=fox, n=2)') "kotlin-stdlib: $(if ($kt) { $kt.Groups[1].Value })"
    Check 'T04.G1.6.lifecycle' ($null -ne (Wait-Log $log5 'T04: lifecycle=onResume' 10)) 'onCreate, onResume'
    if ($h5 -ne [IntPtr]::Zero) { $code5 = Close-App $h5 $p5 } else { if (-not $p5.HasExited) { $p5.Kill() }; $code5 = $null }
    Check 'T04.G1.6.exit' ($code5 -eq 0) "exit $code5"
}

<#
Suite Native: G2 (PLAN.md §9). T05 runs every case of native-probe suite on each build of the
N0 library and the stress cycles; T06 loads each malformed or out-of-profile ELF and checks the
refusal's category and reason, before any guest code runs.
#>
function Test-Native {
    $probe = Join-Path $out 'bin\native-probe.exe'
    $dir = Join-Path $repo 'tests\fixtures\elf'
    $record = Join-Path $dir 'fixtures.json'
    if (-not (Test-Path $probe) -or -not (Test-Path $record)) { throw "native-probe.exe or the ELF fixtures are missing: run scripts/windows/build.ps1 -Component NativeProbe" }
    $built = (Get-Content $record -Raw | ConvertFrom-Json).fixtures
    $table = Import-PowerShellDataFile (Join-Path $repo 'tests\windows-arm64\guest-elf\fixtures.psd1')

    foreach ($pe in 'native-probe.exe', 'capstone.dll') {
        $m = Get-PeMachine (Join-Path $out "bin\$pe")
        Check "T05.pe.$pe" ($m -eq 'ARM64') $m
    }
    foreach ($f in $table.Fixtures) {
        if ($f.Id -notin 'T05', 'T06') { continue }
        $file = Join-Path $dir $f.Name
        $stem = [IO.Path]::GetFileNameWithoutExtension($f.Name)
        $rec = $built | Where-Object name -EQ $f.Name
        $sha = if (Test-Path $file) { (Get-FileHash $file -Algorithm SHA256).Hash.ToLower() } else { 'missing' }
        if (-not $rec -or $rec.sha256 -ne $sha) { Check "$($f.Id).$stem.hash" $false "sha256 $sha is not the one fixtures.json records"; continue }
        if ($f.Id -eq 'T05') {
            $o = & $probe suite $file
            $code = $LASTEXITCODE
            $load = ($o | Select-String '^load ') -join ''
            $fails = @($o | Select-String '^case \S+ FAIL' | ForEach-Object { $_.Line })
            if ($f.Name -eq 'libn0abi.so') {
                foreach ($c in $o | Select-String '^case (\S+) (PASS|FAIL)(.*)') {
                    Check "T05.$($c.Matches[0].Groups[1].Value)" ($c.Matches[0].Groups[2].Value -eq 'PASS') "$($c.Matches[0].Groups[2].Value)$($c.Matches[0].Groups[3].Value)"
                }
                Check 'T05.screen' (($o -join "`n") -match 'screened 0x[0-9a-f]+ code bytes: \d+ instructions') (($o | Select-String 'screened').Line.Trim())
            }
            $result = ($o | Select-String '^result ').Line
            Check "T05.$stem" ($code -eq 0 -and $fails.Count -eq 0 -and $result -match '^result (\d+)/\1 PASS$') "$result; $($load -replace '^load [^:]+: ', '')$(if ($fails) { '; ' + ($fails -join '; ') })"
        } else {
            $o = & $probe load $file
            $code = $LASTEXITCODE
            $line = ($o | Select-String '^reject ').Line
            $ok = $code -eq 2 -and -not ($o | Select-String '^load ') -and $line -like "reject $($f.Expect) $($f.Name)*" -and $line -like "*$($f.Reason)*"
            Check "T06.$stem" $ok "exit $code; $line"
        }
    }

    # G2: repeated load/run/unload without growth, a refused load rolled back each time, and
    # 32 copies at once at 32 distinct bases, all passing.
    $o = & $probe stress (Join-Path $dir 'libn0abi.so') --iterations 2000 --copies 32 --rejected (Join-Path $dir 'neg-missing.so')
    $code = $LASTEXITCODE
    $cycles = ($o | Select-String 'load/run/unload cycles').Line
    $copies = ($o | Select-String 'copies loaded at once').Line
    Check 'T05.stress.cycles' ($code -eq 0 -and $cycles -match 'all PASS; per \d+ cycles heap allocations \+0 then \+0 bytes' -and $cycles -match 'private regions \+0, handles \+0') ($cycles -replace '^stress [^:]+: ', '')
    Check 'T05.stress.copies' ($code -eq 0 -and $copies -match '32 copies loaded at once at 32 distinct bases, all PASS') ($copies -replace '^stress [^:]+: ', '')

    # A second process places the module elsewhere and gets the same results.
    $bases = foreach ($i in 1, 2) {
        $o = & $probe suite (Join-Path $dir 'libn0abi.so')
        if ($LASTEXITCODE -eq 0) { (($o | Select-String '^load ').Line -replace '.* at (0x[0-9a-f]+)$', '$1') } else { 'FAIL' }
    }
    Check 'T05.processes' (-not ($bases -contains 'FAIL')) "two processes, all cases PASS at $($bases -join ' and ')"

    # The probe's own refusal of input it cannot read.
    & $probe load (Join-Path $dir 'absent.so') 2> $null
    Check 'T05.neg.unreadable' ($LASTEXITCODE -eq 1) "exit $LASTEXITCODE"
}

<# Runs T08 and its refusal matrix with a deadline so a broken join/barrier cannot hang tests. #>
function Test-Bionic([string]$Command = '') {
    $probe = Join-Path $out 'bin\native-probe.exe'
    $dir = Join-Path $repo 'tests\fixtures\elf'
    $record = Join-Path $dir 'fixtures.json'
    if (-not (Test-Path $probe) -or -not (Test-Path $record)) { throw 'Build NativeProbe first' }
    $built = (Get-Content $record -Raw | ConvertFrom-Json).fixtures
    $table = Import-PowerShellDataFile (Join-Path $repo 'tests\windows-arm64\guest-elf\fixtures.psd1')
    $logs = Join-Path $out 'test-data\logs'
    New-Item -ItemType Directory -Force $logs | Out-Null
    foreach ($f in $table.Fixtures | Where-Object {
        if ($Command) { $_.ContainsKey('Command') -and $_.Command -eq $Command }
        else { $_.Id -eq 'T08' -or $_.Name -in 'neg-tls.so', 'neg-svc.so', 'neg-tpidr.so', 'neg-x18.so' }
    }) {
        $file = Join-Path $dir $f.Name
        $rec = $built | Where-Object name -EQ $f.Name
        $sha = if (Test-Path $file) { (Get-FileHash $file -Algorithm SHA256).Hash.ToLower() } else { 'missing' }
        Check "T08.$($f.Name).hash" ($null -ne $rec -and $rec.sha256 -eq $sha) "sha256 $sha"
        if (-not $rec -or $rec.sha256 -ne $sha) { continue }
        $stdout = Join-Path $logs "T08-$($f.Name).out"
        $stderr = Join-Path $logs "T08-$($f.Name).err"
        $probeCommand = if ($f.ContainsKey('Command')) { $f.Command } else { 'threads' }
        $arguments = @($probeCommand, "`"$file`"")
        if ($probeCommand -eq 'io') {
            $ioRoot = Join-Path $out 'test-data\客体 文件 root'
            $ioOutside = Join-Path $out 'test-data\io-outside'
            foreach ($directory in @("$ioRoot\data\中文 dir", "$ioRoot\sdcard", $ioOutside)) {
                New-Item -ItemType Directory -Force $directory | Out-Null
            }
            [IO.File]::WriteAllText("$ioOutside\sentinel.txt", 'untouched')
            $junction = "$ioRoot\data\escape"
            if (-not (Test-Path $junction)) { New-Item -ItemType Junction -Path $junction -Target $ioOutside | Out-Null }
            $arguments += "`"$ioRoot`""
        }
        if ($probeCommand -eq 'format') {
            $helper = Join-Path $out 'bin\guest\libwinapk-bionic.so'
            $helperRecord = Get-Content (Join-Path $out 'bin\guest\bionic.json') -Raw | ConvertFrom-Json
            $helperHash = (Get-FileHash $helper -Algorithm SHA256).Hash.ToLower()
            Check 'T08.format.helper.hash' ($helperHash -eq $helperRecord.sha256) $helperHash
            if ($helperHash -ne $helperRecord.sha256) { continue }
            $arguments += "`"$helper`""
        }
        $p = Start-Process $probe -ArgumentList $arguments -PassThru -NoNewWindow -RedirectStandardOutput $stdout -RedirectStandardError $stderr
        try {
            if (-not $p.WaitForExit(60000)) {
                $p.Kill()
                $p.WaitForExit()
                Check "T08.$($f.Name).deadline" $false 'probe exceeded 60 seconds'
                continue
            }
            $o = Get-Content $stdout -Raw
            $err = Get-Content $stderr -Raw
            if ($f.ContainsKey('Expect')) {
                Check "T08.$($f.Name).refusal" ($p.ExitCode -eq 2 -and $o -match "reject $($f.Expect) " -and $o.Contains($f.Reason)) $o.Trim()
            } elseif ($f.ContainsKey('Exit')) {
                $ok = $p.ExitCode -eq $f.Exit -and $err -and $err.Contains($f.Reason)
                if ($f.ContainsKey('Fault')) { $ok = $ok -and $err -match "vaddr 0x[0-9a-f]+: Windows exception 0x$($f.Fault);" }
                Check "T08.$($f.Name).fatal" $ok "exit $($p.ExitCode); $err"
            } else {
                $cases = [regex]::Matches($o, '(?m)^case (T08\.\S+) (PASS|FAIL): (.*)')
                foreach ($c in $cases) { Check $c.Groups[1].Value ($c.Groups[2].Value -eq 'PASS') $c.Groups[3].Value.TrimEnd() }
                $expectedCases = if ($f.ContainsKey('Cases')) { $f.Cases } else { 9 }
                $ok = $p.ExitCode -eq 0 -and $cases.Count -eq $expectedCases -and $o -match 'result T08 PASS'
                Check "T08.$($f.Name).result" $ok "exit $($p.ExitCode), $($cases.Count) cases$(if (-not $ok) { '; ' + $o + $err })"
                if ($probeCommand -eq 'io') {
                    $mapped = "$ioRoot\data\中文 dir\空 格.bin"
                    $bytes = if (Test-Path $mapped) { [Convert]::ToHexString([IO.File]::ReadAllBytes($mapped)) } else { 'missing' }
                    Check 'T08.io.mapping' ($bytes -eq '410042FF5A' -and [IO.File]::ReadAllText("$ioOutside\sentinel.txt") -eq 'untouched') "mapped bytes $bytes; outside sentinel unchanged"
                }
                if ($probeCommand -eq 'format') {
                    $lines = @($err -split '\r?\n' | Where-Object { $_ })
                    $expected = @('guest-log 4 T08: literal %ld UTF8 中文', 'guest-log 5 T08: n=4294967297 text=中文 %', 'guest-log 3 T08: v=7:ok')
                    Check 'T08.format.log-bytes' (($lines -join "`n") -ceq ($expected -join "`n")) $err
                }
            }
        } finally { $p.Dispose() }
    }
    if (-not $Command) { Test-Modules; Test-Libraries }
}

<# Runs T07 once as a compact transaction matrix, with manifest checks and a hard deadline. #>
function Test-Modules {
    $probe = Join-Path $out 'bin\native-probe.exe'
    $dir = Join-Path $repo 'tests\fixtures\elf'
    $built = (Get-Content (Join-Path $dir 'fixtures.json') -Raw | ConvertFrom-Json).fixtures
    $table = Import-PowerShellDataFile (Join-Path $repo 'tests\windows-arm64\guest-elf\fixtures.psd1')
    foreach ($f in $table.Fixtures | Where-Object { $_.Id -eq 'T07' -and -not $_.ContainsKey('Command') }) {
        $record = $built | Where-Object name -EQ $f.Name
        $file = Join-Path $dir $f.Name
        $hash = if (Test-Path $file) { (Get-FileHash $file -Algorithm SHA256).Hash.ToLower() } else { 'missing' }
        Check "T07.$($f.Name).hash" ($null -ne $record -and $record.sha256 -eq $hash) $hash
        if (-not $record -or $record.sha256 -ne $hash) { return }
    }
    $logs = Join-Path $out 'test-data\logs'
    New-Item -ItemType Directory -Force $logs | Out-Null
    $stdout = Join-Path $logs 'T07-modules.out'
    $stderr = Join-Path $logs 'T07-modules.err'
    $p = Start-Process $probe -ArgumentList @('modules', "`"$dir`"") -NoNewWindow -PassThru -RedirectStandardOutput $stdout -RedirectStandardError $stderr
    try {
        if (-not $p.WaitForExit(30000)) {
            $p.Kill()
            $p.WaitForExit()
            Check 'T07.deadline' $false 'module matrix exceeded 30 seconds'
            return
        }
        $text = [string](Get-Content $stdout -Raw)
        $cases = [regex]::Matches($text, '(?m)^case (T07\.\S+) (PASS|FAIL)')
        foreach ($case in $cases) { Check $case.Groups[1].Value ($case.Groups[2].Value -eq 'PASS') $case.Groups[2].Value }
        $ok = $p.ExitCode -eq 0 -and $cases.Count -eq 10 -and $text -match 'result T07 10/10 PASS'
        Check 'T07.result' $ok "exit $($p.ExitCode), $($cases.Count) semantic cases$(if (-not $ok) { '; ' + $text + (Get-Content $stderr -Raw) })"
    } finally { $p.Dispose() }
}

<# Runs one normally linked NDK fixture and compact refusal variants, never loading system ELF files. #>
function Test-Libraries {
    $dir = Join-Path $repo 'tests\fixtures\elf'
    $built = (Get-Content (Join-Path $dir 'fixtures.json') -Raw | ConvertFrom-Json).fixtures
    $table = Import-PowerShellDataFile (Join-Path $repo 'tests\windows-arm64\guest-elf\fixtures.psd1')
    $cases = Join-Path $out 'test-data\n1-libraries'
    $names = @{ 'libbionic.so' = 'good'; 'neg-bionic-import.so' = 'missing'; 'neg-bionic-version.so' = 'version'; 'neg-bionic-chain.so' = 'chain' }
    foreach ($f in $table.Fixtures | Where-Object { $_.ContainsKey('Command') -and $_.Command -eq 'libraries' }) {
        $file = Join-Path $dir $f.Name
        $record = $built | Where-Object name -EQ $f.Name
        $hash = (Get-FileHash $file -Algorithm SHA256).Hash.ToLower()
        Check "T07.n1.$($f.Name).hash" ($null -ne $record -and $record.sha256 -eq $hash) $hash
        if (-not $record -or $record.sha256 -ne $hash) { return }
        $stage = Join-Path $cases $names[$f.Name]
        New-Item -ItemType Directory -Force $stage | Out-Null
        Copy-Item $file (Join-Path $stage 'libbionic.so') -Force
    }
    $helper = Join-Path $out 'bin\guest\libwinapk-bionic.so'
    $helperRecord = Get-Content (Join-Path $out 'bin\guest\bionic.json') -Raw | ConvertFrom-Json
    $helperHash = (Get-FileHash $helper -Algorithm SHA256).Hash.ToLower()
    Check 'T07.n1.helper.hash' ($helperHash -eq $helperRecord.sha256) $helperHash
    if ($helperHash -ne $helperRecord.sha256) { return }
    $logs = Join-Path $out 'test-data\logs'
    New-Item -ItemType Directory -Force $logs | Out-Null
    $stdout = Join-Path $logs 'T07-n1.out'
    $stderr = Join-Path $logs 'T07-n1.err'
    $p = Start-Process (Join-Path $out 'bin\native-probe.exe') -ArgumentList @('libraries', "`"$cases`"", "`"$helper`"") -NoNewWindow -PassThru -RedirectStandardOutput $stdout -RedirectStandardError $stderr
    try {
        if (-not $p.WaitForExit(30000)) {
            $p.Kill(); $p.WaitForExit()
            Check 'T07.n1.deadline' $false 'library matrix exceeded 30 seconds'
            return
        }
        $text = [string](Get-Content $stdout -Raw)
        $err = [string](Get-Content $stderr -Raw)
        $checks = [regex]::Matches($text, '(?m)^case (T07\.n1-\S+) (PASS|FAIL)')
        foreach ($c in $checks) { Check $c.Groups[1].Value ($c.Groups[2].Value -eq 'PASS') $c.Groups[2].Value }
        $ok = $p.ExitCode -eq 0 -and $checks.Count -eq 6 -and $text -match 'result T07.N1 6/6 PASS' -and
              $err.Trim() -ceq 'guest-log 4 T07: versioned Bionic'
        Check 'T07.n1.result' $ok "exit $($p.ExitCode), $($checks.Count) cases$(if (-not $ok) { '; ' + $text + $err })"
    } finally { $p.Dispose() }
}

<# Runs T09's compact real-JVM JNI foundation and fatal boundary variants; never reports G3. #>
function Test-Jni {
    $fixture = Join-Path $repo 'tests\fixtures\elf\libjni-probe.so'
    $records = (Get-Content (Join-Path $repo 'tests\fixtures\elf\fixtures.json') -Raw | ConvertFrom-Json).fixtures
    $table = Import-PowerShellDataFile (Join-Path $repo 'tests\windows-arm64\guest-elf\fixtures.psd1')
    foreach ($recipe in $table.Fixtures | Where-Object Id -EQ 'T09') {
        $record = $records | Where-Object name -EQ $recipe.Name
        $hash = (Get-FileHash (Join-Path $repo "tests\fixtures\elf\$($recipe.Name)") -Algorithm SHA256).Hash.ToLower()
        Check "T09.$($recipe.Name).hash" ($null -ne $record -and $record.sha256 -eq $hash) $hash
        if (-not $record -or $record.sha256 -ne $hash) { return }
    }
    $helper = Join-Path $out 'bin\guest\libwinapk-jni.so'
    foreach ($name in 'jni', 'bionic') {
        $helperRecord = Get-Content (Join-Path $out "bin\guest\$name.json") -Raw | ConvertFrom-Json
        $helperHash = (Get-FileHash (Join-Path $out "bin\guest\libwinapk-$name.so") -Algorithm SHA256).Hash.ToLower()
        Check "T09.$name.helper.hash" ($helperHash -eq $helperRecord.sha256) $helperHash
        if ($helperHash -ne $helperRecord.sha256) { return }
    }
    $logs = Join-Path $out 'test-data\logs'
    New-Item -ItemType Directory -Force $logs | Out-Null
    $jdk = Find-Jdk
    foreach ($variant in @(
        @{ Name = 'positive'; Extra = @(); Reason = ''; Exit = 0 },
        @{ Name = 'unsupported'; Extra = @('4'); Reason = 'GUEST_ABI_UNSUPPORTED JNI: unsupported entry \(slot 217\)'; Exit = 3 },
        @{ Name = 'thread'; Extra = @('5'); Reason = 'GUEST_ABI_UNSUPPORTED JNI: foreign, detached or cross-thread JNIEnv'; Exit = 3 },
        @{ Name = 'trap'; Extra = @('6'); Reason = 'GUEST_EXECUTION_FAULT libjni-probe.so vaddr 0x.*Windows exception 0xc000001d'; Exit = 3 },
        @{ Name = 'access'; Extra = @('7'); Reason = 'GUEST_EXECUTION_FAULT libjni-probe.so vaddr 0x.*Windows exception 0xc0000005'; Exit = 3 },
        @{ Name = 'onload-escape'; Extra = @('8'); Reason = 'GUEST_ABI_UNSUPPORTED JNI: native entry from failed or competing JNI_OnLoad'; Exit = 3 }
    )) {
        $stdout = Join-Path $logs "T09-$($variant.Name).out"
        $stderr = Join-Path $logs "T09-$($variant.Name).err"
        $arguments = @("`"$jdk`"", "`"$fixture`"", "`"$helper`"") + $variant.Extra
        $p = Start-Process (Join-Path $out 'bin\jni-probe.exe') -ArgumentList $arguments -NoNewWindow -PassThru -RedirectStandardOutput $stdout -RedirectStandardError $stderr
        try {
            if ($variant.Name -eq 'positive') {
                $cfg = [uint32]0
                $cfgQueried = [WinApkTestWin]::GetProcessMitigationPolicy($p.Handle, 7, [ref]$cfg, [UIntPtr]4)
                Write-Host "diagnostic T09.positive.cfg=$($cfg -band 1) query=$cfgQueried"
            }
            if (-not $p.WaitForExit(15000)) {
                $p.Kill(); $p.WaitForExit()
                Check "T09.$($variant.Name).deadline" $false 'exceeded 15 seconds'
                continue
            }
            $text = [string](Get-Content $stdout -Raw)
            $err = [string](Get-Content $stderr -Raw)
            if ($variant.Name -eq 'positive') {
                $cases = [regex]::Matches($text, '(?m)^case (T09\.\S+) (PASS|FAIL)')
                foreach ($c in $cases) { Check $c.Groups[1].Value ($c.Groups[2].Value -eq 'PASS') $c.Groups[2].Value }
                $compilation = [IO.File]::ReadAllText((Join-Path $logs 'T09-compilation.xml'))
                $ok = $cfgQueried -and $p.ExitCode -eq 0 -and $cases.Count -eq 9 -and $text -match 'result T09.foundation PASS \(not G3\)' -and
                      [string]::IsNullOrWhiteSpace($err) -and $text -notmatch '(?i)WARNING|FATAL ERROR' -and
                      $compilation -match "<nmethod\b[^>]*method='com.winapk.probe.GuestJniProbe implicit \(I\)I'"
            } else {
                $ok = $p.ExitCode -eq $variant.Exit -and $err -match $variant.Reason -and $text -notmatch 'result T09.foundation PASS'
            }
            Check "T09.$($variant.Name).result" $ok "exit $($p.ExitCode)$(if (-not $ok) { '; ' + $text + $err })"
        } finally { $p.Dispose() }
    }
}

<# Runs the source-owned T09/G3 APK matrix, normal close, disabled mode and explicit JNI refusal. #>
function Test-ApkJni {
    $script:appSize = '360x720'
    $logs = Join-Path $out 'test-data\logs'
    New-Item -ItemType Directory -Force $logs | Out-Null
    $data = Join-Path $out 'test-data\apk-jni'
    $apk = Join-Path $repo 'tests\fixtures\hello-jni.apk'
    $record = Get-Content (Join-Path $repo 'tests\fixtures\hello-jni.json') -Raw | ConvertFrom-Json
    Check 'T09.apk.hash' ($record.apkSha256 -eq (Get-FileHash $apk -Algorithm SHA256).Hash.ToLower()) 'APK source/build record'
    $insJni = (& $winapk inspect $apk --json | Out-String) | ConvertFrom-Json
    Check 'T09.apk.inspect' ($insJni.ok -and $insJni.label -eq 'Hello JNI' -and $insJni.profile -eq 'N1' -and $insJni.verdict -eq 'needs-native') "label=$($insJni.label) profile=$($insJni.profile) verdict=$($insJni.verdict)"
    foreach ($variant in @(
        @{ Name = 'off'; Mode = 'off'; Profile = '' },
        @{ Name = 'n0-refused'; Mode = 'restricted'; Profile = 'N0' },
        @{ Name = 'restricted'; Mode = 'restricted'; Profile = 'N1' },
        @{ Name = 'unsupported'; Mode = 'restricted'; Profile = 'N1' }
    )) {
        $mode = $variant.Mode
        $log = Join-Path $logs "T09-apk-$($variant.Name).log"
        $oldOptions = $env:JAVA_TOOL_OPTIONS
        try {
            if ($variant.Name -eq 'restricted') { $env:JAVA_TOOL_OPTIONS = "$oldOptions -Xcheck:jni" }
            $p = Start-App $apk $data $log $mode 'off' $variant.Profile
        } finally { $env:JAVA_TOOL_OPTIONS = $oldOptions }
        $h = Find-AppWindow $p.Id 'Hello JNI' 15
        $cfg = [uint32]0
        $policy = [WinApkTestWin]::GetProcessMitigationPolicy($p.Handle, 7, [ref]$cfg, [UIntPtr]4)
        Write-Host "diagnostic T09.apk.$($variant.Name).cfg=$($cfg -band 1) query=$policy"
        $result = Wait-Log $log 'JNI: native\.(value|error)=(.*)' 10
        $ready = Wait-Log $log 'ViewCanvasDriver: macapkDrawFrame fire #1 present \d+x\d+ nonzero=[1-9]\d*/'
        $ok = $result -and $(if ($mode -eq 'off') {
            $result.Groups[1].Value -eq 'error' -and $result.Groups[2].Value.StartsWith('GUEST_NATIVE_DISABLED:')
        } elseif ($variant.Name -eq 'n0-refused') {
            $result.Groups[1].Value -eq 'error' -and ($result.Groups[2].Value -match '^GUEST_(SYMBOL_UNRESOLVED|ABI_UNSUPPORTED):')
        } else { $result.Groups[1].Value -eq 'value' -and $result.Groups[2].Value.Trim() -eq '43 checks=PASS matrix=PASS' })
        $ok = $ok -and $policy -and $h -ne [IntPtr]::Zero -and $null -ne $ready -and
              (Read-Log $log) -notmatch 'WARNING in native method|FATAL ERROR in native method'
        Check "T09.apk.$($variant.Name)" $ok "$(if ($result) { $result.Value }); frame=$($null -ne $ready)"
        if ($variant.Name -eq 'restricted' -and $ok -and $h -ne [IntPtr]::Zero) {
            $layout = Wait-Log $log 'JNI: refusal.bounds=(\d+,\d+,\d+,\d+)' 5
            if ($layout) {
                Add-Type -AssemblyName System.Drawing
                $rect = New-Object WinApkTestWin+RECT
                [WinApkTestWin]::GetClientRect($h, [ref]$rect) | Out-Null
                $bitmap = New-Object System.Drawing.Bitmap $rect.R, $rect.B
                $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
                $dc = $graphics.GetHdc()
                try { [WinApkTestWin]::PrintWindow($h, $dc, 3) | Out-Null }
                finally { $graphics.ReleaseHdc($dc); $graphics.Dispose() }
                try { $bitmap.Save((Join-Path $logs 'T09-apk-activity.png')) }
                finally { $bitmap.Dispose() }
            }
        }
        if ($variant.Name -eq 'unsupported') {
            $bounds = Wait-Log $log 'JNI: refusal.bounds=(\d+,\d+,\d+,\d+)' 5
            if ($bounds -and $h -ne [IntPtr]::Zero) { Send-Tap $h $bounds.Groups[1].Value }
            $exited = $p.WaitForExit(5000)
            $reason = Read-Log $log
            Check 'T09.apk.unsupported.refused' ($ok -and $exited -and $p.ExitCode -eq 3 -and
                $reason -match 'GUEST_ABI_UNSUPPORTED JNI: unsupported entry \(slot 217\)') "exit $(if ($exited) { $p.ExitCode }); MonitorEnter refused"
            if (-not $exited) { $p.Kill() }
            continue
        }
        if ($h -ne [IntPtr]::Zero) { $code = Close-App $h $p }
        else { if (-not $p.HasExited) { $p.Kill() }; $code = $null }
        Check "T09.apk.$($variant.Name).exit" ($code -eq 0) "exit $code"
    }
    # Mutate only one APK ZIP entry: reject traversal before DEX conversion or any guest execution.
    $bad = Join-Path $data 'unsafe-library.apk'
    Copy-Item $apk $bad -Force
    Add-Type -AssemblyName System.IO.Compression, System.IO.Compression.FileSystem
    $zip = [IO.Compression.ZipFile]::Open($bad, 'Update')
    try {
        $entry = $zip.CreateEntry('lib/arm64-v8a/../escape.so')
        $stream = $entry.Open()
        try { $stream.WriteByte(0) } finally { $stream.Dispose() }
    } finally { $zip.Dispose() }
    $log = Join-Path $logs 'T09-apk-unsafe.log'
    & $winapk run $bad --native=restricted --data-dir $data 2> $log
    $text = Get-Content $log -Raw
    Check 'T09.apk.unsafe-entry' ($LASTEXITCODE -eq 65 -and $text -match 'GUEST_ABI_UNSUPPORTED: portable ARM64' -and $text -notmatch 'stage boot|converting DEX') "exit $LASTEXITCODE; no DEX or guest execution"
}

<# Checks two explicit hardware ANGLE paths and one unavailable-Vulkan refusal; never counts as G4. #>
function Test-GraphicsHost {
    $probe = Join-Path $out 'bin\angle-probe.exe'
    $directory = Join-Path $out 'bin\angle'
    $logs = Join-Path $out 'test-data\logs'
    New-Item -ItemType Directory -Force $logs | Out-Null
    $record = Get-Content (Join-Path $directory 'angle.json') -Raw | ConvertFrom-Json
    $fixture = Get-Content (Join-Path $out 'bin\angle-probe.json') -Raw | ConvertFrom-Json
    Check 'T10.probe' ((Get-PeMachine $probe) -eq 'ARM64' -and
        (Get-FileHash $probe -Algorithm SHA256).Hash.ToLower() -eq $fixture.sha256 -and
        (Get-FileHash (Join-Path $repo $fixture.source) -Algorithm SHA256).Hash.ToLower() -eq $fixture.sourceSha256) 'ARM64 probe, source/build/expected-output record'
    foreach ($name in 'libEGL.dll', 'libGLESv2.dll') {
        $path = Join-Path $directory $name
        Check "T10.$name" ((Get-PeMachine $path) -eq 'ARM64' -and
            (Get-FileHash $path -Algorithm SHA256).Hash.ToLower() -eq $record.sha256.$name) 'ARM64 and recorded source-build hash'
    }
    foreach ($variant in @(
        @{ Name = 'vulkan'; Backend = 'vulkan'; Unavailable = $false },
        @{ Name = 'd3d11'; Backend = 'd3d11'; Unavailable = $false },
        @{ Name = 'vulkan-unavailable'; Backend = 'vulkan'; Unavailable = $true }
    )) {
        $stdout = Join-Path $logs "T10-host-$($variant.Name).out"
        $stderr = Join-Path $logs "T10-host-$($variant.Name).err"
        $saved = @{}
        foreach ($key in 'VK_DRIVER_FILES', 'VK_ICD_FILENAMES', 'VK_LOADER_DRIVERS_SELECT', 'PATH') {
            $saved[$key] = [Environment]::GetEnvironmentVariable($key)
        }
        try {
            # Prove the private DLL path does not rely on development-tool PATH entries.
            $env:PATH = "$env:SystemRoot\System32;$env:SystemRoot"
            if ($variant.Unavailable) {
                $env:VK_DRIVER_FILES = Join-Path $logs 'not-an-installed-icd.json'
                $env:VK_ICD_FILENAMES = $env:VK_DRIVER_FILES
                $env:VK_LOADER_DRIVERS_SELECT = '*not-an-installed-icd*'
            }
            $p = Start-Process $probe -ArgumentList @($variant.Backend, "`"$directory`"") -NoNewWindow -PassThru `
                -RedirectStandardOutput $stdout -RedirectStandardError $stderr
        } finally {
            Set-TestEnv $saved
        }
        try {
            if (-not $p.WaitForExit(30000)) {
                $p.Kill(); $p.WaitForExit()
                Check "T10.host.$($variant.Name)" $false 'exceeded 30 seconds'
                continue
            }
            $text = [string](Get-Content $stdout -Raw)
            $errorText = [string](Get-Content $stderr -Raw)
            if ($variant.Unavailable) {
                $ok = $p.ExitCode -eq 2 -and $text -match 'GRAPHICS_CAPABILITY_MISSING backend=vulkan.*no fallback' -and
                    $text -notmatch 'renderer backend=d3d11|result T10.host PASS'
            } else {
                $ok = $p.ExitCode -eq 0 -and $text.Contains("result T10.host PASS backend=$($variant.Backend)") -and
                    $text -match 'software=no' -and [regex]::Matches($text, '(?m)^pixel ').Count -eq 4
                $text -split "`n" | Where-Object { $_ -match '^renderer |^device |^pixel ' } | ForEach-Object { Write-Host $_ }
            }
            Check "T10.host.$($variant.Name)" $ok "exit $($p.ExitCode); host only, not G4$(if (-not $ok) { '; ' + $text + $errorText })"
        } finally { $p.Dispose() }
    }
}

<# Runs one APK through explicit Vulkan, auto-to-D3D11, and explicit Vulkan refusal. #>
function Test-GraphicsJava {
    $script:appSize = '360x720'
    $apk = Join-Path $repo 'tests\fixtures\hello-gles.apk'
    $fixture = Get-Content (Join-Path $repo 'tests\fixtures\hello-gles.json') -Raw | ConvertFrom-Json
    Check 'T10.java.hash' ((Get-FileHash $apk).Hash.ToLower() -eq $fixture.apkSha256) 'APK build record'
    $logs = Join-Path $out 'test-data\logs'
    New-Item -ItemType Directory -Force $logs | Out-Null
    $data = Join-Path $out 'test-data\graphics-java'
    foreach ($variant in @(
        @{ Name = 'vulkan'; Request = 'vulkan'; Backend = 'vulkan'; Unavailable = $false },
        @{ Name = 'auto-hardware'; Request = 'auto'; Backend = 'd3d11'; Unavailable = $false },
        @{ Name = 'auto'; Request = 'auto'; Backend = 'd3d11'; Unavailable = $true },
        @{ Name = 'vulkan-unavailable'; Request = 'vulkan'; Backend = ''; Unavailable = $true }
    )) {
        $log = Join-Path $logs "T10-java-$($variant.Name).log"
        $saved = @{}
        foreach ($key in 'VK_DRIVER_FILES', 'VK_ICD_FILENAMES', 'VK_LOADER_DRIVERS_SELECT', 'JAVA_TOOL_OPTIONS') {
            $saved[$key] = [Environment]::GetEnvironmentVariable($key)
        }
        try {
            if ($variant.Unavailable) {
                $env:VK_DRIVER_FILES = Join-Path $logs 'not-an-installed-icd.json'
                $env:VK_ICD_FILENAMES = $env:VK_DRIVER_FILES
                $env:VK_LOADER_DRIVERS_SELECT = '*not-an-installed-icd*'
            }
            $env:JAVA_TOOL_OPTIONS = "$($saved.JAVA_TOOL_OPTIONS) -Xcheck:jni"
            $p = Start-App $apk $data $log 'off' $variant.Request
        } finally {
            Set-TestEnv $saved
        }
        try {
            if (-not $variant.Backend) {
                $exited = $p.WaitForExit(30000)
                $text = Read-Log $log
                Check 'T10.java.vulkan-unavailable' ($exited -and $p.ExitCode -eq 1 -and
                    $text -match 'GRAPHICS_CAPABILITY_MISSING: Java GLES backend=vulkan.*no fallback' -and
                    $text -notmatch 'backend=d3d11|T10\.java: PASS|WARNING in native method|FATAL ERROR in native method') 'explicit Vulkan, no alternate renderer'
                continue
            }
            $h = Find-AppWindow $p.Id 'Hello GLES' 15 $p
            $first = Wait-Log $log 'T10\.java: PASS frame=1 contexts=1 size=(\d+)x(\d+)' 30 $p
            if (!$first -or !$h) {
                Check "T10.java.$($variant.Name)" $false 'no first frame or window; see APK log'
                continue
            }
            $wr = New-Object WinApkTestWin+RECT
            $rc = New-Object WinApkTestWin+RECT
            $resized = [WinApkTestWin]::GetWindowRect($h, [ref]$wr) -and
                [WinApkTestWin]::SetWindowPos($h, [IntPtr]::Zero, 0, 0, ($wr.R - $wr.L + 64), ($wr.B - $wr.T + 32), 0x16) -and
                [WinApkTestWin]::GetClientRect($h, [ref]$rc)
            $extent = "$($rc.R)x$($rc.B)"
            $second = if ($resized) { Wait-Log $log "T10\.java: PASS frame=2 contexts=1 size=$extent\b" 10 $p }
            $restored = $false
            if ($second -and $variant.Request -eq 'auto') {
                Send-SysCommand $h 0xF020
                if (Wait-Log $log 'minimised; Java GLES suspended' 5 $p) {
                    Send-SysCommand $h 0xF120
                    $restored = $null -ne (Wait-Log $log "restored; drawing at full rate[\s\S]*T10\.java: PASS frame=3 contexts=1 size=$extent\b" 10 $p)
                }
            }
            $code = Close-App $h $p
            $text = Read-Log $log
            $nextFrame = if ($variant.Request -eq 'auto') { 4 } else { 3 }
            Check "T10.java.$($variant.Name)" ($first -and $second -and $extent -ne "$($first.Groups[1].Value)x$($first.Groups[2].Value)" -and
                $code -eq 0 -and $text -match "Java GLES request=$($variant.Request) backend=$($variant.Backend) renderer=" -and
                ($variant.Request -ne 'auto' -or ($restored -and $text -match 'Java GLES auto: (?:no native vendor Vulkan ICD; preferring hardware d3d11|backend=vulkan.*trying d3d11)')) -and
                $text -match 'Java GLES teardown complete' -and
                $text -notmatch "GRAPHICS_RENDER_FAILED|WARNING in native method|FATAL ERROR in native method|T10\.java: PASS frame=$nextFrame\b") "APK frame readbacks, resized to $extent, one context, restore=$restored and normal teardown"
        } finally {
            if (!$p.HasExited) { $p.Kill(); $p.WaitForExit() }
            $p.Dispose()
        }
    }
}

<# Checks actual NDK window references and replacement in one APK, without any EGL or Vulkan. #>
function Test-Surface {
    $apk = Join-Path $repo 'tests\fixtures\hello-surface.apk'
    $record = Get-Content (Join-Path $repo 'tests\fixtures\hello-surface.json') -Raw | ConvertFrom-Json
    Check 'T10.surface.hash' ((Get-FileHash $apk).Hash.ToLower() -eq $record.apkSha256) 'APK and NDK build record'
    $script:appSize = '360x720'
    [WinApkTestWin]::SetProcessDpiAwarenessContext([IntPtr]-4) | Out-Null
    $logs = Join-Path $out 'test-data\logs'
    New-Item -ItemType Directory -Force $logs | Out-Null
    $log = Join-Path $logs 'T10-surface.log'
    $data = Join-Path $out 'test-data\surface'
    $oldOptions = [Environment]::GetEnvironmentVariable('JAVA_TOOL_OPTIONS')
    try {
        $env:JAVA_TOOL_OPTIONS = "$oldOptions -Xcheck:jni"
        $p = Start-App $apk $data $log 'restricted'
    } finally { Set-TestEnv @{ JAVA_TOOL_OPTIONS = $oldOptions } }
    try {
        $h = Find-AppWindow $p.Id 'Hello Surface' 15 $p
        $initial = Wait-Log $log 'T10\.surface: initial=(\d+x\d+)' 15 $p
        $rc = New-Object WinApkTestWin+RECT
        $ok = $h -and $initial -and [WinApkTestWin]::GetClientRect($h, [ref]$rc) -and
            $initial.Groups[1].Value -eq "$($rc.R)x$($rc.B)"
        Check 'T10.surface.attach' $ok "actual Java holder and initial client extent $($rc.R)x$($rc.B)"
        if (-not $ok) { return }
        $wr = New-Object WinApkTestWin+RECT
        $resized = [WinApkTestWin]::GetWindowRect($h, [ref]$wr) -and
            [WinApkTestWin]::SetWindowPos($h, [IntPtr]::Zero, 0, 0, ($wr.R - $wr.L + 64), ($wr.B - $wr.T + 32), 0x16) -and
            [WinApkTestWin]::GetClientRect($h, [ref]$rc)
        $extent = "$($rc.R)x$($rc.B)"
        $after = if ($resized) { Wait-Log $log "T10\.surface: resized=$extent\b" 10 $p }
        Check 'T10.surface.resize' ($after -and $initial.Groups[1].Value -ne $extent) "same guest reference at new client extent $extent"
        $replaced = Wait-Log $log 'T10\.surface: PASS release/replace/stale' 10 $p
        $code = Close-App $h $p
        $text = Read-Log $log
        Check 'T10.surface.replace' ($replaced -and $code -eq 0 -and
            $text -notmatch 'WARNING in native method|FATAL ERROR in native method|Java GLES request=|AssertionError') 'new attachment identity, negative token matrix, CheckJNI and normal close'
    } finally {
        if (!$p.HasExited) { $p.Kill(); $p.WaitForExit() }
        $p.Dispose()
    }
}

<# Runs two explicit guest backends and the same APK with Vulkan hidden from the child. #>
function Test-GraphicsGuest {
    $apk = Join-Path $repo 'tests\fixtures\hello-guest-gles.apk'
    $record = Get-Content (Join-Path $repo 'tests\fixtures\hello-guest-gles.json') -Raw | ConvertFrom-Json
    Check 'T11.hash' ((Get-FileHash $apk).Hash.ToLower() -eq $record.apkSha256) 'APK and NDK build record'
    $script:appSize = '360x720'
    [WinApkTestWin]::SetProcessDpiAwarenessContext([IntPtr]-4) | Out-Null
    $logs = Join-Path $out 'test-data\logs'
    New-Item -ItemType Directory -Force $logs | Out-Null
    $data = Join-Path $out 'test-data\graphics-guest'
    foreach ($variant in @(
        @{ Name = 'd3d11'; Request = 'd3d11'; Unavailable = $false },
        @{ Name = 'vulkan'; Request = 'vulkan'; Unavailable = $false },
        @{ Name = 'vulkan-unavailable'; Request = 'vulkan'; Unavailable = $true }
    )) {
        $log = Join-Path $logs "T11-$($variant.Name).log"
        $saved = @{}
        foreach ($key in 'VK_DRIVER_FILES', 'VK_ICD_FILENAMES', 'VK_LOADER_DRIVERS_SELECT', 'JAVA_TOOL_OPTIONS') {
            $saved[$key] = [Environment]::GetEnvironmentVariable($key)
        }
        try {
            if ($variant.Unavailable) {
                $env:VK_DRIVER_FILES = Join-Path $logs 'not-an-installed-icd.json'
                $env:VK_ICD_FILENAMES = $env:VK_DRIVER_FILES
                $env:VK_LOADER_DRIVERS_SELECT = '*not-an-installed-icd*'
            }
            $env:JAVA_TOOL_OPTIONS = "$($saved.JAVA_TOOL_OPTIONS) -Xcheck:jni"
            $p = Start-App $apk $data $log 'restricted' $variant.Request
        } finally { Set-TestEnv $saved }
        try {
            $h = Find-AppWindow $p.Id 'Guest GLES' 15 $p
            $first = Wait-Log $log 'T11: (PASS initial=(\d+x\d+)|EGL_NOT_INITIALIZED|FAIL [^\r\n]+)' 15 $p
            if (!$h -or !$first) {
                Check "T11.$($variant.Name)" $false 'no initial outcome or window; see APK log'
                continue
            }
            if ($variant.Unavailable) {
                $code = Close-App $h $p
                $text = Read-Log $log
                Check 'T11.vulkan-unavailable' ($code -eq 0 -and $first.Value -eq 'T11: EGL_NOT_INITIALIZED' -and
                    $text -match 'guest EGL backend=vulkan; no fallback' -and
                    $text -notmatch 'T11: PASS|backend=d3d11|WARNING in native method|FATAL ERROR in native method') 'explicit Vulkan initialization refused; APK observed the EGL error'
                continue
            }
            if (!$first.Groups[2].Success) {
                Check "T11.$($variant.Name)" $false $first.Value
                continue
            }
            $wr = New-Object WinApkTestWin+RECT
            $rc = New-Object WinApkTestWin+RECT
            $initial = [WinApkTestWin]::GetClientRect($h, [ref]$rc) -and $first.Groups[2].Value -eq "$($rc.R)x$($rc.B)"
            Send-SysCommand $h 0xF020
            $minimized = Wait-Log $log 'T11: (PASS minimized=EGL_BAD_SURFACE|FAIL [^\r\n]+)' 5 $p
            Send-SysCommand $h 0xF120
            $initialExtent = $first.Groups[2].Value
            $restore = if ($minimized -and $minimized.Value -eq 'T11: PASS minimized=EGL_BAD_SURFACE') {
                Wait-Log $log "T11: (PASS restored=$initialExtent\b|FAIL [^\r\n]+)" 5 $p
            }
            if (!$restore -or $restore.Value -ne "T11: PASS restored=$initialExtent") {
                Check "T11.$($variant.Name)" $false 'expected zero-size swap refusal and same-extent draw after restore; see APK log'
                continue
            }
            $resized = [WinApkTestWin]::GetWindowRect($h, [ref]$wr) -and
                [WinApkTestWin]::SetWindowPos($h, [IntPtr]::Zero, 0, 0, ($wr.R - $wr.L + 64), ($wr.B - $wr.T + 32), 0x16) -and
                [WinApkTestWin]::GetClientRect($h, [ref]$rc)
            $extent = "$($rc.R)x$($rc.B)"
            $after = if ($resized) { Wait-Log $log "T11: (PASS recreated=$extent\b|FAIL [^\r\n]+)" 10 $p }
            $code = Close-App $h $p
            $text = Read-Log $log
            Check "T11.$($variant.Name)" ($initial -and $after -and $after.Value -eq "T11: PASS recreated=$extent" -and
                $extent -ne $first.Groups[2].Value -and $code -eq 0 -and
                $text -match "guest EGL request=$($variant.Request) backend=$($variant.Request)" -and
                $text -match "guest GLES renderer backend=$($variant.Request) name=ANGLE" -and
                $text -match 'guest EGL teardown complete' -and
                $text -notmatch 'WARNING in native method|FATAL ERROR in native method|GRAPHICS_RENDER_FAILED|T11: FAIL') "APK readback through minimize/restore and resize; recreated $extent; EGL teardown"
        } finally {
            if (!$p.HasExited) { $p.Kill(); $p.WaitForExit() }
            $p.Dispose()
        }
    }
}

<# Checks guest Vulkan transfer-clear presentation and Android WSI in one APK. #>
function Test-VulkanWsi {
    $apk = Join-Path $repo 'tests\fixtures\hello-vulkan-wsi.apk'
    $record = Get-Content (Join-Path $repo 'tests\fixtures\hello-vulkan-wsi.json') -Raw | ConvertFrom-Json
    Check 'T13.wsi.hash' ((Get-FileHash $apk).Hash.ToLower() -eq $record.apkSha256) 'APK and NDK build record'
    $script:appSize = '360x720'
    [WinApkTestWin]::SetProcessDpiAwarenessContext([IntPtr]-4) | Out-Null
    $logs = Join-Path $out 'test-data\logs'
    New-Item -ItemType Directory -Force $logs | Out-Null
    $data = Join-Path $out 'test-data\vulkan-wsi'
    foreach ($unavailable in $false, $true) {
        $name = if ($unavailable) { 'unavailable' } else { 'live' }
        $log = Join-Path $logs "T13-wsi-$name.log"
        $saved = @{}
        foreach ($key in 'VK_DRIVER_FILES', 'VK_ICD_FILENAMES', 'VK_LOADER_DRIVERS_SELECT', 'JAVA_TOOL_OPTIONS') {
            $saved[$key] = [Environment]::GetEnvironmentVariable($key)
        }
        try {
            if ($unavailable) {
                $env:VK_DRIVER_FILES = Join-Path $logs 'not-an-installed-icd.json'
                $env:VK_ICD_FILENAMES = $env:VK_DRIVER_FILES
                $env:VK_LOADER_DRIVERS_SELECT = '*not-an-installed-icd*'
            }
            $env:JAVA_TOOL_OPTIONS = "$($saved.JAVA_TOOL_OPTIONS) -Xcheck:jni"
            $p = Start-App $apk $data $log 'restricted' 'vulkan'
        } finally { Set-TestEnv $saved }
        try {
            $h = Find-AppWindow $p.Id 'Guest Vulkan WSI' 15 $p
            $first = Wait-Log $log 'T13\.wsi: (initial=(\d+x\d+)|VULKAN_UNAVAILABLE|FAIL [^\r\n]+)' 12 $p
            if (!$h -or !$first) {
                Check "T13.wsi.$name" $false 'no initial result; see APK log'
                continue
            }
            if ($unavailable) {
                $code = Close-App $h $p
                $text = Read-Log $log
                Check 'T13.wsi.unavailable' ($code -eq 0 -and $first.Value -eq 'T13.wsi: VULKAN_UNAVAILABLE' -and
                    $text -notmatch 'T13\.wsi: initial=|backend=d3d11|WARNING in native method|FATAL ERROR in native method') 'Vulkan ICD hidden; no WSI or fallback'
                continue
            }
            if (!$first.Groups[2].Success) {
                Check 'T13.wsi.live' $false $first.Value
                continue
            }
            $wr = New-Object WinApkTestWin+RECT
            $rc = New-Object WinApkTestWin+RECT
            $initial = [WinApkTestWin]::GetClientRect($h, [ref]$rc) -and $first.Groups[2].Value -eq "$($rc.R)x$($rc.B)"
            Send-SysCommand $h 0xF020
            $minimized = Wait-Log $log 'T13\.wsi: (PASS minimized=VK_ERROR_OUT_OF_DATE_KHR|FAIL [^\r\n]+)' 5 $p
            Send-SysCommand $h 0xF120
            $initialExtent = $first.Groups[2].Value
            $restore = if ($minimized -and $minimized.Value -eq 'T13.wsi: PASS minimized=VK_ERROR_OUT_OF_DATE_KHR') {
                Wait-Log $log "T13\.wsi: (restored=$initialExtent\b|FAIL [^\r\n]+)" 5 $p
            }
            if (!$restore -or $restore.Value -ne "T13.wsi: restored=$initialExtent") {
                Check 'T13.wsi.live' $false 'expected zero-size acquire refusal and present after restore; see APK log'
                continue
            }
            $resized = [WinApkTestWin]::GetWindowRect($h, [ref]$wr) -and
                [WinApkTestWin]::SetWindowPos($h, [IntPtr]::Zero, 0, 0, ($wr.R - $wr.L + 64), ($wr.B - $wr.T + 32), 0x16) -and
                [WinApkTestWin]::GetClientRect($h, [ref]$rc)
            $extent = "$($rc.R)x$($rc.B)"
            $after = if ($resized) { Wait-Log $log 'T13\.wsi: (PASS replacement|FAIL [^\r\n]+)' 10 $p }
            $code = Close-App $h $p
            $text = Read-Log $log
            Check 'T13.wsi.live' ($initial -and $after -and $after.Value -eq 'T13.wsi: PASS replacement' -and
                $extent -ne $first.Groups[2].Value -and $text -match "T13\.wsi: resized=$extent\b" -and $code -eq 0 -and
                $text -match 'guest Vulkan instance teardown complete' -and
                $text -notmatch 'WARNING in native method|FATAL ERROR in native method|T13\.wsi: FAIL') "graphics pipeline + VkImage shader readback, guest allocator/debug callbacks, GPU buffer readback, three presents across minimize/restore and resize to $extent, replacement and teardown"
        } finally {
            if (!$p.HasExited) { $p.Kill(); $p.WaitForExit() }
            $p.Dispose()
        }
    }
    $lossLog = Join-Path $logs 'T13-wsi-device-loss.log'
    $savedLoss = @{ WINAPK_TEST_VK_DEVICE_LOST = [Environment]::GetEnvironmentVariable('WINAPK_TEST_VK_DEVICE_LOST') }
    Set-TestEnv @{ WINAPK_TEST_VK_DEVICE_LOST = '1' }
    $lp = Start-App $apk $data $lossLog 'restricted' 'vulkan'
    try {
        $lh = Find-AppWindow $lp.Id 'Guest Vulkan WSI' 15 $lp
        $lossHit = Wait-Log $lossLog 'T13\.wsi: FAIL initial-present=' 15 $lp
        $lcode = if ($lh) { Close-App $lh $lp }
        $ltext = Read-Log $lossLog
        Check 'T13.wsi.deviceLoss' ([bool]$lossHit -and $lcode -eq 0 -and
            $ltext -match 'injected device loss' -and $ltext -match 'guest Vulkan instance teardown complete') 'VK_ERROR_DEVICE_LOST surfaces to guest and tears down without hanging'
    } finally {
        Set-TestEnv $savedLoss
        if (!$lp.HasExited) { $lp.Kill(); $lp.WaitForExit() }
        $lp.Dispose()
    }
}

<# Verifies that terminating the launcher reaps an active DEX converter. #>
function Test-DexJob {
    $root = Join-Path $out ("test-data\dex-job-" + [guid]::NewGuid().ToString('N'))
    New-Item -ItemType Directory $root | Out-Null
    $log = Join-Path $root 'run.log'
    $apk = Join-Path $repo 'tests\fixtures\hello-jni.apk'
    $script:appSize = '360x720'
    $saved = @{ JAVA_TOOL_OPTIONS = [Environment]::GetEnvironmentVariable('JAVA_TOOL_OPTIONS') }
    try {
        # Gate JVM startup on a process-local, ephemeral loopback socket so the child cannot
        # finish normally before cancellation. Nothing attaches to the debug agent.
        $env:JAVA_TOOL_OPTIONS = '-agentlib:jdwp=transport=dt_socket,server=y,suspend=y,address=127.0.0.1:0'
        $p = Start-App $apk $root $log 'restricted'
    } finally { Set-TestEnv $saved }
    $child = $null
    try {
        $started = Wait-Log $log 'stage dex-child pid=(\d+)' 10 $p
        if ($started) { $child = Get-Process -Id ([int]$started.Groups[1].Value) -ErrorAction SilentlyContinue }
        $ready = if ($child) { Wait-Log "$log.out" 'Listening for transport dt_socket at address: \d+' 5 $child }
        if (!$ready -or $child.HasExited -or $p.HasExited) {
            Check 'T14.dex-job' $false "converter did not reach its startup gate; see $log"
            return
        }
        $p.Kill()
        $exited = $p.WaitForExit(5000) -and $child.WaitForExit(5000)
        $cached = @(Get-ChildItem -LiteralPath $root -Recurse -File -Filter 'classes.jar')
        Check 'T14.dex-job' ($exited -and !$cached.Count -and (Read-Log $log) -notmatch 'stage dex-converted') 'launcher termination reaped the live converter without publishing a cache'
    } finally {
        if (!$p.HasExited) { $p.Kill(); $p.WaitForExit() }
        $p.Dispose()
        if ($child) {
            if (!$child.HasExited) { $child.Kill(); $child.WaitForExit() }
            $child.Dispose()
        }
    }
}

<# Verifies the staged archive and runs one real APK from a fresh Unicode package path. #>
function Test-Package {
    if (!$PackageZip) { throw 'Package requires -PackageZip (output from scripts/windows/package.ps1)' }
    $zip = (Resolve-Path -LiteralPath $PackageZip).Path
    $root = Join-Path $out ("test-data\CLI 包 " + [guid]::NewGuid().ToString('N').Substring(0,8))
    $install = Join-Path $root '安装 文件'
    [IO.Compression.ZipFile]::ExtractToDirectory($zip, $install)
    $manifest = Get-Content -LiteralPath (Join-Path $install 'package.json') -Raw | ConvertFrom-Json
    $files = @(Get-ChildItem -LiteralPath $install -Recurse -File)
    $hashes = @{}
    foreach ($file in $files) {
        $relative = [IO.Path]::GetRelativePath($install, $file.FullName).Replace('\','/')
        if ($relative -ne 'package.json') { $hashes[$relative] = (Get-FileHash -LiteralPath $file.FullName).Hash.ToLowerInvariant() }
    }
    $listed = @{}
    foreach ($entry in $manifest.files) { $listed.Add($entry.path, $entry.sha256) }
    $source = Join-Path (Split-Path -Parent $zip) $manifest.source.archive
    $bad = @($listed.Keys | Where-Object { !$hashes.ContainsKey($_) -or $listed[$_] -ne $hashes[$_] })
    $pe = @($files | Where-Object Extension -in '.dll','.exe')
    $badPe = @($pe | Where-Object { (Get-PeMachine $_.FullName) -ne 'ARM64' })
    $isBundle = $manifest.PSObject.Properties['target'] -and $manifest.target -eq 'bundle'
    $frontendOk = if ($isBundle) { Test-Path -LiteralPath (Join-Path $install 'Windows-ARM64.apk.exe') } else { $true }
    $minPe = if ($isBundle) { 6 } else { 5 }
    $archived = $manifest.arch -eq 'arm64' -and $manifest.configuration -eq 'Release' -and $frontendOk -and
        $hashes.Count -eq $listed.Count -and $bad.Count -eq 0 -and $badPe.Count -eq 0 -and $pe.Count -ge $minPe -and
        (Get-FileHash -LiteralPath $source).Hash.ToLowerInvariant() -eq $manifest.source.sha256
    if ($archived) {
        $snapshot = [IO.Compression.ZipFile]::OpenRead($source)
        try {
            $entries = @($snapshot.Entries | ForEach-Object FullName)
            $archived = -not ($entries -match '\.dylib$') -and
                [bool]($entries -match '/macapk/runtime/platform/windows/src/run\.cpp$')
        } finally { $snapshot.Dispose() }
    }
    Check 'T14.package.inventory' $archived "$($hashes.Count) payload files, $($pe.Count) ARM64 PEs$(if ($isBundle) { ' (front end + runtime)' }), source ZIP hash"
    if (!$archived) { return }

    $winapk = Join-Path $install 'bin\winapk.exe'
    $fixture = Join-Path $root '样例 APK.apk'
    Copy-Item -LiteralPath (Join-Path $repo 'tests\fixtures\hello-jni.apk') -Destination $fixture
    $jdk = Find-Jdk
    $keys = 'PATH','JAVA_HOME','WINAPK_JAVA_HOME','LOCALAPPDATA','JAVA_TOOL_OPTIONS'
    $saved = @{}
    foreach ($key in $keys) { $saved[$key] = [Environment]::GetEnvironmentVariable($key) }
    $env:LOCALAPPDATA = Join-Path $root '干净 用户'
    New-Item -ItemType Directory $env:LOCALAPPDATA | Out-Null
    Set-TestEnv @{ PATH = "$env:SystemRoot\System32;$env:SystemRoot"; JAVA_HOME = $null; WINAPK_JAVA_HOME = $null; JAVA_TOOL_OPTIONS = $null }
    try {
        $inspect = & $winapk inspect $fixture --json
        $inspectExit = $LASTEXITCODE
        $missing = & $winapk run $fixture --native=restricted --data-dir (Join-Path $root '缺少 JVM') 2>&1
        Check 'T14.package.dependencies' ($inspectExit -eq 0 -and ($inspect -join "`n") -match 'org\.winapk\.fixture\.jni' -and
            $LASTEXITCODE -eq 69 -and ($missing -join "`n") -match 'DEPENDENCY_MISSING: no JVM found') 'inspect works without Java; run refuses missing JVM before boot'
        $env:WINAPK_JAVA_HOME = $jdk
        $doctor = & $winapk doctor --json | ConvertFrom-Json
        Check 'T14.package.doctor' ($LASTEXITCODE -eq 0 -and $doctor.jvm.started -eq 'ok' -and
            $doctor.jvm.arch -eq 'arm64' -and $doctor.jvm.source -eq 'WINAPK_JAVA_HOME' -and
            -not $doctor.graphics.angleFilesArm64) 'external ARM64 JVM; private ANGLE explicitly absent'
        $script:appSize = '360x720'
        $log = Join-Path $root 'run.log'
        $p = Start-App $fixture (Join-Path $root '应用 数据') $log 'restricted'
        try {
            $h = Find-AppWindow $p.Id 'Hello JNI' 15 $p
            $frame = Wait-Log $log 'ViewCanvasDriver: macapkDrawFrame fire #1 present \d+x\d+ nonzero=[1-9]\d*/' 30 $p
            $code = if ($h) { Close-App $h $p }
            $text = Read-Log $log
            Check 'T14.package.run' ($h -and $frame -and $code -eq 0 -and $text -match 'stage dex-converted' -and
                $text -match 'JNI: native\.value=43 checks=PASS matrix=PASS') 'cold DEX, JNI values, real bitmap frame and normal close from unpacked ZIP'
        } finally {
            if (!$p.HasExited) { $p.Kill(); $p.WaitForExit() }
            $p.Dispose()
        }
    } finally { Set-TestEnv $saved }
}

<# Verifies NativeOs, isolated host clipboard, DirectWrite fonts, /proc synthesis, URL allowlist and WM_DPICHANGED. #>
function Test-HostServices {
    $jdk = Find-Jdk
    $lib = Join-Path $out 'lib'
    $bridge = Join-Path $out 'bin\winapk-bridge.dll'
    if (-not (Test-Path $bridge)) { throw "Missing $bridge; run build.ps1 -Component Host" }
    $root = Join-Path $out ('test-data\host-services-' + [guid]::NewGuid().ToString('N'))
    $classes = Join-Path $root 'classes'
    $work = Join-Path $root 'work'
    $urlCap = Join-Path $root 'url-capture.txt'
    New-Item -ItemType Directory -Force $classes, $work | Out-Null

    $cp = "$lib\android-framework.jar;$lib\macapk-loader.jar;$lib\bcprov-jdk18on-1.79.jar"
    Invoke-Checked (Join-Path $jdk 'bin\javac.exe') @(
        '--release', '21', '-encoding', 'UTF-8', '-cp', $cp, '-d', $classes,
        (Join-Path $repo 'tests\windows-arm64\host-probes\HostServicesProbe.java')
    )
    $saved = @{
        MACAPK_CLIPBOARD_TEST_PASTEBOARD = [Environment]::GetEnvironmentVariable('MACAPK_CLIPBOARD_TEST_PASTEBOARD')
        WINAPK_TEST_CAPTURE_URL = [Environment]::GetEnvironmentVariable('WINAPK_TEST_CAPTURE_URL')
        PATH = [Environment]::GetEnvironmentVariable('PATH')
    }
    Set-TestEnv @{
        MACAPK_CLIPBOARD_TEST_PASTEBOARD = 'unique'
        WINAPK_TEST_CAPTURE_URL = $urlCap
        PATH = "$(Split-Path $bridge);$($saved.PATH)"
    }
    try {
        $javaArgs = @(
            '-Xcheck:jni',
            "--patch-module=java.base=$lib\java_base_patch",
            "--patch-module=jdk.unsupported=$lib\jdk_unsupported_patch",
            '--enable-native-access=ALL-UNNAMED',
            '--add-opens=java.base/java.io=ALL-UNNAMED',
            '--add-opens=java.base/java.lang=ALL-UNNAMED',
            '-cp', "$classes;$cp", 'HostServicesProbe', $bridge, $work, $urlCap
        )
        $probeOut = & (Join-Path $jdk 'bin\java.exe') @javaArgs 2>&1
        $probeCode = $LASTEXITCODE
        $txt = ($probeOut -join "`n")
        Check 'HostServices.nativeOs' ($probeCode -eq 0 -and $txt -match 'PASS NativeOs fd/memfd/lock/sysconf/statvfs/env/xz') 'fd/memfd/flock/sysconf/statvfs/env/liblzma xz positive + negative'
        Check 'HostServices.clipboard' ($probeCode -eq 0 -and $txt -match 'PASS ClipboardManager isolated roundtrip') 'isolated pasteboard UTF-16 emoji, CF_HTML, ConcealedType and focus gate'
        Check 'HostServices.fontsProcUrl' ($probeCode -eq 0 -and $txt -match 'PASS DirectWrite host fonts, /proc synthesis and openHostUrl allowlist' -and
            $txt -notmatch 'WARNING in native method|FATAL ERROR in native method') 'DirectWrite CJK fonts.xml, /proc/meminfo + status, URL scheme allowlist and CheckJNI'
        if ($probeCode -ne 0) { Write-Host $txt }
    } finally {
        Set-TestEnv $saved
    }

    # Dynamic WM_DPICHANGED check on a real WinApkAppWindow
    $apk = Join-Path $repo 'tests\fixtures\hello-java.apk'
    $dpiLog = Join-Path $root 'dpi.log'
    $script:appSize = '360x720'
    $p = Start-App $apk (Join-Path $root 'dpi-data') $dpiLog 'off'
    try {
        $h = Find-AppWindow $p.Id 'Hello Java' 20 $p
        $first = Wait-Log $dpiLog 'ViewCanvasDriver: macapkDrawFrame fire #1 present' 25 $p
        $dpiOk = $false
        if ($h -ne [IntPtr]::Zero -and $first) {
            $wr = New-Object WinApkTestWin+RECT
            [void][WinApkTestWin]::GetWindowRect($h, [ref]$wr)
            $wr.R = $wr.L + 540
            $wr.B = $wr.T + 1080
            $wparam = [IntPtr]((144 -shl 16) -bor 144)
            [void][WinApkTestWin]::SendMessageW($h, 0x02E0, $wparam, [ref]$wr) # WM_DPICHANGED
            $dpiHit = Wait-Log $dpiLog '\[winapk\] window: dpi changed to 144' 10 $p
            $code = Close-App $h $p
            $dpiOk = [bool]$dpiHit -and ($code -eq 0)
        }
        Check 'HostServices.dpiChanged' $dpiOk 'WM_DPICHANGED resizes window, invalidates surface/canvas and exits cleanly'
    } finally {
        if (!$p.HasExited) { $p.Kill(); $p.WaitForExit() }
        $p.Dispose()
    }
}

<# Verifies WASAPI hardware output + endpoint rebind, formats/latency/resampling, SoundPool/MediaPlayer and deviceless fallback. #>
function Test-Audio {
    $jdk = Find-Jdk
    $lib = Join-Path $out 'lib'
    $bridge = Join-Path $out 'bin\winapk-bridge.dll'
    if (-not (Test-Path $bridge)) { throw "Missing $bridge; run build.ps1 -Component Host" }
    $root = Join-Path $out ('test-data\audio-' + [guid]::NewGuid().ToString('N'))
    $classes = Join-Path $root 'classes'
    New-Item -ItemType Directory -Force $classes | Out-Null

    $cp = "$lib\android-framework.jar;$lib\macapk-loader.jar;$lib\bcprov-jdk18on-1.79.jar"
    Invoke-Checked (Join-Path $jdk 'bin\javac.exe') @(
        '--release', '21', '-encoding', 'UTF-8', '-cp', $cp, '-d', $classes,
        (Join-Path $repo 'tests\windows-arm64\host-probes\AudioProbe.java')
    )
    $javaExe = Join-Path $jdk 'bin\java.exe'
    $commonArgs = @(
        '-Xcheck:jni',
        "--patch-module=java.base=$lib\java_base_patch",
        "--patch-module=jdk.unsupported=$lib\jdk_unsupported_patch",
        '--enable-native-access=ALL-UNNAMED',
        '-cp', "$classes;$cp", 'AudioProbe', $bridge
    )
    $saved = @{
        WINAPK_AUDIO_BACKEND = [Environment]::GetEnvironmentVariable('WINAPK_AUDIO_BACKEND')
        WINAPK_TEST_AUDIO_REBIND_ONCE = [Environment]::GetEnvironmentVariable('WINAPK_TEST_AUDIO_REBIND_ONCE')
        PATH = [Environment]::GetEnvironmentVariable('PATH')
    }
    try {
        # 1. Deterministic virtual mixer: formats, latency clamp, non-blocking/blocking, resampling, underrun, SoundPool, MediaPlayer
        Set-TestEnv @{ WINAPK_AUDIO_BACKEND = 'virtual'; WINAPK_TEST_AUDIO_REBIND_ONCE = $null; PATH = "$(Split-Path $bridge);$($saved.PATH)" }
        $vOut = (& $javaExe @commonArgs 'virtual' 2>&1) -join "`n"
        Check 'Audio.virtualFormatsAndClips' ($LASTEXITCODE -eq 0 -and $vOut -match 'PASS AudioProbe mode=virtual' -and
            $vOut -notmatch 'WARNING in native method|FATAL ERROR in native method') 'PCM_8/16/FLOAT, buffer clamp, 96kHz resample, underruns, SoundPool and MediaPlayer'

        # 2. Real hardware WASAPI (silent 0.0f samples) + live endpoint-change rebind
        Set-TestEnv @{ WINAPK_AUDIO_BACKEND = 'wasapi'; WINAPK_TEST_AUDIO_REBIND_ONCE = '1'; PATH = "$(Split-Path $bridge);$($saved.PATH)" }
        $hwOut = (& $javaExe @commonArgs 'hardware' 2>&1) -join "`n"
        $hwStarted = $hwOut -match '\[winapk\] audio: WASAPI shared output started'
        $hwRebound = $hwOut -match '\[winapk\] audio: default WASAPI endpoint changed; rebinding'
        Check 'Audio.wasapiHardwareAndRebind' ($LASTEXITCODE -eq 0 -and $hwOut -match 'PASS AudioProbe mode=hardware' -and
            ($hwStarted -and $hwRebound) -and $hwOut -notmatch 'WARNING in native method|FATAL ERROR in native method') 'shared-mode WASAPI streaming, QPC timestamps and live default-endpoint rebind'

        # 3. Deviceless / no-endpoint fallback
        Set-TestEnv @{ WINAPK_AUDIO_BACKEND = 'none'; WINAPK_TEST_AUDIO_REBIND_ONCE = $null; PATH = "$(Split-Path $bridge);$($saved.PATH)" }
        $noneOut = (& $javaExe @commonArgs 'deviceless' 2>&1) -join "`n"
        Check 'Audio.devicelessFallback' ($LASTEXITCODE -eq 0 -and $noneOut -match 'PASS AudioProbe mode=deviceless') 'no-endpoint host degrades without blocking'
    } finally {
        Set-TestEnv $saved
    }
}

<# Verifies P8 T15 Skia Direct3D 12 AGSL RuntimeShader and RenderEffect pixel contracts + negative refusal. #>
function Test-Agsl {
    $jdk = Find-Jdk
    $lib = Join-Path $out 'lib'
    $bridge = Join-Path $out 'bin\winapk-bridge.dll'
    $skiaDll = Join-Path $out 'bin\skia\skiko-windows-arm64.dll'
    if (-not (Test-Path $bridge)) { throw "Missing $bridge; run build.ps1 -Component Host" }
    if (-not (Test-Path $skiaDll)) { throw "Missing $skiaDll; run build.ps1 -Component Skia" }
    Check 'T15.agsl.pe' ((Get-PeMachine $skiaDll) -eq 'ARM64') 'skiko-windows-arm64.dll is ARM64 PE'

    $root = Join-Path $out ('test-data\agsl-' + [guid]::NewGuid().ToString('N'))
    $classes = Join-Path $root 'classes'
    New-Item -ItemType Directory -Force $classes | Out-Null

    $cp = "$lib\android-framework.jar;$lib\macapk-loader.jar;$lib\bcprov-jdk18on-1.79.jar"
    Invoke-Checked (Join-Path $jdk 'bin\javac.exe') @(
        '--release', '21', '-encoding', 'UTF-8', '-cp', $cp, '-d', $classes,
        (Join-Path $repo 'tests\windows-arm64\host-probes\AgslSmokeWindows.java')
    )
    $javaExe = Join-Path $jdk 'bin\java.exe'
    $baseArgs = @(
        '-Xcheck:jni',
        '-Djava.awt.headless=true',
        "--patch-module=java.base=$lib\java_base_patch",
        "--patch-module=jdk.unsupported=$lib\jdk_unsupported_patch",
        '--enable-native-access=ALL-UNNAMED',
        '--add-opens=java.base/java.lang=ALL-UNNAMED',
        '-cp', "$classes;$cp", 'AgslSmokeWindows', $bridge
    )
    $saved = @{
        WINAPK_DISABLE_SKIA = [Environment]::GetEnvironmentVariable('WINAPK_DISABLE_SKIA')
        WINAPK_SKIA_DLL = [Environment]::GetEnvironmentVariable('WINAPK_SKIA_DLL')
        PATH = [Environment]::GetEnvironmentVariable('PATH')
    }
    try {
        Set-TestEnv @{ WINAPK_DISABLE_SKIA = $null; WINAPK_SKIA_DLL = $skiaDll; PATH = "$(Split-Path $bridge);$(Split-Path $skiaDll);$($saved.PATH)" }
        $posOut = (& $javaExe @baseArgs 2>&1) -join "`n"
        $posCode = $LASTEXITCODE
        $backendLine = if ($posOut -match 'BACKEND=([^\r\n]+)') { $Matches[1] } else { 'missing' }
        Check 'T15.agsl.d3d12' ($posCode -eq 0 -and $posOut -match 'PASS ALL 11 T15 AGSL pixel contracts' -and
            $posOut -notmatch 'WARNING in native method|FATAL ERROR in native method') "backend=$backendLine; all 11 AGSL/RenderEffect pixel contracts passed"
        if ($posCode -ne 0) { Write-Host $posOut }

        Set-TestEnv @{ WINAPK_DISABLE_SKIA = '1'; WINAPK_SKIA_DLL = $skiaDll; PATH = "$(Split-Path $bridge);$($saved.PATH)" }
        $negOut = (& $javaExe @baseArgs 'unavailable' 2>&1) -join "`n"
        Check 'T15.agsl.unavailable' ($LASTEXITCODE -eq 0 -and $negOut -match 'PASS T15 unavailable explicit failure without silent CPU fallback') 'disabled Skia fails with GRAPHICS_CAPABILITY_MISSING'
    } finally {
        Set-TestEnv $saved
    }
}

switch ($Suite) { 'Host' { Test-Host } 'HostServices' { Test-HostServices } 'Audio' { Test-Audio } 'Agsl' { Test-Agsl } 'Java' { Test-Java } 'Native' { Test-Native } 'Bionic' { Test-Bionic } 'Modules' { Test-Modules } 'Memory' { Test-Bionic -Command memory } 'Io' { Test-Bionic -Command io } 'Faults' { Test-Bionic -Command faults } 'Format' { Test-Bionic -Command format } 'Libraries' { Test-Libraries } 'Jni' { Test-Jni } 'ApkJni' { Test-ApkJni } 'GraphicsHost' { Test-GraphicsHost } 'GraphicsJava' { Test-GraphicsJava } 'Surface' { Test-Surface } 'GraphicsGuest' { Test-GraphicsGuest } 'VulkanWsi' { Test-VulkanWsi } 'DexJob' { Test-DexJob } 'Package' { Test-Package } 'Stability' { & "$PSScriptRoot\test-stability.ps1"; if ($LASTEXITCODE) { $script:failed++ } } }
if ($script:failed -gt 0) { Write-Host "$script:failed check(s) failed"; exit 1 }
Write-Host 'all checks passed'
exit 0
