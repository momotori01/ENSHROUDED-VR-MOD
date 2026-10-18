"""Audit the two-hand hold (-TwoHandWieldVhvr: left grip, hands along the weapon) with -LiveMelee and -LiveGuard.

Reference: docs/analysis/vhvr_spec_20261010/melee.md 2.2-2.4 (entry, pose from both hands, default slide),
3.2 (two-hand momentum) and block.md 4.1 (weapon block). Pass the probe directory. The expected moves are
those of tools/make_two_hand_fixture.py.
"""
import argparse, csv, json, math, sys
from pathlib import Path
from check_recorded_xr_native import rotate

SWORD = 1125155603
WEAPONS = dict(axe=1940764111, hammer=3936779840)
# main grip along the model axis (weapon_mount.hpp): measured per weapon, or the catalog group's value
GRIP_Y = dict(axe=dict(measured=0.15, catalog=0.15), hammer=dict(measured=0.15, catalog=0.15))   # twoHandMeleeVrGripY since 2026-10-11 10:40 (0.6334 in the game stance before)
MOUNT = dict(measured='native_melee_right_grip', catalog='catalog_melee_right_grip')
LEFT_BEHIND, RIGHT_BEHIND = (620, 1399), (1500, 1599)
WHEEL = (1345, 1374)             # right grip while two-handed: the quick wheel opens and the hold pauses
# Since 2026-10-13 the weapon hand's own stroke counts in a two-hand hold and a heavy weapon held with both hands
# asks for 3 m/s like the others (include/evr/live_melee.hpp; user after the headset run game-20261013-105615). The
# script's first stroke, 4.23 m/s at 700 - too slow for the 4.5 m/s of before - attacks now.
PRESSES = [(700, 703), (800, 803), (900, 903), (1200, 1203)]
REQUIRED = 3.0
DT = 1 / 90


