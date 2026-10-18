"""Audit the staff held with both hands (-TwoHandWieldVhvr with -RangedAim) of one Mock session.

Reference: docs/analysis/vhvr_spec_20261010/staff_throw.md 2.2, 2.3 and section 3: with both hands on the
staff the shot flies along the line from the rear hand to the front hand; with one hand along the pointer
ray. Pass the probe directory. The expected moves are those of tools/make_staff_two_hand_fixture.py.
"""
import argparse, csv, json, math, struct
from pathlib import Path
from check_capture_cameras import rotate, qmul
from check_ranged_aim import unit, degrees_between, euler, STAFF, WEAPON_PITCH

EMITTER, REAR_REACH, MAIN_BEHIND = .63, .6988, .1747     # ranged_aim.hpp, weapon_mount.hpp twoHandStaffProfile
HOLDS = [(580, 899), (1060, 1299), (1620, 1859)]   # while the left grip is held (user rule 2026-10-11)
CASTS = [(700, 789, 'left_behind'), (1140, 1229, 'right_behind'), (1420, 1509, 'one_hand')]
PROBE = (1700, 1789, 'left_behind')   # a fourth cast: the game may refuse it (run game013903); audited when it happens


def expected_line(h, kind, scale):
    """Origin of the model, its axis, the emitter and the shot direction in tracking space, from the hand script."""
    left, right = h[1:4], h[14:17]
    distance = math.dist(left, right)
    if kind == 'left_behind':
        axis = unit([r - l for l, r in zip(left, right)])
        to_main = min(distance, REAR_REACH * scale)
        origin = [l + a * to_main for l, a in zip(left, axis)]
        main_y, off_y = (distance - to_main) / scale, -to_main / scale
    elif kind == 'right_behind':
        axis = unit([l - r for l, r in zip(left, right)])
        to_main = MAIN_BEHIND * scale
        origin = [r + a * to_main for r, a in zip(right, axis)]
        main_y, off_y = -to_main / scale, (distance - to_main) / scale
    else:
        axis = rotate(qmul(euler(*h[17:20]), euler(0, WEAPON_PITCH, 0)), [0, 1, 0])
        origin, main_y, off_y = right, 0.0, 0.0
    emitter = [o + a * EMITTER * scale for o, a in zip(origin, axis)]
    direction = axis if kind != 'one_hand' else rotate(euler(*h[20:23]), [0, 0, -1])
    return axis, emitter, direction, main_y, off_y


