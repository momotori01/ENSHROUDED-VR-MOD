"""Independent single native mainhand action acceptance, not motion-contact proof."""
import argparse,json
from pathlib import Path
from check_action_sequences import check as action_check,decode

def check(rows):
    event=lambda name:[e for e in rows if e['event']==name]
    phases=event('controller_attack_phase');packets=event('controller_attack_packet_polled')
    restored=event('controller_fixture_restored');enabled=event('controller_attack_fixture_enabled')
    actions=[dict(decode(bytes.fromhex(x['base_actor_bytes'])),us=x['us'],entity=x['entity'],tick=x['tick']) for x in event('action_sequence_snapshot')]
    active=[x for x in actions if x['state']&1]
    equipment={p['item'] for p in phases};local={p['entity'] for p in phases}
    c={
        'explicit_single_button_no_direct_damage':len(enabled)==1 and enabled[0].get('single_button_pulse') and not enabled[0].get('direct_damage_writes',True) and enabled[0].get('stable_melee_appearance_required'),
        'phases_settle_press_observe_complete':[p['phase'] for p in phases]==[1,2,3,4],
        'one_X_pulse_then_neutral':[p['buttons'] for p in packets]==[0,16384,0] and all(p['right_trigger']==0 for p in packets),
        'stable_equipped_identity':len(equipment)==1 and 0 not in equipment and len(local)==1 and 0 not in local,
        'native_gameplay_ui_preserved':bool(phases) and all(p['screen']==1 and p['auxiliary']==0 and p['observation_valid'] for p in phases),
        'no_fixture_failures':not any('failure' in e['event'] for e in rows),
        'input_backend_restored':len(restored)==1 and all(restored[0].get(k) for k in ('pointers_restored','native_connection_seen','native_disconnection_seen','system_settings_unchanged')) and restored[0]['native_connected']==0,
        'real_native_action_observed':bool(active),
        'action_used_equipped_item':bool(active) and all(x['usedItem'] in equipment and x['entity'] in local and x['trigger_actorEntityId']==x['entity'] for x in active),
        'one_native_action_identity':len({(x['trigger_sequenceId'],x['trigger_triggerTime'],x['trigger_callIndex']) for x in active})==1,
        'native_playback_progress':len({x['currentTime'] for x in active})>=3 and all(x['sequenceLength']>0 for x in active),
        'idle_before_and_after':bool(active) and any(not(x['state']&1) and x['us']<active[0]['us'] for x in actions) and any(not(x['state']&1) and x['us']>active[-1]['us'] for x in actions),
        'activity_after_pulse_before_completion':len(phases)==4 and bool(active) and phases[1]['us']<=active[0]['us']<active[-1]['us']<=phases[3]['us'] and not phases[-1]['active'] and phases[-1]['active_samples']>0,
    }
    structural,_,_=action_check(rows);c.update(structural)
    metrics={'active_samples':len(active),'sequence_ids':sorted({x['trigger_sequenceId'] for x in active}),'sequence_lengths_raw':sorted({x['sequenceLength'] for x in active}),'time_unit_known':False}
    return c,metrics

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('probe',type=Path);p.add_argument('--negative-tests',action='store_true');a=p.parse_args()
    rows=[json.loads(x) for x in (a.probe/'view-resource-probe.jsonl').read_text().splitlines()]
    checks,metrics=check(rows)
    if a.negative_tests:
        import copy
        variants={}
        variants['missing_release']=[x for x in rows if not(x['event']=='controller_attack_packet_polled' and x['packet']==3)]
        variants['no_actions']=[x for x in rows if x['event']!='action_sequence_snapshot']
        variants['no_restore']=[x for x in rows if x['event']!='controller_fixture_restored']
        for name,key,val in [('wrong_item','item',1),('wrong_local','entity',5),('UI_override','screen',4),('early_complete','us',1)]:
            r=copy.deepcopy(rows);next(x for x in r if x['event']=='controller_attack_phase' and x['phase']==4)[key]=val;variants[name]=r
        for name,r in variants.items():checks['reject_'+name]=not all(check(r)[0].values())
    (a.probe/'native-attack-checks.json').write_text(json.dumps(dict(checks={k:'PASS' if v else 'FAIL' for k,v in checks.items()},metrics=metrics,limitation=__doc__),indent=2)+'\n')
    for k,v in checks.items():print('PASS' if v else 'FAIL',k)
    print('PASS',sum(bool(v) for v in checks.values()),'FAIL',sum(not v for v in checks.values()));return int(not all(checks.values()))
if __name__=='__main__':raise SystemExit(main())
