"""Audit a no-shot bow cancel against native input/state evidence in both Worlds."""
import argparse,collections,json,struct
from pathlib import Path

def audit(probe,reason):
 e=[json.loads(s) for s in (probe/'view-resource-probe.jsonl').read_text().splitlines()];ph=[x for x in e if x['event']=='bow_input_trial_phase'];held=next((x['us'] for x in ph if x['phase']==2),0);want=3 if reason=='release' else 4;cancel=next((x['us'] for x in ph if x['phase']==want),0)
 pads=[x for x in e if x['event']=='controller_live_sample'];draw=[x for x in pads if held<=x['us']<cancel];after=[x for x in pads if cancel<x['us']<cancel+1000000]
 raw=[x for x in e if x['event']=='input_bow_sample'];worlds=collections.defaultdict(list)
 for x in raw:
  b=bytes.fromhex(x['weapon_bytes']);worlds[x['world']].append(dict(us=x['us'],frame=x['frame'],state=b[0],item=struct.unpack_from('<I',b,0x4c)[0],digital=x['digital'],queued=b[0x39],ammo=struct.unpack_from('<I',b,0x60)[0],missing=struct.unpack_from('<H',b,0x6c)[0],read_only=x['read_only'] and x['source_unchanged']))
 checks=dict(explicit_cancel=any(x['event']=='bow_input_trial_mode' and x.get('cancel_only') and not x['release_tap'] for x in e),phases=[x['phase'] for x in ph]==[1,2,want],no_pad_fire=bool(pads) and all(not x['triggers'][1] and not x['buttons']&0x4000 for x in pads),charge_without_shoot=bool(draw) and all(x['triggers']==[255,0] for x in draw),release_drops_aim=bool(after) and all(x['triggers']==[0,0] for x in after),two_worlds=len(worlds)==2,native_charge=True,native_holster=True,no_native_shoot=True,no_native_fire=True,source_unchanged=True,completed=any(x['event']=='view_resource_hook_complete' and x['result']==0 for x in e),no_failure=not any('failure' in x['event'] for x in e))
 summaries={}
 for key,w in worlds.items():
  charge=[x for x in w if held<=x['us']<cancel and x['state']==2 and x['item']==327157552 and x['digital']&(1<<47) and x['ammo'] and not x['missing']]
  holster=[x for x in w if cancel<=x['us']<cancel+1000000 and x['state']==0 and not x['digital']&(1<<47)]
  checks['native_charge'] &= bool(charge);checks['native_holster'] &= bool(holster)
  checks['no_native_shoot'] &= not any(x['digital']&((1<<48)|(1<<49)) for x in w)
  checks['no_native_fire'] &= not any(x['state'] in [3,4] or x['queued'] for x in w)
  checks['source_unchanged'] &= all(x['read_only'] for x in w)
  transitions=[]
  for x in w:
   if not transitions or transitions[-1]['state']!=x['state']:transitions.append({k:x[k] for k in ['us','frame','state','item']})
  summaries[key]=dict(samples=len(w),charge_samples=len(charge),holster_delay_us=holster[0]['us']-cancel if holster else None,transitions=transitions)
 return dict(status='PASS' if all(checks.values()) else 'FAIL',checks=checks,reason=reason,phases=ph,worlds=summaries,limitation='One known bow and one specified Mock cancellation condition. No proof for all UI, equip, disconnect, session changes, physical draw power, projectile aim or hardware.')
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('probe',type=Path);p.add_argument('--reason',choices=['release','scope'],default='release');a=p.parse_args();r=audit(a.probe,a.reason);(a.probe/'bow-cancel-trial-check.json').write_text(json.dumps(r,indent=2)+'\n');print(json.dumps(r,indent=2));raise SystemExit(r['status']!='PASS')
