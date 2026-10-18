"""Finite native two-action, single-slot MOD-ledger reuse proof.
Does not prove unlimited play, pre-Impact cancellation retirement, World recreation,
network rollback handling or damage to an actual enemy.
"""
import argparse,hashlib,json,struct
from pathlib import Path
from check_contact_quiescence import audit as quiescence_audit

def audit(rows):
 ev=lambda n:[r for r in rows if r['event']==n]
 c,m=quiescence_audit(rows,retiring=True)
 enable=ev('contact_recycle_enabled');end=ev('contact_recycle_removed');retired=ev('contact_recycled');watches=ev('contact_lifetime_watch');fences=ev('contact_quiescence_sample')
 c['explicit_single_slot_finite_trial']=len(enable)==1 and enable[0]['event_slots']==enable[0]['collider_slots']==1 and enable[0]['expected_actions']==2 and enable[0]['finite_trial'] and enable[0]['writes_gameplay'] is False and enable[0]['changes_mod_ledgers']
 c['two_exact_retirements']=len(retired)==2 and [r['retired'] for r in retired]==[1,2] and len({(r['world'],r['entity'],r['trigger']) for r in retired})==2
 c['each_slot_one_to_zero']=bool(retired) and all(r['events_before']==r['colliders_before']==1 and r['events_after']==r['colliders_after']==0 and r['native_Impact_absent'] and r['actor_cleared'] and r['quiet'] and r['writes_gameplay'] is False and r['changes_mod_ledgers'] for r in retired)
 c['both_slots_empty_at_shutdown']=len(end)==1 and end[0]['retired']==2 and end[0]['events_remaining']==end[0]['colliders_remaining']==0 and not end[0]['continuity_fault'] and end[0]['writes_gameplay'] is False and end[0]['changes_mod_ledgers']
 c['observed_scope_and_tick_counter_continuity']=bool(fences) and len({(r['world'],r['registry'],r['task_list'],r['actor']) for r in fences})==1 and all(a['tick']<=b['tick'] and a['entity_counter']<=b['entity_counter'] for a,b in zip(fences,fences[1:]))
 timeline=[]
 selections=ev('motion_event_selection')+ev('motion_event_selection_budget_witness');ready=ev('sequence_collider_ready_sample')+ev('sequence_collider_ready_budget_witness')
 appends=[r for r in ev('motion_event_retime') if r['operation']=='append']
 for w in watches:
  matches=[r for r in retired if all(r[k]==w[k] for k in ('world','actor','entity','sequence','item','trigger'))]
  gone=[r for r in ev('contact_lifetime_destroy_end') if r['world']==w['world'] and r['entity']==w['entity']]
  selected=[r for r in selections if (r['world'],r['entity'],r['sequence'],r['item'],r['trigger'])==(w['world'],w['actor'],w['sequence'],w['item'],w['trigger'])]
  valid=len(matches)==len(gone)==1 and bool(selected)
  if valid:
   r=matches[0];idle=[f for f in fences if f['world']==r['world'] and f['actor']==r['actor'] and f['tick']==r['tick'] and not f['active'] and f['quiet'] and f['us']<r['us']]
   original=[a for a in appends if (a['world'],a['entity'],a['trigger'])==(r['world'],r['actor'],r['trigger'])]
   active=[f for f in fences if f['active'] and f['world']==r['world'] and f['actor']==r['actor'] and f['trigger']==r['trigger']]
   queries=[q for q in ready if q.get('entity')==w['entity'] and q.get('owner_lookup',{}).get('world')==w['world']]
   valid=bool(idle) and bool(active) and bool(queries) and len(original)==1 and r['us']>gone[0]['us'] and all(s['us']<r['us'] for s in selected) and all(q['us']<r['us'] for q in queries)
   valid=valid and original[0]['call']==r['call'] and original[0]['sub']==r['sub'] and all((bytes.fromhex(f['actor_bytes'])[20],bytes.fromhex(f['actor_bytes'])[0x170])==(r['call'],r['sub']) for f in active)
   timeline.append(dict(entity=r['entity'],trigger=r['trigger'],retired_tick=r['tick'],retired_after_destroy_us=r['us']-gone[0]['us'],retired_after_last_selection_us=r['us']-max(s['us'] for s in selected)))
  c['retire_after_native_completion_'+str(w['entity'])]=bool(valid)
 c['slot_reused_between_two_appends']=len(appends)==len(retired)==2 and appends[0]['us']<retired[0]['us']<appends[1]['us']<retired[1]['us']
 m['recycling']=timeline
 return c,m

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('probe',type=Path);p.add_argument('--negative-tests',action='store_true');a=p.parse_args()
 if (a.probe/'world-contact-recycle.txt').read_text().strip()!='EVR_CONTACT_RECYCLE_1 1':raise ValueError('explicit finite one-slot policy required')
 source=a.probe/'view-resource-probe.jsonl';digest=hashlib.sha256(source.read_bytes()).hexdigest();proof=json.loads((a.probe/'contact-lifetime-checks.json').read_text())
 if proof['source_sha256']!=digest or set(proof['checks'].values())!={'PASS'}:raise ValueError('exact full native contact lifetime proof must PASS first')
 def unique(pairs):
  d={}
  for k,v in pairs:
   if k in d:raise ValueError('duplicate JSON key '+k)
   d[k]=v
  return d
 rows=[json.loads(l,object_pairs_hook=unique) for l in source.read_text().splitlines()];c,m=audit(rows)
 if a.negative_tests and all(c.values()):
  mutants=[('multiple_slots','contact_recycle_enabled','event_slots',8),('uncleared_event_slot','contact_recycled','events_after',1),('uncleared_collider_slot','contact_recycled','colliders_after',1),('early_retire','contact_recycled','us',0),('wrong_actor','contact_recycled','actor',0),('wrong_trigger','contact_recycled','trigger',-1),('gameplay_write','contact_recycled','writes_gameplay',True),('World_discontinuity','contact_recycle_removed','continuity_fault',True),('unfinished_query','contact_quiescence_sample','query_scopes',1),('no_retirement_declared','contact_quiescence_enabled','retires_ledger_entries',False)]
  for name,event,field,value in mutants:
   bad=[dict(r,**{field:value}) if r['event']==event else r for r in rows];c['reject_'+name]=not all(audit(bad)[0].values())
  c['reject_missing_recycle']=not all(audit([r for r in rows if r['event']!='contact_recycled'])[0].values())
 else:c['negative_baseline_qualified']=not a.negative_tests
 c['full_contact_lifetime_proof_PASS']=True
 report=dict(checks={k:'PASS' if v else 'FAIL' for k,v in c.items()},metrics=m,source_sha256=digest,limitation=__doc__)
 (a.probe/'contact-recycling-checks.json').write_text(json.dumps(report,indent=2)+'\n')
 for k,v in c.items():print('PASS' if v else 'FAIL',k)
 print('PASS',sum(bool(v) for v in c.values()),'FAIL',sum(not v for v in c.values()));print(json.dumps(m,indent=2));return int(not all(c.values()))
if __name__=='__main__':raise SystemExit(main())
