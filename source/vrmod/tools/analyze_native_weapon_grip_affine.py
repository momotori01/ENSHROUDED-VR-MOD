"""Exploratory affine grip candidates from archived idle bones; timestamps differ, no calibration approval."""
import json,math,struct
from pathlib import Path
from check_recorded_xr_native import normalize,multiply

def mat(q,s):
 x,y,z,w=normalize(q);r=[[1-2*(y*y+z*z),2*(x*y-z*w),2*(x*z+y*w)],[2*(x*y+z*w),1-2*(x*x+z*z),2*(y*z-x*w)],[2*(x*z-y*w),2*(y*z+x*w),1-2*(x*x+y*y)]]
 return [[r[i][j]*s[j] for j in range(3)] for i in range(3)]
def mv(m,v):return [sum(m[i][j]*v[j] for j in range(3)) for i in range(3)]
def mm(a,b):return [[sum(a[i][k]*b[k][j] for k in range(3)) for j in range(3)] for i in range(3)]
def inv(m):
 a=[list(row)+[float(i==j) for j in range(3)] for i,row in enumerate(m)]
 for col in range(3):
  pivot=max(range(col,3),key=lambda r:abs(a[r][col]));a[col],a[pivot]=a[pivot],a[col]
  d=a[col][col]
  if abs(d)<1e-8:raise ValueError('singular transform')
  a[col]=[v/d for v in a[col]]
  for row in range(3):
   if row!=col:
    factor=a[row][col];a[row]=[v-factor*w for v,w in zip(a[row],a[col])]
 return [row[3:] for row in a]
def pose(q,p,s):return mat(q,s),list(p),normalize(q)
def combine(a,b):
 am,ap,aq=a;bm,bp,bq=b;return mm(am,bm),[u+v for u,v in zip(ap,mv(am,bp))],normalize(multiply(aq,bq))
def model(raw):
 p=struct.unpack_from('<3q',raw);q=struct.unpack_from('<4f',raw,24);s=struct.unpack_from('<3f',raw,40)
 return pose(q,[v/4294967296 for v in p],s),s

def analyze(probe):
 r=[json.loads(l) for l in (probe/'view-resource-probe.jsonl').read_text().splitlines()];folder=probe/'hand-pose';parents=struct.unpack('<102H',(folder/'joint-parents.bin').read_bytes());raw=(folder/'current-joints.bin').read_bytes()
 data=next((probe/'equipment-models').glob('11-*.bin')).read_bytes();world,hs=model(data[0x38:0x70]);globalBones=[];scaleError=0;normError=0
 for i,parent in enumerate(parents):
  v=struct.unpack_from('<12f',raw,i*48);scaleError=max(scaleError,max(abs(x-1) for x in v[8:11]));normError=max(normError,abs(sum(q*q for q in v[:4])-1));local=pose(v[:4],v[4:7],v[8:11]);globalBones.append(local if parent==65535 else combine(globalBones[parent],local))
 left,right=[combine(world,globalBones[i]) for i in [64,94]];stamp=next(x['us'] for x in r if x['event']=='hand_pose_metadata');weapon=min((x for x in r if x['event']=='attachment_model' and x['item_id']==1940764111),key=lambda x:abs(x['us']-stamp));wp,ws=model(bytes.fromhex(weapon['current_transform_bytes']))
 relative=lambda a,b:mv(inv(a[0]),[u-v for u,v in zip(b[1],a[1])]);rq=right[2];iq=[-rq[0],-rq[1],-rq[2],rq[3]]
 return dict(source=str(probe),time_separation_us=weapon['us']-stamp,hand_scale=hs,weapon_scale=ws,joint_scale_error=scaleError,joint_quaternion_norm_error=normError,
  right_grip_to_weapon=dict(p=mv(mat(iq,[1,1,1]),[u-v for u,v in zip(wp[1],right[1])]),q=normalize(multiply(iq,wp[2]))),left_grip_in_weapon=relative(wp,left),right_grip_in_weapon=relative(wp,right),hand_distance=math.dist(left[1],right[1]))
if __name__=='__main__':
 paths=['game-20261004-131911/view-resource-probe-20261004-132042','game-20261004-135508/view-resource-probe-20261004-135618','game-20261004-140317/view-resource-probe-20261004-140431'];records=[]
 for s in paths:
  try:records.append(analyze(Path('vrmod/output')/s))
  except Exception as e:records.append(dict(source=s,error=str(e)))
 Path('docs/analysis/native_weapon_grip_affine_candidates.json').write_text(json.dumps(dict(scope=__doc__,limitations='Normalizes compressed bone quaternions; native operation order not emulated here. Idle off-hand may not hold weapon. No runtime constants may be derived without same-frame action/mesh evidence.',records=records),indent=2)+'\n')
 for r in records:print(r)
