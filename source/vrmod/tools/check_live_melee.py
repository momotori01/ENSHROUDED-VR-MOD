"""Audit live melee (-LiveMelee): VHVR-style swings become single native attack presses.

Reference: docs/analysis/vhvr_spec_20261010/melee.md 3.2/3.4 (hasMomentum). Pass the probe directory.
The expected strokes are those of tools/make_live_melee_fixture.py.
"""
import argparse, csv, json, math
from pathlib import Path
from check_capture_cameras import rotate, qmul

SWORD, AXE = 1125155603, 1940764111
DT = 1 / 90                      # Mock predicted time per hand frame
LEVER, WEAPON_PITCH = .125, -35.0
# (first frame, last frame, item, heavy, thrust) of every stroke that must press, and windows that must not.
PRESSES = [(560, 563, SWORD, False, False), (760, 763, SWORD, False, False), (1260, 1263, AXE, True, True), (1360, 1363, AXE, True, False)]
QUIET = [(600, 755), (776, 1255), (1270, 1355), (1370, 10 ** 9)]


def euler(yaw, pitch, roll):
    y, p, r = [math.radians(t) / 2 for t in (yaw, pitch, roll)]
    return qmul(qmul([0, math.sin(y), 0, math.cos(y)], [math.sin(p), 0, 0, math.cos(p)]), [0, 0, math.sin(r), math.cos(r)])


def cross(a, b):
    return [a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]]


