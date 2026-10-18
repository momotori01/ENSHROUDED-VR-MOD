#!/usr/bin/env python3
"""Audit the grip rule of the live guard (2026-10-14, user: "with a weapon that can guard, the guard is up while the
left grip is held; the block faces where the shield faces; the moment the grip is pressed, or the left controller swung
fast, is a parry - the same for a two-hand weapon, held with both hands; swung with both hands at once, the attack has
priority").

  check_guard_grip.py <probe directory> [--scope first,last]... [--two-hand first,last]...

For every scope (frames of the hand script where a shield, or with --two-hand a weapon held with both hands, is in the
hands) the hand script's left grip (column 11 of mock-hands.csv) gives the windows:
  guard_only_with_grip   no block press outside a window of the left grip
  gaps_explained         inside a window, after the first press, every gap of the press is a parry's re-press (12 frames
                         at most), holds a melee press (the attack first), follows a swing parry (its 350 ms release,
                         2026-10-15), or the weapon is no longer held to guard
  grip_press_parry       the first press of every window is a parry press
  native_blocking        the game's Blocking state comes on within 45 frames of the first press of the scope
  look_is_the_guard      while pressed, the character looks along the guard's camera (input_actor_final_sample's look
                         against its camera, 3 degrees, from 12 frames after a press; the guard window records - unchecked where none are)
  two_hand_only          (--two-hand) the presses are those of the two-hand weapon
Mock only; a block or parry of a hit is not audited (no enemy).
"""
import argparse, bisect, csv, json, math
from pathlib import Path


def windows(rows, first, last):
    on = [f for f in sorted(rows) if first <= f <= last and rows[f] >= .5]
    out = []
    for f in on:
        if out and f == out[-1][1] + 1: out[-1][1] = f
        else: out.append([f, f])
    return [tuple(w) for w in out]


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument('probe', type=Path); p.add_argument('--scope', action='append', default=[]); p.add_argument('--two-hand', action='append', default=[])
    a = p.parse_args()
    events = [json.loads(s) for s in (a.probe / 'view-resource-probe.jsonl').read_text(encoding='utf-8').splitlines()]
    ev = lambda name: [e for e in events if e['event'] == name]
    grip = {}
    for r in csv.reader(s for s in (a.probe.parent / 'mock-hands.csv').read_text().splitlines() if s and not s.startswith('#')):
        grip[int(float(r[0]))] = float(r[11])
    states, natives, melee, swings = ev('live_guard_state'), ev('live_guard_native'), ev('live_melee_press'), ev('live_guard_swing_parry')
    anchors = sorted((e['us'], e['recorded_xr_frame']) for e in ev('recorded_xr_anchor'))
    us = [x[0] for x in anchors]
    frame_of = lambda u: anchors[min(bisect.bisect_left(us, u), len(us) - 1)][1] if anchors else 0
    look = [(frame_of(e['us']), e) for e in ev('input_actor_final_sample') if e.get('guard_window')]
    yaw = lambda v: math.degrees(math.atan2(v[0], v[2]))
    def qyaw(q):
        qx, qy, qz, qw = q; return math.degrees(math.atan2(2 * (qx * qz + qw * qy), 1 - 2 * (qx * qx + qy * qy)))
    checks, report, unchecked = {}, {}, []
    scopes = [(s, False) for s in a.scope] + [(s, True) for s in a.two_hand]
    for text, two in scopes:
        first, last = (int(v) for v in text.split(','))
        name = ('two_hand ' if two else 'shield ') + text
        inside = [s for s in states if first <= s['frame'] <= last]
        spans, start = [], None
        for s in inside:
            if s['pressed'] and start is None: start = s
            elif not s['pressed'] and start is not None: spans.append((start, s)); start = None
        if start is not None: spans.append((start, {'frame': last}))
        wins = windows(grip, first, last)
        pressed = [(a_['frame'], b['frame']) for a_, b in spans]
        in_window = lambda f: any(w0 - 3 <= f <= w1 + 3 for w0, w1 in wins)
        ok_only = bool(pressed) and all(in_window(a_) and in_window(b - 1) for a_, b in pressed)
        gaps_ok = True
        ends = {b['frame']: b for _, b in spans}
        for (a0, b0), (a1, b1) in zip(pressed, pressed[1:]):
            same = any(w0 - 3 <= b0 and a1 <= w1 + 3 for w0, w1 in wins)
            if not same: continue
            end = ends.get(b0, {})
            unarmed = bool(end) and not end.get('binding') and not end.get('shield') and not end.get('two_hand_weapon')   # no longer a guard's weapon (both hands let go)
            gaps_ok &= (a1 - b0 <= 12) or unarmed or any(b0 - 2 <= m['frame'] <= a1 for m in melee) or any(b0 - 2 <= s['frame'] <= b0 + 2 for s in swings)
        firsts = []
        for w0, w1 in wins:
            s = next((x for x in inside if x['pressed'] and w0 - 3 <= x['frame'] <= w1), None)
            if s: firsts.append(s)
        parry_ok = bool(firsts) and all(s['parry'] for s in firsts)
        on = [x['frame'] for x in natives if x['blocking'] and first <= x['frame'] <= last]
        native_ok = bool(pressed) and any(pressed[0][0] <= f <= pressed[0][0] + 45 for f in on)
        # The character turns to the guard over a few frames once the look mode is the camera's (about 0.1 s): from 12
        # frames after a press on (Mock game-20261015-000828).
        looks = [(f, e) for f, e in look if any(a_ + 12 <= f < b for a_, b in pressed)]
        look_ok = None if not looks else all(abs((yaw(e['world_look']) - qyaw(e['camera_orientation']) + 180) % 360 - 180) <= 3 for _, e in looks)
        two_ok = (not two) or all(s['two_hand_weapon'] and not s['shield'] for s in inside if s['pressed'])
        for k, v in (('guard_only_with_grip', ok_only), ('gaps_explained', gaps_ok), ('grip_press_parry', parry_ok), ('native_blocking', native_ok),
                     ('look_is_the_guard', look_ok), ('two_hand_only', two_ok)):
            if v is None: unchecked.append(name + ' ' + k); continue   # no records of the look in this scope
            checks[name + ' ' + k] = v
        report[name] = dict(grip_windows=wins, pressed=pressed, parry_frames=[s['frame'] for s in inside if s['parry']], native_on=on[:8],
                            melee_presses=[m['frame'] for m in melee if first <= m['frame'] <= last], look_samples=len(looks))
    print(json.dumps(dict(status='PASS' if checks and all(checks.values()) else 'FAIL', checks=checks, unchecked=unchecked, scopes=report,
                          limitation='Mock, no enemy: the block button, the native Blocking state and the look are audited, not a blocked hit.'), indent=1))


if __name__ == '__main__':
    main()
