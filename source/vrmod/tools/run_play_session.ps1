param(
    [ValidateRange(1,240)][int]$Minutes=15,
    [ValidateRange(20,180)][int]$StartAfterSeconds=90, # the live probe starts this long after the game (2026-10-19, user: "shorten the wait before the Mock moves"); the probe waits for the camera itself since 10-16
    [string]$Pose='./vrmod/test_env/world-static-pose-1280x1440.csv',
    [ValidatePattern('^([0-9a-f]{8})?$')][string]$ExpectedWorld='', # verify native selection before any play input
    [switch]$MockNavigate,                     # short external commands, Mock only
    [switch]$MockNavigateHands,                # with -MockNavigate: the hand script keeps its buttons and sticks, commands only add the left stick
    [switch]$EyeImageSweep,                    # Mock navigation raw image dump / existing diagnostic sweep
    [switch]$TwoHandWield,                     # explicit measured-asset display/slide trial
    [switch]$TwoHandWieldVhvr,                 # two-hand hold with VHVR's entry rule: left grip, hands along the weapon (implies -TwoHandWield)
    [switch]$BodyIk,                           # experiment: the player's own body on a mod-owned pose (hands at the controllers), with -LiveInput
    [switch]$TwoHandCatalogOnly,               # diagnostic: measured melee assets also take the catalog group mount (implies -TwoHandWieldVhvr)
    [switch]$ItemCatalog,                      # Mock-only read-only resident item definitions
    [switch]$ShieldMount,                      # Mock-only measured shield display beside main weapon
    [switch]$RangedAnimation,                  # Mock-only reviewed staff/wand and bow private poses
    [switch]$BowAnimation,                     # Mock-only private pose on reviewed bow
    [switch]$BowSpawnObserve,                  # Mock-only read-only native Crosshair spawn witness
    [switch]$BowTrackedAim,                    # Mock one release uses private two-hand projectile aim
    [switch]$BowDraw,                          # Mock VHVR-style repeatable draw: two-hand bow orientation, re-armed after every shot
    [switch]$RangedSurvey,                     # Mock survey: left primary -> native X, Crosshair witness logs other event types
    [switch]$RangedAim,                        # Mock VHVR-style staff/wand aim: weapon emitter + pointer ray, wand trigger -> attack
    [switch]$LiveMelee,                        # with -LiveInput: a VHVR-style swing of the melee weapon becomes one native attack press
    [switch]$BodyHeading,                      # with -LiveInput: the character's body faces where the headset looks (VHVR), also while walking sideways or backward
    [switch]$LiveHeldFollow,                   # experiment with LiveHeldItems: the game entity of the right-hand item follows the tracked hand (its light and effects with it)
    [switch]$LiveHeldItems,                    # with the weapon display: tools are shown and swung like melee weapons; every catalog shield is shown
    [switch]$LiveMeleeContact,                 # with -LiveMelee and the weapon display: the attack's hit volume follows the tracked weapon
    [switch]$LiveMeleeContactObserve,          # the same, plus read-only physics-query and hit observers (Mock evidence)
    [switch]$LiveMeleeEarly,                   # with -LiveMeleeContact: the hit volume starts at once when the swing starts the attack (no wind-up delay)
    [switch]$LiveMeleeTargets,                 # with -LiveMeleeEarly: a stroke attacks only at a target (VHVR), not through the air
    [double]$LiveMeleeTravel=0,                # with -LiveMelee: metres (0..1) a stroke has to cover above the required speed before it attacks
    [switch]$LiveMeleeHeavyByMotion,           # with -LiveMelee: VHVR's judgement by the motion (strong thrust, hook ...) also makes a heavy attack; default only "right trigger held while swinging"
    [ValidateRange(-1,3000)][int]$LiveMeleeHeavyHoldMs=-1, # with -LiveMelee: a stroke VHVR takes as the secondary attack holds the attack button this long = heavy attack (-1: hook default 400 ms, 0: off)
    [double]$LiveMeleeHeavyFactor=0,           # with -LiveMelee: factor (1..2) on the swing speed that a heavy weapon needs; 0 keeps VHVR's 1.5
    [switch]$LiveMeleeStill,                   # with -LiveMeleeEarly: the attack plays no animation and does not turn or move the character
    [ValidateRange(1000,20000)][int]$LogNameCap=3000, # per-event-name log limit (launcher -LogNameCap); long combined trials need more
    [switch]$RealVrWeaponTrial,                # with -RealVr: allow the Mock-reviewed weapon display, bow and staff routes (supervised trial)
    [switch]$ShareVfxModels,                   # the eye without the VFX culls the VFX models for itself: aim circle / building area in both eyes (needs the shared stages)
    [switch]$ShareDecals,                      # the eye without the VFX borrows the owner's decal list: aim circles on the ground in both eyes (needs the shared stages)
    [switch]$MenuPointer,                      # with -LiveInput -UiQuad: in a menu the right hand's ray is the mouse (trigger = left click, B = right click), a dot shows the place
    [switch]$UiAutoHide,                       # with -LiveInput -UiQuad: the HUD fades out 5 s after it last changed (health, mana, anything shown); world pins stay
    [switch]$QuickSlotMotion,                  # with -LiveInput: right B held opens the quick wheel, the right hand points, release takes; right grip free
    [switch]$LiveGuard,                        # with -LiveInput: a raised shield (or two-hand weapon held across) holds the native block
    [switch]$BowPhysicalRelease,               # Mock-only physical release, native aim/power retained
    [switch]$BowPhysicalPose,                  # Mock-only private bow deformation from physical distance
    [switch]$BowPhysicalCancel,                # Mock-only near-string acquisition, physical pull, no firing
    [switch]$BowCancelTrial,                   # Mock-only LT charge then cancel without an RT tap
    [switch]$BowReleaseTrial,                  # Mock-only charge while held, short RT on release
    [switch]$BowAimTrial,                      # Mock-only native LT aim + RT shoot comparison
    [switch]$BowInputTrial,                    # Mock-only one known-bow RT -> native X hold/release
    [switch]$WeaponMountObserve,               # Mock live per-item read-only hand/weapon mount snapshots
    [switch]$DlssObserve,                      # read-only per-eye DLSS size/evaluation audit
    [string]$SaveReference='',                  # explicitly restore one managed snapshot after preserving current saves
    [switch]$CaptureOnRequest,                 # one manual GPU capture during Mock navigation
    [switch]$CaptureShaders,                    # Mock diagnostic: the layer writes every shader module (shader_<hash>.spv, 64 MB cap)
    [switch]$CaptureWorldFinal,                 # Mock diagnostic: existing portable RenderDoc capture at selected pair
    [ValidateRange(301,3000)][int]$CapturePair=3000,
    [ValidateSet('None','Core','Sync')][string]$VulkanValidation='None',   # Mock diagnostic (2026-10-17): the portable Khronos validation layer, its log in the run's folder
    [string]$SettingsFile='',          # optional enshrouded_local.json applied only for this run, then restored
    [switch]$DesktopMirror,
    [int]$LumaEvery=120,
    [int]$GpuTimingEvery=0,
    [switch]$XrSynchronous,                    # diagnostic: wait for GPU completion each frame; launcher strict path
    [switch]$NoSharedStages,           # minimal probe set (no VFX-light sharing / early XR / shared culling / water / culling history)
    [ValidateRange(0,1)][double]$MockVisibilityMask=0,  # mock runtime offers a test visibility mask (fov crop test)
    [switch]$NoFovCrop,
    [switch]$SymmetricFov,                      # opt-in native centered-projection compatibility
    [ValidateRange(-1,60)][int]$DesktopSkip=-1,  # explicit probe --world-desktop-skip N (default: refresh original View once per 30 frames; 0 = full suppression diagnostic)
    [switch]$DesktopDiscard,                    # probe --world-desktop-discard (desktop renders, result discarded)
    [switch]$DesktopSkipVfx,                    # kept for older commands; the runner adds the eye VFX owner when the eyes borrow VFX
    [switch]$DesktopCompare,                    # Mock-only requested skip/render A/B at fixed draw camera
    [switch]$DesktopRender,                     # opt-out: render the desktop View every frame (--world-desktop-render)
    [switch]$NoDesktopMirror,
    [switch]$GiDump,                            # read-only GI object dump (probe --world-gi-dump)
    [switch]$GiShare,                           # opt-in GI share (right eye borrows the left eye GI; darker right-eye shadows with head motion, 2026-10-09)
    [switch]$GiOwn,                             # kept for older commands (per-eye GI is the default since 2026-10-09)
    [ValidateSet('','observe','stable')][string]$EyeFrustum='', # diagnostic only
    [string]$SceneryReplay="",                    # Mock-only draw camera reference for repeatable image comparisons
    [switch]$NoHzb,                              # diagnostic: private main-view HZB rejection only
    [string]$ShadowReceiverPlane='',             # opt-in generated local shader directory
    [string]$GiProbeFallback='',                 # opt-in empty GI probe fallback; native history retained
    [ValidateSet('','observe','match','compare')][string]$FogProjection='', # opt-in fog history optical match
    [switch]$ShadowProjection,                   # opt-in native shadow perspective conversion
    [switch]$DetailBindings,                     # opt-in: match private detail shader/draw owners
    [switch]$ObserveCulling,                     # read-only private native culling constants
    [switch]$ObjectWatch,                       # diagnostic: fog object write map (probe --world-object-watch)
    [switch]$KeepDof,                           # keep the game depth of field in the eyes (default: off in VR)
    [string]$VrSharpness='',                    # launcher -VrSharpness (0..1 or Keep); empty = launcher default
    [switch]$HandsOnly,                         # hide the local body except hands (as in VR play); needed when the pose turns the head
    [switch]$UiQuad,                            # HUD on the XR quad + UI capture/pipeline records (vignette investigation)
    [switch]$Interactive,                       # visible game window: the user moves the character, then <launch>/start.request starts VR
    [ValidateRange(1,120)][int]$StartWaitMinutes=45,
    [switch]$OnlineSync,                        # other players' VR bodies (hooks/online_sync.hpp): this player's body goes to the players of the session who run the mod, theirs are shown (with -BodyIk)
    [switch]$Menu,                              # the game starts at its own menu (join or host a session there, in the visible window); VR starts when <launch>/start.request appears
    [string]$WriteProfile='',                   # no game: the policy files this session's eyes would get, into this new folder (the package's profile, tools/pack_release.ps1)
    [ValidateRange(-1,4)][double]$ForceFreezing=-1, # diagnostic: force the freezing vignette amount (probe --world-force-freezing)
    [switch]$GameVignettes,                         # opt-out: the game's own status vignettes instead of the VR rim tint
    [ValidateRange(-1,1)][double]$StatusTint=-1,    # VR rim tint strength 0..1 (probe --world-status-tint); -1 = runner default 0.5
    [ValidateSet('','Dlaa','UltraQuality','Quality','Balanced','Performance','UltraPerformance')][string]$VrDlssQuality='', # launcher -VrDlssQuality (empty = the user's DLSS mode)
    [switch]$WalkFixture,                       # TEST ONLY: HMD walking camera + four synthetic analog walking pulses (probe --world-input-walk-fixture)
    [switch]$UiPartial,                         # diagnostic: launcher -UiPartial (UI frames with unreviewed draws are kept)
    [ValidateRange(0,3000)][int]$FixtureAfter=0,                # arm the synthetic controller fixture after N pairs (0 = default 30)
    [ValidateSet('','Menu','Tabs','Tour')][string]$MenuFixture='',  # TEST ONLY: synthetic controller opens the menu (Menu) or also switches tabs (Tabs); UI captured in both states
    [switch]$RealVr,                            # 2026-10-09: the user's running SteamVR and headset (launcher -PrepareRealVr), no mock pose
    [switch]$LiveInput,                         # continuous play input from the VR controllers (probe --world-live-input; tracked hands)
    [string]$Hands='',                          # Mock only: hand/controller CSV for -LiveInput (launcher -MockHandSequence)
    [ValidateRange(0.25,2.0)][double]$RenderScale=1.0, # eye size = runtime recommended size x this (probe --render-scale)
    [string]$GripTrim='',                       # launcher -GripTrim "yaw,pitch,roll" or none (default: EDF6VR angles with -RealVr)
    [switch]$NoShareLights,                     # the other eye does not borrow the VFX point lights (left-right lighting comparison, 2026-10-14)
    [switch]$NoVfxParticles,                    # opt-out: no transparent VFX particles (fire, smoke) bridged into the right eye
    [switch]$MoveHeldVfx,                       # opt-in scoped torch effect attachment
    [switch]$ObserveHeldVfx,                    # read-only held effect ownership diagnostic
    [switch]$NoFingerCurl,                      # opt-out: static hands (default with -LiveInput: EDF6VR-style finger curl)
    [ValidateRange(-90,90)][double]$WeaponPitch=-35, # VHVR equipped/unequipped target delta; weapon only
    [string]$GiShareKeep='',                    # diagnostic with -GiShare: stage mask still run in the right eye (probe --world-gi-share-keep)
    [string]$UseGeneration=''                   # diagnostic (2026-10-16): run a stored build/diagnostic-generations/<name> instead of building (a fresh copy per injection)
)
# -UseGeneration: a copy of the stored generation under a new name (a DLL path already loaded in the process would not load
# again), and the manifests in build/bin/Release point at it. The next build.ps1 writes the manifests again.
function Use-StoredGeneration([string]$from){
    $generations=(Resolve-Path -LiteralPath 'vrmod/build/diagnostic-generations').Path.TrimEnd('\')
    $source=(Resolve-Path -LiteralPath $from).Path.TrimEnd('\')
    if(-not $source.StartsWith($generations+'\',[StringComparison]::OrdinalIgnoreCase) -or !(Test-Path -LiteralPath (Join-Path $source 'build-evidence.json'))){throw 'UseGeneration must be a stored diagnostic generation.'}
    $copy=Join-Path $generations ((Get-Date -Format 'yyyyMMdd-HHmmss-fff')+'-reuse')
    New-Item -ItemType Directory -Path $copy | Out-Null;Copy-Item -LiteralPath (Join-Path $source 'Release') -Destination $copy -Recurse
    $evidence=Get-Content -LiteralPath (Join-Path $source 'build-evidence.json') -Raw | ConvertFrom-Json
    foreach($b in $evidence.binaries){$f=Join-Path $copy ('Release/'+(Split-Path $b.path -Leaf));if((Get-FileHash -LiteralPath $f -Algorithm SHA256).Hash -ne $b.sha256){throw 'Stored generation hash mismatch.'}}
    @{reused_from=$source;copied=(Get-Date -Format o);binaries=$evidence.binaries} | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $copy 'build-evidence.json') -Encoding utf8
    foreach($m in Get-ChildItem -LiteralPath 'vrmod/build/bin/Release' -Filter 'evr_*.json'){
        $j=Get-Content -LiteralPath $m.FullName -Raw | ConvertFrom-Json
        $target=if($j.layer){$j.layer}else{$j}
        if(!$target.library_path -or $target.library_path -notmatch 'diagnostic-generations'){continue}
        $target.library_path=((Join-Path $copy ('Release/'+(Split-Path $target.library_path -Leaf))) -replace '\\','/')
        if(!(Test-Path -LiteralPath $target.library_path)){throw "Stored generation lacks $($target.library_path)"}
        $j | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $m.FullName -Encoding utf8
    }
    "stored generation $source -> $copy"
}
# Managed play session with the Mock runtime (2026-10-06):
#   [settings window] -> launch_diagnostics (detached, no pipes) -> unused proof -> fresh DLL -> live probe with
#   --world-session $Minutes (ends at the minute limit or <probe>/stop.request) -> normal close (WM_SYSCOMMAND/SC_CLOSE)
#   -> check_play_session.py -> settings restored. nvidia-smi is sampled every 10 s into claude-gpu-samples.csv.
# This is a diagnostic session with a static mock pose, not a real-HMD session.
# -RealVr (2026-10-09): the same flow with the user's running SteamVR (checked read-only by the launcher; the runtime
# is set for the game process only). The host VR settings are snapshotted before and verified after the run.
# -LiveInput: tracked hands plus the live controller table (include/evr/live_pad.hpp) instead of any fixture.
$ErrorActionPreference='Stop'
if($OnlineSync -and !$BodyIk){throw 'OnlineSync requires BodyIk (the body that is sent is the one the body IK solves).'}
if($Menu -and ($SaveReference -or $ExpectedWorld -or $SceneryReplay)){throw 'Menu starts at the game''s menu: no save reference, expected world or scenery replay.'}
if($Menu){$Interactive=$true}
if($MockNavigate -and ($RealVr -or !$LiveInput)){throw 'MockNavigate requires Mock LiveInput.'}
if($MockNavigateHands -and !$MockNavigate){throw 'MockNavigateHands requires MockNavigate.'}
if($DesktopCompare -and (!$MockNavigate -or !$SceneryReplay -or $DesktopRender -or $DesktopDiscard)){throw 'DesktopCompare requires MockNavigate, SceneryReplay and desktop skip.'}
if($RealVrWeaponTrial -and !$RealVr){throw 'RealVrWeaponTrial is only for -RealVr.'}
$mockOnly=$RealVr -and !$RealVrWeaponTrial   # weapon display/bow/staff routes: Mock, or an explicit supervised real-headset trial
if($WeaponMountObserve -and ($mockOnly -or !$LiveInput)){throw 'WeaponMountObserve requires Mock LiveInput (or -RealVrWeaponTrial).'}
if($BowDraw){$BowTrackedAim=$true}
if($TwoHandCatalogOnly){$TwoHandWieldVhvr=$true}
if($TwoHandWieldVhvr){$TwoHandWield=$true}
if($RangedAim){$RangedAnimation=$true;$BowSpawnObserve=$true}
if(($LiveMelee -or $LiveGuard) -and !$LiveInput){throw 'LiveMelee and LiveGuard require LiveInput.'}
if($LiveMeleeTargets -and !$LiveMeleeEarly -and !$LiveMeleeStill){throw 'LiveMeleeTargets requires LiveMeleeEarly.'};if($LiveMeleeStill){$LiveMeleeEarly=$true};if($LiveMeleeContactObserve -or $LiveMeleeEarly){$LiveMeleeContact=$true}
if($LiveMeleeContact -and (!$LiveMelee -or !$WeaponMountObserve)){throw 'LiveMeleeContact requires LiveMelee and the weapon display (WeaponMountObserve).'}
if($LiveHeldItems -and !$WeaponMountObserve){throw 'LiveHeldItems requires the weapon display (WeaponMountObserve).'}
if($LiveHeldFollow -and !$LiveHeldItems){throw 'LiveHeldFollow requires LiveHeldItems.'}
if($BodyHeading -and !$LiveInput){throw 'BodyHeading requires LiveInput.'}
if($BowTrackedAim){$BowPhysicalRelease=$true;$BowSpawnObserve=$true}
if($BowSpawnObserve -and ($mockOnly -or !$LiveInput -or !$WeaponMountObserve)){throw 'BowSpawnObserve requires Mock live mount survey.'}
if($BowPhysicalRelease){$BowPhysicalPose=$true}
if($BowPhysicalPose){$BowPhysicalCancel=$true}
if($RangedAnimation -or $BowPhysicalCancel){$BowAnimation=$true}
if($BowPhysicalCancel -and ($BowInputTrial -or $BowAimTrial -or $BowReleaseTrial -or $BowCancelTrial -or ($TwoHandWield -and !$BowDraw))){throw 'Physical bow trial excludes other input trials.'}
if($BowAnimation -and ($mockOnly -or !$LiveInput -or !$WeaponMountObserve)){throw 'BowAnimation requires Mock live mount survey.'}
if($ItemCatalog -and ($RealVr -or !$LiveInput -or !$WeaponMountObserve)){throw 'ItemCatalog requires Mock live mount survey.'}
if($ShieldMount -and ($mockOnly -or !$LiveInput -or !$WeaponMountObserve)){throw 'ShieldMount requires Mock live mount survey.'}
if(@($BowReleaseTrial,$BowAimTrial,$BowCancelTrial).Where({$_}).Count -gt 1){throw 'Choose one isolated bow input comparison.'}
if($BowAimTrial -or $BowReleaseTrial -or $BowCancelTrial){$BowInputTrial=$true}
if($BowInputTrial -and ($RealVr -or !$LiveInput -or !$WeaponMountObserve -or $TwoHandWield)){throw 'BowInputTrial requires isolated Mock live mount survey.'}
if($TwoHandWield -and !$LiveInput){throw 'TwoHandWield requires LiveInput.'}
if($EyeImageSweep -and ($RealVr -or !$MockNavigate)){throw 'EyeImageSweep in this runner requires MockNavigate.'}
if($CaptureOnRequest -and ((!$MockNavigate -and !$RealVr) -or !$CaptureWorldFinal)){throw 'CaptureOnRequest requires CaptureWorldFinal and MockNavigate or RealVr (2026-10-18: on the headset the VR bug report button asks for it).'}
if($RealVr -and $SceneryReplay){throw 'SceneryReplay is a Mock-only draw camera fixture.'}
if($FogProjection -eq 'compare' -and ($RealVr -or !$SceneryReplay)){throw 'Fog comparison is a Mock-only scenery replay fixture.'}
if($RealVr -and $CaptureWorldFinal -and !$CaptureOnRequest){throw 'This bounded capture is a Mock-only diagnostic (on the headset only on request: -CaptureOnRequest).'}
if($CapturePair -ne 3000 -and -not $CaptureWorldFinal){throw 'CapturePair requires CaptureWorldFinal.'}
if($RealVr -and $Hands){throw '-Hands is a Mock input; the real controllers are used with -RealVr.'}
if($LiveInput -and !$RealVr -and !$Hands){throw '-LiveInput on the Mock runtime needs -Hands (a hand/controller CSV).'}
if($LiveInput -and ($MenuFixture -or $WalkFixture -or $NoSharedStages)){throw '-LiveInput excludes the menu/walk fixtures and needs the shared stages (early XR, shared culling).'}
Set-Location -LiteralPath (Split-Path (Split-Path $PSScriptRoot -Parent) -Parent)
$pw=(Get-Process -Id $PID).Path
$py='S:/stable/Python31011/python.exe'
$vfxParticles=-not $NoVfxParticles -and -not $NoSharedStages
# The live probe's arguments: for the session below, and for the package's profile (-WriteProfile).
function Get-ProbeArguments([string]$launchFolder,[string]$proofFolder){
        $probe=@('--launch',$launchFolder,'--world-live-xr',$proofFolder,'--live-frames',$(if($CaptureWorldFinal){"$CapturePair"}else{'3000'}),'--render-scale',$RenderScale.ToString([Globalization.CultureInfo]::InvariantCulture),'--world-eye-private-transient','--world-shared-updates-once','--world-root-camera','first-person','--world-endurance','--world-session',"$Minutes")
        if(-not $NoSharedStages){if(-not $NoShareLights){$probe+='--world-share-vfx-lights'};$probe+=@('--world-early-xr','--world-shared-culling-camera','--world-water-feedback','--world-culling-history');if($ShareDecals){$probe+='--world-share-vfx-decals'};if($ShareVfxModels){$probe+='--world-share-vfx-models'}}
        if($vfxParticles){$probe+=@('--world-vfx-transparent-observe','--world-vfx-transparent-render')}
        if($DesktopSkip -ge 0){$probe+=@('--world-desktop-skip',"$DesktopSkip")}
        if($DesktopRender){$probe+='--world-desktop-render'}
        if($DesktopCompare){$probe+='--world-desktop-compare'}
        if($CaptureWorldFinal){$probe+='--capture-world-final'}
        if($CaptureOnRequest){$probe+='--capture-on-request'}
        if($GiDump){$probe+='--world-gi-dump'}
        if($DlssObserve){$probe+='--world-dlss-observe'}
        if($WeaponMountObserve){$probe+='--world-native-grip-observe'}
        if($BowInputTrial){$probe+='--world-bow-input-trial'}
        if($BowAimTrial){$probe+='--world-bow-aim-trial'}
        if($BowReleaseTrial){$probe+='--world-bow-release-trial'}
        if($BowCancelTrial){$probe+='--world-bow-cancel-trial'}
        if($BowPhysicalCancel){$probe+='--world-bow-physical-cancel'}
        if($BowPhysicalPose){$probe+='--world-bow-physical-pose'}
        if($BowTrackedAim){$probe+='--world-bow-tracked-aim'}
        if($BowDraw){$probe+='--world-bow-draw'}
        if($RangedSurvey){$probe+='--world-ranged-survey'}
        if($RangedAim){$probe+='--world-ranged-aim'}
        if($LiveMelee){$probe+='--world-live-melee';if($LiveMeleeTargets){$probe+='--world-live-melee-targets'};if($LiveMeleeHeavyFactor -gt 0){$probe+='--world-live-melee-heavy-factor';$probe+=$LiveMeleeHeavyFactor.ToString([Globalization.CultureInfo]::InvariantCulture)};if($LiveMeleeHeavyByMotion){$probe+='--world-live-melee-heavy-motion'};if($LiveMeleeHeavyHoldMs -ge 0){$probe+='--world-live-melee-heavy-hold';$probe+="$LiveMeleeHeavyHoldMs"};if($LiveMeleeTravel -gt 0){$probe+='--world-live-melee-travel';$probe+=$LiveMeleeTravel.ToString([Globalization.CultureInfo]::InvariantCulture)}}
        if($LiveHeldItems){$probe+='--world-live-held-items'}
        if($LiveHeldFollow){$probe+='--world-live-held-follow'}
        if($BodyHeading){$probe+='--world-body-heading'}
        if($LiveMeleeContact){$probe+='--world-live-melee-contact';$probe+=$(if($LiveMeleeEarly -and $LiveMeleeContactObserve){'early-observe'}elseif($LiveMeleeEarly){'early'}elseif($LiveMeleeContactObserve){'observe'}else{'apply'})+$(if($LiveMeleeStill){'-still'}else{''})}
        if($LiveGuard){$probe+='--world-live-guard'}
        if($QuickSlotMotion){$probe+='--world-quick-slot-motion'}
        if($UiAutoHide){$probe+='--world-ui-auto-hide'}
        if($MenuPointer){$probe+='--world-menu-pointer'}
        if($RealVrWeaponTrial){$probe+='--world-weapon-trial-real'}
        if($BowPhysicalRelease){$probe+='--world-bow-physical-release'}
        if($BowSpawnObserve){$probe+='--world-bow-spawn-observe'}
        if($BowAnimation){$probe+='--world-bow-animation'}
        if($RangedAnimation){$probe+='--world-ranged-animation'}
        if($ShieldMount){$probe+='--world-shield-mount'}
        if($ItemCatalog){$probe+='--world-item-catalog'}
        if($TwoHandWield){$probe+='--world-two-hand-wield'}
        if($TwoHandWieldVhvr){$probe+='--world-two-hand-wield-vhvr'}
        if($TwoHandCatalogOnly){$probe+='--world-two-hand-catalog-only'}
        if($BodyIk){$probe+='--world-body-ik'}
        if($EyeImageSweep){$probe+='--world-eye-dealias'}
        if($GiShare){$probe+='--world-gi-share';if($GiShareKeep){$probe+=@('--world-gi-share-keep',$GiShareKeep)}}
        if($GiOwn){$probe+='--world-gi-own'}
        if($ObjectWatch){$probe+='--world-object-watch'}
        if($EyeFrustum){$probe+=@('--world-eye-frustum',$EyeFrustum)}
        if($ObserveCulling){$probe+='--world-scenery-culling'}
        if($FogProjection){$probe+=@('--world-fog-projection',$FogProjection)}
        if($ShadowProjection){$probe+='--world-shadow-projection'}
        if($DetailBindings){$probe+='--world-detail-bindings'}
        if($NoHzb){$probe+='--world-scenery-no-hzb'}
        if($SceneryReplay){$probe+=@('--world-scenery-replay',(Resolve-Path -LiteralPath $SceneryReplay).Path)}
        if($KeepDof){$probe+='--world-keep-dof'}
        if($ForceFreezing -ge 0){$probe+=@('--world-force-freezing',"$ForceFreezing")}
        if($GameVignettes){$probe+='--world-game-vignettes'}
        if($WalkFixture){$probe+=@('--world-input-observe','--world-input-head-walking','--world-input-walk-fixture')}
        if($FixtureAfter -gt 0){$probe+=@('--world-controller-fixture-after',"$FixtureAfter")}
        if($MenuFixture){$probe+='--world-input-observe';$probe+=$(if($MenuFixture -eq 'Tour'){'--world-controller-menu-tour'}elseif($MenuFixture -eq 'Tabs'){'--world-controller-tabs-fixture'}else{'--world-controller-menu-fixture'})}
        if($StatusTint -ge 0){$probe+=@('--world-status-tint',"$StatusTint")}
        if($HandsOnly -or $LiveInput){$probe+=@('--world-hands-only','--world-head-visibility','--world-camera-modes','--world-equipment-observe')}
        if($ObserveHeldVfx){$probe+='--world-held-vfx-observe'}
        if($MoveHeldVfx){$probe+='--world-held-vfx-mount'}
        if($LiveInput){$probe+=@('--world-weapon-pitch',$WeaponPitch.ToString([Globalization.CultureInfo]::InvariantCulture),'--world-live-input','--world-motion-visual','--world-motion-hands','--world-attachments-observe','--world-input-observe','--world-input-head-walking')}
        # Finger curl (2026-10-09 "指の可動はEDF6を参考にして"): the closed hand is the grip calibration's native pose.
        $curlProfile='vrmod/output/grip-preview-20261004-1530'
        if($LiveInput -and -not $NoFingerCurl -and (Test-Path "$curlProfile/hand-pose.bin")){$probe+=@('--world-hand-curl',$curlProfile)}
        if($DesktopDiscard){$probe+='--world-desktop-discard'}
        if($DesktopSkipVfx){$probe+='--world-desktop-skip-vfx'}
        $probe
}
if($WriteProfile){
    if(!$RealVr -or !$LiveInput){throw '-WriteProfile is for the real-headset play session (-RealVr -LiveInput).'}
    $profileArguments=@(Get-ProbeArguments 'PROFILE' 'PROFILE')+@('--write-profile',$WriteProfile)
    & $py vrmod/tools/run_render_resource_probe.py @profileArguments
    exit $LASTEXITCODE
}
if($Hands -and !$RealVr){& $py vrmod/tools/validate_mock_hands.py $Hands;if($LASTEXITCODE -ne 0){throw 'Mock hand sequence failed native runtime preflight before settings/save/game changes.'}}
if($SettingsFile){& $py vrmod/tools/validate_gi_settings.py $SettingsFile;if($LASTEXITCODE -ne 0){throw 'GI preset validation failed before settings/save changes.'}}
if(Get-Process enshrouded -ErrorAction SilentlyContinue){throw 'The game is already running; close it first.'}
if($SaveReference){& (Join-Path $PSScriptRoot 'restore_scenery_backup.ps1') -Backup $SaveReference}
$cur='enshrouded_local.json';$settingsRecord=$null
# 1.0 (2026-10-16): the game reads its machine settings from Saved Games\keen games\Enshrouded\local_settings.json.
$machineSettings=Join-Path $env:USERPROFILE 'Saved Games\keen games\Enshrouded\local_settings.json';if(Test-Path -LiteralPath $machineSettings){$cur=$machineSettings}
if($SettingsFile){
    $pre=(Get-FileHash $cur).Hash
    $d='vrmod/output/settings-backups/'+(Get-Date -Format 'yyyyMMdd-HHmmss')+'-play-session'
    New-Item -ItemType Directory $d | Out-Null;Copy-Item $cur "$d/enshrouded_local.json";Copy-Item $SettingsFile $cur -Force
    $settingsRecord=@{changed="play session: $SettingsFile applied for the run";backup="$d/enshrouded_local.json";original_sha256=$pre;applied_sha256=(Get-FileHash $cur).Hash;started=(Get-Date -Format o);dir=$d}
    $settingsRecord | ConvertTo-Json | Set-Content "$d/change.json" -Encoding utf8
    "settings applied $($settingsRecord.applied_sha256)"
}
try{
    if($UseGeneration){Use-StoredGeneration $UseGeneration}
    $before=@(Get-ChildItem vrmod/output -Directory -Filter 'game-*' | ForEach-Object Name)
    $extra=@();if($DesktopMirror){$extra+='-DesktopMirror'};if($NoDesktopMirror){$extra+='-NoDesktopMirror'};if($GpuTimingEvery -gt 0){$extra+="-GpuTimingEvery $GpuTimingEvery"};if($VrDlssQuality){$extra+="-VrDlssQuality $VrDlssQuality"};if($HandsOnly){$extra+='-HandsOnly'};if($UiQuad){$extra+='-UiQuad'};if($UiPartial -or $UiQuad){$extra+='-UiPartial'};   # 2026-10-17: the headset's start keeps UI frames with an unreviewed draw (include/evr/auto_start.hpp); the Mock with the UI plate does as well
    if($Interactive){$extra+='-InteractiveWindow'}
    # An explicit settings file is applied whole for the run; otherwise the launcher applies DLSS for VR and restores it.
    if($SettingsFile){$extra+='-VrUpscaler Keep';if($VrSharpness){$extra+="-VrSharpness $VrSharpness"}else{$extra+='-VrSharpness Keep'}}elseif($VrSharpness){$extra+="-VrSharpness $VrSharpness"}
    if($MockNavigate){$extra+='-MockNavigate'};if($MockNavigateHands){$extra+='-MockNavigateHands'}
    if($EyeImageSweep){$extra+='-EyeImageSweep'}
    if($SymmetricFov){$extra+='-SymmetricFov'}
    if($ShadowReceiverPlane){$extra+=('-ShadowReceiverPlane "'+$ShadowReceiverPlane+'"')}
    if($GiProbeFallback){$extra+=('-GiProbeFallback "'+$GiProbeFallback+'"')}
    if($CaptureWorldFinal){$extra+='-RenderDoc'};if($CaptureShaders){$extra+='-CaptureShaders'}
    if($MockVisibilityMask -gt 0){$extra+="-MockVisibilityMask $MockVisibilityMask"};if($NoFovCrop){$extra+='-NoFovCrop'}
    if($LiveInput){$extra+='-MotionHands';if(!$HandsOnly){$extra+='-HandsOnly'}};if($Hands){$extra+="-MockHandSequence $Hands"}
    if($GripTrim){$extra+="-GripTrim $GripTrim"}
    # 2026-10-09 real-device feedback "炎は左目でしか見えない": play sessions borrowed the VFX lights for the right eye but
    # never drew the transparent particles there (only the full quick wheel trial did). Default on with the shared stages.
    $vfxParticles=-not $NoVfxParticles -and -not $NoSharedStages
    if($vfxParticles){$extra+='-VfxTransparentShader'}
    if(-not $XrSynchronous){$extra+='-XrAsyncCompletion'}
    $runtimeArgs=if($RealVr){'-PrepareRealVr'}else{"-PrepareMock -MockPoseSequence $Pose"}
    $argList="-NoProfile -File ./vrmod/launch_diagnostics.ps1 $runtimeArgs -LiveStereo$(if(!$Menu){' -DirectPlay'}) -RenderCpuArenaMiB 1024 -VulkanValidation $VulkanValidation -EyeLumaEvery $LumaEvery -LogNameCap $LogNameCap $($extra -join ' ')"
    # Real device: read-only snapshot of the host VR settings (openvrpaths, steamvr.vrsettings, chaperone, OpenXR
    # registration, persistent environment), compared after the run. SteamVR itself may rewrite its own settings files
    # during a real session, so a difference is reported for review, not treated as a failure of the run.
    $hostSnapshot=$null
    if($RealVr){$hostSnapshot=(& $pw -NoProfile -File ./vrmod/tools/host_vr_settings.ps1 -Action Backup | Select-Object -Last 1);"host VR settings snapshot $hostSnapshot"}
    # The game process takes the launcher's environment: the online switch is read by the hooks there.
    $onlineBefore=$env:EVR_ONLINE_SYNC;if($OnlineSync){$env:EVR_ONLINE_SYNC='1'}
    try{Start-Process -FilePath $pw -ArgumentList $argList -WindowStyle Hidden | Out-Null}finally{$env:EVR_ONLINE_SYNC=$onlineBefore}
    $deadline=(Get-Date).AddSeconds(90);$g=$null
    do{Start-Sleep -Seconds 2;$g=Get-ChildItem vrmod/output -Directory -Filter 'game-*' | Where-Object { $before -notcontains $_.Name -and (Test-Path "$($_.FullName)/launch.json") } | Sort-Object Name | Select-Object -Last 1}while(-not $g -and (Get-Date) -lt $deadline)
    if(-not $g){throw 'launch record not created'}
    $l=Get-Content "$($g.FullName)/launch.json" -Raw | ConvertFrom-Json;$p=Get-Process -Id $l.pid
    if($p.Path -ne (Resolve-Path 'enshrouded.exe').Path -or $p.StartTime.ToUniversalTime().ToFileTimeUtc().ToString() -ne $l.creation_filetime){throw 'launched process identity mismatch'}
    "== $($g.Name) pid $($l.pid)"
    # 2026-10-16: the Mock ran at 4 pairs/s with the GPU at 100% from 16:40 to 17:10 and was back to normal at 17:30 with the
    # same DLLs - the busy processes are kept too (claude-gpu-processes.txt: pmon lines with an SM share), to see who it was.
    $smi=Start-Job -ScriptBlock { param($out,$secs) $end=(Get-Date).AddSeconds($secs); $procs=Join-Path (Split-Path $out -Parent) 'claude-gpu-processes.txt'
        while((Get-Date) -lt $end){ $x=& nvidia-smi --query-gpu=timestamp,memory.used,utilization.gpu,temperature.gpu --format=csv,noheader 2>$null; if($x){Add-Content -Path $out -Value $x}
            $busy=@(& nvidia-smi pmon -c 1 -s u 2>$null | Where-Object { $_ -notmatch '^#' -and $_ -match '^\s*\d+\s+\d+\s+\S+\s+\d+' }); if($busy.Count){Add-Content -Path $procs -Value ((Get-Date -Format 'HH:mm:ss')+' '+(($busy | ForEach-Object { ($_ -replace '\s+',' ').Trim() }) -join ' | '))}
            Start-Sleep -Seconds 10 } } -ArgumentList "$($g.FullName)/claude-gpu-samples.csv",($Minutes*60+400)
    try{
        if($SaveReference){& $py vrmod/tools/check_scenery_save.py $g.FullName $SaveReference;if($LASTEXITCODE -ne 0){throw 'Launch save snapshot differs from comparison reference.'}}
        if($Menu){
            # No world is open yet: the user joins or hosts a session first; the proofs below need the world.
            "READY_FOR_USER $(Get-Date -Format HH:mm:ss): join or host a session in the game, then create $($g.FullName)\start.request"
            $startDeadline=(Get-Date).AddMinutes($StartWaitMinutes)
            while(-not (Test-Path "$($g.FullName)/start.request")){
                if($p.HasExited){throw 'game exited before start.request'}
                if((Get-Date) -ge $startDeadline){throw 'start.request not created in time'}
                Start-Sleep -Seconds 1
            }
            "START_REQUESTED $(Get-Date -Format HH:mm:ss)"
        }
        $w=22-((Get-Date)-$p.StartTime).TotalSeconds;if($w -gt 0){Start-Sleep -Seconds ([int][Math]::Ceiling($w))}
        if($ExpectedWorld){& $py vrmod/tools/check_play_world.py $g.FullName --expected $ExpectedWorld;if($LASTEXITCODE -ne 0){throw 'Native direct-play selected a different world; closing without play input.'}}
        & $py vrmod/tools/run_render_resource_probe.py --launch $g.FullName --world-view-resolution --world-water-feedback --world-culling-history *>&1 | Out-File "$($g.FullName)/play-session-unused.log" -Encoding utf8
        if($LASTEXITCODE -ne 0){throw 'unused resource proof failed (see play-session-unused.log)'}
        $u=(Get-ChildItem $g.FullName -Directory -Filter 'view-resource-probe-*' | Sort-Object Name | Select-Object -Last 1).FullName
        if($UseGeneration){Use-StoredGeneration $UseGeneration | Out-File "$($g.FullName)/play-session-build.log" -Encoding utf8}
        else{
        & $pw -NoProfile -File ./vrmod/build.ps1 -SkipTests *>&1 | Out-File "$($g.FullName)/play-session-build.log" -Encoding utf8
        if($LASTEXITCODE -ne 0){throw 'fresh DLL build failed (see play-session-build.log)'}
        }
        $wait=$StartAfterSeconds-((Get-Date)-$p.StartTime).TotalSeconds;if($wait -gt 0){Start-Sleep -Seconds ([int]$wait)}
        if($Interactive -and !$Menu){
            # 2026-10-06: spawn points are fixed to bases, so the user walks/travels in the visible window first.
            "READY_FOR_USER $(Get-Date -Format HH:mm:ss): waiting for $($g.FullName)\start.request"
            $startDeadline=(Get-Date).AddMinutes($StartWaitMinutes)
            while(-not (Test-Path "$($g.FullName)/start.request")){
                if($p.HasExited){throw 'game exited before start.request'}
                if((Get-Date) -ge $startDeadline){throw 'start.request not created in time'}
                Start-Sleep -Seconds 1
            }
            "START_REQUESTED $(Get-Date -Format HH:mm:ss)"
        }
        $probe=@(Get-ProbeArguments $g.FullName $u)
        # Real device (2026-10-09, game-20261009-103033): the game polls the gamepad only while its window has focus, and the
        # user wears the headset. Mock live input has the same focus requirement (game-20261009-165008).
        # Bring the verified game window to the front once, before the fixture starts.
        $focusJob=$null
        if($RealVr -or $LiveInput){$focusDelay=20;$focusJob=Start-Job -ScriptBlock { param($pw,$root,$launch,$delay) Set-Location -LiteralPath $root; Start-Sleep -Seconds $delay; & $pw -NoProfile -File ./vrmod/tools/focus_game_window.ps1 -Launch $launch *>&1 | Select-Object -Last 1 } -ArgumentList $pw,(Get-Location).Path,$g.FullName,$focusDelay}
        "session running for up to $Minutes min; create <probe dir>/stop.request to end early"
        $probe+='--no-log-echo'   # the events are in <probe dir>/view-resource-probe.jsonl; the log keeps the result lines
        & $py vrmod/tools/run_render_resource_probe.py @probe *>&1 | Out-File "$($g.FullName)/play-session-probe.log" -Encoding utf8
        "PROBE_EXIT=$LASTEXITCODE"
        if($focusJob){"focus: $(Receive-Job $focusJob -Wait -AutoRemoveJob)"}
    }finally{
        & $pw -NoProfile -File ./vrmod/tools/request_game_close.ps1 -Launch $g.FullName -DryRun *>&1 | Select-Object -Last 1
        & $pw -NoProfile -File ./vrmod/tools/request_game_close.ps1 -Launch $g.FullName -WaitSeconds 180 *>&1 | Select-Object -Last 1
        if($p.HasExited -and (Test-Path 'enshrouded.log')){Copy-Item -LiteralPath 'enshrouded.log' -Destination "$($g.FullName)/completed-game.log"}
        Stop-Job $smi -ErrorAction SilentlyContinue;Remove-Job $smi -Force -ErrorAction SilentlyContinue
    }
    & $py vrmod/tools/check_play_session.py $g.FullName --report "$($g.FullName)/play-session-check.json"
    if($LiveInput){
        $generatedProbe=Get-ChildItem -LiteralPath $g.FullName -Directory -Filter 'view-resource-probe-*' | Where-Object {Test-Path -LiteralPath (Join-Path $_.FullName 'world-session.txt')} | Sort-Object Name | Select-Object -Last 1
        if(-not $generatedProbe){throw 'Generated-instance audit has no play-session probe.'}
        & $py vrmod/tools/check_culling_history.py $generatedProbe.FullName --rendered --generated-only
        if($LASTEXITCODE -ne 0){throw 'Generated-instance count sharing audit failed (generated-instance-checks.json).'}
    }
    & $py vrmod/tools/check_active_renderer.py $g.FullName
    if($LASTEXITCODE -ne 0){
        if($CaptureWorldFinal){Write-Warning 'GPU capture renderer differs from requested mode; see active-renderer-check.json. Do not treat it as a DLSS visual comparison.'}
        else{throw 'Native renderer differs from requested mode (active-renderer-check.json).'}
    }
    if($ShadowReceiverPlane){& $py vrmod/tools/check_shadow_receiver.py $g.FullName;if($LASTEXITCODE -ne 0){throw 'Shadow receiver shader audit failed.'}}
    if($GiProbeFallback){& $py vrmod/tools/check_gi_probe.py $g.FullName;if($LASTEXITCODE -ne 0){throw 'GI probe shader audit failed.'}}
    if($FogProjection){& $py vrmod/tools/check_fog_projection.py $g.FullName;if($LASTEXITCODE -ne 0){throw 'Fog projection audit failed.'}}
    if($ShadowProjection){& $py vrmod/tools/check_shadow_projection.py $g.FullName;if($LASTEXITCODE -ne 0){throw 'Native shadow projection audit failed.'}}
    if($DetailBindings){& $py vrmod/tools/check_detail_bindings.py $g.FullName;if($LASTEXITCODE -ne 0){throw 'Private detail binding audit failed (detail-bindings-check.json).'}}
    if($CaptureWorldFinal){$captureCheck=@('--pair',"$CapturePair");if($CaptureOnRequest){$captureCheck=@('--manual')}; & $py vrmod/tools/check_world_capture.py $g.FullName @captureCheck;if($LASTEXITCODE -ne 0){throw 'Requested RenderDoc capture was not completed (world-capture-check.json).'}}
    if($hostSnapshot){
        $verify=& $pw -NoProfile -File ./vrmod/tools/host_vr_settings.ps1 -Action Verify -Snapshot $hostSnapshot *>&1
        $verify | Out-File "$($g.FullName)/host-vr-verify.txt" -Encoding utf8
        $differ=@($verify | Where-Object { "$_" -like 'FAIL *' })
        "host VR settings: $(@($verify | Where-Object { "$_" -like 'PASS *' }).Count) unchanged, $($differ.Count) changed$(if($differ){' (review: '+(($differ|ForEach-Object{"$_".Substring(5)}) -join '; ')+')'})"
    }
}finally{
    if($settingsRecord){
        for($i=0;$i -lt 120 -and (Get-Process enshrouded -ErrorAction SilentlyContinue);$i++){Start-Sleep -Seconds 2}
        if(Get-Process enshrouded -ErrorAction SilentlyContinue){"game still running - settings NOT restored ($($settingsRecord.backup))"}
        else{Copy-Item $settingsRecord.backup $cur -Force;$settingsRecord.restored_sha256=(Get-FileHash $cur).Hash;$settingsRecord.ended=(Get-Date -Format o)
            $settingsRecord | ConvertTo-Json | Set-Content "$($settingsRecord.dir)/change.json" -Encoding utf8;"settings restored $($settingsRecord.restored_sha256)"}
    }
}
