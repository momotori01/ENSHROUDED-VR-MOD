"""Audit the player IK on the player's own model (-BodyIk, hooks/motion_body_ik.hpp + include/evr/body_ik.hpp) of one session.

Pass the probe directory. Every logged pose (one in 30 frames) is checked against the skeleton's rest lengths
(docs/analysis/motion_hand_default_globals.json): the hand sockets are on the grips, the head joint sits behind
the eyes, the chest block hangs rigidly from the neck pivot, the hips are under that pivot within what the spine allows,
the arm bones keep their lengths and the feet keep the animation's height. Mock or real device.
"""
import argparse, json, math
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
EYE_IN_HEAD, EYE_FROM_PIVOT = (0, .0522, .1), (0, .13, .1)


def sub(a, b): return [x - y for x, y in zip(a, b)]
def norm(a): return math.sqrt(sum(x * x for x in a))
def flat(a): return math.hypot(a[0], a[2])


def audit(probe):
    rest = {r['index']: r['p'] for r in json.loads((ROOT / 'docs/analysis/motion_hand_default_globals.json').read_text(encoding='utf-8'))}
    upper, fore = norm(sub(rest[42], rest[40])), norm(sub(rest[44], rest[42]))
    rest_pivot = [rest[17][i] + EYE_IN_HEAD[i] - EYE_FROM_PIVOT[i] for i in range(3)]
    torso = rest[15][1] - rest[1][1]
    events = [json.loads(s) for s in (probe / 'view-resource-probe.jsonl').read_text(encoding='utf-8').splitlines()]
    ev = lambda name: [e for e in events if e['event'] == name]
    poses, created, disposed, released = ev('motion_body_ik_pose'), ev('motion_body_ik_created'), ev('motion_body_ik_disposed'), ev('motion_body_ik_released')
    checks = dict(enabled=len(ev('motion_body_ik_enabled')) == 1, created=len(created) == 1 and created[0].get('solver') == 'neck_pivot_chest_arms_hips_knees',
                  completed=any(e['result'] == 0 for e in ev('view_resource_hook_complete')),
                  no_failure=not any('failure' in e['event'] for e in events), poses=len(poses) >= 20)
    worst = dict(socket=0.0, grip=0.0, head=0.0, pivot=0.0, chest=0.0, hips_side=0.0, upper=0.0, fore=0.0, shoulders=0.0, foot=0.0, stretch=0.0, leg_short=0.0)
    torso_seen, below = [], []
    for p in poses:
        worst['socket'] = max(worst['socket'], *p['socket_error'])
        worst['stretch'] = max(worst['stretch'], *p['arm_stretch'])
        worst['leg_short'] = max(worst['leg_short'], *p['leg_short'])
        for h in range(2):
            worst['grip'] = max(worst['grip'], norm(sub(p['sockets'][h], p['grips'][h])))
            worst['upper'] = max(worst['upper'], abs(norm(sub(p['elbows'][h], p['shoulders'][h])) - upper))
            if p['arm_stretch'][h] == 0:
                worst['fore'] = max(worst['fore'], abs(norm(sub(p['wrists'][h], p['elbows'][h])) - fore))
            if p['leg_short'][h] == 0:
                worst['foot'] = max(worst['foot'], abs(p['ankles'][h][1] - p['animated_ankles'][h][1]))
        worst['head'] = max(worst['head'], abs(norm(sub(p['eyes'], p['head'])) - norm(EYE_IN_HEAD)))
        worst['pivot'] = max(worst['pivot'], abs(norm(sub(p['eyes'], p['pivot'])) - norm(EYE_FROM_PIVOT)))
        worst['chest'] = max(worst['chest'], abs(norm(sub(p['chest'], p['pivot'])) - norm(sub(rest[15], rest_pivot))), abs((p['chest'][1] - p['pivot'][1]) - (rest[15][1] - rest_pivot[1])))
        worst['hips_side'] = max(worst['hips_side'], abs(flat(sub(p['hips'], p['pivot'])) - flat(sub(rest[1], rest_pivot))))
        # A hand past the arm's length draws its shoulder up to 6 cm toward it.
        worst['shoulders'] = max(worst['shoulders'], abs(norm(sub(p['shoulders'][1], p['shoulders'][0])) - norm(sub(rest[70], rest[40]))))
        torso_seen.append(p['torso']); below.append(p['hips_below_animated'])
        if abs((p['chest'][1] - p['hips'][1]) / torso - p['torso']) > 2e-3:
            worst['chest'] = max(worst['chest'], 1.0)
    if poses:
        checks['hands_on_the_grips'] = worst['socket'] < 2e-3 and worst['grip'] < 2e-3
        checks['head_behind_the_eyes'] = worst['head'] < 2e-3
        checks['chest_hangs_from_the_neck_pivot'] = worst['pivot'] < 2e-3 and worst['chest'] < 2e-3 and worst['shoulders'] < .125
        checks['hips_under_the_neck_pivot'] = worst['hips_side'] < 2e-3
        checks['spine_within_limits'] = min(torso_seen) >= .75 - 1e-3 and max(torso_seen) <= 1 + 1e-3
        checks['arm_lengths_kept'] = worst['upper'] < 2e-3 and worst['fore'] < 2e-3
        checks['feet_keep_animated_height'] = worst['foot'] < 5e-3
        checks['game_pose_untouched'] = all(p['native_pose_unchanged'] for p in poses)
        last = poses[-1]
        # Since 2026-10-12 the copy of the bare hands rides on this pose too while the body is driven (event
        # motion_body_ik_hands; hooks/motion_body_ik.hpp): one model more than the bound body parts.
        hands_on_pose = 1 if ev('motion_body_ik_hands') else 0
        checks['parts_on_the_mod_pose'] = last['bound'] >= 1 and last['pose_models'] == last['bound'] + hands_on_pose
    # At the end the parts go back onto the game's pose, unless the game has already removed them with its Scene.
    checks['parts_returned_at_the_end'] = len(disposed) == 1 and disposed[0]['models_left_on_pose'] == 0 and \
        disposed[0]['restored'] + disposed[0]['already_gone'] == (poses[-1]['bound'] if poses else 0)
    return dict(status='PASS' if all(checks.values()) else 'FAIL', checks=checks, poses=len(poses), worst_m={k: round(v, 5) for k, v in worst.items()},
                torso_ratio=[round(min(torso_seen), 4), round(max(torso_seen), 4)] if torso_seen else [],
                hips_below_animated_m=[round(min(below), 4), round(max(below), 4)] if below else [],
                frames_with_stretched_arm=sum(1 for p in poses if max(p['arm_stretch']) > 0),
                frames_with_short_leg=sum(1 for p in poses if max(p['leg_short']) > 0),
                bound_parts=poses[-1]['bound'] if poses else 0, releases=[(r['frame'], r['reason'], r['restored']) for r in released],
                disposed=disposed[0] if disposed else None,
                limitation='Numbers only: what the body looks like (shadow, clothing near the camera, held items) is judged from the captured images.')


if __name__ == '__main__':
    a = argparse.ArgumentParser(description=__doc__)
    a.add_argument('probe', type=Path)
    v = a.parse_args()
    r = audit(v.probe)
    (v.probe / 'body-ik-check.json').write_text(json.dumps(r, indent=2) + '\n', encoding='utf-8')
    print(json.dumps(r, indent=2))
    raise SystemExit(r['status'] != 'PASS')
