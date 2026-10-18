"""Private native hand pose/frame/ownership checks; visual calibration needs images."""
import argparse,copy,json,math,struct
from pathlib import Path
from check_recorded_xr_native import rotate,multiply,normalize

def rows(p):return [json.loads(x) for x in p.read_text(encoding='utf-8-sig').splitlines()]
def check(native,without_weapon=False,equipment_switch=False,active_exit=False,quick_wheel=False,expected_pairs=600):
 ev=lambda n:[x for x in native if x['event']==n]
 created=ev('motion_hand_created');poses=ev('motion_hand_pose');modelEnd=ev('motion_hand_model_disposed');poseEnd=ev('motion_hand_pose_disposed');pairs=ev('live_native_pair_complete')
 c=created[0] if len(created)==1 else {};s={x['serial'] for x in pairs};p={x['serial']:x for x in poses};w={x['serial']:x for x in ev('motion_visual_pose')};eyes=[x for x in ev('head_visibility_payload') if x['serial'] in s];original=[x for x in ev('world_shared_render_stage') if x['eye']==-1]
 contexts={x['serial']:x for x in ev('motion_visual_context')}
 sync=contexts if without_weapon or equipment_switch or quick_wheel else w
 endings=ev('world_active_xr_exit_begin');count_ok=len(pairs)==expected_pairs
 if active_exit:count_ok=len(endings)==1 and 30<=len(pairs)<3000 and endings[0]['limit']==3000 and endings[0]['pairs']==len(pairs)+int(endings[0]['pair_pending'])
 checks=dict(
  clone_and_pose_distinct=len(created)==1 and c['handle']!=c['source_handle'] and c['pose_handle']!=c['source_pose'],
  reviewed_skeleton=c.get('joints')==102 and c.get('skin_joints')==42 and c.get('private_pose') is True,
  every_pair_same_frame_hands=count_ok and len(p)==len(poses) and s<=set(p) and all(x['serial'] in sync and x['frame']==sync[x['serial']]['frame'] and x['time']==sync[x['serial']]['time'] for x in poses),
  original_model_and_pose_isolation=bool(poses) and all(x.get('source_unchanged') is True and x['handle']==c.get('handle') and x['pose_handle']==c.get('pose_handle') for x in poses),
  native_hidden_SDF_removed=bool(poses) and c.get('mask')==c.get('sdf')==0 and all(x['mask']==x['sdf']==0 for x in poses),
  skinning_palette_complete=bool(poses) and all(x['palette_count']==42 for x in poses),
  update_precedes_native_upload=bool(poses) and all(len([o for o in original if o['serial']==x['serial'] and o['us']>x['us']])==2 for x in poses),
  both_eyes_force_owned_hands=bool(eyes) and all(x.get('extra_instance')==(c.get('handle',0)&0xffffff) and x['extra_instance'] not in x['indices'] and (c.get('source_handle',0)&0xffffff) in x['indices'] for x in eyes),
  model_then_pose_retirement=len(modelEnd)==len(poseEnd)==1 and modelEnd[0]['us']<poseEnd[0]['us'] and all(e['generation_invalidated'] and e['reference_released'] for e in modelEnd+poseEnd) and poseEnd[0]['updates']==len(poses),
  no_native_failures=not any('failure' in x['event'] or 'failed' in x['event'] for x in native))
 if contexts or without_weapon:
  checks['current_world_context_before_hands']=bool(contexts) and all(x['serial'] in contexts and contexts[x['serial']]['frame']==x['frame'] and contexts[x['serial']]['time']==x['time'] and contexts[x['serial']]['us']<x['us'] and contexts[x['serial']]['cpu_complete'] and contexts[x['serial']]['same_frame_hand'] and contexts[x['serial']]['first_person'] for x in poses)
  checks['one_scene_view_thread']=len({(x['scene'],x['view'],x['thread']) for x in contexts.values()})==1
 if without_weapon:
  selections={x['serial']:x for x in ev('motion_visual_selection')}
  checks['no_weapon_clone_or_pose']=not w and not ev('motion_visual_created') and not ev('motion_visual_disposed')
  checks['unsupported_bow_proven_every_pair']=s<=set(selections) and bool(s) and all(selections[i]['item']==327157552 and selections[i]['slot']==9 and selections[i]['category']==2 and selections[i]['item_slot']==5 and selections[i]['target']==0 and selections[i]['source_handle'] and selections[i]['source_unchanged'] and not selections[i]['weapon_ready'] for i in s)
  checks['source_hidden_only_private_eyes']=bool(eyes) and all(x['serial'] in selections and x['entity']==selections[x['serial']]['parent'] and (selections[x['serial']]['source_handle']&0xffffff) in x['indices'] and x['hand_instance']==0xffffffff and x['only_private_stack_padding_changed'] for x in eyes)
  checks['no_attack_authority_or_collider']=not any(x['event'] in ('native_attack_ticket','motion_attack_fixture_enabled','controller_attack_fixture_enabled','controller_attack_packet_polled') for x in native)
 return checks


