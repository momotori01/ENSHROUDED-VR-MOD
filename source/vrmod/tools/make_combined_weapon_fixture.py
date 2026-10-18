#!/usr/bin/env python3
"""One long Mock hand script that goes through every weapon route in a single session.

It joins the reviewed single-route scripts, shifted in time, so the routes are exercised
across weapon changes the way a play session does:
  bow, three shots -> staff, two casts -> wand, two shots -> sword strokes -> axe strokes
  -> sword and shield guard -> back to the bow, one more shot.
With --two-hand the axe two-hand hold (entry, strokes, weapon guard, reversed hold, wheel) follows the guard part.
With --ranged-after-melee the staff, the wand and the fourth bow shot come after all melee parts and a one second walk:
for a session without the scenery replay, where the view follows the character as it does on a headset.
With --left-grip-entry the two-hand part is the left-grip version (user rule 2026-10-11) and the left grip is up
during the melee part, where the still-hand source script holds it down throughout.
With --melee-guard-only the script holds just the sword and axe strokes and the shield guard: the feature set
of a plain live-input session (-LiveMelee -LiveGuard) without the weapon display routes.
The frame offsets of every part are written beside the CSV as JSON for the checkers.
"""
import argparse, json, pathlib

POSITION = (1, 2, 3, 14, 15, 16)
CONTROLS = {10: '0', 23: '0', 24: '0', 29: '0', 30: '0', 31: '0', 32: '0', 33: '0', 34: '0', 35: '0', 36: '0', 37: '0', 38: '0'}
RIGHT_GRAB, RIGHT_STICK_X, RIGHT_STICK_Y, LEFT_STICK_Y, LEFT_GRAB = 24, 32, 33, 30, 11
BOW = ('-0.9063077870366499', '0.42261826174069944')   # quick wheel direction of the bow in the home save


def load(path):
    rows = [line.split(',') for line in path.read_text(encoding='utf-8').splitlines() if line and not line.startswith('#')]
    if len(rows) != 4300 or any(len(r) != 39 for r in rows):
        raise SystemExit(f'unexpected script {path}')
    return rows


