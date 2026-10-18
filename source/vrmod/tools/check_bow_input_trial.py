"""Check one finite Mock RT-to-PresetA-X bow charge/release trial, not projectile aim."""
import argparse,json,collections,struct
from pathlib import Path

def audit(probe,aimed=False):
 e=[json.loads(s) for s in (probe/'view-resource-probe.jsonl').read_text().splitlines()]
 phases=[x for x in e if x['event']=='bow_input_trial_phase'];pad=[x for x in e if x['event']=='controller_live_sample'];pressed=[x for x in pad if (x['triggers'][1] if aimed else x['buttons']&0x4000)]
 raw=[x for x in e if x['event']=='input_bow_sample'];worlds={}
 for x in raw:
  b=bytes.fromhex(x['weapon_bytes']);item=struct.unpack_from('<I',b,0x4c)[0]
  if item!=327157552:continue
  w=worlds.setdefault(x['world'],dict(samples=0,states={},transitions=[]));w['samples']+=1;s=b[0];w['states'][s]=w['states'].get(s,0)+1
  if not w['transitions'] or w['transitions'][-1]['state']!=s:w['transitions'].append(dict(frame=x['frame'],state=s,ammo=struct.unpack_from('<I',b,0x60)[0],missing_resources=struct.unpack_from('<H',b,0x6c)[0],original_trigger=b[0x40],current_trigger=b[0x41]))
 checks=dict(explicit_once=any(x['event']=='bow_input_trial_enabled' and x['maximum_presses']==1 and not x['native_state_write'] for x in e),phases=[x['phase'] for x in phases]==[1,2,3],one_press=bool(pressed) and all(945<=x['frame']<=1035 for x in pressed),rt_consumed=bool(pad) and all((not x['buttons']&0x4000) if aimed else x['triggers'][1]==0 for x in pad),released=any(x['frame']==1036 and not (x['triggers'][1] if aimed else x['buttons']&0x4000) for x in pad),no_abort=not any(x['phase']==4 for x in phases),native_charge=bool(worlds) and all(2 in w['states'] for w in worlds.values()),native_fire=bool(worlds) and all(3 in w['states'] for w in worlds.values()),fire_after_release=bool(worlds) and all(all(t['frame']>=1036 for t in w['transitions'] if t['state']==3) for w in worlds.values()),native_ammo=bool(worlds) and all(any(t['state']==3 and t['ammo'] and not t['missing_resources'] for t in w['transitions']) for w in worlds.values()),completed=any(x['event']=='view_resource_hook_complete' and x['result']==0 for x in e),no_failure=not any('failure' in x['event'] for x in e))
 if aimed:
  checks['native_aim']=bool(worlds) and all(1 in w['states'] for w in worlds.values())
  checks['lt_retained_during_draw']=bool(pressed) and all(x['triggers'][0]==255 for x in pressed)
  checks['lt_released']=any(x['frame']>1100 and x['triggers']==[0,0] for x in pad)
  checks['aim_mode']=any(x['event']=='bow_input_trial_mode' and x['aimed_triggers'] for x in e)
 return dict(status='PASS' if all(checks.values()) else 'FAIL',checks=checks,phases=phases,pressed_samples=len(pressed),worlds=worlds,limitation='Native state transition only. Does not establish projectile origin/direction/hit, physical draw or safe cancellation; Mock-only explicit one-press trial.')
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('probe',type=Path);p.add_argument('--aim-triggers',action='store_true');a=p.parse_args();r=audit(a.probe,a.aim_triggers);(a.probe/'bow-input-trial-check.json').write_text(json.dumps(r,indent=2)+'\n');print(json.dumps(r,indent=2));raise SystemExit(r['status']!='PASS')