def independent_first_pose(probe,native):
 poses=[x for x in native if x['event']=='motion_hand_pose']
 if not poses:return False,{}
 h=probe/'hand-pose';parents=struct.unpack('<102H',(h/'joint-parents.bin').read_bytes());b=(probe/'hand-owned-joints.bin').read_bytes();d=((probe/'grip-hand-pose.bin') if (probe/'grip-hand-pose.bin').exists() else (h/'default-joints.bin')).read_bytes()
 if len(b)!=102*48:return False,{}
 worlds=[];unmodified=True
 for i,parent in enumerate(parents):
  j=struct.unpack_from('<12f',b,i*48);q=normalize(j[:4]);p=list(j[4:7])
  if parent!=65535:
   if parent>=i:return False,{}
   pq,pp=worlds[parent];q=normalize(multiply(pq,q));p=[x+y for x,y in zip(pp,rotate(pq,p))]
  worlds.append((q,p))
  if i not in (43,73):
   orig=struct.unpack_from('<12f',d,i*48);unmodified &= all(abs(a-c)<2e-6 for a,c in zip(orig,j))
 errp=errq=0
 for h,index in enumerate((64,94)):
  target=poses[0]['grips'][h];q,p=worlds[index]
  errp=max(errp,max(abs(a-c) for a,c in zip(p,target[:3])));errq=max(errq,min(max(abs(a-c) for a,c in zip(q,target[3:])),max(abs(a+c) for a,c in zip(q,target[3:]))))
 return unmodified and errp<2e-5 and errq<2e-5,dict(socket_position=errp,socket_orientation=errq,other_local_joints_unchanged=bool(unmodified))

