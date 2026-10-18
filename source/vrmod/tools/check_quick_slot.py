"""Audit the quick slot by hand motion (-QuickSlotMotion) of one Mock session.

User decision 2026-10-11 after VHVR's quick menu (docs/analysis/vhvr_spec_20261010/ui_hud.md section 5):
right B held opens the game's quick wheel, the right hand points at an item, release takes it; the right
grip no longer opens the wheel. Pass the probe directory. The expected moves are those of
tools/make_quick_slot_fixture.py.
"""
import argparse, json, math
from pathlib import Path

AXE, SWORD = 1940764111, 1125155603
HOLDS = dict(axe=(400, 469), cancel=(700, 759), sword=(900, 969))
GRIP = (1100, 1159)


def audit(probe):
    events = [json.loads(s) for s in (probe / 'view-resource-probe.jsonl').read_text(encoding='utf-8').splitlines()]
    ev = lambda name: [e for e in events if e['event'] == name]
    states, pads, poses = ev('quick_slot_state'), ev('controller_live_sample'), ev('motion_visual_pose')
    within = lambda f, span, slack=0: span[0] - slack <= f <= span[1] + slack
    kinds = [(s['kind'], s['frame']) for s in states]
    checks = dict(enabled=len(ev('quick_slot_motion_enabled')) == 1,
                  completed=any(e['result'] == 0 for e in ev('view_resource_hook_complete')),
                  no_failure=not any('failure' in e['event'] for e in events))
    # Opened about 250 ms into every hold, never at once; chosen, cancelled, chosen on the releases.
    opened = [f for k, f in kinds if k == 'opened']
    checks['opens_after_hold'] = len(opened) == 3 and all(within(f, (span[0] + 4, span[0] + 40)) for f, span in zip(opened, HOLDS.values()))
    ends = [(k, f) for k, f in kinds if k in ('chosen', 'cancelled')]
    checks['release_decides'] = [k for k, _ in ends] == ['chosen', 'cancelled', 'chosen'] and all(within(f, (span[1] + 1, span[1] + 4)) for (_, f), span in zip(ends, HOLDS.values()))
    checks['no_tap'] = not any(k == 'tapped' for k, _ in kinds)
    # The stick at release points where the hand went: 51 degrees for the axe, 129 for the sword.
    chosen = [s for s in states if s['kind'] == 'chosen']
    angles = [math.degrees(math.atan2(s['stick'][1], s['stick'][0])) for s in chosen]
    checks['points_with_the_hand'] = len(angles) == 2 and abs(angles[0] - 51) < 1 and abs(angles[1] - 129) < 1
    # The game: the wheel button only inside the holds, and the weapon in hand changes accordingly.
    wheel = [x['frame'] for x in pads if x['buttons'] & 0x200]
    checks['wheel_button_only_while_held'] = bool(wheel) and all(any(within(f, span, 3) for span in HOLDS.values()) for f in wheel)
    checks['grip_is_free'] = any(x['raw_grips'][1] and within(x['frame'], GRIP) for x in pads) and not any(within(f, GRIP, 20) for f in wheel)
    item_at = lambda frame: next((x['item'] for x in poses if x['frame'] >= frame), None)
    checks['axe_taken'] = item_at(HOLDS['axe'][1] + 60) == AXE and item_at(HOLDS['cancel'][1] + 60) == AXE
    checks['sword_taken'] = item_at(HOLDS['sword'][1] + 60) == SWORD and item_at(GRIP[1] + 60) == SWORD
    checks['y_button_not_pressed'] = not any(x['buttons'] & 0x8000 for x in pads)
    return dict(status='PASS' if all(checks.values()) else 'FAIL', checks=checks, states=kinds, stick_degrees_at_release=angles,
                wheel_button_frames=[min(wheel), max(wheel), len(wheel)] if wheel else [],
                limitation='Mock. The wheel is the game\'s own, shown on the UI panel, not a ring at the hand as in VHVR; '
                           'the short press (Y) is covered by the unit test only. Hardware feel is unverified.')


if __name__ == '__main__':
    a = argparse.ArgumentParser(description=__doc__)
    a.add_argument('probe', type=Path)
    v = a.parse_args()
    r = audit(v.probe)
    (v.probe / 'quick-slot-check.json').write_text(json.dumps(r, indent=2) + '\n', encoding='utf-8')
    print(json.dumps(r, indent=2))
    raise SystemExit(r['status'] != 'PASS')
