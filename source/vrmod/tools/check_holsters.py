#!/usr/bin/env python3
"""Check a Mock trial of the holsters (hands: tools/make_holster_fixture.py, default plan; flags: the usual ones with
-QuickSlotMotion, which frees the right grip as on the headset).

  check_holsters.py <view-resource-probe directory>

The default plan begins with the test save's bow of the action bar (slot 1) in the hand, a ranged weapon and a shield
in the equipment:
  480 right shoulder  -> no slot (bare hands)          600 right shoulder -> slot 1 again
  720 left shoulder   -> a bow in the hand: no slot    840 left shoulder  -> the ranged-combat bit held
  960 left hip        -> the shield off                1080 left hip      -> the shield on again
  1200 left shoulder  -> the bit let go (bare hands)   1320 right shoulder -> slot 1 again
  1440 the left grip in front of the body -> nothing
For every press: the place and what was asked; then what the game showed within a second (the selected slot, the
shield slot of the equipment, the bit in the input that was sent). No press is asked without an answer, none answers
without being asked, and the private input copy reports no change outside the permitted fields.
"""
import json, pathlib, sys

PLAN = [  # frame, act, select, shield, armed afterwards, selected slot afterwards, shield worn afterwards
    (480, 'weapon', 16, -1, False, 16, True), (600, 'weapon', 1, -1, False, 1, True),
    (720, 'ranged', 16, -1, False, 16, True), (840, 'ranged', -1, -1, True, 16, True),
    (960, 'shield', -1, 1, True, 16, False), (1080, 'shield', -1, 0, True, 16, True),
    (1200, 'ranged', -1, -1, False, 16, True), (1320, 'weapon', 1, -1, False, 1, True)]
PLACE = {'weapon': 2, 'ranged': 1, 'shield': 3}


def main():
    if len(sys.argv) != 2:
        raise SystemExit(__doc__)
    path = pathlib.Path(sys.argv[1]) / 'view-resource-probe.jsonl'
    acts, presses, inputs, layouts, failures, enabled, frame = [], [], [], [], [], False, 0
    with path.open(encoding='utf-8', errors='replace') as lines:
        for line in lines:
            if '"event":"live_native_frame_begin"' in line:
                at = line.find('"frame":')
                if at >= 0:
                    frame = int(line[at + 8:].split(',')[0].split('}')[0])
                continue
            if '"event":"holster' not in line and '"event":"equipment_layout"' not in line and '"event":"input_shadow_failure"' not in line:
                continue
            e = json.loads(line); name = e['event']
            if name == 'holsters_enabled': enabled = True
            elif name == 'holster_act': acts.append((frame, e))
            elif name == 'holster_press': presses.append((frame, e))
            elif name == 'holster_input': inputs.append((frame, e))
            elif name == 'equipment_layout': layouts.append((frame, e))
            elif name == 'input_shadow_failure': failures.append(e)
    results = []

    def check(name, ok, detail=''):
        results.append(ok); print(('PASS ' if ok else 'FAIL ') + name + (' ' + detail if detail else ''))

    check('holsters_enabled', enabled)
    check('presses_as_planned', [f for f, _ in presses] == [p[0] for p in PLAN], str([f for f, _ in presses]))
    check('no_press_without_an_act', len(acts) == len(presses) and all(a[0] == p[0] for a, p in zip(acts, presses)))
    for at, act, select, shield, armed, selected, worn in PLAN:
        mine = [e for f, e in acts if f == at]
        press = [e for f, e in presses if f == at]
        if not mine or not press:
            check('%d_%s' % (at, act), False, 'no press'); continue
        e, p = mine[0], press[0]
        ok = e['act'] == act and e['asked'] and e['select'] == select and e['shield'] == shield and e['armed'] == armed and p['place'] == PLACE[act] and p['asked'] and p['vibration'] == [0.3, 200]
        # What the game showed afterwards: the last layout within 30 frames (about a second).
        after = [l for f, l in layouts if at < f <= at + 30]
        last = after[-1] if after else ([l for f, l in layouts if f <= at] or [None])[-1]
        shown = last is not None and last['selected_slot'] == selected and (last['equipment'][15][2] != 0) == worn
        sent = [i for f, i in inputs if at <= f <= at + 30]
        bit = bool(sent) and sent[-1]['ranged_combat_bit'] == armed
        check('%d_%s' % (at, act), ok and shown and bit, 'asked=%s select=%s shield=%s armed=%s -> slot %s, shield worn %s, bit %s' % (
            e['asked'], e['select'], e['shield'], e['armed'], last and last['selected_slot'], last and last['equipment'][15][2] != 0, sent[-1]['ranged_combat_bit'] if sent else None))
    # The shield came back as the same item.
    shields = [l['equipment'][15][2] for _, l in layouts]
    check('shield_the_same_again', bool(shields) and shields[0] != 0 and shields[-1] == shields[0] and 0 in shields)
    # The game's own selections were never overwritten: every override that ended, ended by the game's own action.
    check('private_copy_permitted_fields_only', not failures, str(failures[:1]))
    print('holsters', 'PASS' if all(results) else 'FAIL', '%d/%d' % (sum(results), len(results)))
    return 0 if all(results) else 1


if __name__ == '__main__':
    sys.exit(main())
