"""Audit LT charge and one finite native RT tap after physical-trigger release."""
import argparse,json,struct,collections
from pathlib import Path

def audit(probe):
 e=[json.loads(s) for s in (probe/'view-resource-probe.jsonl').read_text().splitlines()]
 phases=[x for x in e if x['event']=='bow_input_trial_phase'];release=next((x['us'] for x in phases if x['phase']==3),0);held=next((x['us'] for x in phases if x['phase']==2),0)
 pad=[x for x in e if x['event']=='controller_live_sample'];draw=[x for x in pad if held<=x['us']<release];shots=[x for x in pad if x['triggers'][1]]
 end=next((x['us'] for x in pad if shots and x['us']>shots[0]['us'] and x['triggers'][1]==0),0)
 raw=[x for x in e if x['event']=='input_bow_sample'];worlds={};taps=[]
 for x in raw:
  b=bytes.fromhex(x['weapon_bytes'])
  if struct.unpack_from('<I',b,0x4c)[0]!=327157552:continue
  w=worlds.setdefault(x['world'],[])
  if x['digital']&(1<<49):taps.append(dict(world=x['world'],us=x['us'],frame=x['frame']))
  if not w or w[-1]['state']!=b[0]:w.append(dict(us=x['us'],frame=x['frame'],state=b[0],ammo=struct.unpack_from('<I',b,0x60)[0],missing=struct.unpack_from('<H',b,0x6c)[0],queued=b[0x39]))
 checks=dict(explicit_release_trial=any(x['event']=='bow_input_trial_mode' and x.get('release_tap') for x in e),phases=[x['phase'] for x in phases]==[1,2,3],charge_without_shoot=bool(draw) and all(x['triggers']==[255,0] and not x['buttons']&0x4000 for x in draw),one_release_pulse=bool(shots) and all(release<=x['us']<=release+100000 for x in shots),pulse_duration=bool(shots) and 50000<=end-shots[0]['us']<=100000,native_charge=bool(worlds) and all(any(t['state']==2 and held<=t['us']<release for t in w) for w in worlds.values()),native_tap=bool(worlds) and set(worlds)=={t['world'] for t in taps} and all(t['us']>=release for t in taps),native_fire=bool(worlds) and all(any(t['state']==3 and release<=t['us']<release+1000000 and t['ammo'] and not t['missing'] for t in w) for w in worlds.values()),no_early_fire=bool(worlds) and all(all(t['us']>=release for t in w if t['state']==3) for w in worlds.values()),completed=any(x['event']=='view_resource_hook_complete' and x['result']==0 for x in e),no_failure=not any('failure' in x['event'] for x in e))
 return dict(status='PASS' if all(checks.values()) else 'FAIL',checks=checks,pulse_us=end-shots[0]['us'] if shots else None,phases=phases,worlds=worlds,native_taps=taps,limitation='Mock native input/state proof only; physical draw distance, cancellation under every scope, projectile origin/direction and real hardware remain unverified.')
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('probe',type=Path);a=p.parse_args();r=audit(a.probe);(a.probe/'bow-release-trial-check.json').write_text(json.dumps(r,indent=2)+'\n');print(json.dumps(r,indent=2));raise SystemExit(r['status']!='PASS')
