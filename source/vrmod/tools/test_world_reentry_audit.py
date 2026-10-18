"""Synthetic two-cycle regression from a saved native cycle, never native proof."""
import argparse,copy,json
from pathlib import Path
from check_world_lifetimes import audit
p=argparse.ArgumentParser();p.add_argument('source',type=Path);a=p.parse_args()
rows=[json.loads(l) for l in (a.source/'view-resource-probe.jsonl').read_text().splitlines()]
base=audit(rows,'epoch')[0]
if not all(base.values()):raise ValueError('Base native data invalid: '+str([k for k,v in base.items() if not v]))
span=max(r['us'] for r in rows)-min(r['us'] for r in rows)+1000000
max_call=max(r.get('call_id',0) for r in rows)
native=[r for r in rows if 'call_id' in r]
first=copy.deepcopy(native);second=copy.deepcopy(native)
for r in second:
 r['us']+=span
 if r.get('call_id'):r['call_id']+=max_call
 if r.get('parent_call_id'):r['parent_call_id']+=max_call
 if 'epoch_serial' in r:r['epoch_serial']+=3
 if 'serial' in r:r['serial']+=3
whole=first+second
for r in rows:
 if 'call_id' in r:continue
 if r['event'] not in ('world_lifecycle_hook','world_epoch_shadow_enabled','world_lifecycle_observation_begin','world_lifecycle_removed','world_lifecycle_observation_complete'):continue
 r=copy.deepcopy(r)
 if r['event'] in ('world_lifecycle_removed','world_lifecycle_observation_complete'):r['us']+=span
 if r['event']=='world_lifecycle_observation_begin':r.update(reentry_trial=True,expected_generations=2,xr_trial=False,seconds=420)
 whole.append(r)
last_retired=max(r['us'] for r in first if r['event']=='world_epoch_retired')
whole.append(dict(event='world_reentry_gap',us=last_retired+1,created=3,closed_roots=2,epochs_remaining=0,broker_drained=True,provider_retained=True,serials_reset=False,native_roots_returned=True,claims_VR_reentry=False))
bc=[r for r in first+second if r['event']=='world_broker_created']
for old in bc[:3]:
 for new in bc[3:]:whole.append(dict(event='world_reentry_stale_token',us=new['us']+1,old_provider=old['provider'],new_provider=new['provider'],old_serial=old['serial'],new_serial=new['serial'],old_slot=old['slot'],old_world=old['world'],new_world=new['world'],status=6,reads_native_memory=False,serials_reset=False))
whole.sort(key=lambda r:r['us']);positive=audit(whole,'reentry')[0]
if not all(positive.values()):raise ValueError('Synthetic replay invalid: '+str([k for k,v in positive.items() if not v]))
checks={'original_one_generation':all(base.values()),'single_cycle_not_reentry':not all(audit(rows,'reentry')[0].values()),'synthetic_same_addresses_new_serials':all(positive.values())}
for label,event,change,key in [('accepted_old_token','world_reentry_stale_token',{'status':0},'old_tokens_rejected_after_each_new_world'),('native_read','world_reentry_stale_token',{'reads_native_memory':True},'old_tokens_rejected_after_each_new_world'),('provider_restart','world_reentry_stale_token',{'new_provider':999},'old_tokens_rejected_after_each_new_world'),('first_not_drained','world_reentry_gap',{'broker_drained':False},'first_generation_drained_without_reset'),('reset_serials','world_reentry_gap',{'serials_reset':True},'first_generation_drained_without_reset'),('late_gap','world_reentry_gap',{'us':10**30},'gap_between_real_native_generations'),('too_many_generations','world_lifecycle_observation_begin',{'expected_generations':3},'explicit_two_generation_policy')]:
 bad=copy.deepcopy(whole);next(r for r in bad if r['event']==event).update(**change);checks['reject_'+label]=not audit(bad,'reentry')[0][key]
for label,events in [('missing_detach',{'world_lifecycle_removed'}),('missing_completion',{'world_lifecycle_observation_complete'}),('partial_single_cycle',{'world_lifecycle_removed','world_lifecycle_observation_complete','world_reentry_stale_token'})]:
 bad=[copy.deepcopy(r) for r in (whole if label!='partial_single_cycle' else first) if r['event'] not in events]
 checks['reject_'+label]=not all(audit(bad,'reentry')[0].values())
for k,v in checks.items():print('PASS' if v else 'FAIL',k)
raise SystemExit(int(not all(checks.values())))
