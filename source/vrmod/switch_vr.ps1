param([ValidateSet('Virtual','Null','Physical','Status')][string]$Mode = 'Status')
$ErrorActionPreference = 'Stop'
$test = Join-Path $PSScriptRoot 'test_env/steamvr'
$sessionFile = Join-Path $test 'active-session.json'
if ($Mode -in @('Virtual','Null')) {
    & (Join-Path $PSScriptRoot 'launch_steamvr_test.ps1') -Virtual:($Mode -eq 'Virtual')
    exit $LASTEXITCODE
}
if ($Mode -eq 'Status') {
    Write-Output "Isolated session recorded: $(Test-Path -LiteralPath $sessionFile)"
    Get-Process vrserver,vrcompositor -ErrorAction SilentlyContinue | Select-Object ProcessName,Id,StartTime
    Get-ItemProperty 'HKLM:\SOFTWARE\Khronos\OpenXR\1' -Name ActiveRuntime | Select-Object ActiveRuntime
    exit 0
}
if (!(Test-Path -LiteralPath $sessionFile)) {
    Write-Output 'No managed test session. Host registration was not changed; normal SteamVR launch uses physical configuration.'
    exit 0
}
$session = Get-Content -LiteralPath $sessionFile -Raw | ConvertFrom-Json
function Get-OwnedProcess($record) {
    $p = Get-Process -Id $record.pid -ErrorAction SilentlyContinue
    # PowerShell 7.5 ConvertFrom-Json may deserialize ISO strings as DateTime.
    # Compare UTC ticks, never an ISO string against a DateTime object's display text.
    if ($p -and $p.Path -eq $record.path -and $p.StartTime.ToUniversalTime().Ticks -eq ([datetime]$record.startUtc).ToUniversalTime().Ticks) { $p }
}
if ($session.virtual -and @($session.processes | ForEach-Object { Get-OwnedProcess $_ }).Count) {
    # Driver consumes this test-local request and emits the documented OpenVR quit event.
    'quit' | Set-Content -LiteralPath (Join-Path $test 'shutdown.request')
    $deadline = (Get-Date).AddSeconds(12)
    do {
        $remaining = @($session.processes | ForEach-Object { Get-OwnedProcess $_ })
        if (!$remaining) { break }
        Start-Sleep -Milliseconds 250
    } while ((Get-Date) -lt $deadline)
}
# If the test runtime is unresponsive, terminate only the exact test-owned processes.
# Never stop an existing physical SteamVR session or a process whose PID was reused.
foreach ($record in $session.processes) {
    $p = Get-OwnedProcess $record
    if ($p) { Write-Output "Stopping remaining test process $($p.ProcessName) $($p.Id)"; $p | Stop-Process }
}
$remaining = @($session.processes | ForEach-Object { Get-OwnedProcess $_ })
foreach ($p in $remaining) { if (!$p.WaitForExit(10000)) { throw "Test process $($p.Id) did not exit" } }
$remaining = @($session.processes | ForEach-Object { Get-OwnedProcess $_ })
if ($remaining.Count) { throw 'Managed test processes remain active; physical switch is incomplete.' }
& (Join-Path $PSScriptRoot 'tools/host_vr_settings.ps1') -Action Verify -Snapshot $session.baseline
if ($LASTEXITCODE) { throw 'Host setting verification failed' }
Move-Item -LiteralPath $sessionFile -Destination (Join-Path $test ('completed-session-'+(Get-Date -Format 'yyyyMMdd-HHmmss-fff')+'.json'))
Write-Output 'Physical configuration verified. Start SteamVR normally when ready; no registry or user configuration restore was needed.'
