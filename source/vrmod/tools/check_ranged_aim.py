"""Audit the VHVR-style staff/wand aim (-RangedAim) of one Mock session.

Reference: docs/analysis/vhvr_spec_20261010/staff_throw.md section 3 (one-handed staff:
shot from a point on the weapon axis, along the hand's pointer ray). Pass the probe directory.
"""
import argparse, csv, json, math, struct
from pathlib import Path
from check_capture_cameras import rotate, qmul

STAFF, WAND = 1255239784, 3851016100
EMITTER = {STAFF: .63, WAND: .5228}
WEAPON_PITCH = -35.0   # run_play_session default -WeaponPitch, applied by mountLiveWeapon


def unit(v):
    n = math.sqrt(sum(x * x for x in v))
    return [x / n for x in v]


def degrees_between(u, v):
    return math.degrees(math.acos(max(-1, min(1, sum(a * b for a, b in zip(unit(u), unit(v)))))))


def euler(yaw, pitch, roll):
    y, p, r = [math.radians(t) / 2 for t in (yaw, pitch, roll)]
    return qmul(qmul([0, math.sin(y), 0, math.cos(y)], [math.sin(p), 0, 0, math.cos(p)]), [0, 0, math.sin(r), math.cos(r)])


def audit(probe, expected):
    events = [json.loads(s) for s in (probe / 'view-resource-probe.jsonl').read_text(encoding='utf-8').splitlines()]
    ev = lambda name: [e for e in events if e['event'] == name]
    hands = {int(r[0]): list(map(float, r)) for r in csv.reader(
        s for s in (probe.parent / 'mock-hands.csv').read_text().splitlines() if s and not s.startswith('#'))}
    poses = {(e['frame'], e['item']): e for e in ev('motion_visual_pose')}
    calls = ev('ranged_aim_call')
    casts = ev('ranged_cast_sample')
    spawns = {e['sample']: e for e in ev('bow_spawn_sample')}
    removed = ev('bow_spawn_removed')
    checks = dict(
        enabled=len(ev('ranged_aim_enabled')) == 1,
        shots=[(c['item'], c['path']) for c in calls] == expected and all(c['applied'] and c['reason'] == 'applied' for c in calls),
        native_result=True, private_only=True, placement=True, weapon_relation=True,
        restored=len(removed) == 1 and removed[0]['restored'] and not removed[0]['failures'] and not removed[0]['callbacks'],
        completed=any(e['result'] == 0 for e in ev('view_resource_hook_complete')),
        no_failure=not any('failure' in e['event'] for e in events))
    details, lines = [], []
    cast_index = 0
    for c in calls:
        item, frame = c['item'], c['frame']
        if c['path'] == 'cast_event':
            x = casts[cast_index] if cast_index < len(casts) else None
            cast_index += 1
        else:
            x = spawns.get(c['sample'])
        m = poses.get((frame, item))
        if not x or not m or frame not in hands:
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

        # From the hand script alone (tracking space): model = right grip pitched by the mount,
        # emitter on the model's +Y, direction = aim pose -Z.
        h = hands[frame]
        model_q = qmul(euler(*h[17:20]), euler(0, WEAPON_PITCH, 0))
        axis = rotate(model_q, [0, 1, 0])
        length = EMITTER[item] * m['scale'][1]
        emitter = [p + a * length for p, a in zip(h[14:17], axis)]
        pointer = rotate(euler(*h[20:23]), [0, 0, -1])
        # In the world: the same weapon as shown this frame.
        world_axis = rotate(m['orientation'], [0, 1, 0])
        offset = [(p - q) / 2 ** 32 for p, q in zip(c['aim_q32'], m['q32'])]
        emitter_error = max(abs(o - a * length) for o, a in zip(offset, world_axis))
        angle_error = abs(degrees_between(c['aim_direction'], world_axis) - degrees_between(pointer, axis))
        checks['weapon_relation'] &= emitter_error < 2e-5 and angle_error < .005 and m['mount_kind'] == 'live_vhvr_pitch'
        lines.append((frame, c['aim_direction'], pointer, [p / 2 ** 32 for p in c['aim_q32']], emitter))
        details.append(dict(item=item, path=c['path'], frame=frame, position_error_m=position_error, direction_error=direction_error,
                            emitter_error_m=emitter_error, pointer_to_weapon_axis_degrees=degrees_between(pointer, axis), angle_error_degrees=angle_error))
    # Rigid mapping between tracking space and the world: angles and distances between shots agree.
    between = []
    for (f0, d0, p0, o0, e0), (f1, d1, p1, o1, e1) in zip(lines, lines[1:]):
        between.append(dict(frames=[f0, f1], world_degrees=degrees_between(d0, d1), script_degrees=degrees_between(p0, p1),
                            world_distance_m=math.dist(o0, o1), script_distance_m=math.dist(e0, e1)))
    checks['shot_to_shot'] = len(between) == len(expected) - 1 and all(
        abs(b['world_degrees'] - b['script_degrees']) < .005 and abs(b['world_distance_m'] - b['script_distance_m']) < 2e-5 for b in between)

    # Input: with the wand the right trigger is the native attack press (X) and RT stays 0.
    wand_frames = [c['frame'] for c in calls if c['item'] == WAND]
    pads = [x for x in ev('controller_live_sample') if wand_frames and min(wand_frames) - 60 <= x['frame'] <= max(wand_frames) + 60]
    presses, held = 0, False
    for x in pads:
        down = bool(x['buttons'] & 0x4000)
        presses += down and not held
        held = down
    wand_count = sum(item == WAND for item, _ in expected)
    checks['wand_trigger_is_attack'] = presses == wand_count and not any(x['triggers'][1] for x in pads if x['frame'] >= min(wand_frames) - 40)
    return dict(status='PASS' if all(checks.values()) else 'FAIL', checks=checks, shots=details, between_shots=between,
                limitation='Mock, one reviewed staff and one reviewed wand, held in one hand. Cast cost, charge and the projectile itself are native and '
                           'not audited. Two-handed staff aiming, other models and hardware are unverified.')


if __name__ == '__main__':
    a = argparse.ArgumentParser(description=__doc__)
    a.add_argument('probe', type=Path)
    v = a.parse_args()
    r = audit(v.probe, [(STAFF, 'cast_event'), (STAFF, 'cast_event'), (WAND, 'spawn_event'), (WAND, 'spawn_event')])
    (v.probe / 'ranged-aim-check.json').write_text(json.dumps(r, indent=2) + '\n', encoding='utf-8')
    print(json.dumps(r, indent=2))
    raise SystemExit(r['status'] != 'PASS')
