param([Parameter(Mandatory)][string]$Launch,[Parameter(Mandatory)][string]$Probe)
$ErrorActionPreference='Stop'
if(!(Test-Path -LiteralPath (Join-Path $Launch 'process-exit.json'))){throw 'Wait for normal game exit before final audits.'}
$results=[System.Collections.Generic.List[object]]::new()
function Audit-Python([string]$Name,[string[]]$Arguments){
 $log=Join-Path $Probe "$Name-audit.log"
 & 'S:/stable/Python31011/python.exe' @Arguments *> $log
 $results.Add([pscustomobject]@{name=$Name;status=$(if($LASTEXITCODE -eq 0){'PASS'}else{'FAIL'});log=$log})
}
Audit-Python 'vertical-native' @('vrmod/tools/check_vertical_stick.py',$Probe,'--negative-tests')
Audit-Python 'vertical-render' @('vrmod/tools/check_live_native_xr.py',$Probe,'--pose-expectation','fixed-root-motion')
Audit-Python 'vertical-hands' @('vrmod/tools/check_motion_hands.py',$Probe,'--negative-tests')
Audit-Python 'vertical-weapon' @('vrmod/tools/check_motion_visual.py',$Probe,'--negative-tests','--pose-expectation','vertical-stick')
Audit-Python 'vertical-save' @('vrmod/tools/audit_save_backup.py',$Launch)
$uiLog=Join-Path $Probe 'vertical-ui-audit.log'
try {
 & ./vrmod/tools/run_ui_image_checks.ps1 -Launch $Launch -RequireCleanup *> $uiLog
 $ui=Get-Content -LiteralPath (Join-Path $Launch 'native-xr/ui-xr-checks.json') -Raw | ConvertFrom-Json
 if(!$ui.checks -or @($ui.checks.PSObject.Properties | Where-Object Value -ne 'PASS').Count){throw 'UI failed.'}
 $results.Add([pscustomobject]@{name='vertical-ui';status='PASS';log=$uiLog})
} catch {$results.Add([pscustomobject]@{name='vertical-ui';status='FAIL';log=$uiLog;error=$_.Exception.Message})}
$failed=@($results | Where-Object status -eq 'FAIL')
@{status=$(if($failed.Count){'FAIL'}else{'PASS'});checks=$results.ToArray();scope='Finite right stick L3/B mapping, native state, same-frame eyes and native UI; no full input scheme/reentry/unlimited-play claim.'} | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $Probe 'vertical-trial-checks.json') -Encoding utf8
$results | Format-Table name,status
if($failed.Count){throw "$($failed.Count) stages failed; evidence retained."}
