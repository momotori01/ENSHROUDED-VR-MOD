"""Version-gated camera observation. Never patches game instructions or player data."""
import argparse,hashlib,json,pathlib,subprocess,sys,time
sys.path.insert(0,str(pathlib.Path(__file__).parent/'python'))
import pefile

def main():
    p=argparse.ArgumentParser();p.add_argument('--launch',type=pathlib.Path,required=True);p.add_argument('--seconds',type=int,default=3);p.add_argument('--samples',type=int,default=2);a=p.parse_args()
    mod=pathlib.Path(__file__).resolve().parents[1];game=mod.parent/'enshrouded.exe'
    profile=json.loads((mod/'config/profiles/build23966345.json').read_text())
    binary=game.read_bytes()
    if hashlib.sha256(binary).hexdigest()!=profile['exe_sha256']:raise RuntimeError('Unsupported executable SHA; observation disabled')
    probe=profile['camera_probe']
    if not probe.get('enabled', False):
        raise RuntimeError(probe.get('disabled_reason', 'Hardware game probe is disabled'))
    pattern=bytes.fromhex(probe['pattern']);rva=int(probe['rva'],16)
    pe=pefile.PE(data=binary,fast_load=True);offset=pe.get_offset_from_rva(rva)
    if binary[offset:offset+len(pattern)]!=pattern or binary.count(pattern)!=1:raise RuntimeError('Signature is missing/ambiguous; observation disabled')
    launch=json.loads((a.launch/'launch.json').read_text(encoding='utf-8-sig'))
    if launch['mode']!='diagnostic_only':raise RuntimeError('Launch is not the managed diagnostic game')
    telemetry=a.launch/'vulkan.jsonl'
    if not telemetry.is_file() or time.time()-telemetry.stat().st_mtime>10:
        raise RuntimeError('Game telemetry is stale; resolve paused rendering/modal before attaching the debugger')
    output=a.launch/time.strftime('camera-probe-%Y%m%d-%H%M%S.jsonl')
    cmd=[str(mod/'build/bin/Release/evr_process_probe.exe'),'--pid',str(launch['pid']),'--rva',probe['rva'],
         '--seconds',str(a.seconds),'--samples',str(a.samples),'--expected-path',str(game),'--expected-bytes',probe['pattern'],'--output',str(output)]
    print('Validated build SHA and unique code signature; observing camera query only',flush=True)
    # The debugger owns bounded shutdown and restores debug registers before detach.
    # Do not forcibly kill a debugger attached to the game on an arbitrary Python timeout.
    result=subprocess.run(cmd)
    print('Observation:',output)
    if result.returncode==0:
        rows=[json.loads(line) for line in output.read_text(encoding='utf8').splitlines()]
        complete=next((row for row in reversed(rows) if row['event']=='probe_complete'),{})
        if not complete.get('samples'):
            print('NO HIT: cleanup succeeded, but no camera observation was captured',file=sys.stderr)
            return 3
    return result.returncode

if __name__=='__main__':sys.exit(main())
