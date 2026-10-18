"""Audit live guard (-LiveGuard, with -LiveMelee): a raised shield holds the native block button.

Reference: docs/analysis/vhvr_spec_20261010/block.md 2.2/3.1 (ShieldBlock, Gesture) plus the user's
posture rule. Pass the probe directory. The expected moves are those of tools/make_live_guard_fixture.py.
"""
import argparse, csv, json, math
from pathlib import Path

SHIELD, SWORD = 691231505, 1125155603
HEAD = (0.0, 1.65, 0.0)   # Mock head, looking along +X


def audit(probe, offset=0, last=10 ** 9):
    # offset/last: the guard script placed inside a longer combined script.
    first_frame = offset + 371 if offset else 0
    scoped = lambda e: first_frame <= e['frame'] <= last
    events = [json.loads(s) for s in (probe / 'view-resource-probe.jsonl').read_text(encoding='utf-8').splitlines()]
    ev = lambda name: [e for e in events if e['event'] == name]
    hands = {int(r[0]): list(map(float, r)) for r in csv.reader(
        s for s in (probe.parent / 'mock-hands.csv').read_text().splitlines() if s and not s.startswith('#'))}
    states, natives, melee = ([e for e in ev(name) if scoped(e)] for name in ('live_guard_state', 'live_guard_native', 'live_melee_press'))
    checks = dict(enabled=len(ev('live_guard_enabled')) == 1,
                  completed=any(e['result'] == 0 for e in ev('view_resource_hook_complete')),
                  no_failure=not any('failure' in e['event'] for e in events))
    # Pressed intervals from the state log.
    intervals, start = [], None
    for s in states:
        if s['pressed'] and start is None:
            start = s
        elif not s['pressed'] and start is not None:
            intervals.append((start, s))
            start = None
    spans = [(a['frame'], b['frame']) for a, b in intervals]
    within = lambda value, low, high: low <= value <= high
    quick = 1 if ev('live_melee_tap_released') else 0  # -LiveMeleeEarly: the attack tap ends when the game has read it
    expected = [((566, 576), (718, 728)),     # A .. B
                ((906, 914), (1000, 1002)),   # D .. the push
                ((1001, 1008), (1100, 1102)), # after the parry gap .. the sword stroke
                ((1102 - quick, 1112), (1218, 1228))] # after the attack press .. G (one frame after a quick tap)
    expected = [((a + offset, b + offset), (c + offset, d + offset)) for (a, b), (c, d) in expected]
    checks['press_intervals'] = start is None and len(spans) == len(expected) and all(
        within(a, *first) and within(b, *last) for (a, b), (first, last) in zip(spans, expected))
    checks['shield_only'] = bool(states) and all(s['shield'] and not s['two_hand_weapon'] and s['binding'] >> 32 == SHIELD for s in states)
    # The posture rule rebuilt from the hand script: back of the left hand (-X) against the front (+X),
    # hand 0.55 m below the head at most and 0.15 m ahead at least.
    rule = True
    for first, _ in intervals:
        h = hands[first['frame']]
        # The body's front: level direction from the head to the point between the hands (live_guard.hpp,
        # since the headset run game100334). The back of the left hand is its -X: (-cos yaw, 0, sin yaw).
        mid = [(h[1] + h[14]) / 2 - HEAD[0], (h[3] + h[16]) / 2 - HEAD[2]]
        size = math.hypot(*mid)
        front = [mid[0] / size, mid[1] / size] if size >= .12 else [1.0, 0.0]
        yaw = math.radians(h[4])
        facing = -math.cos(yaw) * front[0] + math.sin(yaw) * front[1]
        ahead = (h[1] - HEAD[0]) * front[0] + (h[3] - HEAD[2]) * front[1]
        # The script reaches exactly 60 degrees and exactly 0.55 m on a frame boundary; allow float rounding there.
        rule &= facing > .5 - 1e-5 and h[2] - HEAD[1] >= -.55 - 1e-5 and ahead >= .15 and abs(first['facing'] - facing) < 2e-3
    for frame in range(600 + offset, 700 + offset):   # held while still
        rule &= any(a <= frame < b for a, b in spans)
    for frame in list(range(735 + offset, 900 + offset)) + list(range(1235 + offset, min(1340 + offset, last))):   # hanging arm or turned away: never
        rule &= not any(a <= frame < b for a, b in spans)
    checks['posture_rule'] = rule
    # Parry presses: raising into place (D) and the push (E). Nothing else.
    parries = [s['frame'] for s in states if s['parry']]
    checks['parry_presses'] = len(parries) == 2 and within(parries[0], 906 + offset, 914 + offset) and within(parries[1], 1000 + offset, 1002 + offset)
    gap = spans[2][0] - spans[1][1] if len(spans) > 2 else 0
    checks['parry_gap'] = 1 <= gap <= 6   # about 70 ms of wall time
    # Attack priority: the melee press of the stroke lies inside the released time.
    checks['attack_priority'] = len(melee) == 1 and within(melee[0]['frame'], 1100 + offset, 1103 + offset) and len(spans) > 3 and spans[2][1] <= melee[0]['frame'] + 1 and spans[3][0] > melee[0]['frame']

    # The pad: LB follows the pressed state, and nothing else presses LB.
    pads = [x for x in ev('controller_live_sample') if scoped(x)]
    edges, held = [], False
    for x in pads:
        down = bool(x['buttons'] & 0x100)
        if down and not held:
            edges.append(x['frame'])
        held = down
    checks['pad_block_button'] = len(edges) == len(expected) and all(abs(e - a) <= 3 for e, (a, _) in zip(edges, spans)) and not held

    # Native: the game's own Blocking state follows the button.
    on = [x for x in natives if x['blocking']]
    off = [x for x in natives if not x['blocking']]
    native = dict(blocking_on_frames=[x['frame'] for x in on], blocking_off_frames=[x['frame'] for x in off], can_block_when_on=all(x['can_block'] for x in on))
    checks['native_blocking'] = (any(within(x['frame'], spans[0][0], spans[0][0] + 45) for x in on) if spans else False) and \
        (any(within(x['frame'], spans[1][0], spans[1][0] + 45) for x in on) if len(spans) > 1 else False) and \
        bool(natives) and not natives[-1]['blocking'] and all(any(a - 2 <= x['frame'] <= b + 45 for a, b in spans) for x in on)
    return dict(status='PASS' if all(checks.values()) else 'FAIL', checks=checks, pressed_frames=spans, parry_frames=parries,
                pad_edges=edges, melee_press_frames=[m['frame'] for m in melee], native=native,
                limitation='Mock, one shield with one sword, no enemy: the block button and the native Blocking state are audited, not a blocked or '
                           'parried hit. The attack side is assumed in front of the head. Two-hand weapon guard and hardware feel are unverified.')


if __name__ == '__main__':
    a = argparse.ArgumentParser(description=__doc__)
    a.add_argument('probe', type=Path)
    a.add_argument('--offset', type=int, default=0)
    a.add_argument('--last', type=int, default=10 ** 9)
    v = a.parse_args()
    r = audit(v.probe, v.offset, v.last)
    (v.probe / 'live-guard-check.json').write_text(json.dumps(r, indent=2) + '\n', encoding='utf-8')
    print(json.dumps(r, indent=2))
    raise SystemExit(r['status'] != 'PASS')
