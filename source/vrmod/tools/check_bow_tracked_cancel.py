"""Known Mock physical release candidate: tracking loss before release must not fire."""
import argparse,json,struct
from pathlib import Path

def audit(p):
 e=[json.loads(s) for s in (p/'view-resource-probe.jsonl').read_text().splitlines()];ev=lambda n:[x for x in e if x['event']==n]
 physical=ev('bow_physical_trial_sample');held=next((x for x in physical if x['phase']==2),{});aborted=next((x for x in physical if x['phase']==4),{});at=aborted.get('us',0);pads=ev('controller_live_sample')
 rows={}
 for x in ev('input_bow_sample'):
  b=bytes.fromhex(x['weapon_bytes'])
  if struct.unpack_from('<I',b,0x4c)[0]!=327157552:continue
  rows.setdefault(x['world'],[]).append(dict(us=x['us'],state=b[0],digital=x['digital'],queued=b[0x39],source_unchanged=x['source_unchanged']))
 summaries={k:dict(charge=any(v['state']==2 and held.get('us',0)<=v['us']<at for v in a),holster_delay_us=next((v['us']-at for v in a if at<=v['us']<at+1000000 and v['state']==0 and not v['digital']&(1<<47)),None)) for k,a in rows.items()}
 removed=ev('bow_spawn_removed')
 checks=dict(tracked_mode=len(ev('bow_tracked_aim_enabled'))==1,acquired=held.get('frame')==540,aborted_before_release=aborted.get('frame')==700 and aborted.get('triggers')==[0,0],
 no_rearm=bool(aborted) and not any(x['phase'] in [2,5,6] and x['us']>at for x in physical),no_pad_fire=bool(pads) and all(x['triggers'][1]==0 and not x['buttons']&0x4000 for x in pads),
 no_shot_latch=not ev('bow_tracked_shot_armed'),no_projectile=not ev('bow_tracked_aim_call') and not ev('bow_spawn_sample') and not ev('bow_spawn_submit_result'),
 native_cancel=len(rows)==2 and all(x['charge'] and x['holster_delay_us'] is not None for x in summaries.values()),
 no_native_fire=bool(rows) and all(x['state'] not in [3,4] and not x['queued'] and not x['digital']&((1<<48)|(1<<49)) and x['source_unchanged'] for a in rows.values() for x in a),
 observer_retired=len(removed)==1 and removed[0]['restored'] and not any(removed[0][k] for k in ['calls','samples','dispatch_samples','submit_samples','failures','callbacks']),
 completed=any(x['result']==0 for x in ev('view_resource_hook_complete')),no_failure=not any('failure' in x['event'] for x in e))
 return dict(status='PASS' if all(checks.values()) else 'FAIL',checks=checks,worlds=summaries,limitation='Single known Mock right-hand tracking loss before release. Does not establish all UI/equip/session/disconnect cases or cancelling a projectile after native creation.')
if __name__=='__main__':
 a=argparse.ArgumentParser(description=__doc__);a.add_argument('probe',type=Path);v=a.parse_args();r=audit(v.probe);(v.probe/'bow-tracked-cancel-check.json').write_text(json.dumps(r,indent=2)+'\n',encoding='utf-8');print(json.dumps(r,indent=2));raise SystemExit(r['status']!='PASS')
