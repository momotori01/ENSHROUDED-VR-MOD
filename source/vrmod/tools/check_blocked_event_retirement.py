"""Finite pre-Impact offhand cancellation with native event-only ledger retirement.
Requires independent hand/weapon/cancellation and native stereo checks.
This is not a World recreation, unlimited-play or actual-enemy-damage proof.
"""
import argparse,hashlib,json,struct
from pathlib import Path

def audit(rows):
 ev=lambda n:[r for r in rows if r['event']==n]
 watch=ev('blocked_action_lifetime_watch');blocked=ev('motion_event_blocked');note=ev('blocked_action_lifetime_suppressed');supp=[r for r in ev('motion_event_retime') if r['operation']=='suppress'];fences=ev('contact_quiescence_sample');gone=ev('blocked_event_recycled');end=ev('blocked_event_recycle_removed');hook=ev('contact_quiescence_removed');enable=ev('blocked_event_recycle_enabled')
 c=dict(explicit_single_event_finite_trial=len(enable)==1 and enable[0]['event_slots']==enable[0]['expected_actions']==1 and enable[0]['expected_Impacts']==0 and enable[0]['finite_trial'] and enable[0]['writes_gameplay'] is False and enable[0]['changes_mod_ledgers'],
        one_exact_block_suppress_retire=len(watch)==len(blocked)==len(note)==len(supp)==len(gone)==1,
        no_append_or_unexpected_Impact=not [r for r in ev('motion_event_retime') if r['operation']=='append'] and not ev('blocked_action_unexpected_collider') and not ev('motion_collider_apply_begin') and not ev('motion_collider_cancel_begin'),
        no_evidence_failure=not ev('blocked_action_lifetime_failure') and not ev('contact_quiescence_failure'),
        hook_restored_drained=len(hook)==1 and hook[0]['restored'] and hook[0]['callbacks']==hook[0]['failures']==hook[0]['omitted']==0 and hook[0]['retires_ledger_entries'] and hook[0]['samples']==len(fences)>10,
        event_entry_empty_at_shutdown=len(end)==1 and end[0]['registrations']==end[0]['suppressions']==end[0]['retired']==1 and end[0]['matching_colliders']==end[0]['failures']==end[0]['events_remaining']==0 and not end[0]['continuity_fault'] and end[0]['writes_gameplay'] is False and end[0]['changes_mod_ledgers'])
 qe=ev('contact_quiescence_enabled');recording=ev('motion_event_recording_summary')
 c['exact_native_flush_hook']=len(qe)==1 and qe[0]['rva']=='0x931ad0' and qe[0]['caller']=='0x93bdf3' and qe[0]['read_only'] and qe[0]['retires_ledger_entries']
 c['all_native_selections_recorded']=len(recording)==1 and recording[0]['processed']==recording[0]['recorded']+recording[0]['witness'] and recording[0]['omitted']==0
 counts=('running','queued','completed_pending','command_bytes','command_count','selector_callbacks','query_scopes')
 raw=bool(fences);quiet=bool(fences)
 for r in fences:
  b=bytes.fromhex(r['actor_bytes']);raw &= len(b)==3104 and (bool(b[0x1b1]&1),struct.unpack_from('<I',b,0)[0],struct.unpack_from('<q',b,8)[0],struct.unpack_from('<I',b,0x38)[0])==(r['active'],r['sequence'],r['trigger'],r['item'])
  quiet &= r['quiet']==all(r[k]==0 for k in counts) and r['world_kind']==0 and r['watched']==r['destroyed']==0 and r['read_only'] and r['retires_ledger_entries']
 c['raw_Actor_independent_decode']=bool(raw);c['server_boundary_without_Collider_watch']=bool(quiet)
 c['scope_tick_counter_continuity']=bool(fences) and len({(r['world'],r['registry'],r['task_list'],r['actor']) for r in fences})==1 and all(a['tick']<=b['tick'] and a['entity_counter']<=b['entity_counter'] for a,b in zip(fences,fences[1:]))
 valid=False;m={}
 if c['one_exact_block_suppress_retire']:
  w,b,n,s,r=watch[0],blocked[0],note[0],supp[0],gone[0]
  same=lambda a,actor_name:all(a[k]==w[k] for k in ('world','item','sequence','trigger')) and a[actor_name]==w['actor']
  active=[f for f in fences if f['active']];idle=[f for f in fences if not f['active'] and f['quiet'] and f['tick']==r['tick'] and f['sequence']==f['item']==f['trigger']==0]
  selected=[x for x in rows if x['event'] in ('motion_event_selection','motion_event_selection_budget_witness') and x['world']==w['world'] and x['entity']==w['actor'] and x['trigger']==w['trigger']]
  valid=all(b[k]==w[k] for k in ('world','item','trigger')) and b['entity']==w['actor'] and same(s,'entity') and same(r,'actor') and n['world']==w['world'] and n['actor']==w['actor'] and n['trigger']==w['trigger'] and n['variant']==w['variant']==b['variant'] and w['blocked_ledger_verified']
  valid=valid and bool(active) and len(idle)==1 and bool(selected) and w['us']<b['us']<n['us']<s['us']<idle[0]['us']<r['us'] and all(x['us']<r['us'] for x in selected)
  valid=valid and all(same(f,'actor') and (bytes.fromhex(f['actor_bytes'])[20],bytes.fromhex(f['actor_bytes'])[0x170])==(w['call'],w['sub']) for f in active)
  valid=valid and r['events_before']==1 and r['events_after']==0 and r['suppressions']==1 and r['matching_colliders']==0 and r['actor_cleared'] and r['quiet'] and r['writes_gameplay'] is False and r['changes_mod_ledgers']
  m=dict(active_fences=len(active),retired_tick=r['tick'],after_suppression_us=r['us']-s['us'],after_last_selection_us=r['us']-max((x['us'] for x in selected),default=r['us']))
 c['full_identity_and_native_end_before_retirement']=bool(valid)
 return c,m

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('probe',type=Path);p.add_argument('--negative-tests',action='store_true');a=p.parse_args()
 if (a.probe/'world-blocked-event-recycle.txt').read_text().strip()!='EVR_BLOCKED_EVENT_RECYCLE_1':raise ValueError('explicit isolated event-only policy required')
 def unique(pairs):
  d={}
  for k,v in pairs:
   if k in d:raise ValueError('duplicate JSON field '+k)
   d[k]=v
  return d
 source=a.probe/'view-resource-probe.jsonl';rows=[json.loads(l,object_pairs_hook=unique) for l in source.read_text().splitlines()];c,m=audit(rows)
 if a.negative_tests and all(c.values()):
  for name,event,field,value in [('early_retire','blocked_event_recycled','us',0),('wrong_actor','blocked_event_recycled','actor',0),('wrong_trigger','blocked_event_recycled','trigger',0),('entry_left','blocked_event_recycled','events_after',1),('unexpected_Impact','blocked_event_recycled','matching_colliders',1),('missing_suppression','blocked_event_recycled','suppressions',0),('unfinished_query','contact_quiescence_sample','query_scopes',1),('raw_active','contact_quiescence_sample','active',True),('hook_left','contact_quiescence_removed','restored',False),('World_discontinuity','blocked_event_recycle_removed','continuity_fault',True)]:
   bad=[dict(r,**{field:value}) if r['event']==event else r for r in rows];c['reject_'+name]=not all(audit(bad)[0].values())
 else:c['negative_baseline_qualified']=not a.negative_tests
 for name in ('offhand-release-checks.json','motion-hand-checks.json','motion-visual-checks.json','live-native-checks-fixed-root-motion.json'):
  report=json.loads((a.probe/name).read_text());c[name+'_PASS']=bool(report['checks']) and set(report['checks'].values())=={'PASS'}
 c['native_runner_PASS']=json.loads((a.probe/'probe-status.json').read_text())['status']=='PASS'
 c['normal_exit']=json.loads((a.probe.parent/'process-exit.json').read_text(encoding='utf-8-sig'))['exit_code']==0
 c['Core_Sync_clean']=(a.probe.parent/'validation.log').stat().st_size==0
 report=dict(checks={k:'PASS' if v else 'FAIL' for k,v in c.items()},metrics=m,source_sha256=hashlib.sha256(source.read_bytes()).hexdigest(),limitation=__doc__)
 (a.probe/'blocked-event-retirement-checks.json').write_text(json.dumps(report,indent=2)+'\n')
 for k,v in c.items():print('PASS' if v else 'FAIL',k)
 print('PASS',sum(bool(v) for v in c.values()),'FAIL',sum(not v for v in c.values()));print(json.dumps(m,indent=2));return int(not all(c.values()))
if __name__=='__main__':raise SystemExit(main())
