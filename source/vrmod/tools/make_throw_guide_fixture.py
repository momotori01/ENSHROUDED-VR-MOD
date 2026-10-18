#!/usr/bin/env python3
"""Mock hand and head scripts for the throw's guide (2026-10-18; include/evr/live_throw.hpp ThrowGuide).

The user's way of throwing: the right trigger gets the throw ready, the left hand held forward shows a guide line
along its own pointing and decides the direction, the right hand's swing with the trigger let go throws.
No save of the tests has a throwable: run the game with EVR_THROW_TEST_ANY_ITEM=1 (whatever is held counts as one).

The head stands still at (0, 1.65, 0) looking along +X, 15 degrees down (the first rows of
home-look-down-20261011-head.csv, kept for the whole script). Hand rows: Mock CSV of 39 columns
(runtime/mock_actions.hpp): left hand 1-13, right hand 14-26, focus 27, profile 28, sticks 29-34, face buttons 35-38.
  600         right trigger held (the throw got ready); the left hand held out from 600 to 640: (0.62, 1.45, -0.12),
              pointing along +X and 20 degrees up (aim yaw -90, pitch 20) - the guide shows
  700         a picture of both eyes (key script CAPTURE): the dots
  740..759    the right hand swings forward and up; the left arm pulled back to the side in 745..760 (as in a throw)
  760         the trigger let go while the hand moves: throw 1, along the guide (0.94, 0.34, 0)
  1000        trigger held with the left hand down at the side: no guide
  1040..1059  the swing forward and to the left; release in 1060: throw 2, along the swing (the left hand gave no direction)
"""
import argparse, math, pathlib

LEFT = 1; RIGHT = 14
POS, YAW, AIM, TRIGGER = 0, 3, 6, 9          # offsets within a hand's 13 columns


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument('--hands-source', type=pathlib.Path, default=pathlib.Path('vrmod/test_env/home-idle-hands-20261013.csv'))
    p.add_argument('--head-source', type=pathlib.Path, default=pathlib.Path('vrmod/test_env/home-look-down-20261011-head.csv'))
    p.add_argument('--hands', type=pathlib.Path, default=pathlib.Path('vrmod/test_env/throw-guide-hands-20261018.csv'))
    p.add_argument('--head', type=pathlib.Path, default=pathlib.Path('vrmod/test_env/throw-guide-head-20261018.csv'))
    p.add_argument('--frames', type=int, default=1400)
    a = p.parse_args()
    idle = [l.split(',') for l in a.hands_source.read_text(encoding='utf-8').splitlines() if l and not l.startswith('#')][0]
    head = [l.split(',') for l in a.head_source.read_text(encoding='utf-8').splitlines() if l and not l.startswith('#')][0]
    if len(idle) != 39 or idle[LEFT:LEFT + 3] != ['0.38', '1.25', '-0.25'] or idle[RIGHT:RIGHT + 3] != ['0.38', '1.25', '0.25'] or head[1:5] != ['0', '1.65', '0', '-90']:
        raise SystemExit('unexpected source scripts')
    fmt = lambda v: f'{v:.4f}'.rstrip('0').rstrip('.') if isinstance(v, float) else str(v)
    lerp = lambda a, b, t: tuple(x + (y - x) * t for x, y in zip(a, b))
    left_rest, left_out, left_back = (0.38, 1.25, -0.25), (0.62, 1.45, -0.12), (0.05, 1.2, -0.3)
    right_rest = (0.38, 1.25, 0.25)
    rows = []
    for f in range(1, a.frames + 1):
        r = list(idle); r[0] = str(f)
        left, left_aim, right, trigger = left_rest, (0.0, 0.0), right_rest, 0.0
        if 600 <= f < 1000:
            trigger = 1.0 if f < 760 else 0.0
            t = min(1.0, (f - 600) / 40)
            left = lerp(left_rest, left_out, t); left_aim = (-90.0, 20.0)
            if f >= 745: left = lerp(left_out, left_back, min(1.0, (f - 745) / 15))
            if 740 <= f < 760: right = tuple(x + d * (f - 739) for x, d in zip(right_rest, (0.03, 0.015, 0.0)))
            elif f >= 760: right = tuple(x + d * 20 for x, d in zip(right_rest, (0.03, 0.015, 0.0)))
        elif 1000 <= f < 1300:
            trigger = 1.0 if f < 1060 else 0.0
            left = left_back; left_aim = (-90.0, -40.0)
            if 1040 <= f < 1060: right = tuple(x + d * (f - 1039) for x, d in zip(right_rest, (0.02, 0.0, -0.03)))
            elif f >= 1060: right = tuple(x + d * 20 for x, d in zip(right_rest, (0.02, 0.0, -0.03)))
        for i in range(3):
            r[LEFT + POS + i] = fmt(float(left[i])); r[RIGHT + POS + i] = fmt(float(right[i]))
        r[LEFT + AIM] = fmt(left_aim[0]); r[LEFT + AIM + 1] = fmt(left_aim[1])
        r[RIGHT + TRIGGER] = fmt(trigger)
        rows.append(','.join(r))
    a.hands.write_text('# Throw guide (tools/make_throw_guide_fixture.py)\n' + '\n'.join(rows) + '\n', encoding='utf-8')
    a.head.write_text('# Still head for the throw guide (tools/make_throw_guide_fixture.py)\n' +
                      '\n'.join(','.join([str(f)] + head[1:]) for f in range(1, a.frames + 1)) + '\n', encoding='utf-8')
    print(f'wrote {a.hands} and {a.head}, {a.frames} frames')


if __name__ == '__main__':
    main()
