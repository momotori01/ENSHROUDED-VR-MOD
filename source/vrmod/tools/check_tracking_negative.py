"""Negative controls on copies of a passing world tracking-recovery trace."""
import argparse,copy,csv,json,pathlib
from check_tracking_recovery import load,check_tracking

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('probe',type=pathlib.Path);a=p.parse_args();launch=a.probe.parent
    native=load(a.probe/'view-resource-probe.jsonl');lo=min(e['us'] for e in native);hi=max(e['us'] for e in native)
    xr=[e for e in load(launch/'native-xr/openxr.jsonl') if lo<=e['us']<=hi];mock=[e for e in load(launch/'mock.jsonl') if lo<=e['us']<=hi]
    with (launch/'mock-pose.csv').open() as f:poses=list(csv.reader(line for line in f if not line.startswith('#')))
    checks,metrics=check_tracking(native,xr,mock,poses)
    if not all(checks.values()):raise ValueError('negative controls require a passing source')
    first=metrics['intervals'][0]['first'];last=metrics['intervals'][0]['last']
    begin=next(e['us'] for e in native if e['event']=='world_tracking_frame_deferred' and e['frame']==first)
    end=next(e['us'] for e in native if e['event']=='input_pose_published' and e['frame']==last+1)
    rows=[]
    for case in range(6):
        n,x,m,c=copy.deepcopy((native,xr,mock,poses))
        if case==0:
            next(e for e in n if e['event']=='input_head_walking_sample' and begin<=e['us']<end)['applied']=True
            key='tracking_all_loss_intervals_preserve_native_input'
        elif case==1:
            e=copy.deepcopy(next(e for e in n if e['event']=='input_pose_published'));e['frame']=first;n.append(e)
            key='tracking_no_stale_hmd_publication'
        elif case==2:
            next(e for e in n if e['event']=='calibration_eye_recorded')['serial']=next(e['serial'] for e in n if e['event']=='world_tracking_frame_deferred')
            key='tracking_no_eye_draw_or_layer_during_loss'
        elif case==3:
            next(e for e in n if e['event']=='input_pose_published' and e['frame']==last+1)['origin_epoch']+=1
            key='tracking_same_session_and_origin_after_recovery'
        elif case==4:
            next(e for e in x if e['event']=='native_frame_deferred')['empty_frame_ended']=False
            key='tracking_empty_frames_end_once'
        else:
            c[first-1][8]='1';key='tracking_loss_matches_explicit_csv'
        result,_=check_tracking(n,x,m,c);rows.append(dict(case=case,expected_failed_check=key,status='PASS' if not result[key] else 'FAIL'))
    pathlib.Path('docs/analysis/tracking_recovery_negative_controls.json').write_text(json.dumps(dict(source=str(a.probe),source_unchanged=True,cases=rows),indent=2)+'\n')
    print(sum(r['status']=='PASS' for r in rows),'PASS /',sum(r['status']=='FAIL' for r in rows),'FAIL');return int(any(r['status']!='PASS' for r in rows))
if __name__=='__main__':raise SystemExit(main())