def main():
    p = argparse.ArgumentParser()
    p.add_argument('--directory', type=pathlib.Path, default=pathlib.Path('vrmod/test_env'))
    p.add_argument('--output', type=pathlib.Path, default=pathlib.Path('vrmod/test_env/home-combined-weapons-20261011.csv'))
    p.add_argument('--two-hand', action='store_true', help='append tools/make_two_hand_fixture.py after the guard part (use another --output)')
    p.add_argument('--ranged-after-melee', action='store_true', help='staff, wand and the fourth bow shot after the melee parts and a short walk (use another --output)')
    p.add_argument('--left-grip-entry', action='store_true', help='with --two-hand: the left-grip two-hand script, left grip up during the melee part (use another --output)')
    p.add_argument('--melee-guard-only', action='store_true', help='only the melee strokes and the shield guard (use another --output)')
    a = p.parse_args()
    if (a.two_hand or a.ranged_after_melee or a.melee_guard_only) and a.output.name == 'home-combined-weapons-20261011.csv':
        raise SystemExit('the reviewed combined script is kept as it is: give --output with --two-hand or --ranged-after-melee')
    bow = load(a.directory / 'home-bow-draw-three-shots-20261010.csv')
    aim = load(a.directory / 'home-staff-wand-aim-20261010.csv')
    melee = load(a.directory / 'home-live-melee-20261011.csv')
    if a.left_grip_entry:
        if not a.two_hand:
            raise SystemExit('--left-grip-entry goes with --two-hand')
        for row in melee:   # a held left grip with the hands along the axe would be a two-hand hold (the thrust stroke)
            row[LEFT_GRAB] = '0'
    guard = load(a.directory / 'home-live-guard-20261011.csv')
    out, parts = [], {}

    def take(name, source, first, last):
        parts[name] = dict(offset=len(out) - (first - 1), first=len(out) + 1, last=len(out) + last - first + 1)
        out.extend(list(source[f - 1]) for f in range(first, last + 1))

    def blend(source_row, target_row, frames):   # hands glide to the next pose with neutral controls
        for i in range(1, frames + 1):
            row = list(target_row)
            for column in POSITION:
                u, v = float(source_row[column]), float(target_row[column])
                row[column] = repr(round(u + (v - u) * i / frames, 9))
            for column, value in CONTROLS.items():
                row[column] = value
            out.append(row)

    def still(row, frames, grab=range(0), stick=range(0), direction=('0', '0'), walk=range(0)):
        for i in range(frames):
            r = list(row)
            for column, value in CONTROLS.items():
                r[column] = value
            if i in walk:
                r[LEFT_STICK_Y] = '1'
            if i in grab:
                r[RIGHT_GRAB] = '1'
            if i in stick:
                r[RIGHT_STICK_X], r[RIGHT_STICK_Y] = direction
            out.append(r)

    # The bow comes back before the melee parts: every native melee attack steps the character
    # forward, while the Mock scenery replay keeps the draw camera where it was. After a few
    # attacks the shown weapon is metres from the player and a tracked shot is refused, as it
    # should be (combined run game005042). On a headset the view follows the character.
    def ranged():
        take('aim', aim, 371, 1420)
        blend(aim[1419], bow[430], 30)
        still(bow[430], 100, grab=range(0, 60), stick=range(20, 70), direction=BOW)   # quick wheel back to the bow, then settle
        take('bow_again', bow, 431, 800)

    def melee_parts():
        take('melee', melee, 371, 1450)
        take('guard', guard, 371, 1300)
        if a.two_hand:
            take('two_hand', load(a.directory / ('home-two-hand-leftgrip-20261011.csv' if a.left_grip_entry else 'home-two-hand-20261011.csv')), 371, 1800)

    if a.melee_guard_only:
        take('melee', melee, 1, 1450)
        take('guard', guard, 371, 1300)
        still(out[-1], 2600 - len(out))
        for index, row in enumerate(out, 1):
            row[0] = str(index)
        summary = dict(parts=parts, rows=len(out), stop_frame=parts['guard']['last'] + 60)
        lines = ['# Melee strokes and shield guard only (tools/make_combined_weapon_fixture.py --melee-guard-only).', '# ' + json.dumps({k: v['offset'] for k, v in parts.items()})]
        lines += [','.join(r) for r in out]
        a.output.write_text('\n'.join(lines) + '\n', encoding='utf-8', newline='\n')
        a.output.with_suffix('.json').write_text(json.dumps(summary, indent=2) + '\n', encoding='utf-8')
        print(a.output, json.dumps(summary))
        return
    take('bow', bow, 1, 1300)
    if a.ranged_after_melee:
        blend(bow[1299], melee[370], 30)
        melee_parts()
        still(guard[1299], 150, walk=range(20, 110))   # left stick forward for one second
        blend(guard[1299], aim[370], 30)
        ranged()
        final = 'bow_again'
    else:
        blend(bow[1299], aim[370], 30)
        ranged()
        blend(bow[799], melee[370], 30)
        melee_parts()
        final = 'two_hand' if a.two_hand else 'guard'
    still(out[-1], (6500 if a.two_hand else 5000) - len(out))
    for index, row in enumerate(out, 1):
        row[0] = str(index)
    again = parts['bow_again']['offset']
    summary = dict(parts=parts, rows=len(out),
                   bow=dict(releases=[720, 1010, 1230, 720 + again], holds=[[540, 720], [837, 1010], [1100, 1230], [540 + again, 720 + again]]),
                   stop_frame=min(len(out) - 20, parts[final]['last'] + 60))
    lines = ['# Every weapon route in one session (tools/make_combined_weapon_fixture.py). Part offsets are in the JSON beside this file.',
             '# ' + json.dumps({k: v['offset'] for k, v in parts.items()})]
    lines += [','.join(r) for r in out]
    a.output.write_text('\n'.join(lines) + '\n', encoding='utf-8', newline='\n')
    a.output.with_suffix('.json').write_text(json.dumps(summary, indent=2) + '\n', encoding='utf-8')
    print(a.output, json.dumps(summary))


if __name__ == '__main__':
    main()
