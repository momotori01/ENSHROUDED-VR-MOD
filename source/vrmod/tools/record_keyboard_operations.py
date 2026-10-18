#!/usr/bin/env python3
"""Record what the user does on the keyboard (and the mouse buttons) in the game's window, for repeating it later.

Started next to a running trial: record_keyboard_operations.py <launch output txt name>
It finds the trial's game folder from the launch output, then - until the game exits - polls the state of every key
(GetAsyncKeyState, 100 times a second; no hook is installed, nothing is injected, no setting is changed) and writes
each press and release that happens WHILE THE GAME'S WINDOW IS IN FRONT to <game folder>/keyboard-operations.jsonl:
wall clock, the performance counter in microseconds (the probe's `us`), the key, down or up, and for mouse buttons
the place in the window. Keys pressed while another window is in front are not recorded.
Half a second after a press it asks the trial for one picture of the eyes and of the UI image (xr-capture.request, at
most one a second), so that every step has the screen it led to: native-xr/ui-frame-<n>-xr.rgba8 (1920x1080).
Reads the keyboard state only; starts and stops nothing.
"""
import ctypes, json, pathlib, re, sys, time
from ctypes import wintypes

root = pathlib.Path('S:/SteamLibrary/steamapps/common/Enshrouded')
user32 = ctypes.windll.user32; kernel32 = ctypes.windll.kernel32
NAMES = {1: 'MOUSE_LEFT', 2: 'MOUSE_RIGHT', 4: 'MOUSE_MIDDLE', 8: 'BACKSPACE', 9: 'TAB', 13: 'ENTER', 16: 'SHIFT', 17: 'CTRL', 18: 'ALT', 20: 'CAPSLOCK', 27: 'ESC', 32: 'SPACE',
         33: 'PAGEUP', 34: 'PAGEDOWN', 35: 'END', 36: 'HOME', 37: 'LEFT', 38: 'UP', 39: 'RIGHT', 40: 'DOWN', 45: 'INSERT', 46: 'DELETE', 91: 'WIN_LEFT', 92: 'WIN_RIGHT',
         160: 'SHIFT_LEFT', 161: 'SHIFT_RIGHT', 162: 'CTRL_LEFT', 163: 'CTRL_RIGHT', 164: 'ALT_LEFT', 165: 'ALT_RIGHT'}
for c in range(48, 58): NAMES[c] = chr(c)
for c in range(65, 91): NAMES[c] = chr(c)
for i in range(1, 13): NAMES[111 + i] = 'F%d' % i
for i in range(10): NAMES[96 + i] = 'NUMPAD%d' % i


def image_name(pid):
    h = kernel32.OpenProcess(0x1000, False, pid)        # PROCESS_QUERY_LIMITED_INFORMATION
    if not h: return ''
    try:
        buf = ctypes.create_unicode_buffer(1024); n = wintypes.DWORD(1024)
        return buf.value if kernel32.QueryFullProcessImageNameW(h, 0, buf, ctypes.byref(n)) else ''
    finally:
        kernel32.CloseHandle(h)


def front():
    """(window, pid, True when the window in front belongs to the game)."""
    w = user32.GetForegroundWindow(); pid = wintypes.DWORD(0)
    if not w: return 0, 0, False
    user32.GetWindowThreadProcessId(w, ctypes.byref(pid))
    return w, pid.value, image_name(pid.value).lower().endswith('enshrouded.exe')


def main():
    out = root / 'vrmod/output' / sys.argv[1]; game = None
    for _ in range(600):
        t = out.read_text(encoding='utf-8', errors='replace') if out.exists() else ''
        m = re.search(r'game-\d{8}-\d{6}', t)
        if m: game = root / 'vrmod/output' / m.group(0); break
        time.sleep(1)
    if not game: sys.exit('no game folder')
    log = game / 'keyboard-operations.jsonl'; request = game / 'xr-capture.request'
    print(game.name, log, flush=True)
    state = [False] * 256; pending = None; last_request = 0.0; presses = 0; captures = 0; names = {}
    with log.open('a', encoding='utf-8') as f:
        def write(row):
            row.update(wall=round(time.time(), 3), us=int(time.perf_counter() * 1e6)); f.write(json.dumps(row) + '\n'); f.flush()
        write(dict(event='recorder_started', poll_hz=100, only_while_the_game_window_is_in_front=True))
        while not (game / 'process-exit.json').exists():
            w, pid, ours = front(); now = time.monotonic()
            for vk in range(1, 255):
                if vk in (16, 17, 18): continue                      # the generic modifiers repeat the left/right ones
                down = bool(user32.GetAsyncKeyState(vk) & 0x8000)
                if down == state[vk]: continue
                state[vk] = down
                if not ours: continue
                row = dict(event='key', vk=vk, key=NAMES.get(vk, 'VK_%d' % vk), down=down)
                if vk in (1, 2, 4):
                    p = wintypes.POINT(); user32.GetCursorPos(ctypes.byref(p)); user32.ScreenToClient(w, ctypes.byref(p))
                    r = wintypes.RECT(); user32.GetClientRect(w, ctypes.byref(r)); row.update(client=[p.x, p.y], client_size=[r.right, r.bottom])
                write(row)
                if down:
                    presses += 1; pending = now + .5
            if pending and now >= pending and now - last_request >= 1.0 and not request.exists():
                try:
                    request.touch(); captures += 1; last_request = now; pending = None; write(dict(event='picture_requested', number=captures))
                except OSError:
                    pass
            time.sleep(.01)
        write(dict(event='recorder_ended', presses=presses, pictures_requested=captures))
    print('ended', presses, 'presses', captures, 'pictures', flush=True)


if __name__ == '__main__':
    main()
