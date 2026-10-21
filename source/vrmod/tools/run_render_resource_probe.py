"""Bounded native resource/View diagnostics and optional stereo calibration.

Synthetic/recorded calibration and explicitly requested live OpenXR game-image
submission are separate diagnostic modes, not a playable VR mod.
"""
import argparse
import hashlib
import json
import pathlib
import subprocess
import time
from inject_camera_observer import inject, creation_filetime, Process


def retained_scene_probe(launch):
    # A completed drawing probe keeps private resources + cleanup hooks until
    # native Scene disposal. Fresh DLL globals do not make a second injection
    # independent. Require a fresh managed process for the next drawing trial.
    for log in sorted(launch.glob('*-resource-probe-*/view-resource-probe.jsonl')):
        for line in log.read_text().splitlines():
            try:event=json.loads(line)
            except json.JSONDecodeError:continue
            if event.get('event') in ('calibration_retained_until_scene_disposal',
                    'calibration_retained_until_process_exit','calibration_pair_retained_on_failure'):
                return log.parent
    return None


def postcheck(launch, output, started, crashes_before, game, filename='probe-postcheck.json'):
    """A native callback returning zero does not prove the game survived it."""
    # Periodic game_frame records occur every 120 presents. Under validation,
    # ten seconds may contain two healthy samples less than four seconds apart.
    # Keep the four-second evidence requirement; extend observation only when
    # necessary, up to thirty seconds. Never interpret missing samples as PASS.
    def telemetry():
        events=[]
        for line in (output.parent/'vulkan.jsonl').read_text(encoding='utf-8-sig').splitlines():
            try:events.append(json.loads(line))
            except json.JSONDecodeError:pass # final line may be in flight
        return events
    initial=[e for e in telemetry() if e.get('event')=='game_frame']
    progress_after=max(started,initial[-1]['us'] if initial else started)
    observed_at=time.monotonic()
    deadline=observed_at+30
    alive=True
    process_error=None
    frames=[];fresh=False
    while time.monotonic()<deadline:
        try:
            proc=Process(launch['pid'],game)
            try:alive=creation_filetime(proc.handle)==int(launch['creation_filetime'])
            finally:proc.close()
        except OSError as error:
            # Lack of observation access is not evidence of a crash or exit.
            alive=False;process_error=dict(errno=error.errno,operation=str(error))
        if not alive:break
        if time.monotonic()-observed_at>=10:
            frames=[e for e in telemetry() if e.get('event')=='game_frame' and e['us']>progress_after]
            fresh=len(frames)>1 and frames[-1]['frame']>frames[0]['frame'] and frames[-1]['us']-frames[0]['us']>=4000000
            if fresh:break
        time.sleep(.2)
    frames=[e for e in telemetry() if e.get('event')=='game_frame' and e['us']>progress_after]
    fresh=len(frames)>1 and frames[-1]['frame']>frames[0]['frame'] and frames[-1]['us']-frames[0]['us']>=4000000
    new_crashes={p.name for p in (game.parent/'crash_dumps').iterdir()}-crashes_before
    result=dict(expected_process_alive=alive,process_observation_error=process_error,
                rendering_continued_four_seconds=fresh,new_crash_directories=sorted(new_crashes),
                crash_baseline=sorted(crashes_before),observed_seconds=time.monotonic()-observed_at,
                observation_start_frame_us=progress_after,
                frame_samples=len(frames),first_frame_us=frames[0]['us'] if frames else None,last_frame_us=frames[-1]['us'] if frames else None,
                status='PASS' if alive and fresh and not new_crashes else 'FAIL',limit='10..30-second observation requiring four seconds between fresh recorded frames; not proof of long-term stability')
    (output/filename).write_text(json.dumps(result,indent=2),encoding='utf8')
    print(result)
    return result['status']=='PASS'


