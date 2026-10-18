#!/usr/bin/env python3
"""Run one home equipment Mock trial to a replay frame and wait for its session check.

Wraps the manual sequence used on 2026-10-11: start run_with_sound.ps1 (normal exit, close sound only),
read the game folder name from its output, request the normal stop at the given replay frame, request
a few eye captures, and wait until play-session-check.json exists. Prints the game folder, the probe
folder, the exit code and the session verdict as one JSON line. It launches the game: tell the user first.
"""
import argparse, json, pathlib, re, subprocess, sys, time

ROOT = pathlib.Path(__file__).resolve().parents[3]
PWSH = r'pwsh'


def main():
    # DO NOT USE as it is. The only run through this wrapper (game-20261011-033249, started from Git Bash ->
    # python -> pwsh) ended inside the game's own start-up, 1 ms in, before any MOD code:
    #   [memory] Could not allocate 786,432,000 bytes ... error=487 / FATAL ERROR: MemorySystem_InitializationFailed
    # The same trial started the usual way one minute later (PowerShell tool -> pwsh -> run_with_sound.ps1 in the
    # background, game-20261011-033355) started normally. Which layer of this wrapper causes it is not known.
    raise SystemExit('run_mock_trial_to_frame.py is disabled: see the comment in main(); start trials the usual way')
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--name', required=True, help='label of the output text file under vrmod/output')
    p.add_argument('--args', required=True, help='arguments of run_home_equipment_trial.ps1')
    p.add_argument('--frame', type=int, required=True, help='replay frame of the normal stop')
    p.add_argument('--captures', default='', help='comma separated replay frames for eye captures')
    p.add_argument('--burst', type=int, default=0)
    a = p.parse_args()
    out = ROOT / 'vrmod/output' / f'{a.name}.txt'
    if out.exists():
        raise SystemExit(f'{out} exists: choose another --name')
    runner = subprocess.Popen([PWSH, '-NoProfile', '-File', str(ROOT / 'vrmod/tools/handoff_helpers/run_with_sound.ps1'),
                               '-Script', './vrmod/tools/handoff_helpers/run_home_equipment_trial.ps1', '-ScriptArgs', a.args, '-Out', str(out)],
                              cwd=ROOT, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    game = None
    for _ in range(300):
        text = out.read_text(encoding='utf-8', errors='replace') if out.exists() else ''
        found = re.search(r'game-\d{8}-\d{6}', text)
        if found:
            game = ROOT / 'vrmod/output' / found.group(0)
            break
        if runner.poll() is not None:
            raise SystemExit('runner ended before a game folder appeared:\n' + text[-1500:])
        time.sleep(1)
    if not game:
        raise SystemExit('no game folder')
    py = sys.executable
    helpers = [subprocess.Popen([py, str(ROOT / 'vrmod/tools/stop_at_replay_frame.py'), str(game), '--frame', str(a.frame), '--timeout', '900'],
                                cwd=ROOT, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)]
    if a.captures:
        helpers.append(subprocess.Popen([py, str(ROOT / 'vrmod/tools/capture_replay_frames.py'), str(game), '--frames', a.captures,
                                         '--burst', str(a.burst or a.frame - 200), '--timeout', '900'], cwd=ROOT, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE))
    runner.wait()
    for h in helpers:
        h.wait(timeout=60)
    result = dict(game=game.name, runner_exit=runner.returncode, stop_helper_exit=helpers[0].returncode)
    exit_file, check = game / 'process-exit.json', game / 'play-session-check.json'
    if exit_file.exists():
        e = json.loads(exit_file.read_text(encoding='utf-8-sig'))
        result.update(exit_code=e.get('exit_code'), exit_time=e.get('exit_time'))
    if check.exists():
        c = json.loads(check.read_text(encoding='utf-8'))
        result.update(probe=c.get('probe'), play=c.get('status'), failed=[k for k, v in c.get('checks', {}).items() if v != 'PASS'], pairs=c.get('total_pairs'))
    print(json.dumps(result))


if __name__ == '__main__':
    main()
