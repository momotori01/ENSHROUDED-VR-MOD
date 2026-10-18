"""Two calibrated physical swings, independent native acknowledgements and per-action ticket retirement.
No enemy contact/damage, unlimited GPU session, or real-HMD claim.
"""
import argparse,copy,json,struct
from pathlib import Path
from generate_melee_repeat_fixture import verify

def audit(rows,contact=False):
 ev=lambda n:[r for r in rows if r['event']==n]
 enabled=ev('melee_session_enabled');cmd=ev('melee_session_command');issued=[r for r in cmd if r['issued']];ack=[r for r in cmd if r['accepted']];retired=[r for r in cmd if r['retired']]
 selections={r['frame']:r for r in ev('motion_visual_selection')};tickets=ev('native_attack_ticket');ends=ev('melee_ticket_retired');samples=ev('motion_attack_sample');raw=ev('action_sequence_snapshot')
 native=[]
 for r in raw:
  b=bytes.fromhex(r['base_actor_bytes']);active=bool(b[0x1b1]&1)
  native.append(dict(us=r['us'],active=active,trigger=struct.unpack_from('<H',b,0x18)[0],sequence=struct.unpack_from('<I',b,0)[0],time=struct.unpack_from('<q',b,8)[0],can_primary=bool(struct.unpack_from('<I',b,0xbc8 if active else 0xc00)[0]&1)))
 c=dict(explicit_measured_two_action_trial=len(enabled)==1 and enabled[0].get('coordinator_unlimited') is True and enabled[0].get('fixture_attack_limit')==2 and enabled[0].get('direct_damage_writes') is False,
  two_new_physical_requests=[r['frame'] for r in issued]==[520,600] and [r['issued'] for r in issued]==[1,2] and all(r['can_primary'] and r['reason']==0 for r in issued),
  two_native_acknowledgements=[r['accepted'] for r in ack]==[1,2],
  two_native_retirements=[r['retired'] for r in retired]==[1,2] and all(r['current']==0 for r in retired),
  no_revocation=bool(cmd) and not any(r['revoked'] for r in cmd),
  current_render_owner=bool(cmd) and all(r['frame'] in selections and r['world']==selections[r['frame']]['world'] and int(r['world'],16)!=0 and r['native_ms'] for r in cmd),
  two_distinct_contact_tickets=[r['request'] for r in tickets]==[1,2] and [r['serial'] for r in tickets]==[1,2] and [r['frame'] for r in tickets]==[520,600],
  each_contact_ticket_retired=[r['request'] for r in ends]==[1,2] and [r['serial'] for r in ends]==[1,2] and all(r['valid'] is False and r['revoked'] is False for r in ends),
  raw_actor_read_only=bool(raw) and all(r['read_only'] and r['stable_read'] and r['entity']==r['local_entity'] for r in raw),
  two_distinct_native_actions=len({(r['sequence'],r['time']) for r in native if r['active'] and r['trigger']==1})==2,
  fixture_completed_neutral=bool(samples) and samples[-1]['phase']==5 and samples[-1]['completed_attacks']==2 and samples[-1]['buttons']==0,
  contact_geometry_not_enabled=not ev('motion_collider_apply_enabled') and not ev('motion_event_retime'))
 if contact:
  c.pop('contact_geometry_not_enabled')
  enabled_contact=ev('melee_session_contact_enabled')
  c['explicit_contact_mode']=len(enabled_contact)==1 and enabled_contact[0].get('expected_attacks')==2 and enabled_contact[0].get('measured_geometry') is True and bool(ev('motion_collider_apply_enabled')) and bool(ev('motion_event_retime'))
 c['issue_ack_end_before_next']=len(issued)==len(ack)==len(retired)==len(ends)==2 and all(issued[i]['us']<ack[i]['us']<retired[i]['us']<=ends[i]['us'] for i in range(2)) and ends[0]['us']<issued[1]['us']
 authoritative=True
 for start,accepted,end in zip(issued,ack,retired):
  before=[r for r in native if r['us']<=start['us']];at=[r for r in native if r['us']<=accepted['us']];after=[r for r in native if r['us']<=end['us']]
  authoritative &= bool(before and at and after) and not before[-1]['active'] and before[-1]['can_primary'] and at[-1]['active'] and at[-1]['trigger']==1 and not after[-1]['active']
 c['raw_native_permission_ack_end']=len(issued)==len(ack)==len(retired)==2 and authoritative
 spam=[r for r in samples if 532<=r['frame']<=537]
 c['six_fast_recovery_samples_rejected']=len(spam)==6 and all(3<r['speed']<4 and r['controller_speed'][0]==0 and r['active'] and r['accepted'] and r['remaining']>0 and r['blocked']==7 and r['request']==1 and r['gesture_request']==0 and r['buttons']==0 for r in spam)
 rest=[r for r in samples if 551<=r['frame']<600]
 c['no_queued_attack_on_recovery_end']=len(rest)>=35 and all(r['gesture_request']==0 and r['buttons']==0 for r in rest)
 c['both_recoveries_have_fading_feedback']=True
 for identity in (1,2):
  recovery=[r for r in samples if r['request']==identity and r['accepted'] and r['active'] and r['remaining']>0]
  ratios=[r['remaining']/r['total'] for r in recovery if r['total']>0]
  c['both_recoveries_have_fading_feedback'] &= len(ratios)>=5 and len(ratios)==len(recovery) and all(0<v<=1 for v in ratios) and all(a>=b for a,b in zip(ratios,ratios[1:])) and ratios[0]>ratios[-1]
 return c

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('probe',type=Path);p.add_argument('--negative-tests',action='store_true');a=p.parse_args();probe=a.probe
 if (probe/'world-melee-session.txt').read_text().strip()!='EVR_MELEE_SESSION_REPEAT_2':raise ValueError('explicit repeated coordinator trial required')
 fixture=verify(probe.parent/'mock-hands.csv',json.loads((probe/'grip-preview.json').read_text()))
 rows=[json.loads(l) for l in (probe/'view-resource-probe.jsonl').read_text().splitlines()];c=audit(rows)
 c['runner_PASS']=json.loads((probe/'probe-status.json').read_text())['status']=='PASS';c['stereo_600_pairs']=sum(r['event']=='live_native_pair_complete' for r in rows)==600
 c['normal_exit']=json.loads((probe.parent/'process-exit.json').read_text(encoding='utf-8-sig'))['exit_code']==0;c['Core_Sync_clean']=(probe.parent/'validation.log').stat().st_size==0
 if a.negative_tests:
  mutations=[('melee_session_command','accepted',0),('melee_session_command','retired',0),('melee_session_command','world','0x0'),('melee_session_command','can_primary',False),('melee_session_command','revoked',True),('native_attack_ticket','serial',1),('melee_ticket_retired','valid',True),('melee_ticket_retired','request',1),('motion_attack_sample','gesture_request',3),('motion_attack_sample','speed',0),('action_sequence_snapshot','stable_read',False)]
  for event,key,value in mutations:
   bad=copy.deepcopy(rows)
   for r in bad:
    if r['event']==event and (event!='motion_attack_sample' or 532<=r['frame']<=537):r[key]=value
   c['reject_'+event+'_'+key]=not all(audit(bad).values())
 report=dict(checks={k:'PASS' if v else 'FAIL' for k,v in c.items()},fixture=fixture,limitation=__doc__);(probe/'melee-repeat-checks.json').write_text(json.dumps(report,indent=2)+'\n')
 for k,v in c.items():print('PASS' if v else 'FAIL',k)
 print('PASS',sum(bool(v) for v in c.values()),'FAIL',sum(not v for v in c.values()));return int(not all(c.values()))
if __name__=='__main__':raise SystemExit(main())
