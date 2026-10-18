#!/usr/bin/env python3
"""Short Mock hand script: the one-hand sword of the home save, two strokes. For looking at what a stroke shows
(the white arc of the swing, user note 2026-10-11) without the long combined script.

On the default (derived) home save the script starts with the bow of the first action bar page:
  600..669   wheel towards the third slot (129 degrees): the sword
  800, 1000  strokes of the right hand across (8 frames of 0.05 m = 4.5 m/s; Mock hand frames are 1/90 s apart),
             each followed by a slow return
"""
import argparse, json, math, pathlib

RIGHT_Z = 16
LEFT_TRIGGER, LEFT_GRAB, RIGHT_TRIGGER, RIGHT_GRAB, RIGHT_STICK_X, RIGHT_STICK_Y, RIGHT_STICK_CLICK = 10, 11, 23, 24, 32, 33, 34
SWORD = 129.0
STROKES = [800, 1000]


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--source', type=pathlib.Path, default=pathlib.Path('vrmod/test_env/home-ranged-animation-20261010.csv'))
    p.add_argument('--output', type=pathlib.Path, default=pathlib.Path('vrmod/test_env/home-sword-strokes-20261012.csv'))
    a = p.parse_args()
    rows = [line.split(',') for line in a.source.read_text(encoding='utf-8').splitlines() if line and not line.startswith('#')]
    if len(rows) != 4300 or any(len(r) != 39 for r in rows) or rows[0][RIGHT_Z] != '0.25':
        raise SystemExit('unexpected source script')
    for r in rows:
        for c in (LEFT_TRIGGER, LEFT_GRAB, RIGHT_TRIGGER, RIGHT_GRAB, RIGHT_STICK_X, RIGHT_STICK_Y, RIGHT_STICK_CLICK):
            r[c] = '0'
    angle = math.radians(SWORD)
    for f in range(600, 660):
        rows[f - 1][RIGHT_GRAB] = '1'
    for f in range(620, 670):
        rows[f - 1][RIGHT_STICK_X], rows[f - 1][RIGHT_STICK_Y] = repr(math.cos(angle)), repr(math.sin(angle))
    moves = {}
    for first in STROKES:
        for f in range(first, first + 8):
            moves[f] = -.05
        for f in range(first + 20, first + 60):
            moves[f] = .05 * 8 / 40
    z = .25
    for f in range(1, len(rows) + 1):
        z += moves.get(f, 0.0)
        rows[f - 1][RIGHT_Z] = repr(round(z, 9))
    if abs(z - .25) > 1e-6:
        raise SystemExit('hand does not return')
    meta = dict(wheel=[600, 670], strokes=STROKES, stop_frame=1150)
    out = ['# One-hand sword: selected 600, strokes of the right hand at 800 and 1000 (tools/make_sword_stroke_fixture.py).', '# ' + json.dumps(meta)]
    out += [','.join(r) for r in rows]
    a.output.write_text('\n'.join(out) + '\n', encoding='utf-8', newline='\n')
    print(a.output)


if __name__ == '__main__':
    main()
