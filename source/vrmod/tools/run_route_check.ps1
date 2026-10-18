param(
    [Parameter(Mandatory)][string]$Route,  # a folder of vrmod/test_env/routes (tools/save_route.py)
    [switch]$Plain,                        # the walk only (replay.keys, head.csv): no look round, no pictures
    [switch]$NoShareLights,[switch]$DesktopRender, # left-right lighting comparisons (run_home_equipment_trial.ps1)
    [string]$TrialArgs='',                 # more switches of run_home_equipment_trial.ps1
    [int]$Minutes=0,                       # 0: what route.json says (the script ends the session itself: STOP)
    [int]$FrameMs=33,
    [string]$Save='',                      # another save backup to start from (a folder of vrmod/output/save-backups); the route's otherwise                      # Mock frames paced to this (EVR_MOCK_FRAME_MS): runs of one script walk alike; 0 = free
    [string]$Out=''
)
# Self-test along a walk the user recorded (2026-10-14, user: "I press F8 where the faults show; in the self-test, turn
# round there taking pictures"). Starts the Mock session from the route's save with the route's input script
# (hooks/key_script.hpp, EVR_KEY_SCRIPT): every key and mouse movement of the user at its XR frame, and at every spot
# a look round in steps of 30 degrees with one picture of both eyes per direction (half size, EVR_CAPTURE_HALF=1).
# The script's last step ends the session normally; run_with_sound.ps1 plays the close sound only. Then
# tools/check_route_run.py compares the walk with the user's and the two eyes of every picture
# (<game folder>/route-check/report.txt).
Set-Location -LiteralPath 'S:\SteamLibrary\steamapps\common\Enshrouded'
$pw='pwsh'
$dir=Join-Path './vrmod/test_env/routes' $Route
$info=Get-Content -LiteralPath (Join-Path $dir 'route.json') -Raw -Encoding utf8 | ConvertFrom-Json
$keys=Join-Path $dir $(if($Plain){'replay.keys'}else{'check.keys'})
$head=Join-Path $dir $(if($Plain){'head.csv'}else{'check-head.csv'})
$save="./vrmod/output/save-backups/$(if($Save){$Save}else{$info.save})"
foreach($f in @($keys,$head,$save)){if(!(Test-Path -LiteralPath $f)){throw "Missing route input: $f"}}
if($Minutes -le 0){$Minutes=if($Plain){[int]$info.replay_minutes}else{[int]$info.check.minutes}}
if(!$Out){$Out="./vrmod/output/route-$(if($Plain){'replay'}else{'check'})-$Route-$(Get-Date -Format 'yyyyMMdd-HHmmss').txt"}
if(Get-Process -Name enshrouded,EDF6 -ErrorAction SilentlyContinue){throw 'enshrouded or EDF6 is running'}
$trial='-Ranged -BowDraw -RangedAim -Shield -LiveMelee -LiveGuard -TwoHandWieldVhvr -LiveMeleeContact -LiveMeleeEarly -LiveMeleeStill -BodyIk -LiveHeldItems -LiveHeldFollow -UiAutoHide -MenuPointer -ShareVfxModels -ShareDecals -OnlineSync -NoSceneryReplay'
$trial+=" -Minutes $Minutes -LogNameCap 8000 -Hands ./vrmod/test_env/home-idle-hands-20261013.csv -Pose $head -SaveReference $save"
if($NoShareLights){$trial+=' -NoShareLights'};if($DesktopRender){$trial+=' -DesktopRender'};if($TrialArgs){$trial+=" $TrialArgs"}
"route $Route $(if($Plain){'replay'}else{'check'}) minutes $Minutes out $Out"
$env:EVR_HAND_SMOOTHING='0';$env:EVR_KEY_SCRIPT=(Resolve-Path -LiteralPath $keys).Path;$env:EVR_INPUT_RECORD='1'
if($FrameMs -gt 0){$env:EVR_MOCK_FRAME_MS="$FrameMs"}
if(!$Plain){$env:EVR_CAPTURE_HALF='1'}
try{& $pw -NoProfile -File ./vrmod/tools/handoff_helpers/run_with_sound.ps1 -Script './vrmod/tools/handoff_helpers/run_home_equipment_trial.ps1' -ScriptArgs $trial -Out $Out}
finally{$env:EVR_HAND_SMOOTHING=$null;$env:EVR_KEY_SCRIPT=$null;$env:EVR_INPUT_RECORD=$null;$env:EVR_CAPTURE_HALF=$null;$env:EVR_MOCK_FRAME_MS=$null}
$game=(Select-String -LiteralPath $Out -Pattern 'game-\d{8}-\d{6}' | Select-Object -First 1).Matches.Value
if(!$game){throw "no game folder in $Out"}
$check=@('./vrmod/tools/check_route_run.py',$Route,$game);if($Plain){$check+='--plain'}
& python @check
