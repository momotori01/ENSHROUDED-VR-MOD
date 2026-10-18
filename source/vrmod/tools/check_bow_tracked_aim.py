"""Audit one physical bow release using private camera values and native submission."""
import argparse,csv,json,math,struct
from pathlib import Path
from check_capture_cameras import rotate,qmul
from check_bow_physical_release import audit as input_audit

def audit(p):
 events=[json.loads(s) for s in (p/'view-resource-probe.jsonl').read_text().splitlines()];ev=lambda n:[e for e in events if e['event']==n]
 armed=ev('bow_tracked_shot_armed');calls=ev('bow_tracked_aim_call');shots=ev('bow_spawn_sample');dispatch=ev('bow_spawn_dispatch_result');submits=ev('bow_spawn_submit_result');removed=ev('bow_spawn_removed')
 checks=dict(enabled=len(ev('bow_tracked_aim_enabled'))==1,one_shot=len(armed)==len(calls)==len(shots)==len(dispatch)==len(submits)==1,release_scope=True,private_input_only=True,source_unchanged=True,native_identity=True,submission=True,geometry=True,tracked_math=True,restored=len(removed)==1 and removed[0]['restored'] and not removed[0]['failures'] and not removed[0]['callbacks'] and removed[0]['samples']==removed[0]['dispatch_samples']==removed[0]['submit_samples']==1)
 details=[]
 hands={int(row[0]):list(map(float,row)) for row in csv.reader(s for s in (p.parent/'mock-hands.csv').read_text().splitlines() if s and not s.startswith('#'))}
 for a,c,x,d,s in zip(armed,calls,shots,dispatch,submits):
  checks['release_scope'] &= a['armed'] and c['applied'] and c['consumed'] and a['frame']==c['release_frame']==720 and a['us']<c['us'] and 0<=c['age_ms']<=200 and 0<=c['current_frame']-a['frame']<=8 and c['entity']==a['entity']==x['native_entity']==x['local_entity']
  original=bytes.fromhex(c['original_input_bytes']);private=bytes.fromhex(c['private_input_bytes']);camera=bytes.fromhex(c['effective_camera_bytes'])
  checks['private_input_only'] &= len(original)==len(private)==1320 and len(camera)==44 and original[:0x28]==private[:0x28] and original[0x54:]==private[0x54:] and private[0x28:0x54]==camera and original[0x28:0x54]==bytes.fromhex(x['camera_bytes']) and original!=private and camera==bytes.fromhex(x['effective_camera_bytes'])
  checks['source_unchanged'] &= x['sources_unchanged'] and x['tracked_aim'] and not x['read_only'] and x['result'] and s['source_unchanged'] and d['read_only'] and s['read_only']
  native=bytes.fromhex(x['native_weapon_bytes']);cached=bytes.fromhex(x['observed_weapon_bytes'])
  nearest=min(ev('input_bow_sample'),key=lambda v:abs(v['us']-x['us']) if v['input']==x['input'] else 10**30)
  checks['native_identity'] &= x['native_identity_observed'] and 0<=x['identity_age_ms']<=100 and nearest['world']==x['native_world'] and nearest['entity']==a['entity'] and native[0]==cached[0]==3 and native[0x4c:0x50]==cached[0x4c:0x50]==struct.pack('<I',327157552) and native[0x60:0x64]==cached[0x60:0x64]==struct.pack('<I',1454438654)
  transform=bytes.fromhex(x['transform_bytes']);event=bytes.fromhex(x['event_bytes'])
  checks['submission'] &= c['sample']==x['sample']==d['sample']==s['sample'] and d['same_transform'] and bytes.fromhex(d['params_bytes'])[0x18:0x4c]==transform[:52] and d['params']==s['params'] and d['result'] and s['result'] and x['us']<=d['us']<=s['us']
  m=next(v for v in ev('motion_visual_pose') if v['frame']==a['frame'] and v['item']==327157552);h=hands[a['frame']]
  yaw,pitch,roll=[math.radians(t)/2 for t in h[4:7]]
  q=qmul(qmul([0,math.sin(yaw),0,math.cos(yaw)],[math.sin(pitch),0,0,math.cos(pitch)]),[0,0,math.sin(roll),math.cos(roll)])
  local=rotate([-q[0],-q[1],-q[2],q[3]],[v-w for v,w in zip(h[14:17],h[1:4])]);nock=[local[i]/m['scale'][i] for i in range(3)]
  checks['geometry'] &= a['model_q32']==m['q32'] and max(abs(v-w) for v,w in zip(a['model_q'],m['orientation']))<1e-6 and max(abs(v-w) for v,w in zip(nock,a['nock']))<1e-6 and a['binding']==(327157552<<32)|m['source_handle']
  rest=[0,.00155006*m['scale'][1],0];delta=[rest[i]-nock[i]*m['scale'][i]*(1 if i<2 else -1) for i in range(3)];span=math.sqrt(sum(v*v for v in delta));direction=rotate(m['orientation'],[v/span for v in delta]);off=rotate(m['orientation'],rest)
  actualpos=struct.unpack_from('<3q',transform);actualq=struct.unpack_from('<4f',transform,24);actualdir=rotate(actualq,[0,0,1])
  poserr=max(abs((actualpos[i]-m['q32'][i])/2**32-off[i]) for i in range(3));direrr=max(abs(v-w) for v,w in zip(direction,actualdir))
  checks['tracked_math'] &= x['typed_dispatch_path'] and event[0x148]==1 and event[0x90] in [2,3] and struct.unpack_from('<8f',event,0x104)==(0.,)*8 and poserr<3e-6 and direrr<3e-6 and struct.unpack_from('<3f',transform,40)==(1.,1.,1.)
  oldq=struct.unpack_from('<4f',original,0x40);oldoffq=struct.unpack_from('<4f',event,0xd8);olddir=rotate(qmul(oldq,oldoffq),[0,0,1]);angle=math.degrees(math.acos(max(-1,min(1,sum(v*w for v,w in zip(olddir,actualdir))))))
  details.append(dict(sample=x['sample'],release_frame=a['frame'],consume_frame=c['current_frame'],age_ms=c['age_ms'],position_error_m=poserr,direction_error=direrr,angle_from_native_camera_degrees=angle,submission_delay_us=s['us']-x['us']))
 spawn=dict(status='PASS' if all(checks.values()) else 'FAIL',checks=checks,shots=details)
 inp=input_audit(p,lambda _:spawn)
 checks['physical_input']=inp['status']=='PASS'
 return dict(status='PASS' if all(checks.values()) else 'FAIL',checks=checks,shots=details,input_checks=inp['checks'],pulse_us=inp['pulse_us'],limitation='Mock known-bow one-shot tracked transform accepted by native spawn. Native timed power retained. Provisional grip/arrow rest; final ECS projectile/collision, repeat nocking, hardware and other weapons remain unverified.')
if __name__=='__main__':
 a=argparse.ArgumentParser(description=__doc__);a.add_argument('probe',type=Path);v=a.parse_args();r=audit(v.probe);(v.probe/'bow-tracked-aim-check.json').write_text(json.dumps(r,indent=2)+'\n',encoding='utf-8');print(json.dumps(r,indent=2));raise SystemExit(r['status']!='PASS')
