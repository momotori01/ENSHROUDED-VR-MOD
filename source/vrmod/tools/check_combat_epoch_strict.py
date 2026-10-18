"""Finite native epoch enforcement transport proof; not teardown-race or re-entry proof."""
import argparse,copy,hashlib,json,pathlib
from combat_epoch_policy import published,IDENTITY
STAGES={'input','poll','ticket','pose','event','append','suppress','collider','flush','scope'}
def audit(source,rows,scenario="repeat"):
 expected=1 if scenario=="post-impact-cancel" else 2
 c={};ev=lambda name:[r for r in rows if r['event']==name]
 try:client,server=published(source,live=False);c['initial_native_pair']=True
 except (KeyError,ValueError):return {'initial_native_pair':False},{}
 exact=lambda a,b:all(a.get(k)==b.get(k) for k in IDENTITY)
 scoped=lambda r:r.get('epoch_provider')==server['provider'] and r.get('epoch_serial')==server['serial'] and r.get('epoch_kind')==0 and r.get('epoch_registry')==server['registry'] and r.get('epoch_root')==server['owner_root']
 pins=ev('combat_epoch_gate_pinned');enable=ev('combat_epoch_gate_enabled');checks=ev('combat_epoch_gate_check');done=ev('combat_epoch_gate_complete');released=ev('combat_epoch_gate_released')
 c['unique_exact_value_leases']=len(pins)==2 and all(exact(a,b) and a['handle']>0 and a['pins']==1 for a,b in zip(pins,(client,server))) and len({p['handle'] for p in pins})==2
 c['enforcement_without_ecs_pin_claim']=len(enable)==1 and enable[0].get('enforced') is True and enable[0].get('ecs_pinned') is False and enable[0].get('atomic_native_permission') is False and not ev('combat_epoch_shadow_enabled')
 c['every_gate_observed']=bool(checks) and {r['stage'] for r in checks}==STAGES
 c['samples_from_exact_live_roles']=bool(checks) and all(exact(r['client'],client) and exact(r['server'],server) and all(r[role]['phase']==r[role]['flags']==r[role]['pins']==1 for role in ('client','server')) and r['input_valid'] and r['contact_valid'] and r['enforced'] for r in checks)
 c['native_observed_roles_match_or_rejected']=all(((int(r['observed_client'],16)==0 or r['observed_client']==client['world']) and (int(r['observed_server'],16)==0 or r['observed_server']==server['world'])) or r['allowed'] is False for r in checks)
 c['other_world_flush_rejected']=any(r['stage']=='flush' and r['observed_server']!=server['world'] and r['allowed'] is False for r in checks)
 c['all_stages_admitted_valid_work']=all(any(r['stage']==stage and r['allowed'] for r in checks) for stage in STAGES)
 tickets=ev('native_attack_ticket');events=ev('motion_event_retime');colliders=ev('motion_collider_apply_begin');watches=ev('contact_lifetime_watch');retired=ev('contact_recycled');fences=ev('contact_quiescence_sample')
 c['epoch_bound_tickets']=len(tickets)==expected and all(scoped(r) and r.get('client_epoch_serial')==client['serial'] and r.get('client_epoch_world')==client['world'] for r in tickets)
 c['exact_consumed_events_keep_server_epoch']=len(events)==2*expected and [r['operation'] for r in events].count('append')==expected and [r['operation'] for r in events].count('suppress')==expected and all(scoped(r) and r['actor_and_storage_unchanged'] for r in events)
 if scenario=='post-impact-cancel':
  cancel=ev('motion_collider_cancel_begin');aborts=ev('motion_offhand_release_abort')
  c['cancel_keeps_owned_epoch']=bool(cancel) and all(scoped(r) and not r['forced_fixture'] and r['latched'] for r in cancel)
  c['genuine_offhand_abort']=len(aborts)==1 and aborts[0]['expected'] and aborts[0]['ticket_revoked'] and aborts[0]['pulse_released']
 c['collider_queries_use_same_server_epoch']=bool(colliders) and all(scoped(r) for r in colliders)
 c['watch_and_fence_retire_same_epoch']=len(watches)==len(retired)==expected and all(scoped(r) for r in watches+retired+fences) and bool(fences) and all(r.get('watch_epoch_serial')==server['serial'] and r['events_after']==r['colliders_after']==0 for r in retired)
 c['all_roles_released_after_native_hooks']=len(released)==2 and len(done)==1 and all(exact(r,p) and r['handle']==p['handle'] and r['exact_release_succeeded'] and r['saved_request_metadata'] for r,p in zip(released,pins)) and done[0]['retained']==0 and not done[0]['faulted'] and all(done[0]['checks'].get(stage,0)>0 for stage in STAGES)
 complete=ev('view_resource_hook_complete');closing=[r for r in source if r['event']=='world_broker_closing'];stop=[r for r in source if r['event']=='world_broker_stopped']
 c['probe_then_normal_world_shutdown']=len(complete)==len(done)==1 and complete[0]['result']==0 and len(closing)==3 and done[0]['us']<complete[0]['us']<min(r['us'] for r in closing)
 c['persistent_provider_drained']=len(stop)==1 and all(stop[0][k]==0 for k in ('worlds','leases','closures','flags'))
 c['no_unexpected_epoch_fault_or_cancel']=not ev('controller_attack_epoch_stopped') and not ev('motion_event_epoch_cancelled') and not ev('view_resource_hook_failure')
 return c,dict(gates=done[0]['checks'] if done else {},denied=done[0]['denied'] if done else {},collider_queries=len(colliders),scenario=scenario,scope=__doc__)

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('publisher',type=pathlib.Path);p.add_argument('probe',type=pathlib.Path);p.add_argument('--negative-tests',action='store_true');p.add_argument('--scenario',choices=('repeat','post-impact-cancel'),default='repeat');a=p.parse_args()
 read=lambda path:[json.loads(x) for x in path.read_text(encoding='utf-8-sig').splitlines()]
 source=read(a.publisher/'view-resource-probe.jsonl');keep={'native_attack_ticket','motion_event_retime','motion_event_epoch_cancelled','controller_attack_epoch_stopped','motion_collider_apply_begin','contact_lifetime_watch','contact_recycled','contact_quiescence_sample','view_resource_hook_complete','view_resource_hook_failure','motion_collider_cancel_begin','motion_offhand_release_abort'}
 rows=[r for r in read(a.probe/'view-resource-probe.jsonl') if r['event'].startswith('combat_epoch_') or r['event'] in keep];c,m=audit(source,rows,a.scenario)
 provenance=json.loads((a.probe/'combat-epoch-provenance.json').read_text());launch=json.loads((a.probe.parent/'launch.json').read_text(encoding='utf-8-sig'))
 c['exact_scenario_policy']=((a.probe/'world-cancel-contact-recycle.txt').exists() if a.scenario=='post-impact-cancel' else (a.probe/'world-melee-session-contact.txt').exists())
 c['exact_live_publisher_prefix']=a.publisher.resolve().parent==a.probe.resolve().parent and pathlib.Path(provenance['publisher']).resolve()==a.publisher.resolve() and provenance['pid']==launch['pid'] and provenance['creation_filetime']==launch['creation_filetime'] and provenance['shadow_only'] is False and hashlib.sha256((a.publisher/'view-resource-probe.jsonl').read_bytes()[:provenance['source_prefix_bytes']]).hexdigest()==provenance['source_prefix_sha256']
 status=json.loads((a.probe/'probe-status.json').read_text());exit_data=json.loads((a.probe.parent/'process-exit.json').read_text(encoding='utf-8-sig'));c['normal_exit_and_survival']=status['status']=='PASS' and exit_data['exit_code']==0 and (a.probe.parent/'validation.log').stat().st_size==0
 if a.negative_tests:
  mutations=[('foreign_world_allowed','combat_epoch_gate_check',{'observed_server':'0x1','allowed':True}),('ticket_epoch','native_attack_ticket',{'epoch_serial':999}),('ticket_client','native_attack_ticket',{'client_epoch_serial':999}),('append_epoch','motion_event_retime',{'epoch_serial':999}),('query_epoch','motion_collider_apply_begin',{'epoch_serial':999}),('watch_epoch','contact_lifetime_watch',{'epoch_serial':999}),('retire_boundary','contact_recycled',{'watch_epoch_serial':999}),('fence_epoch','contact_quiescence_sample',{'epoch_serial':999}),('lease_leak','combat_epoch_gate_complete',{'retained':1}),('fault_ignored','combat_epoch_gate_complete',{'faulted':True}),('pin_claim','combat_epoch_gate_enabled',{'ecs_pinned':True}),('missing_gate','combat_epoch_gate_check',{'stage':'unknown'}),('early_shutdown','combat_epoch_gate_complete',{'us':10**30})]
  targets={'foreign_world_allowed':'native_observed_roles_match_or_rejected','ticket_epoch':'epoch_bound_tickets','ticket_client':'epoch_bound_tickets','append_epoch':'exact_consumed_events_keep_server_epoch','query_epoch':'collider_queries_use_same_server_epoch','watch_epoch':'watch_and_fence_retire_same_epoch','retire_boundary':'watch_and_fence_retire_same_epoch','fence_epoch':'watch_and_fence_retire_same_epoch','lease_leak':'all_roles_released_after_native_hooks','fault_ignored':'all_roles_released_after_native_hooks','pin_claim':'enforcement_without_ecs_pin_claim','missing_gate':'every_gate_observed','early_shutdown':'probe_then_normal_world_shutdown'}
  if a.scenario=='post-impact-cancel':
   mutations.append(('cancel_epoch','motion_collider_cancel_begin',{'epoch_serial':999}));targets['cancel_epoch']='cancel_keeps_owned_epoch'
  for label,event,change in mutations:
   bad=copy.deepcopy(rows);next(r for r in bad if r['event']==event).update(**change)
   try:
    target=targets[label]
    # A pre-existing unrelated failure cannot make a mutation test pass.
    c['negative_'+label]=bool(c.get(target)) and audit(source,bad,a.scenario)[0].get(target) is False
   except (KeyError,ValueError,TypeError):c['negative_'+label]=False
 report=dict(status='PASS' if all(c.values()) else 'FAIL',checks=c,metrics=m,source_sha256=hashlib.sha256((a.probe/'view-resource-probe.jsonl').read_bytes()).hexdigest());(a.probe/'combat-epoch-strict-checks.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
 for k,v in c.items():print('PASS' if v else 'FAIL',k)
 print(m);return int(not all(c.values()))
if __name__=='__main__':raise SystemExit(main())
