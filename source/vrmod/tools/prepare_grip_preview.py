"""Build a local-asset grip preview from a successful simultaneous native capture. Never enables combat."""
import argparse,hashlib,json,math,struct
from pathlib import Path
from emulate_native_grip import Engine,relative,combine
from analyze_native_weapon_grip_affine import mm,mv,inv,mat
from check_recorded_xr_native import normalize,multiply,rotate

def unit(v):
 n=math.sqrt(sum(x*x for x in v))
 if not math.isfinite(n) or n<1e-6:raise ValueError('degenerate anatomy')
 return [x/n for x in v]
def cross(a,b):return [a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]]
def polar(m):
 for _ in range(12):
  mi=inv(m);m=[[.5*(m[i][j]+mi[j][i]) for j in range(3)] for i in range(3)]
 if sum(m[0][i]*cross(m[1],m[2])[i] for i in range(3))<.99:raise ValueError('improper rotation')
 return m
def quat(m):
 trace=sum(m[i][i] for i in range(3))
 if trace>0:
  s=math.sqrt(trace+1)*2;q=[(m[2][1]-m[1][2])/s,(m[0][2]-m[2][0])/s,(m[1][0]-m[0][1])/s,s/4]
 else:
  i=max(range(3),key=lambda n:m[n][n]);j=(i+1)%3;k=(i+2)%3;s=math.sqrt(1+m[i][i]-m[j][j]-m[k][k])*2;q=[0.,0.,0.,0.];q[i]=s/4;q[j]=(m[j][i]+m[i][j])/s;q[k]=(m[k][i]+m[i][k])/s;q[3]=(m[k][j]-m[j][k])/s
 return normalize(q)
def to_xr(q,p):return [-q[0],-q[1],q[2],q[3]],[p[0],p[1],-p[2]]
def prepare(probe,out):
 for name in ('native-grip-checks.json','motion-hand-checks.json','motion-visual-checks.json'):
  checks=json.loads((probe/name).read_text())['checks']
  if not checks or any(v!='PASS' for v in checks.values()):raise ValueError('source proof not PASS: '+name)
 em=json.loads((probe/'native-grip-emulation.json').read_text())
 if len(em['checks'])!=53 or any(c['status']!='PASS' for c in em['checks']) or em['substitutions']:raise ValueError('native FK proof missing')
 frames=em['frames'];selected=min(frames,key=lambda a:sum(math.dist(a['right_in_weapon'][1],b['right_in_weapon'][1]) for b in frames))
 events=[json.loads(x) for x in (probe/'view-resource-probe.jsonl').read_text().splitlines()];sample=next(x for x in events if x['event']=='native_grip_sample' and x['frame']==selected['frame'])
 if sample['weapon_type']!=7 or sample['attachment_slot']!=9 or sample['item']!=1940764111:raise ValueError('only measured two-hand axe is reviewed for preview')
 folder=probe/sample['folder'];raw=(folder/'joints.bin').read_bytes();default=(probe/'hand-pose/default-joints.bin').read_bytes()[:4896];parents=struct.unpack('<102H',(probe/'hand-pose/joint-parents.bin').read_bytes());e=Engine();neutral=e.bones(default,parents);current=e.bones(raw,parents);hand=(folder/'hand-model.bin').read_bytes()[0x38:0x70];weapon=(folder/'weapon-model.bin').read_bytes()[0x38:0x70];hw=e.model(hand,hand[:24]);ww=e.model(weapon,hand[:24]);wq=normalize(struct.unpack_from('<4f',weapon,24));scale=list(struct.unpack_from('<3f',weapon,40));frozen=bytearray(default);grips=[];anatomy=[]
 for wrist,index,middle,little,socket,first,last in [(44,49,53,61,64,45,63),(74,79,83,91,94,75,93)]:
  origin=neutral[wrist];local=lambda b:mv(inv(origin[0]),[u-v for u,v in zip(neutral[b][1],origin[1])]);thumb=unit([a-b for a,b in zip(local(index),local(little))]);forward=unit(local(middle));x=unit([-v for v in cross(forward,thumb)]);z=thumb;y=unit(cross(z,x));basis=[[x[i],y[i],z[i]] for i in range(3)];q=quat(basis);socketValues=struct.unpack_from('<12f',raw,socket*48);position=list(socketValues[4:7]);grip=combine(combine(hw,current[wrist]),(basis,position));grips.append((quat(polar(grip[0])),grip[1]));anatomy.append(dict(wrist=wrist,index=index,middle=middle,little=little,socket=socket,q=q,p=position))
  for i in range(first,last+1):
   source=struct.unpack_from('<12f',raw,i*48);ref=struct.unpack_from('<12f',default,i*48)
   if max(abs(a-b) for a,b in zip(source[4:7],ref[4:7]))>1e-6:raise ValueError('finger translation changed; new calibration needed')
   struct.pack_into('<4f',frozen,i*48,*normalize(source[:4]))
  struct.pack_into('<4f',frozen,socket*48,*q);struct.pack_into('<3f',frozen,socket*48+16,*position)
 rq,rp=grips[1];inverseRight=[-rq[0],-rq[1],-rq[2],rq[3]];cq=normalize(multiply(inverseRight,wq));cp=rotate(inverseRight,[a-b for a,b in zip(ww[1],rp)]);cq,cp=to_xr(cq,cp);iq=[-wq[0],-wq[1],-wq[2],wq[3]];off=rotate(iq,[a-b for a,b in zip(grips[0][1],ww[1])]);off[2]*=-1
 out.mkdir(parents=True,exist_ok=False);(out/'hand-pose.bin').write_bytes(frozen)
 # Plain numeric format: resource identity, XR-relative rigid offset, frozen
 # native scale and scale-baked model-space off-hand position.
 words=['EVR_GRIP_PREVIEW_1',str(sample['item']),str(sample['weapon_type']),str(sample['attachment_slot'])]
 for key in ('hand_guid','weapon_guid'):words += [str(v) for v in struct.unpack('<4I',bytes.fromhex(sample[key]))]
 words += [format(v,'.9g') for v in cp+cq+scale+off];(out/'profile.txt').write_text(' '.join(words)+'\n')
 report=dict(scope=__doc__,source=str(probe),sample=sample,anatomy=anatomy,grip_to_model_XR=dict(p=cp,q=cq),scale=scale,off_hand_in_scaled_model_XR=off,right_to_left_orientation_XR=to_xr(normalize(multiply(inverseRight,grips[0][0])),[0,0,0])[0],closed_fingers=True,tracking_independent_of_animation=True,combat_approved=False,limitations=['Grip origin uses measured native socket, physical comfort adjustment pending.','Anatomical axes use the observed open-hand hierarchy; fingers use normalized rotations only, preserving neutral lengths/scales.','Preview restricted to one measured item/resource; two-hand gameplay and actual damage remain unverified.'],files={n:hashlib.sha256((out/n).read_bytes()).hexdigest() for n in ('profile.txt','hand-pose.bin')})
 (out/'profile.json').write_text(json.dumps(report,indent=2)+'\n');print('preview',out,'frame',sample['frame'],'offset',cp,'quaternion',cq,'offhand',off)
 return report
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('probe',type=Path);p.add_argument('output',type=Path);a=p.parse_args();prepare(a.probe,a.output)
