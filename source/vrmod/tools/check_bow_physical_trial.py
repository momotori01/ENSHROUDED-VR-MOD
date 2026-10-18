"""Audit known-bow near-string acquisition and no-shot physical return in Mock."""
import argparse,collections,csv,json,math,struct
from pathlib import Path
from check_capture_cameras import qmul,rotate

def audit(p):
 e=[json.loads(s) for s in (p/'view-resource-probe.jsonl').read_text().splitlines()]
 ev=lambda n:[x for x in e if x['event']==n]
 ss=ev('bow_physical_trial_sample');active=[x for x in ss if x['phase']==2];cancel=[x for x in ss if x['phase']==3]
 started=active[0]['us'] if active else 0;ended=cancel[0]['us'] if cancel else 0
 far=[x for x in ss if 390<=x['frame']<=430];held=[x for x in ss if 545<=x['frame']<=715]
 checks=dict(enabled=bool(ev('bow_physical_trial_enabled')) and all(not x['firing_enabled'] and not x['native_power_write'] for x in ev('bow_physical_trial_enabled')),
 distant_press_rejected=bool(far) and any(x['raw_trigger']>.8 for x in far) and all(x['phase']!=2 and x['triggers']==[0,0] for x in far),
 acquired_at_nock=bool(active) and active[0]['near'] and 538<=active[0]['frame']<=542,
 holds_after_leaving_nock=bool(held) and all(x['phase']==2 and x['triggers']==[255,0] for x in held),
 pull_35cm=bool(active) and .34<max(x['pull'] for x in active)<.36,
 returned_before_cancel=bool(cancel) and 718<=cancel[0]['frame']<=722 and cancel[0]['pull']<.003,
 cancelled_no_rearm=bool(cancel) and all(x['phase']==3 and x['triggers']==[0,0] for x in ss if x['us']>=ended),
 no_pad_fire=bool(ev('controller_live_sample')) and all(not x['triggers'][1] and not x['buttons']&0x4000 for x in ev('controller_live_sample')),
 same_frame_geometry=True,measured_nock=True,native_charge=True,native_holster=True,no_native_shoot=True,no_native_fire=True,source_unchanged=True,
 completed=any(x['result']==0 for x in ev('view_resource_hook_complete')),no_failure=not any('failure' in x['event'] for x in e))
 hands={int(r[0]):[float(x) for x in r] for r in csv.reader(x for x in (p.parent/'mock-hands.csv').read_text().splitlines() if x and not x.startswith('#'))}
 poses={x['frame']:x for x in ev('motion_visual_pose') if x['item']==327157552};err=0
 for x in ss:
  if not x['valid']:continue
  v=poses.get(x['frame']);r=hands.get(x['frame']);checks['same_frame_geometry'] &= bool(v) and x['frame']==x['geometry_frame'] and x['binding']==((327157552<<32)|v['source_handle']) if v else False
  if not v or not r:checks['measured_nock']=False;continue
  yaw,pitch,roll=[math.radians(z)/2 for z in r[4:7]];q=qmul(qmul([0,math.sin(yaw),0,math.cos(yaw)],[math.sin(pitch),0,0,math.cos(pitch)]),[0,0,math.sin(roll),math.cos(roll)])
  n=rotate(q,[0,.00155006*v['scale'][1],.24005109*v['scale'][2]]);n=[a+b for a,b in zip(n,r[1:4])]
  err=max(err,max(abs(a-b) for a,b in zip(n,x['nock'])))
 checks['measured_nock'] &= err<2e-5
 worlds=collections.defaultdict(list)
 for x in ev('input_bow_sample'):
  b=bytes.fromhex(x['weapon_bytes']);worlds[x['world']].append(dict(us=x['us'],state=b[0],item=struct.unpack_from('<I',b,0x4c)[0],digital=x['digital'],queued=b[0x39],unchanged=x['read_only'] and x['source_unchanged']))
 checks['two_worlds']=len(worlds)==2;delays={}
 for key,w in worlds.items():
  charge=[x for x in w if started<=x['us']<ended and x['state']==2 and x['item']==327157552 and x['digital']&(1<<47)]
  holster=[x for x in w if ended<=x['us']<ended+1000000 and x['state']==0 and not x['digital']&(1<<47)]
  checks['native_charge'] &= bool(charge);checks['native_holster'] &= bool(holster)
  checks['no_native_shoot'] &= not any(x['digital']&((1<<48)|(1<<49)) for x in w)
  checks['no_native_fire'] &= not any(x['state'] in [3,4] or x['queued'] for x in w)
  checks['source_unchanged'] &= all(x['unchanged'] for x in w)
  delays[key]=holster[0]['us']-ended if holster else None
 return dict(status='PASS' if all(checks.values()) else 'FAIL',checks=checks,samples=len(ss),max_nock_error_m=err,max_pull_m=max((x['pull'] for x in active),default=0),holster_delays_us=delays,limitation='Known bow, one Mock pull/return input audit. Visual bones require the separate check_physical_bow_pose audit. Native power, aiming and shooting are not established here.')
if __name__=='__main__':
 a=argparse.ArgumentParser(description=__doc__);a.add_argument('probe',type=Path);v=a.parse_args();r=audit(v.probe);(v.probe/'bow-physical-trial-check.json').write_text(json.dumps(r,indent=2)+'\n');print(json.dumps(r,indent=2));raise SystemExit(r['status']!='PASS')
