"""Verify serial-tagged reusable watch storage in a finite native trial.
Requires the existing exact-source retirement proof. Does not prove World epochs,
unlimited GPU play, live enemy damage or mixed cancellation in one native session.
"""
import argparse,hashlib,json
from pathlib import Path

def audit(rows,blocked=False):
 ev=lambda name:[r for r in rows if r['event']==name]
 watches=ev('blocked_action_lifetime_watch' if blocked else 'contact_lifetime_watch')
 retired=ev('blocked_event_recycled' if blocked else 'contact_recycled')
 ends=ev('blocked_event_recycle_removed' if blocked else 'contact_recycle_removed')
 key=lambda r:(r.get('watch_slot',-1),r.get('watch_serial',0))
 expected=1 if blocked else 2
 c=dict(expected_lifetimes=len(watches)==len(retired)==expected,
        one_reused_watch_slot=bool(watches) and all(r.get('watch_slot',-1)==0 for r in watches),
        monotonic_distinct_serials=[r.get('watch_serial',0) for r in watches]==list(range(1,expected+1)),
        no_watch_leak=bool(retired) and all(r.get('watch_slots_remaining',-1)==0 for r in retired) and len(ends)==1 and ends[0].get('watch_slots_remaining',-1)==0)
 valid=bool(watches)
 for index,w in enumerate(watches):
  matches=[r for r in retired if key(r)==key(w)]
  ok=len(matches)==1
  if ok:
   r=matches[0];ok=all(r[k]==w[k] for k in ('world','actor','sequence','item','trigger')) and w['us']<r['us']
   if index+1<len(watches):ok &= r['us']<watches[index+1]['us']
   if not blocked:
    begin=[d for d in ev('contact_lifetime_destroy_begin') if key(d)==key(w)]
    end=[d for d in ev('contact_lifetime_destroy_end') if key(d)==key(w)]
    ok &= len(begin)==len(end)==1
    if begin and end:ok &= w['us']<begin[0]['us']<end[0]['us']<r['us'] and all(d['world']==w['world'] and d['entity']==w['entity'] for d in begin+end+[r])
  valid &= bool(ok)
 c['exact_token_owner_order_and_single_completion']=bool(valid)
 return c

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('probe',type=Path);p.add_argument('--blocked',action='store_true');p.add_argument('--negative-tests',action='store_true');a=p.parse_args()
 source=a.probe/'view-resource-probe.jsonl';digest=hashlib.sha256(source.read_bytes()).hexdigest()
 prerequisite='blocked-event-retirement-checks.json' if a.blocked else 'contact-recycling-checks.json'
 proof=json.loads((a.probe/prerequisite).read_text())
 if proof['source_sha256']!=digest or not proof['checks'] or set(proof['checks'].values())!={'PASS'}:raise ValueError('exact-source full retirement proof required first')
 def unique(pairs):
  d={}
  for k,v in pairs:
   if k in d:raise ValueError('duplicate JSON key '+k)
   d[k]=v
  return d
 rows=[json.loads(l,object_pairs_hook=unique) for l in source.read_text().splitlines()];c=audit(rows,a.blocked)
 if a.negative_tests and all(c.values()):
  watch='blocked_action_lifetime_watch' if a.blocked else 'contact_lifetime_watch';retire='blocked_event_recycled' if a.blocked else 'contact_recycled'
  for label,event,field,value in [('zero_serial',watch,'watch_serial',0),('wrong_slot',retire,'watch_slot',1),('stale_completion',retire,'watch_serial',99),('unfinished_watch',retire,'watch_slots_remaining',1),('early_reuse',retire,'us',0),('wrong_owner',retire,'actor',0)]:
   bad=[dict(r,**{field:value}) if r['event']==event else r for r in rows];c['reject_'+label]=not all(audit(bad,a.blocked).values())
  c['reject_duplicate_completion']=not all(audit(rows+[next(r for r in rows if r['event']==retire)],a.blocked).values())
  if not a.blocked:
   bad=[dict(r,watch_serial=1) if r['event'] in (watch,retire,'contact_lifetime_destroy_begin','contact_lifetime_destroy_end') else r for r in rows];c['reject_ABA_serial_reused']=not all(audit(bad).values())
 else:c['negative_baseline_qualified']=not a.negative_tests
 c['exact_source_retirement_proof_PASS']=True
 result=dict(checks={k:'PASS' if v else 'FAIL' for k,v in c.items()},source_sha256=digest,prerequisite=prerequisite,limitation=__doc__)
 (a.probe/'contact-pool-checks.json').write_text(json.dumps(result,indent=2)+'\n')
 for k,v in c.items():print('PASS' if v else 'FAIL',k)
 print('PASS',sum(bool(v) for v in c.values()),'FAIL',sum(not v for v in c.values()));return int(not all(c.values()))
if __name__=='__main__':raise SystemExit(main())
