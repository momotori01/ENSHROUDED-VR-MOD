"""Calibrated hand/weapon visual preview only; no native damage or grip comfort claim."""
import argparse,hashlib,json,math,struct
from pathlib import Path
from check_recorded_xr_native import normalize,rotate
from grip_preview_math import calibrated
from prepare_grip_preview import cross,unit
from analyze_native_weapon_grip_affine import pose,combine,inv,mv

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('probe',type=Path);a=p.parse_args();profile=json.loads((a.probe/'grip-preview.json').read_text());events=[json.loads(x) for x in (a.probe/'view-resource-probe.jsonl').read_text().splitlines()];poses=[e for e in events if e['event']=='grip_preview_pose'];weapon={e['serial']:e for e in events if e['event']=='motion_visual_pose'};sequence=[[float(v) for v in l.split(',')] for l in (a.probe.parent/'mock-hands.csv').read_text().splitlines() if l and not l.startswith('#')];checks={};errors={'position':0.,'orientation':0.}
 checks['profile_copied_exactly']=all(hashlib.sha256((a.probe/target).read_bytes()).hexdigest()==profile['files'][source] for source,target in [('profile.txt','world-grip-preview.txt'),('hand-pose.bin','grip-hand-pose.bin')])
 checks['frozen_single_axe_preview']=profile['sample']['weapon_type']==7 and profile['sample']['item']==1940764111 and profile['combat_approved'] is False and profile['tracking_independent_of_animation'] is True
 checks['no_native_input_fixture']=not (a.probe/'controller-ui-fixture.txt').exists() and not any(e['event'].startswith('motion_attack_') or e['event'].startswith('motion_collider_applied') for e in events)
 checks['both_holding_modes']=set(e['two_hands'] for e in poses)=={True,False} and min(sum(e['two_hands']==v for e in poses) for v in (True,False))>=100
 checks['every_frame_profile_verified']=bool(poses) and len(poses)==len(weapon) and all(e['serial'] in weapon and e['resource_verified'] is True and e['combat_enabled'] is False for e in poses)
 modes=True
 for e in poses:
  row=sequence[min(e['frame']-1,len(sequence)-1)];ep,eq,both=calibrated(row,profile);modes &= e['two_hands']==both;errors['position']=max(errors['position'],max(abs(x-y) for x,y in zip(ep,e['position'])));errors['orientation']=max(errors['orientation'],min(max(abs(x-y) for x,y in zip(eq,e['orientation'])),max(abs(x+y) for x,y in zip(eq,e['orientation']))))
 checks['independent_calibrated_pose']=modes and errors['position']<2e-6 and errors['orientation']<2e-6
 # Check anatomical axes and only the selected finger/socket pose changes.
 source=Path(profile['source']);parents=struct.unpack('<102H',(source/'hand-pose/joint-parents.bin').read_bytes());default=(source/'hand-pose/default-joints.bin').read_bytes()[:4896];raw=(source/profile['sample']['folder']/'joints.bin').read_bytes();frozen=(a.probe/'grip-hand-pose.bin').read_bytes();bones=[]
 for i,parent in enumerate(parents):
  v=struct.unpack_from('<12f',default,i*48);local=pose(v[:4],v[4:7],v[8:11]);bones.append(local if parent==65535 else combine(bones[parent],local))
 axes=True;shape=True;fingers=set(range(45,64))|set(range(75,94));sockets={64,94}
 for c in profile['anatomy']:
  wrist=bones[c['wrist']];local=lambda n:mv(inv(wrist[0]),[u-v for u,v in zip(bones[n][1],wrist[1])]);z=unit([u-v for u,v in zip(local(c['index']),local(c['little']))]);x=unit([-v for v in cross(unit(local(c['middle'])),z)]);q=struct.unpack_from('<4f',frozen,c['socket']*48)
  axes &= max(abs(u-v) for u,v in zip(rotate(q,[0,0,1]),z))<2e-6 and max(abs(u-v) for u,v in zip(rotate(q,[1,0,0]),x))<2e-6
 for i in range(102):
  f=struct.unpack_from('<12f',frozen,i*48);d=struct.unpack_from('<12f',default,i*48);r=struct.unpack_from('<12f',raw,i*48)
  if i in fingers:shape &= max(abs(u-v) for u,v in zip(f[:4],normalize(r[:4])))<2e-6 and f[4:]==d[4:]
  elif i in sockets:shape &= max(abs(u-v) for u,v in zip(f[4:7],r[4:7]))<2e-6 and f[8:]==d[8:]
  else:shape &= f==d
 checks['anatomical_grip_axes']=axes;checks['frozen_fingers_only']=shape
 for n in ('motion-hand-checks.json','motion-visual-checks.json'):
  checks[n+'_PASS']=all(v=='PASS' for v in json.loads((a.probe/n).read_text())['checks'].values())
 checks['normal_exit']=json.loads((a.probe.parent/'process-exit.json').read_text(encoding='utf-8-sig'))['exit_code']==0
 checks['Core_Sync_clean']=(a.probe.parent/'validation.log').stat().st_size==0
 report=dict(checks={k:'PASS' if v else 'FAIL' for k,v in checks.items()},errors=errors,limitation=__doc__);(a.probe/'grip-preview-checks.json').write_text(json.dumps(report,indent=2)+'\n')
 for k,v in checks.items():print('PASS' if v else 'FAIL',k)
 print(errors);print('PASS',sum(checks.values()),'FAIL',sum(not v for v in checks.values()));return int(not all(checks.values()))
if __name__=='__main__':raise SystemExit(main())
