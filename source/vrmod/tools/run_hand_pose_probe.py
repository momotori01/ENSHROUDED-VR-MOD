"""Explicit unbound native RenderPose lifecycle test. No original/model pose writes."""
import argparse,hashlib,json,pathlib,struct,time
from inject_camera_observer import inject
from run_render_resource_probe import postcheck,retained_scene_probe

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--launch',type=pathlib.Path,required=True);p.add_argument('--world-proof',type=pathlib.Path,required=True);p.add_argument('--hierarchy',type=pathlib.Path,required=True);a=p.parse_args()
 mod=pathlib.Path(__file__).resolve().parents[1];game=mod.parent/'enshrouded.exe'
 if a.world_proof.resolve().parent!=a.launch.resolve():raise ValueError('world proof must belong to launch')
 if retained_scene_probe(a.launch) or list(a.launch.glob('hand-pose-probe-*')):raise ValueError('fresh undrawn process required')
 profile=json.loads((mod/'config/profiles/build23966345.json').read_text())
 if hashlib.sha256(game.read_bytes()).hexdigest()!=profile['exe_sha256']:raise ValueError('unsupported executable hash')
 launch=json.loads((a.launch/'launch.json').read_text(encoding='utf-8-sig'))
 if time.time()-(a.launch/'vulkan.jsonl').stat().st_mtime>10:raise ValueError('stale managed game telemetry')
 events=[json.loads(x) for x in (a.world_proof/'view-resource-probe.jsonl').read_text().splitlines()]
 done=[e for e in events if e['event']=='view_resource_hook_complete']
 guards=[e for e in events if e['event']=='world_feature_guard_observation']
 if len(done)!=1 or done[0]['result'] or not done[0]['original_bytes_restored'] or len(guards)!=1 or guards[0]['unknown_features']:raise ValueError('invalid world proof')
 post=json.loads((a.world_proof/'probe-postcheck.json').read_text())
 if post['status']!='PASS':raise ValueError('world proof postcheck failed')
 data=a.hierarchy.read_bytes()
 if len(data)!=72:raise ValueError('hierarchy header size')
 words=struct.unpack_from('<4I',data)
 if not any(words) or not 1<=struct.unpack_from('<I',data,48)[0]<=256:raise ValueError('hierarchy header invalid')
 out=a.launch/time.strftime('hand-pose-probe-%Y%m%d-%H%M%S')
 policy=f"EVR_UNUSED_HAND_POSE_1 {guards[0]['scene']} {guards[0]['view']} "+' '.join(map(str,words))+'\n'
 dll=pathlib.Path(json.loads((mod/'build/bin/Release/evr_view_resource_probe.json').read_text())['library_path'])
 crashes={x.name for x in (game.parent/'crash_dumps').iterdir()}
 result=inject(launch['pid'],game,out,dll,b'EVR_ProbeHandPose',launch['creation_filetime'],timeout_ms=25000,input_files={'hand-pose.txt':policy.encode()})
 (out/'source.json').write_text(json.dumps(dict(world_proof=str(a.world_proof.resolve()),hierarchy=str(a.hierarchy.resolve()),hierarchy_sha256=hashlib.sha256(data).hexdigest(),exe_sha256=profile['exe_sha256']),indent=2)+'\n',encoding='utf8')
 events=[json.loads(x) for x in (out/'view-resource-probe.jsonl').read_text().splitlines()]
 survived=postcheck(launch,out,events[-1]['us'],crashes,game)
 print('Result:',result,'Output:',out);return result or int(not survived)
if __name__=='__main__':raise SystemExit(main())
