"""Verify finite native input drain with World/UI reads skipped after admission
closure. Native Worlds remain Alive in this scenario; no backend-destruction,
actual World-exit combat, re-entry, or indefinite-play claim.
"""
import argparse,copy,hashlib,json
from pathlib import Path

def audit(rows):
 ev=lambda name:[r for r in rows if r['event']==name]
 stops=ev('controller_world_epoch_stopped');closed=ev('combat_epoch_admission_exercised');en=ev('combat_epoch_cancel_trial_enabled')
 checks={'one_explicit_admission_stop':len(stops)==len(closed)==len(en)==1 and stops[0]['expected'] and stops[0]['state']==1 and not closed[0]['native_world_destroyed']}
 at=stops[0]['us'] if len(stops)==1 else None
 ui=[r for r in ev('input_ui_native_sample') if at is not None and r['us']>at]
 fixed=ev('controller_fixture_restored');con=ev('controller_fixture_connected');removed=ev('input_ui_observer_removed');cancel=ev('melee_epoch_cancelled')
 checks['ui_skipped_before_backend_restore']=len(fixed)==1 and bool(ui) and ui[0]['us']<fixed[0]['us']
 checks['no_world_ui_read_after_stop']=bool(ui) and all(not r['readable'] and not r['stable_read'] and r['world']=='0x0' and r['sender_context']=='0x0' and r['entity']==0 and r['hud_screen']==255 and r['return_unchanged'] for r in ui)
 checks['frontend_native_thread_preserved']=len(con)==len(stops)==1 and con[0]['thread']==stops[0]['thread']
 checks['ui_observer_drained_without_read_fault']=len(removed)==1 and removed[0]['read_failures']==removed[0]['callbacks']==0 and removed[0]['restored']
 checks['owned_backend_restored_and_real_disconnect']=len(fixed)==1 and fixed[0]['pointers_restored'] and fixed[0]['native_connection_seen'] and fixed[0]['native_disconnection_seen'] and fixed[0]['disconnect_results']>0 and fixed[0]['native_connected']==fixed[0]['rescan_pending']==0
 checks['coordinator_cancelled_without_new_combat_reads']=len(cancel)==1 and cancel[0]['us']>=at and cancel[0]['revoked'] and cancel[0]['retired']==1 and not any(r['us']>at for r in ev('motion_attack_sample')) if at is not None else False
 applied=[r for r in rows if r['event'] in ('input_head_walking_sample','input_head_idle_sample') and r['applied']]
 checks['no_new_vr_aim_after_close']=bool(applied) and at is not None and all(r['us']<at for r in applied)
 checks['no_fixture_failure']=not ev('controller_fixture_failure') and not ev('input_head_walking_failure')
 return checks,at

def verify_provenance(probe):
 raw=(probe/'view-resource-probe.jsonl').read_bytes();r=json.loads((probe/'epoch-admission-cancel-checks.json').read_text())
 return (r['status']=='PASS' and bool(r['checks']) and all(v is True for v in r['checks'].values()) and r['source_sha256']==hashlib.sha256(raw).hexdigest() and
         (probe/'world-combat-epoch-cancel.txt').read_text().strip()=='EVR_COMBAT_EPOCH_CANCEL_1')

def validated_stop(probe,rows):
 checks,at=audit(rows)
 if not verify_provenance(probe) or not all(checks.values()):raise ValueError('World-less input drain lacks exact successful admission-stop evidence')
 return at

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('probe',type=Path);p.add_argument('--negative-tests',action='store_true');a=p.parse_args()
 raw=(a.probe/'view-resource-probe.jsonl').read_bytes();rows=[json.loads(l) for l in raw.decode().splitlines()];checks,at=audit(rows)
 checks['same_raw_independent_admission_audit']=verify_provenance(a.probe)
 if a.negative_tests:
  cases=[('wrong_stop','controller_world_epoch_stopped',{'state':2},'one_explicit_admission_stop'),('native_destroyed','combat_epoch_admission_exercised',{'native_world_destroyed':True},'one_explicit_admission_stop'),('read_after_close','input_ui_native_sample',{'readable':True},'no_world_ui_read_after_stop'),('bad_thread','controller_fixture_connected',{'thread':0},'frontend_native_thread_preserved'),('unrestored','controller_fixture_restored',{'pointers_restored':False},'owned_backend_restored_and_real_disconnect'),('fake_disconnect','controller_fixture_restored',{'native_disconnection_seen':False},'owned_backend_restored_and_real_disconnect'),('ui_read_fault','input_ui_observer_removed',{'read_failures':1},'ui_observer_drained_without_read_fault'),('late_aim','input_head_walking_sample',{'us':at+1,'applied':True},'no_new_vr_aim_after_close')]
  for name,event,change,target in cases:
   bad=copy.deepcopy(rows);next(r for r in bad if r['event']==event and (event!='input_ui_native_sample' or r['us']>at)).update(change)
   checks['negative_'+name]=checks[target] and not audit(bad)[0][target]
 report=dict(status='PASS' if all(checks.values()) else 'FAIL',checks=checks,source_sha256=hashlib.sha256(raw).hexdigest(),scope=__doc__,stop_us=at)
 (a.probe/'worldless-input-drain-checks.json').write_text(json.dumps(report,indent=2)+'\n')
 for k,v in checks.items():print('PASS' if v else 'FAIL',k)
 print('PASS',sum(checks.values()),'FAIL',sum(not x for x in checks.values()));return int(not all(checks.values()))
if __name__=='__main__':raise SystemExit(main())
