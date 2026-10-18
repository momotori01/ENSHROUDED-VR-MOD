"""Compare simultaneous ECS transforms of native weapon visual and Impact collider.
Reports offsets without assuming either pivot is identical. No game modifications.
"""
import argparse,json,math,pathlib,struct

def mul(a,b):
 x,y,z,w=a;X,Y,Z,W=b;return [w*X+x*W+y*Z-z*Y,w*Y-x*Z+y*W+z*X,w*Z+x*Y-y*X+z*W,w*W-x*X-y*Y-z*Z]
def norm(q):
 n=math.sqrt(sum(x*x for x in q));return [x/n for x in q]
def inv(q):
 x,y,z,w=norm(q);return [-x,-y,-z,w]
def rotate(q,v):return mul(mul(norm(q),list(v)+[0]),inv(q))[:3]
def pose(raw):
 b=bytes.fromhex(raw);return [x/2**32 for x in struct.unpack_from('<3q',b)],list(struct.unpack_from('<4f',b,24)),list(struct.unpack_from('<3f',b,40))
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('probe',type=pathlib.Path);a=p.parse_args();rs=[json.loads(x) for x in (a.probe/'view-resource-probe.jsonl').read_text(encoding='utf-8').splitlines()]
 out=[]
 for r in rs:
  if r['event']!='sequence_collider_sample' or not r.get('native_weapon_transform_bytes'):continue
  cp,cq,cs=pose(r['transform_bytes']);wp,wq,ws=pose(r['native_weapon_transform_bytes']);local=rotate(inv(wq),[c-w for c,w in zip(cp,wp)]);q=norm(mul(inv(wq),cq));off=[v/s for v,s in zip(local,ws)];angle=math.degrees(2*math.acos(min(1,abs(q[3]))))
  out.append(dict(sample=r['sample'],entity=r['entity'],weapon=r['native_weapon_entity'],item=r['native_weapon_item'],sequence=r['sequence'],milliseconds=r['milliseconds'],collider_position=cp,weapon_position=wp,weapon_to_collider_offset=off,weapon_to_collider_orientation=q,angle_degrees=angle,collider_scale=cs,weapon_scale=ws))
 result=dict(samples=out,count=len(out),limit=__doc__)
 (a.probe/'weapon-collider-calibration.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
 print(json.dumps(result,indent=2));return not out
if __name__=='__main__':raise SystemExit(main())
