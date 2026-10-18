"""Read-only model/contact geometry comparison. Bounds do not define attack speed or blade identity."""
import argparse,hashlib,json,math,struct
from pathlib import Path
from check_recorded_xr_native import rotate,normalize

def rows(p):
 return [json.loads(l) for l in p.read_text(encoding='utf-8-sig').splitlines() if l]
def proof(p):
 c=json.loads(p.read_text(encoding='utf-8-sig'))['checks']
 if not c or any(v!='PASS' for v in c.values()):raise ValueError('source proof not PASS: '+str(p))
def model(b):
 if len(b)!=320:raise ValueError('RenderModel size')
 lo=list(struct.unpack_from('<3f',b,16));hi=list(struct.unpack_from('<3f',b,32));sphere=list(struct.unpack_from('<4f',b,48))
 scale=list(struct.unpack_from('<3f',b,64));offset=list(struct.unpack_from('<3f',b,76));global_scale=struct.unpack_from('<f',b,192)[0]
 if not all(math.isfinite(v) for v in lo+hi+sphere+scale+offset+[global_scale]) or any(l>=h for l,h in zip(lo,hi)) or sphere[3]<=0:raise ValueError('invalid model bounds/decoder')
 counts=[struct.unpack_from('<I',b,i+4)[0] for i in (88,96,104,112)]
 stride=struct.unpack_from('<I',b,204)[0]
 if any(n<1 or n>256 for n in counts) or stride not in (2,4):raise ValueError('unreviewed rigid model array/index format')
 return dict(min=lo,max=hi,size=[h-l for l,h in zip(lo,hi)],sphere=sphere,position_scale=scale,position_offset=offset,global_scale_raw_uninterpreted=global_scale,array_counts=counts,index_stride=stride,joints=struct.unpack_from('<2H',b,312),render_data_hash=b[132:148].hex())
def box(b):
 if len(b)!=96:raise ValueError('BoxColliderData size')
 p=list(struct.unpack_from('<3f',b,8));q=list(struct.unpack_from('<4f',b,20));half=list(struct.unpack_from('<3f',b,80))
 if not all(math.isfinite(v) for v in p+q+half) or any(v<=0 for v in half) or abs(sum(v*v for v in q)-1)>1e-3 or b[92] not in (0,1) or b[75] not in (0,1):raise ValueError('invalid collider geometry')
 corners=[]
 for i in range(8):
  v=rotate(q,[h*(1 if i&(1<<a) else -1) for a,h in enumerate(half)]);corners.append([x+y for x,y in zip(p,v)])
 return dict(center=p,orientation=q,half_size=half,continuous=bool(b[92]),enabled=bool(b[75]),min=[min(v[a] for v in corners) for a in range(3)],max=[max(v[a] for v in corners) for a in range(3)])
