"""Read-only owned Impact destruction evidence, not proof that all old tasks are quiescent.
No ledger reclamation, native generation guarantee, enemy damage, or unlimited-session claim.
"""
import argparse,json,hashlib
from pathlib import Path
from check_contact_log_budget import audit as budget_audit
from generate_melee_repeat_fixture import verify

def audit(rows,allowed_callers):
 ev=lambda n:[r for r in rows if r['event']==n]
 watch=ev('contact_lifetime_watch');begin=ev('contact_lifetime_destroy_begin');end=ev('contact_lifetime_destroy_end');enabled=ev('contact_lifetime_enabled');done=ev('contact_lifetime_removed')
 key=lambda r:(r['world'],r['entity']);keys={key(r) for r in watch}
 c=dict(explicit_read_only_mode=len(enabled)==1 and enabled[0]['rva']=='0x8b83b0' and enabled[0]['read_only'] and enabled[0]['retires_ledger_entries'] is False,
        two_distinct_watched_Impacts=len(watch)==len(keys)==2 and all(len(bytes.fromhex(r.get('event_bytes','')))==24 for r in watch),
        exactly_two_destructions=len(begin)==len(end)==2 and {key(r) for r in begin}=={key(r) for r in end}==keys,
        native_entity_present_before=bool(begin) and all(r['present'] and int(r['record'],16)!=0 and r['read_only'] for r in begin),
        native_entity_absent_after=bool(end) and all(not r['present'] and r['record']=='0x0' and r['native_result']==1 and r['return_unchanged'] and r['arguments_unchanged'] and r['read_only'] for r in end),
        validated_direct_native_callers=bool(begin) and all(int(r['caller'],16) in allowed_callers for r in begin),
        hook_restored_drained=len(done)==1 and done[0]['watched']==done[0]['destroyed']==2 and done[0]['failures']==done[0]['callbacks']==0 and done[0]['restored'] and done[0]['read_only'] and done[0]['retires_ledger_entries'] is False,
        no_lifetime_failures=not ev('contact_lifetime_failure'))
 timeline=[];valid=True
 for w in watch:
  bs=[r for r in begin if key(r)==key(w)];es=[r for r in end if key(r)==key(w)]
  applied=[r for r in ev('motion_collider_apply_begin') if r['entity']==w['entity']];finished=[r for r in ev('motion_collider_apply_end') if r['ready_sample'] in {a['ready_sample'] for a in applied}]
  retimes=[r for r in ev('motion_event_retime') if r['world']==w['world'] and r['entity']==w['actor'] and r['sequence']==w['sequence'] and r['item']==w['item'] and r['trigger']==w['trigger']]
  valid &= len(bs)==len(es)==1 and bool(applied) and len(applied)==len(finished) and len(retimes)==2
  if len(bs)!=1 or len(es)!=1 or not applied or len(retimes)!=2:continue
  b,e=bs[0],es[0]
  valid &= w['read_only'] and w['ready_sample']==applied[0]['ready_sample'] and w['us']<=applied[0]['us'] and b['uses']==len(applied) and b['last_ready_sample']==applied[-1]['ready_sample'] and max(r['us'] for r in finished)<b['us']<e['us'] and b['thread']==e['thread'] and retimes[0]['operation']=='append' and retimes[1]['operation']=='suppress' and retimes[0]['us']<w['us']
  timeline.append(dict(world=w['world'],entity=w['entity'],record_at_watch=w['record'],record_at_destroy=b['record'],counter_at_watch=w['counter'],counter_at_destroy=b['counter'],caller=b['caller'],ready_samples=len(applied),last_query_to_destroy_us=b['us']-max(r['us'] for r in finished),authored_suppression_to_destroy_us=b['us']-retimes[1]['us'],native_destroy_duration_us=e['us']-b['us'],destroy_after_authored_suppression=b['us']>retimes[1]['us']))
 c['each_original_action_exact_physics_then_destroy']=len(timeline)==2 and valid
 return c,dict(timeline=timeline,record_addresses_reused=len(watch)==2 and watch[0]['record']==watch[1]['record'],world_entity_ids=[w['entity'] for w in watch])

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('probe',type=Path);p.add_argument('--negative-tests',action='store_true');a=p.parse_args();probe=a.probe
 if (probe/'world-contact-lifetimes.txt').read_text().strip()!='EVR_CONTACT_LIFETIMES_1':raise ValueError('explicit read-only policy required')
 graph=Path('docs/analysis/entity_lifetime_destroy/index.json');g=json.loads(graph.read_text());exe_sha=hashlib.sha256(Path('enshrouded.exe').read_bytes()).hexdigest()
 if g['exe_sha256']!=exe_sha:raise ValueError('static native callers belong to another binary')
 allowed={int(e['site'],16)+5 for e in g['edges'] if e['target']=='0x8b83b0'}
 def unique_fields(pairs):
  out={}
  for key,value in pairs:
   if key in out:raise ValueError('duplicate diagnostic JSON key: '+key)
   out[key]=value
  return out
 read=lambda p:[json.loads(l,object_pairs_hook=unique_fields) for l in p.read_text(encoding='utf-8-sig').splitlines()]
 source=probe/'view-resource-probe.jsonl';rows=read(source);c,m=audit(rows,allowed)
 if a.negative_tests and all(c.values()):
  for name,event,field,value in [('present_after','contact_lifetime_destroy_end','present',True),('zero_native_result','contact_lifetime_destroy_end','native_result',0),('wrong_entity','contact_lifetime_destroy_end','entity',0),('wrong_world','contact_lifetime_destroy_end','world','0x0'),('not_restored','contact_lifetime_removed','restored',False),('wrong_caller','contact_lifetime_destroy_begin','caller','0x1'),('live_callback','contact_lifetime_removed','callbacks',1),('incomplete_physics','contact_lifetime_destroy_begin','uses',0),('destroy_too_early','contact_lifetime_destroy_begin','us',0),('ledger_reclaim','contact_lifetime_enabled','retires_ledger_entries',True)]:
   bad=[dict(r,**{field:value}) if r['event']==event else r for r in rows];c['reject_'+name]=not all(audit(bad,allowed)[0].values())
 elif a.negative_tests:c['negative_baseline_qualified']=False
 profile=json.loads((probe/'grip-preview.json').read_text());verify(probe.parent/'mock-hands.csv',profile)
 seq=[[float(v) for v in l.split(',')] for l in (probe.parent/'mock-hands.csv').read_text().splitlines() if l and not l.startswith('#')]
 bc,bm=budget_audit(rows,seq,read(probe.parent/'native-xr/openxr.jsonl'),read(probe.parent/'mock.jsonl'),profile,float((probe/'world-two-hand-attack.txt').read_text().split()[1]));c.update({'budget_'+k:v for k,v in bc.items()});m['contact']=bm
 c['runner_PASS']=json.loads((probe/'probe-status.json').read_text())['status']=='PASS';c['Core_Sync_clean']=(probe.parent/'validation.log').stat().st_size==0;c['normal_exit']=json.loads((probe.parent/'process-exit.json').read_text(encoding='utf-8-sig'))['exit_code']==0
 report=dict(checks={k:'PASS' if v else 'FAIL' for k,v in c.items()},metrics=m,source_sha256=hashlib.sha256(source.read_bytes()).hexdigest(),exe_sha256=exe_sha,static_call_graph=str(graph),limitation=__doc__);(probe/'contact-lifetime-checks.json').write_text(json.dumps(report,indent=2)+'\n')
 for k,v in c.items():print('PASS' if v else 'FAIL',k)
 print('PASS',sum(bool(v) for v in c.values()),'FAIL',sum(not v for v in c.values()));print(json.dumps(m['timeline'],indent=2));return int(not all(c.values()))
if __name__=='__main__':raise SystemExit(main())
