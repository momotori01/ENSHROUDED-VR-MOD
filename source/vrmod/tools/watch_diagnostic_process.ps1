param(
    [Parameter(Mandatory=$true)][string]$LaunchJson,
    [Parameter(Mandatory=$true)][string]$ExpectedExecutable,
    [ValidateRange(1,720)][int]$TimeoutMinutes=720
)
# Read-only process lifetime observation. Never terminates a process or changes a setting.
$ErrorActionPreference='Stop'
$launchPath=(Resolve-Path -LiteralPath $LaunchJson).Path
$out=Split-Path $launchPath -Parent
$reportPath=Join-Path $out 'process-exit.json'
if(Test-Path -LiteralPath $reportPath){throw 'Existing lifetime evidence must not be overwritten.'}
$record=@{observed_start=(Get-Date).ToString('o');status='not_attached';limitation='Exit code alone does not prove native resource cleanup or identify who requested termination.'}
$taskProcess=$null
try {
    $launch=Get-Content -LiteralPath $launchPath -Raw | ConvertFrom-Json
    $expected=(Resolve-Path -LiteralPath $ExpectedExecutable).Path
    $taskProcess=[Diagnostics.Process]::GetProcessById([int]$launch.pid)
    # Keep the native handle open before checking identity; PID reuse cannot change it.
    $heldHandle=$taskProcess.Handle
    $creation=$taskProcess.StartTime.ToUniversalTime().ToFileTimeUtc().ToString()
    if($creation -ne [string]$launch.creation_filetime -or $taskProcess.MainModule.FileName -ne $expected){throw 'Process identity differs from the managed launch.'}
    $record.pid=$taskProcess.Id
    $record.creation_filetime=$creation
    $record.executable=$expected
    $record.status='observing'
    $record | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $out 'process-watch.json')
    $deadline=(Get-Date).AddMinutes($TimeoutMinutes)
    while(!$taskProcess.WaitForExit(1000)){
        if((Get-Date) -ge $deadline){$record.status='observation_timeout';break}
    }
    if($taskProcess.HasExited){
        $signedCode=[int]$taskProcess.ExitCode
        $code=[BitConverter]::ToUInt32([BitConverter]::GetBytes($signedCode),0)
        $record.status='exited'
        $record.exit_code=$code
        $record.exit_code_hex=('0x{0:x8}' -f $code)
        $record.exit_time=$taskProcess.ExitTime.ToString('o')
    }
} catch {
    $record.error=$_.Exception.Message
} finally {
    $record.observed_end=(Get-Date).ToString('o')
    $record | ConvertTo-Json | Set-Content -LiteralPath $reportPath
    if($taskProcess){$taskProcess.Dispose()}
}
