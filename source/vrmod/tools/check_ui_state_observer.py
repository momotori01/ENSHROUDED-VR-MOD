"""Read-only native UI branch evidence; does not certify a media screen hookup."""
import argparse,json
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__);p.add_argument('probe',type=Path);p.add_argument('--expected-state',type=int,choices=range(1,6));a=p.parse_args()
r=[json.loads(x) for x in (a.probe/'view-resource-probe.jsonl').read_text(encoding='utf-8').splitlines()]
def event(name):return [e for e in r if e['event']==name]
b=event('ui_state_dispatch_enabled');e=event('ui_state_dispatch_removed');s=event('ui_state_dispatch_sample')
x=a.probe.parent/'process-exit.json';exit=json.loads(x.read_text(encoding='utf-8-sig')) if x.exists() else {}
checks={
 'explicit_read_only_policy':(a.probe/'world-ui-state-observe.txt').read_text().strip()=='EVR_UI_STATE_OBSERVE_1',
 'signature_verified':len(b)==1 and b[0]['rva']=='0xaac46c' and b[0]['read_only'],
 'samples_within_hook_lifetime':bool(s) and len(b)==len(e)==1 and all(b[0]['us']<v['us']<e[0]['us'] for v in s),
 'known_scoped_native_owner':bool(s) and all(v['known_branch'] and v['scoped_owner_matches'] for v in s),
 'source_unchanged_read_only':bool(s) and all(v['source_unchanged'] and v['read_only'] for v in s),
 'samples_monotonic':bool(s) and all(v['sample']<w['sample'] for v,w in zip(s,s[1:])),
 'no_observer_failures':not event('ui_state_dispatch_failure') and len(e)==1 and e[0]['failures']==0,
 'hook_restored_and_quiescent':len(e)==1 and e[0]['original_bytes_restored'] and e[0]['callbacks']==0 and e[0]['calls']>=max((v['sample'] for v in s),default=1),
 'normal_game_exit':exit.get('exit_code')==0,
}
if a.expected_state is not None:checks['expected_native_ui_state']=bool(s) and all(v['state']==a.expected_state for v in s)
report=dict(checks=checks,states=sorted({v['state'] for v in s}),passed=sum(checks.values()),failed=sum(not v for v in checks.values()),limitation=__doc__)
(a.probe/'ui-state-check.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
for k,v in checks.items():print('PASS' if v else 'FAIL',k)
print(report['passed'],'PASS /',report['failed'],'FAIL')
raise SystemExit(bool(report['failed']))
