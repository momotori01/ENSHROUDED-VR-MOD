param(
    [ValidateRange(1,720)][int]$Minutes=240,
    [string]$SaveReference='',                 # tests: restore one managed save snapshot before the launch (as run_play_session.ps1)
    [ValidatePattern('^([0-9a-f]{8})?$')][string]$ExpectedWorld='', # tests: verify the world the game opened by itself
    [switch]$Menu,                             # start at the game's own menu (join or host a session there); otherwise the game continues the last world by itself
    [switch]$Proof,                            # the older way: the world Scene/View from a proof probe of the launch and a second, freshly built hook library (needs the build tools)
    [switch]$Attach,                           # tests: the hooks are put into the running game by its process alone (tools/run_online_flat.py --attach), as for a game started from Steam
    [switch]$CloseAtEnd,                       # tests: close the game the ordinary way when the session ends (default: the game stays open)
    [switch]$NoFingerCurl,
    [ValidateRange(1000,20000)][int]$LogNameCap=3000
)
# The mod without VR (2026-10-14, user: "VR can be switched on and off", "with the VR display off the other players'
# movements are still seen as long as the mod is installed"). The game runs and draws as it does by itself, in its own
# window; the hooks only drive the other players' VR bodies (tools/run_online_flat.py, hooks EVR_OnlineFlat).
#   launch_diagnostics (no VR preparation) -> EVR_OnlineFlat until -Minutes, <online-flat dir>/stop.request or the
#   game's end. The hooks find the world themselves and wait for one, so with -Menu nothing else has to be done after
#   joining a session. Nothing is built here and no second hook library is needed (unless -Proof).
# With VR: tools/run_play_session.ps1 -OnlineSync.
$ErrorActionPreference='Stop'
Set-Location -LiteralPath (Split-Path (Split-Path $PSScriptRoot -Parent) -Parent)
$pw=(Get-Process -Id $PID).Path
$py='S:/stable/Python31011/python.exe'
if(Get-Process enshrouded -ErrorAction SilentlyContinue){throw 'The game is already running; close it first.'}
# The reference through Steam's own storage (2026-10-20: restore_scenery_backup.ps1 copies behind Steam's back - 1.0 cannot read
# such a save, and it had put back an old user_settings.json; tools/steam_save_write.py writes the .ksd files only).
if($SaveReference){& python -I (Join-Path $PSScriptRoot 'steam_save_write.py') $SaveReference;if($LASTEXITCODE -ne 0){throw 'The save reference could not be written through Steam (steam_save_write.py).'}}
$before=@(Get-ChildItem vrmod/output -Directory -Filter 'game-*' | ForEach-Object Name)
$argList="-NoProfile -File ./vrmod/launch_diagnostics.ps1 -VrUpscaler Keep -VrSharpness Keep -InteractiveWindow -VulkanValidation None -LogNameCap $LogNameCap$(if(!$Menu){' -DirectPlay'})"
Start-Process -FilePath $pw -ArgumentList $argList -WindowStyle Hidden | Out-Null
$deadline=(Get-Date).AddSeconds(90);$g=$null
do{Start-Sleep -Seconds 2;$g=Get-ChildItem vrmod/output -Directory -Filter 'game-*' | Where-Object { $before -notcontains $_.Name -and (Test-Path "$($_.FullName)/launch.json") } | Sort-Object Name | Select-Object -Last 1}while(-not $g -and (Get-Date) -lt $deadline)
if(-not $g){throw 'launch record not created'}
$l=Get-Content "$($g.FullName)/launch.json" -Raw | ConvertFrom-Json;$p=Get-Process -Id $l.pid
if($p.Path -ne (Resolve-Path 'enshrouded.exe').Path -or $p.StartTime.ToUniversalTime().ToFileTimeUtc().ToString() -ne $l.creation_filetime){throw 'launched process identity mismatch'}
"== $($g.Name) pid $($l.pid) (no VR)"
try{
    if(!$Menu){
        if($SaveReference){& $py vrmod/tools/check_scenery_save.py $g.FullName $SaveReference;if($LASTEXITCODE -ne 0){throw 'Launch save snapshot differs from comparison reference.'}}
        $w=22-((Get-Date)-$p.StartTime).TotalSeconds;if($w -gt 0){Start-Sleep -Seconds ([int][Math]::Ceiling($w))}
        if($ExpectedWorld){& $py vrmod/tools/check_play_world.py $g.FullName --expected $ExpectedWorld;if($LASTEXITCODE -ne 0){throw 'Native direct-play selected a different world.'}}
    }else{$w=15-((Get-Date)-$p.StartTime).TotalSeconds;if($w -gt 0){Start-Sleep -Seconds ([int][Math]::Ceiling($w))}}
    $flat=if($Attach){@('--attach','--minutes',"$Minutes")}else{@('--launch',$g.FullName,'--minutes',"$Minutes")}
    if($Proof){
        if($Menu){throw '-Proof needs a world that is open at the start: not with -Menu.'}
        $wait=60-((Get-Date)-$p.StartTime).TotalSeconds;if($wait -gt 0){Start-Sleep -Seconds ([int]$wait)}
        & $py vrmod/tools/run_render_resource_probe.py --launch $g.FullName --world-view-features *>&1 | Out-File "$($g.FullName)/online-flat-proof.log" -Encoding utf8
        if($LASTEXITCODE -ne 0){throw 'world Scene/View proof failed (see online-flat-proof.log)'}
        $u=(Get-ChildItem $g.FullName -Directory -Filter 'view-resource-probe-*' | Sort-Object Name | Select-Object -Last 1).FullName
        & $pw -NoProfile -File ./vrmod/build.ps1 -SkipTests *>&1 | Out-File "$($g.FullName)/online-flat-build.log" -Encoding utf8
        if($LASTEXITCODE -ne 0){throw 'fresh DLL build failed (see online-flat-build.log)'}
        $flat+=@('--proof',$u)
    }
    $curlProfile='vrmod/output/grip-preview-20261004-1530'
    if(-not $NoFingerCurl -and (Test-Path "$curlProfile/hand-pose.bin")){$flat+=@('--hand-curl',$curlProfile)}
    "online session without VR for up to $Minutes min$(if($Menu){': join or host a session in the game, the mod waits for the world'}); create <online-flat dir>/stop.request to end early"
    & $py vrmod/tools/run_online_flat.py @flat *>&1 | Out-File "$($g.FullName)/online-flat-probe.log" -Encoding utf8
    "ONLINE_FLAT_EXIT=$LASTEXITCODE"
}finally{
    if($CloseAtEnd){
        & $pw -NoProfile -File ./vrmod/tools/request_game_close.ps1 -Launch $g.FullName -DryRun *>&1 | Select-Object -Last 1
        & $pw -NoProfile -File ./vrmod/tools/request_game_close.ps1 -Launch $g.FullName -WaitSeconds 180 *>&1 | Select-Object -Last 1
        if($p.HasExited -and (Test-Path 'enshrouded.log')){Copy-Item -LiteralPath 'enshrouded.log' -Destination "$($g.FullName)/completed-game.log"}
    }
}
