param(
    [string]$Hands='./vrmod/test_env/home-ranged-animation-20261010.csv',
    [string]$Pose='./vrmod/test_env/home-mill-20261010.csv', # Mock head script (the still home pose by default)
    [int]$Minutes=2,[int]$LogNameCap=3000,[double]$LiveMeleeHeavyFactor=0,[double]$LiveMeleeTravel=0,[int]$LiveMeleeHeavyHoldMs=-1,[switch]$LiveMeleeHeavyByMotion,
    [switch]$DesktopRender,[switch]$NoShareLights, # left-right lighting comparisons (2026-10-14): the desktop View drawn every frame / no VFX lights lent to the other eye
    [switch]$GiShare,           # the same comparisons: the right eye takes the left eye's GI probes (run_play_session -GiShare)
    [switch]$GiDump,            # read-only dump of both eyes' GI objects on pairs 300/301 (run_play_session -GiDump, 2026-10-15)
    [string]$GiShareKeep='',    # with -GiShare: hex mask of the GI stages the right eye still runs (run_play_session -GiShareKeep)
    [string]$SettingsFile='',   # another enshrouded_local.json for this run only (run_play_session -SettingsFile), e.g. point light shadows off
    [double]$RenderScale=1.0, # Mock: eye size = the head script's size x this (1.1429 gives the 2243 px of a headset run at 0.8)
    [switch]$Catalog,[switch]$Shield,[switch]$Ranged,[switch]$BowRelease,[switch]$BowCancel,[switch]$BowPhysicalCancel,[switch]$BowPhysicalPose,[switch]$BowPhysicalRelease,[switch]$BowSpawnObserve,[switch]$BowTrackedAim,[switch]$BowDraw,[switch]$RangedSurvey,[switch]$RangedAim,[switch]$LiveMelee,[switch]$LiveGuard,[switch]$TwoHandWield,[switch]$TwoHandWieldVhvr,[switch]$TwoHandCatalogOnly,[switch]$BodyIk,[switch]$QuickSlotMotion,[switch]$UiAutoHide,[switch]$MenuPointer,[switch]$ShareVfxModels,[switch]$ShareDecals,[switch]$NoVfxParticles,[switch]$LiveMeleeContact,[switch]$LiveMeleeContactObserve,[switch]$LiveMeleeEarly,[switch]$LiveMeleeStill,[switch]$LiveMeleeTargets,[switch]$LiveHeldItems,[switch]$LiveHeldFollow,[switch]$SavePage2,[switch]$BodyHeading,
    [switch]$Interactive,     # visible game window; VR starts when <game folder>/start.request appears (the user can then use the keyboard in the window)
    [switch]$NoSceneryReplay, # the view follows the character, as on a headset (no fixed draw camera; images are not comparable)
    [switch]$MockNavigate,    # Mock: tools/mock_walk_to.py may steer the left stick by position; the hand script keeps its own buttons
    [switch]$NoMountObserve,  # the plain live-input feature set of the first headset run: no weapon mount survey, no bow/staff/shield display routes
    [string]$SaveReference='',  # another kept save of the same world to start from (a folder under vrmod/output/save-backups), e.g. one saved in sunshine for the shadow tests of 2026-10-13; the checks that expect the default start (first page, bow in hand, at home) do not apply then
    [switch]$OnlineSync,        # other players' VR bodies (run_play_session -OnlineSync); alone it only lists nobody and sends nothing
    [int]$GpuTimingEvery=0,     # every N-th pair with GPU timestamps (tools/analyze_gpu_timing.py); for measuring where the frame's time goes
    [string]$VrDlssQuality='',  # another DLSS mode than Quality for the run (Balanced, Performance ...), for the same
    [string]$ShadowReceiver=''  # another prepared sun shadow shader folder (manifest.json + shadow_receiver.spv), e.g. a diagnostic variant of tools/make_shadow_watch_fixture.py
)
# Home save and reviewed scenery inputs. Validate before launching. Save references, all kept side by side:
#   20261012-054414-655  default: the user's save of 2026-10-11 22:07, saved again from a Mock run (game054137) with the
#                        first action bar page and the bow in hand, as the older hand scripts expect. Same items.
#   20261012-050322-485  -SavePage2: that save exactly as the user left it: second page, construction hammer in hand
#                        (T4 bow, bombs, summoning staff, fishing rod, hammer, daggers, crucible, watering can).
#   20261010-092354-491  the reference before 2026-10-12 (no second page items).
# Page swap in play: wheel held (right grip) + right stick click; D-pad left/right steps through the page.
Set-Location -LiteralPath 'S:\SteamLibrary\steamapps\common\Enshrouded'
$trialArgs=@{
    Minutes=$Minutes;LiveInput=$true;WeaponMountObserve=$true;Hands=$Hands
    Pose=$Pose;SceneryReplay='./vrmod/test_env/home-mill-20261010.txt'
    SaveReference=$(if($SavePage2){'./vrmod/output/save-backups/20261012-050322-485'}else{'./vrmod/output/save-backups/20261012-054414-655'});ExpectedWorld='3bd85c7d'
    UiQuad=$true;EyeFrustum='stable';SymmetricFov=$true;DetailBindings=$true;ShadowProjection=$true
    ShadowReceiverPlane='./vrmod/output/shadow-receiver-live-20261009'
    GiProbeFallback='./vrmod/output/gi-probe-fallback-20261010'
    FogProjection='match';VrDlssQuality='Quality';VrSharpness='0.2'
}
if($SaveReference){$trialArgs.SaveReference=$SaveReference}
if($ShadowReceiver){$trialArgs.ShadowReceiverPlane=$ShadowReceiver}
if($OnlineSync){$trialArgs.OnlineSync=$true};if($GpuTimingEvery -gt 0){$trialArgs.GpuTimingEvery=$GpuTimingEvery};if($VrDlssQuality){$trialArgs.VrDlssQuality=$VrDlssQuality}
foreach($key in @('Hands','Pose','SceneryReplay','SaveReference','ShadowReceiverPlane','GiProbeFallback')){
    if(!(Test-Path -LiteralPath $trialArgs[$key])){throw "Missing trial input: $key = $($trialArgs[$key])"}
}
if($NoSceneryReplay){$trialArgs.Remove('SceneryReplay')}
if($Interactive){$trialArgs.Interactive=$true}
if($LiveMeleeHeavyFactor -gt 0){$trialArgs.LiveMeleeHeavyFactor=$LiveMeleeHeavyFactor}
if($LiveMeleeTravel -gt 0){$trialArgs.LiveMeleeTravel=$LiveMeleeTravel}
if($LiveMeleeHeavyHoldMs -ge 0){$trialArgs.LiveMeleeHeavyHoldMs=$LiveMeleeHeavyHoldMs};if($LiveMeleeHeavyByMotion){$trialArgs.LiveMeleeHeavyByMotion=$true}
if($MockNavigate){$trialArgs.MockNavigate=$true;$trialArgs.MockNavigateHands=$true}
if($DesktopRender){$trialArgs.DesktopRender=$true};if($NoShareLights){$trialArgs.NoShareLights=$true};if($GiShare){$trialArgs.GiShare=$true};if($GiDump){$trialArgs.GiDump=$true};if($GiShareKeep){$trialArgs.GiShareKeep=$GiShareKeep};if($SettingsFile){$trialArgs.SettingsFile=$SettingsFile}
if($NoMountObserve){$trialArgs.Remove('WeaponMountObserve')}
if($Catalog){$trialArgs.ItemCatalog=$true};if($Shield){$trialArgs.ShieldMount=$true}
if($BowSpawnObserve){$trialArgs.BowSpawnObserve=$true}
if($BowPhysicalPose){$trialArgs.BowPhysicalPose=$true}
if($BowPhysicalRelease){$trialArgs.BowPhysicalRelease=$true}
if($BowTrackedAim){$trialArgs.BowTrackedAim=$true}
if($BowDraw){$trialArgs.BowDraw=$true}
if($RangedSurvey){$trialArgs.RangedSurvey=$true}
if($RangedAim){$trialArgs.RangedAim=$true}
if($LiveMelee){$trialArgs.LiveMelee=$true}
if($LiveGuard){$trialArgs.LiveGuard=$true}
if($TwoHandWield){$trialArgs.TwoHandWield=$true}
if($TwoHandWieldVhvr){$trialArgs.TwoHandWieldVhvr=$true}
if($TwoHandCatalogOnly){$trialArgs.TwoHandCatalogOnly=$true}
if($BodyIk){$trialArgs.BodyIk=$true}
if($QuickSlotMotion){$trialArgs.QuickSlotMotion=$true};if($UiAutoHide){$trialArgs.UiAutoHide=$true};if($MenuPointer){$trialArgs.MenuPointer=$true};if($ShareDecals){$trialArgs.ShareDecals=$true};if($ShareVfxModels){$trialArgs.ShareVfxModels=$true};if($NoVfxParticles){$trialArgs.NoVfxParticles=$true}
if($LiveMeleeContact){$trialArgs.LiveMeleeContact=$true};if($LiveMeleeContactObserve){$trialArgs.LiveMeleeContactObserve=$true};if($LiveMeleeEarly){$trialArgs.LiveMeleeEarly=$true};if($LiveMeleeStill){$trialArgs.LiveMeleeStill=$true};if($LiveMeleeTargets){$trialArgs.LiveMeleeTargets=$true};if($LiveHeldItems){$trialArgs.LiveHeldItems=$true};if($LiveHeldFollow){$trialArgs.LiveHeldFollow=$true};if($BodyHeading){$trialArgs.BodyHeading=$true}
if($LogNameCap -ne 3000){$trialArgs.LogNameCap=$LogNameCap}
if($RenderScale -ne 1.0){$trialArgs.RenderScale=$RenderScale}
if($BowPhysicalCancel){$trialArgs.BowPhysicalCancel=$true}
if($BowCancel){$trialArgs.BowCancelTrial=$true}
if($Ranged){$trialArgs.RangedAnimation=$true};if($BowRelease){$trialArgs.BowReleaseTrial=$true}
& ./vrmod/tools/run_play_session.ps1 @trialArgs
