"""Observe quiet server-World command-flush boundaries after the last native event selection.
Finite evidence only: this does not retire history, prove rollback absence, or enable unlimited play.
"""
import argparse,json,hashlib,struct
from pathlib import Path

def audit(rows,retiring=False):
 ev=lambda n:[r for r in rows if r['event']==n]
 enabled=ev('contact_quiescence_enabled');samples=ev('contact_quiescence_sample');end=ev('contact_quiescence_removed');watches=ev('contact_lifetime_watch');destroyed=ev('contact_lifetime_destroy_end')
 selects=[r for r in rows if r['event'] in ('motion_event_selection','motion_event_selection_budget_witness')]
 c=dict(explicit_read_only_flush=len(enabled)==1 and enabled[0]['rva']=='0x931ad0' and enabled[0]['caller']=='0x93bdf3' and enabled[0]['read_only'] and enabled[0]['retires_ledger_entries'] is retiring,
        exact_observation_count=len(end)==1 and end[0]['seen']>=end[0]['samples']==len(samples)>10 and end[0]['omitted']==0,
        observer_restored_drained=len(end)==1 and end[0]['restored'] and end[0]['callbacks']==end[0]['failures']==0 and end[0]['read_only'] and end[0]['retires_ledger_entries'] is retiring,
        no_quiescence_failure=not ev('contact_quiescence_failure'),
        observed_only_owned_server_world=bool(samples) and all(r['world_kind']==0 and any(w['world']==r['world'] and w['actor']==r['actor'] for w in watches) for r in samples))
 raw=quiet=True
 counts=('running','queued','completed_pending','command_bytes','command_count','selector_callbacks','query_scopes')
 for r in samples:
  b=bytes.fromhex(r['actor_bytes'])
  raw &= len(b)==3104 and (bool(b[0x1b1]&1),struct.unpack_from('<I',b,0)[0],struct.unpack_from('<q',b,8)[0],struct.unpack_from('<I',b,0x38)[0])==(r['active'],r['sequence'],r['trigger'],r['item']) and r['read_only'] and r['retires_ledger_entries'] is retiring
  quiet &= r['quiet']==all(r[k]==0 for k in counts) and int(r['task_list'],16)!=0 and 0<r['command_queues']<=1024
 c['raw_Actor_decodes_independently']=bool(samples) and raw;c['quiet_requires_all_native_and_mod_counters_empty']=bool(samples) and quiet
 timelines=[]
 for w in watches:
  selected=[r for r in selects if (r['world'],r['entity'],r['sequence'],r['item'],r['trigger'])==(w['world'],w['actor'],w['sequence'],w['item'],w['trigger'])]
  gone=[r for r in destroyed if (r['world'],r['entity'])==(w['world'],w['entity'])]
  next_start=min((r['us'] for r in watches if r['us']>w['us']),default=float('inf'))
  current=[r for r in samples if r['world']==w['world'] and r['actor']==w['actor'] and w['us']<r['us']<next_start]
  idle=[r for r in current if not r['active'] and r['quiet']]
  active=[r for r in current if r['active'] and r['sequence']==w['sequence'] and r['trigger']==w['trigger'] and r['item']==w['item']]
  valid=bool(selected) and len(gone)==1 and bool(idle) and bool(active)
  if valid:
   first=idle[0];last=max(r['us'] for r in selected);valid &= first['us']>last and first['us']>gone[0]['us'] and all(r['us']<first['us'] for r in active)
   timelines.append(dict(entity=w['entity'],trigger=w['trigger'],active_fences=len(active),quiet_active_fences=sum(r['quiet'] for r in active),idle_fence_tick=first['tick'],idle_fence_after_last_select_us=first['us']-last,idle_fence_after_destroy_us=first['us']-gone[0]['us'],idle_fence_thread=first['thread'],destroy_thread=gone[0]['thread']))
  c['quiet_idle_after_action_'+str(w['entity'])]=valid
 c['two_independent_observed_idle_boundaries']=len(watches)==len(timelines)==2
 return c,dict(timeline=timelines,samples=len(samples),quiet_samples=sum(r['quiet'] for r in samples),not_quiet_samples=sum(not r['quiet'] for r in samples))

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('probe',type=Path);p.add_argument('--negative-tests',action='store_true');a=p.parse_args();probe=a.probe
 if (probe/'world-contact-quiescence.txt').read_text().strip()!='EVR_CONTACT_QUIESCENCE_1':raise ValueError('explicit finite policy required')
 source=probe/'view-resource-probe.jsonl';digest=hashlib.sha256(source.read_bytes()).hexdigest();proof=json.loads((probe/'contact-lifetime-checks.json').read_text())
 if proof['source_sha256']!=digest or set(proof['checks'].values())!={'PASS'}:raise ValueError('passing full native lifetime/contact proof for this exact raw source required')
 def unique(pairs):
  d={}
  for k,v in pairs:
   if k in d:raise ValueError('duplicate JSON key '+k)
   d[k]=v
  return d
 rows=[json.loads(l,object_pairs_hook=unique) for l in source.read_text().splitlines()];c,m=audit(rows)
 if a.negative_tests and all(c.values()):
  for name,event,field,value in [('raw_active','contact_quiescence_sample','active',True),('queued_command','contact_quiescence_sample','command_count',1),('unfinished_task','contact_quiescence_sample','running',1),('live_selector','contact_quiescence_sample','selector_callbacks',1),('borrowed_query','contact_quiescence_sample','query_scopes',1),('client_world','contact_quiescence_sample','world_kind',2),('wrong_actor','contact_quiescence_sample','actor',0),('omitted_fence','contact_quiescence_removed','omitted',1),('hook_not_restored','contact_quiescence_removed','restored',False),('silent_reclaim','contact_quiescence_enabled','retires_ledger_entries',True)]:
   bad=[dict(r,**{field:value}) if r['event']==event else r for r in rows];c['reject_'+name]=not all(audit(bad)[0].values())
 else:c['negative_baseline_qualified']=not a.negative_tests
 c['full_contact_lifetime_proof_PASS']=True
 report=dict(checks={k:'PASS' if v else 'FAIL' for k,v in c.items()},metrics=m,source_sha256=digest,limitation=__doc__);(probe/'contact-quiescence-checks.json').write_text(json.dumps(report,indent=2)+'\n')
 for k,v in c.items():print('PASS' if v else 'FAIL',k)
 print('PASS',sum(bool(v) for v in c.values()),'FAIL',sum(not v for v in c.values()));print(json.dumps(m,indent=2));return int(not all(c.values()))
if __name__=='__main__':raise SystemExit(main())
