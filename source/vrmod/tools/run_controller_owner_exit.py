"""Finite isolated neutral input owner-destruction trial. No XR or combat claim."""
import argparse,hashlib,json,time
from pathlib import Path
from inject_camera_observer import inject

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--launch',type=Path,required=True);p.add_argument('--publisher',type=Path,required=True);p.add_argument('--seconds',type=int,default=120);a=p.parse_args()
 if not 60<=a.seconds<=180:p.error('finite duration must be60..180 seconds')
 if a.publisher.resolve().parent!=a.launch.resolve() or (a.publisher/'input-backend-exit.txt').exists():raise ValueError('use same-process publisher without another backend destructor hook')
 source=(a.publisher/'view-resource-probe.jsonl').read_bytes();rows=[json.loads(l) for l in source.splitlines()]
 if len([r for r in rows if r['event']=='world_broker_created'])!=3:raise ValueError('all three initial Worlds must already be created')
 mod=Path(__file__).resolve().parents[1];game=mod.parent/'enshrouded.exe'
 profile=json.loads((mod/'config/profiles/build23966345.json').read_text());sha=hashlib.sha256(game.read_bytes()).hexdigest()
 if sha!=profile['exe_sha256']:raise ValueError('unsupported game executable')
 launch=json.loads((a.launch/'launch.json').read_text(encoding='utf-8-sig'))
 if time.time()-(a.launch/'vulkan.jsonl').stat().st_mtime>10 or (a.launch/'process-exit.json').exists():raise ValueError('managed process telemetry is stale/ended')
 dll=Path(json.loads((mod/'build/bin/Release/evr_view_resource_probe.json').read_text())['library_path'])
 out=a.launch/time.strftime('controller-owner-exit-%Y%m%d-%H%M%S');crashes={s.name for s in (game.parent/'crash_dumps').iterdir()}
 policy={'controller-owner-exit.txt':f'EVR_CONTROLLER_OWNER_EXIT_1 {a.seconds}\n'.encode(),'controller-ui-fixture.txt':b'EVR_CONTROLLER_NEUTRAL_1\n','owner-exit-provenance.json':(json.dumps({'publisher':str(a.publisher.resolve()),'source_prefix_bytes':len(source),'source_prefix_sha256':hashlib.sha256(source).hexdigest(),'pid':launch['pid'],'creation_filetime':launch['creation_filetime'],'exe_sha256':sha},indent=2)+'\n').encode()}
 result=inject(launch['pid'],game,out,dll,b'EVR_ObserveControllerOwnerExit',launch['creation_filetime'],timeout_ms=(a.seconds+10)*1000,input_files=policy)
 rows=[json.loads(l) for l in (out/'view-resource-probe.jsonl').read_text().splitlines()]
 deadline=time.monotonic()+15;exitfile=a.launch/'process-exit.json'
 while not exitfile.exists() and time.monotonic()<deadline:time.sleep(.1)
 ended=json.loads(exitfile.read_text(encoding='utf-8-sig')) if exitfile.exists() else {}
 newcrashes={s.name for s in (game.parent/'crash_dumps').iterdir()}-crashes
 completed=any(r['event']=='controller_owner_exit_complete' and r['result']==0 for r in rows)
 okay=result==0 and completed and ended.get('status')=='exited' and ended.get('exit_code')==0 and not newcrashes and not any('failure' in r['event'] for r in rows)
 report={'status':'PASS' if okay else 'FAIL','remote_thread_result':result,'owner_finalizer_completed':completed,'process_exit':ended,'new_crash_directories':sorted(newcrashes),'scope':__doc__}
 (out/'controller-owner-run.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2));print(out);return int(not okay)
if __name__=='__main__':raise SystemExit(main())
