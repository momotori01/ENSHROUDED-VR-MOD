"""Run an explicit 15-second native-game projection calibration and capture phases.

This is not stereo output: native, equivalent asymmetric, horizontal shifts, native.
The bounded DLL restores code itself even if image collection fails. No HMD or
host OpenXR/SteamVR settings are used. Never retries a pending remote invocation.
"""
import argparse
import concurrent.futures
import hashlib
import json
import pathlib
import shutil
import statistics
import subprocess
import time
from inject_camera_observer import inject, creation_filetime, Process
from check_capture import ppm


def events(path):
    if not path.exists(): return []
    rows = []
    for line in path.read_text(encoding='utf-8-sig').splitlines():
        try: rows.append(json.loads(line))
        except json.JSONDecodeError: continue  # in-progress final line
    return rows


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--launch', type=pathlib.Path)
    p.add_argument('--self-test', action='store_true')
    p.add_argument('--recorded-fov',type=pathlib.Path,help='Compare original full-feature menu View with saved Runtime FOV; no pose/extent changes')
    args = p.parse_args()
    mod = pathlib.Path(__file__).resolve().parents[1]
    binary = mod / 'build/bin/Release'
    dll = pathlib.Path(json.loads((binary/'evr_projection_probe.json').read_text())['library_path'])
    export=b'EVR_ProbeRecordedFov' if args.recorded_fov else b'EVR_ProbeProjection'
    if args.self_test:
        executable = binary/'evr_process_probe.exe'
        fixture = subprocess.Popen([str(executable), '--fixture'], stdout=subprocess.PIPE, text=True)
        try:
            pid, _ = fixture.stdout.readline().split()
            proc = Process(int(pid), executable)
            try: created = creation_filetime(proc.handle)
            finally: proc.close()
            out = mod/'output'/time.strftime('projection-loader-selftest-%Y%m%d-%H%M%S')
            result = inject(int(pid), executable, out, dll, export, created)
            assert result == 1 and fixture.poll() is None
            assert any(e.get('reason') == 'unsupported loaded executable build' for e in events(out/'projection-probe.jsonl'))
            print('PASS unsupported process refused, fixture alive, argument memory released')
            print(out)
        finally: fixture.terminate(); fixture.wait(timeout=5)
        return 0
    if not args.launch: p.error('--launch required')
    game = mod.parent/'enshrouded.exe'
    profile = json.loads((mod/'config/profiles/build23966345.json').read_text())
    if hashlib.sha256(game.read_bytes()).hexdigest() != profile['exe_sha256']:
        raise ValueError('unsupported game hash')
    launch = json.loads((args.launch/'launch.json').read_text(encoding='utf-8-sig'))
    if launch['mode'] != 'diagnostic_only' or not launch.get('creation_filetime'):
        raise ValueError('managed diagnostic launch with creation identity required')
    if time.time()-(args.launch/'vulkan.jsonl').stat().st_mtime > 10:
        raise ValueError('render telemetry stale')
    out = args.launch/time.strftime('projection-probe-%Y%m%d-%H%M%S')
    captured, seen, requested = {}, {}, {}
    original_files = set(args.launch.glob('game_*.ppm'))
    pending = None
    with concurrent.futures.ThreadPoolExecutor(max_workers=1) as worker:
        inputs={'recorded-xr.txt':args.recorded_fov.read_bytes()} if args.recorded_fov else None
        future = worker.submit(inject, launch['pid'], game, out, dll, export, launch['creation_filetime'], 45000, inputs)
        while not future.done():
            now = time.monotonic()
            for event in events(out/'projection-probe.jsonl'):
                if event['event'] == 'projection_phase_applied': seen.setdefault(event['phase'], now)
            if pending is not None:
                for path in set(args.launch.glob('game_*.ppm'))-original_files:
                    try: ppm(path)
                    except (ValueError, AssertionError, OSError): continue
                    target = out/f'phase{pending}.ppm'
                    shutil.copyfile(path, target)
                    captured[pending] = dict(path=str(target), source=str(path), requested_at=requested[pending])
                    original_files.add(path)
                    print('CAPTURE', pending, target, flush=True)
                    pending = None
                    break
            if pending is None and seen:
                step = max(seen)
                if step not in requested and now-seen[step] >= .4:
                    (args.launch/'capture.request').touch()
                    requested[step] = now
                    pending = step
            time.sleep(.025)
        result = future.result()
    rows = events(out/'projection-probe.jsonl')
    checks = []
    def check(name, ok, details=None):
        checks.append(dict(name=name, status='PASS' if ok else 'FAIL', details=details))
        print(checks[-1]['status'], name, flush=True)
    check('bounded_hook_returned_success', result == 0, result)
    complete = [e for e in rows if e['event'] == 'projection_hook_complete']
    check('code_restored', len(complete) == 1 and complete[0]['original_bytes_restored'])
    check('all_phases_executed', len(complete) == 1 and all(n > 0 for n in complete[0]['phase_counts']))
    check('all_phase_images_captured', set(captured) == set(range(5)))
    hashes = []
    for step, info in sorted(captured.items()):
        width, height, pixels = ppm(pathlib.Path(info['path']))
        sample = pixels[::51]
        check(f'phase{step}_nonblack', statistics.mean(sample) > 3)
        check(f'phase{step}_nonuniform', statistics.pvariance(sample) > 4)
        hashes.append(hashlib.sha256(pixels).hexdigest())
        info.update(width=width, height=height, sha256=hashes[-1])
    check('images_update', len(set(hashes)) > 1)
    report = dict(scope=__doc__, result=result, captures=captured, checks=checks,
                  limitations=['Image changes include animation. This check alone does not prove projection displacement.',
                               'No per-eye game rendering or VR compositing is tested.'])
    (out/'results.json').write_text(json.dumps(report, indent=2), encoding='utf8')
    print('Results:', out)
    return int(any(c['status'] == 'FAIL' for c in checks))


if __name__ == '__main__':
    raise SystemExit(main())
