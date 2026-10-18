#!/usr/bin/env python3
"""What the strokes of a live melee contact session hit (-LiveMeleeContactObserve): the game's own hit commits, where
each target is, and how its Health changed.

The observers are read-only: motion_owned_hit_commit is the game committing a hit of the local player's Impact,
motion_owned_health_delta is the game's own Health change of that target. Nothing here is written by the mod; the
only thing the mod supplies is the pose of the attack's hit volume (the displayed weapon).

   check_live_melee_hits.py <live probe folder> --strokes 1500,1800,2100,2400 --expect 1,0,1,1

--strokes: first frames of the strokes of the hand script; a hit belongs to the last stroke that started before it.
--expect:  per stroke 1 (must hit) or 0 (must not hit); without it only the consistency of the records is checked.
"""
import argparse, bisect, json, math, struct
from pathlib import Path


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('probe', type=Path)
    p.add_argument('--strokes', required=True)
    p.add_argument('--expect', default='')
    a = p.parse_args()
    strokes = [int(v) for v in a.strokes.split(',')]
    expect = [int(v) for v in a.expect.split(',')] if a.expect else []
    frames, positions, commits, ends, deltas, failures, presses = [], {}, [], {}, {}, [], []
    with open(a.probe / 'view-resource-probe.jsonl', encoding='utf-8', errors='replace') as f:
        for line in f:
            if '"event":"root_camera_sample"' in line:
                r = json.loads(line); frames.append((r['us'], r['frame'])); positions[r['frame']] = r['stable_q32']
            elif '"event":"motion_owned_hit_commit"' in line:
                commits.append(json.loads(line))
            elif '"event":"motion_owned_hit_commit_end"' in line:
                r = json.loads(line); ends[r['commit']] = r
            elif '"event":"motion_owned_health_delta"' in line:
                r = json.loads(line); deltas.setdefault(r['commit'], []).append(r)
            elif '"event":"live_melee_tap_released"' in line:
                presses.append(json.loads(line))
            elif 'failure"' in line and '"event":"' in line:
                r = json.loads(line)
                if r['event'].endswith('failure'):
                    failures.append(r['event'])
    times = [u for u, _ in frames]
    frame_of = lambda us: frames[min(bisect.bisect_left(times, us), len(frames) - 1)][1]
    origin = positions[min(positions)]
    hits = []
    for c in commits:
        frame = frame_of(c['us']); me = positions[frame]
        raw = bytes.fromhex(c['candidate_bytes']); x, y, z = struct.unpack_from('<qqq', raw, 8)
        offset = [(x - me[0]) / 2 ** 32, (y - me[1]) / 2 ** 32, (z - me[2]) / 2 ** 32]
        stroke = max([i for i, s in enumerate(strokes) if s <= frame], default=None)
        health = [dict(before=d['before'], after=d['after'], requested=d['requested_delta']) for d in deltas.get(c['commit'], [])]
        hits.append(dict(frame=frame, stroke=None if stroke is None else stroke + 1, frames_after_stroke_start=None if stroke is None else frame - strokes[stroke], target=c['target'],
                         target_from_player_m=[round(v, 2) for v in offset], distance_m=round(math.hypot(offset[0], offset[2]), 2),
                         player_from_start_m=[round((me[0] - origin[0]) / 2 ** 32, 2), round((me[2] - origin[2]) / 2 ** 32, 2)], health=health,
                         commit_end=bool(ends.get(c['commit'], {}).get('candidate_unchanged')) and bool(ends.get(c['commit'], {}).get('read_only'))))
    per_stroke = [sum(1 for h in hits if h['stroke'] == i + 1) for i in range(len(strokes))]
    changes = [d for h in hits for d in h['health']]
    checks = {
        'no_failure_events': not failures,
        'every_commit_ended_unchanged': all(h['commit_end'] for h in hits),
        'every_hit_inside_a_stroke': all(h['stroke'] is not None and 0 <= h['frames_after_stroke_start'] <= 60 for h in hits),
        'health_changes_are_the_requested_decrease': all(d['after'] - d['before'] == d['requested'] and d['requested'] < 0 for d in changes),
        'one_attack_per_stroke': len(presses) == len(strokes),
    }
    if expect:
        checks['hits_as_expected_per_stroke'] = len(expect) == len(strokes) and all((n > 0) == bool(e) for n, e in zip(per_stroke, expect))
    report = dict(status='PASS' if all(checks.values()) else 'FAIL', checks=checks, strokes=strokes, hits_per_stroke=per_stroke, attacks=len(presses), hits=hits,
                  health_decreases=len(changes), failures=sorted(set(failures)),
                  limitation='Targets are whatever stood there (a tree outside the home); no enemy was hit. The amount of damage is the game\'s own.')
    (a.probe / 'live-melee-hits-check.json').write_text(json.dumps(report, indent=1) + '\n', encoding='utf-8')
    print(json.dumps(report, indent=1))
    raise SystemExit(0 if report['status'] == 'PASS' else 1)


if __name__ == '__main__':
    main()
