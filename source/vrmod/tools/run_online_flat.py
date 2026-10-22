"""The mod without VR: other players' VR bodies in the game as it runs by itself (hooks EVR_OnlineFlat).

User, 2026-10-14: "with the VR display off the other players' movements are still seen as long as the mod is
installed"; "VR can be switched on and off". The game is a managed launch (launch_diagnostics, no VR preparation
needed); the world Scene and View come from the passed unused-resolution probe of the same launch, as for VR.
Ends after --minutes or when <output>/stop.request appears. Nothing of the game is drawn differently.
"""
import argparse
import csv
import hashlib
import json
import pathlib
import re
import subprocess
import time
from inject_camera_observer import inject


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument('--launch', type=pathlib.Path, help='the managed launch (vrmod/output/game-...)')
    p.add_argument('--attach', action='store_true', help='no managed launch: the game that is running now (started from Steam as usual); the hooks find the world themselves')
    p.add_argument('--proof', type=pathlib.Path, help='optional: the passed --world-view-features (or --world-view-resolution) probe directory of this launch; without it the hooks find the world themselves (and wait for one at the menu)')
    p.add_argument('--minutes', type=int, default=120)
    p.add_argument('--hand-curl', type=pathlib.Path, help='grip preview directory whose hand-pose.bin is the closed hand (fingers of the other players)')
    p.add_argument('--hand-curl-relaxed', type=float, default=.15)
    p.add_argument('--dll', type=pathlib.Path, help='the hook library to load (default: the one the build recorded; in a copied package, evr_view_resource_probe.dll beside that record)')
    args = p.parse_args()
    if not 1 <= args.minutes <= 720: p.error('--minutes 1..720')
    mod = pathlib.Path(__file__).resolve().parents[1]
    binary = mod / 'build/bin/Release'
    game = mod.parent / 'enshrouded.exe'
    # the build the hook library is made for (include/evr/game_build.hpp; the June profile's hash was left from before 1.0)
    supported = re.search(r'sha256Prefix="([0-9a-f]+)"', (mod / 'include/evr/game_build.hpp').read_text(encoding='utf-8')).group(1)
    if not hashlib.sha256(game.read_bytes()).hexdigest().startswith(supported): raise ValueError('unsupported game hash')
    if bool(args.launch) == bool(args.attach): p.error('one of --launch and --attach')
    if args.attach:
        if args.proof: p.error('--attach finds the world itself: no --proof')
        listed = subprocess.run(['tasklist', '/FI', 'IMAGENAME eq enshrouded.exe', '/FO', 'CSV', '/NH'], capture_output=True, text=True).stdout
        pids = [int(row[1]) for row in csv.reader(listed.splitlines()) if len(row) > 1 and row[0].lower() == 'enshrouded.exe']
        if len(pids) != 1: raise ValueError('exactly one running enshrouded.exe is needed (found %d)' % len(pids))
        args.launch = mod / 'output' / time.strftime('attach-%Y%m%d-%H%M%S'); args.launch.mkdir(parents=True)
        launch = dict(pid=pids[0], creation_filetime=None, mode='attached')
        (args.launch / 'attach.json').write_text(json.dumps(launch, indent=2), encoding='utf-8')
    else:
        launch = json.loads((args.launch / 'launch.json').read_text(encoding='utf-8-sig'))
        if launch['mode'] not in {'diagnostic_only', 'openxr_graphics_preparation'} or not launch.get('creation_filetime'): raise ValueError('managed diagnostic launch required')
    input_files = {'online-flat.txt': f'EVR_ONLINE_FLAT_1 {args.minutes}\n'.encode()}
    if args.proof:
        if args.proof.parent.resolve() != args.launch.resolve(): raise ValueError('the proof is not of this launch')
        status = json.loads((args.proof / 'probe-status.json').read_text(encoding='utf-8'))
        if status.get('status') != 'PASS': raise ValueError('the unused-resolution probe did not pass')
        records = [json.loads(line) for line in (args.proof / 'view-resource-probe.jsonl').read_text(encoding='utf-8', errors='replace').splitlines() if line.strip().endswith('}')]
        views = [e for e in records if e['event'] == 'native_view_created']
        stages = [e for e in records if e['event'] == 'unused_world_resource_diagnostic']
        if len(views) != 2 or len({(e['scene'], e['original_view']) for e in views}) != 1 or len(stages) != 1 or stages[0]['stage'] not in (1, 2):
            raise ValueError('world Scene/View proof missing in the probe')
        input_files['world-resource-proof.txt'] = f"EVR_WORLD_RESOURCE_1 {views[0]['scene']} {views[0]['original_view']}\n".encode()
        input_files['world-resource-provenance.json'] = json.dumps(dict(probe=str(args.proof), creation_filetime=launch['creation_filetime'], pid=launch['pid']), indent=2).encode()
    if args.hand_curl:
        if not 0 <= args.hand_curl_relaxed <= .9: raise ValueError('--hand-curl-relaxed must be 0..0.9')
        curl = json.loads((args.hand_curl / 'profile.json').read_text())
        pose = (args.hand_curl / 'hand-pose.bin').read_bytes()
        if len(pose) != 102 * 48 or hashlib.sha256(pose).hexdigest() != curl['files']['hand-pose.bin'] or not curl.get('closed_fingers'): raise ValueError('closed hand pose does not match its grip profile')
        input_files['world-hand-curl.txt'] = f'EVR_HAND_CURL_1 {args.hand_curl_relaxed}\n'.encode()
        input_files['hand-curl-closed-pose.bin'] = pose
    out = args.launch / time.strftime('online-flat-%Y%m%d-%H%M%S')
    dll = args.dll
    if not dll:
        record = binary / 'evr_view_resource_probe.json'
        dll = pathlib.Path(json.loads(record.read_text())['library_path']) if record.exists() else binary / 'evr_view_resource_probe.dll'
        if not dll.exists(): dll = binary / 'evr_view_resource_probe.dll'   # a package copied to another machine
    dll = dll.resolve()
    if not dll.exists(): raise ValueError(f'hook library not found: {dll}')
    result = inject(launch['pid'], game, out, dll, b'EVR_OnlineFlat', launch['creation_filetime'], timeout_ms=args.minutes * 60000 + 90000, input_files=input_files)
    print('Result:', result, 'Output:', out)
    events = [json.loads(line) for line in (out / 'view-resource-probe.jsonl').read_text(encoding='utf-8', errors='replace').splitlines() if line.strip().endswith('}')]
    done = [e for e in events if e['event'] == 'online_flat_complete']
    failures = [e for e in events if e['event'].endswith('_failure')]
    overall = dict(status='PASS' if result == 0 and done and done[0]['result'] == 0 and not failures else 'FAIL', native_result=result,
                   complete=done[0] if done else None, failures=failures[:5])
    (out / 'online-flat-status.json').write_text(json.dumps(overall, indent=2), encoding='utf-8')
    print('OVERALL', overall['status'])
    return 0 if overall['status'] == 'PASS' else 1


if __name__ == '__main__': raise SystemExit(main())
