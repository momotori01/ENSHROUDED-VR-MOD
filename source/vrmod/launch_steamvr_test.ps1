param([switch]$Virtual)
$ErrorActionPreference='Stop'
if(Get-Process vrserver,vrcompositor -ErrorAction SilentlyContinue){throw 'A SteamVR session is already active; refusing to replace it.'}
$baseline = & (Join-Path $PSScriptRoot 'tools/host_vr_settings.ps1') -Action Backup
if ($LASTEXITCODE) { throw 'Host configuration backup failed' }
$started = Get-Date
& (Join-Path $PSScriptRoot 'setup_steamvr_test.ps1') -Virtual:$Virtual
$test=Join-Path $PSScriptRoot 'test_env/steamvr'
$runtime='C:\Program Files (x86)\Steam\steamapps\common\SteamVR'
$driverName = if($Virtual){'Virtual'}else{'Null'}
$previous = @{}
foreach ($key in @('VR_PATHREG_OVERRIDE','VR_CONFIG_PATH','VR_LOG_PATH','XR_RUNTIME_JSON')) { $previous[$key] = [Environment]::GetEnvironmentVariable($key, 'Process') }
try {
    $env:VR_PATHREG_OVERRIDE=Join-Path $test 'openvrpaths.vrpath'
    $env:VR_CONFIG_PATH=Join-Path $test 'config'
    $env:VR_LOG_PATH=Join-Path $test 'logs'
    $env:XR_RUNTIME_JSON=Join-Path $runtime 'steamxr_win64.json'
    $process=Start-Process -FilePath (Join-Path $runtime 'bin/win64/vrstartup.exe') -WindowStyle Hidden -PassThru
    @{baseline=$baseline;started=$started.ToString('o');virtual=[bool]$Virtual;
      processes=@(@{pid=$process.Id;path=$process.Path;startUtc=$process.StartTime.ToUniversalTime().ToString('o')})} |
        ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $test 'active-session.json')
    $deadline = (Get-Date).AddSeconds(25)
    do {
        Start-Sleep -Milliseconds 250
        $server = Get-Process vrserver -ErrorAction SilentlyContinue
    } while (!$server -and (Get-Date) -lt $deadline)
    if (!$server) { throw 'Isolated SteamVR startup timed out' }
    Start-Sleep -Seconds 2
    $owned = @(Get-Process vrserver,vrmonitor,vrcompositor,vrstartup,steamvr_room_setup -ErrorAction SilentlyContinue |
        Where-Object { $_.StartTime -ge $started.AddSeconds(-1) } |
        ForEach-Object { @{pid=$_.Id;path=$_.Path;startUtc=$_.StartTime.ToUniversalTime().ToString('o')} })
    @{baseline=$baseline;started=$started.ToString('o');virtual=[bool]$Virtual;processes=$owned} | ConvertTo-Json -Depth 6 |
        Set-Content -LiteralPath (Join-Path $test 'active-session.json')
    Write-Output "Started isolated SteamVR $driverName test, PID $($server.Id); baseline $baseline"
} catch {
    $launchFailure = $_
    $owned = @(Get-Process vrserver,vrmonitor,vrcompositor,vrstartup,steamvr_room_setup -ErrorAction SilentlyContinue |
        Where-Object { $_.StartTime -ge $started.AddSeconds(-1) } |
        ForEach-Object { @{pid=$_.Id;path=$_.Path;startUtc=$_.StartTime.ToUniversalTime().ToString('o')} })
    @{baseline=$baseline;started=$started.ToString('o');virtual=[bool]$Virtual;processes=$owned} | ConvertTo-Json -Depth 6 |
        Set-Content -LiteralPath (Join-Path $test 'active-session.json')
    & (Join-Path $PSScriptRoot 'switch_vr.ps1') -Mode Physical
    throw $launchFailure
} finally {
    foreach ($key in $previous.Keys) { [Environment]::SetEnvironmentVariable($key, $previous[$key], 'Process') }
}
