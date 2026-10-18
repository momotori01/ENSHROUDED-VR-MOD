"""Validate saved native fog history UV matrix against two exact private-eye camera snapshots."""
import argparse,json,math,hashlib
from pathlib import Path
import numpy as np
p=argparse.ArgumentParser(description=__doc__);p.add_argument('launch',type=Path);p.add_argument('bindings',type=Path);p.add_argument('--event',type=int,required=True);p.add_argument('--eye',type=int,choices=[0,1],default=0);p.add_argument('--native-fov',type=float,default=65)
a=p.parse_args();report=a.bindings/f'fog-history-matrix-eye-{a.eye}.json';assert not report.exists()
shader=a.bindings/f'event_{a.event}_stage_5.spv';blob=a.bindings/f'event_{a.event}_stage_5_constant_0.bin'
reference=Path('vrmod/output/game-20261010-002614/fog-temporal-bindings/event_6169_stage_5.spv')
assert hashlib.sha256(shader.read_bytes()).digest()==hashlib.sha256(reference.read_bytes()).digest(),'History shader differs; recheck native buffer layout'
b=blob.read_bytes();assert len(b)==112;gpu=np.frombuffer(b,dtype='<f4',count=16,offset=48).reshape(4,4).astype(float)
cap=json.loads((a.launch/'world-capture-check.json').read_text(encoding='utf-8'));serial=[e['serial'] for e in cap['events'] if e['event']=='world_final_capture_ended'][0];cameras={}
for f in a.launch.glob('view-resource-probe-*/world-session.txt'):
 for line in f.with_name('view-resource-probe.jsonl').open('rb'):
  e=json.loads(line)
  if e.get('event')=='calibration_eye_transform' and e['eye']==a.eye and e['serial']<=serial:cameras[e['serial']]=e
previous=max(k for k in cameras if k<serial);assert serial-previous==1
s=np.array([[2,0,0,-1],[0,-2,0,1],[0,0,1,0],[0,0,0,1]],dtype=float)
def predict(fov=None):
 ms=[]
 for seq in [previous,serial]:
  c=cameras[seq];v=np.array(c['view_matrix']).reshape(4,4).T;v[:3,3]=-v[:3,:3]@(np.array(c['camera_q32'],dtype='float64')/2**32);projection=np.array(c['projection_matrix']).reshape(4,4).T
  if fov is not None:projection[0,0]=projection[1,1]=1/math.tan(math.radians(fov/2))
  ms.append(projection@v)
 return np.linalg.inv(s)@ms[0]@np.linalg.inv(ms[1])@s
expected=predict();native=predict(a.native_fov);delta=float(np.max(abs(expected-gpu)));native_delta=float(np.max(abs(native-gpu)));pixels=[]
for z in [0,0.0001,0.01]:
 for x in np.linspace(.1,.9,5):
  for y in np.linspace(.1,.9,5):
   v=np.array([x,y,z,1]);u=expected@v;g=gpu@v;pixels.append(float(np.linalg.norm((u/u[3]-g/g[3])[:2]*1963)))
r=dict(status='PASS' if delta<2e-5 else 'FAIL',eye=a.eye,serial=serial,previous=previous,event=a.event,shader_sha256=hashlib.sha256(shader.read_bytes()).hexdigest(),expected_xr=expected.tolist(),actual=gpu.tolist(),native_candidate_fov_degrees=a.native_fov,native_candidate=native.tolist(),max_matrix_error=delta,max_native_candidate_error=native_delta,max_sample_reprojection_error_pixels_1963=max(pixels),limitation='One GPU capture. Column-form UV matrix; camera Q32 avoids large-world float cancellation. Pixel samples diagnostic, not image quality or all-frame proof.')
report.write_text(json.dumps(r,indent=2)+'\n',encoding='utf-8');print(json.dumps({k:v for k,v in r.items() if k not in ['expected_xr','actual','native_candidate']},indent=2));raise SystemExit(int(r['status']!='PASS'))
