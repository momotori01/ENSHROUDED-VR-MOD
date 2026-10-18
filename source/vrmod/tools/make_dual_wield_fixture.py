#!/usr/bin/env python3
"""Mock hand script for the weapon pair (daggers) of the home save: one weapon in each hand, either hand strikes.

On the default (derived) home save the script starts with the bow of the first action bar page:
  420..539   wheel held, right stick click 460..466: second page (RB + right stick click)
  600..669   wheel towards the sixth slot of the page (51 degrees): the dagger pair
  strokes    8 frames of 0.05 m (4.5 m/s; Mock hand frames are 1/90 s apart), then a slow return:
             right hand across at 800 and 1200, left hand across at 1000, both hands at 1400,
             a slow right sweep (2.25 m/s, no attack) at 1600.
"""
import argparse, json, math, pathlib

LEFT_X, LEFT_Z, RIGHT_X, RIGHT_Z = 1, 3, 14, 16
LEFT_TRIGGER, LEFT_GRAB, RIGHT_TRIGGER, RIGHT_GRAB, RIGHT_STICK_X, RIGHT_STICK_Y, RIGHT_STICK_CLICK = 10, 11, 23, 24, 32, 33, 34
DAGGERS = 51.0


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--source', type=pathlib.Path, default=pathlib.Path('vrmod/test_env/home-ranged-animation-20261010.csv'))
    p.add_argument('--output', type=pathlib.Path, default=pathlib.Path('vrmod/test_env/home-dual-wield-20261012.csv'))
    a = p.parse_args()
    rows = [line.split(',') for line in a.source.read_text(encoding='utf-8').splitlines() if line and not line.startswith('#')]
    if len(rows) != 4300 or any(len(r) != 39 for r in rows) or rows[0][RIGHT_X:RIGHT_Z + 1] != ['0.38', '1.25', '0.25'] or rows[0][LEFT_X:LEFT_Z + 1] != ['0.38', '1.25', '-0.25']:
        raise SystemExit('unexpected source script')
    for r in rows:
        for c in (LEFT_TRIGGER, LEFT_GRAB, RIGHT_TRIGGER, RIGHT_GRAB, RIGHT_STICK_X, RIGHT_STICK_Y, RIGHT_STICK_CLICK):
            r[c] = '0'
    for f in range(420, 540):
        rows[f - 1][RIGHT_GRAB] = '1'
    for f in range(460, 467):
        rows[f - 1][RIGHT_STICK_CLICK] = '1'
    angle = math.radians(DAGGERS)
    for f in range(600, 660):
        rows[f - 1][RIGHT_GRAB] = '1'
    for f in range(620, 670):
        rows[f - 1][RIGHT_STICK_X], rows[f - 1][RIGHT_STICK_Y] = repr(math.cos(angle)), repr(math.sin(angle))
    z = {0: -0.25, 1: 0.25}            # hand -> current z
    moves = {}                         # (frame, hand) -> dz

    def move(hand, first, frames, step):
        for f in range(first, first + frames):
            if (f, hand) in moves:
                raise SystemExit('overlapping move')
            moves[(f, hand)] = step

    def stroke(hand, first, step=.05, frames=8):
        sign = -1 if hand == 1 else 1   # each hand strikes across, towards the middle
        move(hand, first, frames, sign * step)
        move(hand, first + 20, 40, -sign * step * frames / 40)

    stroke(1, 800); stroke(0, 1000); stroke(1, 1200); stroke(1, 1400); stroke(0, 1400); stroke(1, 1600, .025, 10)
    for f in range(1, len(rows) + 1):
        for hand, column in ((0, LEFT_Z), (1, RIGHT_Z)):
            z[hand] += moves.get((f, hand), 0.0)
            rows[f - 1][column] = repr(round(z[hand], 9))
    if abs(z[0] + .25) > 1e-6 or abs(z[1] - .25) > 1e-6:
        raise SystemExit(f'hands do not return: {z}')
    meta = dict(page_swap=[420, 540], wheel=[600, 670], strokes=[dict(frame=800, hand='right'), dict(frame=1000, hand='left'), dict(frame=1200, hand='right'),
                dict(frame=1400, hand='both'), dict(frame=1600, hand='right', slow=True)], stop_frame=1800)
    out = ['# Weapon pair (daggers): second page 420, pair selected 600, strokes right 800, left 1000, right 1200, both 1400, slow right 1600 (tools/make_dual_wield_fixture.py).',
           '# ' + json.dumps(meta)]
    out += [','.join(r) for r in rows]
    a.output.write_text('\n'.join(out) + '\n', encoding='utf-8', newline='\n')
    print(a.output)


if __name__ == '__main__':
    main()
