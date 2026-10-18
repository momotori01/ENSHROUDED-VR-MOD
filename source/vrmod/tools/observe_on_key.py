#!/usr/bin/env python3
"""Take pictures of both eyes in a Mock trial the user steers by keyboard and mouse.

Two ways, both next to a running trial:

  observe_on_key.py <launch output txt name> [--key F9] [--pairs 6] [--burst-key F10] [--burst 32]
      watches the keyboard until the game ends. While the game's window is in front, a press of --key takes --pairs
      single eye pairs one after the other (about 15 frames apart); the first press of --burst-key takes the one burst
      of consecutive frames a launch allows.

  observe_on_key.py <launch output txt name> --now [--seconds 8 | --pairs 10] [--gap 0.05] [--label water]
      takes single pairs right away, as fast as the trial answers or --gap seconds apart, and ends. This is what the
      assistant runs when the user writes "now" in the chat or asks for a signal first.

User, 2026-10-13: "I will go to where the watering can's water and the waterside can be seen, operating it myself -
can you observe with that?" The trial runs as usual (Mock runtime, idle hands, a still head); the user walks with the
keyboard and turns with the mouse. A single pair is asked for with <game folder>/xr-capture.request, as often as
wanted; xr-burst.request (consecutive frames) is honoured once per launch only - a second one is consumed and ignored
(found in game-20261013-185120), so the burst is kept for what needs consecutive frames (flicker). Every request is
put on record in <game folder>/observe-on-key.jsonl with the time and the frame. The key's state is polled
(GetAsyncKeyState, 50 times a second); no hook is installed, nothing is injected, no setting is changed.
The pictures are native-xr/frame-N-eye-0/1.ppm, 23 MB a pair: delete them when the question is answered.
"""
import argparse, ctypes, json, pathlib, re, sys, time
from ctypes import wintypes

root = pathlib.Path('S:/SteamLibrary/steamapps/common/Enshrouded')
user32 = ctypes.windll.user32; kernel32 = ctypes.windll.kernel32
KEYS = {'F%d' % i: 111 + i for i in range(1, 13)}; KEYS.update({'INSERT': 45, 'HOME': 36, 'END': 35, 'PAUSE': 19})


def game_in_front():
    w = user32.GetForegroundWindow(); pid = wintypes.DWORD(0)
    if not w: return False
    user32.GetWindowThreadProcessId(w, ctypes.byref(pid))
    h = kernel32.OpenProcess(0x1000, False, pid.value)
    if not h: return False
    try:
        buf = ctypes.create_unicode_buffer(1024); n = wintypes.DWORD(1024)
        return bool(kernel32.QueryFullProcessImageNameW(h, 0, buf, ctypes.byref(n))) and buf.value.lower().endswith('enshrouded.exe')
    finally:
        kernel32.CloseHandle(h)


def last_frame(log, state):
    """The XR frame the trial has reached (the last line of its OpenXR log that names one)."""
    try:
        with log.open('rb') as f:
            f.seek(0, 2); size = f.tell(); f.seek(max(0, size - 4000)); tail = f.read().decode('utf-8', 'replace')
        found = re.findall(r'"frame":(\d+)', tail)
        if found: state['frame'] = int(found[-1])
    except OSError:
        pass
    return state.get('frame', 0)


def singles(game, count, seconds, gap, label):
    """Single pairs, one request after the other; returns the frames they were asked at."""
    request = game / 'xr-capture.request'; log = game / 'native-xr/openxr.jsonl'; state = {}; frames = []
    end = time.monotonic() + seconds if seconds else None
    while (len(frames) < count if count else True) and (time.monotonic() < end if end else True):
        if (game / 'process-exit.json').exists(): break
        frames.append(last_frame(log, state)); request.write_text('1'); began = time.monotonic()
        while request.exists() and time.monotonic() - began < 5: time.sleep(.01)
        time.sleep(gap)
    with (game / 'observe-on-key.jsonl').open('a', encoding='utf-8') as f:
        f.write(json.dumps(dict(wall=time.strftime('%H:%M:%S'), frames=frames, by=label)) + '\n')
    return frames


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument('launch'); p.add_argument('--key', default='F9', choices=sorted(KEYS)); p.add_argument('--pairs', type=int, default=0)
    p.add_argument('--burst-key', default='F10', choices=sorted(KEYS)); p.add_argument('--burst', type=int, default=32)
    p.add_argument('--now', action='store_true'); p.add_argument('--seconds', type=float, default=0); p.add_argument('--gap', type=float, default=.05)
    p.add_argument('--label', default='')
    a = p.parse_args()
    if not 1 <= a.burst <= 32: p.error('burst: 1..32')
    out = root / 'vrmod/output' / a.launch; game = None
    for _ in range(600):
        t = out.read_text(encoding='utf-8', errors='replace') if out.exists() else ''
        m = re.search(r'game-\d{8}-\d{6}', t)
        if m: game = root / 'vrmod/output' / m.group(0); break
        time.sleep(1)
    if not game: sys.exit('no game folder')
    log = game / 'native-xr/openxr.jsonl'
    if a.now:
        if not log.exists(): sys.exit('the trial does not render yet')
        frames = singles(game, a.pairs or (0 if a.seconds else 6), a.seconds, a.gap, a.label or 'chat')
        print(game.name, len(frames), 'pairs asked at', frames, flush=True); return
    key = KEYS[a.key]; burst_key = KEYS[a.burst_key]; down = burst_down = False; burst_used = False; state = {}; taken = 0
    print(game.name, 'key', a.key, 'singles', a.pairs or 6, '| burst key', a.burst_key, a.burst, 'frames, once', flush=True)
    while not (game / 'process-exit.json').exists():
        pressed = bool(user32.GetAsyncKeyState(key) & 0x8000); burst_pressed = bool(user32.GetAsyncKeyState(burst_key) & 0x8000)
        if log.exists() and game_in_front():
            if pressed and not down:
                frames = singles(game, a.pairs or 6, 0, a.gap, a.label or 'key'); taken += len(frames); print('singles asked at', frames, flush=True)
            if burst_pressed and not burst_down and not burst_used and not (game / 'xr-burst.request').exists():
                burst_used = True; frame = last_frame(log, state); (game / 'xr-burst.request').write_text(str(a.burst))
                with (game / 'observe-on-key.jsonl').open('a', encoding='utf-8') as f:
                    f.write(json.dumps(dict(wall=time.strftime('%H:%M:%S'), frame=frame, pairs=a.burst, by='burst key')) + '\n')
                print('burst of', a.burst, 'asked at frame', frame, '(the only one of this launch)', flush=True)
        down = pressed; burst_down = burst_pressed
        time.sleep(.02)
    print('game ended; single pairs', taken, 'burst used', burst_used, flush=True)


if __name__ == '__main__':
    main()