def audit(probe):
    events = [json.loads(s) for s in (probe / 'view-resource-probe.jsonl').read_text(encoding='utf-8').splitlines()]
    ev = lambda name: [e for e in events if e['event'] == name]
    hands = {int(r[0]): list(map(float, r)) for r in csv.reader(
        s for s in (probe.parent / 'mock-hands.csv').read_text().splitlines() if s and not s.startswith('#'))}
    all_poses = ev('motion_visual_pose')
    poses = {(e['frame'], e['item']): e for e in all_poses}
    calls, casts, removed = ev('ranged_aim_call'), ev('ranged_cast_sample'), ev('bow_spawn_removed')
    expected = CASTS + [PROBE] if len(calls) > len(CASTS) else CASTS
    checks = dict(
        policy=any(x.get('entry') == 'vhvr' and x.get('left_grip_only') for x in ev('live_two_hand_wield_enabled')) and len(ev('ranged_aim_enabled')) == 1,
        casts=len(calls) == len(expected) == len(casts) and all(c['item'] == STAFF and c['path'] == 'cast_event' and c['applied'] and c['reason'] == 'applied' and
                                                           first <= c['frame'] <= last and c['two_handed'] == (kind != 'one_hand')
                                                           for c, (first, last, kind) in zip(calls, expected)),
        native_result=True, private_only=True, placement=True, weapon_relation=True,
        restored=len(removed) == 1 and removed[0]['restored'] and not removed[0]['failures'] and not removed[0]['callbacks'],
        completed=any(e['result'] == 0 for e in ev('view_resource_hook_complete')),
        no_failure=not any('failure' in e['event'] for e in events))
    # The hold itself: two-handed exactly while both grips are held along the staff.
    active = [x['frame'] for x in all_poses if x.get('wield_active')]
    wanted = {f for first, last in HOLDS for f in range(first, last + 1)}
    checks['active_frames'] = set(active) == wanted and len(active) == len(wanted) and \
        all(x['item'] == STAFF and x['mount_kind'] == 'live_vhvr_pitch' for x in all_poses if x.get('wield_active'))
    details, lines = [], []
    for c, x, (_, _, kind) in zip(calls, casts, expected):
        frame = c['frame']
        m = poses.get((frame, STAFF))
        if not m or frame not in hands:
            checks['native_result'] = False
            continue
        transform = bytes.fromhex(x['transform_bytes'])
        checks['native_result'] &= bool(x['result'])
        checks['private_only'] &= bool(x['sources_unchanged'] and x['tracked_aim'])
        position = struct.unpack_from('<3q', transform)
        forward = rotate(struct.unpack_from('<4f', transform, 24), [0, 0, 1])
        position_error = max(abs(p - q) for p, q in zip(position, c['aim_q32'])) / 2 ** 32
        direction_error = max(abs(v - w) for v, w in zip(forward, c['aim_direction']))
        checks['placement'] &= position_error < 3e-6 and direction_error < 3e-6

        scale = m['scale'][1]
        axis, emitter, direction, main_y, off_y = expected_line(hands[frame], kind, scale)
        # In the world: the weapon as shown this frame. The emitter is on its axis, and the shot keeps
        # the script's angle to that axis (zero with both hands, the pointer's 55 degrees with one).
        world_axis = rotate(m['orientation'], [0, 1, 0])
        offset = [(p - q) / 2 ** 32 for p, q in zip(c['aim_q32'], m['q32'])]
        emitter_error = max(abs(o - a * EMITTER * scale) for o, a in zip(offset, world_axis))
        angle_error = abs(degrees_between(c['aim_direction'], world_axis) - degrees_between(direction, axis))
        slide_error = max(abs(m['wield_main_y'] - main_y), abs(m['wield_off_y'] - off_y))
        checks['weapon_relation'] &= emitter_error < 2e-5 and angle_error < .005 and slide_error < 1e-5 and bool(m.get('wield_active')) == (kind != 'one_hand')
        lines.append((frame, c['aim_direction'], direction, [p / 2 ** 32 for p in c['aim_q32']], emitter))
        details.append(dict(kind=kind, frame=frame, two_handed=c['two_handed'], position_error_m=position_error, direction_error=direction_error,
                            emitter_error_m=emitter_error, shot_to_weapon_axis_degrees=degrees_between(c['aim_direction'], world_axis),
                            angle_error_degrees=angle_error, slide_error_m=slide_error))
    # Rigid mapping between tracking space and the world: the angles and distances between the casts
    # agree with the hand script, so each cast follows the line between the hands of its own frame.
    between = []
    for (f0, d0, p0, o0, e0), (f1, d1, p1, o1, e1) in zip(lines, lines[1:]):
        between.append(dict(frames=[f0, f1], world_degrees=degrees_between(d0, d1), script_degrees=degrees_between(p0, p1),
                            world_distance_m=math.dist(o0, o1), script_distance_m=math.dist(e0, e1)))
    checks['cast_to_cast'] = len(between) == len(expected) - 1 and all(
        abs(b['world_degrees'] - b['script_degrees']) < .005 and abs(b['world_distance_m'] - b['script_distance_m']) < 2e-5 for b in between) and \
        len(between) >= 2 and between[0]['script_degrees'] > 20 and between[1]['script_degrees'] > 20
    # The pad: the grips of the hold open no quick wheel, and the right trigger reaches the game for the cast.
    pads = ev('controller_live_sample')
    checks['no_wheel_while_two_handed'] = not any(x.get('wield_grip') for x in pads) and not any(x['buttons'] & 0x200 for x in pads if 560 <= x['frame'] <= 1900)
    checks['trigger_passes'] = all(any(x['triggers'][1] for x in pads if first <= x['frame'] <= last) for first, last, _ in CASTS + [PROBE])
    # The fourth cast: when the game does not start it, its own weapon state must have stayed at rest with the trigger delivered.
    probe_states = sorted({bytes.fromhex(x['weapon_bytes'])[0] for x in ev('input_bow_sample') if PROBE[0] <= x['frame'] <= PROBE[1] + 30})
    probe = 'cast' if len(calls) > len(CASTS) else 'not started by the game' if probe_states == [0] else 'unexplained'
    checks['fourth_cast_explained'] = probe != 'unexplained'
    return dict(status='PASS' if all(checks.values()) else 'FAIL', checks=checks, active_frames=[min(active), max(active), len(active)] if active else [],
                casts=details, between_casts=between, fourth_cast=probe, fourth_cast_native_states=probe_states,
                limitation='Mock, one reviewed staff. Cast cost, charge and the projectile itself are native and not audited. '
                           'The hold ends when either grip is released; other staff models and hardware are unverified.')


if __name__ == '__main__':
    a = argparse.ArgumentParser(description=__doc__)
    a.add_argument('probe', type=Path)
    v = a.parse_args()
    r = audit(v.probe)
    (v.probe / 'staff-two-hand-check.json').write_text(json.dumps(r, indent=2) + '\n', encoding='utf-8')
    print(json.dumps(r, indent=2))
    raise SystemExit(r['status'] != 'PASS')
