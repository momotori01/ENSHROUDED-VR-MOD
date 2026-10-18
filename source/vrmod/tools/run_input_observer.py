"""Bounded read-only gameplay input/UI observations in a managed launch."""
import argparse,hashlib,json,pathlib,time
from inject_camera_observer import inject
from run_render_resource_probe import postcheck

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--launch',type=pathlib.Path,required=True)
    p.add_argument('--seconds',type=int,default=45)
    p.add_argument('--controller-neutral-fixture',action='store_true',help='TEST ONLY: process-local neutral XInput connect/disconnect; no buttons/axes or OS driver')
    p.add_argument('--controller-tabs-fixture',action='store_true',help='TEST ONLY: native controller UI selection/tab navigation, no inventory mutation')
    p.add_argument('--controller-menu-fixture',action='store_true',help='TEST ONLY: native XInput START menu open/close; no movement/attack/confirm buttons')
    p.add_argument('--controller-altar-fixture',action='store_true',help='TEST ONLY: nearby Flame Altar Y open / B cancel; no confirmation or upgrades')
    p.add_argument('--controller-jump-fixture',choices=['single','double','glide'],help='TEST ONLY: bounded A pulse(s), actual native air/ground state required')
    p.add_argument('--identity-shadow',action='store_true',help='Route an identical private input copy through the native builder; no field changes')
    p.add_argument("--action-sequences",action="store_true",help="Read-only full BaseActor snapshots; does not trigger combat or alter cooldown")
    p.add_argument("--controller-attack-fixture",action="store_true",help="TEST ONLY: one bounded native pulse; requires eligible selection and read-only action snapshots")
    p.add_argument("--attachment-candidates",action="store_true",help="Read-only attachment/resource candidates with native owner checks; no model creation or scene mutation")
    p.add_argument("--controller-attack-input",choices=["rt","rb","x"],default="rt",help="Diagnostic physical input; RT was observed as SpecialAbility on current PresetA")
    p.add_argument("--combat-event-retime",type=pathlib.Path,help="TEST ONLY: successful shadow proof required to advance one native Impact and suppress its original occurrence")
    p.add_argument("--combat-event-selection-shadow",action="store_true",help="Run native Impact filter on private times/output; no gameplay change")
    p.add_argument("--combat-event-selection-observe",action="store_true",help="Read-only active event selection during native local attack")
    p.add_argument("--combat-contact-observe",action="store_true",help="Read-only impact, sequence collider and active sequence asset observation during one X attack")
    p.add_argument("--combat-collider-identity",action="store_true",help="One native attack with identical private stack-query transform copies; no ECS transform writes")
    p.add_argument("--combat-target-observe",action="store_true",help="Read-only native Health/current-transform inventory; no input fixture")
    a=p.parse_args()
    if a.combat_target_observe and (not a.action_sequences or any([a.controller_attack_fixture,a.controller_neutral_fixture,a.controller_menu_fixture,a.controller_tabs_fixture,a.controller_altar_fixture,a.controller_jump_fixture])):p.error("target observation requires action sequences and no input fixture")
    if a.combat_event_retime and not a.combat_event_selection_shadow:p.error("retime needs shadow and observer")
    if a.combat_event_selection_shadow and not a.combat_event_selection_observe:p.error("shadow needs event selection observation")
    if a.combat_event_selection_observe and not a.combat_contact_observe:p.error("event selection needs contact observer")
    if a.combat_collider_identity and not a.combat_contact_observe:p.error("identity test requires combat contact observer")
    if a.combat_contact_observe and (not a.action_sequences or not a.controller_attack_fixture or a.controller_attack_input!="x"):p.error("contact observation needs action sequences and one X fixture")
    if a.controller_attack_input!="rt" and not a.controller_attack_fixture:p.error("attack input requires attack fixture")
    if not 5<=a.seconds<=60:p.error('duration must be 5..60 seconds')
    if a.controller_neutral_fixture and a.seconds<10:p.error('neutral controller fixture needs at least10seconds')
    if a.controller_menu_fixture and (a.controller_neutral_fixture or a.seconds<15):p.error('menu fixture requires15seconds and cannot combine with neutral mode')
    if a.controller_tabs_fixture and (a.controller_neutral_fixture or a.controller_menu_fixture or a.seconds<30):p.error('tab fixture requires30seconds and excludes other controller fixtures')
    if a.controller_altar_fixture and (a.controller_neutral_fixture or a.controller_menu_fixture or a.controller_tabs_fixture or a.seconds<15):p.error('altar fixture requires15seconds and excludes other controller fixtures')
    if a.controller_jump_fixture and (a.controller_neutral_fixture or a.controller_menu_fixture or a.controller_tabs_fixture or a.controller_altar_fixture or a.seconds<20):p.error('jump fixture requires20seconds and excludes other fixtures')
    if a.controller_attack_fixture and (not a.action_sequences or a.seconds<20 or a.controller_neutral_fixture or a.controller_menu_fixture or a.controller_tabs_fixture or a.controller_altar_fixture or a.controller_jump_fixture):p.error('attack requires action observation,20seconds, and excludes other fixtures')
    mod=pathlib.Path(__file__).resolve().parents[1];game=mod.parent/'enshrouded.exe'
    profile=json.loads((mod/'config/profiles/build23966345.json').read_text())
    if hashlib.sha256(game.read_bytes()).hexdigest()!=profile['exe_sha256']:raise ValueError('unsupported executable hash')
    launch=json.loads((a.launch/'launch.json').read_text(encoding='utf-8-sig'))
    if time.time()-(a.launch/'vulkan.jsonl').stat().st_mtime>10:raise ValueError('managed game telemetry is stale')
    dll=pathlib.Path(json.loads((mod/'build/bin/Release/evr_view_resource_probe.json').read_text())['library_path'])
    out=a.launch/time.strftime('input-observer-%Y%m%d-%H%M%S')
    crashes={s.name for s in (game.parent/'crash_dumps').iterdir()}
    policy={'input-observer-duration.txt':f'EVR_INPUT_DURATION_1 {a.seconds}\n'.encode()}
    if a.combat_collider_identity:policy['combat-collider-identity.txt']=b'EVR_COLLIDER_IDENTITY_1\n'
    if a.combat_event_retime:
        from check_event_selection import audit
        proof=[json.loads(x) for x in (a.combat_event_retime/'view-resource-probe.jsonl').read_text(encoding='utf8').splitlines()]
        checks,metrics=audit(proof,True)
        if not all(checks.values()):raise ValueError('event shadow proof rejected')
        policy['combat-event-retime.txt']=b'EVR_EVENT_RETIME_1\n'
        policy['event-retime-shadow-proof.json']=json.dumps(dict(source=str(a.combat_event_retime.resolve()),checks=checks,metrics=metrics),indent=2).encode()
    if a.combat_event_selection_shadow:policy['combat-event-selection-shadow.txt']=b'EVR_EVENT_SELECTION_SHADOW_1\n'
    if a.combat_event_selection_observe:policy['combat-event-selection-observe.txt']=b'EVR_EVENT_SELECTION_OBSERVE_1\n'
    if a.combat_contact_observe:policy['combat-contact-observe.txt']=b'EVR_COMBAT_CONTACT_OBSERVE_1\n'
    if a.combat_target_observe:policy['combat-target-observe.txt']=b'EVR_TARGET_OBSERVE_1\n'
    if a.action_sequences:policy['action-sequence-observer.txt']=b'EVR_ACTION_SEQUENCE_OBSERVE_1\n'
    if a.identity_shadow:policy['input-shadow-identity.txt']=b'EVR_INPUT_SHADOW_IDENTITY_1\n'
    if a.controller_neutral_fixture:policy['controller-ui-fixture.txt']=b'EVR_CONTROLLER_NEUTRAL_1\n'
    if a.controller_menu_fixture:policy['controller-ui-fixture.txt']=b'EVR_CONTROLLER_MENU_1\n'
    if a.controller_tabs_fixture:policy['controller-ui-fixture.txt']=b'EVR_CONTROLLER_TABS_1\n'
    if a.controller_altar_fixture:policy['controller-ui-fixture.txt']=b'EVR_CONTROLLER_ALTAR_1\n'
    if a.controller_jump_fixture:policy['controller-ui-fixture.txt']=b'EVR_CONTROLLER_GLIDE_1\n' if a.controller_jump_fixture=='glide' else b'EVR_CONTROLLER_DOUBLE_JUMP_1\n' if a.controller_jump_fixture=='double' else b'EVR_CONTROLLER_JUMP_1\n'
    if a.controller_attack_fixture:policy['controller-ui-fixture.txt']={'rb':b'EVR_CONTROLLER_ATTACK_RB_1\n','x':b'EVR_CONTROLLER_ATTACK_X_1\n','rt':b'EVR_CONTROLLER_ATTACK_1\n'}[a.controller_attack_input]
    if a.attachment_candidates or a.controller_attack_fixture:policy['attachment-candidates.txt']=b'EVR_ATTACHMENT_CANDIDATES_1\n'
    result=inject(launch['pid'],game,out,dll,b'EVR_ObserveGameplayInput',launch['creation_filetime'],
                  timeout_ms=(a.seconds+10)*1000,input_files=policy)
    events=[json.loads(s) for s in (out/'view-resource-probe.jsonl').read_text().splitlines()]
    survived=postcheck(launch,out,events[-1]['us'],crashes,game)
    print('Result:',result,'Output:',out);return result or int(not survived)

if __name__=='__main__':raise SystemExit(main())
