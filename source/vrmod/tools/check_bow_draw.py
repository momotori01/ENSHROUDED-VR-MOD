"""Audit the repeatable VHVR-style bow draw (-BowDraw): every shot of one Mock session.

Reference: docs/analysis/vhvr_spec_20261010/bow.md. Pass the probe directory.
"""
import argparse, csv, json, math, struct, sys
from pathlib import Path
from check_capture_cameras import rotate, qmul

ITEM = 327157552
REST_Y, REST_Z, FULL_PULL, FULL_DRAW = .00155006, .24005109, .55534656, .60


def unit(v):
    n = math.sqrt(sum(x * x for x in v))
    return [x / n for x in v]


def angle(u, v):
    return math.acos(max(-1, min(1, sum(a * b for a, b in zip(unit(u), unit(v))))))


def audit(probe, releases, holds):
    events = [json.loads(s) for s in (probe / 'view-resource-probe.jsonl').read_text(encoding='utf-8').splitlines()]
    # Since 2026-10-13 what is shown takes the hands a little smoothed (include/evr/pose_smoothing.hpp, event
    # hand_smoothing), and the displayed geometry then trails the scripted hands by one or two centimetres while they
    # move. The figures below are for the hands as tracked: run this audit on a trial with EVR_HAND_SMOOTHING=0.
    smoothing = [e for e in events if e.get('event') == 'hand_smoothing']
    if smoothing and smoothing[-1].get('cutoff_hz', 0) > 0:
        print('note: this trial showed smoothed hands; the display checks expect EVR_HAND_SMOOTHING=0', file=sys.stderr)
    ev = lambda name: [e for e in events if e['event'] == name]
    hands = {int(r[0]): list(map(float, r)) for r in csv.reader(
        s for s in (probe.parent / 'mock-hands.csv').read_text().splitlines() if s and not s.startswith('#'))}
    left = lambda f: hands[f][1:4]
    right = lambda f: hands[f][14:17]
    reach = lambda f: math.dist(left(f), right(f))
    count = len(releases)
    samples = ev('bow_physical_trial_sample')
    armed, calls = ev('bow_tracked_shot_armed'), ev('bow_tracked_aim_call')
    by_sample = lambda name: {e['sample']: e for e in ev(name)}
    spawns, dispatches, submits, entities = (by_sample(n) for n in (
        'bow_spawn_sample', 'bow_spawn_dispatch_result', 'bow_spawn_submit_result', 'bow_spawn_entity_observed'))
    poses = {e['frame']: e for e in ev('motion_visual_pose') if e['item'] == ITEM}
    strings = {e['frame']: e for e in ev('motion_bow_physical_pose')}
    removed = ev('bow_spawn_removed')
    checks = dict(
        enabled=len(ev('bow_draw_enabled')) == 1 and len(ev('bow_tracked_aim_enabled')) == 1,
        shot_count=len(armed) == len(calls) == count,
        release_frames=[a['frame'] for a in armed] == releases,
        token=True, native_submission=True, new_projectile=True, geometry=True, aim=True,
        restored=len(removed) == 1 and removed[0]['restored'] and not removed[0]['failures'] and not removed[0]['callbacks'],
        completed=any(e['result'] == 0 for e in ev('view_resource_hook_complete')),
        no_failure=not any('failure' in e['event'] for e in events))
    details, directions, spawned = [], [], []
    for a, c in zip(armed, calls):
        frame = a['frame']
        x, d, s, n = (table.get(c['sample']) for table in (spawns, dispatches, submits, entities))
        checks['token'] &= bool(a['armed'] and c['applied'] and c['consumed'] and c['release_frame'] == frame and
                                0 <= c['age_ms'] <= 500 and 0 <= c['current_frame'] - frame <= 30)
        if not (x and d and s and n and frame in poses and frame in hands):
            checks['native_submission'] = False
            continue
        transform = bytes.fromhex(x['transform_bytes'])
        checks['native_submission'] &= bool(x['result'] and x['tracked_aim'] and x['sources_unchanged'] and d['result'] and d['same_transform'] and
                                            s['result'] and s['source_unchanged'] and x['us'] <= d['us'] <= s['us'])
        checks['new_projectile'] &= bool(n['native_result'] and n['actor_matched'] and n['actor_stable'] and n['spawned_entity'] and
                                         n['spawned_entity'] != n['previous_spawned_entity'])
        spawned.append(n['spawned_entity'])
        # The release frame is still posed from both hands: the drawing hand lies on the
        # model's string axis, at the rest elevation.
        m = poses[frame]
        scale, e = m['scale'], REST_Y * m['scale'][1]
        expected = [0, REST_Y, math.sqrt(reach(frame) ** 2 - e * e) / scale[2]]
        nock_error = max(abs(v - w) for v, w in zip(expected, a['nock']))
        checks['geometry'] &= (m['mount_kind'] == 'native_left_bow_drawn' and a['model_q32'] == m['q32'] and
                               max(abs(v - w) for v, w in zip(a['model_q'], m['orientation'])) < 1e-6 and
                               max(abs(v - w) for v, w in zip(m['grip_position'], left(frame))) < 1e-6 and
                               nock_error < 3e-5 and a['binding'] == (ITEM << 32) | m['source_handle'])
        # Arrow rest -> drawing hand, through the model's native world orientation.
        nock = [a['nock'][i] * scale[i] for i in range(3)]
        delta = [0 - nock[0], e - nock[1], nock[2]]
        direction = rotate(m['orientation'], unit(delta))
        offset = rotate(m['orientation'], [0, e, 0])
        position = struct.unpack_from('<3q', transform)
        actual = rotate(struct.unpack_from('<4f', transform, 24), [0, 0, 1])
        position_error = max(abs((position[i] - m['q32'][i]) / 2 ** 32 - offset[i]) for i in range(3))
        direction_error = max(abs(v - w) for v, w in zip(direction, actual))
        event = bytes.fromhex(x['event_bytes'])
        checks['aim'] &= (event[0x148] == 1 and struct.unpack_from('<8f', event, 0x104) == (0.,) * 8 and
                          position_error < 3e-6 and direction_error < 3e-6)
        directions.append((frame, actual))
        details.append(dict(sample=c['sample'], release_frame=frame, consume_frame=c['current_frame'], age_ms=c['age_ms'],
                            nock_error_m=nock_error, position_error_m=position_error, direction_error=direction_error,
                            spawned_entity=n['spawned_entity'], release_pull_m=reach(frame) - REST_Z * scale[2]))
    checks['new_projectile'] &= len(set(spawned)) == count
    # Independent of the logged model pose: rebuild each shot line from the hand script
    # alone (tracking space) and compare the angle between shots with the world result.
    # The line runs from the drawing hand to the arrow rest, which sits REST_Y above the
    # handle along the bow's limb axis; the limb axis keeps the left hand's roll.
    def script_line(f):
        yaw, pitch, roll = [math.radians(t) / 2 for t in hands[f][4:7]]
        q = qmul(qmul([0, math.sin(yaw), 0, math.cos(yaw)], [math.sin(pitch), 0, 0, math.cos(pitch)]), [0, 0, math.sin(roll), math.cos(roll)])
        back = unit([r - l for l, r in zip(left(f), right(f))])
        hint = rotate(q, [0, 1, 0])
        along = sum(h * b for h, b in zip(hint, back))
        up = unit([h - b * along for h, b in zip(hint, back)])
        tilt = math.asin(REST_Y * poses[f]['scale'][1] / reach(f))
        return [u * math.sin(tilt) - b * math.cos(tilt) for u, b in zip(up, back)]

    between = []
    for (f0, d0), (f1, d1) in zip(directions, directions[1:]):
        between.append(dict(frames=[f0, f1], world_degrees=math.degrees(angle(d0, d1)),
                            script_degrees=math.degrees(angle(script_line(f0), script_line(f1)))))
    checks['shot_to_shot_angle'] = len(between) == count - 1 and all(abs(b['world_degrees'] - b['script_degrees']) < .002 for b in between)

    # Input: the string is taken, held with LT only, and every release is one RT tap.
    held = sorted(x['frame'] for x in samples if x['phase'] == 2)
    taken = [f for f in held if f - 1 not in held]
    released = [x for x in samples if x['phase'] == 5 and x['triggers'] == [255, 255]]
    checks['taken'] = taken == [h[0] for h in holds]
    # The frontend thread may not see every XR frame; nothing may be held outside a hold.
    coverage = [len([f for f in held if first <= f < last]) / (last - first) for first, last in holds]
    checks['held_until_release'] = all(c >= .9 for c in coverage) and all(any(first <= f < last for first, last in holds) for f in held)
    checks['charge_only'] = all(x['triggers'] == [255, 0] and x['valid'] for x in samples if x['phase'] == 2)
    checks['trigger_before_string'] = all(x['near'] and x['distance'] <= .2 for x in samples if x['phase'] == 2 and x['frame'] in taken)
    checks['releases'] = [x['frame'] for x in released] == releases and all(x['raw_trigger'] <= .2 and x['release_pull'] >= .1 for x in released)
    checks['shots_counted'] = bool(samples) and max(x.get('shots', 0) for x in samples) == count
    pads = ev('controller_live_sample')
    pulses, start = [], None
    for x in pads:
        if x['triggers'][1] and start is None:
            start = x['us']
        elif not x['triggers'][1] and start is not None:
            pulses.append((start, x['us']))
            start = None
    taps = [(begin, end) for begin, end in pulses if any(0 <= begin - r['us'] <= 100000 for r in released)]
    # The tap is asked as 80 ms (bow_draw.hpp, since 2026-10-11 03:50; 60 ms before, which sessions up to
    # game034329 show as 49..68 ms). The frontend clock and loop step by about 16 ms: 78..112 ms in the log.
    checks['one_tap_per_release'] = len(taps) == len(released) == count and all(60000 <= end - begin <= 140000 for begin, end in taps)

    # Native: both Worlds charge and fire once per shot. The survey stops after 2048 samples.
    worlds = {}
    survey = ev('input_bow_sample')
    for x in survey:
        b = bytes.fromhex(x['weapon_bytes'])
        if struct.unpack_from('<I', b, 0x4c)[0] != ITEM:
            continue
        rows = worlds.setdefault(x['world'], [])
        if not rows or rows[-1]['state'] != b[0]:
            rows.append(dict(us=x['us'], frame=x['frame'], state=b[0], ammo=struct.unpack_from('<I', b, 0x60)[0], missing=struct.unpack_from('<H', b, 0x6c)[0]))
    # One fire per charge cycle: the cycles are separated by the idle state 0. Within a cycle the server World can show
    # fire, charge, fire on consecutive samples (game133533, frames 725-726: one projectile spawned), so the cycle counts once.
    def cycles(rows):
        # A state that is replaced within the same frame is a replayed sample of the server World, not a state of its
        # own (game143213: idle, fire, idle on frames 1038, 1039, 1039 after the shot had ended; one projectile).
        rows = [r for i, r in enumerate(rows) if i + 1 == len(rows) or rows[i + 1]['frame'] != r['frame']]
        fired, current = 0, False
        for r in rows:
            if r['state'] == 0:
                fired += current; current = False
            elif r['state'] == 3:
                current = True
        return fired + current
    fires = {w: cycles(rows) for w, rows in worlds.items()}
    # A shot released after the survey has used its 2048 records has no native record here (run game012647,
    # fourth shot at 6420); its spawned projectile is still checked above.
    survey_last = max((x['frame'] for x in survey), default=0)
    surveyed = count if len(survey) < 2048 else sum(f + 8 <= survey_last for f in releases)
    checks['native_fire_each_shot'] = len(worlds) == 2 and surveyed >= 1 and all(v == surveyed for v in fires.values()) and \
        all(r['ammo'] and not r['missing'] for rows in worlds.values() for r in rows if r['state'] == 3)

    # Display: while held the bow is posed from both hands and the string is at the shown
    # hand. VHVR "Full" restriction: the string travels at most span*max(sqrt(p), .01), p being
    # the native bow's own draw read from its source pose on the previous frame; the drawing
    # hand is then shown on the string. After each release the bow is back on the left-hand mount.
    grips = {e['frame']: e['grips'] for e in ev('motion_hand_pose')}

    def native_progress(f):
        z = struct.unpack_from('<f', bytes.fromhex(strings[f]['source_joints_bytes']), 5 * 48 + 24)[0]
        return min(max((-z - REST_Z) / FULL_PULL, 0), 1)

    display = dict(drawn=0, rest=0, missing=0, restricted=0, worst_pull_error=0., worst_hand_error=0., worst_progress_error=0., most_held_back_m=0.)
    ok = True
    full_draw_expected = []
    for first, last in holds:
        reached = None
        for f in range(first + 12, last):   # the frontend state reaches the render thread within 8 frames
            m, s, g = poses.get(f), strings.get(f), grips.get(f)
            if not m or not s or not g:
                display['missing'] += 1
                continue
            sy, sz = m['scale'][1], m['scale'][2]
            e = REST_Y * sy
            free = math.sqrt(max(0, reach(f) ** 2 - e * e)) - REST_Z * sz
            span = FULL_DRAW - REST_Z * sz
            limit = span * max(math.sqrt(s['limit_progress']), .01)
            want = min(max(min(free, limit), 0) / sz, FULL_PULL)
            error = abs(s['model_pull'] - want)
            shown = math.sqrt(e * e + (REST_Z * sz + max(min(free, limit), 0)) ** 2) if free > limit else reach(f)
            hand_error = abs(math.dist(g[0][:3], g[1][:3]) - shown)
            display['worst_pull_error'] = max(display['worst_pull_error'], error)
            display['worst_hand_error'] = max(display['worst_hand_error'], hand_error)
            ok &= m['mount_kind'] == 'native_left_bow_drawn' and s['held'] and error < 2e-4 and hand_error < 2e-5
            if abs(free - limit) > 1e-4:
                ok &= s['restricted'] == (free > limit)
            if free > limit:
                display['restricted'] += 1
                display['most_held_back_m'] = max(display['most_held_back_m'], free - limit)
            if f - 1 in strings:
                progress_error = abs(native_progress(f - 1) - s['limit_progress'])
                display['worst_progress_error'] = max(display['worst_progress_error'], progress_error)
                ok &= progress_error < 2e-6
            display['drawn'] += 1
        if any(math.sqrt(max(0, reach(f) ** 2 - (REST_Y) ** 2)) - REST_Z * poses[f]['scale'][2] >= FULL_DRAW - REST_Z * poses[f]['scale'][2]
               for f in range(first, last) if f in poses):
            full_draw_expected.append((first, last))
        for f in range(last + 12, last + 40):
            m, s, g = poses.get(f), strings.get(f), grips.get(f)
            if m and s and g:
                ok &= m['mount_kind'] == 'native_left_bow_animated' and not s['held'] and s['model_pull'] == 0 and not s['restricted'] and                     abs(math.dist(g[0][:3], g[1][:3]) - reach(f)) < 2e-5
                display['rest'] += 1
    checks['display'] = ok and display['drawn'] > 0 and display['rest'] > 0 and not display['missing']
    checks['restriction_exercised'] = display['restricted'] > 0

    # Pulses (VHVR): release = arrow hand 0.9 and bow hand 0.3; first full draw of a hold = bow hand 0.3.
    requests = ev('bow_draw_haptic')
    release_requests = [h['frame'] for h in requests if h['kind'] == 'release']
    full_requests = [h['frame'] for h in requests if h['kind'] == 'full_draw']
    checks['haptic_requests'] = release_requests == releases and len(full_requests) == len(full_draw_expected) and         all(first <= f < last for f, (first, last) in zip(full_requests, full_draw_expected))
    sent = [json.loads(line) for line in (probe.parent / 'native-xr' / 'openxr.jsonl').read_text(encoding='utf-8').splitlines() if '"native_gesture_haptic"' in line]

    def bursts(hand, low, high):
        frames = [x['frame'] for x in sent if low <= x['amplitude'][hand] <= high and x['delivered'][hand]]
        return [f for f in frames if f - 1 not in frames]

    right, left_bursts = bursts(1, .89, .91), bursts(0, .29, .31)
    # The melee strokes after the bow pulse the right hand at 0.9 as well since 2026-10-14: count the bow's part only.
    if releases: right = [b for b in right if b <= releases[-1] + 10]
    expected_left = sorted(releases + full_requests)
    checks['haptics_sent'] = len(right) == count and all(0 <= b - r <= 10 for b, r in zip(right, releases)) and         len(left_bursts) == len(expected_left) and all(0 <= b - r <= 10 for b, r in zip(left_bursts, expected_left))
    haptics = dict(release_requests=release_requests, full_draw_requests=full_requests, right_burst_frames=right, left_burst_frames=left_bursts)

    # The nocked arrow (VHVR: at the string point, pointing at the arrow rest). The game's own arrow stays out of both
    # eyes; its copy is on the tracked string in every hold, only while the string is held, and gone after the release.
    # Checked when the run shows the arrow (motion_arrow_visual.hpp).
    arrows = {e['frame']: e for e in ev('motion_arrow_pose')}
    arrow = dict(shown=bool(arrows))
    if arrows:
        eyes = {}
        for e in ev('motion_arrow_eye_payload'):
            eyes.setdefault(e['serial'], {})[e['eye']] = e
        both_eyes = all(set(eyes.get(a['serial'], {})) == {0, 1} and all(
            p['shield_instance'] == (a['handle'] & 0xffffff) and (a['source_handle'] & 0xffffff) in p['indices'] and
            (a['handle'] & 0xffffff) not in p['indices'] for p in eyes[a['serial']].values()) for a in arrows.values())
        per_hold = [[f for f in arrows if first <= f <= last] for first, last in holds]
        late = [min(frames) - first if frames else None for frames, (first, last) in zip(per_hold, holds)]
        models = ev('motion_arrow_model')
        arrow.update(frames=len(arrows), first_frame_after_hold=late, sources=len(ev('motion_arrow_source')),
                     nock_distance=[m['nock_distance'] for m in models], from_model_extent=[m['from_model_extent'] for m in models],
                     worst_axis_agreement=min(a['bow_axis_agreement'] for a in arrows.values()))
        checks['arrow_on_string'] = all(frames and delay <= 45 and len(frames) >= (last - first - delay) * .95
                                        for frames, delay, (first, last) in zip(per_hold, late, holds)) and \
            all(strings[f]['held'] for f in arrows if f in strings) and sum(map(len, per_hold)) == len(arrows) and \
            all(a['bow_axis_agreement'] > .9999 and a['source_unchanged'] and abs(a['nock_distance']) < 1.5 for a in arrows.values())
        checks['arrow_in_both_eyes'] = both_eyes
        checks['arrow_no_failure'] = not ev('motion_arrow_failure')
    return dict(status='PASS' if all(checks.values()) else 'FAIL', checks=checks, shots=details, angles_between_shots=between, arrow=arrow,
                taken_frames=taken, hold_coverage=coverage, native_fires=fires, native_survey_samples=len(survey), native_survey_last_frame=survey_last, native_fires_expected=surveyed, display=display, haptics=haptics,
                limitation='Mock, one reviewed bow. Native time-based power and ammunition are untouched; flight and hit are left to the '
                           'game and not audited. Grip angle, arrow rest, draw length and feel on hardware are unverified.')


if __name__ == '__main__':
    a = argparse.ArgumentParser(description=__doc__)
    a.add_argument('probe', type=Path)
    a.add_argument('--releases', default='720,1010,1230')
    # Shot 2 presses 30 cm away at 811 and approaches over 831..850: inside 20 cm from frame 837.
    a.add_argument('--holds', default='540-720,837-1010,1100-1230', help='first held frame - release frame for every shot')
    v = a.parse_args()
    r = audit(v.probe, [int(x) for x in v.releases.split(',')], [tuple(map(int, x.split('-'))) for x in v.holds.split(',')])
    (v.probe / 'bow-draw-check.json').write_text(json.dumps(r, indent=2) + '\n', encoding='utf-8')
    print(json.dumps(r, indent=2))
    raise SystemExit(r['status'] != 'PASS')
