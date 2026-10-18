#!/usr/bin/env python3
"""Pictures of the game's window for the steps that record_keyboard_operations.py writes down.

shoot_keyboard_operations.py <game folder name>
Follows <game folder>/keyboard-operations.jsonl; 0.45 s after each key press (and 1.5 s after it, for slow pages) it
copies the game's window from the screen - only while that window is in front - into
<game folder>/keyboard-shots/<milliseconds since the recorder started>-<key>.png, half size. Reads the screen and
the record; changes nothing in the game. Needs PIL (PYTHONPATH=vrmod/tools/python).
"""
import ctypes, json, pathlib, sys, time
from ctypes import wintypes
from PIL import ImageGrab
import record_keyboard_operations as rec

root = pathlib.Path('S:/SteamLibrary/steamapps/common/Enshrouded')
user32 = ctypes.windll.user32


def shot(folder, name):
    w, pid, ours = rec.front()
    if not ours: return False
    r = wintypes.RECT(); p = wintypes.POINT(0, 0)
    if not user32.GetClientRect(w, ctypes.byref(r)) or r.right <= 0 or r.bottom <= 0: return False
    user32.ClientToScreen(w, ctypes.byref(p))
    im = ImageGrab.grab(bbox=(p.x, p.y, p.x + r.right, p.y + r.bottom), all_screens=True)
    im = im.resize((max(1, im.width // 2), max(1, im.height // 2)))
    im.save(folder / name); return True


def main():
    game = root / 'vrmod/output' / sys.argv[1]; log = game / 'keyboard-operations.jsonl'; folder = game / 'keyboard-shots'; folder.mkdir(exist_ok=True)
    offset = 0; start = None; due = []; taken = 0
    while not (game / 'process-exit.json').exists():
        if log.exists():
            with log.open('rb') as f:
                f.seek(offset); data = f.read()
            if data.endswith(b'\n'):
                offset += len(data)
                for line in data.splitlines():
                    try: e = json.loads(line)
                    except ValueError: continue
                    if start is None: start = e['wall']
                    if e.get('event') == 'key' and e.get('down'):
                        ms = int((e['wall'] - start) * 1000)
                        due.append((e['wall'] + .45, '%07d-%s.png' % (ms, e['key']))); due.append((e['wall'] + 1.5, '%07d-%s-later.png' % (ms, e['key'])))
        now = time.time(); ready = [d for d in due if d[0] <= now]; due = [d for d in due if d[0] > now]
        # Several keys in a row: one picture for the newest of each kind is enough.
        for when, name in ready[-2:]:
            try:
                if shot(folder, name): taken += 1
            except Exception as e:
                print('shot failed', e, flush=True)
        time.sleep(.05)
    print('pictures', taken, flush=True)


if __name__ == '__main__':
    sys.path.insert(0, str(pathlib.Path(__file__).parent))
    main()