def independent_grips(native,xr,sequence,require_motion=False):
 poses=[x for x in native if x['event']=='motion_hand_pose'];eyes=[x for x in xr if x['event']=='runtime_eye_pose']
 good=bool(poses) and len(eyes)>=2 and bool(sequence)
 if not good:return False,{}
 center=[sum(e['position'][i] for e in eyes[:2])/2 for i in range(3)]
 good &= all(max(abs(t) for t in e['orientation'][:3])<1e-7 and abs(e['orientation'][3]-1)<1e-7 and abs(e['position'][1]-center[1])<1e-6 and abs(e['position'][2]-center[2])<1e-6 for e in eyes)
 errors=dict(left_position=0.,right_position=0.,left_orientation=0.,right_orientation=0.)
 for pose in poses:
  if pose['frame']<1:return False,errors
  row=sequence[min(pose['frame']-1,len(sequence)-1)]
  for h,side in enumerate(('left','right')):
   values=row[1+13*h:14+13*h];actual=pose['grips'][h]
   expected=[values[0]-center[0],values[1]-center[1],-(values[2]-center[2])]
   yaw,pitch,roll=[math.radians(v)/2 for v in values[3:6]]
   q=multiply(multiply([0,math.sin(yaw),0,math.cos(yaw)],[math.sin(pitch),0,0,math.cos(pitch)]),[0,0,math.sin(roll),math.cos(roll)])
   q=[-q[0],-q[1],q[2],q[3]]
   errors[side+'_position']=max(errors[side+'_position'],max(abs(a-b) for a,b in zip(expected,actual[:3])))
   errors[side+'_orientation']=max(errors[side+'_orientation'],min(max(abs(a-b) for a,b in zip(q,actual[3:])),max(abs(a+b) for a,b in zip(q,actual[3:]))))
 if require_motion:
  good &= all(len({tuple(p['grips'][h][:3]) for p in poses})>100 and len({tuple(p['grips'][h][3:]) for p in poses})>100 for h in (0,1))
 return bool(good and all(e<2e-6 for e in errors.values())),errors

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('probe',type=Path);p.add_argument('--negative-tests',action='store_true');p.add_argument('--quick-wheel',action='store_true',help='Explicit wheel fixture: hands follow same-frame context while an unreviewed weapon is suspended');p.add_argument('--active-exit',action='store_true',help='Explicit early Scene exit with independently audited resource retirement');p.add_argument('--require-both-moving',action='store_true');p.add_argument('--guard-posture',action='store_true',help='Exact guard fixture: both hands translate, rotations remain fixed');p.add_argument('--equipment-switch',action='store_true',help='Explicit bounded hotbar fixture with independently checked weapon lifecycle');p.add_argument('--without-weapon',action='store_true',help='Explicit unsupported bow fixture: no tracked weapon, independent current context required');a=p.parse_args();native=rows(a.probe/'view-resource-probe.jsonl')
 # The pair count is the runner's explicit live config (600 for finite fixtures, up to 3000 for endurance).
 config=a.probe/'live-xr-config.txt';expected=int(config.read_text().split()[1]) if config.is_file() else 600
 checks=check(native,a.without_weapon,a.equipment_switch,a.active_exit,a.quick_wheel,expected)
 if a.quick_wheel:
  import hashlib
  report=json.loads((a.probe/'quick-wheel-checks.json').read_text())
  checks['independently_audited_wheel_same_raw']=not (a.active_exit or a.without_weapon or a.equipment_switch or a.guard_posture) and report['status']=='PASS' and all(report['checks'].values()) and report['raw_sha256']==hashlib.sha256((a.probe/'view-resource-probe.jsonl').read_bytes()).hexdigest() and (a.probe/'controller-ui-fixture.txt').read_text().strip() in ('EVR_QUICK_WHEEL_1','EVR_QUICK_WHEEL_REBIND_1','EVR_QUICK_WHEEL_CONSUMABLE_1','EVR_QUICK_WHEEL_CONSUMABLE_2','EVR_QUICK_WHEEL_SURVEY_1','EVR_GRIP_ROUTER_WHEEL_1','EVR_GRIP_ROUTER_EQUIP_1','EVR_GRIP_ROUTER_EQUIP_2')
 if a.active_exit:
  report=json.loads((a.probe/'active-xr-exit-checks.json').read_text());checks['explicit_active_exit_audited']=report['status']=='PASS' and (a.probe/'world-active-xr-exit.txt').read_text().strip()=='EVR_WORLD_ACTIVE_XR_EXIT_1' and not (a.probe/'controller-ui-fixture.txt').exists()
 if a.equipment_switch and ((a.probe/'world-motion-equipment-switch.txt').read_text().strip()!='EVR_MOTION_EQUIPMENT_SWITCH_1' or a.without_weapon):raise ValueError('isolated equipment fixture required')
 checks['independent_native_joint_FK'],errors=independent_first_pose(a.probe,native)
 xr=rows(a.probe.parent/'native-xr/openxr.jsonl');sequence=[[float(v) for v in line.split(',')] for line in (a.probe.parent/'mock-hands.csv').read_text().splitlines() if line and not line.startswith('#')]
 checks['independent_both_grips_CSV'],gripErrors=independent_grips(native,xr,sequence,a.require_both_moving);errors['CSV']=gripErrors
 if a.guard_posture:
  if a.require_both_moving or a.without_weapon or a.equipment_switch or (a.probe/'controller-ui-fixture.txt').read_text().strip()!='EVR_MOTION_GUARD_1':raise ValueError('isolated posture fixture required')
  from generate_guard_posture_fixture import verify
  verify(a.probe.parent/'mock-hands.csv',json.loads((a.probe/'grip-preview.json').read_text()),delay=350)
  pose=[x for x in native if x['event']=='motion_hand_pose']
  checks['guard_both_hands_translate_without_rotation']=bool(pose) and all(len({tuple(x['grips'][h][:3]) for x in pose})>20 and max(min(max(abs(a-b) for a,b in zip(x['grips'][h][3:],pose[0]['grips'][h][3:])),max(abs(a+b) for a,b in zip(x['grips'][h][3:],pose[0]['grips'][h][3:]))) for x in pose)<2e-6 for h in (0,1))
 checks['clean_Core_Sync']=(a.probe.parent/'validation.log').is_file() and (a.probe.parent/'validation.log').stat().st_size==0
 checks['runner_PASS']=json.loads((a.probe/'probe-status.json').read_text())['status']=='PASS'
 checks['normal_exit']=json.loads((a.probe.parent/'process-exit.json').read_text(encoding='utf-8-sig'))['exit_code']==0
 if a.negative_tests:
  base_valid=all(check(native,a.without_weapon,a.equipment_switch,a.active_exit,a.quick_wheel,expected).values())
  if not a.active_exit:checks['reject_configured_pair_count_mismatch']=base_valid and not all(check(native,a.without_weapon,a.equipment_switch,a.active_exit,a.quick_wheel,expected+1).values())
  for h in (0,1):
   bad=copy.deepcopy(native)
   for x in bad:
    if x['event']=='motion_hand_pose':x['grips'][h][0]+=.01
   checks['reject_wrong_grip_'+str(h)]=not independent_grips(bad,xr,sequence)[0]
  for event,key,value in [('motion_hand_pose','source_unchanged',False),('motion_hand_pose','frame',0),('motion_hand_pose','palette_count',1),('head_visibility_payload','extra_instance',0),('motion_hand_model_disposed','generation_invalidated',False),('motion_hand_pose_disposed','reference_released',False)]:
   bad=copy.deepcopy(native)
   for x in bad:
    if x['event']==event:x[key]=value
   checks['reject_'+event+'_'+key]=base_valid and not all(check(bad,a.without_weapon,a.equipment_switch,a.active_exit,a.quick_wheel,expected).values())
 if a.without_weapon and a.negative_tests:
  for event,key,value in [('motion_visual_context','frame',0),('motion_visual_context','cpu_complete',False),('motion_visual_context','time',0),('motion_visual_selection','weapon_ready',True),('motion_visual_selection','source_unchanged',False),('motion_visual_selection','slot',8),('head_visibility_payload','hand_instance',1)]:
   bad=copy.deepcopy(native)
   for x in bad:
    if x['event']==event:x[key]=value
   checks['reject_'+event+'_'+key]=not all(check(bad,True).values())
  for event in ('motion_visual_context','motion_visual_selection'):
   checks['reject_missing_'+event]=not all(check([x for x in native if x['event']!=event],True).values())
 report=dict(checks={k:'PASS' if v else 'FAIL' for k,v in checks.items()},errors=errors,limitation=__doc__);(a.probe/'motion-hand-checks.json').write_text(json.dumps(report,indent=2)+'\n')
 for k,v in checks.items():print('PASS' if v else 'FAIL',k)
 print(errors);print('PASS',sum(checks.values()),'FAIL',sum(not x for x in checks.values()));return int(not all(checks.values()))
if __name__=='__main__':raise SystemExit(main())
