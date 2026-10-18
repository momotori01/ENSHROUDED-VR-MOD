"""Unlimited melee coordinator in a bounded native one-action trial. No damage/continuous-gameplay claim."""
import argparse,copy,json,struct
from pathlib import Path

def audit(rows):
 ev=lambda n:[r for r in rows if r['event']==n]
 enabled=ev('melee_session_enabled');cmd=ev('melee_session_command');issued=[r for r in cmd if r['issued']];accepted=[r for r in cmd if r['accepted']];retired=[r for r in cmd if r['retired']]
 selections={r['frame']:r for r in ev('motion_visual_selection')};tickets=ev('native_attack_ticket');samples=ev('motion_attack_sample')
 raw=ev('action_sequence_snapshot');native=[]
 for r in raw:
  b=bytes.fromhex(r['base_actor_bytes']);active=bool(b[0x1b1]&1)
  native.append(dict(us=r['us'],active=active,trigger=struct.unpack_from('<H',b,0x18)[0],sequence=struct.unpack_from('<I',b,0)[0],time=struct.unpack_from('<q',b,8)[0],can_primary=bool(struct.unpack_from('<I',b,0xbc8 if active else 0xc00)[0]&1)))
 c=dict(explicit_bounded_coordinator=len(enabled)==1 and enabled[0].get('coordinator_unlimited') is True and enabled[0].get('fixture_attack_limit')==1 and enabled[0].get('direct_damage_writes') is False,
  one_physical_request=len(issued)==1 and issued[0]['frame']==520 and issued[0]['issued']==1 and issued[0]['can_primary'] and issued[0]['reason']==0,
  one_native_ack=len(accepted)==1 and accepted[0]['accepted']==1,
  one_native_retirement=len(retired)==1 and retired[0]['retired']==1 and retired[0]['current']==0,
  no_revocation=bool(cmd) and not any(r['revoked'] for r in cmd),
  qualified_client_world=bool(cmd) and all(r['frame'] in selections and r['world']==selections[r['frame']]['world'] and int(r['world'],16)!=0 and r['native_ms'] for r in cmd),
  request_ack_retire_order=len(issued)==len(accepted)==len(retired)==1 and issued[0]['us']<accepted[0]['us']<retired[0]['us'],
  one_contact_authority_ticket=len(tickets)==1 and tickets[0]['request']==1 and tickets[0]['frame']==520,
  native_sequence_identity=len({(r['sequence'],r['time']) for r in native if r['active'] and r['trigger']==1})==1,
  actor_copies_unchanged=bool(raw) and all(r['read_only'] and r['stable_read'] and r['entity']==r['local_entity'] for r in raw),
  complete_fixture=bool(samples) and samples[-1]['phase']==5 and samples[-1]['completed_attacks']==1 and samples[-1]['buttons']==0,
  no_new_contact_geometry=not ev('motion_collider_apply_enabled') and not ev('motion_event_retime'))
 if len(issued)==len(accepted)==len(retired)==1:
  before=[r for r in native if r['us']<=issued[0]['us']];ack=[r for r in native if r['us']<=accepted[0]['us']];end=[r for r in native if r['us']<=retired[0]['us']]
  c['raw_native_permission_ack_and_end']=bool(before and ack and end) and not before[-1]['active'] and before[-1]['can_primary'] and ack[-1]['active'] and ack[-1]['trigger']==1 and not end[-1]['active']
 else:c['raw_native_permission_ack_and_end']=False
 return c

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('probe',type=Path);p.add_argument('--negative-tests',action='store_true');a=p.parse_args();probe=a.probe
 if (probe/'world-melee-session.txt').read_text().strip()!='EVR_MELEE_SESSION_1':raise ValueError('explicit coordinator fixture required')
 rows=[json.loads(l) for l in (probe/'view-resource-probe.jsonl').read_text().splitlines()];c=audit(rows)
 c['runner_PASS']=json.loads((probe/'probe-status.json').read_text())['status']=='PASS';c['stereo_600_pairs']=sum(r['event']=='live_native_pair_complete' for r in rows)==600
 c['normal_exit']=json.loads((probe.parent/'process-exit.json').read_text(encoding='utf-8-sig'))['exit_code']==0;c['Core_Sync_clean']=(probe.parent/'validation.log').stat().st_size==0
 if a.negative_tests:
  for event,key,value in [('melee_session_command','accepted',0),('melee_session_command','retired',0),('melee_session_command','world','0x0'),('melee_session_command','can_primary',False),('melee_session_command','revoked',True),('melee_session_enabled','direct_damage_writes',True),('native_attack_ticket','request',2),('action_sequence_snapshot','stable_read',False)]:
   bad=copy.deepcopy(rows)
   for r in bad:
    if r['event']==event:r[key]=value
   c['reject_'+event+'_'+key]=not all(audit(bad).values())
 report=dict(checks={k:'PASS' if v else 'FAIL' for k,v in c.items()},limitation=__doc__);(probe/'melee-session-checks.json').write_text(json.dumps(report,indent=2)+'\n')
 for k,v in c.items():print('PASS' if v else 'FAIL',k)
 print('PASS',sum(bool(v) for v in c.values()),'FAIL',sum(not v for v in c.values()));return int(not all(c.values()))
if __name__=='__main__':raise SystemExit(main())
