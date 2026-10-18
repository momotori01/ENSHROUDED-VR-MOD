"""Explicit finite trial: regular logs stop at 3/2, independent test witness stream proves processing continues.
The witness stream remains verbose. This is NOT a zero-logging or unlimited-session performance test.
"""
import argparse,copy,hashlib,json
from pathlib import Path
from check_melee_contact import audit as contact_audit
from generate_melee_repeat_fixture import verify

PARTITIONS=[('motion_event_selection','motion_event_selection_budget_witness','motion_event_recording_summary',3),('sequence_collider_ready_sample','sequence_collider_ready_budget_witness','collider_ready_recording_summary',2)]

def partition(rows):
 ev=lambda n:[r for r in rows if r['event']==n]
 enabled=ev('contact_recording_budget_enabled');checks={'explicit_test_witness_mode':len(enabled)==1 and all(enabled[0].get(k)==v for k,v in dict(event_limit=3,ready_limit=2,test_witness_stream=True,processing_unlimited_by_recording=True).items())};metrics={}
 for regular,witness,summary,limit in PARTITIONS:
  a=ev(regular);b=ev(witness);combined=[r for r in rows if r['event'] in (regular,witness)];done=ev(summary);total=len(combined)
  checks[regular+'_bounded_regular']=[r['sample'] for r in a]==list(range(1,limit+1))
  checks[regular+'_complete_witness']=[r['sample'] for r in b]==list(range(limit+1,total+1)) and len(b)>limit
  checks[regular+'_ordered_partition']=[r['sample'] for r in combined]==list(range(1,total+1)) and len({r['sample'] for r in combined})==total
  checks[regular+'_summary']=len(done)==1 and all(done[0].get(k)==v for k,v in dict(processed=total,recorded=limit,witness=total-limit,omitted=0,limit=limit).items())
  metrics[regular]=dict(regular=len(a),witness=len(b),total=total)
 commands=ev('melee_session_command');issues=[r for r in commands if r['issued']]
 if len(issues)==2:
  for regular,witness,_,_ in PARTITIONS:
   checks[regular+'_second_attack_only_witness']=not any(r['us']>issues[1]['us'] for r in ev(regular)) and any(r['us']>issues[1]['us'] for r in ev(witness))
 else:checks['two_measured_requests']=False
 selections=ev('motion_event_selection');retimes=ev('motion_event_retime')
 checks['late_suppression_and_second_append_after_regular_limit']=len(selections)==3 and len(retimes)==4 and [r['operation'] for r in retimes if r['us']>selections[-1]['us']]==['suppress','append','suppress']
 ready=ev('sequence_collider_ready_sample')+ev('sequence_collider_ready_budget_witness');begin=ev('motion_collider_apply_begin');end=ev('motion_collider_apply_end')
 checks['all_ready_scopes_processed']=len(ready)>2 and sorted(r['sample'] for r in ready)==sorted(r['ready_sample'] for r in begin)==sorted(r['ready_sample'] for r in end)
 checks['no_recording_failures']=not any('failure' in r['event'] or 'failed' in r['event'] for r in rows)
 return checks,metrics

def audit(native,sequence,xr,mock,profile,tolerance):
 c,m=partition(native)
 # Original files/events remain untouched. Only a complete, validated partition
 # is admitted into the existing independent native contact audit.
 if not all(c.values()):return c,m
 rename={w:r for r,w,_,_ in PARTITIONS};normalized=[dict(r,event=rename.get(r['event'],r['event'])) for r in native]
 cc,cm=contact_audit(normalized,sequence,xr,mock,profile,tolerance);c.update({'contact_'+k:v for k,v in cc.items()});m['contact']=cm
 return c,m

def negatives(native,sequence,xr,mock,profile,tolerance):
 baseline,_=audit(native,sequence,xr,mock,profile,tolerance)
 if not all(baseline.values()):raise ValueError('passing baseline required before negative controls')
 checks={}
 for mode in ('missing_witness','regular_over_budget','duplicate_ordinal','wrong_summary','duplicate_Impact','wrong_physics','source_write','lost_authority','live_retired_ticket','asset_write'):
  bad=copy.deepcopy(native)
  witness=next(r for r in bad if r['event']=='motion_event_selection_budget_witness')
  if mode=='missing_witness':bad.remove(witness)
  elif mode=='regular_over_budget':witness['event']='motion_event_selection'
  elif mode=='duplicate_ordinal':witness['sample']=3
  elif mode=='wrong_summary':next(r for r in bad if r['event']=='collider_ready_recording_summary')['processed']+=1
  else:
   event,field,value={'duplicate_Impact':('motion_event_retime_removed','appended',3),'wrong_physics':('sequence_collider_sample','physics_current_bytes','00'*32),'source_write':('motion_collider_apply_end','original_components_unchanged',False),'lost_authority':('motion_collider_shadow','owned_intent',False),'live_retired_ticket':('melee_ticket_retired','valid',True),'asset_write':('motion_event_retime','asset_written',True)}[mode]
   for row in bad:
    if row['event']==event:row[field]=value
  checks['reject_'+mode]=not all(audit(bad,sequence,xr,mock,profile,tolerance)[0].values())
 return checks

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('probe',type=Path);p.add_argument('--negative-tests',action='store_true');a=p.parse_args();probe=a.probe
 if (probe/'world-contact-log-budget.txt').read_text().strip()!='EVR_CONTACT_LOG_BUDGET_1 3 2':raise ValueError('explicit test policy required')
 if (probe/'world-melee-session-contact.txt').read_text().strip()!='EVR_MELEE_SESSION_CONTACT_2':raise ValueError('measured session contact required')
 read=lambda path:[json.loads(l) for l in path.read_text(encoding='utf-8-sig').splitlines()]
 source=probe/'view-resource-probe.jsonl';native=read(source);xr=read(probe.parent/'native-xr/openxr.jsonl');mock=read(probe.parent/'mock.jsonl');profile=json.loads((probe/'grip-preview.json').read_text());verify(probe.parent/'mock-hands.csv',profile)
 sequence=[[float(v) for v in l.split(',')] for l in (probe.parent/'mock-hands.csv').read_text().splitlines() if l and not l.startswith('#')];tolerance=float((probe/'world-two-hand-attack.txt').read_text().split()[1]);c,m=audit(native,sequence,xr,mock,profile,tolerance)
 if a.negative_tests and all(c.values()):c.update(negatives(native,sequence,xr,mock,profile,tolerance))
 elif a.negative_tests:c['negative_controls_baseline_qualified']=False
 c['runner_PASS']=json.loads((probe/'probe-status.json').read_text())['status']=='PASS';c['Core_Sync_clean']=(probe.parent/'validation.log').stat().st_size==0;c['normal_exit']=json.loads((probe.parent/'process-exit.json').read_text(encoding='utf-8-sig'))['exit_code']==0
 report=dict(checks={k:'PASS' if v else 'FAIL' for k,v in c.items()},metrics=m,source_sha256=hashlib.sha256(source.read_bytes()).hexdigest(),limitation=__doc__);(probe/'contact-log-budget-checks.json').write_text(json.dumps(report,indent=2)+'\n')
 for k,v in c.items():print('PASS' if v else 'FAIL',k)
 print('PASS',sum(bool(v) for v in c.values()),'FAIL',sum(not v for v in c.values()),'enemy_HP_proven',m.get('contact',{}).get('live_target_damage_proven'));return int(not all(c.values()))
if __name__=='__main__':raise SystemExit(main())
