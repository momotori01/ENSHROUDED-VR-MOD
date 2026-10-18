"""Bounded native hotbar switch/retained private weapon/independent hands proof. No combat proof."""
import argparse,copy,json
from collections import Counter
from pathlib import Path
from check_motion_visual import check as visual_check,rows
from check_motion_hands import check as hand_check,independent_first_pose,independent_grips

def audit(native,gpu,xr,sequence,profile):
 ev=lambda name:[e for e in native if e['event']==name]
 c,errors=visual_check(native,gpu,xr,sequence,'calibrated',profile)
 c.update({'hands_'+k:v for k,v in hand_check(native,equipment_switch=True).items()})
 pairs=ev('live_native_pair_complete');serials={e['serial'] for e in pairs};poses=ev('motion_visual_pose');pose={e['serial']:e for e in poses}
 selections={e['serial']:e for e in ev('motion_visual_selection')};created=ev('motion_visual_created');clone=created[0] if len(created)==1 else {};handle=clone.get('handle',0);item=clone.get('item',0)
 eyes=[e for e in ev('head_visibility_payload') if e['serial'] in serials];phases=ev('equipment_fixture_phase');rebounds=ev('motion_visual_rebound')
 ready={s for s in serials if selections.get(s,{}).get('weapon_ready')};other={s for s in serials if selections.get(s,{}).get('item') not in (0,item)}
 # Replace only assumptions specific to the former single-source, always-visible fixture.
 c['every_submitted_pair_has_one_hand_pose']=len(pose)==len(poses) and (set(pose)&serials)==ready and bool(ready) and bool(other)
 c['original_model_preserved_native_clone_hidden']=bool(poses) and all(e['serial'] in selections and e['handle']==handle and e['source_handle']==selections[e['serial']]['source_handle'] and e['item']==item==selections[e['serial']]['item'] and e['native_mask']==e['sdf_handle']==0 and e['source_unchanged'] for e in poses)
 c['both_eyes_use_owned_clone']=len(eyes)==2400 and all(Counter((e['eye'],e['culling_flags']&1) for e in eyes if e['serial']==s)=={(0,0):1,(0,1):1,(1,0):1,(1,1):1} for s in serials) and all(e.get('motion_visual') and e['hand_instance']==((handle&0xffffff) if e['serial'] in ready else 0xffffffff) and (e['serial'] not in ready or e['hand_mask_before']==0) for e in eyes)
 c['original_weapon_hidden_only_private_payload']=bool(eyes) and all(e['serial'] in selections and (not selections[e['serial']]['source_handle'] or (selections[e['serial']]['source_handle']&0xffffff) in e['indices']) and (handle&0xffffff) not in e['indices'] and e['only_private_stack_padding_changed'] for e in eyes)
 phase_values=[]
 for e in phases:
  if not phase_values or phase_values[-1]!=e['phase']:phase_values.append(e['phase'])
 c['native_observed_round_trip']=phase_values==[1,2,3,4,5,6,7,8] and all(e['screen']==1 and e['observation_valid'] for e in phases) and len({e['root'] for e in phases})==1 and all(e['item']==item for e in phases if e['phase'] in (1,7,8))
 c['no_private_weapon_without_owned_original']=all(not e['weapon_ready'] or (e['source_handle'] and e['item']==item and e['source_unchanged']) for e in selections.values())
 c['noncalibrated_equipment_is_suspended']=len(other)>=10 and all(s not in pose and not selections[s]['weapon_ready'] and selections[s]['source_unchanged'] for s in other) and bool(ev('motion_visual_dormant'))
 c['owned_clone_rebound_without_recreate']=len(rebounds)==1 and rebounds[0]['handle']==handle and rebounds[0]['item']==item and rebounds[0]['clone_retained'] and not rebounds[0]['combat_authorized'] and rebounds[0]['previous_source']!=rebounds[0]['source'] and rebounds[0]['references']>=2
 c['pose_before_and_after_rebound']=len(rebounds)==1 and any(e['frame']<rebounds[0]['frame'] and e['source_handle']==rebounds[0]['previous_source'] for e in poses) and any(e['frame']>=rebounds[0]['frame'] and e['source_handle']==rebounds[0]['source'] for e in poses) and bool(ev('motion_visual_resumed'))
 packets=[e['buttons'] for e in ev('controller_fixture_packet_polled')];compressed=[]
 for b in packets:
  if not compressed or compressed[-1]!=b:compressed.append(b)
 c['two_native_dpad_pulses_only']=compressed==[0,4,0,8,0]
 c['no_attack_or_gameplay_write_authority']=not any(ev(name) for name in ('native_attack_ticket','motion_attack_fixture_enabled','controller_attack_fixture_enabled','controller_attack_packet_polled','motion_collider_apply_enabled')) and len(ev('equipment_fixture_enabled'))==1 and ev('equipment_fixture_enabled')[0].get('attack_commands') is False and ev('equipment_fixture_enabled')[0].get('selection_writes') is False
 restored=ev('controller_fixture_restored')
 c['native_input_restored']=len(restored)==1 and all(restored[0].get(k) is True for k in ('pointers_restored','native_connection_seen','native_disconnection_seen','system_settings_unchanged')) and restored[0]['native_connected']==restored[0]['rescan_pending']==0
 return c,dict(pose_errors=errors,other_pairs=len(other),ready_pairs=len(ready),phases=phase_values,packets=compressed,rebounds=rebounds)

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('probe',type=Path);p.add_argument('--negative-tests',action='store_true');a=p.parse_args();probe=a.probe;launch=probe.parent
 if (probe/'world-motion-equipment-switch.txt').read_text().strip()!='EVR_MOTION_EQUIPMENT_SWITCH_1':raise ValueError('explicit switch fixture required')
 native=rows(probe/'view-resource-probe.jsonl');gpu=rows(launch/'vulkan.jsonl');xr=rows(launch/'native-xr/openxr.jsonl');profile=json.loads((probe/'grip-preview.json').read_text())
 seq=[[float(v) for v in l.split(',')] for l in (launch/'mock-hands.csv').read_text().splitlines() if l and not l.startswith('#')]
 c,m=audit(native,gpu,xr,seq,profile)
 c['independent_native_hand_FK'],m['hand_FK']=independent_first_pose(probe,native)
 c['independent_both_grips'],m['grips']=independent_grips(native,xr,seq,True)
 c['exact_fixture']=(launch/'mock-hands.csv').read_bytes()==(Path(__file__).resolve().parents[1]/'test_env/calibrated-grip-hands.csv').read_bytes()
 c['runner_PASS']=json.loads((probe/'probe-status.json').read_text())['status']=='PASS'
 c['normal_exit']=json.loads((launch/'process-exit.json').read_text(encoding='utf-8-sig'))['exit_code']==0
 c['Core_Sync_clean']=(launch/'validation.log').stat().st_size==0
 if a.negative_tests:
  for event,key,value in [('motion_visual_pose','source_unchanged',False),('motion_visual_pose','q32',[0,0,0]),('motion_visual_rebound','clone_retained',False),('motion_visual_rebound','source',0),('motion_visual_selection','weapon_ready',True),('controller_fixture_packet_polled','buttons',16384),('equipment_fixture_phase','phase',9),('motion_hand_pose','frame',0),('motion_visual_disposed','reference_released',False),('controller_fixture_restored','pointers_restored',False)]:
   bad=copy.deepcopy(native)
   for e in bad:
    if e['event']==event:e[key]=value
   c['reject_'+event+'_'+key]=not all(audit(bad,gpu,xr,seq,profile)[0].values())
 report=dict(checks={k:'PASS' if v else 'FAIL' for k,v in c.items()},measurements=m,limitation=__doc__);(probe/'equipment-switch-checks.json').write_text(json.dumps(report,indent=2)+'\n')
 for k,v in c.items():print('PASS' if v else 'FAIL',k)
 print('PASS',sum(bool(v) for v in c.values()),'FAIL',sum(not v for v in c.values()));return int(not all(c.values()))
if __name__=='__main__':raise SystemExit(main())