def user_settings_path():
    # 1.0 (2026-10-16): the user settings (the gamepad bindings among them) are the Steam cloud file user_settings.json of
    # the save folder; the game migrated Saved Games\Enshrouded\enshrouded_user.json away. The folder is the one the
    # newest verified save backup names; the old file where there is none.
    import glob,os
    for record in sorted(glob.glob('vrmod/output/save-backups/*/backup.json'),reverse=True):
        try:r=json.loads(pathlib.Path(record).read_text(encoding='utf-8-sig'))
        except Exception:continue
        source=r.get('source')
        if r.get('verified') and source and (pathlib.Path(source)/'user_settings.json').exists():return pathlib.Path(source)/'user_settings.json'
    return pathlib.Path(os.environ['USERPROFILE'])/'Saved Games/Enshrouded/enshrouded_user.json'

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--launch',type=pathlib.Path)
    p.add_argument('--self-test',action='store_true')
    p.add_argument('--view',action='store_true',help='Test unused native View constructor/destructor; temporarily selects Scene renderer back-pointer')
    p.add_argument('--detail-factory',action='store_true',help='Two unused detail objects only, with scoped array-reserve diagnostics; no View/Scene pointer changes or eye rendering')
    p.add_argument('--world-view-features',action='store_true',help='Explicit unused world feature-object diagnostic, with private SSR and VFX excluded; no resolution initialization or eye rendering')
    p.add_argument('--world-view-resolution',action='store_true',help='Explicit unused world View resolution diagnostic with original detail factory observation; no eye rendering')
    p.add_argument('--view-features',action='store_true',help='Also initialize native View feature resources; still performs no eye rendering')
    p.add_argument('--view-isolated-features',action='store_true',help='Unused feature resources excluding scene-owned VFX; original game VFX unchanged')
    p.add_argument('--view-resolution',action='store_true',help='Also configure unused View resolution resources with private owners; VFX excluded')
    p.add_argument('--native-calibration',action='store_true',help='Record one synthetic stereo pair in native Views, VFX excluded; requires RenderDoc, not an XR mod')
    p.add_argument('--retained-calibration',action='store_true',help='One native stereo calibration; retains its two Views/resources until game exit; requires RenderDoc')
    p.add_argument('--scene-lifetime-calibration',action='store_true',help='Retained calibration with resource disposal attached to native Scene owner destruction')
    p.add_argument('--native-series',action='store_true',help='300 native pairs reusing two Views/targets, synthetic pose sequence, Scene-lifetime cleanup')
    p.add_argument('--recorded-xr',type=pathlib.Path,help='Offline recorded Runtime eye poses/FOVs/extents; does not create an XR session')
    p.add_argument('--prepared-xr-session',action='store_true',help='Three empty OpenXR frames on prepared real game device, under native queue gates; no eye rendering')
    p.add_argument('--live-xr',action='store_true',help='One real native game stereo pair using live Runtime poses and XR GPU image submission')
    p.add_argument('--world-live-xr',type=pathlib.Path,help='World diagnostic; requires the passed unused-resolution probe directory from this managed launch')
    p.add_argument('--world-shared-updates-once',action='store_true',help='Experimental: execute shared brick/water updates only in the original view of the same game frame; permits up to 300 world pairs')
    p.add_argument('--world-share-vfx-models',action='store_true',help='With --world-share-vfx-lights: the eye without the VFX culls the VFX models for itself (aim circle of the pickaxe, the blue area of building mode in both eyes)')
    p.add_argument('--world-share-vfx-decals',action='store_true',help='With --world-share-vfx-lights: the eye without the VFX also borrows the decal list of the VFX owner (aim circles on the ground in both eyes)')
    p.add_argument('--world-share-vfx-lights',action='store_true',help='Opt-in causal experiment: borrow same-frame original point-light buffer/counter after private reset; original-frustum limitations apply')
    p.add_argument('--world-early-xr',action='store_true',help='Opt-in: obtain Runtime frame before original shared world rendering, after the first bootstrap frame')
    p.add_argument('--world-scenery-replay',type=pathlib.Path,help='Mock only: pinned draw-only scenery camera reference')
    p.add_argument('--world-scenery-no-hzb',action='store_true',help='Diagnostic: disable only private asymmetric main-view HZB rejection')
    p.add_argument('--world-scenery-culling',action='store_true',help='Read-only private culling constant capture; requires head visibility')
    p.add_argument('--world-fog-projection',choices=['observe','match','compare'],help='Opt-in matching of native fog history projection to the owned XR eye')
    p.add_argument('--world-shadow-projection',action='store_true',help='Opt-in native shadow perspective input conversion')
    p.add_argument('--world-detail-bindings',action='store_true',help='Opt-in private detail shader owner correction')
    p.add_argument('--world-eye-frustum',choices=['observe','stable'],help='Diagnostic: compare native/analytic private-eye frusta; stable applies bounded analytic planes')
    p.add_argument('--world-shared-culling-camera',action='store_true',help='Opt-in: use an HMD-oriented camera enclosing both eyes for original/shared rendering; the helper camera is never an XR eye')
    p.add_argument('--world-vfx-transparent-observe',action='store_true',help='Read-only same-frame transparent VFX source/eye counter and position observations')
    p.add_argument('--world-vfx-transparent-render',action='store_true',help='Experimental: append rebased transparent VFX and enable only the private transparent draw preparation branch')
    p.add_argument('--world-water-feedback',action='store_true',help='Opt-in: private per-eye native download pools and same-serial water feedback aggregation; supports separate unused resolution proof')
    p.add_argument('--world-culling-history',action='store_true',help='Opt-in: independent native visibility history per eye, with instance replacement resets; separate unused lifecycle proof first')
    p.add_argument('--world-culling-fill-only',action='store_true',help='Ablation: initialize private history buffers but preserve all original culling descriptors')
    p.add_argument('--world-root-camera',choices=['observe','first-person'],help='Read native stable root/unshaken camera; optionally anchor bounded eye views at calibrated root height')
    p.add_argument('--world-camera-mode-fixture',action='store_true',help='TEST ONLY: render-only first/event/special/unknown/first transitions; no ECS mode writes')
    p.add_argument('--world-camera-modes',action='store_true',help='Experimental: input-only yaw and native event/special cameras; preserves head tracking')
    p.add_argument('--world-input-observe',action='store_true',help='Read-only local movement camera/input/state observations; no input modifications')
    p.add_argument('--world-input-head-idle',action='store_true',help='Explicit bounded experiment: change private native camera input from HMD only while idle; not gameplay controls')
    p.add_argument('--world-controller-jump-fixture',choices=['single','double','glide'],help='TEST ONLY: actual in-place jump/airborne second jump and camera stabilization')
    p.add_argument('--world-controller-altar-fixture',action='store_true',help='TEST ONLY: actual Flame Altar interaction and native event camera return; no upgrades')
    p.add_argument('--world-controller-fixture-after',type=int,default=None,help='Arm synthetic controller fixtures after N submitted pairs (30..3000, default 30)')
    p.add_argument('--world-hand-curl',type=pathlib.Path,help='Finger curl from the controllers (2026-10-09, EDF6VR-style): grip preview directory whose hand-pose.bin is the closed hand (hash checked against its profile.json)')
    p.add_argument('--world-hand-curl-relaxed',type=float,default=.15,help='Finger curl with no input (EDF6VR HandRelaxed, 0..0.9)')
    p.add_argument('--world-weapon-pitch',type=float,default=None,help='Live weapon-only local pitch in degrees, -90..90; default VHVR reference -35')
    p.add_argument('--world-live-input',action='store_true',help='Continuous play input from the VR controllers (real-device phase 1, 2026-10-09; table in include/evr/live_pad.hpp). Needs tracked hands, input observation and the walking head camera; excludes every controller fixture')
    p.add_argument('--world-controller-menu-tour',action='store_true',help='TEST ONLY: open the menu, visit every tab (LB x3 then RB x5) capturing each, close (VR menu check, 2026-10-06)')
    p.add_argument('--world-controller-tabs-fixture',action='store_true',help='TEST ONLY: native controller selection/RB/LB UI navigation and return')
    p.add_argument('--world-controller-menu-fixture',action='store_true',help='TEST ONLY: native controller menu open/close after30XR pairs, capture both UI states')
    p.add_argument('--world-input-head-walking',action='store_true',help='Explicit bounded ordinary analog gamepad walking camera; digital actions and special locomotion fall back')
    p.add_argument('--world-input-walk-fixture',action='store_true',help='TEST ONLY: synthesize four short analog walking pulses in private input; requires walking policy, idle native controls, validated state and tracking')
    p.add_argument('--world-active-xr-exit',action='store_true',help='Finite renderer-only trial ending via native Scene disposal during active XR; requires normal UI process exit')
    p.add_argument('--world-eye-private-transient',action='store_true',help='(default with live world XR since 2026-10-06) Eye Views get private transient resolution pools (renderer+19c is shared by all Views otherwise)')
    p.add_argument('--world-gi-share',action='store_true',help='Opt-in (default 2026-10-06..10-09): the right eye uses the left eye GI probes and only re-runs its own screen-space GI. With a moving head the right eye shadows come out darker (real-device feedback and Mock yaw sweep game-20261009-111149), so it is no longer the default')
    p.add_argument('--world-gi-share-keep',type=lambda s:int(s,0),default=0,help='Diagnostic: GI share stage bits that still run in the right eye (e.g. 0xF000 = the four ambient-probe stages)')
    p.add_argument('--world-gi-own',action='store_true',help='Each eye updates its own GI probes (default since 2026-10-09; the flag is kept for older commands)')
    p.add_argument('--world-keep-dof',action='store_true',help='Keep the game depth of field in the eyes (VR default since 2026-10-06: depth of field off)')
    p.add_argument('--world-game-vignettes',action='store_true',help="Opt-out: keep the game's status vignettes (HUD frost/blur widgets on the HUD quad, post vignette on all eye edges) instead of the VR rim tint (default since 2026-10-06)")
    p.add_argument('--world-status-tint',type=float,default=0.5,help='Strength 0..1 of the VR status rim tint (low health red, freezing blue; default 0.5, 0 = no rim, HUD frost/blur still hidden)')
    p.add_argument('--world-force-freezing',type=float,default=None,help='Diagnostic: force the freezing vignette amount the HUD widget reads (aa9270), to check its VR presentation without a cold area')
    p.add_argument('--world-vr-sharpness',default=None,help='Eye sharpen strength 0..1 written into the eye render parameters, or game (default: launch.json vr_sharpness from launch_diagnostics -VrSharpness)')
    p.add_argument('--world-object-watch',action='store_true',help='Read-only diagnostic: log which bytes of the fog object each fog function writes (pairs 300..302)')
    p.add_argument('--world-gi-dump',action='store_true',help='Read-only diagnostic: dump both eye GI objects (View+950), eye Views and frame graph resource tables on pairs 300/301 to <probe>/gi-dump')
    p.add_argument('--world-desktop-compare',action='store_true',help='Mock-only requested desktop skip/render comparison at fixed scenery camera')
    p.add_argument('--world-desktop-render',action='store_true',help='Opt-out: keep rendering the original (desktop) View every frame (periodic desktop refresh is the default for live world series since 2026-10-10)')
    p.add_argument('--world-desktop-skip-vfx',action='store_true',help='Experiment with --world-desktop-skip: on skipped frames the left eye temporarily owns the singleton VFX View (right eye has no VFX)')
    p.add_argument('--world-desktop-discard',action='store_true',help='Opt-in experiment (2026-10-06): the original (desktop) View renders normally but its result is replaced by the empty result, so the game does not consume the desktop image')
    p.add_argument('--world-desktop-skip',type=int,default=None,metavar='EVERY',help='Skip the original (desktop) View world render on frames whose eye pair renders; eye 0 runs the shared updates (and owns VFX when the eyes borrow VFX). 0 = skip every frame (diagnostic; stale GI after movement), N>=2 = render the desktop 1 of N (default: 30)')
    p.add_argument('--world-eye-shared-transient',action='store_true',help='Diagnosis only: eye Views keep the renderer-wide transient pool (renderer+19c); DLSS eyes go black once the pool overflows')
    p.add_argument('--world-eye-dealias',action='store_true',help='Eye View resources built with dedicated device memory (no shared transient pool offsets between eyes)')
    p.add_argument('--world-dlss-observe',action='store_true',help='Read-only DLSS evaluate observation (0xdbe9f0): per-call output image/size, render subrect, reset, result')
    p.add_argument('--desktop-render-scale',type=float,default=None,help='Original (desktop) View game RenderScale 0.25..1.0 while live XR runs (wrapper+0x20); restored at teardown')
    p.add_argument('--eye-render-scale',type=float,default=None,help='Eye View game RenderScale 0.25..1.0 (resolution owner +0x20); the game upscaler per the user graphics setting reconstructs the XR target')
    p.add_argument('--world-session',type=int,metavar='MINUTES',help='Play session on top of --world-endurance: no pair limit; ends at <probe>/stop.request or after MINUTES (1..240); logs are name-capped')
    p.add_argument('--world-endurance',action='store_true',help='Explicit 301..3000-pair world diagnostic with a 420-second native deadline; fresh launch required')
    p.add_argument('--world-controller-motion-burst',type=int,default=0,help='TEST ONLY: capture1..32 consecutive stereo pairs from the first jump input')
    p.add_argument('--world-body-clip-distance',type=float,help='Explicit experimental near-body clip distance; requires prepared shaders and local parts')
    p.add_argument('--world-motion-tracking-loss',action='store_true',help='TEST ONLY: expect one post-request right-hand tracking loss from mock CSV; cancellation must be independently audited')
    p.add_argument('--world-motion-attack',action='store_true',help='TEST ONLY: one grip swing triggers one native PresetA X attack; requires motion hands, mock and endurance')
    p.add_argument('--world-motion-two-hand',type=pathlib.Path,help='One calibrated two-hand native attack gate; requires successful visual preview proof, excludes tracked Collider changes')
    p.add_argument('--world-motion-head-interlock',action='store_true',help='TEST ONLY: same-frame temple gesture inhibits one calibrated swing; no menu commands or Collider replacement')
    p.add_argument('--world-motion-right-speed',action='store_true',help='TEST ONLY: requires controller-speed; left-only must not start, right-only must start')
    p.add_argument('--world-motion-controller-speed',action='store_true',help='TEST ONLY: held-controller wrist twists then one translation; requires two-hand gate, excludes collider/repeat/loss')
    p.add_argument('--world-motion-repeat',action='store_true',help='TEST ONLY: exactly two archived wrist swings, separate native actions and tracked colliders')
    p.add_argument('--world-motion-recovery-swing',action='store_true',help='TEST ONLY: add a separate fast return swing during native recovery; requires repeat')
    p.add_argument('--world-motion-event-retime',type=pathlib.Path,help='One tracked swing with early native Impact; requires successful native retime proof')
    p.add_argument('--world-motion-two-hand-early',type=pathlib.Path,help='Calibrated early Impact integration; requires successful calibrated native-window physics proof')
    p.add_argument('--world-motion-offhand-release',type=pathlib.Path,help='Bounded genuine offhand grab release; requires successful calibrated early Impact proof')
    p.add_argument('--world-motion-collider-cancel-after',type=int,help='Test-only native cancellation after 1..10 applied collider updates')
    p.add_argument('--world-motion-collider-apply',type=pathlib.Path,help='Apply tracked rigid pose during one native attack, requiring a successful shadow proof directory')
    p.add_argument('--world-motion-collider-shadow',action='store_true',help='Read-only rendered-weapon pose proposal at native collision boundary')
    p.add_argument('--world-motion-contact-observe',action='store_true',help='Read-only native Impact VM geometry observation during explicit one-swing diagnostic')
    p.add_argument('--world-motion-guard',action='store_true',help='TEST ONLY: calibrated hand postures drive two native LB holds, without attacks')
    p.add_argument('--world-cancel-contact-recycle',type=pathlib.Path,help='FINITE TEST: reclaim one post-Impact offhand cancellation after native destruction and cleared/quiet Actor; requires prior 445 proof')
    p.add_argument('--world-blocked-event-recycle',type=pathlib.Path,help='FINITE TEST: reclaim one blocked Impact event only after its authored suppression and native cleared/quiet Actor boundary')
    p.add_argument('--world-epoch-strict',type=pathlib.Path,help='Finite measured enforcement using immutable client/server epochs; requires validated shadow proof')
    p.add_argument('--world-contact-end-retire',action='store_true',help='Finite bridge: keep completed native action values until actual World exit after 600pairs; not active attack exit')
    p.add_argument('--world-contact-owner-exit',action='store_true',help='Join completed-action terminal bridge with reader-stopped native input owner handoff; no active combat exit claim')
    p.add_argument('--world-epoch-cancel',action='store_true',help='Close only MOD client admission during first native Impact; keep Worlds alive for retirement')
    p.add_argument('--world-epoch-proof',type=pathlib.Path,help='Completed native five-boundary role shadow proof for strict mode')
    p.add_argument('--world-epoch-shadow',type=pathlib.Path,help='Finite read-only role comparison against the initial XR World broker publisher; requires session/contact recycle')
    p.add_argument('--world-contact-recycle',type=pathlib.Path,help='FINITE TEST: reuse one ledger slot after proven native action/Impact completion; requires exact prior quiescence proof')
    p.add_argument('--world-contact-quiescence',action='store_true',help='READ ONLY: sample Actor/queue state at the exact native command-flush return')
    p.add_argument('--world-contact-lifetimes',action='store_true',help='READ ONLY: observe owned Impact entity destruction, never reclaim ledger entries')
    p.add_argument('--world-contact-log-budget',action='store_true',help='TEST ONLY: regular event/ready logs stop at3/2; separate witness records prove processing continues')
    p.add_argument('--world-melee-session-contact',type=pathlib.Path,help='Combine measured two-action coordinator proof with existing calibrated early Impact/physics proofs')
    p.add_argument('--world-melee-session-repeat',action='store_true',help='Two measured right swings with a rejected recovery swing; requires melee-session, no collider changes')
    p.add_argument('--world-melee-session',action='store_true',help='Test the unlimited melee coordinator under the existing one-action right-speed fixture')
    p.add_argument('--world-guard-fixture',action='store_true',help='TEST ONLY: two finite native LB holds and block/release observation, no motion attack')
    p.add_argument('--world-motion-equipment-switch',action='store_true',help='TEST ONLY: finite Dpad left/right native hotbar switch with render-only calibrated resource rebinding')
    p.add_argument('--world-quick-wheel',action='store_true',help='TEST ONLY: hold RB, native radial stick selection and neutral rearm')
    p.add_argument('--world-quick-wheel-consumable',action='store_true',help='TEST ONLY with --world-quick-wheel: select the right-slot consumable then the up-right weapon (v2 schedule); observation only, never sends X')
    p.add_argument('--world-grip-router-equip',action='store_true',help='TEST ONLY with --world-quick-wheel: router zone presses execute equip requests only through the native wheel toward planned weapon slots')
    p.add_argument('--world-grip-router-equip-overlap',action='store_true',help='TEST ONLY with --world-grip-router-equip: shoulder presses made with the hand resting at its temple (gesture active), v2 head-relative shoulder reach')
    p.add_argument('--world-head-gesture-left',action='store_true',help='TEST ONLY with --world-head-gesture: the LEFT hand at the left temple drives the same menu-only fixture')
    p.add_argument('--world-grip-router-wheel',action='store_true',help='TEST ONLY with --world-quick-wheel: RB only from GripRouter long press; short and shoulder-zone presses must not open the wheel; equip requests are logged, never executed')
    p.add_argument('--world-quick-wheel-survey',action='store_true',help='TEST ONLY with --world-quick-wheel: select five weapon/tool slots and log every SlotSelection change read-only; never a consumable or face button')
    p.add_argument('--world-quick-wheel-rebind',action='store_true',help='TEST ONLY with --world-quick-wheel: three native wheel selections (up-right/up/up-right) for retained private weapon suspend/rebind')
    p.add_argument('--world-vertical-stick',action='store_true',help='TEST ONLY: XR right stick up twice and down once -> native L3/B, no attacks')
    p.add_argument('--world-head-gesture',action='store_true',help='TEST ONLY: right temple gesture opens/navigates/closes native menu, no movement or confirmation')
    p.add_argument('--world-grip-preview',type=pathlib.Path,help='Measured local grip preview directory; visual only, refuses combat fixtures')
    p.add_argument('--world-bow-spawn-observe',action='store_true',help='Mock-only read-only native Crosshair spawn observer')
    p.add_argument('--world-bow-tracked-aim',action='store_true',help='Mock one physical release with private two-hand aim')
    p.add_argument('--world-bow-draw',action='store_true',help='Mock VHVR-style repeatable draw on the tracked aim route (bow_draw.hpp)')
    p.add_argument('--world-ranged-survey',action='store_true',help='Mock survey: left primary -> native X, Crosshair witness logs other event types')
    p.add_argument('--world-ranged-aim',action='store_true',help='Mock VHVR-style staff/wand: emitter on the weapon axis, pointer-ray direction, wand trigger -> attack')
    p.add_argument('--world-weapon-trial-real',action='store_true',help='Explicit: allow the Mock-reviewed weapon display/bow/staff routes on a real headset (supervised trial)')
    p.add_argument('--world-live-melee-targets',action='store_true',help='With --world-live-melee and the early Impact: a stroke attacks only while the blade is near something that has Health (VHVR: the weapon has to reach a target; a stroke through the air does nothing)')
    p.add_argument('--world-live-melee-heavy-hold',type=int,help='With --world-live-melee: milliseconds (0..3000) the attack button is held for a stroke that VHVR takes as the secondary attack (the game then starts the heavy attack); not given: the hook default of 400; 0: every stroke stays a short press')
    p.add_argument('--world-live-melee-heavy-motion',action='store_true',help='With --world-live-melee: a stroke that VHVR takes as the secondary attack by its motion alone is a heavy attack too (default: only a stroke with the right trigger held)')
    p.add_argument('--world-live-melee-travel',type=float,help='With --world-live-melee: metres (0..1) a stroke has to cover above the required speed before it attacks; default 0, at once')
    p.add_argument('--world-live-melee-heavy-factor',type=float,help='With --world-live-melee: factor (1..2, VHVR 1.5) on the 3 m/s swing speed that a heavy weapon needs')
    p.add_argument('--world-live-melee',action='store_true',help='Live input: a VHVR-style swing of the shown melee weapon becomes one native attack press')
    p.add_argument('--world-body-heading',action='store_true',help='Live input: the local character faces where the headset looks (the native look selection takes the camera during ordinary ground play)')
    p.add_argument('--world-live-held-follow',action='store_true',help='Experiment with the live held items: the game entity of the item in the tracked right hand follows that hand, so its light and effects appear at the tracked item')
    p.add_argument('--world-live-held-items',action='store_true',help='With the weapon display: tools (item slot 3) are shown and swung like melee weapons, and every catalog shield is shown')
    p.add_argument('--world-live-melee-contact',choices=('apply','observe','early','early-observe','early-still','early-observe-still'),help='With live melee and the weapon display: the attack collider follows the tracked weapon (early: the Impact starts on the first tick of the attack; observe: adds the read-only physics-query and hit observers; still: the attack plays no animation and does not turn or move the character)')
    p.add_argument('--world-ui-auto-hide',action='store_true',help='Live input with the UI quad: the HUD fades out five seconds after it last changed; what the game pins to things in the world stays')
    p.add_argument('--world-menu-pointer',action='store_true',help='Live input with the UI quad: while a menu is open the right hand pointing at the UI plate is the mouse (trigger left click, B right click); a dot shows the place')
    p.add_argument('--world-quick-slot-motion',action='store_true',help='Live input: right B held opens the native quick wheel, the right hand points at an item, release takes it; the right grip is free')
    p.add_argument('--world-live-guard',action='store_true',help='Live input: a raised shield (or two-hand weapon held across) holds the native block button')
    p.add_argument('--world-bow-physical-release',action='store_true',help='Mock physical draw release, native aim/power retained')
    p.add_argument('--world-bow-physical-pose',action='store_true',help='Private visual bow deformation using physical distance')
    p.add_argument('--world-bow-physical-cancel',action='store_true',help='Mock-only near-string physical bow no-shot trial')
    p.add_argument('--world-bow-cancel-trial',action='store_true',help='Mock-only LT charge then no-shot cancellation')
    p.add_argument('--world-bow-release-trial',action='store_true',help='Mock-only LT charge during draw and a single RT tap on release')
    p.add_argument('--world-bow-aim-trial',action='store_true',help='Mock-only LT aim plus one RT hold/release, requires bow input trial')
    p.add_argument('--world-item-catalog',action='store_true',help='Mock read-only resident item/model-reference catalog')
    p.add_argument('--world-ranged-animation',action='store_true',help='Mock reviewed staff/wand private animated poses; requires bow animation survey')
    p.add_argument('--world-bow-animation',action='store_true',help='Mock known-bow private animated pose, requires native survey')
    p.add_argument('--world-shield-mount',action='store_true',help='Mock measured shield private display; requires native survey')
    p.add_argument('--world-bow-input-trial',action='store_true',help='Mock-only one observed-bow RT to native PresetA X press/release trial')
    p.add_argument('--world-two-hand-wield',action='store_true',help='Explicit live display trial: measured melee grip sliding; no combat authorization')
    p.add_argument('--world-two-hand-wield-vhvr',action='store_true',help='With --world-two-hand-wield: VHVR entry rule (left grip, hands along the weapon) instead of the measured-contact trial')
    p.add_argument('--world-body-ik',action='store_true',help='Experiment: local body parts on a mod-owned pose (animated joints, hands at the controllers); needs live input')
    p.add_argument('--world-two-hand-catalog-only',action='store_true',help='Diagnostic, with the VHVR entry: measured melee assets also take the catalog group mount')
    p.add_argument('--world-native-grip-observe',action='store_true',help='Read-only same-frame original hand/weapon snapshots; requires private hand diagnostic')
    p.add_argument('--world-motion-hands',action='store_true',help='TEST ONLY: private native hand skeleton following both grips; requires motion visual')
    p.add_argument('--world-motion-visual',action='store_true',help='TEST ONLY: one owned native weapon at OpenXR right-grip pose, private eyes only; requires MotionHands/HandsOnly launch')
    p.add_argument('--world-hands-only',action='store_true',help='Local first-person hands/weapons only, preserving native events/UI/shadows; requires HandsOnly launch')
    p.add_argument('--world-ui-state-observe',action='store_true',help='Read-only scoped native UI dispatch observation; does not switch rendering')
    p.add_argument('--world-body-motion-clip-distance',type=float,help='Optional validated Actor jump/fall/glide/dodge body clip; requires walking observer; exclusive with glide clip')
    p.add_argument('--world-body-glide-clip-distance',type=float,help='Optional explicit Camera25-only body clip experiment; keeps base distance in other postures')
    p.add_argument('--world-body-clip-parts',type=int,nargs='+',default=[16],help='Local body equipment parts for the clip experiment; default upper armour only')
    p.add_argument('--world-body-upload-observe',action='store_true',help='Read-only scoped depth/color constant uploads; no live clipping')
    p.add_argument('--world-held-vfx-mount',action='store_true',help='Opt-in scoped torch emitter pose from validated live weapon')
    p.add_argument('--world-held-vfx-observe',action='store_true',help='Read-only held effect ownership at the completed VFX query')
    p.add_argument('--world-attachments-observe',action='store_true',help='Read-only local equipment child ownership and model observations')
    p.add_argument('--world-equipment-observe',action='store_true',help='Read-only bounded local body/equipment model handles; no mesh visibility changes')
    p.add_argument('--world-head-visibility',action='store_true',help='Private main-subview head exclusion, using continuous validated equipment handles and a prepared dynamic shader')
    p.add_argument('--capture-glide',action='store_true',help='RenderDoc capture on first native Camera25 during explicit glide fixture')
    p.add_argument('--capture-world-final',action='store_true',help='Capture the final original/eye render passes locally under a managed RenderDoc launch')
    p.add_argument('--capture-on-request',action='store_true',help='Mock navigation: arm one GPU capture at world-capture.request')
    p.add_argument('--capture-initial-fog-sdf',action='store_true',help='Capture one initial left-eye fog SDF upload after native CPU completion, including warmup frames')
    p.add_argument('--live-frames',type=int,default=1)
    p.add_argument('--write-profile',type=pathlib.Path,help='No game: write the policy files this live world session would be given into this (new or empty) folder - the package\'s profile, which the loader (loader/loader.cpp) copies into each session (tools/pack_release.ps1)')
    p.add_argument('--no-log-echo',action='store_true',help='Do not print the probe log to stdout: it stays in the output folder as view-resource-probe.jsonl (a play session copied it into play-session-probe.log, 50 to 100 MB a run)')
    p.add_argument('--render-scale',type=float,default=1.0)
    p.add_argument('--sync-camera-fov',action='store_true',help='Diagnostic: set private Camera FOV envelope as well as exact per-eye projection')
    p.add_argument('--world-active-contact-owner',action='store_true',help='Completed native action/input owner with renderer active until real Scene exit')
    p.add_argument('--world-render-hook-batch',action='store_true',help='Explicit 12-hook render teardown batching after Scene and GPU completion')
    p.add_argument('--world-interrupted-actor-owner',action='store_true',help='Explicit active native Actor destruction after owned Impact/suppression, then genuine World end')
    args=p.parse_args()
    if args.world_interrupted_actor_owner and not args.world_active_contact_owner:p.error('interrupted Actor owner requires explicit active contact owner')
    if args.world_render_hook_batch and not args.world_active_contact_owner:p.error('render hook batch requires active contact owner')
    if args.world_active_contact_owner and (not args.world_active_xr_exit or not args.world_contact_owner_exit or not args.world_contact_end_retire or not args.world_epoch_cancel or not args.world_motion_attack or args.live_frames!=3000):p.error('active contact owner requires explicit active Scene, terminal, owner and completed-action cancellation policies')
    if args.world_active_xr_exit and (not args.world_endurance or not args.world_live_xr or args.live_frames!=3000 or (args.world_motion_attack and not args.world_active_contact_owner) or args.world_controller_menu_fixture or args.world_controller_jump_fixture or args.world_controller_tabs_fixture or args.world_controller_altar_fixture or args.world_head_gesture or args.world_motion_equipment_switch or args.world_motion_guard):p.error('active XR exit requires isolated renderer with 3000-pair ceiling and no input fixture')
    if args.world_endurance and (not args.world_live_xr or not args.world_shared_updates_once or not 301<=args.live_frames<=3000):p.error('endurance requires shared live world and 301..3000 pairs')
    frame_limit=3000 if args.world_endurance else 300
    if args.world_live_xr:
        args.live_xr=True
        if not 1<=args.live_frames<=frame_limit or (args.live_frames!=1 and not args.world_shared_updates_once) or args.sync_camera_fov:p.error(f'world diagnostic needs 1..{frame_limit} pairs, shared-update policy for series, and default auxiliary FOV')
    if args.world_shared_updates_once and not args.world_live_xr:p.error('shared-update policy requires --world-live-xr')
    if args.world_share_vfx_lights and not (args.world_live_xr and args.world_shared_updates_once):p.error('shared VFX lights require live world and shared updates once')
    if args.world_early_xr and not (args.world_live_xr and args.world_shared_updates_once):p.error('early XR requires live world and shared updates once')
    if args.world_shared_culling_camera and not (args.world_early_xr and args.world_root_camera=='first-person' and args.world_share_vfx_lights):p.error('shared culling camera requires early XR, root first-person and shared VFX lights')
    if args.world_vfx_transparent_render:args.world_vfx_transparent_observe=True
    if args.world_vfx_transparent_observe and not args.world_shared_culling_camera:p.error('transparent VFX observation requires shared culling camera')
    if args.world_root_camera and not args.world_live_xr:p.error('root camera requires live world diagnostic')
    if args.world_camera_mode_fixture and (not args.world_camera_modes or not args.world_input_observe or args.live_frames<600):p.error('camera fixture requires modes, input observation, and at least 600 pairs')
    if args.world_camera_modes and (not args.world_live_xr or args.world_root_camera!='first-person'):p.error('camera modes require live first-person')
    if args.world_controller_jump_fixture and (not args.world_live_xr or args.world_root_camera!='first-person' or not args.world_input_observe or not args.world_camera_modes or args.live_frames<600 or args.world_camera_mode_fixture or args.world_input_walk_fixture or args.world_controller_menu_fixture or args.world_controller_tabs_fixture or args.world_controller_altar_fixture):p.error('jump fixture needs600pairs, live first-person, input observation, camera modes, and excludes other fixtures')
    controller_altar=args.world_controller_altar_fixture
    if controller_altar:
        if args.world_controller_menu_fixture or args.world_controller_tabs_fixture or args.live_frames<600 or not args.world_camera_modes:p.error("altar fixture requires600pairs, camera modes, and no other controller fixture")
        args.world_controller_menu_fixture=True
    controller_tour=args.world_controller_menu_tour
    if controller_tour:
        if args.world_controller_menu_fixture or args.world_controller_tabs_fixture or args.live_frames<600:p.error('menu tour requires600pairs and excludes other menu fixtures')
        args.world_controller_menu_fixture=True
    controller_tabs=args.world_controller_tabs_fixture
    if controller_tabs:
        if args.world_controller_menu_fixture or args.live_frames<600:p.error('tabs fixture requires600pairs and excludes basic menu fixture')
        args.world_controller_menu_fixture=True
    if args.world_controller_menu_fixture and (not args.world_live_xr or args.world_root_camera!='first-person' or not args.world_input_observe or args.live_frames<300 or args.world_camera_mode_fixture or args.world_input_walk_fixture):p.error('controller menu fixture requires live first-person, input observation,300pairs minimum, and no other fixture')
    if args.world_input_observe and not args.world_root_camera:p.error('input observer requires root camera local entity proof')
    if args.world_input_head_idle and (not args.world_input_observe or args.world_root_camera!='first-person' or not args.world_live_xr):p.error('idle head camera requires live XR, first-person root and input observation')
    if args.world_input_head_walking and (args.world_input_head_idle or not args.world_input_observe or args.world_root_camera!='first-person' or not args.world_live_xr):p.error('walking head camera requires live XR, first-person root and input observation, and excludes idle policy')
    if args.world_input_walk_fixture and not args.world_input_head_walking:p.error('walking fixture requires explicit walking head camera')
    if args.world_culling_fill_only and not (args.world_culling_history and args.world_live_xr):p.error('fill-only ablation requires live world culling history')
    if args.world_culling_history and not (args.world_view_resolution or args.world_live_xr):p.error('culling history requires unused world resolution or live world diagnostic')
    if args.world_equipment_observe and not args.world_root_camera:p.error('equipment observer requires root camera identity validation')
    if args.world_head_visibility and not (args.world_root_camera=='first-person' and args.world_equipment_observe and args.world_culling_history and not args.world_culling_fill_only):
        p.error('head visibility requires first-person, equipment observer, and full private culling history')
    if args.world_water_feedback and not (args.world_view_resolution or (args.world_live_xr and args.world_shared_updates_once)):
        p.error('private water feedback requires unused world resolution or live world series with shared updates once')
    if args.capture_glide and (args.world_controller_jump_fixture!='glide' or args.capture_world_final or args.capture_initial_fog_sdf):p.error('glide capture requires explicit glide fixture and excludes other captures')
    if args.capture_on_request and not args.capture_world_final:p.error('request capture requires final capture enabled')
    if args.capture_world_final and (not args.world_live_xr or args.live_frames<2):p.error('final capture requires a world stereo series')
    if args.capture_initial_fog_sdf and (not args.world_live_xr or args.capture_world_final):p.error('initial fog capture requires world stereo and excludes final capture')
    if sum((args.detail_factory,args.world_view_features,args.world_view_resolution))>1:p.error('choose one unused world diagnostic')
    if (args.detail_factory or args.world_view_features or args.world_view_resolution) and any((args.view,args.view_features,args.view_isolated_features,args.view_resolution,
            args.native_calibration,args.retained_calibration,args.scene_lifetime_calibration,args.native_series,
            args.recorded_xr,args.prepared_xr_session,args.live_xr,args.sync_camera_fov)):
        p.error('unused world probes must run as separate diagnostics')
    if args.sync_camera_fov and not args.live_xr:p.error('--sync-camera-fov requires --live-xr')
    if args.live_xr and not (1<=args.live_frames<=frame_limit and 0<args.render_scale<=2):p.error(f'live diagnostic bounds: 1..{frame_limit} frames, 0<RenderScale<=2')
    if args.prepared_xr_session or args.live_xr:args.view=True
    if args.native_series:args.scene_lifetime_calibration=True
    if args.recorded_xr:args.scene_lifetime_calibration=True
    if args.scene_lifetime_calibration:args.retained_calibration=True
    if args.native_calibration:p.error('Mid-frame cleanup calibration disabled after driver crash. Retained calibration is a separate bounded diagnostic.')
    if args.view_features:
        p.error('Feature View probe disabled after 2026-09-13 04:25 native ACCESS_VIOLATION. See docs/view_resource_lifecycle.md; constructor-only --view remains available.')
    mod=pathlib.Path(__file__).resolve().parents[1]
    binary=mod/'build/bin/Release'
    kind='view' if args.world_view_resolution or args.world_view_features or args.detail_factory or args.view or args.view_features or args.view_isolated_features or args.view_resolution or args.retained_calibration else 'render'
    export=b'EVR_ProbeRetainedCalibration' if args.retained_calibration else b'EVR_ProbeViewResolution' if args.view_resolution else b'EVR_ProbeIsolatedViewFeatures' if args.view_isolated_features else b'EVR_ProbeViewResources' if args.view else b'EVR_ProbeRenderResources'
    if args.scene_lifetime_calibration:export=b'EVR_ProbeSceneLifetimeCalibration'
    if args.native_series:export=b'EVR_ProbeNativeSeries'
    if args.recorded_xr:export=b'EVR_ProbeRecordedXr'
    if args.prepared_xr_session:export=b'EVR_ProbePreparedGameSession'
    if args.live_xr:export=b'EVR_ProbeLiveNativeFov' if args.sync_camera_fov else b'EVR_ProbeLiveNativeXr'
    if args.world_live_xr:export=b'EVR_ProbeWorldNativeXr'
    if args.detail_factory:export=b'EVR_ProbeDetailFactory'
    if args.world_view_features:export=b'EVR_ProbeWorldViewFeatures'
    if args.world_view_resolution:export=b'EVR_ProbeWorldViewResolution'
    filename=f'{kind}-resource-probe.jsonl'
    dll=pathlib.Path(json.loads((binary/f'evr_{kind}_resource_probe.json').read_text())['library_path'])
    if args.self_test:
        executable=binary/'evr_process_probe.exe'
        fixture=subprocess.Popen([str(executable),'--fixture'],stdout=subprocess.PIPE,text=True)
        try:
            pid,_=fixture.stdout.readline().split()
            proc=Process(int(pid),executable)
            try:created=creation_filetime(proc.handle)
            finally:proc.close()
            out=mod/'output'/time.strftime(kind+'-resource-loader-selftest-%Y%m%d-%H%M%S')
            result=inject(int(pid),executable,out,dll,export,created,input_files={'live-xr-config.txt':b'EVR_LIVE_XR_1 1 1\n'} if args.live_xr else None)
            assert result==1 and fixture.poll() is None
            assert 'unsupported loaded executable build' in (out/filename).read_text()
            print('PASS unsupported process refused, fixture alive, argument memory released')
        finally:fixture.terminate();fixture.wait(timeout=5)
        return 0
    if args.write_profile:
        # The package's profile (user, 2026-10-14: the game is started from Steam, the loader does the launch): the
        # same checks and files as a real-headset play session gets, without a game. The launch record is what the
        # loader's VR start sets up (include/evr/auto_start.hpp); the world proof and the values of the settings file
        # are written by the loader for each session.
        if not args.world_live_xr or args.world_session is None:p.error('--write-profile is for the live world play session')
        if args.write_profile.exists() and any(args.write_profile.iterdir()):p.error('--write-profile needs a new or empty folder')
        args.launch=args.write_profile
        (args.write_profile).mkdir(parents=True,exist_ok=True)
        (args.write_profile/'launch.json').write_text(json.dumps(dict(pid=0,creation_filetime='profile',mode='openxr_graphics_preparation',runtime='real_steamvr',real_vr={},live_stereo=True,
            motion_hands=True,hands_only=True,vfx_transparent_shader=True,body_clip_shader=True,vr_sharpness='game')),encoding='utf-8')
    if not args.launch:p.error('--launch required')
    game=mod.parent/'enshrouded.exe'
    profile=json.loads((mod/'config/profiles/build26082781.json').read_text())
    if hashlib.sha256(game.read_bytes()).hexdigest()!=profile['exe_sha256']:raise ValueError('unsupported game hash')
    launch=json.loads((args.launch/'launch.json').read_text(encoding='utf-8-sig'))
    if args.write_profile:(args.write_profile/'launch.json').unlink()
    if prior:=retained_scene_probe(args.launch):
        raise ValueError(f'prior drawing probe retains Scene resources: {prior.name}; normally close the game and use a fresh managed launch, not just a fresh DLL')
    if args.world_vfx_transparent_render and not launch.get('vfx_transparent_shader'):raise ValueError('transparent VFX shader must be prepared at launch')
    if launch.get('render_cpu_arena_mib'):
        layer_events=[json.loads(line) for line in (args.launch/'vulkan.jsonl').read_text(encoding='utf-8-sig').splitlines() if line.strip().endswith('}')]
        arena=[e for e in layer_events if e.get('event')=='render_cpu_arena_initialized']
        retired=[e for e in layer_events if e.get('event')=='render_cpu_arena_hook_removed']
        if len(arena)!=1 or not arena[0].get('native_backing_matches') or arena[0].get('native_bytes')!=launch['render_cpu_arena_mib']*1024*1024 or len(retired)!=1 or retired[0].get('status')!=0:
            raise ValueError('requested native CPU arena has not initialized and retired its constructor hook; no probe attempted')
    expected_mode='openxr_graphics_preparation' if args.prepared_xr_session or args.live_xr else 'diagnostic_only'
    allowed_modes={'diagnostic_only','openxr_graphics_preparation'} if args.detail_factory or args.world_view_features or args.world_view_resolution else {expected_mode}
    if launch['mode'] not in allowed_modes or not launch.get('creation_filetime'):raise ValueError('matching managed diagnostic launch required')
    if args.live_xr and not launch.get('live_stereo'):raise ValueError('live native XR not enabled by launcher')
    if (args.capture_world_final or args.capture_initial_fog_sdf or args.capture_glide) and not launch.get('renderdoc'):raise ValueError('world capture requires managed RenderDoc launch')
    if args.prepared_xr_session and not launch.get('session_probe'):raise ValueError('game session probe not enabled by launcher')
    if args.retained_calibration and not launch.get('renderdoc'):raise ValueError('native calibration requires the managed portable RenderDoc launch')
    if not args.write_profile and time.time()-(args.launch/'vulkan.jsonl').stat().st_mtime>10:raise ValueError('render telemetry stale')
    out=args.launch/time.strftime(kind+'-resource-probe-%Y%m%d-%H%M%S')
    input_files={}
    if args.world_epoch_cancel:
        if not args.world_epoch_strict or not args.world_melee_session_repeat or not args.world_contact_recycle or (args.world_active_xr_exit and not args.world_active_contact_owner):raise ValueError('epoch cancellation requires isolated strict repeat/contact trial')
        input_files['world-combat-epoch-cancel.txt']=b'EVR_COMBAT_EPOCH_CANCEL_1\n'
    if args.world_contact_end_retire:
        if not args.world_epoch_cancel or args.live_frames!=(3000 if args.world_active_contact_owner else 600) or (args.world_active_xr_exit and not args.world_active_contact_owner) or not args.world_endurance:raise ValueError('terminal contact bridge requires isolated 600pair admission cancellation/endurance')
        input_files['world-contact-end-retire.txt']=b'EVR_CONTACT_END_RETIRE_1\n'
    if args.world_contact_owner_exit:
        if not args.world_contact_end_retire:raise ValueError('native input owner join requires explicit terminal-contact policy')
        input_files['world-contact-owner-exit.txt']=b'EVR_CONTACT_OWNER_EXIT_1\n'
    if args.world_epoch_shadow or args.world_epoch_strict:
        repeat_epoch=bool(args.world_melee_session_contact and args.world_contact_recycle)
        cancel_epoch=bool(args.world_epoch_strict and args.world_cancel_contact_recycle)
        if repeat_epoch==cancel_epoch:raise ValueError('select one measured epoch path: repeat/contact or strict post-Impact cancellation')
        from combat_epoch_policy import prepare as prepare_combat_epochs
        if args.world_epoch_shadow and args.world_epoch_strict:raise ValueError("select shadow or strict, never two consumers")
        if args.world_epoch_strict:
            if not args.world_epoch_proof:raise ValueError("strict epochs require exact native shadow proof")
            from check_combat_epoch_shadow import audit as epoch_audit
            proof=args.world_epoch_proof;raw=(proof/"view-resource-probe.jsonl").read_bytes();report=json.loads((proof/"combat-epoch-shadow-checks.json").read_text())
            if report.get("status")!="PASS" or not all(report["checks"].values()) or report["source_sha256"]!=hashlib.sha256(raw).hexdigest():raise ValueError("shadow proof report/hash failed")
            previous=json.loads((proof/"combat-epoch-provenance.json").read_text());source=pathlib.Path(previous["publisher"])/"view-resource-probe.jsonl"
            if not all(epoch_audit([json.loads(l) for l in source.read_text().splitlines()],[json.loads(l) for l in raw.decode().splitlines()])[0].values()):raise ValueError("shadow proof raw revalidation failed")
            input_files["combat-epoch-validation.json"]=json.dumps(dict(proof=str(proof.resolve()),sha256=hashlib.sha256(raw).hexdigest(),raw_revalidated=True)).encode()
        input_files.update(prepare_combat_epochs(args.world_epoch_strict or args.world_epoch_shadow,args.launch,bool(args.world_epoch_strict)))
    if args.world_attachments_observe:
        if not args.world_equipment_observe or not args.world_input_observe:raise ValueError('attachment observations require equipment/input observers')
        input_files['world-attachments-observe.txt']=b'EVR_ATTACHMENTS_OBSERVE_1\n'
    if args.world_held_vfx_observe or args.world_held_vfx_mount:
        if not args.world_attachments_observe:raise ValueError('held VFX observation requires attachment observation')
        input_files['world-held-vfx-observe.txt']=b'EVR_HELD_VFX_OBSERVE_1\n'
    if args.world_held_vfx_mount:
        if not args.world_live_input or not args.world_motion_visual:raise ValueError('held VFX mount requires live weapon display')
        input_files['world-held-vfx-mount.txt']=b'EVR_HELD_VFX_MOUNT_1\n'
    if args.world_hands_only:
        if not launch.get('hands_only') or not args.world_head_visibility or not args.world_camera_modes or args.world_body_clip_distance is not None:raise ValueError('hands-only requires prepared launch, head/equipment, camera modes and no distance clip')
        input_files['world-hands-only.txt']=b'EVR_HANDS_ONLY_1\n' 
    if args.world_cancel_contact_recycle:
        if not args.world_motion_offhand_release or not args.world_motion_event_retime or not args.world_motion_collider_apply or args.world_contact_lifetimes or args.world_contact_quiescence or args.world_contact_recycle or args.world_contact_log_budget or args.world_blocked_event_recycle or args.world_melee_session or args.world_motion_controller_speed or args.world_motion_right_speed or args.world_motion_tracking_loss or args.world_motion_collider_cancel_after is not None:raise ValueError('post-Impact retirement requires isolated calibrated offhand fixture')
        proof=args.world_cancel_contact_recycle
        from check_offhand_release import audit as offhand_audit
        from check_motion_two_hand import rows as proof_rows,sequence as proof_sequence
        c,m=offhand_audit(proof_rows(proof/'view-resource-probe.jsonl'),proof_sequence(proof.parent/'mock-hands.csv'),json.loads((proof/'grip-preview.json').read_text()),float((proof/'world-two-hand-attack.txt').read_text().split()[1]))
        if not all(c.values()) or m['loss'][0]!=445 or m['blocked_events'] or not m['tracked_colliders'] or not m['cancelled_colliders']:raise ValueError('exact post-Impact native cancellation proof required')
        report=json.loads((proof/'offhand-release-checks.json').read_text())
        if not report['checks'] or set(report['checks'].values())!={'PASS'}:raise ValueError('full offhand cancellation proof must PASS')
        if (args.launch/'mock-hands.csv').read_bytes()!=pathlib.Path('vrmod/test_env/two-hand-release-445.csv').read_bytes():raise ValueError('post-Impact 445 fixture required')
        input_files['world-cancel-contact-recycle.txt']=b'EVR_CANCEL_CONTACT_RECYCLE_1\n'
        input_files['cancel-contact-proof.json']=json.dumps(dict(source=str(proof.resolve()),source_sha256=hashlib.sha256((proof/'view-resource-probe.jsonl').read_bytes()).hexdigest(),native_cancellation_revalidated=True),indent=2).encode()
    if args.world_blocked_event_recycle:
        if not args.world_motion_offhand_release or not args.world_motion_event_retime or args.world_contact_lifetimes or args.world_contact_quiescence or args.world_contact_recycle or args.world_contact_log_budget or args.world_melee_session:raise ValueError('blocked-event retirement requires isolated pre-Impact offhand fixture')
        proof=args.world_blocked_event_recycle
        from check_offhand_release import audit as offhand_audit
        from check_motion_two_hand import rows as proof_rows,sequence as proof_sequence
        c,m=offhand_audit(proof_rows(proof/'view-resource-probe.jsonl'),proof_sequence(proof.parent/'mock-hands.csv'),json.loads((proof/'grip-preview.json').read_text()),float((proof/'world-two-hand-attack.txt').read_text().split()[1]))
        if not all(c.values()) or m['loss'][0]!=441 or m['blocked_events']!=1:raise ValueError('exact pre-Impact cancellation proof required')
        report=json.loads((proof/'offhand-release-checks.json').read_text())
        if set(report['checks'].values())!={'PASS'}:raise ValueError('full offhand release proof must pass')
        if (args.launch/'mock-hands.csv').read_bytes()!=pathlib.Path('vrmod/test_env/two-hand-release-441.csv').read_bytes():raise ValueError('pre-Impact 441 fixture required')
        input_files['world-blocked-event-recycle.txt']=b'EVR_BLOCKED_EVENT_RECYCLE_1\n'
    if args.world_contact_recycle:
        if not args.world_contact_quiescence or not args.world_contact_log_budget:raise ValueError('recycling requires finite lifetime/quiescence/budget witnesses')
        proof=args.world_contact_recycle
        source=proof/'view-resource-probe.jsonl'
        digest=hashlib.sha256(source.read_bytes()).hexdigest()
        for name in ('contact-quiescence-checks.json','contact-lifetime-checks.json'):
            report=json.loads((proof/name).read_text())
            if report['source_sha256']!=digest or set(report['checks'].values())!={'PASS'}:raise ValueError('exact passing native lifetime/quiescence proof required')
        input_files['world-contact-recycle.txt']=b'EVR_CONTACT_RECYCLE_1 1\n'
    if args.world_contact_quiescence:
        if not args.world_contact_lifetimes:raise ValueError('quiescence observation requires owned Impact lifetime trial')
        input_files['world-contact-quiescence.txt']=b'EVR_CONTACT_QUIESCENCE_1\n'
    if args.world_contact_lifetimes:
        if not args.world_melee_session_contact:raise ValueError('lifetime observation requires finite measured contact mode')
        input_files['world-contact-lifetimes.txt']=b'EVR_CONTACT_LIFETIMES_1\n'
    if args.world_contact_log_budget:
        if not args.world_melee_session_contact:raise ValueError('tiny recording budget requires measured finite contact mode')
        input_files['world-contact-log-budget.txt']=b'EVR_CONTACT_LOG_BUDGET_1 3 2\n'
    if args.world_melee_session_contact:
        if not args.world_melee_session or not args.world_melee_session_repeat or not args.world_motion_right_speed or not args.world_grip_preview or not args.world_motion_two_hand_early or not args.world_motion_contact_observe or not args.world_motion_collider_shadow or not args.world_motion_collider_apply or not args.world_motion_event_retime or args.world_motion_collider_cancel_after is not None or args.world_motion_repeat or args.world_motion_tracking_loss or args.world_motion_offhand_release:raise ValueError('session contact requires measured repeat and existing early/geometry proofs, no cancellation fixture')
        from check_melee_repeat import audit as session_audit
        from generate_melee_repeat_fixture import verify as verify_session_fixture
        proof=args.world_melee_session_contact
        if (proof/'world-melee-session.txt').read_text().strip()!='EVR_MELEE_SESSION_REPEAT_2' or (proof/'world-melee-session-contact.txt').exists():raise ValueError('isolated coordinator proof required')
        proof_rows=[json.loads(l) for l in (proof/'view-resource-probe.jsonl').read_text().splitlines()]
        if not all(session_audit(proof_rows).values()):raise ValueError('session proof failed independent revalidation')
        for name in ('melee-repeat-checks.json','controller-speed-checks.json','motion-feedback-checks.json','motion-hand-checks.json','motion-visual-checks.json','live-native-checks-fixed-root-motion.json'):
            checks=json.loads((proof/name).read_text())['checks']
            if not checks or any(v!='PASS' for v in checks.values()):raise ValueError('session proof dependency failed: '+name)
        if (proof.parent/'mock-hands.csv').read_bytes()!=(args.launch/'mock-hands.csv').read_bytes() or (proof/'world-grip-preview.txt').read_bytes()!=(args.world_grip_preview/'profile.txt').read_bytes() or (proof/'grip-hand-pose.bin').read_bytes()!=(args.world_grip_preview/'hand-pose.bin').read_bytes():raise ValueError('session pose/profile differs from native evidence')
        verify_session_fixture(args.launch/'mock-hands.csv',json.loads((args.world_grip_preview/'profile.json').read_text()))
        input_files['world-melee-session-contact.txt']=b'EVR_MELEE_SESSION_CONTACT_2\n'
        input_files['melee-session-contact-proof.json']=json.dumps(dict(source=str(proof.resolve()),coordinator_revalidated=True,source_sha256=hashlib.sha256((proof/'view-resource-probe.jsonl').read_bytes()).hexdigest(),geometry_proof=str(args.world_motion_collider_apply.resolve()),early_proof=str(args.world_motion_two_hand_early.resolve())),indent=2).encode()
    if args.world_motion_recovery_swing and not args.world_motion_repeat:raise ValueError('recovery swing requires explicit repeat mode')
    if args.world_motion_repeat and (not args.world_motion_attack or not args.world_motion_event_retime or not args.world_motion_collider_apply or args.world_motion_tracking_loss or args.world_motion_collider_cancel_after is not None):raise ValueError('repeat needs exact two-swing early Impact/geometry diagnostic, without loss or forced cancellation')
    if args.world_motion_tracking_loss:
        if not args.world_motion_event_retime or args.world_motion_collider_cancel_after is not None:raise ValueError('tracking loss needs early Impact, no forced cancel')
        seq=[[float(v) for v in line.split(',')] for line in (args.launch/'mock-hands.csv').read_text().splitlines() if line and not line.startswith('#')]
        if not any(x[0]>440 and x[26]==0 for x in seq):raise ValueError('mock sequence has no post-swing right tracking loss')
        input_files['world-motion-tracking-loss.txt']=b'EVR_MOTION_TRACKING_LOSS_1\n'
    if args.world_motion_contact_observe:
        if not args.world_motion_attack:raise ValueError('contact observation requires one-swing motion diagnostic')
        input_files['world-motion-contact-observe.txt']=b'EVR_MOTION_CONTACT_OBSERVE_1\n'
    if args.world_motion_event_retime:
        if not args.world_motion_collider_apply or args.world_motion_collider_cancel_after is not None:raise ValueError('early Impact test requires tracked geometry and no cancellation fixture')
        from check_event_retime import audit as retime_audit
        proof=args.world_motion_event_retime;rows=[json.loads(x) for x in (proof/'view-resource-probe.jsonl').read_text(encoding='utf8').splitlines()]
        checks,metrics=retime_audit(rows)
        if not all(checks.values()):raise ValueError('native event retime proof failed revalidation')
        input_files['world-motion-event-retime.txt']=b'EVR_MOTION_EVENT_RETIME_1\n'
        input_files['motion-event-retime-proof.json']=json.dumps(dict(source=str(proof.resolve()),checks=checks,metrics=metrics),indent=2).encode()
    if args.world_motion_collider_cancel_after is not None:
        if not args.world_motion_collider_apply or not 1<=args.world_motion_collider_cancel_after<=10:raise ValueError('cancellation fixture needs tracked collider and count 1..10')
        input_files['world-motion-collider-cancel-after.txt']=(str(args.world_motion_collider_cancel_after)+'\n').encode('ascii')
    if args.world_motion_collider_apply:
        if not args.world_motion_collider_shadow:raise ValueError('tracked collision requires same shadow gating')
        proof=args.world_motion_collider_apply
        from check_motion_collider_shadow import audit as shadow_audit
        rows=[json.loads(s) for s in (proof/'view-resource-probe.jsonl').read_text(encoding='utf8').splitlines()]
        if not all(shadow_audit(rows)[0].values()):raise ValueError('shadow proof failed independent revalidation')
        if not args.world_motion_tracking_loss and not args.world_motion_repeat and not args.world_motion_offhand_release and not args.world_melee_session_contact and (proof.parent/'mock-hands.csv').read_bytes()!=(args.launch/'mock-hands.csv').read_bytes():raise ValueError('shadow fixture differs')
        if args.world_motion_two_hand:
            from check_calibrated_collider_shadow import audit as calibrated_audit
            from check_motion_two_hand import sequence as read_hand_sequence
            if not args.world_grip_preview:raise ValueError('calibrated collision needs measured grip profile')
            measured=json.loads((proof/'grip-preview.json').read_text())
            checks,_=calibrated_audit(rows,read_hand_sequence(proof.parent/'mock-hands.csv'),measured,float((proof/'world-two-hand-attack.txt').read_text().split()[1]))
            if not all(checks.values()):raise ValueError('calibrated read-only physics proof failed revalidation')
            for result in ('calibrated-collider-shadow-checks.json','motion-two-hand-checks.json','motion-hand-checks.json','motion-visual-checks.json'):
                c=json.loads((proof/result).read_text())['checks']
                if not c or any(v!='PASS' for v in c.values()):raise ValueError('calibrated proof dependency failed')
            if (proof/'world-grip-preview.txt').read_bytes()!=(args.world_grip_preview/'profile.txt').read_bytes() or (proof/'grip-hand-pose.bin').read_bytes()!=(args.world_grip_preview/'hand-pose.bin').read_bytes():raise ValueError('calibrated collision profile differs from read-only physics proof')
        input_files['world-motion-collider-apply.txt']=b'EVR_MOTION_COLLIDER_APPLY_1\n'
        input_files['motion-collider-shadow-proof.json']=json.dumps({'source':str(proof.resolve()),'read_only_proof_revalidated':True}).encode('utf8')
    if args.world_motion_collider_shadow:
        if not args.world_motion_contact_observe or not args.world_motion_visual:raise ValueError('collider shadow requires contact observation and tracked visual')
        input_files['world-motion-collider-shadow.txt']=b'EVR_MOTION_COLLIDER_SHADOW_1\n'
    if args.world_guard_fixture or args.world_motion_guard:
        if args.world_guard_fixture and args.world_motion_guard:raise ValueError('choose one guard fixture')
        if not args.world_grip_preview or not args.world_motion_hands or not args.world_endurance or not args.world_input_head_walking or not args.world_camera_modes or args.live_frames!=600 or args.world_motion_attack or args.world_motion_equipment_switch or args.world_motion_collider_apply or args.world_motion_collider_shadow or args.world_native_grip_observe or args.world_head_gesture or args.world_controller_menu_fixture or args.world_controller_jump_fixture or args.world_input_walk_fixture or args.world_camera_mode_fixture:raise ValueError('guard fixture requires isolated calibrated hands and 600pairs')
        import os
        config_path=user_settings_path()
        config=json.loads(config_path.read_text(encoding='utf-8-sig'));gp=config['input']['gamepad']
        if gp['selectedPresetId']!='PresetA' or gp['bindings']['gameplay']:raise ValueError('guard diagnostic requires existing unmodified PresetA')
        if args.world_motion_guard:
            from generate_guard_posture_fixture import verify,PROFILE
            verify(args.launch/'mock-hands.csv',json.loads((args.world_grip_preview/'profile.json').read_text()),delay=350)
            input_files['guard-sequence.json']=json.dumps(dict(delay_frames=350,first_expected_sample=364,sha256=hashlib.sha256((args.launch/'mock-hands.csv').read_bytes()).hexdigest())).encode('ascii')
            input_files['world-motion-guard.txt']=PROFILE.encode('ascii')
        elif (args.launch/'mock-hands.csv').read_bytes()!=pathlib.Path('vrmod/test_env/calibrated-grip-hands.csv').read_bytes():raise ValueError('guard fixture requires exact calibrated hands')
        if (args.launch/'mock-pose.csv').read_bytes()!=pathlib.Path('vrmod/test_env/world-static-pose.csv').read_bytes():raise ValueError('guard fixture requires static head')
        input_files['controller-ui-fixture.txt']=b'EVR_MOTION_GUARD_1\n' if args.world_motion_guard else b'EVR_CONTROLLER_GUARD_1\n'
        input_files['guard-binding.json']=json.dumps(dict(path=str(config_path),sha256=hashlib.sha256(config_path.read_bytes()).hexdigest(),gamepad=gp),indent=2).encode()
    if args.world_motion_equipment_switch:
        if not args.world_grip_preview or not args.world_motion_hands or not args.world_endurance or not args.world_input_head_walking or not args.world_camera_modes or args.live_frames!=600 or args.world_motion_attack or args.world_motion_collider_apply or args.world_motion_collider_shadow or args.world_native_grip_observe or args.world_head_gesture or args.world_controller_menu_fixture or args.world_controller_jump_fixture or args.world_input_walk_fixture or args.world_camera_mode_fixture:raise ValueError('equipment switch requires isolated calibrated visual/hands and 600pairs')
        import os
        config_path=user_settings_path()
        config=json.loads(config_path.read_text(encoding='utf-8-sig'));gp=config['input']['gamepad']
        if gp['selectedPresetId']!='PresetA' or gp['bindings']['gameplay']:raise ValueError('equipment diagnostic requires existing unmodified PresetA')
        if (args.launch/'mock-hands.csv').read_bytes()!=pathlib.Path('vrmod/test_env/calibrated-grip-hands.csv').read_bytes():raise ValueError('equipment switch requires exact calibrated hand fixture')
        if (args.launch/'mock-pose.csv').read_bytes()!=pathlib.Path('vrmod/test_env/world-static-pose.csv').read_bytes():raise ValueError('equipment switch requires static head')
        input_files['controller-ui-fixture.txt']=b'EVR_CONTROLLER_EQUIPMENT_1\n'
        input_files['world-motion-equipment-switch.txt']=b'EVR_MOTION_EQUIPMENT_SWITCH_1\n'
        input_files['equipment-binding.json']=json.dumps(dict(path=str(config_path),sha256=hashlib.sha256(config_path.read_bytes()).hexdigest(),gamepad=gp),indent=2).encode()
    if (args.world_vertical_stick or args.world_quick_wheel) and (args.world_head_gesture or (args.world_vertical_stick and args.world_quick_wheel) or args.world_guard_fixture or args.world_motion_guard or args.world_motion_equipment_switch or args.world_active_xr_exit or args.world_controller_tabs_fixture or args.world_controller_altar_fixture or args.world_motion_attack or args.world_motion_collider_apply or args.world_motion_collider_shadow or args.world_native_grip_observe):raise ValueError('vertical mapping requires an isolated input fixture')
    if (args.world_quick_wheel_rebind or args.world_quick_wheel_consumable or args.world_quick_wheel_survey or args.world_grip_router_wheel or args.world_grip_router_equip) and not args.world_quick_wheel:raise ValueError('wheel rebind/consumable/survey/router requires the isolated quick wheel fixture')
    if sum(map(bool,(args.world_quick_wheel_rebind,args.world_quick_wheel_consumable,args.world_quick_wheel_survey,args.world_grip_router_wheel,args.world_grip_router_equip)))>1:raise ValueError('choose one quick wheel schedule')
    if args.world_grip_router_equip_overlap and not args.world_grip_router_equip:raise ValueError('overlap equip modifies the router equip fixture')
    if args.world_head_gesture_left and not args.world_head_gesture:raise ValueError('left temple menu modifies the head gesture fixture')
    if args.world_head_gesture or args.world_vertical_stick or args.world_quick_wheel:
        if not args.world_motion_hands or not args.world_endurance or not args.world_input_head_walking or not args.world_camera_modes or args.live_frames!=600 or args.world_motion_attack or args.world_controller_menu_fixture or args.world_controller_jump_fixture or args.world_input_walk_fixture or args.world_camera_mode_fixture:raise ValueError('gesture menu needs isolated hands/600pairs/camera policy and no other input fixture')
        import os
        config_path=user_settings_path()
        config=json.loads(config_path.read_text(encoding='utf-8-sig'));gp=config['input']['gamepad']
        if gp['selectedPresetId']!='PresetA' or gp['bindings']['gameplay']:raise ValueError('gesture diagnostic requires existing unmodified PresetA')
        mocklog=args.launch/'mock.jsonl'
        if not mocklog.is_file() or not any(json.loads(line).get('event')=='instance_created' for line in mocklog.read_text().splitlines()):raise ValueError('gesture requires prepared process-local mock')
        hand_fixture=('world-quick-wheel-rebind-hands.csv' if args.world_quick_wheel_rebind else 'world-quick-wheel-consumable2-hands.csv' if args.world_quick_wheel_consumable else 'world-quick-wheel-survey-hands.csv' if args.world_quick_wheel_survey else 'world-grip-router-wheel-hands.csv' if args.world_grip_router_wheel else 'world-grip-router-equip-hands.csv' if args.world_grip_router_equip else 'world-quick-wheel-hands.csv') if args.world_quick_wheel else 'world-vertical-stick-hands.csv' if args.world_vertical_stick else 'world-head-gesture-hands.csv'
        if args.world_grip_router_equip_overlap:hand_fixture='world-grip-router-equip-overlap-hands.csv'
        if args.world_head_gesture_left:hand_fixture='world-head-gesture-left-hands.csv'
        if (args.launch/'mock-hands.csv').read_bytes()!=(pathlib.Path('vrmod/test_env')/hand_fixture).read_bytes():raise ValueError('gesture/vertical requires exact approved fixture')
        if (args.launch/'mock-pose.csv').read_bytes()!=pathlib.Path('vrmod/test_env/world-static-pose.csv').read_bytes():raise ValueError('gesture requires approved static head')
        input_files['controller-ui-fixture.txt']=(b'EVR_QUICK_WHEEL_REBIND_1\n' if args.world_quick_wheel_rebind else b'EVR_QUICK_WHEEL_CONSUMABLE_2\n' if args.world_quick_wheel_consumable else b'EVR_QUICK_WHEEL_SURVEY_1\n' if args.world_quick_wheel_survey else b'EVR_GRIP_ROUTER_WHEEL_1\n' if args.world_grip_router_wheel else b'EVR_GRIP_ROUTER_EQUIP_1\n' if args.world_grip_router_equip else b'EVR_QUICK_WHEEL_1\n') if args.world_quick_wheel else b'EVR_VERTICAL_STICK_1\n' if args.world_vertical_stick else b'EVR_HEAD_GESTURE_MENU_1\n'
        if args.world_grip_router_equip_overlap:input_files['controller-ui-fixture.txt']=b'EVR_GRIP_ROUTER_EQUIP_2\n'
        if args.world_head_gesture_left:input_files['controller-ui-fixture.txt']=b'EVR_HEAD_GESTURE_MENU_LEFT_1\n'
        input_files['gesture-binding.json']=json.dumps(dict(path=str(config_path),sha256=hashlib.sha256(config_path.read_bytes()).hexdigest(),gamepad=gp),indent=2).encode()
        # Synthetic test calibration only (head->navel, zones, timings); not product body data.
        if args.world_grip_router_wheel or args.world_grip_router_equip:input_files['world-grip-router.txt']=b'EVR_SYNTHETIC_GRIP_ROUTER_1 0 -0.6 0 1 0.22 0.40 -0.05 -0.22 0.40 -0.05 0.22 -0.05 0 -0.22 -0.05 0 0.12 0.16 0.3 200000000 300000000 100000000\n'
        # v2 adds the head-relative shoulder reach (below above lateral forward keepMargin), VHVR back-reach geometry as reference.
        if args.world_grip_router_equip_overlap:input_files['world-grip-router.txt']=b'EVR_SYNTHETIC_GRIP_ROUTER_2 0 -0.6 0 1 0.22 0.40 -0.05 -0.22 0.40 -0.05 0.22 -0.05 0 -0.22 -0.05 0 0.12 0.16 0.3 200000000 300000000 100000000 0.25 0.125 0.5 0 0.04\n'
    if args.world_motion_offhand_release:
        if not args.world_motion_two_hand_early or args.world_motion_tracking_loss or args.world_motion_repeat or args.world_motion_collider_cancel_after is not None:raise ValueError('offhand release needs calibrated early proof, no tracking loss/repeat/forced cancellation')
        from check_calibrated_collider_apply import audit as early_apply_audit
        from check_motion_two_hand import rows as proof_rows,sequence as proof_sequence
        proof=args.world_motion_offhand_release
        checks,metrics=early_apply_audit(proof_rows(proof/'view-resource-probe.jsonl'),proof_sequence(proof.parent/'mock-hands.csv'),json.loads((proof/'grip-preview.json').read_text()),float((proof/'world-two-hand-attack.txt').read_text().split()[1]),early=True)
        if not all(checks.values()):raise ValueError('calibrated early proof failed revalidation')
        for name in ('calibrated-collider-apply-checks.json','motion-two-hand-checks.json','motion-hand-checks.json','motion-visual-checks.json'):
            c=json.loads((proof/name).read_text())['checks']
            if not c or any(v!='PASS' for v in c.values()):raise ValueError('offhand release proof dependency failed')
        if (proof/'world-grip-preview.txt').read_bytes()!=(args.world_grip_preview/'profile.txt').read_bytes() or (proof/'grip-hand-pose.bin').read_bytes()!=(args.world_grip_preview/'hand-pose.bin').read_bytes():raise ValueError('offhand release profile differs')
        actual=(args.launch/'mock-hands.csv').read_bytes()
        if actual not in [pathlib.Path(f'vrmod/test_env/two-hand-release-{f}.csv').read_bytes() for f in (441,445)]:raise ValueError('offhand release needs approved CSV')
        base=proof_sequence(proof.parent/'mock-hands.csv');changed=proof_sequence(args.launch/'mock-hands.csv')
        if len(base)!=len(changed) or any(a[:11]!=b[:11] or a[12:]!=b[12:] for a,b in zip(base,changed)):raise ValueError('offhand fixture changed fields other than grab')
        input_files['world-motion-offhand-release.txt']=b'EVR_MOTION_OFFHAND_RELEASE_1\n'
        input_files['offhand-release-proof.json']=json.dumps(dict(source=str(proof.resolve()),early_physics_revalidated=True,metrics=metrics),indent=2).encode()
    if args.world_motion_two_hand_early:
        if not args.world_motion_two_hand or not args.world_motion_event_retime or not args.world_motion_collider_apply:raise ValueError('calibrated early Impact requires two-hand gate, event and physics proofs')
        from check_calibrated_collider_apply import audit as apply_audit
        from check_motion_two_hand import rows as proof_rows,sequence as proof_sequence
        proof=args.world_motion_two_hand_early
        checks,metrics=apply_audit(proof_rows(proof/'view-resource-probe.jsonl'),proof_sequence(proof.parent/'mock-hands.csv'),json.loads((proof/'grip-preview.json').read_text()),float((proof/'world-two-hand-attack.txt').read_text().split()[1]))
        if not all(checks.values()):raise ValueError('calibrated native-window physics proof failed revalidation')
        for name in ('calibrated-collider-apply-checks.json','motion-two-hand-checks.json','motion-hand-checks.json','motion-visual-checks.json'):
            checks=json.loads((proof/name).read_text())['checks']
            if not checks or any(v!='PASS' for v in checks.values()):raise ValueError('calibrated early proof dependency failed')
        if not args.world_motion_offhand_release and not args.world_melee_session_contact and (proof.parent/'mock-hands.csv').read_bytes()!=(args.launch/'mock-hands.csv').read_bytes():raise ValueError('calibrated early fixture differs')
        if (proof/'world-grip-preview.txt').read_bytes()!=(args.world_grip_preview/'profile.txt').read_bytes() or (proof/'grip-hand-pose.bin').read_bytes()!=(args.world_grip_preview/'hand-pose.bin').read_bytes():raise ValueError('calibrated early profile differs')
        input_files['two-hand-early-proof.json']=json.dumps(dict(source=str(proof.resolve()),native_window_physics_revalidated=True,metrics=metrics),indent=2).encode()
    if args.world_motion_head_interlock:
        if not args.world_motion_two_hand or not args.world_grip_preview or args.world_motion_controller_speed or args.world_motion_collider_apply or args.world_motion_collider_shadow or args.world_motion_event_retime or args.world_motion_repeat or args.world_motion_offhand_release or args.world_motion_tracking_loss or args.world_head_gesture:raise ValueError('head interlock requires isolated calibrated action test')
        from generate_head_combat_fixture import verify
        verify(args.launch/'mock-hands.csv',json.loads((args.world_grip_preview/'profile.json').read_text()))
        input_files['world-head-combat.txt']=b'EVR_HEAD_COMBAT_1\n'
    if args.world_motion_right_speed and not args.world_motion_controller_speed:raise ValueError('right-speed requires explicit controller speed mode')
    if args.world_melee_session_repeat and not args.world_melee_session:raise ValueError('melee repeat requires explicit coordinator')
    if args.world_melee_session:
        if not args.world_motion_right_speed or not args.world_motion_two_hand or not args.world_motion_attack or args.live_frames!=(3000 if args.world_active_contact_owner else 600) or args.world_motion_head_interlock:raise ValueError('first session adapter requires isolated measured right-speed trial')
        input_files['world-melee-session.txt']=b'EVR_MELEE_SESSION_REPEAT_2\n' if args.world_melee_session_repeat else b'EVR_MELEE_SESSION_1\n'
    if args.world_motion_controller_speed:
        if not args.world_motion_two_hand or ((args.world_motion_collider_apply or args.world_motion_collider_shadow or args.world_motion_event_retime) and not args.world_melee_session_contact) or args.world_motion_repeat or args.world_motion_offhand_release or args.world_motion_tracking_loss:raise ValueError('controller speed requires isolated action test or validated session contact mode')
        if args.world_motion_right_speed:
            if args.world_melee_session_repeat:
                from generate_melee_repeat_fixture import verify
            else:
                from generate_right_hand_speed_fixture import verify
            input_files['right-hand-speed-policy.txt']=b'EVR_RIGHT_HAND_SPEED_1\n'
        else:
            from generate_controller_speed_fixture import verify
        verify(args.launch/'mock-hands.csv',json.loads((args.world_grip_preview/'profile.json').read_text()))
        input_files['controller-speed-policy.txt']=b'EVR_CONTROLLER_TRANSLATION_1\n'
    if args.world_motion_two_hand:
        if not args.world_motion_attack or not args.world_grip_preview or args.world_motion_repeat or args.world_motion_tracking_loss or (args.world_motion_event_retime and not args.world_motion_two_hand_early) or args.world_head_gesture:raise ValueError('two-hand gate is isolated one attack; early Impact needs calibrated physics proof and loss needs separate proof')
        proof=args.world_motion_two_hand
        for name in ('grip-preview-checks.json','motion-hand-checks.json','motion-visual-checks.json'):
            checks=json.loads((proof/name).read_text())['checks']
            if not checks or any(v!='PASS' for v in checks.values()):raise ValueError('two-hand visual proof failed')
        if (proof/'world-grip-preview.txt').read_bytes()!=(args.world_grip_preview/'profile.txt').read_bytes() or (proof/'grip-hand-pose.bin').read_bytes()!=(args.world_grip_preview/'hand-pose.bin').read_bytes():raise ValueError('two-hand profile differs from rendered proof')
        if (args.launch/'mock-pose.csv').read_bytes()!=pathlib.Path('vrmod/test_env/world-static-pose.csv').read_bytes():raise ValueError('two-hand gate needs static HMD')
        input_files['world-two-hand-attack.txt']=b'EVR_TWO_HAND_ATTACK_1 0.035\n'
    if args.world_motion_attack:
        if not args.world_motion_hands or not args.world_endurance or not args.world_input_head_walking or args.world_controller_menu_fixture or args.world_controller_jump_fixture or args.world_input_walk_fixture or args.world_camera_mode_fixture:raise ValueError('motion attack needs isolated tracked hands endurance and no other input fixture')
        import os
        config_path=user_settings_path()
        config=json.loads(config_path.read_text(encoding='utf-8-sig'))
        gp=config['input']['gamepad']
        if gp['selectedPresetId']!='PresetA' or gp['bindings']['gameplay']:raise ValueError('one-swing diagnostic requires existing unmodified PresetA gamepad binding')
        mocklog=args.launch/'mock.jsonl'
        if not mocklog.is_file() or not any(json.loads(line).get('event')=='instance_created' for line in mocklog.read_text().splitlines()):raise ValueError('motion attack requires an already prepared process-local mock instance')
        fixture=pathlib.Path('vrmod/test_env/single-swing-hands.csv').read_bytes()
        actual=(args.launch/'mock-hands.csv').read_bytes()
        if args.world_motion_two_hand:
            approved_name='melee-repeat-hands.csv' if args.world_melee_session_repeat else 'right-hand-speed-hands.csv' if args.world_motion_right_speed else 'head-combat-hands.csv' if args.world_motion_head_interlock else 'controller-speed-late-hands.csv' if args.world_motion_controller_speed else 'two-hand-attack-hands.csv'
            if not args.world_motion_offhand_release and actual!=(pathlib.Path('vrmod/test_env')/approved_name).read_bytes():raise ValueError('two-hand attack needs exact approved fixture')
        elif args.world_motion_tracking_loss:
            approved=[pathlib.Path(f'vrmod/test_env/single-swing-loss-{start}.csv').read_bytes() for start in (441,445)]
            if actual not in approved:raise ValueError('tracking loss requires an exact approved bounded loss fixture')
            base=[[float(v) for v in l.split(',')] for l in fixture.decode().splitlines() if l and not l.startswith('#')]
            if len(base)!=len(seq) or any(a[:26]!=b[:26] or a[27:]!=b[27:] for a,b in zip(base,seq)):raise ValueError('loss fixture altered pose, action or focus')
        elif args.world_motion_repeat:
            repeat_name='two-swing-cooldown-hands.csv' if args.world_motion_recovery_swing else 'two-swing-hands.csv'
            if actual!=(pathlib.Path('vrmod/test_env')/repeat_name).read_bytes():raise ValueError('repeat needs exact approved fixture')
            base=[l.split(',') for l in fixture.decode().splitlines() if l and not l.startswith('#')];repeat=[l.split(',') for l in actual.decode().splitlines() if l and not l.startswith('#')]
            if len(base)!=len(repeat):raise ValueError('repeat fixture length differs')
            for row,original in zip(repeat,base):
                f=int(row[0]);expected=original.copy()
                if 500<=f<550:expected[14]=str(float(base[498][14])+(float(base[438][14])-float(base[498][14]))*min(f-499,31)/31)
                if 535<=f<=580:expected[14:27]=base[f-111][14:27]
                if args.world_motion_recovery_swing:
                    if 450<=f<=455:expected[14]=str(.5-.045*(f-449))
                    elif 456<=f<=461:expected[14]=str(.23+.045*(f-455))
                if row!=expected:raise ValueError('repeat fixture altered unapproved fields')
            if (args.world_motion_collider_apply.parent/'mock-hands.csv').read_bytes()!=fixture:raise ValueError('repeat calibration must come from approved single-swing proof')
        elif actual!=fixture:raise ValueError('one-swing diagnostic requires exact archived single-swing fixture')
        input_files['controller-ui-fixture.txt']=b'EVR_MOTION_ATTACK_REPEAT_2\n' if args.world_motion_repeat or args.world_melee_session_repeat else b'EVR_MOTION_ATTACK_1\n'
        input_files['motion-attack-binding.json']=json.dumps(dict(path=str(config_path),sha256=hashlib.sha256(config_path.read_bytes()).hexdigest(),gamepad=gp),indent=2).encode()
    if args.world_motion_hands and not args.world_motion_visual:raise ValueError('motion hands requires motion visual')
    if args.world_grip_preview:
        if not args.world_motion_hands or (args.world_motion_attack and not args.world_motion_two_hand) or args.world_head_gesture or (args.world_motion_collider_apply and not args.world_motion_two_hand):raise ValueError('grip preview needs private hands and only the separately proved two-hand gate may send attack input')
        profile=json.loads((args.world_grip_preview/'profile.json').read_text())
        if profile.get('combat_approved') is not False or not profile.get('closed_fingers'):raise ValueError('grip preview scope invalid')
        for name in ('profile.txt','hand-pose.bin'):
            if hashlib.sha256((args.world_grip_preview/name).read_bytes()).hexdigest()!=profile['files'][name]:raise ValueError('grip preview source changed')
        input_files['world-grip-preview.txt']=(args.world_grip_preview/'profile.txt').read_bytes()
        input_files['grip-hand-pose.bin']=(args.world_grip_preview/'hand-pose.bin').read_bytes()
        input_files['grip-preview.json']=(args.world_grip_preview/'profile.json').read_bytes()
    # Weapon display/aim routes were reviewed on the Mock runtime. A real headset needs the explicit trial switch.
    weapon_runtime=launch.get('runtime')=='mock' or args.world_weapon_trial_real
    if args.world_bow_spawn_observe:
        if not weapon_runtime or not args.world_live_input or not args.world_native_grip_observe:raise ValueError('spawn observer requires Mock live mount survey')
        input_files['world-bow-spawn-observe.txt']=b'EVR_BOW_SPAWN_OBSERVE_1\n'
    if args.world_live_melee:
        if not args.world_live_input:raise ValueError('live melee requires live controller input')
        input_files['world-live-melee.txt']=b'EVR_LIVE_MELEE_1'+(b' heavy=%.2f'%args.world_live_melee_heavy_factor if args.world_live_melee_heavy_factor else b'')+(b' travel=%.2f'%args.world_live_melee_travel if args.world_live_melee_travel else b'')+(b' heavy_hold=%d'%args.world_live_melee_heavy_hold if args.world_live_melee_heavy_hold is not None else b'')+(b' heavy_motion' if args.world_live_melee_heavy_motion else b'')+(b' targets' if args.world_live_melee_targets else b'')+b'\n'
    if args.world_body_heading:
        if not args.world_live_input:raise ValueError('body heading requires live controller input')
        input_files['world-body-heading.txt']=b'EVR_BODY_HEADING_1\n'
    if args.world_live_held_items:
        if not args.world_motion_visual:raise ValueError('live held items require the tracked weapon display')
        input_files['world-live-held-items.txt']=b'EVR_LIVE_HELD_ITEMS_1 follow\n' if args.world_live_held_follow else b'EVR_LIVE_HELD_ITEMS_1\n'
    elif args.world_live_held_follow:raise ValueError('held follow requires live held items')
    if args.world_live_melee_contact:
        if not args.world_live_melee or not args.world_motion_visual:raise ValueError('live melee contact requires live melee and the tracked weapon display')
        input_files['world-live-melee-contact.txt']=b'EVR_LIVE_MELEE_CONTACT_1'+(b' early' if 'early' in args.world_live_melee_contact else b'')+(b' observe' if 'observe' in args.world_live_melee_contact else b'')+(b' still' if 'still' in args.world_live_melee_contact else b'')+b'\n'
    if args.world_ui_auto_hide:
        if not args.world_live_input:raise ValueError('UI auto-hide requires live controller input')
        input_files['world-ui-auto-hide.txt']=b'EVR_UI_AUTO_HIDE_1\n'
    if args.world_menu_pointer:
        if not args.world_live_input:raise ValueError('the menu pointer requires live controller input')
        input_files['world-menu-pointer.txt']=b'EVR_MENU_POINTER_1\n'
    if args.world_quick_slot_motion:
        if not args.world_live_input:raise ValueError('quick slot by hand motion requires live controller input')
        input_files['world-quick-slot-motion.txt']=b'EVR_QUICK_SLOT_MOTION_1\n'
    if args.world_live_guard:
        if not args.world_live_input:raise ValueError('live guard requires live controller input')
        input_files['world-live-guard.txt']=b'EVR_LIVE_GUARD_1\n'
    if args.world_ranged_survey:
        if launch.get('runtime')!='mock' or not args.world_bow_spawn_observe:raise ValueError('ranged survey requires the Mock spawn observer')
        input_files['world-ranged-survey.txt']=b'EVR_RANGED_SURVEY_1\n'
    if args.world_ranged_aim:
        if not weapon_runtime or not args.world_bow_spawn_observe or not args.world_ranged_animation:raise ValueError('ranged aim requires the Mock spawn observer and the reviewed staff/wand display')
        input_files['world-ranged-aim.txt']=b'EVR_RANGED_AIM_1\n'
    if args.world_bow_tracked_aim:
        if not weapon_runtime or not args.world_bow_physical_release or not args.world_bow_spawn_observe:raise ValueError('tracked aim requires Mock physical release and spawn observer')
        input_files['world-bow-tracked-aim.txt']=b'EVR_BOW_TRACKED_AIM_1\n'
    if args.world_bow_draw:
        if not weapon_runtime or not args.world_bow_tracked_aim:raise ValueError('bow draw requires the Mock tracked aim route')
        input_files['world-bow-draw.txt']=b'EVR_BOW_DRAW_1\n'
    if args.world_bow_physical_release:
        if not args.world_bow_physical_pose or not args.world_bow_physical_cancel or not weapon_runtime:raise ValueError('physical release requires Mock physical pose trial')
        input_files['world-bow-physical-release.txt']=b'EVR_BOW_PHYSICAL_RELEASE_1\n'
    if args.world_bow_physical_pose:
        if not args.world_bow_physical_cancel:raise ValueError('physical pose needs physical no-shot input trial')
        input_files['world-bow-physical-pose.txt']=b'EVR_BOW_PHYSICAL_POSE_1\n'
    if args.world_bow_physical_cancel:
        if not args.world_bow_animation or not args.world_native_grip_observe or args.world_bow_input_trial:raise ValueError('physical bow requires isolated Mock animation/mount trial')
        if not weapon_runtime or not args.world_live_input or (args.world_two_hand_wield and not args.world_bow_draw):raise ValueError('physical bow trial is isolated Mock live input only')
        import os
        config_path=user_settings_path()
        gp=json.loads(config_path.read_text(encoding='utf-8-sig'))['input']['gamepad']
        if gp['selectedPresetId']!='PresetA' or gp['bindings']['gameplay']:raise ValueError('physical bow trial requires existing unmodified PresetA')
        input_files['world-bow-physical-cancel.txt']=b'EVR_BOW_PHYSICAL_CANCEL_1\n'
    if args.world_bow_cancel_trial and (not args.world_bow_input_trial or args.world_bow_release_trial or args.world_bow_aim_trial):raise ValueError('cancel trial requires an isolated bow input trial')
    if args.world_bow_release_trial and (not args.world_bow_input_trial or args.world_bow_aim_trial):raise ValueError('release trial needs its own isolated bow input trial')
    if args.world_bow_aim_trial and not args.world_bow_input_trial:raise ValueError('bow aim trial needs bow input trial')
    if args.world_item_catalog:
        if launch.get('runtime')!='mock' or not args.world_live_input or not args.world_native_grip_observe:raise ValueError('item catalog needs Mock live native survey')
        input_files['world-item-catalog.txt']=b'EVR_ITEM_CATALOG_1\n'
    if args.world_shield_mount:
        if not args.world_native_grip_observe or not args.world_live_input:raise ValueError('shield mount needs explicit live native survey')
        if not weapon_runtime:raise ValueError('shield mount trial is Mock only')
        input_files['world-shield-mount.txt']=b'EVR_SHIELD_MOUNT_1\n'
    if args.world_ranged_animation:
        if not args.world_bow_animation or not weapon_runtime or not args.world_native_grip_observe:raise ValueError('ranged animation needs Mock bow animation survey')
        input_files['world-ranged-animation.txt']=b'EVR_RANGED_ANIMATION_1\n'
    if args.world_bow_animation:
        if not weapon_runtime or not args.world_live_input or not args.world_native_grip_observe or args.world_grip_preview:raise ValueError('bow animation needs Mock live mount survey')
        input_files['world-bow-animation.txt']=b'EVR_BOW_ANIMATION_1\n'
    if args.world_bow_input_trial:
        if launch.get('runtime')!='mock' or not args.world_live_input or not args.world_native_grip_observe or args.world_grip_preview or args.world_two_hand_wield:raise ValueError('bow input trial requires isolated Mock live mount survey')
        import os
        config_path=user_settings_path()
        gp=json.loads(config_path.read_text(encoding='utf-8-sig'))['input']['gamepad']
        if gp['selectedPresetId']!='PresetA' or gp['bindings']['gameplay']:raise ValueError('bow input trial requires existing unmodified PresetA')
        input_files['world-bow-input-trial.txt']=b'EVR_BOW_CANCEL_TRIAL_1\n' if args.world_bow_cancel_trial else b'EVR_BOW_RELEASE_TRIAL_1\n' if args.world_bow_release_trial else b'EVR_BOW_AIM_TRIAL_1\n' if args.world_bow_aim_trial else b'EVR_BOW_INPUT_TRIAL_1\n'
    if args.world_body_ik:
        if not args.world_live_input:raise ValueError('body ik experiment requires live input')
        input_files['world-body-ik.txt']=b'EVR_BODY_IK_1\n'
    if args.world_two_hand_wield:
        if not args.world_live_input or args.world_grip_preview:raise ValueError('two-hand wield requires live input and excludes legacy grip preview')
        if args.world_two_hand_catalog_only and not args.world_two_hand_wield_vhvr:raise ValueError('catalog-only two-hand mounts require the VHVR entry')
        input_files['world-two-hand-wield.txt']=b'EVR_LIVE_TWO_HAND_WIELD_3\n' if args.world_two_hand_catalog_only else b'EVR_LIVE_TWO_HAND_WIELD_2\n' if args.world_two_hand_wield_vhvr else b'EVR_LIVE_TWO_HAND_WIELD_1\n'
    elif args.world_two_hand_wield_vhvr or args.world_two_hand_catalog_only:raise ValueError('VHVR two-hand entry requires --world-two-hand-wield')
    if args.world_native_grip_observe:
        if not args.world_motion_hands:raise ValueError('native grip observation requires hands at proven world boundary')
        input_files['world-native-grip-observe.txt']=b'EVR_NATIVE_GRIP_OBSERVE_1\n'
    if args.world_motion_visual:
        if not launch.get('motion_hands') or not args.world_hands_only or not args.world_attachments_observe or not args.world_early_xr or not args.world_shared_culling_camera:raise ValueError('motion visual requires explicit hand actions, private appearance, attachment ownership and early XR')
        input_files['world-motion-visual.txt']=b'EVR_MOTION_VISUAL_1\n'
        if args.world_motion_hands:input_files['world-motion-hands.txt']=b'EVR_MOTION_HAND_VISUAL_1\n'
    if args.world_ui_state_observe:input_files['world-ui-state-observe.txt']=b'EVR_UI_STATE_OBSERVE_1\n'
    if args.world_culling_history:input_files['world-culling-history.txt']=b'EVR_CULLING_HISTORY_1\n'
    if args.world_culling_fill_only:input_files['world-culling-fill-only.txt']=b'EVR_CULLING_FILL_ONLY_1\n'
    if args.world_water_feedback:input_files['world-water-feedback.txt']=b'EVR_WATER_FEEDBACK_1\n'
    if args.world_root_camera:input_files['world-root-camera.txt']=f'EVR_ROOT_CAMERA_1 {1 if args.world_root_camera=="observe" else 2}\n'.encode()
    if args.world_camera_mode_fixture:input_files['world-camera-mode-fixture.txt']=b'EVR_CAMERA_MODE_FIXTURE_1\n'
    if args.world_camera_modes:input_files['world-camera-modes.txt']=b'EVR_CAMERA_MODES_1\n'
    if args.world_input_observe:input_files['world-input-observer.txt']=b'EVR_INPUT_OBSERVE_1\n'
    if args.world_input_walk_fixture:input_files['world-input-walk-fixture.txt']=b'EVR_INPUT_WALK_FIXTURE_1\n'
    if args.world_input_head_idle:input_files['world-input-head-idle.txt']=b'EVR_INPUT_HEAD_IDLE_1\n'
    if args.world_controller_jump_fixture:input_files['controller-ui-fixture.txt']=b'EVR_CONTROLLER_GLIDE_1\n' if args.world_controller_jump_fixture=='glide' else b'EVR_CONTROLLER_DOUBLE_JUMP_1\n' if args.world_controller_jump_fixture=='double' else b'EVR_CONTROLLER_JUMP_1\n'
    if args.world_controller_menu_fixture:input_files['controller-ui-fixture.txt']=b'EVR_CONTROLLER_ALTAR_1\n' if controller_altar else b'EVR_CONTROLLER_TOUR_1\n' if controller_tour else b'EVR_CONTROLLER_TABS_1\n' if controller_tabs else b'EVR_CONTROLLER_MENU_1\n'
    if args.world_input_head_walking:input_files['world-input-head-walking.txt']=b'EVR_INPUT_HEAD_WALKING_1\n'
    if args.world_live_input:
        if 'controller-ui-fixture.txt' in input_files or args.world_input_walk_fixture or args.world_camera_mode_fixture or args.world_input_head_idle:raise ValueError('live input excludes every controller/walk/camera fixture')
        if not args.world_live_xr or not args.world_endurance or not args.world_motion_hands or not args.world_input_observe or not args.world_input_head_walking or args.world_root_camera!='first-person':
            raise ValueError('live input needs live world XR, endurance, tracked motion hands, input observation, the walking head camera and the first-person root')
        input_files['controller-ui-fixture.txt']=b'EVR_CONTROLLER_LIVE_1\n'
    if args.world_weapon_pitch is not None:
        if not args.world_live_input or not -90<=args.world_weapon_pitch<=90:raise ValueError('weapon pitch needs live input and -90..90 degrees')
        input_files['world-weapon-mount.txt']=f'EVR_WEAPON_MOUNT_1 {args.world_weapon_pitch}\n'.encode()
    if args.world_hand_curl:
        if not args.world_motion_hands or args.world_grip_preview:raise ValueError('finger curl needs the tracked hands and is not combined with the grip preview hands')
        if not 0<=args.world_hand_curl_relaxed<=.9:raise ValueError('--world-hand-curl-relaxed must be 0..0.9')
        profile=json.loads((args.world_hand_curl/'profile.json').read_text())
        pose=(args.world_hand_curl/'hand-pose.bin').read_bytes()
        if len(pose)!=102*48 or hashlib.sha256(pose).hexdigest()!=profile['files']['hand-pose.bin'] or not profile.get('closed_fingers'):raise ValueError('closed hand pose does not match its grip profile')
        input_files['world-hand-curl.txt']=f'EVR_HAND_CURL_1 {args.world_hand_curl_relaxed}\n'.encode()
        input_files['hand-curl-closed-pose.bin']=pose
    if args.world_render_hook_batch:input_files['world-render-hook-batch.txt']=b'EVR_RENDER_HOOK_BATCH_1\n'
    if args.world_active_contact_owner:input_files['world-active-contact-owner.txt']=b'EVR_ACTIVE_CONTACT_OWNER_1\n'
    if args.world_interrupted_actor_owner:input_files['world-interrupted-actor-owner.txt']=b'EVR_INTERRUPTED_ACTOR_OWNER_1\n'
    if args.world_active_xr_exit:input_files['world-active-xr-exit.txt']=b'EVR_WORLD_ACTIVE_XR_EXIT_1\n'
    if args.world_endurance:input_files['world-endurance.txt']=b'EVR_WORLD_ENDURANCE_1\n'
    if args.world_session is not None:
        if not args.world_endurance or not args.world_live_xr or not 1<=args.world_session<=240:raise ValueError('play session needs --world-endurance, live world XR and 1..240 minutes')
        input_files['world-session.txt']=f'EVR_WORLD_SESSION_1 {args.world_session}\n'.encode()
    # Default since 2026-10-06 (user decision): live world series skip the original (desktop) View render; the desktop
    # shows the right-eye mirror. When the eyes borrow VFX lights/particles, eye 0 owns VFX on skipped frames.
    if args.world_desktop_render and (args.world_desktop_skip is not None or args.world_desktop_skip_vfx):raise ValueError('--world-desktop-render keeps the desktop render; do not combine it with desktop skip')
    # Full suppression leaves native world/GI state stale after moving. Preserve one original
    # View update per 30 candidates; eyes and their histories remain independent every frame.
    # The explicit Mock A/B fixture must still start from full suppression to reproduce it.
    if args.world_desktop_skip is None and args.world_live_xr and args.world_shared_updates_once and not args.world_desktop_render and not args.world_desktop_discard:
        args.world_desktop_skip=0 if args.world_desktop_compare else 30
    if args.world_desktop_skip is not None and (args.world_share_vfx_lights or getattr(args,'world_vfx_transparent_observe',False)):args.world_desktop_skip_vfx=True
    if args.world_desktop_skip_vfx:
        if args.world_desktop_skip is None:raise ValueError('VFX takeover needs --world-desktop-skip')
        input_files['world-desktop-skip-vfx.txt']=b'EVR_DESKTOP_SKIP_VFX_1\n'
    # Default since 2026-10-06: the right eye uses the left eye GI probes (world-space, updated once per frame).
    if args.world_gi_own and args.world_gi_share:raise ValueError('choose --world-gi-share or --world-gi-own')
    # 2026-10-09: GI share is opt-in again (right eye shadows darker with head motion); the default is per-eye GI.
    if args.world_gi_share:
        if not args.world_live_xr:raise ValueError('GI share needs live world XR')
        keep=args.world_gi_share_keep or 0
        if not 0<=keep<(1<<20):raise ValueError('--world-gi-share-keep is a 20-bit stage mask')
        input_files['world-gi-share.txt']=(f'EVR_GI_SHARE_1 {keep:x}\n' if keep else 'EVR_GI_SHARE_1\n').encode()
    if args.world_keep_dof:input_files['world-keep-dof.txt']=b'EVR_KEEP_DOF_1\n'
    # VR sharpness (2026-10-06): with DLSS the game passes 0 to its own sharpen pass, so the eyes get the value directly.
    sharp=args.world_vr_sharpness
    if sharp is None and args.world_live_xr:sharp=str(launch.get('vr_sharpness','game'))
    if sharp is not None and sharp not in ('game','unchanged','Keep'):
        if not args.world_live_xr:raise ValueError('VR sharpness needs live world XR')
        value=float(sharp)
        if not 0<=value<=1:raise ValueError('VR sharpness must be in 0..1')
        input_files['world-vr-sharpness.txt']=f'EVR_VR_SHARPNESS_1 {value:g}\n'.encode()
    # Default since 2026-10-06 (user): status vignettes become a faint red/blue rim on the outer side of each eye.
    if args.world_live_xr and args.world_shared_updates_once and not args.world_game_vignettes:input_files['world-edge-tint.txt']=f'EVR_EDGE_TINT_1 {args.world_status_tint:g}\n'.encode()
    if not 0<=args.world_status_tint<=1:raise ValueError('--world-status-tint must be 0..1')
    if args.world_controller_fixture_after is not None:
        if not 30<=args.world_controller_fixture_after<=3000:raise ValueError('--world-controller-fixture-after must be 30..3000')
        input_files['world-controller-fixture-arm.txt']=f'EVR_CONTROLLER_FIXTURE_ARM_1 {args.world_controller_fixture_after}\n'.encode()
    if args.world_force_freezing is not None:
        if not args.world_live_xr or not 0<=args.world_force_freezing<=4:raise ValueError('forced freezing needs live world XR and 0..4')
        input_files['world-force-freezing.txt']=f'EVR_FORCE_FREEZING_1 {args.world_force_freezing:g}\n'.encode()
    if args.world_object_watch:
        if not args.world_live_xr:raise ValueError('object watch needs live world XR')
        input_files['world-object-watch.txt']=b'EVR_OBJECT_WATCH_1\n'
    if args.world_gi_dump:
        if not args.world_live_xr:raise ValueError('GI dump needs live world XR')
        input_files['world-gi-dump.txt']=b'EVR_GI_DUMP_1\n'
    if args.world_desktop_discard:
        if not args.world_live_xr or args.world_desktop_skip is not None:raise ValueError('desktop discard needs live world XR and excludes desktop skip')
        input_files['world-desktop-discard.txt']=b'EVR_DESKTOP_DISCARD_1\n'
    if args.world_desktop_skip is not None:
        if not args.world_live_xr or not args.world_shared_updates_once or args.world_desktop_skip==1 or not 0<=args.world_desktop_skip<=60:raise ValueError('desktop skip needs live world XR, shared updates once and 0 or 2..60')
        if (args.world_share_vfx_lights or getattr(args,'world_vfx_transparent_observe',False)) and not args.world_desktop_skip_vfx:raise ValueError('desktop skip with borrowed VFX lights/particles needs --world-desktop-skip-vfx (eye 0 owns VFX)')
        input_files['world-desktop-skip.txt']=f'EVR_DESKTOP_SKIP_1 {args.world_desktop_skip}\n'.encode()
    if args.world_desktop_compare:
        if launch.get('runtime')!='mock' or not launch.get('mock_navigation') or not args.world_scenery_replay or args.world_desktop_skip is None:
            raise ValueError('desktop compare requires opted-in Mock, scenery replay and desktop skip')
        input_files['world-desktop-compare.txt']=b'EVR_DESKTOP_COMPARE_1\n'
    if args.world_eye_dealias:
        if not args.world_live_xr:raise ValueError('eye de-alias needs live world XR')
        input_files['world-eye-dealias.txt']=b'EVR_EYE_DEALIAS_1\n'
    # VR launches use DLSS by default (launch_diagnostics -VrUpscaler Dlss), which needs private eye transient pools.
    if args.world_live_xr and not args.world_eye_shared_transient:args.world_eye_private_transient=True
    if args.world_eye_shared_transient and args.world_eye_private_transient:raise ValueError('choose private or shared eye transient pools')
    if args.world_eye_private_transient:
        if not args.world_live_xr:raise ValueError('eye private transient pools need live world XR')
        input_files['world-eye-private-transient.txt']=b'EVR_EYE_PRIVATE_TRANSIENT_1\n'
    if args.world_dlss_observe:
        if not args.world_live_xr:raise ValueError('DLSS observation needs live world XR')
        input_files['world-dlss-eval-observe.txt']=b'EVR_DLSS_EVAL_OBSERVE_1\n'
    if args.desktop_render_scale is not None:
        if not args.world_live_xr or not .25<=args.desktop_render_scale<=1:raise ValueError('desktop render scale needs live world XR and 0.25..1.0')
        input_files['world-desktop-render-scale.txt']=f'EVR_DESKTOP_RENDER_SCALE_1 {args.desktop_render_scale}\n'.encode()
    if args.eye_render_scale is not None:
        if not args.world_live_xr or not .25<=args.eye_render_scale<=1:raise ValueError('eye render scale needs live world XR and 0.25..1.0')
        input_files['world-eye-render-scale.txt']=f'EVR_EYE_RENDER_SCALE_1 {args.eye_render_scale}\n'.encode()
    if args.world_controller_motion_burst:
        if not 1<=args.world_controller_motion_burst<=32 or not args.world_controller_jump_fixture or launch.get('renderdoc'):
            raise ValueError('motion burst requires explicit motion fixture,1..32pairs and no RenderDoc')
        input_files['controller-motion-burst.txt']=('EVR_CONTROLLER_MOTION_BURST_1 '+str(args.world_controller_motion_burst)+'\n').encode()
    if args.world_body_clip_distance is not None:
        if not launch.get('body_clip_shader') or not args.world_head_visibility:raise ValueError('body clip requires launch shader preparation and head/equipment scope')
        input_files['world-body-clip.txt']=('EVR_BODY_CLIP_1 '+str(args.world_body_clip_distance)+' '+' '.join(map(str,args.world_body_clip_parts))+'\n').encode()
    if args.world_body_motion_clip_distance is not None:
        import math
        if args.world_body_glide_clip_distance is not None or args.world_body_clip_distance is None or not args.world_camera_modes or not args.world_input_head_walking or not math.isfinite(args.world_body_motion_clip_distance) or not args.world_body_clip_distance<=args.world_body_motion_clip_distance<=1:
            raise ValueError('motion clip requires base, camera modes, walking observer, bounded finite distance and no glide policy')
        input_files['world-body-motion-clip.txt']=('EVR_BODY_MOTION_CLIP_1 '+str(args.world_body_motion_clip_distance)+'\n').encode()
    if args.world_body_glide_clip_distance is not None:
        import math
        if args.world_body_clip_distance is None or not args.world_camera_modes or not math.isfinite(args.world_body_glide_clip_distance) or not args.world_body_clip_distance<=args.world_body_glide_clip_distance<=1:
            raise ValueError('glide body clip requires base clip, camera modes and bounded finite distance')
        input_files['world-body-glide-clip.txt']=('EVR_BODY_GLIDE_CLIP_1 '+str(args.world_body_glide_clip_distance)+'\n').encode()
    if args.world_body_upload_observe:input_files['world-body-upload-observe.txt']=b'EVR_BODY_UPLOAD_OBSERVE_1\n'
    if args.world_equipment_observe:input_files['world-equipment-observer.txt']=b'EVR_EQUIPMENT_OBSERVE_1\n'
    if args.world_head_visibility:input_files['world-head-visibility.txt']=b'EVR_HEAD_VISIBILITY_1\n'
    if args.capture_glide:input_files['world-glide-capture.txt']=b'EVR_WORLD_GLIDE_CAPTURE_1\n'
    if args.capture_world_final:input_files['world-final-capture.txt']=b'EVR_WORLD_CAPTURE_1\n'
    if args.capture_on_request:
        # Mock navigation, or (2026-10-18) the headset, where the VR bug report button asks for it (hooks/bug_report.hpp)
        if not ((launch.get('runtime')=='mock' and launch.get('mock_navigation')) or launch.get('runtime')=='real_steamvr'):raise ValueError('manual capture requires opted-in Mock navigation or a headset launch')
        input_files['world-capture-on-request.txt']=b'EVR_WORLD_CAPTURE_REQUEST_1\n'
    if args.capture_initial_fog_sdf:input_files['world-initial-fog-sdf-capture.txt']=b'EVR_WORLD_FOG_SDF_CAPTURE_1\n'
    if args.world_shared_updates_once:input_files['world-shared-update-policy.txt']=b'EVR_WORLD_SHARED_ONCE_1\n'
    if args.world_share_vfx_lights:input_files['world-share-vfx-lights.txt']=b'EVR_SHARE_VFX_LIGHTS_1\n'
    if args.world_share_vfx_models:
        if not args.world_share_vfx_lights:p.error('--world-share-vfx-models requires --world-share-vfx-lights')
        input_files['world-share-vfx-models.txt']=b'EVR_SHARE_VFX_MODELS_1\n'
    if args.world_share_vfx_decals:
        if not args.world_share_vfx_lights:p.error('--world-share-vfx-decals requires --world-share-vfx-lights')
        input_files['world-share-vfx-decals.txt']=b'EVR_SHARE_VFX_DECALS_1\n'
    if args.world_early_xr:input_files['world-early-xr.txt']=b'EVR_WORLD_EARLY_XR_1\n'
    if args.world_scenery_replay:
        if launch.get('real_vr') or not args.world_live_xr or not args.world_early_xr:
            raise ValueError('scenery replay camera requires explicit Mock live XR and early shared camera')
        data=args.world_scenery_replay.read_bytes()
        fields=data.decode('utf-8').split()
        if len(data)>1024 or len(fields)!=8 or fields[0]!='EVR_SCENERY_REPLAY_1':raise ValueError('invalid scenery reference')
        input_files['world-scenery-replay.txt']=data
    if args.world_scenery_culling or args.world_scenery_no_hzb:input_files['world-scenery-culling.txt']=b'EVR_SCENERY_CULLING_OBSERVE_1\n'
    if args.world_scenery_no_hzb:input_files['world-scenery-no-hzb.txt']=b'EVR_SCENERY_NO_HZB_1\n'
    if args.world_fog_projection:
        if not args.world_live_xr:raise ValueError('Fog projection requires live world eyes')
        if args.world_fog_projection=='compare' and not args.world_scenery_replay:raise ValueError('Fog comparison requires a Mock scenery replay')
        input_files['world-fog-projection.txt']=('EVR_FOG_PROJECTION_1 '+args.world_fog_projection+'\n').encode()
    if args.world_shadow_projection:
        if not args.world_live_xr: raise ValueError('Shadow projection requires live native world eyes')
        input_files['world-shadow-projection.txt']=b'EVR_SHADOW_PROJECTION_1\n'
    if args.world_detail_bindings:
        if not args.world_live_xr:raise ValueError('private detail binding correction requires live world eyes')
        input_files['world-detail-bindings.txt']=b'EVR_DETAIL_BINDINGS_1\n'
    if args.world_eye_frustum:input_files['world-eye-frustum.txt']=('EVR_EYE_FRUSTUM_1 '+args.world_eye_frustum+'\n').encode()
    if args.world_shared_culling_camera:input_files['world-shared-culling-camera.txt']=b'EVR_SHARED_CULLING_CAMERA_1\n'
    if args.world_vfx_transparent_observe:input_files['world-vfx-transparent-observe.txt']=b'EVR_VFX_TRANSPARENT_OBSERVE_1\n'
    if args.world_vfx_transparent_render:input_files['world-vfx-transparent-render.txt']=b'EVR_VFX_TRANSPARENT_RENDER_1\n'
    if args.world_live_xr and not args.write_profile:
        proof=args.world_live_xr.resolve()
        if proof.parent!=args.launch.resolve() or not launch.get('render_cpu_arena_mib'):
            raise ValueError('world resource proof and native CPU arena must belong to this launch')
        records=[json.loads(line) for line in (proof/'view-resource-probe.jsonl').read_text().splitlines()]
        completed=[e for e in records if e['event']=='native_view_probe_complete']
        stages=[e for e in records if e['event']=='unused_world_resource_diagnostic']
        views=[e for e in records if e['event']=='native_view_created']
        post=json.loads((proof/'probe-postcheck.json').read_text())
        hook=[e for e in records if e['event']=='view_resource_hook_complete']
        # 1.0 (2026-10-16): a sharing that the unused proof left out as not ported to this build (optional_hook_skipped:
        # the per-eye culling history, the private water feedback) is not asked for in the session either.
        skipped={e.get('hook') for e in records if e['event']=='optional_hook_skipped'}
        if args.world_culling_history and 'culling_history' in skipped:
            args.world_culling_history=False;args.world_culling_fill_only=False
            input_files.pop('world-culling-history.txt',None);input_files.pop('world-culling-fill-only.txt',None)
            print('culling history left out: the unused proof skipped it (not ported to this build)')
        if args.world_water_feedback and 'water_feedback' in skipped:
            args.world_water_feedback=False;input_files.pop('world-water-feedback.txt',None)
            print('private water feedback left out: the unused proof skipped it (not ported to this build)')
        if args.world_culling_history:
            made=[e for e in records if e['event']=='culling_history_created']
            freed=[e for e in records if e['event']=='culling_history_released']
            if len(made)!=2 or len(freed)!=2 or len({e['object'] for e in made})!=2 or {e['index'] for e in made}!={e['index'] for e in freed} or any(e['object']==e['source_object'] for e in made):
                raise ValueError('private culling history lacks successful unused lifecycle proof from this launch')
        if args.world_water_feedback:
            created=[e for e in records if e['event']=='water_feedback_pool_created']
            released=[e for e in records if e['event']=='water_feedback_pool_released']
            failures=[e for e in records if e['event']=='water_feedback_failure']
            if len(created)!=2 or len(released)!=2 or {e['pool'] for e in created}!={e['pool'] for e in released} or failures:
                raise ValueError('private water pool creation/disposal lacks successful unused proof from this launch')
        if len(completed)!=1 or not all(completed[0].get(k) for k in ('two_views_destroyed','original_view_unchanged','scene_time_unchanged','back_pointer_restored')) or len(stages)!=1 or stages[0]['stage']!=2 or len(views)!=2 or len({(e['scene'],e['original_view']) for e in views})!=1 or post['status']!='PASS' or len(hook)!=1 or hook[0]['result']!=0:
            raise ValueError('world unused resolution creation/disposal has not passed')
        input_files['world-resource-proof.txt']=f"EVR_WORLD_RESOURCE_1 {views[0]['scene']} {views[0]['original_view']}\n".encode()
        input_files['world-resource-provenance.json']=json.dumps(dict(probe=str(proof),creation_filetime=launch['creation_filetime'],pid=launch['pid']),indent=2).encode()
    if args.live_xr:input_files['live-xr-config.txt']=f'EVR_LIVE_XR_1 {args.live_frames} {args.render_scale}\n'.encode()
    if args.recorded_xr:
        input_files['recorded-xr.txt']=args.recorded_xr.read_bytes()
        provenance=args.recorded_xr.with_suffix('.json')
        if provenance.exists():input_files['recorded-xr-provenance.json']=provenance.read_bytes()
    if args.write_profile:
        # Written by the loader for each session: the eye size, the minute limit (none), the sharpening, the proof.
        for name in ('live-xr-config.txt','world-session.txt'):
            if name not in input_files:raise ValueError('profile: '+name+' expected')
            del input_files[name]
        for name,content in sorted(input_files.items()):(args.write_profile/name).write_bytes(content)
        print('PROFILE',len(input_files),'files:',' '.join(sorted(input_files)))
        return 0
    crashes_before={p.name for p in (game.parent/'crash_dumps').iterdir()}
    result=inject(launch['pid'],game,out,dll,export,launch['creation_filetime'],timeout_ms=(args.world_session*60000+90000) if args.world_session else 435000 if args.world_endurance else 75000 if args.retained_calibration or args.prepared_xr_session or args.live_xr else 20000,input_files=input_files)
    print('Result:',result,'Output:',out)
    if not args.no_log_echo:print((out/filename).read_text())
    events=[json.loads(line) for line in (out/filename).read_text().splitlines()]
    if args.world_active_xr_exit or args.world_contact_end_retire:
        deadline=time.monotonic()+20;exit_file=args.launch/'process-exit.json'
        while not exit_file.exists() and time.monotonic()<deadline:time.sleep(.1)
        exit_data=json.loads(exit_file.read_text(encoding='utf-8-sig')) if exit_file.exists() else {}
        new_crashes={p.name for p in (game.parent/'crash_dumps').iterdir()}-crashes_before
        exited=any(r['event']=='world_active_xr_exit_complete' and r.get('result')==0 for r in events)
        if args.world_contact_end_retire:
            exited=any(r['event']=='contact_terminal_complete' and r.get('retired')==1 for r in events) and any(r['event']=='view_resource_hook_complete' and r.get('result')==0 for r in events)
        if args.world_contact_owner_exit:
            exited=exited and any(r['event']=='contact_owner_exit_complete' and r.get('result')==0 for r in events) and any(r['event']=='controller_owner_exit_complete' and r.get('result')==0 for r in events)
        survived=exit_data.get('status')=='exited' and exit_data.get('exit_code')==0 and not new_crashes and exited
        (out/'probe-postcheck.json').write_text(json.dumps(dict(status='PASS' if survived else 'FAIL',expected_normal_exit=True,process_exit=exit_data,new_crash_directories=sorted(new_crashes),**({'terminal_contact_completed':exited} if args.world_contact_end_retire else {'active_xr_exit_completed':exited}),scope='completed native action values retired at actual World end; no active combat or reentry claim' if args.world_contact_end_retire else 'active renderer/Scene exit; no active combat or reentry claim'),indent=2)+'\n',encoding='utf-8')
    else:survived=postcheck(launch,out,events[-1]['us'],crashes_before,game)
    overall=dict(status='PASS' if result==0 and survived else 'FAIL',native_result=result,postcheck_survived=survived,
                 limit='Surviving the probe does not imply the requested rendering completed; both results must pass.')
    (out/'probe-status.json').write_text(json.dumps(overall,indent=2),encoding='utf-8')
    print('OVERALL',overall)
    return result or int(not survived)


if __name__=='__main__':raise SystemExit(main())
