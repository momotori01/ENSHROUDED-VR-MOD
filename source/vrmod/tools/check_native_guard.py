"""Native LB -> gameplay Block -> actor Blocking, two finite holds. No enemy/parry success proof."""
import argparse,copy,json,struct
from pathlib import Path

def audit(rows):
 ev=lambda name:[r for r in rows if r['event']==name]
 phases=ev('guard_fixture_phase');packets=ev('controller_fixture_packet_polled');snaps=ev('action_sequence_snapshot')
 compressed=[]
 for r in packets:
  if not compressed or r['buttons']!=compressed[-1]:compressed.append(r['buttons'])
 c={'explicit_nonattack_fixture':len(ev('guard_fixture_enabled'))==1 and ev('guard_fixture_enabled')[0].get('buttons_only')==[256] and ev('guard_fixture_enabled')[0].get('attack_commands') is False and ev('guard_fixture_enabled')[0].get('native_state_writes') is False,
    'two_native_LB_pulses_only':compressed==[0,256,0,256,0],
    'valid_single_local_owner':bool(phases) and len({r['root'] for r in phases})==1 and len({r['item'] for r in phases})==1 and all(r['root'] and r['item'] and r['observation_valid'] and r['screen']==1 for r in phases),
    'complete_with_two_holds':bool(phases) and phases[-1]['phase']==4 and phases[-1]['holds']==2 and not any(r['phase']==5 for r in phases),
    'no_combat_fixture':not any(ev(x) for x in ('native_attack_ticket','motion_attack_fixture_enabled','controller_attack_fixture_enabled','motion_collider_apply_enabled'))}
 spans=[];active=None
 for r in phases:
  if r['phase']==2 and active is None:active=r
  if active and r['phase']==3:spans.append((active,r));active=None
 observed=[];native=[]
 for s in snaps:
  b=bytes.fromhex(s['base_actor_bytes']);state=struct.unpack_from('<Q',b,0xbf8)[0];a=bool(b[0x1b1]&1)
  if a:state=(state|struct.unpack_from('<Q',b,0xbd0)[0])&~struct.unpack_from('<Q',b,0xbd8)[0]
  raw,scope=struct.unpack_from('<I',b,0xc00)[0],struct.unpack_from('<I',b,0xbc8)[0]
  native.append(dict(us=s['us'],digital=s['input_digital'],blocking=bool(state&4),can_block=bool((raw&scope if a else raw)&64),active=a,trigger=struct.unpack_from('<H',b,0x18)[0]))
 for start,end in spans:
  subset=[r for r in native if start['us']<=r['us']<end['us']]
  hits=[r for r in subset if r['blocking'] and r['can_block'] and r['digital'] in (1<<28,(1<<28)|(1<<4))]
  observed.append(dict(duration_ms=end['milliseconds']-start['milliseconds'],samples=len(subset),block_samples=len(hits)))
 c['both_holds_reach_native_blocking']=len(spans)==2 and all(r['duration_ms']>=1200 and r['block_samples']>=5 for r in observed)
 c['digital_inputs_only_block_or_verified_PresetA_alias']=bool(native) and all(r['digital'] in (0,1<<28,(1<<28)|(1<<4)) for r in native)
 c['no_primary_attack_sequence']=bool(native) and not any(r['active'] and r['trigger']==1 for r in native)
 releases=[r for r in phases if r['phase']==3 and not r['blocking'] and r['digital']==0]
 c['both_releases_observed_neutral']=len(releases)>=2 and bool(native) and not native[-1]['blocking'] and native[-1]['digital']==0
 c['actor_copies_stable_read_only']=bool(snaps) and all(s['entity']==s['local_entity'] and s['stable_read'] and s['read_only'] for s in snaps)
 restored=ev('controller_fixture_restored');c['native_input_restored']=len(restored)==1 and all(restored[0].get(k) is True for k in ('pointers_restored','native_connection_seen','native_disconnection_seen','system_settings_unchanged')) and restored[0]['native_connected']==restored[0]['rescan_pending']==0
 return c,dict(packets=compressed,holds=observed,raw_actor_samples=len(native),active_triggers=sorted({r['trigger'] for r in native if r['active']}))

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('probe',type=Path);p.add_argument('--negative-tests',action='store_true');a=p.parse_args();probe=a.probe
 if (probe/'controller-ui-fixture.txt').read_text().strip()!='EVR_CONTROLLER_GUARD_1':raise ValueError('explicit native guard fixture required')
 rows=[json.loads(l) for l in (probe/'view-resource-probe.jsonl').read_text().splitlines()];c,m=audit(rows)
 c['native_stereo_600_pairs']=sum(r['event']=='live_native_pair_complete' for r in rows)==600
 c['runner_PASS']=json.loads((probe/'probe-status.json').read_text())['status']=='PASS'
 c['normal_exit']=json.loads((probe.parent/'process-exit.json').read_text(encoding='utf-8-sig'))['exit_code']==0
 c['Core_Sync_clean']=(probe.parent/'validation.log').stat().st_size==0
 if a.negative_tests:
  for event,key,value in [('guard_fixture_phase','phase',5),('guard_fixture_phase','root',0),('guard_fixture_enabled','native_state_writes',True),('controller_fixture_packet_polled','buttons',16384),('action_sequence_snapshot','input_digital',1),('action_sequence_snapshot','stable_read',False),('controller_fixture_restored','pointers_restored',False)]:
   bad=copy.deepcopy(rows)
   for r in bad:
    if r['event']==event:r[key]=value
   c['reject_'+event+'_'+key]=not all(audit(bad)[0].values())
  for label in ('blocking_cleared','primary_sequence'):
   bad=copy.deepcopy(rows)
   for r in bad:
    if r['event']!='action_sequence_snapshot':continue
    b=bytearray.fromhex(r['base_actor_bytes'])
    if label=='blocking_cleared':
     for offset in (0xbf8,0xbd0):struct.pack_into('<Q',b,offset,struct.unpack_from('<Q',b,offset)[0]&~4)
    else:struct.pack_into('<H',b,0x18,1)
    r['base_actor_bytes']=b.hex()
   c['reject_raw_actor_'+label]=not all(audit(bad)[0].values())
 report=dict(checks={k:'PASS' if v else 'FAIL' for k,v in c.items()},measurements=m,limitation=__doc__)
 (probe/'native-guard-checks.json').write_text(json.dumps(report,indent=2)+'\n')
 for k,v in c.items():print('PASS' if v else 'FAIL',k)
 print('PASS',sum(bool(v) for v in c.values()),'FAIL',sum(not v for v in c.values()));return int(not all(c.values()))
if __name__=='__main__':raise SystemExit(main())