def analyze(render,combat,profile_path):
 for n in ('native-grip-checks.json','motion-hand-checks.json','motion-visual-checks.json'):proof(render/n)
 proof(combat/'calibrated-collider-apply-checks.json')
 profile=json.loads(profile_path.read_text());identity=profile['sample'];rs=rows(render/'view-resource-probe.jsonl');samples=[r for r in rs if r['event']=='native_grip_sample']
 if len(samples)!=20:raise ValueError('bounded twenty render samples required')
 blobs=[]
 for r in samples:
  if (r['item'],r['weapon_type'],r['weapon_guid'])!=(identity['item'],identity['weapon_type'],identity['weapon_guid']) or r.get('weapon_render_model_bytes')!=320 or r.get('weapon_render_model_stable') is not True or not r['read_only'] or not r['source_unchanged']:raise ValueError('render identity/stability proof missing')
  b=(render/r['folder']/'weapon-render-model.bin').read_bytes();model(b);blobs.append(b)
 if any(b!=blobs[0] for b in blobs):raise ValueError('immutable model metadata changed')
 geometry=model(blobs[0]);ns=rows(combat/'view-resource-probe.jsonl');assets=[r for r in ns if r['event']=='sequence_asset_snapshot' and r['item']==identity['item']]
 # Local ancestry is indirect via the Impact owner; parent_entity/local_parent
 # may be zero on the spawned Collider. Join independently proved apply scopes.
 from check_calibrated_collider_apply import audit
 checks,_=audit(ns,[[float(v) for v in l.split(',')] for l in (combat.parent/'mock-hands.csv').read_text().splitlines() if l and not l.startswith('#')],json.loads((combat/'grip-preview.json').read_text()),float((combat/'world-two-hand-attack.txt').read_text().split()[1]),early=True)
 if not checks or not all(checks.values()):raise ValueError('native apply source failed fresh ownership revalidation')
 scopes={r['ready_sample']:r for r in ns if r['event']=='motion_collider_apply_begin'}
 owned=[r for r in ns if r['event']=='sequence_collider_sample' and r.get('ready_sample') in scopes and r['entity']==scopes[r['ready_sample']]['entity'] and r.get('physics_updated')]
 if len(owned)!=len(scopes) or len(scopes)<3:raise ValueError('physics/owned apply scope mismatch')
 guids={bytes.fromhex(r['event_bytes'])[8:24].hex() for r in owned}
 events=[]
 for a in assets:
  if not a['stable_read'] or not a['read_only']:raise ValueError('unstable native asset')
  for sub in a['subsequences']:
   for e in sub['events']:
    d=bytes.fromhex(e.get('spawn_impact',''))
    if len(d)!=488 or d[344:360].hex() not in guids:continue
    shapes=[]
    for shape in e['shapes']:
     if shape['type_hash']!=0x88154c34:raise ValueError('unreviewed native contact shape')
     shapes.append(box(bytes.fromhex(shape['box_data'])))
    events.append(dict(sequence=a['sequence'],subsequence=sub['index'],event=e['index'],guid=d[344:360].hex(),shapes=shapes))
 if len(events)!=1 or len(guids)!=1:raise ValueError('single proved native Impact required')
 sc=profile['scale'];lo=[v*s for v,s in zip(geometry['min'],sc)];hi=[v*s for v,s in zip(geometry['max'],sc)]
 # Native game axes -> XR model axes. The origin stays the observed weapon origin.
 xrlo=[lo[0],lo[1],-hi[2]];xrhi=[hi[0],hi[1],-lo[2]]
 gp=profile['grip_to_model_XR'];iq=[-gp['q'][0],-gp['q'][1],-gp['q'][2],gp['q'][3]];main=rotate(iq,[-v for v in gp['p']]);off=profile['off_hand_in_scaled_model_XR']
 # This is only a geometric comparison; neither a box corner nor its longest
 # axis proves the blade or an allowed hit. Never emit a combat-approved profile.
 return dict(scope=__doc__,render_probe=str(render),combat_probe=str(combat),item=identity['item'],weapon_guid=identity['weapon_guid'],model_sha256=hashlib.sha256(blobs[0]).hexdigest(),samples=len(blobs),model=geometry,model_scaled_XR=dict(min=xrlo,max=xrhi),main_grip_scaled_XR=main,off_hand_scaled_XR=off,attack_speed_basis="controller_translation_v1 (no model point)",native_events=events,combat_approved=False,limits=['Render bounds are the complete model AABB; no triangle/edge identity is inferred.','Native impact boxes can exceed the visible weapon and do not identify a blade.','World units, native simulation scale and render scale must be distinguished before replacing collision geometry.','globalScale at offset192 is zero in this live asset; its semantics remain unproved and it was not applied to bounds.', 'No native hit shape, damage, cooldown or gameplay state was changed.'])
def self_test():
 b=bytearray(320);struct.pack_into('<3f',b,16,-.2,0,-.1);struct.pack_into('<3f',b,32,.2,1.4,.1);struct.pack_into('<4f',b,48,0,.7,0,.8);struct.pack_into('<f',b,192,1);struct.pack_into('<I',b,204,2)
 for i in (88,96,104,112):struct.pack_into('<I',b,i+4,1)
 m=model(b);assert abs(m['size'][1]-1.4)<1e-6
 c=bytearray(96);struct.pack_into('<3f',c,8,0,.9,0);struct.pack_into('<4f',c,20,0,0,0,1);struct.pack_into('<3f',c,80,.2,1.1,.3);c[75]=c[92]=1
 v=box(c);assert abs(v['max'][1]-2)<1e-6 and abs(v['min'][1]+.2)<1e-6
 cases=[(model,b,16,'<f',float('nan')),(model,b,32,'<f',-1),(model,b,192,'<f',float('nan')),(model,b,204,'<I',8),(model,b,100,'<I',257),(box,c,80,'<f',-1),(box,c,20,'<f',3),(box,c,92,'<B',2)]
 for fn,raw,off,fmt,value in cases:
  bad=bytearray(raw);struct.pack_into(fmt,bad,off,value)
  try:fn(bad)
  except ValueError:continue
  raise AssertionError('invalid geometry accepted')
 zero=bytearray(b);struct.pack_into('<f',zero,192,0);m0=model(zero);assert m0['min']==m['min'] and m0['max']==m['max'] and m0['global_scale_raw_uninterpreted']==0
 print('PASS 11 synthetic decoder cases; globalScale recorded without applying it; no live geometry claim')
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--render-probe',type=Path);p.add_argument('--combat-probe',type=Path);p.add_argument('--profile',type=Path);p.add_argument('--output',type=Path);p.add_argument('--self-test',action='store_true');a=p.parse_args()
 if a.self_test:self_test();return 0
 if any(x is None for x in (a.render_probe,a.combat_probe,a.profile,a.output)):p.error('all source/output paths required')
 r=analyze(a.render_probe,a.combat_probe,a.profile);a.output.write_text(json.dumps(r,indent=2)+'\n');print(json.dumps(r,indent=2));return 0
if __name__=='__main__':raise SystemExit(main())
