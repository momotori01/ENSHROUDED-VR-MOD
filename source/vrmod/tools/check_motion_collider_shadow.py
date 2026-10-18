"""Read-only rendered weapon/collider proposal validation; never proves VR damage."""
import argparse,copy,json,pathlib,struct,math

def audit(rows):
 ev=lambda name:[r for r in rows if r['event']==name]
 proposals=ev('motion_collider_shadow');eligible=[r for r in proposals if r['eligible']]
 poses={r['serial']:r for r in ev('motion_visual_pose')};end={r.get('ready_sample'):r for r in ev('sequence_collider_sample') if r.get('ready_sample')}
 checks={
  'shadow_enabled':len(ev('motion_collider_shadow_enabled'))==1,
  'proposals_observed':len(proposals)>=3,
  'eligible_owned_proposals':len(eligible)>=3,
  'no_shadow_or_observer_failure':not any(r['event'].endswith('_failure') for r in rows),
  'query_and_ecs_not_written':bool(proposals) and all(not r['applied'] and not r['ecs_written'] for r in proposals) and not ev('sequence_collider_identity_begin'),
 }
 match=world=owner=fresh=epochs=states=attachment=geometry=untouched=True
 differences=[]
 for r in eligible:
  p=poses.get(r['serial'],{})
  match &= all(p.get(a)==r.get(b) for a,b in [('frame','frame'),('time','time'),('item','item'),('entity','root'),('world_transform_bytes','render_transform'),('session','session'),('origin','origin'),('client_world','client_world')])
  world &= r['client_world']==r['camera_world']!='0x0' and r['actor_world']==r['impact_world']!='0x0'
  owner &= r['root']==r['impact_root']!=r['entity'] and r['item']==r['impact_item']!=0 and r['weapon_pide']==r['item_pide']!=0
  fresh &= all(0<=r['milliseconds']-r[k]<=limit for k,limit in [('pose_ms',200),('hand_ms',200),('actor_ms',200),('visual_ms',500)])
  epochs &= 0<=r['hand_frame']-r['frame']<=2 and r['hand_session']==r['session']!=0 and r['hand_origin']==r['origin']!=0
  states &= all(r[k] for k in ['normal_ui','first_person','tracked','actor_active','actor_alive','actor_ready'])
  a=bytes.fromhex(r['visual_attachment']);b=bytes.fromhex(r['impact_attachment'])
  attachment &= len(a)==48 and a==b and struct.unpack_from('<10f',a)==(0.,)*9+(1.,) and a[40:42]==b'\0\x09' and struct.unpack_from('<I',a,44)[0]==r['root']
  t=bytes.fromhex(r['proposed_transform']);o=bytes.fromhex(r['original_transform']);geometry &= len(t)==56 and len(o)==56
  if len(t)==len(o)==56:
   q=struct.unpack_from('<4f',t,24);s=struct.unpack_from('<3f',t,40);render=bytes.fromhex(r['render_transform']);geometry &= t[:40]==render[:40] and t[40:]==o[40:]
   geometry &= all(math.isfinite(x) for x in q+s) and abs(sum(x*x for x in q)-1)<1e-3 and s[0]==s[1]==s[2]>0
   x=struct.unpack_from('<3q',t);y=struct.unpack_from('<3q',o);differences.append(math.sqrt(sum(((a-b)/2**32)**2 for a,b in zip(x,y))))
 for r in proposals:
  s=end.get(r['ready_sample'],{})
  untouched &= s.get('transform_bytes')==r['original_transform'] and s.get('thread')==r['thread'] and s.get('entity')==r['entity'] and s.get('us',0)>r['us']
 for k,v in [('exact_display_pose',match),('qualified_worlds',world),('native_root_item_pide',owner),('fresh_samples',fresh),('session_origin_frame',epochs),('normal_gameplay_and_tracking',states),('identity_attachment_calibration',attachment),('valid_native_transform',geometry)]:checks[k]=bool(eligible) and bool(v)
 checks['native_transform_stays_original']=bool(proposals) and untouched
 checks['proposal_differs_from_animation']=bool(differences) and max(differences)>.1
 done=ev('motion_collider_shadow_removed');checks['observer_closed_channel_invalidated']=len(done)==1 and done[0]['channel_invalidated'] and done[0]['failures']==0 and done[0]['samples']==len(proposals) and done[0]['accepted']==len(eligible)
 metrics=dict(proposals=len(proposals),eligible=len(eligible),rejected=len(proposals)-len(eligible),reasons={v:sum(r['reason']==v for r in proposals) for v in sorted({r['reason'] for r in proposals})},animation_distance_min=min(differences,default=None),animation_distance_max=max(differences,default=None),pose_age_max=max((r['milliseconds']-r['pose_ms'] for r in eligible),default=None))
 return checks,metrics

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('probe',type=pathlib.Path);p.add_argument('--negative-tests',action='store_true');a=p.parse_args();rows=[json.loads(s) for s in (a.probe/'view-resource-probe.jsonl').read_text(encoding='utf8').splitlines()];c,m=audit(rows)
 if a.negative_tests:
  for name,key,value in [('wrong_world','impact_world','0x0'),('wrong_item','impact_item',0),('recenter','hand_origin',0),('old_pose','pose_ms',1),('tracking_lost','tracked',False),('ui_menu','normal_ui',False),('different_display','proposed_transform','00'*56),('write_in_shadow','applied',True)]:
   bad=copy.deepcopy(rows);r=next((r for r in bad if r['event']=='motion_collider_shadow' and r['eligible']),None)
   if r:r[key]=value
   c['reject_'+name]=r is not None and not all(audit(bad)[0].values())
 out=dict(checks={k:'PASS' if v else 'FAIL' for k,v in c.items()},passed=sum(c.values()),failed=sum(not v for v in c.values()),metrics=m,limitation=__doc__)
 (a.probe/'motion-collider-shadow-checks.json').write_text(json.dumps(out,indent=2)+'\n',encoding='utf8')
 for k,v in c.items():print('PASS' if v else 'FAIL',k)
 print('PASS',out['passed'],'FAIL',out['failed'],m);return bool(out['failed'])
if __name__=='__main__':raise SystemExit(main())
