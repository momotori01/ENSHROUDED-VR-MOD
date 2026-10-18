#!/usr/bin/env python3
"""Mock scripts (hands and head) for looking all around outdoors, for comparing the two eyes' pictures.

User, 2026-10-13: "in the last test I once saw a distant tree that was shown to one eye only ... if all the fixes are
done before I am back, look around outside the house and check whether anything is drawn differently for the two
eyes." The walk out of the home is the one of make_decal_outdoor_fixture.py (right until the wall, then forward
through the door to the grass); nothing is pressed after that and the hands stay at rest. Then the head turns in
steps of 30 degrees, twice round: once a little below level (5 degrees down), once 15 degrees up (tree tops, the far
hills). It stays 60 frames in every direction; the picture is taken 5 frames before it turns on.
  600..709    left stick right, 710..837 forward
  900..1619   first round (12 directions), 1620..2339 second round
Stop at 2350. tools/check_eye_difference.py reads the pictures.
"""
import argparse, pathlib

LEFT_STICK_X, LEFT_STICK_Y = 29, 30
START, DWELL, STEPS = 900, 60, 12
ROUNDS = (-5., 15.)


def frames():
    return [START + DWELL * k + DWELL - 5 for k in range(STEPS * len(ROUNDS))]


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument('--walk', type=pathlib.Path, default=pathlib.Path('vrmod/test_env/home-decal-outdoor-20261012.csv'))
    p.add_argument('--head', type=pathlib.Path, default=pathlib.Path('vrmod/test_env/home-decal-outdoor-20261012-head.csv'))
    p.add_argument('--output', type=pathlib.Path, default=pathlib.Path('vrmod/test_env/home-scenery-survey-20261013.csv'))
    a = p.parse_args()
    walk = [line.split(',') for line in a.walk.read_text(encoding='utf-8').splitlines() if line and not line.startswith('#')]
    head = [line.split(',') for line in a.head.read_text(encoding='utf-8').splitlines() if line and not line.startswith('#')]
    if any(len(r) != 39 for r in walk) or walk[757][LEFT_STICK_Y] != '1.0' or head[0][4] != '-90':
        raise SystemExit('unexpected source scripts')
    end = START + DWELL * STEPS * len(ROUNDS)
    rest = walk[0]; out = []
    for frame in range(1, end + 20):
        r = list(rest); r[0] = str(frame)
        if frame < 900:
            r[LEFT_STICK_X:LEFT_STICK_Y + 1] = walk[frame - 1][LEFT_STICK_X:LEFT_STICK_Y + 1]
        out.append(','.join(r))
    a.output.write_text('\n'.join(['# Looking all around outdoors: walk 600..837, then 24 directions of 60 frames from 900 (tools/make_scenery_survey_fixture.py). Stop at %d.' % (end + 10)] + out) + '\n', encoding='utf-8', newline='\n')
    level = head[0]; rows = []
    for frame in range(1, len(head) + 1):
        r = list(level); r[0] = str(frame)
        if START <= frame < end:
            k = (frame - START) // DWELL
            r[4] = '%g' % (-90 + 30 * (k % STEPS)); r[5] = '%g' % ROUNDS[k // STEPS]
        rows.append(','.join(r))
    target = a.output.with_name(a.output.stem + '-head.csv')
    target.write_text('\n'.join(['# Head script for the look all around (tools/make_scenery_survey_fixture.py): 12 directions 5 degrees down, 12 directions 15 degrees up.'] + rows) + '\n', encoding='utf-8', newline='\n')
    print(a.output, target, 'stop', end + 10)
    print('pictures', ','.join(str(f) for f in frames()))


if __name__ == '__main__':
    main()
