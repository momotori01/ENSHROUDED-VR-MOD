"""Audit the body heading (-BodyHeading) of one Mock session walked with tools/make_walk_fixture.py.

VHVR: the body follows the headset's yaw. Pass the probe directory. From the local actor snapshots (read-only copies
at the game's own query boundary) the check takes the yaw of the body and of the camera while the stick is held
forward, back, left and right. With the body heading the body stays on the camera's yaw in all four; without it
(--baseline) the game turns the body toward the way it walks, and the check only reports the angles.
"""
import argparse, json, math, struct
from pathlib import Path

PARTS = dict(forward=(500, 619), back=(700, 819), left=(900, 1019), right=(1100, 1219))


def yaw(q):
    x, y, z, w = q
    fx, fz = 2 * (x * z + w * y), 1 - 2 * (x * x + y * y)
    return math.degrees(math.atan2(fx, fz))


def difference(a, b):
    return (a - b + 180) % 360 - 180


def audit(probe, baseline):
    events = [json.loads(s) for s in (probe / 'view-resource-probe.jsonl').read_text(encoding='utf-8').splitlines()]
    ev = lambda name: [e for e in events if e['event'] == name]
    pads = ev('controller_live_sample')
    frame_at = lambda us: next((p['frame'] for p in pads if p['us'] >= us), None)
    rows = []
    for e in ev('action_sequence_snapshot'):
        c = e.get('native_look_context')
        if not c:
            continue
        body = bytes.fromhex(c['body_transform_bytes'])
        rows.append(dict(us=e['us'], body=yaw(struct.unpack_from('<4f', body, 24)), camera=yaw(e['camera_orientation']),
                         look=math.degrees(math.atan2(e['world_look'][0], e['world_look'][2])), mode=c['mode'], move=c['world_move']))
    # Frames from the pad log (same clock): each snapshot takes the frame of the next pad sample.
    index = 0
    for r in rows:
        while index < len(pads) and pads[index]['us'] < r['us']:
            index += 1
        r['frame'] = pads[index]['frame'] if index < len(pads) else None
    checks = dict(completed=any(e['result'] == 0 for e in ev('view_resource_hook_complete')),
                  no_failure=not any('failure' in e['event'] for e in events), snapshots=len(rows) > 200)
    enabled, applied, removed = ev('body_heading_enabled'), ev('body_heading_applied'), ev('body_heading_removed')
    parts = {}
    for name, (first, last) in PARTS.items():
        inside = [r for r in rows if r['frame'] is not None and first + 40 <= r['frame'] <= last]   # after the turn has settled
        moving = [r for r in inside if any(abs(v) > 1e-4 for v in r['move'])]
        if not inside:
            parts[name] = None
            continue
        off = [abs(difference(r['body'], r['camera'])) for r in inside]
        look = [abs(difference(r['look'], r['camera'])) for r in inside]
        parts[name] = dict(samples=len(inside), moving=len(moving), body_from_camera_degrees=[round(min(off), 1), round(sum(off) / len(off), 1), round(max(off), 1)],
                           look_from_camera_degrees=round(max(look), 2))
    checks['walked_all_four_ways'] = all(v and v['moving'] >= 10 for v in parts.values())
    if baseline:
        checks['not_enabled'] = not enabled and not applied
    else:
        checks['enabled'] = len(enabled) == 1 and bool(applied)
        checks['removed_cleanly'] = len(removed) == 1 and removed[0]['outstanding'] == 0 and removed[0]['failures'] == 0 and removed[0]['lost_scopes'] == 0
        checks['look_is_the_camera'] = all(v and v['look_from_camera_degrees'] < 1 for v in parts.values())
        checks['body_faces_the_view'] = all(v and v['body_from_camera_degrees'][2] < 25 for v in parts.values())
    return dict(status='PASS' if all(checks.values()) else 'FAIL', checks=checks, parts=parts, applied=removed[0] if removed else None,
                native_modes=sorted(set(r['mode'] for r in rows)),
                limitation='Mock, still headset. The snapshot log holds the first 4096 samples (about 68 s). How the legs look is judged from images or on the headset.')


if __name__ == '__main__':
    a = argparse.ArgumentParser(description=__doc__)
    a.add_argument('probe', type=Path)
    a.add_argument('--baseline', action='store_true')
    v = a.parse_args()
    r = audit(v.probe, v.baseline)
    (v.probe / 'body-heading-check.json').write_text(json.dumps(r, indent=2) + '\n', encoding='utf-8')
    print(json.dumps(r, indent=2))
    raise SystemExit(r['status'] != 'PASS')
