"""Audit the opt-in physical string acquisition/draw/release with one native bow shot."""
import argparse,json,struct
from pathlib import Path
from check_bow_spawn_submission import audit as submission

def audit(probe,submission_audit=submission):
 events=[json.loads(s) for s in (probe/'view-resource-probe.jsonl').read_text().splitlines()];ev=lambda n:[x for x in events if x['event']==n]
 p=ev('bow_physical_trial_sample');r=next((x for x in p if x['phase']==5),{});held=next((x['us'] for x in p if x['phase']==2),0);release=r.get('us',0)
 pads=ev('controller_live_sample');shots=[x for x in pads if x['triggers'][1]];end=next((x['us'] for x in pads if shots and x['us']>shots[0]['us'] and not x['triggers'][1]),0)
 worlds={};taps=[]
 for x in ev('input_bow_sample'):
  b=bytes.fromhex(x['weapon_bytes'])
  if struct.unpack_from('<I',b,0x4c)[0]!=327157552:continue
  rows=worlds.setdefault(x['world'],[])
  if not rows or rows[-1]['state']!=b[0]:rows.append(dict(us=x['us'],frame=x['frame'],state=b[0],ammo=struct.unpack_from('<I',b,0x60)[0],missing=struct.unpack_from('<H',b,0x6c)[0]))
  if x['digital']&(1<<49):taps.append(x)
 draw=[x for x in p if x['phase']==2];fire= submission_audit(probe)
 checks=dict(enabled=len(ev('bow_physical_release_enabled'))==1,distant_press_refused=any(x['frame']==390 and x['phase']==0 and not x['near'] for x in p),
 near_acquisition=any(x['frame']==540 and x['phase']==2 and x['near'] for x in p),
 physical_release=bool(r) and r['frame']==720 and r['raw_trigger']<=.2 and r['release_pull']>.34 and r['triggers']==[255,255],
 charge_only=bool(draw) and all(x['triggers']==[255,0] and x['valid'] for x in draw),
 one_pulse=bool(shots) and all(release<=x['us']<=release+100000 for x in shots) and 50000<=end-shots[0]['us']<=100000,
 completed_once=any(x['phase']==6 for x in p) and not any(x['phase'] in [2,5] and x['us']>next((z['us'] for z in p if z['phase']==6),10**30) for x in p),
 native_charge=len(worlds)==2 and all(any(x['state']==2 and held<=x['us']<release for x in rows) for rows in worlds.values()),
 native_one_fire=len(worlds)==2 and all(sum(x['state']==3 for x in rows)==1 and all(release<=x['us']<=release+1000000 and x['ammo'] and not x['missing'] for x in rows if x['state']==3) for rows in worlds.values()),
 native_taps=bool(taps) and {x['world'] for x in taps}==set(worlds) and all(x['us']>=release for x in taps),
 projectile_submission=fire['status']=='PASS' and len(fire['shots'])==1,
 completed=any(x['result']==0 for x in ev('view_resource_hook_complete')),no_failure=not any('failure' in x['event'] for x in events))
 return dict(status='PASS' if all(checks.values()) else 'FAIL',checks=checks,release_frame=r.get('frame'),release_pull_m=r.get('release_pull'),pulse_us=end-shots[0]['us'] if shots else None,worlds=worlds,submission=fire,limitation='Mock one-shot physical input only. Native time-based power and native camera aim retained; controller-based projectile direction and physical draw damage remain unimplemented.')
if __name__=='__main__':
 a=argparse.ArgumentParser(description=__doc__);a.add_argument('probe',type=Path);v=a.parse_args();r=audit(v.probe);(v.probe/'bow-physical-release-check.json').write_text(json.dumps(r,indent=2)+'\n',encoding='utf-8');print(json.dumps(r,indent=2));raise SystemExit(r['status']!='PASS')