def audit(probe, offset=0, last=10 ** 9, mount='measured', weapon='axe', grip=False):
    AXE = WEAPONS[weapon]   # the two-hand weapon of the session
    # offset/last: the two-hand script placed inside a longer combined script.
    first_frame = offset + 371 if offset else 0
    events = [json.loads(s) for s in (probe / 'view-resource-probe.jsonl').read_text(encoding='utf-8').splitlines()]
    # Since 2026-10-13 what is shown takes the hands a little smoothed (include/evr/pose_smoothing.hpp, event
    # hand_smoothing), and the displayed geometry then trails the scripted hands by one or two centimetres while they
    # move. The figures below are for the hands as tracked: run this audit on a trial with EVR_HAND_SMOOTHING=0.
    smoothing = [e for e in events if e.get('event') == 'hand_smoothing']
    if smoothing and smoothing[-1].get('cutoff_hz', 0) > 0:
        print('note: this trial showed smoothed hands; the display checks expect EVR_HAND_SMOOTHING=0', file=sys.stderr)
    every = lambda name: [e for e in events if e['event'] == name]
    ev = lambda name: [e for e in events if e['event'] == name and first_frame <= e['frame'] <= last]
    left_behind, right_behind = [tuple(f + offset for f in span) for span in (LEFT_BEHIND, RIGHT_BEHIND)]
    script = {int(r[0]): list(map(float, r)) for r in csv.reader(
        s for s in (probe.parent / 'mock-hands.csv').read_text().splitlines() if s and not s.startswith('#'))}
    poses = ev('motion_visual_pose')
    active = [x for x in poses if x.get('wield_active')]
    hand_poses = {x['serial']: x for x in ev('motion_hand_pose')}
    anchors = {x['recorded_xr_frame']: x for x in every('recorded_xr_anchor')}
    within = lambda value, low, high: low <= value <= high
    checks = dict(policy=any(x.get('entry') == 'vhvr' and x.get('left_grip_only') and x.get('catalog_only') == (mount == 'catalog') and
                             x['display_only'] and not x['combat_authorized'] for x in every('live_two_hand_wield_enabled')),
                  completed=any(e['result'] == 0 for e in every('view_resource_hook_complete')),
                  no_failure=not any('failure' in e['event'] for e in events))

    # 1. Entry and exit: two-handed exactly while the left grip is held with the hands along the weapon.
    # The open quick wheel pauses the hold; when it closes is the frontend's timing, so that stretch is free.
    wanted = set(range(left_behind[0], left_behind[1] + 1)) | set(range(right_behind[0], right_behind[1] + 1))
    free = set(range(WHEEL[0] + offset, WHEEL[1] + offset + 16))
    frames = [x['frame'] for x in active]
    checks['active_frames'] = set(frames) <= wanted and set(frames) - free == wanted - free and len(frames) == len(set(frames))
    checks['axe_only'] = bool(active) and all(x['item'] == AXE and x['mount_kind'] == MOUNT[mount] and x['grip_hand'] == 1 for x in active)

    # 2. Pose: the weapon axis passes through both hands; VHVR default slide.
    errors = dict(left_m=0., right_m=0., distance_m=0., slide_m=0.)
    evidence = bool(active)
    for x in active:
        h, a, s = hand_poses.get(x['serial']), anchors.get(x['frame']), script.get(x['frame'])
        if not h or not a or not s:
            evidence = False
            continue
        scale = x['scale'][1]
        distance = math.dist(s[1:4], s[14:17])
        grip = GRIP_Y[weapon][mount] * scale
        rear = (grip - min(distance, grip)) / scale          # model coordinate of the rear hand
        front = rear + distance / scale
        main, off = (front, rear) if x['frame'] <= left_behind[1] else (rear, front)   # main = right hand
        errors['distance_m'] = max(errors['distance_m'], abs(x['wield_distance'] - distance))
        errors['slide_m'] = max(errors['slide_m'], abs(x['wield_main_y'] - main), abs(x['wield_off_y'] - off))
        for hand, y, label in ((0, x['wield_off_y'], 'left_m'), (1, x['wield_main_y'], 'right_m')):
            v = rotate(x['orientation'], [0, y * scale, 0])
            actual = [(u - c) / 2 ** 32 + d for u, c, d in zip(x['q32'], a['camera_q32'], v)]
            target = rotate(a['orientation'], [u + c for u, c in zip(h['grips'][hand][:3], a['camera_offset'])])
            errors[label] = max(errors[label], math.dist(actual, target))
        evidence &= x['source_unchanged'] and h['source_unchanged'] and x['native_mask'] == 0 and x['sdf_handle'] == 0 and h['frame'] == x['frame']
    checks['complete_evidence'] = evidence
    checks['both_hands_on_axis'] = bool(active) and max(errors['left_m'], errors['right_m']) < 1e-4
    checks['vhvr_slide'] = bool(active) and errors['distance_m'] < 1e-5 and errors['slide_m'] < 1e-5

    # 3. The pad: the hold needs no right grip and claims none, so the right grip is the quick wheel throughout.
    pads = ev('controller_live_sample')
    claimed = [x for x in pads if x.get('wield_grip')]
    wheel = (WHEEL[0] + offset, WHEEL[1] + offset)
    checks['no_wheel_without_right_grip'] = not claimed and not any(x['buttons'] & 0x200 for x in pads if 600 + offset <= x['frame'] <= 1680 + offset and not wheel[0] <= x['frame'] <= wheel[1] + 3) and \
        any(x['raw_grips'] == [1, 0] and 700 + offset <= x['frame'] <= 1300 + offset for x in pads)
    inactive = {x['frame'] for x in poses if not x.get('wield_active') and x['item'] == AXE}
    checks['wheel_pauses_hold'] = any(x['buttons'] & 0x200 and wheel[0] <= x['frame'] <= wheel[1] + 3 for x in pads) and \
        len(inactive & set(range(wheel[0], wheel[1] + 1))) >= 10 and set(range(wheel[1] + 16, left_behind[1] + 1)) <= set(frames)
    checks['wheel_with_hands_across'] = any(x['buttons'] & 0x200 and x['raw_grips'] == [1, 1] and not x.get('wield_grip') and 1700 + offset <= x['frame'] <= 1760 + offset for x in pads)
    checks['sword_after_wheel'] = any(x['item'] == SWORD and x['frame'] >= 1770 + offset for x in poses) and not any(x['item'] == AXE and x['frame'] >= 1800 + offset for x in poses)

    # 4. Attacks: VHVR two-hand momentum (direction of both hand velocities summed, the faster hand counts), or the
    # weapon hand's own stroke where that is the faster (the script's strokes do not turn the hand).
    presses = ev('live_melee_press')
    stroke_ok = len(presses) == len(PRESSES)
    details = []
    for e, (first, final) in zip(presses, PRESSES):
        s0, s1 = script[e['frame'] - 1], script[e['frame']]
        left = [(b - a) / DT for a, b in zip(s0[1:4], s1[1:4])]
        right = [(b - a) / DT for a, b in zip(s0[14:17], s1[14:17])]
        total = [u + v for u, v in zip(left, right)]
        size = math.sqrt(sum(v * v for v in total))
        speed = max(sum(u * v for u, v in zip(left, total)), sum(u * v for u, v in zip(right, total))) / size if size else 0
        speed = max(speed, math.sqrt(sum(v * v for v in right)))
        stroke_ok &= within(e['frame'], first + offset, final + offset) and e['binding'] >> 32 == AXE and e['heavy'] and e['two_handed'] and not e['thrust'] and \
            abs(e['required'] - REQUIRED) < 1e-6 and abs(e['speed'] - speed) < 2e-3 and speed > REQUIRED
        details.append(dict(frame=e['frame'], speed=e['speed'], script_speed=speed, right_hand_speed=math.sqrt(sum(v * v for v in right))))
    checks['two_hand_strokes'] = stroke_ok
    checks['left_hand_alone_counts'] = len(details) > 2 and details[2]['right_hand_speed'] < 1e-6
    natives, starts, previous = ev('live_melee_native'), [], None
    for x in natives:
        if x['active'] and (previous is None or not previous['active'] or previous['trigger_time'] != x['trigger_time']):
            starts.append(x)
        previous = x
    # With a two-hand weapon the native block is an action of the weapon itself (run game011308: its own
    # sequence, starting on the frame the Blocking state turns on). Those are not attacks.
    block_frames = {x['frame'] for x in ev('live_guard_native') if x['blocking']}
    block_actions = [x for x in starts if x['frame'] in block_frames]
    starts = [x for x in starts if x['frame'] not in block_frames]
    checks['native_attack_per_press'] = len(starts) == len(presses) and all(x['used_item'] == AXE and 0 <= x['us'] - e['us'] <= 1500000 for x, e in zip(starts, presses)) and \
        not {x['sequence'] for x in starts} & {x['sequence'] for x in block_actions}
    # The attack sequence may differ between presses: a press inside the game's combo window starts the
    # next attack of the chain (run game012647, second stroke). It only must not be the block action.

    # 5. Guard: the weapon held level across the body with both hands raised holds the native block.
    states = ev('live_guard_state')
    intervals, start = [], None
    for s in states:
        if s['pressed'] and start is None:
            start = s
        elif not s['pressed'] and start is not None:
            intervals.append((start, s))
            start = None
    spans = [(a['frame'], b['frame']) for a, b in intervals]
    expected = [((1041, 1065), (1200, 1203)), ((1201, 1225), (1320, 1336))]
    expected = [((a + offset, b + offset), (c + offset, d + offset)) for (a, b), (c, d) in expected]
    checks['guard_intervals'] = start is None and len(spans) == len(expected) and all(
        within(a, *first) and within(b, *final) for (a, b), (first, final) in zip(spans, expected))
    checks['guard_weapon_only'] = bool(states) and all(s['two_hand_weapon'] and not s['shield'] and s['binding'] >> 32 == AXE for s in states if s['pressed'] or s['posture'])
    rule = bool(intervals)
    for first, _ in intervals:
        s = script[first['frame']]
        axis = [r - l for l, r in zip(s[1:4], s[14:17])]
        angle = math.degrees(math.acos(axis[0] / math.sqrt(sum(v * v for v in axis))))   # against the head's +X
        rule &= 60 < angle < 120 and min(s[2], s[15]) - 1.65 >= -.55 and min(s[1], s[14]) >= .15 and abs(first['facing'] - angle) < .05
    checks['guard_posture_rule'] = rule
    checks['attack_priority'] = len(spans) == 2 and len(presses) == 4 and spans[0][1] <= presses[3]['frame'] + 1 and spans[1][0] > presses[3]['frame']
    guard_native = ev('live_guard_native')
    on = [x['frame'] for x in guard_native if x['blocking']]
    checks['native_blocking'] = bool(spans) and any(within(f, spans[0][0], spans[0][0] + 45) for f in on) and bool(guard_native) and not guard_native[-1]['blocking'] and \
        all(any(a - 2 <= f <= b + 45 for a, b in spans) for f in on)
    if grip:   # the grip rule of the guard (2026-10-14): tools/check_guard_grip.py audits the guard; the pose rule's parts are left out
        for k in ('guard_intervals', 'guard_posture_rule', 'attack_priority', 'native_blocking', 'guard_weapon_only'): checks.pop(k, None)
    return dict(status='PASS' if all(checks.values()) else 'FAIL', checks=checks, active_frames=[min(frames), max(frames), len(frames)] if frames else [],
                max_errors=errors, grip_claim_samples=len(claimed), melee_presses=details,
                native_attacks=[dict(frame=x['frame'], used_item=x['used_item'], sequence=x['sequence']) for x in starts],
                native_block_actions=[dict(frame=x['frame'], used_item=x['used_item'], sequence=x['sequence']) for x in block_actions], guard_pressed_frames=spans,
                guard_parry_frames=[s['frame'] for s in states if s['parry']], native_blocking_on_frames=on,
                limitation='Mock, one measured axe, no enemy. The two-hand hold is a private display; attacks and the block go through the native buttons, '
                           'and hits, damage and stamina are native and not audited. Non-sticky hold only (either grip released ends it); '
                           'VHVR per-class slides (battle axe, polearm), the front/rear swap and hardware feel are unverified.')


if __name__ == '__main__':
    a = argparse.ArgumentParser(description=__doc__)
    a.add_argument('probe', type=Path)
    a.add_argument('--offset', type=int, default=0)
    a.add_argument('--grip', action='store_true', help='the guard runs by the grip rule (check_guard_grip.py audits it)')
    a.add_argument('--last', type=int, default=10 ** 9)
    a.add_argument('--mount', choices=('measured', 'catalog'), default='measured', help='catalog: the session ran with -TwoHandCatalogOnly')
    a.add_argument('--weapon', choices=tuple(WEAPONS), default='axe', help='the weapon selected by the hand script (make_two_hand_fixture.py --weapon)')
    v = a.parse_args()
    r = audit(v.probe, v.offset, v.last, v.mount, v.weapon, v.grip)
    (v.probe / 'two-hand-check.json').write_text(json.dumps(r, indent=2) + '\n', encoding='utf-8')
    print(json.dumps(r, indent=2))
    raise SystemExit(r['status'] != 'PASS')
