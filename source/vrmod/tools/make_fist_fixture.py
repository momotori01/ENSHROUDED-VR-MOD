#!/usr/bin/env python3
"""Mock hand script for bare hands (VHVR FistCollision): the home save has no empty slot, so the three bombs of the
second action bar page are thrown first; the emptied slot leaves the hands bare.

Use it with -QuickSlotMotion (the right grip is free there; the wheel is the right upper button):
  420..539    right upper button held (wheel), right stick click 460..466: second page, the bombs
  700, 900, 1100   three throws (trigger held 45 frames, the hand moves forward and up, release while moving)
  1400        right punch, grip open                      -> nothing (an empty hand must be clenched)
  1500..1579  right grip held, punch at 1520              -> attack, right hand
  1700..1779  left grip held, punch at 1720               -> attack, left hand
  1900..1979  right grip held, slow push at 1920 (0.9 m/s) -> nothing (below 0.45 x 3 m/s)
  2100..2199  both grips held, right punch 2110, left punch 2160 -> two attacks
A punch is 8 frames of 0.03 m forward (2.7 m/s; Mock hand frames are 1/90 s apart), then a return at a third of
that speed (0.9 m/s: below the requirement, so the way back is no second punch).
"""
import argparse, json, pathlib

LEFT_X, RIGHT_X, RIGHT_Y = 1, 14, 15
LEFT_TRIGGER, LEFT_GRAB, RIGHT_TRIGGER, RIGHT_GRAB, RIGHT_STICK_X, RIGHT_STICK_Y, RIGHT_STICK_CLICK, RIGHT_SECONDARY = 10, 11, 23, 24, 32, 33, 34, 38
THROWS = [700, 900, 1100]
PUNCHES = [dict(frame=1400, hand=1, grip=None, step=.03, attack=False), dict(frame=1520, hand=1, grip=(1, 1500, 1580), step=.03, attack=True),
           dict(frame=1720, hand=0, grip=(0, 1700, 1780), step=.03, attack=True), dict(frame=1920, hand=1, grip=(1, 1900, 1980), step=.01, attack=False),
           dict(frame=2110, hand=1, grip=(1, 2100, 2200), step=.03, attack=True), dict(frame=2160, hand=0, grip=(0, 2100, 2200), step=.03, attack=True)]


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--source', type=pathlib.Path, default=pathlib.Path('vrmod/test_env/home-ranged-animation-20261010.csv'))
    p.add_argument('--output', type=pathlib.Path, default=pathlib.Path('vrmod/test_env/home-fists-20261012.csv'))
    a = p.parse_args()
    rows = [line.split(',') for line in a.source.read_text(encoding='utf-8').splitlines() if line and not line.startswith('#')]
    if len(rows) != 4300 or any(len(r) != 39 for r in rows) or rows[0][RIGHT_X:RIGHT_X + 3] != ['0.38', '1.25', '0.25'] or rows[0][LEFT_X:LEFT_X + 3] != ['0.38', '1.25', '-0.25']:
        raise SystemExit('unexpected source script')
    for r in rows:
        for c in (LEFT_TRIGGER, LEFT_GRAB, RIGHT_TRIGGER, RIGHT_GRAB, RIGHT_STICK_X, RIGHT_STICK_Y, RIGHT_STICK_CLICK, RIGHT_SECONDARY):
            r[c] = '0'
    for f in range(420, 540):
        rows[f - 1][RIGHT_SECONDARY] = '1'
    for f in range(460, 467):
        rows[f - 1][RIGHT_STICK_CLICK] = '1'
    moves = {}                         # (frame, column) -> step

    def move(column, first, frames, step):
        for f in range(first, first + frames):
            if (f, column) in moves:
                raise SystemExit('overlapping move')
            moves[(f, column)] = step

    for t in THROWS:
        for f in range(t, t + 45):
            rows[f - 1][RIGHT_TRIGGER] = '1'
        move(RIGHT_X, t + 30, 20, .03); move(RIGHT_Y, t + 30, 20, .015)
        move(RIGHT_X, t + 70, 80, -.03 * 20 / 80); move(RIGHT_Y, t + 70, 80, -.015 * 20 / 80)
    for q in PUNCHES:
        column = RIGHT_X if q['hand'] else LEFT_X
        move(column, q['frame'], 8, q['step']); move(column, q['frame'] + 12, 24, -q['step'] * 8 / 24)   # back at a third of the speed
        if q['grip']:
            hand, first, end = q['grip']
            for f in range(first, end):
                rows[f - 1][RIGHT_GRAB if hand else LEFT_GRAB] = '1'
    at = {LEFT_X: .38, RIGHT_X: .38, RIGHT_Y: 1.25}
    for f in range(1, len(rows) + 1):
        for column in at:
            at[column] += moves.get((f, column), 0.0)
            rows[f - 1][column] = repr(round(at[column], 9))
    if max(abs(at[LEFT_X] - .38), abs(at[RIGHT_X] - .38), abs(at[RIGHT_Y] - 1.25)) > 1e-6:
        raise SystemExit(f'hands do not return: {at}')
    meta = dict(page_swap=[420, 540], throws=THROWS, punches=PUNCHES, stop_frame=2350)
    out = ['# Bare hands: second page 420 (right upper button + stick click), three throws 700/900/1100, punches 1400 (open hand), 1520 right, 1720 left, 1920 slow, 2110 right + 2160 left (tools/make_fist_fixture.py).',
           '# ' + json.dumps(meta)]
    out += [','.join(r) for r in rows]
    a.output.write_text('\n'.join(out) + '\n', encoding='utf-8', newline='\n')
    print(a.output)


if __name__ == '__main__':
    main()
