param(
    [switch]$DryRun      # print what would be started, start nothing
)
# "Start the game" of "Enshrouded VR setting.exe" (2026-10-14, user: "VR mode is switched on and off there").
# Reads vrmod\config\EnshroudedVR.ini and starts the game the way it says:
#   VR=1  the VR session (tools/run_play_session.ps1 with the full feature set of the headset runs), with SteamVR.
#   VR=0  the game as Steam starts it, on the monitor; with Online\Sync=1 the mod without VR is then put into the
#         running game (tools/run_online_flat.py --attach): other players' VR bodies are shown.
# Nothing here restores or checks a save: this is for play, with the saves as they are.
$ErrorActionPreference='Stop'
$game=Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
Set-Location -LiteralPath $game
function Read-Setting([string]$text,[string]$section,[string]$key,[string]$fallback){
    $current=''
    foreach($line in ($text -split "`r?`n")){
        $t=$line.Trim()
        if($t -match '^\[(.+)\]$'){$current=$Matches[1].Trim();continue}
        if($current -ieq $section -and $t -match '^([^;#=\[][^=]*)=(.*)$' -and $Matches[1].Trim() -ieq $key){return (($Matches[2] -split ';')[0]).Trim()}
    }
    $fallback
}
$ini=Join-Path $game 'vrmod/config/EnshroudedVR.ini'
if(!(Test-Path -LiteralPath $ini)){Copy-Item -LiteralPath (Join-Path $game 'vrmod/config/EnshroudedVR.defaults.ini') -Destination $ini}
$text=[IO.File]::ReadAllText($ini)
$vr=(Read-Setting $text 'Mode' 'VR' '1') -ne '0'
$online=(Read-Setting $text 'Online' 'Sync' '0') -eq '1'
$menu=(Read-Setting $text 'Online' 'Menu' '0') -eq '1'
$scale=0.8;$parsed=0.0
if([double]::TryParse((Read-Setting $text 'Picture' 'RenderScale' '0.8'),[Globalization.NumberStyles]::Float,[Globalization.CultureInfo]::InvariantCulture,[ref]$parsed) -and $parsed -ge 0.25 -and $parsed -le 2){$scale=$parsed}
$dlss=Read-Setting $text 'Picture' 'Dlss' 'Quality';if($dlss -notin 'Dlaa','UltraQuality','Quality','Balanced','Performance','UltraPerformance'){$dlss='Quality'}
$sharp=0.2;if([double]::TryParse((Read-Setting $text 'Picture' 'Sharpness' '0.2'),[Globalization.NumberStyles]::Float,[Globalization.CultureInfo]::InvariantCulture,[ref]$parsed) -and $parsed -ge 0 -and $parsed -le 1){$sharp=$parsed}
$minutes=240;$whole=0;if([int]::TryParse((Read-Setting $text 'Session' 'Minutes' '240'),[ref]$whole) -and $whole -ge 1 -and $whole -le 240){$minutes=$whole}
$number={param($v) $v.ToString([Globalization.CultureInfo]::InvariantCulture)}
"Enshrouded VR MOD: VR=$([int]$vr) Online=$([int]$online) Menu=$([int]$menu) RenderScale=$(& $number $scale) Dlss=$dlss Sharpness=$(& $number $sharp) Minutes=$minutes"
if(Get-Process enshrouded -ErrorAction SilentlyContinue){throw 'The game is already running; close it first.'}
if($vr){
    # The feature set of the headset runs (docs/real_device_plan_20261012.md), without the test save and world check.
    $session=@{
        RealVr=$true;LiveInput=$true;Minutes=$minutes;UiQuad=$true;RenderScale=$scale;EyeFrustum='stable';SymmetricFov=$true;DetailBindings=$true;ShadowProjection=$true
        ShadowReceiverPlane='./vrmod/output/shadow-receiver-live-20261009';GiProbeFallback='./vrmod/output/gi-probe-fallback-20261010';FogProjection='match'
        VrDlssQuality=$dlss;VrSharpness=(& $number $sharp)
        LiveMelee=$true;LiveGuard=$true;TwoHandWieldVhvr=$true;QuickSlotMotion=$true;RealVrWeaponTrial=$true;WeaponMountObserve=$true;RangedAnimation=$true;ShieldMount=$true
        BowDraw=$true;RangedAim=$true;BodyIk=$true;LiveMeleeContact=$true;LiveMeleeEarly=$true;LiveMeleeStill=$true;LiveMeleeTargets=$true;LiveHeldItems=$true;LiveHeldFollow=$true
        UiAutoHide=$true;ShareVfxModels=$true;ShareDecals=$true;MenuPointer=$true
    }
    if($online){$session.OnlineSync=$true};if($menu){$session.Menu=$true}
    if($DryRun){"would start: run_play_session.ps1 "+(($session.GetEnumerator() | Sort-Object Name | ForEach-Object { if($_.Value -is [bool]){"-$($_.Name)"}else{"-$($_.Name) $($_.Value)"} }) -join ' ');return}
    if(!(Get-Process vrserver -ErrorAction SilentlyContinue)){'SteamVR is not running. Start SteamVR, then press Enter here.';[void](Read-Host)}
    if($menu){'The game starts at its menu. Join or host a session in the game window; then create the file start.request in the newest vrmod\output\game-... folder to begin VR.'}
    & (Join-Path $PSScriptRoot 'run_play_session.ps1') @session
}else{
    if($DryRun){"would start: the game from Steam (steam://rungameid/1203620)$(if($online){', then tools/run_online_flat.py --attach --minutes '+$minutes})";return}
    Start-Process 'steam://rungameid/1203620'
    if($online){
        'Waiting for the game ...'
        $deadline=(Get-Date).AddMinutes(5)
        while(-not (Get-Process enshrouded -ErrorAction SilentlyContinue)){if((Get-Date) -ge $deadline){throw 'The game did not start.'};Start-Sleep -Seconds 2}
        Start-Sleep -Seconds 20
        $py='S:/stable/Python31011/python.exe';if(!(Test-Path -LiteralPath $py)){$py='python'}
        $flat=@('--attach','--minutes',"$minutes")
        $curl='vrmod/output/grip-preview-20261004-1530';if(Test-Path -LiteralPath "$curl/hand-pose.bin"){$flat+=@('--hand-curl',$curl)}
        "Other players' VR bodies are shown for up to $minutes min (this window stays open; it closes when the game does)."
        & $py vrmod/tools/run_online_flat.py @flat
    }
}
