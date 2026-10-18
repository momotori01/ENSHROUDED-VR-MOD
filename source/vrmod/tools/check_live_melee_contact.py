"""Audit the live melee contact (-LiveMeleeContact, optionally -LiveMeleeEarly) of one session.

Real-device feedback 2026-10-11: the attack must hit where the weapon in the hand is, not where the game's animation
swings. Pass the probe directory. Checked from the logs:
  * every swing that pressed the attack button gets one attack collider, and how many frames later;
  * every update of that collider took the transform of the weapon as it was displayed (not the animated one),
    and the game handed exactly that transform to its physics;
  * the collider sweeps from its own previous tracked pose;
  * with --early: the attack's Impact was started on the first tick of the action and its authored occurrence withheld.
No enemy is hit in these Mock sessions: damage is not part of this check.
"""
import argparse, collections, json, struct
from pathlib import Path


def transform(h):
    b = bytes.fromhex(h); x, y, z = struct.unpack('<qqq', b[:24]); q = struct.unpack('<4f', b[24:40])
    return [x / 2 ** 32, y / 2 ** 32, z / 2 ** 32], list(q)


def audit(probe, early, delay_limit):
    events = [json.loads(s) for s in (probe / 'view-resource-probe.jsonl').read_text(encoding='utf-8').splitlines()]
    ev = lambda name: [e for e in events if e['event'] == name]
    enabled, presses, shadows = ev('live_melee_contact_enabled'), ev('live_melee_press'), ev('motion_collider_shadow')
    begins, ends, drained, samples = ev('motion_collider_apply_begin'), ev('motion_collider_apply_end'), ev('motion_collider_apply_drained'), ev('sequence_collider_sample')
    retimes, removed = ev('live_event_retime'), ev('live_event_retime_removed')
    shown = set(e['world_transform_bytes'][:80] for e in ev('motion_visual_pose') if 'world_transform_bytes' in e)
    # A weapon pair (daggers): the left-hand weapon is a shown weapon too (motion_offhand_visual.hpp).
    offhand = set(e['world_transform_bytes'][:80] for e in ev('motion_offhand_pose') if 'world_transform_bytes' in e)
    checks = dict(enabled=len(enabled) == 1 and enabled[0].get('early_impact', False) is early,
                  completed=any(e['result'] == 0 for e in ev('view_resource_hook_complete')),
                  no_failure=not any('failure' in e['event'] for e in events), swings=len(presses) >= 1)
    by_entity = collections.OrderedDict()
    for b in begins:
        by_entity.setdefault(b['entity'], []).append(b)
    first = sorted((u[0]['frame'], entity) for entity, u in by_entity.items())
    delays = []
    for p in presses:
        after = [f for f, _ in first if f > p['frame']]
        delays.append(min(after) - p['frame'] if after else None)
    checks['a_collider_for_every_swing'] = bool(delays) and all(d is not None and d <= delay_limit for d in delays)
    checks['one_collider_per_swing'] = len(by_entity) == len(presses)
    reasons = collections.Counter(e['reason'] for e in shadows)
    checks['updates_tracked'] = bool(begins) and len(begins) == len(ends) and all(e['query_restored'] and e['original_components_unchanged'] for e in ends)
    # Since 2026-10-15 the hit volume is put a little ahead of the tracked blade (motion_collider_apply.hpp leadPosition,
    # lead_metres at most 0.15): the orientation is the shown weapon's, the position moved on by the blade's speed.
    rotations = set(s[48:80] for s in shown) | set(s[48:80] for s in offhand)
    def applied_is_shown(b):
        a = b['applied_current'][:80]
        if a in shown or a in offhand: return True
        lead = b.get('lead_metres') or 0
        return 0 < lead <= 0.151 and a[48:80] in rotations
    checks['applied_is_the_shown_weapon'] = bool(begins) and all(applied_is_shown(b) for b in begins) and all(b.get('live') for b in begins)
    # With a pair, the hit volume of an attack is on the weapon of the hand whose stroke started it (VHVR: either hand strikes).
    pair = dict(shown=bool(offhand))
    if offhand:
        # By frame, as both hands can pass through the same pose at different times: the right-hand weapon of the
        # same frame, the left-hand weapon of the frame before (that part is prepared after the right one).
        right_at = {e['frame']: e['world_transform_bytes'][:80] for e in ev('motion_visual_pose') if 'world_transform_bytes' in e}
        left_at = {e['frame']: e['world_transform_bytes'][:80] for e in ev('motion_offhand_pose') if 'world_transform_bytes' in e}
        sides = []
        for press in presses:
            after = sorted((f, entity) for f, entity in first if f > press['frame'])
            if not after:
                sides.append(None); continue
            updates = by_entity[after[0][1]]
            left = sum(u['applied_current'][:80] in (left_at.get(u['frame'] - 1), left_at.get(u['frame']), left_at.get(u['frame'] - 2)) for u in updates)
            right = sum(u['applied_current'][:80] in (right_at.get(u['frame']), right_at.get(u['frame'] - 1)) for u in updates)
            sides.append(dict(frame=press['frame'], hand=press.get('hand', 1), updates=len(updates), on_left_weapon=left, on_right_weapon=right))
        pair.update(strokes=sides, left_strokes=sum(1 for x in sides if x and x['hand'] == 0), right_strokes=sum(1 for x in sides if x and x['hand'] == 1))
        checks['pair_hit_volume_on_the_striking_hand'] = all(x and x['on_left_weapon' if x['hand'] == 0 else 'on_right_weapon'] == x['updates'] for x in sides) and             pair['left_strokes'] > 0 and pair['right_strokes'] > 0 and any(x['on_right_weapon'] < x['updates'] for x in sides if x and x['hand'] == 0)
    # The animated transform must differ, or the check proves nothing.
    apart = []
    for b in begins:
        (ap, _), (op, _) = transform(b['applied_current']), transform(b['original_current'])
        apart.append(sum((u - v) ** 2 for u, v in zip(ap, op)) ** .5)
    checks['differs_from_the_animation'] = bool(apart) and min(apart) > .05
    by_ready = {b['ready_sample']: b for b in begins}
    physics = wrong = 0
    for s in samples:
        b = by_ready.get(s['ready_sample'])
        if not b or s['entity'] != b['entity']:
            continue
        raw = bytes.fromhex(s['physics_current_bytes']); values = struct.unpack('<8f', raw)
        position, q = transform(b['applied_current'])
        physics += 1
        wrong += not (all(abs(u - v) < 2e-2 for u, v in zip(values[:3], position)) and all(abs(u - v) < 2e-3 for u, v in zip(values[4:], q)))
    checks['physics_takes_the_tracked_pose'] = physics >= len(by_entity) and wrong == 0
    sweeps_ok = True
    for updates in by_entity.values():
        for i, b in enumerate(updates):
            previous = updates[i - 1]['applied_current'][:104] if i else b['applied_current'][:104]
            if b['previous_present'] and b['applied_previous'][:104] != previous and b['continuous']:
                sweeps_ok = False
            if not i and b['continuous']:
                sweeps_ok = False
    checks['sweeps_from_the_previous_tracked_pose'] = sweeps_ok
    checks['scopes_closed'] = len(drained) == 1 and drained[0]['opened'] == drained[0]['closed'] and not drained[0]['outstanding'] and not drained[0]['failures'] and drained[0].get('live') is True
    appends, suppresses = [e for e in retimes if e['operation'] == 'append'], [e for e in retimes if e['operation'] == 'suppress']
    if early:
        checks['impact_started_with_the_action'] = len(appends) == len(presses) and all(e['actor_and_storage_unchanged'] and e['range'][0] == 0 and e['authored'] > e['range'][1] for e in appends)
        # A swing made while walking ends its attack as soon as the ability to move comes back (-LiveMeleeStill), before
        # the authored Impact is due: then there is nothing to withhold. one_collider_per_swing above still demands
        # that no swing produced a second hit volume.
        started = {e['trigger'] for e in appends}
        checks['authored_impact_withheld'] = len(suppresses) <= len(appends) and len({e['trigger'] for e in suppresses}) == len(suppresses) and all(e['actor_and_storage_unchanged'] and e['trigger'] in started for e in suppresses)
        checks['summary'] = len(removed) == 1 and removed[0]['appended'] == len(appends) and removed[0]['failures'] == 0
    else:
        checks['no_retime'] = not retimes
    return dict(status='PASS' if all(checks.values()) else 'FAIL', checks=checks, pair=pair, swings=[p['frame'] for p in presses], collider_first_frames=first,
                frames_from_swing_to_collider=delays, updates_per_collider=[len(u) for u in by_entity.values()], shadow_reasons=dict(reasons),
                physics_samples=physics, physics_mismatches=wrong, metres_between_tracked_and_animated=[round(min(apart), 3), round(max(apart), 3)] if apart else [],
                early_appends=[dict(frame_ms=e['milliseconds'], press_age_ms=e['press_age_ms'], authored_ms=round(e['authored'] / 1e6, 1), length_ms=round(e['length'] / 1e6, 1)) for e in appends],
                not_retimed=[(e['reason'], e.get('impacts')) for e in ev('live_event_not_retimed')], drained=drained[0] if drained else None,
                limitation='Mock, no target: that the hit volume is where the weapon is, and when, is proven; damage to an enemy is not.')


if __name__ == '__main__':
    a = argparse.ArgumentParser(description=__doc__)
    a.add_argument('probe', type=Path)
    a.add_argument('--early', action='store_true')
    a.add_argument('--delay-frames', type=int, default=None, help='largest allowed distance from the swing to the collider (default 40, with --early 8)')
    v = a.parse_args()
    r = audit(v.probe, v.early, v.delay_frames if v.delay_frames is not None else (8 if v.early else 40))
    (v.probe / 'live-melee-contact-check.json').write_text(json.dumps(r, indent=2) + '\n', encoding='utf-8')
    print(json.dumps(r, indent=2))
    raise SystemExit(r['status'] != 'PASS')
