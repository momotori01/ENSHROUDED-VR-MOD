"""Replay allowlisted native frustum instructions using recorded eye matrices (offline only)."""
import argparse,json,math,struct
from pathlib import Path
from emulate_frustum_math import FrustumMath
from unicorn.x86_const import UC_X86_REG_RAX
p=argparse.ArgumentParser();p.add_argument('probe',type=Path);p.add_argument('--step',type=int,default=120);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
engine=FrustumMath(Path('enshrouded.exe'));near=.01;far=10000.;state={};rows=[]
for line in (a.probe/'view-resource-probe.jsonl').open(encoding='utf-8'):
 e=json.loads(line)
 if e['event']=='recorded_xr_anchor':near=e['near'];far=e['far']
 if e['event']=='recorded_xr_eye_applied':state[e['eye']]=e
 if e['event']!='calibration_eye_transform' or e['eye'] not in state:continue
 s=state[e['eye']]
 if s['recorded_xr_frame']%a.step:continue
 matrix=e['view_matrix'];f=s['fov'];bounds=[math.tan(f[i])*near for i in [0,1,3,2]]
 descriptor=bytearray(80);struct.pack_into('<I6f',descriptor,0,3,*bounds,near,far);descriptor[0x48]=1
 engine.cpu.mem_write(engine.data+0x400,struct.pack('<16f',*matrix));engine.cpu.mem_write(engine.data+0x500,bytes(descriptor));engine.cpu.mem_write(engine.data+0x600,bytes(128));engine.invoke(0xd647e0,engine.data+0x600,engine.data+0x400,engine.data+0x500)
 planes=[struct.unpack('<4f',engine.cpu.mem_read(engine.data+0x600+i*16,16)) for i in range(6)]
 # Camera basis is rigid: camera point to world via transpose rotation.
 def world(v):return [sum((v[k]-matrix[12+k])*matrix[j*4+k] for k in range(3)) for j in range(3)]
 depths={}
 for z in [.1,1,10,100,1000]:
  pts=[world([bounds[x]/near*z,bounds[y]/near*z,z]) for x in [0,1] for y in [2,3]]
  depths[str(z)]=max(sum(pl[k]*v[k] for k in range(3))+pl[3] for pl in planes[:5] for v in pts)
 rows.append(dict(frame=s['recorded_xr_frame'],eye=e['eye'],serial=e['serial'],native_ok=bool(engine.cpu.reg_read(UC_X86_REG_RAX)&255),max_boundary_exclusion_m=depths,planes=planes,matrix=matrix))
r=dict(scope=__doc__,rows=rows,samples=len(rows),max_exclusion_m={str(z):max((x['max_boundary_exclusion_m'][str(z)] for x in rows),default=None) for z in [.1,1,10,100,1000]},limitations=['Descriptor reconstructed from logged FOV and near/far; final float rounding may differ.', 'Saved affine matrix uses native world-relative coefficients; boundary errors include float matrix storage.', 'Not proof of terrain/grass/fog root cause or of native GPU consumption.'])
a.output.parent.mkdir(parents=True,exist_ok=True);a.output.write_text(json.dumps(r,indent=2)+'\n',encoding='utf-8');print(json.dumps({k:v for k,v in r.items() if k!='rows'},indent=2))
