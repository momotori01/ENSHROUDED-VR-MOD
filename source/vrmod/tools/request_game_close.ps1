param(
    [Parameter(Mandatory=$true)][string]$Launch,
    [string]$ExpectedExecutable,
    [ValidateRange(1,600)][int]$WaitSeconds=180,
    [switch]$DryRun
)
# Ordinary UI close request for one managed diagnostic launch (Claude replacement for the
# Codex-only "Sky Alt+F4"). It verifies the launch PID/path/creation time, requires exactly one
# unowned top-level game window, posts WM_SYSCOMMAND/SC_CLOSE once (the command DefWindowProc
# produces for Alt+F4) and then only observes. It never kills, never calls a native game exit
# function, never retries with a stronger method and never changes settings.
$ErrorActionPreference='Stop'
$launchDir=(Resolve-Path -LiteralPath $Launch).Path
if(!$ExpectedExecutable){$ExpectedExecutable=Join-Path (Split-Path (Split-Path $PSScriptRoot -Parent) -Parent) 'enshrouded.exe'}
$expected=(Resolve-Path -LiteralPath $ExpectedExecutable).Path
$tracePath=Join-Path $launchDir $(if($DryRun){'ui-close-dryrun-'+(Get-Date -Format 'HHmmss-fff')+'.json'}else{'ui-close-request.json'})
if(!$DryRun -and (Test-Path -LiteralPath $tracePath)){throw 'A close request was already recorded for this launch; refusing a second request.'}
Add-Type -TypeDefinition @'
using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Text;
public static class EvrCloseWin32 {
    public delegate bool EnumProc(IntPtr hwnd, IntPtr lParam);
    [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc cb, IntPtr lParam);
    [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr hwnd, out uint pid);
    [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetClassNameW(IntPtr hwnd, StringBuilder s, int n);
    [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetWindowTextW(IntPtr hwnd, StringBuilder s, int n);
    [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr hwnd);
    [DllImport("user32.dll")] public static extern IntPtr GetWindow(IntPtr hwnd, uint cmd);
    [DllImport("user32.dll", EntryPoint="GetWindowLongPtrW")] public static extern IntPtr GetWindowLongPtr(IntPtr hwnd, int index);
    [DllImport("user32.dll", SetLastError=true)] public static extern bool PostMessageW(IntPtr hwnd, uint msg, IntPtr w, IntPtr l);
    [DllImport("user32.dll")] public static extern bool IsWindow(IntPtr hwnd);
    public static List<IntPtr> TopLevel(uint pid) {
        var found = new List<IntPtr>();
        EnumWindows((h, l) => { uint owner; GetWindowThreadProcessId(h, out owner); if (owner == pid) found.Add(h); return true; }, IntPtr.Zero);
        return found;
    }
    public static string Cls(IntPtr h) { var s = new StringBuilder(256); GetClassNameW(h, s, s.Capacity); return s.ToString(); }
    public static string Txt(IntPtr h) { var s = new StringBuilder(512); GetWindowTextW(h, s, s.Capacity); return s.ToString(); }
}
'@
$record=[ordered]@{launch=$launchDir;dry_run=[bool]$DryRun;status='not_started';method='PostMessage WM_SYSCOMMAND(0x0112) SC_CLOSE(0xF060) to the single verified game window; same system command DefWindowProc issues for Alt+F4';native_action_invoked=$false;force_kill=$false;retry=$false}
$process=$null
try {
    $launchRecord=Get-Content -LiteralPath (Join-Path $launchDir 'launch.json') -Raw | ConvertFrom-Json
    $process=[Diagnostics.Process]::GetProcessById([int]$launchRecord.pid)
    $heldHandle=$process.Handle
    $creation=$process.StartTime.ToUniversalTime().ToFileTimeUtc().ToString()
    $path=$process.MainModule.FileName
    $record.pid=$process.Id;$record.creation_filetime=$creation;$record.executable=$path
    if($creation -ne [string]$launchRecord.creation_filetime -or $path -ne $expected){throw 'Process identity differs from the managed launch.'}
    if(Test-Path -LiteralPath (Join-Path $launchDir 'process-exit.json')){throw 'Launch already has exit evidence.'}
    $windows=foreach($h in [EvrCloseWin32]::TopLevel([uint32]$process.Id)){
        $style=[int64][EvrCloseWin32]::GetWindowLongPtr($h,-16)
        [ordered]@{hwnd=('0x{0:x}' -f [int64]$h);class=[EvrCloseWin32]::Cls($h);title=[EvrCloseWin32]::Txt($h);visible=[EvrCloseWin32]::IsWindowVisible($h);owned=([EvrCloseWin32]::GetWindow($h,4) -ne [IntPtr]::Zero);style=('0x{0:x8}' -f $style);handle=$h}
    }
    $windows=@($windows)
    # IME/text-service helpers belong to every GUI thread; hidden driver helpers (for example
    # NVOpenGLPbuffer or temp_d3d_window_*) are not the game surface. Only one visible,
    # unowned top-level window may receive the request.
    $candidates=@($windows | Where-Object { !$_.owned -and $_.visible -and $_.class -notin @('IME','MSCTFIME UI') -and $_.class -notlike 'CicMarshalWnd*' })
    $record.windows=@($windows | ForEach-Object { $copy=[ordered]@{}; foreach($k in $_.Keys){ if($k -ne 'handle'){$copy[$k]=$_[$k]} }; $copy })
    $record.candidate_count=$candidates.Count
    if($candidates.Count -ne 1){$record.status='refused_window_not_unique';throw "Expected exactly one unowned game window, found $($candidates.Count)."}
    $target=$candidates[0]
    $record.target=[ordered]@{hwnd=$target.hwnd;class=$target.class;title=$target.title;visible=$target.visible}
    if($DryRun){$record.status='dry_run_identified'}
    else {
    $record.qpc_frequency=[Diagnostics.Stopwatch]::Frequency
    $record.request_begin_utc=(Get-Date).ToUniversalTime().ToString('o')
    $record.request_begin_qpc=[Diagnostics.Stopwatch]::GetTimestamp()
    if(![EvrCloseWin32]::IsWindow($target.handle)){$record.status='window_vanished_before_request';throw 'Window vanished before the request.'}
    $posted=[EvrCloseWin32]::PostMessageW($target.handle,0x0112,[IntPtr]0xF060,[IntPtr]::Zero)
    $record.request_end_qpc=[Diagnostics.Stopwatch]::GetTimestamp()
    $record.request_end_utc=(Get-Date).ToUniversalTime().ToString('o')
    $record.posted=$posted
    if(!$posted){$record.status='post_failed';$record.win32_error=[Runtime.InteropServices.Marshal]::GetLastWin32Error();throw 'PostMessage failed.'}
    $record.status='UI_CLOSE_SENT'
    $record | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $tracePath -Encoding utf8
    if($process.WaitForExit($WaitSeconds*1000)){
        $code=[BitConverter]::ToUInt32([BitConverter]::GetBytes([int]$process.ExitCode),0)
        $record.status='exited_after_request';$record.exit_code=$code;$record.exit_code_hex=('0x{0:x8}' -f $code);$record.exit_time=$process.ExitTime.ToString('o')
    } else {
        $record.status='still_running_after_wait'
        $record.note='No kill or second request was issued. Inspect the game state and close it through its own UI.'
    }
    }
} catch {
    if($record.status -in @('not_started','UI_CLOSE_SENT')){$record.status='error'}
    $record.error=$_.Exception.Message
} finally {
    $record.observed_end_utc=(Get-Date).ToUniversalTime().ToString('o')
    $record | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $tracePath -Encoding utf8
    if($process){$process.Dispose()}
}
Write-Output ("{0} {1}" -f $record.status,$tracePath)
if($record.status -notin @('exited_after_request','dry_run_identified')){exit 1}
