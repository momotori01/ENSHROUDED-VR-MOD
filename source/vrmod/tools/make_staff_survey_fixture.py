#!/usr/bin/env python3
"""Mock hand script that surveys native staff and wand trigger behaviour (read-only observers).

Selects the staff, then the wand, through the native quick wheel (same directions as
home-ranged-animation-20261010.csv) and applies four trigger patterns to each:
RT tap, RT hold, LT hold + RT tap, LT hold + RT hold. Hands stay still.
"""
import argparse, pathlib

LEFT_TRIGGER, RIGHT_TRIGGER, RIGHT_GRAB, RIGHT_STICK_X, RIGHT_STICK_Y, LEFT_PRIMARY = 10, 23, 24, 32, 33, 35
RIGHT_Y, RIGHT_Z, RIGHT_AIM_YAW, RIGHT_AIM_PITCH = 15, 16, 20, 21
STAFF = ('-0.9998476951563913', '-0.017452406437283477')
WAND = ('0.224951054343865', '0.9743700647852352')


def main():
    p = argparse.ArgumentParser()
    p.add_argument('--source', type=pathlib.Path, default=pathlib.Path('vrmod/test_env/home-ranged-animation-20261010.csv'))
    p.add_argument('--output', type=pathlib.Path, default=pathlib.Path('vrmod/test_env/home-staff-wand-survey-20261010.csv'))
    p.add_argument('--variant', type=int, choices=(1, 2, 3), default=1)
    a = p.parse_args()
    rows = [line.split(',') for line in a.source.read_text(encoding='utf-8').splitlines() if line and not line.startswith('#')]
    if len(rows) != 4300 or any(len(r) != 39 for r in rows):
        raise SystemExit('unexpected source script')
    for r in rows:   # neutral controls; poses and tracking flags stay as in the source
        r[LEFT_TRIGGER] = r[RIGHT_TRIGGER] = '0'
        r[RIGHT_GRAB] = '0'
        r[RIGHT_STICK_X] = r[RIGHT_STICK_Y] = '0'
        r[LEFT_PRIMARY] = '0'

    def span(first, last, column, value):
        for frame in range(first, last + 1):
            rows[frame - 1][column] = value

    def select(start, direction):   # grab, push the stick, release the grab, centre the stick
        span(start, start + 59, RIGHT_GRAB, '1')
        span(start + 20, start + 69, RIGHT_STICK_X, direction[0])
        span(start + 20, start + 69, RIGHT_STICK_Y, direction[1])

    def patterns(start):
        span(start, start + 2, RIGHT_TRIGGER, '1')                 # A: RT tap
        span(start + 80, start + 169, RIGHT_TRIGGER, '1')          # B: RT hold 90 frames
        span(start + 240, start + 359, LEFT_TRIGGER, '1')          # C: LT hold, RT tap inside
        span(start + 300, start + 302, RIGHT_TRIGGER, '1')
        span(start + 440, start + 579, LEFT_TRIGGER, '1')          # D: LT hold, RT hold inside
        span(start + 480, start + 539, RIGHT_TRIGGER, '1')

    select(400, STAFF)
    if a.variant == 3:
        # -RangedAim regression: two staff casts and two wand shots, each with a different
        # pointer direction and hand position. The wand's trigger is the native attack press.
        def aim(first, yaw, pitch, dy, dz):
            for frame in range(first, 4301):
                r = rows[frame - 1]
                r[RIGHT_AIM_YAW], r[RIGHT_AIM_PITCH] = yaw, pitch
                r[RIGHT_Y], r[RIGHT_Z] = repr(1.25 + dy), repr(0.25 + dz)

        if any(r[RIGHT_Y] != '1.25' or r[RIGHT_Z] != '0.25' for r in rows):
            raise SystemExit('source right hand position differs')
        aim(1, '-90', '0', 0.0, 0.0)
        span(560, 649, RIGHT_TRIGGER, '1')         # staff cast 1 (RT hold)
        aim(700, '-60', '15', 0.10, -0.05)
        span(760, 849, RIGHT_TRIGGER, '1')         # staff cast 2
        select(1000, WAND)
        span(1160, 1164, RIGHT_TRIGGER, '1')       # wand shot 1 (trigger -> native attack)
        aim(1230, '-110', '-10', 0.0, 0.0)
        span(1300, 1304, RIGHT_TRIGGER, '1')       # wand shot 2
        out = ['# -RangedAim regression: staff selected at 400, RT hold 560..649 and 760..849; wand selected at 1000, trigger 1160..1164 and 1300..1304.',
               '# Right pointer: yaw -90 pitch 0 until 699; yaw -60 pitch 15, hand +10 cm up / 5 cm in, 700..1229; yaw -110 pitch -10, hand back, from 1230.']
        out += [','.join(r) for r in rows]
        a.output.write_text('\n'.join(out) + '\n', encoding='utf-8', newline='\n')
        print(a.output)
        return
    select(1200, WAND)
    if a.variant == 1:
        patterns(560)
        patterns(1360)
        out = ['# Native trigger survey: staff selected at 400, patterns from 560; wand selected at 1200, patterns from 1360.',
               '# Per weapon: RT tap +0, RT hold +80..169, LT hold +240..359 with RT tap +300, LT hold +440..579 with RT hold +480..539.']
    else:
        # Variant 2 (-RangedSurvey maps the left primary face button to native X).
        span(560, 562, LEFT_PRIMARY, '1')          # staff: X tap
        span(640, 729, LEFT_PRIMARY, '1')          # staff: X hold
        span(800, 889, RIGHT_TRIGGER, '1')         # staff: RT hold (cast seen in variant 1)
        span(960, 1079, LEFT_TRIGGER, '1')         # staff: LT hold with RT tap
        span(1020, 1022, RIGHT_TRIGGER, '1')
        span(1360, 1362, LEFT_PRIMARY, '1')        # wand: X tap
        span(1440, 1529, LEFT_PRIMARY, '1')        # wand: X hold
        for start in (1600, 1620, 1640):           # wand: three X taps
            span(start, start + 2, LEFT_PRIMARY, '1')
        out = ['# Native X/trigger survey for -RangedSurvey: staff selected at 400, wand at 1200.',
               '# Staff: X tap 560, X hold 640..729, RT hold 800..889, LT 960..1079 with RT tap 1020. Wand: X tap 1360, X hold 1440..1529, X taps 1600/1620/1640.']
    out += [','.join(r) for r in rows]
    a.output.write_text('\n'.join(out) + '\n', encoding='utf-8', newline='\n')
    print(a.output)


if __name__ == '__main__':
    main()