def audit(probe, offset=0, last=10 ** 9):
    # offset/last: the stroke script placed inside a longer combined script (frames shifted by offset, ending at last).
    first_frame = offset + 371 if offset else 0
    pressed_at = [(a + offset, b + offset, item, heavy, thrust) for a, b, item, heavy, thrust in PRESSES]
    quiet = [(a + offset, min(b + offset, last)) for a, b in QUIET]
    scoped = lambda e: first_frame <= e['frame'] <= last
    events = [json.loads(s) for s in (probe / 'view-resource-probe.jsonl').read_text(encoding='utf-8').splitlines()]
    ev = lambda name: [e for e in events if e['event'] == name]
    hands = {int(r[0]): list(map(float, r)) for r in csv.reader(
        s for s in (probe.parent / 'mock-hands.csv').read_text().splitlines() if s and not s.startswith('#'))}
    presses, natives = [e for e in ev('live_melee_press') if scoped(e)], [e for e in ev('live_melee_native') if scoped(e)]
    checks = dict(enabled=len(ev('live_melee_enabled')) == 1, press_count=len(presses) == len(pressed_at), strokes=True, momentum=True,
                  completed=any(e['result'] == 0 for e in ev('view_resource_hook_complete')),
                  no_failure=not any('failure' in e['event'] for e in events))
    details = []
    for e, (first, final, item, heavy, thrust) in zip(presses, pressed_at):
        frame = e['frame']
        checks['strokes'] &= first <= frame <= final and e['binding'] >> 32 == item and e['heavy'] == heavy and e['thrust'] == thrust and not e['two_handed']
        # VHVR one-hand momentum rebuilt from the hand script: hand velocity plus rotation at the 12.5 cm lever.
        h0, h1 = hands[frame - 1], hands[frame]
        linear = [(b - a) / DT for a, b in zip(h0[14:17], h1[14:17])]
        turn = math.radians(h1[17] - h0[17]) / DT   # the script only turns the grip about tracking Y
        forward = e['forward']
        velocity = [v + c for v, c in zip(linear, cross([0, turn, 0], [f * LEVER for f in forward]))]
        speed = math.sqrt(sum(v * v for v in velocity))
        mount_axis = rotate(qmul(euler(*h1[17:20]), euler(0, WEAPON_PITCH, 0)), [0, 1, 0])
        axis_degrees = math.degrees(math.acos(max(-1, min(1, sum(a * b for a, b in zip(forward, mount_axis))))))
        required = 3 * (1.5 if heavy else 1)
        along = sum(v * f for v, f in zip(velocity, forward))
        is_thrust = speed > 0 and math.degrees(math.acos(max(-1, min(1, along / speed)))) < 30 and along >= 4
        checks['momentum'] &= (abs(speed - e['speed']) < 2e-3 and abs(e['required'] - required) < 1e-6 and (speed > required or is_thrust) and
                               is_thrust == thrust and abs(math.sqrt(sum(f * f for f in forward)) - 1) < 1e-5 and
                               axis_degrees < (1e-3 if item == SWORD else 15))
        details.append(dict(frame=frame, item=item, speed=e['speed'], script_speed=speed, required=required, thrust=thrust, weapon_axis_from_mount_degrees=axis_degrees))
    checks['no_press_elsewhere'] = not any(any(a <= e['frame'] <= b for a, b in quiet) for e in presses)

    # The pad: one X pulse of about 120 ms per press, nothing else on X.
    pads = [x for x in ev('controller_live_sample') if scoped(x)]
    pulses, start = [], None
    for x in pads:
        down = bool(x['buttons'] & 0x4000)
        if down and start is None:
            start = x
        elif not down and start is not None:
            pulses.append((start['frame'], x['us'] - start['us']))
            start = None
    # With the early Impact (-LiveMeleeEarly) the tap ends as soon as the game has read it; otherwise it lasts 120 ms.
    quick = [e for e in ev('live_melee_tap_released') if scoped(e)]
    low, high = (8000, 80000) if quick else (90000, 220000)
    checks['pad_pulses'] = len(pulses) == len(pressed_at) and all(low <= us <= high for _, us in pulses) and (not quick or len(quick) == len(pulses)) and \
        all(a <= frame <= b + 3 for (frame, _), (a, b, *_) in zip(pulses, pressed_at))

    # Native: the game's own action sequence starts once per press, with the weapon of that press.
    starts, previous = [], None
    for x in natives:
        if x['active'] and (previous is None or not previous['active'] or previous['trigger_time'] != x['trigger_time']):
            starts.append(x)
        previous = x
    native_ok = len(starts) == len(pressed_at)
    for x, e, (_, _, item, *_) in zip(starts, presses, pressed_at):
        native_ok &= x['used_item'] == item and 0 <= x['us'] - e['us'] <= 1500000
    checks['native_attack_per_press'] = native_ok

    # VHVR pulse on the weapon hand when the attack starts: right hand, 0.9 since 2026-10-14 (headset run 6, user: "the
    # vibration of a hit may be stronger"; 0.5 before).
    sent = [json.loads(line) for line in (probe.parent / 'native-xr' / 'openxr.jsonl').read_text(encoding='utf-8').splitlines() if '"native_gesture_haptic"' in line]
    frames = [x['frame'] for x in sent if .89 <= x['amplitude'][1] <= .91 and x['delivered'][1] and scoped(x)]
    bursts = [f for f in frames if f - 1 not in frames]
    checks['haptic_per_press'] = len(bursts) == len(pressed_at) and all(0 <= b - e['frame'] <= 10 for b, e in zip(bursts, presses))
    return dict(status='PASS' if all(checks.values()) else 'FAIL', checks=checks, presses=details, pad_pulses=pulses,
                native_starts=[dict(frame=x['frame'], used_item=x['used_item'], sequence=x['sequence']) for x in starts], haptic_bursts=bursts,
                limitation='Mock, one sword and one axe, one hand. The stroke starts a native attack; the hit test, damage, stamina and combo are native and '
                           'not audited, and no enemy was present. Two-hand strokes and hardware feel are unverified.')


if __name__ == '__main__':
    a = argparse.ArgumentParser(description=__doc__)
    a.add_argument('probe', type=Path)
    a.add_argument('--offset', type=int, default=0)
    a.add_argument('--last', type=int, default=10 ** 9)
    v = a.parse_args()
    r = audit(v.probe, v.offset, v.last)
    (v.probe / 'live-melee-check.json').write_text(json.dumps(r, indent=2) + '\n', encoding='utf-8')
    print(json.dumps(r, indent=2))
    raise SystemExit(r['status'] != 'PASS')
