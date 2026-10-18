[CmdletBinding()]
param([ValidateRange(60,420)][int]$Seconds=60,[switch]$Reentry,[switch]$EpochShadow,[switch]$NormalExit,[switch]$EpochBroker,[switch]$MotionEpochShadow,[switch]$Preclose,[switch]$PrecloseAdmission,[switch]$ExitProgress,[switch]$InputBackendExit,[ValidateSet('repeat','post-impact-cancel')][string]$MotionScenario='repeat')
$ErrorActionPreference='Stop'
$modRoot=Split-Path $PSScriptRoot -Parent
if(Get-Process enshrouded -ErrorAction SilentlyContinue){throw 'Game already running; no duplicate launch.'}
if($Reentry){
 if($MotionEpochShadow -or $InputBackendExit -or $Seconds -lt 300){throw 'Reentry requires isolated 300..420 seconds without XR/input-owner trial.'}
 $EpochShadow=$true;$NormalExit=$true;$EpochBroker=$true;$Preclose=$true;$PrecloseAdmission=$true;$ExitProgress=$true
}
if($InputBackendExit -and (!$EpochBroker -or !$NormalExit -or $MotionEpochShadow)){throw 'Input backend exit requires isolated broker/normal-exit.'}
if($ExitProgress -and $MotionEpochShadow){throw 'Exit progress and XR trial cannot share the render hook.'}
if($MotionScenario -ne 'repeat' -and !$MotionEpochShadow){throw 'Motion scenario needs the explicit XR epoch trial.'}
if($MotionEpochShadow){
 if($Seconds -lt 300){throw 'Motion epoch trial requires explicit 300..420 seconds.'}
 $EpochShadow=$true;$NormalExit=$true;$EpochBroker=$true
 & (Join-Path $modRoot 'launch_diagnostics.ps1') -PrepareMock -LiveStereo -DirectPlay -RenderCpuArenaMiB 1024 -VulkanValidation Sync -HandsOnly -MotionHands -UiQuad -VfxTransparentShader -MockPoseSequence (Join-Path $modRoot 'test_env/world-static-pose.csv') -MockHandSequence (Join-Path $modRoot $(if($MotionScenario -eq 'post-impact-cancel'){'test_env/two-hand-release-445.csv'}else{'test_env/melee-repeat-hands.csv'}))
}else{
 if(!$Reentry -and $Seconds -gt 180){throw 'Ordinary World observer duration remains 60..180 seconds.'}
 & (Join-Path $modRoot 'launch_diagnostics.ps1') -DirectPlay -VulkanValidation Sync -InteractiveWindow:$Reentry
}
# A PowerShell script need not reset LASTEXITCODE. Identify the real launch instead.
$gameProcess=@(Get-Process enshrouded -ErrorAction Stop)
if($gameProcess.Count -ne 1){throw 'Expected one managed game process.'}
$launchCandidates=@(Get-ChildItem (Join-Path $modRoot 'output') -Directory -Filter 'game-20*' | Where-Object {$_.Name -match '^game-[0-9]{8}-[0-9]{6}$'} | Sort-Object Name -Descending | Select-Object -First 3)
$selectedLaunch=$null
foreach($candidate in $launchCandidates){
 $recordPath=Join-Path $candidate.FullName 'launch.json'
 if(!(Test-Path -LiteralPath $recordPath)){continue}
 $record=Get-Content -LiteralPath $recordPath -Raw | ConvertFrom-Json
 if($record.pid -eq $gameProcess[0].Id){$selectedLaunch=$candidate;break}
}
if(!$selectedLaunch){throw 'No exact managed PID launch record; refusing injection.'}
$extra=@()
if($Reentry){$extra+='--reentry'}
if($MotionEpochShadow){$extra+='--xr-trial'}
if($EpochShadow){$extra+='--epoch-shadow'}
if($NormalExit){$extra+='--normal-exit'}
if($EpochBroker){$extra+='--epoch-broker'}
if($PrecloseAdmission){$Preclose=$true;$extra+='--preclose-admission'}
if($InputBackendExit){$extra+='--input-backend-exit'}
if($ExitProgress){$Preclose=$true;$extra+='--exit-progress'}
if($Preclose){$extra+='--preclose'}
& 'S:/stable/Python31011/python.exe' (Join-Path $PSScriptRoot 'run_world_lifetimes.py') --launch $selectedLaunch.FullName --seconds $Seconds --startup @extra
if($LASTEXITCODE -ne 0){throw 'World lifecycle probe failed; preserve logs and close game through normal UI.'}
