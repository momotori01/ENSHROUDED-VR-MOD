#!/usr/bin/env python3
"""A walk the user made on the desktop in a Mock session, as a Mock key script timed by the game's clock (2026-10-18,
user: "I repeat the movement on the desktop - record it so that the Mock can use it; the frames must line up, else the
movement is not repeated").

  timed_route_keys.py <game folder> <out.keys> [--stop-after-ms 2000] [--burst-at-f8]

Reads <game folder>/input-record.jsonl (EVR_INPUT_RECORD=1, hooks/key_script.hpp: every keyboard and mouse record the
game read, under its XR frame, with the probe log's clock "us") and writes the script of hooks/key_script.hpp:
  "<F> ORIGIN" one frame before the first record, and every record as "t<ms> RAWKEY make flags vkey" / "t<ms> MOUSE dx dy"
  at its time after that frame. The game moves by time, not by frames (save_route.py writes frames; a replay whose frames
  take another time ends elsewhere: game-20261014-184902). F8 (the pictures of the record, EVR_CAPTURE_KEY=1) becomes
  "t<ms> CAPTURE" (--burst-at-f8: "BURST 8" for the first four), the Windows keys and Alt are left out (save_route.py).
  The end is "t<ms> STOP" --stop-after-ms after the last record.
The time of the ORIGIN frame is the probe log's live_native_frame_begin of that frame (the replay's ORIGIN is performed
on that frame too). Reads only.
"""
import argparse, json, pathlib

FUNCTION_KEYS = range(0x70, 0x88)
F8 = 0x77
LEAVE_KEYS = {0x5b, 0x5c, 0x5d, 0x12, 0xa4, 0xa5}


def frame_times(game):
    """XR frame -> us of live_native_frame_begin, from the game folder's last probe log."""
    logs = sorted(game.glob('view-resource-probe-*/view-resource-probe.jsonl'))
    times = {}
    for log in logs:
        for line in open(log, encoding='utf-8', errors='replace'):
            if '"live_native_frame_begin"' not in line: continue
            try: e = json.loads(line)
            except ValueError: continue
            times.setdefault(e['frame'], e['us'])
    return times


def main():
    a = argparse.ArgumentParser()
    a.add_argument('game'); a.add_argument('out')
    a.add_argument('--stop-after-ms', type=int, default=2000)
    a.add_argument('--burst-at-f8', action='store_true')
    o = a.parse_args()
    game = pathlib.Path(o.game)
    records = [json.loads(l) for l in open(game / 'input-record.jsonl', encoding='utf-8') if l.strip()]
    if not records: raise SystemExit('no records')
    first = min(r['frame'] for r in records)
    origin = max(1, first - 1)
    times = frame_times(game)
    if origin not in times: raise SystemExit('no live_native_frame_begin for frame %d' % origin)
    us0 = times[origin]
    lines = ['# %s: the user\'s walk on the desktop, timed after frame %d (tools/timed_route_keys.py)' % (game.name, origin),
             '%d ORIGIN' % origin]
    bursts = 0; last = 0; keys = mice = shots = 0
    for r in records:
        t = max(0, round((r['us'] - us0) / 1000)); last = max(last, t)
        if 'mouse' in r:
            dx, dy = r['mouse']
            if dx or dy: lines.append('t%d MOUSE %d %d' % (t, dx, dy)); mice += 1
        elif 'vkey' in r:
            if r['vkey'] == F8:
                if r['down']:
                    if o.burst_at_f8 and bursts < 4: lines.append('t%d BURST 8' % t); bursts += 1
                    else: lines.append('t%d CAPTURE' % t)
                    shots += 1
                continue
            if r['vkey'] in FUNCTION_KEYS or r['vkey'] in LEAVE_KEYS: continue
            lines.append('t%d RAWKEY %d %d %d' % (t, r['make'], r['flags'], r['vkey'])); keys += 1
    lines.append('t%d STOP' % (last + o.stop_after_ms))
    pathlib.Path(o.out).write_text('\n'.join(lines) + '\n', encoding='utf-8', newline='\n')
    print('origin frame', origin, 'keys', keys, 'mouse', mice, 'pictures', shots, 'end ms', last + o.stop_after_ms, '->', o.out)


if __name__ == '__main__':
    main()
