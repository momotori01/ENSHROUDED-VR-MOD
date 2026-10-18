#!/usr/bin/env python3
"""Mock hand script for a building piece in the hand (the crucible of the home save), with -QuickSlotMotion: the quick
slot is the right upper button plus hand motion, the right grip is free and is the game's modifier for the piece.

The Mock head looks along +X, the user's right is +Z. Slot k of a page lies at 181 - 26 k degrees on the wheel.
  420..539    right upper button held (wheel), right stick click 460..466: second page
  600..669    right upper button held; 625..644 the right hand moves 12 cm towards 25 degrees (the crucible, slot 7
              of the page); released at 670: the crucible is taken; 680..699 hand back
  800..899    right grip held (modifier); right stick to the right 820..829: the piece turns by 90 degrees
  950..959    upper left button: the second action (the game's LB)
  1050..1059  right trigger: the piece is placed
"""
import argparse, json, math, pathlib

RIGHT = (14, 15, 16)
LEFT_TRIGGER, LEFT_GRAB, RIGHT_TRIGGER, RIGHT_GRAB, RIGHT_STICK_X, RIGHT_STICK_Y, RIGHT_STICK_CLICK = 10, 11, 23, 24, 32, 33, 34
LEFT_PRIMARY, LEFT_SECONDARY, RIGHT_PRIMARY, RIGHT_SECONDARY = 35, 36, 37, 38
CRUCIBLE_DEGREES, REACH = 181 - 26 * 6, .12


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--source', type=pathlib.Path, default=pathlib.Path('vrmod/test_env/home-ranged-animation-20261010.csv'))
    p.add_argument('--output', type=pathlib.Path, default=pathlib.Path('vrmod/test_env/home-piece-20261012.csv'))
    a = p.parse_args()
    rows = [line.split(',') for line in a.source.read_text(encoding='utf-8').splitlines() if line and not line.startswith('#')]
    if len(rows) != 4300 or any(len(r) != 39 for r in rows) or rows[0][14:17] != ['0.38', '1.25', '0.25']:
        raise SystemExit('unexpected source script')
    for r in rows:
        for c in (LEFT_TRIGGER, LEFT_GRAB, RIGHT_TRIGGER, RIGHT_GRAB, RIGHT_STICK_X, RIGHT_STICK_Y, RIGHT_STICK_CLICK, LEFT_PRIMARY, LEFT_SECONDARY, RIGHT_PRIMARY, RIGHT_SECONDARY):
            r[c] = '0'

    def hold(column, first, end):
        for f in range(first, end):
            rows[f - 1][column] = '1'

    hold(RIGHT_SECONDARY, 420, 540); hold(RIGHT_STICK_CLICK, 460, 467)
    hold(RIGHT_SECONDARY, 600, 670)
    angle = math.radians(CRUCIBLE_DEGREES)
    step = (0.0, REACH * math.sin(angle) / 20, REACH * math.cos(angle) / 20)     # x forward, y up, z right
    moves = {}
    for f in range(625, 645):
        moves[f] = step
    for f in range(680, 700):
        moves[f] = tuple(-s for s in step)
    at = [0.38, 1.25, 0.25]
    for f in range(1, len(rows) + 1):
        m = moves.get(f, (0.0, 0.0, 0.0))
        at = [at[i] + m[i] for i in range(3)]
        for i, c in enumerate(RIGHT):
            rows[f - 1][c] = repr(round(at[i], 9))
    if max(abs(at[0] - .38), abs(at[1] - 1.25), abs(at[2] - .25)) > 1e-6:
        raise SystemExit('hand does not return')
    hold(RIGHT_GRAB, 800, 900); hold(RIGHT_STICK_X, 820, 830)
    hold(LEFT_SECONDARY, 950, 960)
    hold(RIGHT_TRIGGER, 1050, 1060)
    meta = dict(page_swap=[420, 540], quick_slot=[600, 670], modifier=[800, 900], turn=[820, 830], second=[950, 960], place=[1050, 1060], stop_frame=1200)
    out = ['# Building piece (crucible) with -QuickSlotMotion: second page 420, crucible 600, grip 800..899 with stick right 820, upper left button 950, right trigger 1050 (tools/make_piece_fixture.py).',
           '# ' + json.dumps(meta)]
    out += [','.join(r) for r in rows]
    a.output.write_text('\n'.join(out) + '\n', encoding='utf-8', newline='\n')
    print(a.output)


if __name__ == '__main__':
    main()
