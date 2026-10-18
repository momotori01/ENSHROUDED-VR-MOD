"""Finite native admission-stop/cancellation proof. Native Worlds remain alive;
this is NOT native World destruction, re-entry, indefinite play or enemy HP proof.
"""
import argparse,copy,hashlib,json
from pathlib import Path
from check_combat_epoch_strict import audit as strict_audit
from check_cancel_contact_retirement import audit as retire_audit

def audit(source,rows,allowed):
 c,m=strict_audit(source,rows,'post-impact-cancel')
 for key in ('samples_from_exact_live_roles','genuine_offhand_abort','no_unexpected_epoch_fault_or_cancel'):del c[key]
 rc,rm=retire_audit(rows,allowed);del rc['explicit_single_slot_cancel'];c.update({'retire_'+k:v for k,v in rc.items()});m.update(rm)
 ev=lambda name:[r for r in rows if r['event']==name]
 enables=ev('combat_epoch_cancel_trial_enabled');closes=ev('combat_epoch_admission_exercised');stops=ev('controller_world_epoch_stopped');cancels=ev('melee_epoch_cancelled')
 c['explicit_trial']=len(enables)==1 and enables[0]['expected_actions']==1 and enables[0]['late_swing_still_present'] and not enables[0]['native_world_destroyed']
 c['one_stop']=len(closes)==len(stops)==len(cancels)==1
 c['exact_closed_client_live_server']=c['one_stop'] and closes[0]['client']['flags']==5 and closes[0]['server']['flags']==1 and all(closes[0][r]['phase']==1 for r in ('client','server')) and closes[0]['quiet'] and closes[0]['active_action'] and closes[0]['applied_scopes']>=2 and not closes[0]['native_world_destroyed'] and not closes[0]['writes_gameplay']
 c['coordinator_cancelled_once']=c['one_stop'] and stops[0]['expected'] and stops[0]['state']==1 and stops[0]['ticket_revoked'] and stops[0]['ledgers_retained'] and cancels[0]['retired']==1 and cancels[0]['revoked'] and cancels[0]['reason']==6
 checks=ev('combat_epoch_gate_check')
 c['post_close_denies_new_work']=c['one_stop'] and any(r['stage'] in ('input','pose','event','append','collider') and not r['allowed'] and r['client']['flags']==5 for r in checks)
 c['no_admission_after_observed_close']=bool(checks) and all(not r['allowed'] or r['stage'] in ('suppress','flush','scope') or r['client']['flags']==r['server']['flags']==1 for r in checks)
 c['exact_roles_kept_across_stop']=bool(closes) and all(all(r[role][k]==closes[0][role][k] for role in ('client','server') for k in ('world','registry','owner_root','serial','provider','slot','kind')) for r in checks)
 packets=ev('controller_attack_packet_polled');restored=ev('controller_fixture_restored');release=ev('controller_attack_release_confirmed');disconnect=ev('controller_fixture_disconnected')
 c['neutral_consumed_before_disconnect']=len(release)==len(disconnect)==len(restored)==1 and release[0]['neutral_packet']==release[0]['observed_packet'] and release[0]['release_before_disconnect'] and release[0]['us']<disconnect[0]['us']<restored[0]['us'] and restored[0]['pointers_restored'] and restored[0]['native_disconnection_seen'] and restored[0]['native_connected']==restored[0]['rescan_pending']==0
 # A bounded attack pulse may already have released before admission closes.
 # Require evidence that the exact stopped neutral packet was consumed before
 # disconnect, and reject every later nonzero packet. Do not invent a new poll.
 c['no_late_nonzero_packet']=c['one_stop'] and len(disconnect)==1 and bool(packets) and any(r['packet']==stops[0]['neutral_packet'] and r['us']<disconnect[0]['us'] and not r['buttons'] and not r['right_trigger'] for r in packets) and all(not r['buttons'] and not r['right_trigger'] for r in packets if r['us']>stops[0]['us'])
 app=ev('motion_collider_apply_begin');cancel=ev('motion_collider_cancel_begin');sup=[r for r in ev('motion_event_retime') if r['operation']=='suppress'];retire=ev('contact_recycled')
 c['old_event_retained_until_retirement']=c['one_stop'] and len(sup)==len(retire)==1 and bool(cancel) and max(r['us'] for r in app)<closes[0]['us']<cancel[0]['us']<retire[0]['us'] and closes[0]['us']<sup[0]['us']<retire[0]['us']
 c['no_wrong_cancellation_reason']=not ev('motion_offhand_release_abort') and not ev('motion_tracking_loss_abort') and not ev('controller_fixture_failure')
 m.update(scope=__doc__);return c,m

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('publisher',type=Path);p.add_argument('probe',type=Path);p.add_argument('--negative-tests',action='store_true');a=p.parse_args()
 read=lambda path:[json.loads(x) for x in path.read_text(encoding='utf-8-sig').splitlines()]
 source=read(a.publisher/'view-resource-probe.jsonl');raw=(a.probe/'view-resource-probe.jsonl').read_bytes();rows=[json.loads(x) for x in raw.decode().splitlines()]
 graph=Path('docs/analysis/entity_lifetime_destroy/index.json');g=json.loads(graph.read_text())
 if g['exe_sha256']!=hashlib.sha256(Path('enshrouded.exe').read_bytes()).hexdigest():raise ValueError('native caller graph belongs to a different executable')
 allowed={int(e['site'],16)+5 for e in g['edges'] if e['target']=='0x8b83b0'}
 c,m=audit(source,rows,allowed)
 c['policy']=(a.probe/'world-combat-epoch-cancel.txt').read_text().strip()=='EVR_COMBAT_EPOCH_CANCEL_1'
 provenance=json.loads((a.probe/'combat-epoch-provenance.json').read_text());launch=json.loads((a.probe.parent/'launch.json').read_text(encoding='utf-8-sig'))
 c['exact_live_publisher_prefix']=a.publisher.resolve().parent==a.probe.resolve().parent and Path(provenance['publisher']).resolve()==a.publisher.resolve() and provenance['pid']==launch['pid'] and provenance['creation_filetime']==launch['creation_filetime'] and provenance['shadow_only'] is False and hashlib.sha256((a.publisher/'view-resource-probe.jsonl').read_bytes()[:provenance['source_prefix_bytes']]).hexdigest()==provenance['source_prefix_sha256']
 c['normal_exit_and_survival']=json.loads((a.probe/'probe-status.json').read_text())['status']=='PASS' and json.loads((a.probe.parent/'process-exit.json').read_text(encoding='utf-8-sig'))['exit_code']==0 and (a.probe.parent/'validation.log').stat().st_size==0
 if a.negative_tests:
  for label,event,change,target in [('wrong_reason','controller_world_epoch_stopped',{'state':2},'coordinator_cancelled_once'),('late_attack','controller_attack_packet_polled',{'buttons':16384,'us':10**29},'no_late_nonzero_packet'),('unconsumed_neutral','controller_attack_release_confirmed',{'observed_packet':999},'neutral_consumed_before_disconnect'),('native_destroyed','combat_epoch_admission_exercised',{'native_world_destroyed':True},'exact_closed_client_live_server'),('no_retirement','contact_recycle_removed',{'events_remaining':1},'retire_all_storage_empty'),('lease_leak','combat_epoch_gate_complete',{'retained':1},'all_roles_released_after_native_hooks'),('no_cancel','melee_epoch_cancelled',{'retired':0},'coordinator_cancelled_once')]:
   bad=copy.deepcopy(rows);next(r for r in bad if r['event']==event).update(change)
   c['negative_'+label]=bool(c.get(target)) and audit(source,bad,allowed)[0].get(target) is False
 report=dict(status='PASS' if all(c.values()) else 'FAIL',checks=c,metrics=m,source_sha256=hashlib.sha256(raw).hexdigest());(a.probe/'epoch-admission-cancel-checks.json').write_text(json.dumps(report,indent=2)+'\n')
 for k,v in c.items():print('PASS' if v else 'FAIL',k)
 print(m);return int(not all(c.values()))
if __name__=='__main__':raise SystemExit(main())
