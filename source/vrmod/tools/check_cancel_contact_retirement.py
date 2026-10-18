"""Finite post-Impact offhand cancellation: exact native destruction, late event
suppression, quiescent Actor end and retirement of event/collider/watch storage.
Does not establish unlimited play, World epochs, native mixed actions or enemy HP.
"""
import argparse,hashlib,json,struct
from pathlib import Path

def audit(rows,allowed):
 ev=lambda n:[r for r in rows if r['event']==n]
 watch=ev('contact_lifetime_watch');begin=ev('contact_lifetime_destroy_begin');end=ev('contact_lifetime_destroy_end');retire=ev('contact_recycled');summary=ev('contact_recycle_removed');enable=ev('contact_recycle_enabled');fences=ev('contact_quiescence_sample')
 c=dict(one_cancel_action=len(watch)==len(begin)==len(end)==len(retire)==1,
        explicit_single_slot_cancel=len(enable)==1 and enable[0].get('post_Impact_cancel_trial') is True and enable[0]['event_slots']==enable[0]['collider_slots']==enable[0]['watch_slots']==enable[0]['expected_actions']==1 and enable[0]['serial_tokens'] and enable[0]['finite_trial'] and not enable[0]['writes_gameplay'],
        all_storage_empty=len(summary)==1 and summary[0]['retired']==1 and summary[0]['events_remaining']==summary[0]['colliders_remaining']==summary[0]['watch_slots_remaining']==0 and not summary[0]['continuity_fault'],
        no_failure=not any('failure' in r['event'] or 'failed' in r['event'] for r in rows))
 lh=ev('contact_lifetime_enabled');ld=ev('contact_lifetime_removed');qh=ev('contact_quiescence_enabled');qd=ev('contact_quiescence_removed')
 c['exact_native_hooks']=len(lh)==len(qh)==1 and lh[0]['rva']=='0x8b83b0' and qh[0]['rva']=='0x931ad0' and qh[0]['caller']=='0x93bdf3' and lh[0]['read_only'] and not lh[0]['retires_ledger_entries'] and qh[0]['read_only'] and qh[0]['retires_ledger_entries']
 c['destroy_restored_drained']=len(ld)==1 and ld[0]['watched']==ld[0]['destroyed']==1 and ld[0]['callbacks']==ld[0]['failures']==0 and ld[0]['restored']
 c['fence_restored_complete']=len(qd)==1 and qd[0]['restored'] and qd[0]['callbacks']==qd[0]['failures']==qd[0]['omitted']==0 and qd[0]['samples']==len(fences)>10
 c['scope_continuity']=bool(fences) and len({(r['world'],r['registry'],r['task_list'],r['actor']) for r in fences})==1 and all(a['tick']<=b['tick'] and a['entity_counter']<=b['entity_counter'] for a,b in zip(fences,fences[1:]))
 raw=bool(fences);quiet=bool(fences)
 for r in fences:
  b=bytes.fromhex(r['actor_bytes']);raw &= len(b)==3104 and (bool(b[0x1b1]&1),struct.unpack_from('<I',b)[0],struct.unpack_from('<q',b,8)[0],struct.unpack_from('<I',b,0x38)[0])==(r['active'],r['sequence'],r['trigger'],r['item'])
  quiet &= r['quiet']==all(r[k]==0 for k in ('running','queued','completed_pending','command_bytes','command_count','selector_callbacks','query_scopes')) and r['world_kind']==0 and int(r['task_list'],16)!=0 and r['read_only'] and r['retires_ledger_entries']
 c['raw_Actor_independent_decode']=bool(raw);c['full_native_and_mod_quiescence']=bool(quiet)
 for event in ('motion_event_recording_summary','collider_ready_recording_summary'):
  records=ev(event);c[event+'_complete']=len(records)==1 and records[0]['processed']==records[0]['recorded']+records[0]['witness'] and records[0]['omitted']==0
 app=[r for r in ev('motion_event_retime') if r['operation']=='append'];sup=[r for r in ev('motion_event_retime') if r['operation']=='suppress'];ab=ev('motion_collider_apply_begin');ae=ev('motion_collider_apply_end');cb=ev('motion_collider_cancel_begin');ce=ev('motion_collider_cancel_end')
 c['genuine_after_spawn_cancel']=len(app)==len(sup)==1 and not ev('motion_event_blocked') and bool(ab) and bool(cb) and len(ab)==len(ae) and len(cb)==len(ce) and all(not r['forced_fixture'] and r['latched'] for r in cb) and all(not r['collider_after'] and r['lookup_suppressed'] for r in ce)
 valid=False;m={}
 if c['one_cancel_action'] and c['genuine_after_spawn_cancel']:
  w,d,e,r=watch[0],begin[0],end[0],retire[0]
  key=lambda x:(x.get('watch_slot'),x.get('watch_serial'))
  same=lambda x,actor:all(x[k]==w[k] for k in ('world','sequence','item','trigger')) and x[actor]==w['actor']
  selects=[x for x in rows if x['event'] in ('motion_event_selection','motion_event_selection_budget_witness') and same(x,'entity')]
  active=[x for x in fences if x['active']];idle=[x for x in fences if not x['active'] and x['quiet'] and x['tick']==r['tick'] and x['sequence']==x['item']==x['trigger']==0]
  valid=key(w)==key(d)==key(e)==key(r)==(0,1) and all(x['world']==w['world'] and x['entity']==w['entity'] for x in (d,e,r)) and same(r,'actor') and same(app[0],'entity') and same(sup[0],'entity')
  valid &= int(d['caller'],16) in allowed and d['present'] and int(d['record'],16)!=0 and not e['present'] and e['record']=='0x0' and e['native_result']==1 and e['arguments_unchanged'] and e['return_unchanged']
  scopes=sorted(ab+cb,key=lambda x:x['ready_sample']);finishes=ae+ce
  valid &= all(x['entity']==w['entity'] for x in scopes) and d['uses']==len(scopes) and d['last_ready_sample']==scopes[-1]['ready_sample'] and w['ready_sample']==scopes[0]['ready_sample']
  valid &= app[0]['us']<w['us']<=ab[0]['us'] and all(x['us']<cb[0]['us'] for x in ab) and max(x['us'] for x in finishes)<d['us']<e['us']<r['us'] and sup[0]['us']<r['us']
  valid &= bool(active) and len(idle)==1 and bool(selects) and all(same(x,'actor') and (bytes.fromhex(x['actor_bytes'])[20],bytes.fromhex(x['actor_bytes'])[0x170])==(r['call'],r['sub']) for x in active) and max(x['us'] for x in selects)<idle[0]['us']<r['us']
  valid &= all(x['tick']<r['tick'] for x in active) and r['events_before']==r['colliders_before']==1 and r['events_after']==r['colliders_after']==r['watch_slots_remaining']==0 and r['native_Impact_absent'] and r['actor_cleared'] and r['quiet'] and not r['writes_gameplay']
  m=dict(entity=w['entity'],physics_updates=len(ab),cancel_scopes=len(cb),active_fences=len(active),after_destroy_us=r['us']-e['us'],after_suppress_us=r['us']-sup[0]['us'],after_last_selector_us=r['us']-max((x['us'] for x in selects),default=r['us']),retired_tick=r['tick'])
 c['exact_owner_token_cancel_destroy_suppress_and_clear_before_retire']=bool(valid)
 return c,m

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('probe',type=Path);p.add_argument('--negative-tests',action='store_true');a=p.parse_args()
 if (a.probe/'world-cancel-contact-recycle.txt').read_text().strip()!='EVR_CANCEL_CONTACT_RECYCLE_1':raise ValueError('dedicated post-Impact policy required')
 graph=Path('docs/analysis/entity_lifetime_destroy/index.json');g=json.loads(graph.read_text());exe=hashlib.sha256(Path('enshrouded.exe').read_bytes()).hexdigest()
 if g['exe_sha256']!=exe:raise ValueError('native caller graph belongs to a different executable')
 allowed={int(e['site'],16)+5 for e in g['edges'] if e['target']=='0x8b83b0'}
 def unique(pairs):
  d={}
  for k,v in pairs:
   if k in d:raise ValueError('duplicate diagnostic JSON key '+k)
   d[k]=v
  return d
 source=a.probe/'view-resource-probe.jsonl';rows=[json.loads(l,object_pairs_hook=unique) for l in source.read_text().splitlines()];c,m=audit(rows,allowed)
 if a.negative_tests and all(c.values()):
  for name,event,key,value in [('early_retire','contact_recycled','us',0),('wrong_serial','contact_lifetime_destroy_end','watch_serial',2),('wrong_owner','contact_recycled','actor',0),('entry_left','contact_recycled','events_after',1),('watch_left','contact_recycle_removed','watch_slots_remaining',1),('native_entity_present','contact_lifetime_destroy_end','present',True),('wrong_caller','contact_lifetime_destroy_begin','caller','0x1'),('incomplete_cancel','contact_lifetime_destroy_begin','uses',0),('resurrected_collider','motion_collider_cancel_end','collider_after',123),('forced_cancel','motion_collider_cancel_begin','forced_fixture',True),('unfinished_query','contact_quiescence_sample','query_scopes',1),('raw_active','contact_quiescence_sample','active',True),('World_discontinuity','contact_recycle_removed','continuity_fault',True),('missing_suppression','motion_event_retime','operation','append')]:
   bad=[dict(r,**{key:value}) if r['event']==event else r for r in rows];c['reject_'+name]=not all(audit(bad,allowed)[0].values())
 else:c['negative_baseline_qualified']=not a.negative_tests
 for name in ('offhand-release-checks.json','motion-hand-checks.json','motion-visual-checks.json','live-native-checks-fixed-root-motion.json'):
  report=json.loads((a.probe/name).read_text());c[name+'_PASS']=bool(report['checks']) and set(report['checks'].values())=={'PASS'}
 c['native_runner_PASS']=json.loads((a.probe/'probe-status.json').read_text())['status']=='PASS';c['normal_exit']=json.loads((a.probe.parent/'process-exit.json').read_text(encoding='utf-8-sig'))['exit_code']==0;c['Core_Sync_clean']=(a.probe.parent/'validation.log').stat().st_size==0
 result=dict(checks={k:'PASS' if v else 'FAIL' for k,v in c.items()},metrics=m,source_sha256=hashlib.sha256(source.read_bytes()).hexdigest(),exe_sha256=exe,static_call_graph=str(graph),limitation=__doc__);(a.probe/'cancel-contact-retirement-checks.json').write_text(json.dumps(result,indent=2)+'\n')
 for k,v in c.items():print('PASS' if v else 'FAIL',k)
 print('PASS',sum(bool(v) for v in c.values()),'FAIL',sum(not v for v in c.values()));print(json.dumps(m,indent=2));return int(not all(c.values()))
if __name__=='__main__':raise SystemExit(main())
