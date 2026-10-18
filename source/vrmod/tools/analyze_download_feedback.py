"""Summarize read-only pool observations; failed copies remain explicit failures."""
import argparse
import collections
import json
from pathlib import Path


def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('probe',type=Path);a=p.parse_args()
    rows=[json.loads(l) for l in (a.probe/'view-resource-probe.jsonl').read_text().splitlines()]
    summaries={}
    for e in rows:
        if e['event']!='download_feedback_pool':continue
        for c in e['calls']:
            key=(e['eye'],c['caller'],c['pool'])
            s=summaries.setdefault(key,dict(eye=e['eye'],caller=c['caller'],pool=c['pool'],calls=0,
                status_counts=collections.Counter(),capacities=set(),max_pending=0,failures_without_eligible_slot=0,
                failures=0,unreadable=0,first_failure=None))
            s['calls']+=1;s['status_counts'][c['status']]+=1
            s['capacities'].add(c['before']['capacity']);s['max_pending']=max(s['max_pending'],c['after']['pending'])
            if not c['before']['valid'] or not c['after']['valid']:s['unreadable']+=1
            if c['status']:
                s['failures']+=1
                if s['first_failure'] is None:s['first_failure']=dict(serial=e['serial'],observation=c)
                if c['after']['valid'] and all(slot[0]>c['after']['completed'] for slot in c['after']['slots']):
                    s['failures_without_eligible_slot']+=1
    output=list(summaries.values())
    for s in output:s['capacities']=sorted(s['capacities'])
    result=dict(groups=output,total_failures=sum(s['failures'] for s in output),
        limit='Native slot snapshots at the observed call boundary; no atomic cross-thread or complete GPU visibility/quality proof.')
    (a.probe/'download-feedback-analysis.json').write_text(json.dumps(result,indent=2))
    for s in output:print(json.dumps({k:v for k,v in s.items() if k!='first_failure'}))
    print('FAIL' if result['total_failures'] else 'PASS','native feedback copy failures',result['total_failures'])


if __name__=='__main__':main()
