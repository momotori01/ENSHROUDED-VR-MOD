"""Replay policy changes in private copies; never rewrite native evidence."""
import argparse,hashlib,json
from pathlib import Path
import check_joined_contact_owner as joined
from check_active_contact_owner import audit

def main():
 p=argparse.ArgumentParser();p.add_argument('probe',type=Path);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
 raw,rows,source,allowed,*_=joined.load(a.probe);actors,_=joined.validate_callers(rows)
 def run(rr):return audit(source,rr,allowed,actors,[],[],[],[],{},visuals=False,interrupted=True)[0]
 original=run(rows);checks={'original_native_checks_pass':all(original.values())}
 b=next(i for i,r in enumerate(rows) if r['event']=='contact_actor_native_boundary')
 held=list(rows);held[b]=dict(rows[b],value_hold_policy=2,global_drain_required_before_retire=True,destructor_callbacks=2)
 checks['another_destructor_does_not_erase_or_invalidate_hold']=all(run(held).values())
 for label,event,change,key in [('no_owner','contact_actor_native_boundary',{'destructor_callbacks':0},'interrupted_owner_value_hold_boundary'),('live_query','contact_actor_native_boundary',{'query_scopes':1},'interrupted_owner_value_hold_boundary'),('no_final_drain','contact_actor_native_boundary',{'global_drain_required_before_retire':False},'interrupted_owner_value_hold_boundary'),('final_destructor_busy','contact_lifetime_removed',{'callbacks':1},'quiescent_contact_lifetime_removed'),('early_erasure','contact_terminal_recycled',{'us':0},'wait_then_actual_end_then_transaction'),('read_dead_actor','contact_actor_owner_destroy_end',{'reads_destroyed_actor':True},'actor_native_record_present_only_before_original')]:
  rr=list(held);i=next(i for i,r in enumerate(rr) if r['event']==event);rr[i]=dict(rr[i],**change);bad=run(rr)
  if key not in bad:raise ValueError('Missing named regression target: '+key)
  checks['reject_'+label]=bool(run(held).get(key)) and not bad[key]
 result=dict(status='PASS' if all(checks.values()) else 'FAIL',scope='CPU replay only, additional callback is synthetic; native raw unchanged',checks=checks,source=str(a.probe/'view-resource-probe.jsonl'),source_sha256=hashlib.sha256(raw).hexdigest())
 a.output.write_text(json.dumps(result,indent=2)+'\n')
 for k,v in checks.items():print('PASS' if v else 'FAIL',k)
 return int(not all(checks.values()))
if __name__=='__main__':raise SystemExit(main())
