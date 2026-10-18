#!/usr/bin/env python3
"""Mock hand script without any input: both hands at rest, no stick, trigger, grip or button.

For interactive sessions (-Interactive) in which the user moves the character with the keyboard: the usual home script
(home-ranged-animation-20261010.csv) pulls the trigger and opens the quick wheel, which would disturb the walk.
"""
import argparse, pathlib

LEFT_TRIGGER, LEFT_GRAB, RIGHT_TRIGGER, RIGHT_GRAB = 10, 11, 23, 24
LEFT_STICK_X, LEFT_STICK_Y, RIGHT_STICK_X, RIGHT_STICK_Y, RIGHT_SECONDARY = 29, 30, 32, 33, 38


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--hands-source', type=pathlib.Path, default=pathlib.Path('vrmod/test_env/home-ranged-animation-20261010.csv'))
    p.add_argument('--name', default='home-still-hands-20261011')
    a = p.parse_args()
    rows = [line.split(',') for line in a.hands_source.read_text(encoding='utf-8').splitlines() if line and not line.startswith('#')]
    if len(rows) != 4300 or any(len(r) != 39 for r in rows) or len({tuple(r[1:4] + r[14:17]) for r in rows}) != 1:
        raise SystemExit('unexpected hand source')
    for r in rows:
        for column in (LEFT_TRIGGER, LEFT_GRAB, RIGHT_TRIGGER, RIGHT_GRAB, LEFT_STICK_X, LEFT_STICK_Y, RIGHT_STICK_X, RIGHT_STICK_Y, RIGHT_SECONDARY):
            r[column] = '0'
    out = pathlib.Path('vrmod/test_env') / (a.name + '.csv')
    out.write_text('\n'.join(['# Hands at rest, no input at all: for sessions in which the user walks with the keyboard (tools/make_still_hands_fixture.py).'] + [','.join(r) for r in rows]) + '\n', encoding='utf-8', newline='\n')
    print(out, len(rows))


if __name__ == '__main__':
    main()
