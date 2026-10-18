"""Rigidly align a recorded Mock head trajectory to a fixed home camera; no game state edits."""
import argparse,hashlib,json,math
from pathlib import Path
from replay_real_head_pose import angles,quat,mul
from check_capture_cameras import rotate,conjugate

def rows(path):return [list(map(float,l.split(','))) for l in path.read_text().splitlines() if l and not l.startswith('#')]
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('motion',type=Path);p.add_argument('reference',type=Path);p.add_argument('output',type=Path);p.add_argument('--rows',type=int,default=6000);a=p.parse_args()
 assert not a.output.exists() and 2500<=a.rows<=10000
 src=rows(a.motion);target=rows(a.reference)[0];assert len(src)<=a.rows
 q0=quat(*[math.radians(v) for v in src[0][4:7]]);qt=quat(*[math.radians(v) for v in target[4:7]])
 alignment=mul(qt,conjugate(q0));out=[];errors=[]
 for i,r in enumerate(src+[src[-1]]*(a.rows-len(src)),1):
  pos=rotate(alignment,[r[j]-src[0][j] for j in range(1,4)])
  orientation,err=angles(mul(alignment,quat(*[math.radians(v) for v in r[4:7]])));errors.append(err)
  out.append([i,*[target[j+1]+pos[j] for j in range(3)],*orientation,r[7],*target[8:]])
 a.output.write_text('# Rigidly aligned measured head motion; fixed reference optics; final pose held.\n'+'\n'.join(','.join(f'{v:.12g}' for v in row) for row in out)+'\n')
 before=math.dist(src[0][1:4],src[-1][1:4]);after=math.dist(out[0][1:4],out[len(src)-1][1:4]);assert abs(before-after)<1e-8
 report=dict(source=str(a.motion),source_sha256=hashlib.sha256(a.motion.read_bytes()).hexdigest(),reference=str(a.reference),reference_sha256=hashlib.sha256(a.reference.read_bytes()).hexdigest(),recorded_rows=len(src),output_rows=len(out),alignment_quaternion=alignment,max_quaternion_roundtrip_error=max(errors),first_position_error=max(abs(out[0][j]-target[j]) for j in range(1,4)),first_orientation_error=max(abs(out[0][j]-target[j]) for j in range(4,7)),endpoint_distance_preservation_error=abs(before-after),limitation='Rigid pose placement only. Optics copied from fixed reference. One recorded pose per frame; player, physics, weather and real compositor not reproduced.')
 a.output.with_suffix('.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
if __name__=='__main__':main()
