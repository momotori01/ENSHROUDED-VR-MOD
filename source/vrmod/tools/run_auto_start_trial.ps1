param(
    [ValidateSet('Flat','MockVr','Off','Installed')][string]$Mode='Flat',   # Installed: the settings file as it is in EnshroudedVR
    [ValidateRange(20,3600)][int]$Seconds=120,       # how long the game runs before it is closed the ordinary way
    [switch]$Menu,                                   # the game starts at its own menu (as from Steam); default --play: straight into the last world
    [string]$Hands='./vrmod/test_env/home-combined-weapons-leftgrip-20261011.csv',   # MockVr: the hand/controller script
    [string]$Pose='./vrmod/test_env/world-static-pose-1280x1440.csv',               # MockVr: the head pose
    [string]$SaveReference='',                       # restore this managed save snapshot first
    [ValidateRange(0.25,2.0)][double]$RenderScale=0.8,
    [string[]]$Environment=@(),                      # more NAME=value lines for [Environment] of the test's settings file
    [switch]$FakeBody,                               # Flat: the in-process self test - a made-up body with waving arms on the own character
    [switch]$NoRuntime,                              # MockVr: an OpenXR runtime that is not there (a PC without a headset's runtime)
    [string]$PlateDump=''                            # MockVr: "n,m" - the plate's image of the n-th screen frame and the m-th world frame with the plate, read back (EVR_SCREEN_XR_DUMP)
)
# A start of the game the way a player's start goes (user, 2026-10-14: "started from the Steam launcher as usual, the
# game starts with the settings of last time"): enshrouded.exe is started with nothing of the development launch in
# its environment, and the loader (dinput8.dll in the game's folder, loader/loader.cpp) does the rest from a settings
# file. The file here is the test's own (EVR_AUTO_INI), so the player's EnshroudedVR.ini stays as it is.
#   Flat    VR=0: the other players' VR bodies without VR.
#   MockVr  VR=1 with the tests' OpenXR runtime instead of a headset (named in the file's [Environment]).
#   Off     VR=0, Sync=0: the loader does nothing.
# The saves are backed up first, as for every direct start. After -Seconds the game's window is asked to close.
$ErrorActionPreference='Stop'
$mod=Split-Path $PSScriptRoot -Parent;$game=Split-Path $mod -Parent
Set-Location -LiteralPath $game
if(Get-Process enshrouded -ErrorAction SilentlyContinue){throw 'The game is already running; close it first.'}
if(!(Test-Path -LiteralPath 'dinput8.dll') -or !(Test-Path -LiteralPath 'EnshroudedVR/bin/evr_hooks.dll')){throw 'The package is not in the game folder (tools/pack_release.ps1 -Install).'}
if($SaveReference){& (Join-Path $PSScriptRoot 'restore_scenery_backup.ps1') -Backup $SaveReference}
$source=$null
foreach($candidate in (Get-ChildItem -LiteralPath (Join-Path $mod 'output/save-backups') -Directory | Sort-Object Name -Descending)){
    $recordPath=Join-Path $candidate.FullName 'backup.json';if(!(Test-Path -LiteralPath $recordPath)){continue}
    $record=Get-Content -LiteralPath $recordPath -Raw | ConvertFrom-Json
    if($record.verified -and $record.source -match '[\\/]userdata[\\/][0-9]+[\\/]1203620[\\/]remote$'){$source=$record.source;break}
}
if(!$source){throw 'No earlier verified save backup names the save folder.'}
$backup=& (Join-Path $PSScriptRoot 'backup_game_saves.ps1') -Source $source -PassThru
if(!$backup.verified){throw 'The save backup did not complete.'}
"save backup: $($backup.path)"
$out=Join-Path $mod ('output/auto-start-'+(Get-Date -Format 'yyyyMMdd-HHmmss'));New-Item -ItemType Directory -Path $out | Out-Null
$ini=$null
if($Mode -ne 'Installed'){
    $number={param($v) $v.ToString([Globalization.CultureInfo]::InvariantCulture)}
    $lines=@('; a test''s settings (tools/run_auto_start_trial.ps1)','[Mode]',"VR=$(if($Mode -eq 'MockVr'){1}else{0})",'[Online]',"Sync=$(if($Mode -eq 'Off'){0}else{1})",'[Picture]',"RenderScale=$(& $number $RenderScale)",'Dlss=Quality','Sharpness=0.2','[Log]','Keep=0','[Environment]')
    if($Mode -eq 'MockVr'){
        & 'S:/stable/Python31011/python.exe' vrmod/tools/validate_mock_hands.py $Hands;if($LASTEXITCODE -ne 0){throw 'The hand script failed its preflight.'}
        Copy-Item -LiteralPath $Hands -Destination (Join-Path $out 'mock-hands.csv');Copy-Item -LiteralPath $Pose -Destination (Join-Path $out 'mock-pose.csv')
        $lines+=@("XR_RUNTIME_JSON=$(Join-Path $mod 'build\bin\Release\evr_mock_runtime.json')","EVR_MOCK_HAND_SEQUENCE=$(Join-Path $out 'mock-hands.csv')","EVR_POSE_SEQUENCE=$(Join-Path $out 'mock-pose.csv')",
            'EVR_MOCK_LOG=%LOG%\mock.jsonl','EVR_GRIP_TRIM=','EVR_HAND_SMOOTHING=0')
    }
    if($FakeBody){$lines+=@('EVR_SYNC_SELF=2','EVR_SYNC_FAKE_BODY=1')}
    if($NoRuntime){$lines+='XR_RUNTIME_JSON=C:/evr-no-such-runtime/none.json'}
    if($PlateDump){$lines+="EVR_SCREEN_XR_DUMP=$(($PlateDump -split '[ ,]+') -join ',')"}
    $lines+=$Environment
    $ini=Join-Path $out 'EnshroudedVR.ini';[IO.File]::WriteAllText($ini,(($lines -join "`r`n")+"`r`n"),(New-Object Text.UTF8Encoding $false))
}
$logs=Join-Path $game 'EnshroudedVR/logs'
$before=@();if(Test-Path -LiteralPath $logs){$before=@(Get-ChildItem -LiteralPath $logs -Directory | ForEach-Object Name)}
$previous=@{};foreach($key in @('EVR_AUTO_INI','SteamAppId','SteamGameId','EVR_LAYER_OUTPUT')){$previous[$key]=[Environment]::GetEnvironmentVariable($key,'Process')}
try{
    # The game alone, as Steam starts it: only Steam's own two variables, and where the test's settings are.
    $env:SteamAppId='1203620';$env:SteamGameId='1203620';Remove-Item Env:EVR_LAYER_OUTPUT -ErrorAction SilentlyContinue
    if($ini){$env:EVR_AUTO_INI=$ini}else{Remove-Item Env:EVR_AUTO_INI -ErrorAction SilentlyContinue}
    $arguments=@{};if(!$Menu){$arguments.ArgumentList='--play'}
    $process=Start-Process -FilePath (Join-Path $game 'enshrouded.exe') -WorkingDirectory $game -WindowStyle Normal -PassThru @arguments
}finally{foreach($key in $previous.Keys){[Environment]::SetEnvironmentVariable($key,$previous[$key],'Process')}}
@{pid=$process.Id;output=$out;mode="auto_start_$Mode";creation_filetime=$process.StartTime.ToUniversalTime().ToFileTimeUtc().ToString();started=(Get-Date).ToString('o')} | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $out 'launch.json')
"== started pid $($process.Id), mode $Mode, test folder $out"
$deadline=(Get-Date).AddSeconds($Seconds)
while((Get-Date) -lt $deadline -and !$process.HasExited -and !(Test-Path -LiteralPath (Join-Path $out 'stop.request'))){Start-Sleep -Seconds 1}
if($process.HasExited){"the game ended by itself, exit code $($process.ExitCode)"}
else{
    $pw=(Get-Process -Id $PID).Path
    & $pw -NoProfile -File (Join-Path $PSScriptRoot 'request_game_close.ps1') -Launch $out -WaitSeconds 180 *>&1 | Select-Object -Last 1
}
if($process.HasExited -and (Test-Path -LiteralPath 'enshrouded.log')){Copy-Item -LiteralPath 'enshrouded.log' -Destination (Join-Path $out 'completed-game.log')}
$new=@();if(Test-Path -LiteralPath $logs){$new=@(Get-ChildItem -LiteralPath $logs -Directory | Where-Object { $before -notcontains $_.Name } | ForEach-Object FullName)}
"loader folder: $(if($new){$new -join ', '}else{'(none - the loader made no folder for this start)'})"
"exit code: $(if($process.HasExited){$process.ExitCode}else{'still running'})"
