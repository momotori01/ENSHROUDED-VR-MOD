"""Offline native idle grip candidates from archived bones/models. Not same-frame calibration."""
import json,math,struct
from pathlib import Path
from check_recorded_xr_native import normalize,multiply,rotate

def inverse(q):q=normalize(q);return [-q[0],-q[1],-q[2],q[3]]
def compose(a,b):
 aq,ap,ascale=a;bq,bp,bscale=b;return normalize(multiply(aq,bq)),[u+v for u,v in zip(ap,rotate(aq,[x*ascale for x in bp]))],ascale*bscale
def rel(a,b):
 aq,ap,ascale=a;bq,bp,bscale=b;return normalize(multiply(inverse(aq),bq)),[x/ascale for x in rotate(inverse(aq),[u-v for u,v in zip(bp,ap)])],bscale/ascale
def transform(b):
 p=struct.unpack_from('<3q',b);q=struct.unpack_from('<4f',b,24);scale=struct.unpack_from('<3f',b,40)
 if min(scale)<=0 or max(scale)-min(scale)>1e-5:raise ValueError('nonuniform native model scale')
 return normalize(q),[x/4294967296 for x in p],scale[0]

def analyze(probe):
 data=[json.loads(l) for l in (probe/'view-resource-probe.jsonl').read_text().splitlines()]
 h=probe/'hand-pose';parents=struct.unpack('<102H',(h/'joint-parents.bin').read_bytes());raw=(h/'current-joints.bin').read_bytes()
 hands=next((probe/'equipment-models').glob('11-*.bin')).read_bytes();world=transform(hands[0x38:0x70]);globals=[];scaleError=0
 for i,parent in enumerate(parents):
  v=struct.unpack_from('<12f',raw,i*48);scaleError=max(scaleError,max(abs(x-1) for x in v[8:11]));pose=(normalize(v[:4]),list(v[4:7]),v[8])
  if parent!=65535:pose=compose(globals[parent],pose)
  globals.append(pose)
 hs=[compose(world,globals[i]) for i in [64,94]]
 hs=[(q,p,1) for q,p,scale in hs] # grip positions in native world units; hands are rigid controller frames
 meta=next(x for x in data if x['event']=='hand_pose_metadata');weapon=min((x for x in data if x['event']=='attachment_model' and x['item_id']==1940764111),key=lambda x:abs(x['us']-meta['us']))
 wp=transform(bytes.fromhex(weapon['current_transform_bytes']));rightToWeapon=rel(hs[1],wp);leftInWeapon=rel(wp,hs[0]);rightInWeapon=rel(wp,hs[1])
 return dict(source=str(probe),hand_time=meta['us'],weapon_time=weapon['us'],time_separation_us=weapon['us']-meta['us'],unit_joint_scale_error=scaleError,model_scales=dict(hand=world[2],weapon=wp[2]),
  right_grip_to_weapon=dict(q=rightToWeapon[0],p=rightToWeapon[1]),left_grip_in_weapon=dict(q=leftInWeapon[0],p=leftInWeapon[1]),right_grip_in_weapon=dict(q=rightInWeapon[0],p=rightInWeapon[1]),hand_distance=math.dist(hs[0][1],hs[1][1]))

if __name__=='__main__':
 paths=['game-20261004-131911/view-resource-probe-20261004-132042','game-20261004-135508/view-resource-probe-20261004-135618','game-20261004-140317/view-resource-probe-20261004-140431']
 results=[]
 for s in paths:
  try:results.append(analyze(Path('vrmod/output')/s))
  except Exception as e:results.append(dict(source=s,error=str(e)))
 Path('docs/analysis/native_weapon_grip_candidates.json').write_text(json.dumps(dict(scope=__doc__,results=results),indent=2)+'\n')
 for r in results:print(r)
