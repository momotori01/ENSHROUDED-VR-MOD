param([Parameter(Mandatory)][string]$Launch,[Parameter(Mandatory)][string]$Probe,[Parameter(Mandatory)][string]$Publisher,[ValidateSet('repeat','post-impact-cancel','terminal-retire','terminal-owner','active-owner','active-interrupted')][string]$Scenario='repeat',[switch]$ActorOwner,[switch]$ActorAdmission)
$ErrorActionPreference='Stop'
if($ActorAdmission){$ActorOwner=$true}
if($ActorOwner -and $Scenario -notin @('terminal-retire','terminal-owner','active-owner','active-interrupted')){throw 'Actor owner evidence requires terminal-retire.'}
if(!(Test-Path (Join-Path $Launch 'process-exit.json'))){throw 'Audit after normal game exit only.'}
$python='S:/stable/Python31011/python.exe'
$auditResults=[System.Collections.Generic.List[object]]::new()
function Invoke-PythonAudit([string]$Name,[string[]]$Arguments){
 $log=Join-Path $Probe "$Name-audit.log"
 try {
  & $python @Arguments *> $log
  $code=$LASTEXITCODE
  $auditResults.Add([pscustomobject]@{name=$Name;status=$(if($code -eq 0){'PASS'}else{'FAIL'});exit_code=$code;log=$log})
 } catch {
  $_ | Out-String | Add-Content -LiteralPath $log
  $auditResults.Add([pscustomobject]@{name=$Name;status='FAIL';error=$_.Exception.Message;log=$log})
 }
}
# Retain every independent result, including when the publisher missed its deadline.
# Dependency-aware checkers reject absent/failed prerequisite reports themselves.
if($Scenario -in @('active-owner','active-interrupted')){$extra=if($Scenario -eq 'active-interrupted'){@('--interrupted-actor')}else{@()};Invoke-PythonAudit 'epoch-active-owner' (@('vrmod/tools/check_active_contact_owner.py',$Probe,'--negative-tests')+$extra)}elseif($Scenario -eq 'terminal-owner'){Invoke-PythonAudit 'epoch-joined' @('vrmod/tools/check_joined_contact_owner.py',$Probe,'--negative-tests')}elseif($Scenario -eq 'terminal-retire'){Invoke-PythonAudit 'epoch-terminal' @('vrmod/tools/check_terminal_contact.py',$Publisher,$Probe,'--negative-tests')}else{Invoke-PythonAudit 'epoch-strict' @('vrmod/tools/check_combat_epoch_strict.py',$Publisher,$Probe,'--negative-tests','--scenario',$Scenario)}
if($ActorOwner -and $Scenario -notin @('terminal-owner','active-owner','active-interrupted')){$actorArgs=@('vrmod/tools/check_actor_owner_exit.py',$Publisher,$Probe,'--negative-tests');if($ActorAdmission){$actorArgs+='--admission-stop'};Invoke-PythonAudit 'actor-owner' $actorArgs}
if($Scenario -eq 'active-interrupted'){Invoke-PythonAudit 'attack-exit-timing' @('vrmod/tools/check_attack_exit_request.py',$Probe,'--negative-tests')}
Invoke-PythonAudit 'epoch-publisher' @('vrmod/tools/check_world_lifetimes.py',$Publisher,'--mode','epoch','--negative-tests')
if($Scenario -notin @('active-owner','active-interrupted')){
$renderArgs=@('vrmod/tools/check_live_native_xr.py',$Probe,'--pose-expectation','fixed-root-motion')
if($Scenario -eq 'terminal-retire'){$renderArgs+='--terminal-contact-stop'}
if($Scenario -eq 'terminal-owner'){$renderArgs+='--joined-contact-stop'}
Invoke-PythonAudit 'epoch-render' $renderArgs
$baseTools=if($Scenario -eq 'repeat'){@('check_motion_hands','check_controller_swing_speed','check_contact_lifetimes','check_contact_recycling','check_contact_pool')}else{@('check_motion_hands')}
foreach($tool in $baseTools){
 Invoke-PythonAudit $tool @("vrmod/tools/$tool.py",$Probe,'--negative-tests')
}
$poseExpectation=if($Scenario -in @('repeat','terminal-retire','terminal-owner','active-owner','active-interrupted')){'melee-repeat'}else{'calibrated-release'}
Invoke-PythonAudit 'epoch-weapon' @('vrmod/tools/check_motion_visual.py',$Probe,'--negative-tests','--pose-expectation',$poseExpectation)
}else{Invoke-PythonAudit 'epoch-images' @('vrmod/tools/check_native_xr_images.py',(Join-Path $Launch 'native-xr'))}
if($Scenario -eq 'post-impact-cancel'){
 Invoke-PythonAudit 'epoch-offhand' @('vrmod/tools/check_offhand_release.py',$Probe,'--negative-tests')
 Invoke-PythonAudit 'epoch-cancel-retirement' @('vrmod/tools/check_cancel_contact_retirement.py',$Probe,'--negative-tests')
}
$uiLog=Join-Path $Probe 'epoch-ui-audit.log'
try {
 $uiExtra=@{};if($Scenario -in @('active-owner','active-interrupted')){$uiExtra['ActiveContactOwnerProbe']=$Probe}
 & ./vrmod/tools/run_ui_image_checks.ps1 -Launch $Launch -RequireCleanup @uiExtra *> $uiLog
 $ui=Get-Content -LiteralPath (Join-Path $Launch 'native-xr/ui-xr-checks.json') -Raw | ConvertFrom-Json
 if(!$ui.checks -or @($ui.checks.PSObject.Properties | Where-Object Value -ne 'PASS').Count){throw 'UI checker did not report all PASS.'}
 $auditResults.Add([pscustomobject]@{name='epoch-ui';status='PASS';log=$uiLog})
} catch {
 $_ | Out-String | Add-Content -LiteralPath $uiLog
 $auditResults.Add([pscustomobject]@{name='epoch-ui';status='FAIL';error=$_.Exception.Message;log=$uiLog})
}
Invoke-PythonAudit 'epoch-save' @('vrmod/tools/audit_save_backup.py',$Launch)
$failed=@($auditResults | Where-Object status -eq 'FAIL')
$scope=if($Scenario -eq 'active-interrupted'){'Actual active Actor exit after owned Impact end and late suppression; genuine World/input/Scene/GPU retirement. Outstanding physics-scope exit, reentry and enemy HP remain unproven.'}elseif($Scenario -eq 'active-owner'){'Active stereo/Scene/GPU retirement joined with completed-action World and native input owner shutdown; attack-in-flight/reentry remain unproven.'}elseif($Scenario -eq 'terminal-owner'){'Finite completed-action native World retirement joined with native input owner handoff; active attack/renderer exit and re-entry unproven.'}elseif($Scenario -eq 'terminal-retire'){'Finite completed-action values retained until actual server World end; 600 pairs/input drain. Active-attack exit, backend handoff, re-entry and enemy HP remain unproven.'}else{'Finite strict epoch trial and native XR/contact/UI regression; no teardown race or enemy HP claim.'}
$report=[ordered]@{status=$(if($failed.Count){'FAIL'}else{'PASS'});checks=$auditResults.ToArray();scenario=$Scenario;scope=$scope}
$report | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $Probe 'combat-epoch-trial-checks.json') -Encoding utf8
$auditResults | Select-Object name,status | Format-Table
if($failed.Count){throw "$($failed.Count) regression stages failed; all reports and raw logs retained."}
"PASS $scope"
