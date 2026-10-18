"""Independent hand posture -> native LB/block verification; no enemy parry success claim."""
import argparse,copy,csv,json,math
from pathlib import Path
from check_native_guard import audit as native_audit

def audit(rows,poses,delay=0):
    c,m=native_audit(rows);ev=lambda n:[r for r in rows if r['event']==n]
    samples=ev('motion_guard_sample');enable=ev('motion_guard_enabled');edges=[r for r in samples if r['edge']]
    c['explicit_synthetic_body_policy']=len(enable)==1 and enable[0].get('synthetic_calibration') is True and enable[0].get('startup_requires_lowering') is True and enable[0].get('parry_state_writes') is False
    c['two_posture_edges_only']=len(edges)==2 and 110+delay<=edges[0]['frame']<=130+delay and 210+delay<=edges[1]['frame']<=235+delay
    c['initial_raised_pose_does_not_press']=any((31+delay if not delay else 364)<=r['frame']<=60+delay for r in samples) and all(r['buttons']==0 for r in samples if r['frame']<110+delay)
    c['same_calibration_epoch']=bool(samples) and len({(r['session'],r['origin'],r['binding']) for r in samples})==1 and all(r['session'] and r['origin'] and r['binding'] for r in samples)
    c['fresh_same_frame_guard']=bool(samples) and all(r['fresh'] and r['frame'] in poses and 0<=r['milliseconds']-r['receipt']<=200 for r in samples)
    errs=[]
    for r in samples:
        if r['frame'] not in poses:continue
        v=poses[r['frame']]
        errs.extend([abs(r['forward']+v[3]),abs(r['above_navel']-(v[2]-1))])
        errs.extend(abs(a-b) for a,b in zip(r['navel'],[0,1,0]));errs.extend(abs(a-b) for a,b in zip(r['body_forward'],[0,0,-1]))
    c['body_and_guard_region_from_actual_csv']=bool(errs) and max(errs)<1e-5
    c['each_edge_native_can_block']=len(edges)==2 and all(r['can_block'] and r['posture'] and r['buttons']==256 and r['blocked']==0 for r in edges)
    c['front_entry_above_navel']=len(edges)==2 and edges[0]['above_navel']>=0 and edges[0]['forward']>=.29999
    c['overhead_without_forward_extension']=len(edges)==2 and edges[1]['above_navel']>=.59999 and abs(edges[1]['forward'])<1e-5
    # Correlate accepted packets with the earlier posture edge/release rather
    # than trusting only the adapter's self-reported status.
    packets=ev('controller_fixture_packet_polled')
    c['LB_packets_have_preceding_posture']=len(edges)==2 and all(any(e['us']<=p['us'] and p['us']-e['us']<500000 for e in edges) for p in packets if p['buttons']==256)
    m.update(posture_frames=len(samples),edges=[{k:r[k] for k in ('frame','forward','above_navel','milliseconds')} for r in edges],max_body_geometry_error=max(errs,default=None))
    return c,m

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('probe',type=Path);p.add_argument('--negative-tests',action='store_true');a=p.parse_args();probe=a.probe
    if (probe/'controller-ui-fixture.txt').read_text().strip()!='EVR_MOTION_GUARD_1':raise ValueError('explicit motion guard fixture required')
    rows=[json.loads(l) for l in (probe/'view-resource-probe.jsonl').read_text().splitlines()]
    poses={int(v[0]):list(map(float,v)) for v in csv.reader(l for l in (probe.parent/'mock-hands.csv').read_text().splitlines() if l and not l.startswith('#'))}
    delay=json.loads((probe/'guard-sequence.json').read_text())['delay_frames'] if (probe/'guard-sequence.json').exists() else 0
    c,m=audit(rows,poses,delay)
    c['native_stereo_600_pairs']=sum(r['event']=='live_native_pair_complete' for r in rows)==600
    c['runner_PASS']=json.loads((probe/'probe-status.json').read_text())['status']=='PASS'
    c['normal_exit']=json.loads((probe.parent/'process-exit.json').read_text(encoding='utf-8-sig'))['exit_code']==0
    c['Core_Sync_clean']=(probe.parent/'validation.log').stat().st_size==0
    if a.negative_tests:
        for event,key,value in [('motion_guard_sample','edge',False),('motion_guard_sample','forward',5),('motion_guard_sample','above_navel',-1),('motion_guard_sample','fresh',False),('motion_guard_sample','binding',0),('motion_guard_sample','can_block',False),('motion_guard_enabled','parry_state_writes',True),('guard_fixture_phase','phase',5),('controller_fixture_packet_polled','buttons',16384),('action_sequence_snapshot','input_digital',1),('controller_fixture_restored','pointers_restored',False)]:
            bad=copy.deepcopy(rows)
            for r in bad:
                if r['event']==event:r[key]=value
            c['reject_'+event+'_'+key]=not all(audit(bad,poses,delay)[0].values())
    report=dict(checks={k:'PASS' if v else 'FAIL' for k,v in c.items()},measurements=m,limitation=__doc__)
    (probe/'motion-guard-checks.json').write_text(json.dumps(report,indent=2)+'\n')
    for k,v in c.items():print('PASS' if v else 'FAIL',k)
    print('PASS',sum(bool(v) for v in c.values()),'FAIL',sum(not v for v in c.values()));return int(not all(c.values()))
if __name__=='__main__':raise SystemExit(main())
