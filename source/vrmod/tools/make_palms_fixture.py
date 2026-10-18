#!/usr/bin/env python3
"""Mock hand script that shows the palms and the pads of the fingertips to the eyes, nothing pressed.

For looking at the gloves from the inside of the hand (headset run game-20261013-005905: "the pads of the fingertips
come through the gloves"; hooks/head_visibility_bridge.hpp leaves the copy of the bare hands out where the equipment
covers the hands). The head of the Mock stands at (0, 1.65, 0), looks along +X and 15 degrees down.

  1..419      both hands at rest (the usual place, backs of the hands up)
  420..699    palms up, level, 0.35 m ahead and 0.30 m under the eyes
  700..999    the fingers point 50 degrees up, the palms towards the face, a little higher and further
  1000..1200  at rest again
The grip's angles are yaw, pitch, roll in degrees (runtime/mock_actions.hpp: euler(yaw, pitch, roll)); the palm of
the right hand looks along the grip's -X, so a roll of -90 turns it up (the left hand: +90).
"""
import argparse, pathlib

LEFT, RIGHT = 1, 14            # first column of a hand: x, y, z, yaw, pitch, roll
PHASES = [(420, 700, (.35, 1.35, .12), 0.), (700, 1000, (.42, 1.45, .12), 50.)]   # first frame, end, place of the right hand, pitch


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument('--source', type=pathlib.Path, default=pathlib.Path('vrmod/test_env/home-idle-hands-20261013.csv'))
    p.add_argument('--output', type=pathlib.Path, default=pathlib.Path('vrmod/test_env/home-palms-up-20261013.csv'))
    a = p.parse_args()
    rows = [line.split(',') for line in a.source.read_text(encoding='utf-8').splitlines() if line and not line.startswith('#')]
    if not rows or any(len(r) != 39 for r in rows) or rows[0][LEFT + 3:LEFT + 6] != ['-90', '0', '0'] or rows[0][RIGHT + 3:RIGHT + 6] != ['-90', '0', '0']:
        raise SystemExit('unexpected source script')
    rest = rows[0]
    out = []
    for frame in range(1, 1201):
        r = list(rest); r[0] = str(frame)
        for first, end, (x, y, z), pitch in PHASES:
            if first <= frame < end:
                r[LEFT:LEFT + 6] = ['%g' % x, '%g' % y, '%g' % -z, '-90', '%g' % pitch, '90']
                r[RIGHT:RIGHT + 6] = ['%g' % x, '%g' % y, '%g' % z, '-90', '%g' % pitch, '-90']
        out.append(','.join(r))
    a.output.write_text('\n'.join(['# Palms and fingertip pads towards the eyes: level 420..699, fingers 50 degrees up 700..999; nothing pressed (tools/make_palms_fixture.py). Stop at 1100.'] + out) + '\n', encoding='utf-8', newline='\n')
    print(a.output, len(out), 'stop 1100, pictures 560,850')


if __name__ == '__main__':
    main()
