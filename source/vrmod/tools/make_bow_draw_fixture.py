#!/usr/bin/env python3
"""Three-shot Mock hand script for -BowDraw, derived from the reviewed one-shot script.

Shot 1 is the source unchanged (press at the string, 35 cm pull, release at 720).
Shot 2 presses the trigger 30 cm away from the string and then approaches it (VHVR
takes the string once the held hand is within 20 cm), overdraws to about 41 cm, release at 1010.
Shot 3 is a shorter pull (source rows 551..590), release at 1230.
Rows are edited as text so the integer columns keep their integer form.
"""
import argparse, pathlib

RIGHT_POSITION = (14, 15, 16)
RIGHT_TRIGGER = 23


def main():
    p = argparse.ArgumentParser()
    p.add_argument('--source', type=pathlib.Path, default=pathlib.Path('vrmod/test_env/home-bow-tracked-angled-20261010.csv'))
    p.add_argument('--output', type=pathlib.Path, default=pathlib.Path('vrmod/test_env/home-bow-draw-three-shots-20261010.csv'))
    a = p.parse_args()
    rows = [line.split(',') for line in a.source.read_text(encoding='utf-8').splitlines() if line and not line.startswith('#')]
    if len(rows) != 4300 or any(len(r) != 39 for r in rows) or [int(r[0]) for r in rows] != list(range(1, 4301)):
        raise SystemExit('unexpected source script')

    def position(frame):
        return [float(rows[frame - 1][c]) for c in RIGHT_POSITION]

    def trigger(frame):
        return float(rows[frame - 1][RIGHT_TRIGGER])

    # The source facts this script depends on.
    if trigger(539) != 0 or trigger(540) != 1 or trigger(719) != 1 or trigger(720) != 0 or position(790) != position(500) or position(620) != position(719):
        raise SystemExit('source timeline differs')
    nock, far = position(790), position(1)
    span = sum((f - n) ** 2 for f, n in zip(far, nock)) ** .5
    away = [n + (f - n) * (.30 / span) for f, n in zip(far, nock)]

    def lerp(u, v, t):
        return [x + (y - x) * t for x, y in zip(u, v)]

    plan = {}  # frame -> (position, trigger)
    for frame in range(791, 811):   # leave the string, trigger up
        plan[frame] = (lerp(nock, away, (frame - 790) / 20), 0)
    for frame in range(811, 831):   # press away from the string: nothing is taken
        plan[frame] = (away, 1)
    for frame in range(831, 851):   # approach with the trigger held: taken inside 20 cm
        plan[frame] = (lerp(away, nock, (frame - 830) / 20), 1)
    # Shot 2 overdraws: the source path stretched 1.15x about the string (about 41 cm), past
    # the 0.6 m full draw, so the full-draw pulse and the held-back string are exercised.
    def stretched(frame):
        return lerp(nock, position(frame), 1.15)

    for i, frame in enumerate(range(851, 921)):
        plan[frame] = (stretched(551 + i), 1)
    for frame in range(921, 1010):
        plan[frame] = (stretched(620), 1)
    plan[1010] = (stretched(620), 0)   # release
    for i, frame in enumerate(range(1011, 1081)):
        plan[frame] = (stretched(721 + i), 0)
    for frame in range(1081, 1100):
        plan[frame] = (nock, 0)
    for frame in range(1100, 1111):   # press at the string
        plan[frame] = (nock, 1)
    for i, frame in enumerate(range(1111, 1151)):   # shorter pull
        plan[frame] = (position(551 + i), 1)
    for frame in range(1151, 1230):
        plan[frame] = (position(590), 1)
    plan[1230] = (position(590), 0)   # release
    for i, frame in enumerate(range(1231, 1271)):
        plan[frame] = (position(590 - i - 1) if 590 - i - 1 >= 551 else nock, 0)

    template = rows[789]
    out = ['# Three shots for -BowDraw: left bow yaw +20 degrees and pitch -10 (source home-bow-tracked-angled-20261010).',
           '# Releases at 720, 1010 (trigger pressed 30 cm away, then approach, 41 cm overdraw) and 1230 (shorter pull). No wheel input.']
    for frame in range(1, 4301):
        if frame <= 790:
            row = list(rows[frame - 1])
        else:
            row = list(template)
            row[0] = str(frame)
            where, pressed = plan.get(frame, (nock, 0))
            for column, value in zip(RIGHT_POSITION, where):
                row[column] = repr(float(value))
            row[RIGHT_TRIGGER] = '1.0' if pressed else '0.0'
        out.append(','.join(row))
    a.output.write_text('\n'.join(out) + '\n', encoding='utf-8', newline='\n')
    print(a.output, len(out) - 2, 'rows; away', [round(x, 4) for x in away], 'nock', nock)


if __name__ == '__main__':
    main()
