#!/usr/bin/env python3
"""Mock hand script for casting the fishing rod by a swing (include/evr/live_swing_cast.hpp; user decision 2026-10-12:
the rod casts at the moment the trigger is let go while swinging - a tip speed of 3.5 m/s at some time of the hold).

On the default (derived) home save the script starts with the bow of the first action bar page.
  420..539    wheel held, right stick click 460..466: second page
  600         wheel towards the fishing rod (slot 4 of the page)
  800..835    trigger held; 830..841 the wrist turns 4 degrees a frame (Mock hand frames are 1/90 s apart: 360 deg/s,
              about 11.9 m/s at a tip 1.89 m away); the trigger is let go at 836, in the middle of the flick: the cast
              has to come there, and live_fishing_cast with it
  2000..2059  trigger held; 2010..2021 the same flick, the trigger let go only at 2060, long after it: still a cast by
              the swing (what counts is the highest speed while the trigger was held), at 2060
  1100..1129  trigger held with a still hand, let go at 1130: the game's own cast (no cast by a swing)
  1400..1459  trigger held; 1410..1449 the wrist turns 0.5 degrees a frame (45 deg/s, about 1.5 m/s at the tip): too
              slow for a cast by the swing; letting go at 1460 casts
  1700..1711  the same flick without the trigger: nothing
Between the cases the wrist turns back slowly, the trigger free.
"""
import argparse, json, math, pathlib

RIGHT_YAW, RIGHT_PITCH = 17, 18
LEFT_STICK_CLICK = 31
LEFT_TRIGGER, LEFT_GRAB, RIGHT_TRIGGER, RIGHT_GRAB, RIGHT_STICK_X, RIGHT_STICK_Y, RIGHT_STICK_CLICK = 10, 11, 23, 24, 32, 33, 34
TRIGGERS = [(800, 836), (1100, 1130), (1400, 1460), (2000, 2060)]
TURNS = [(830, 842, 4.0), (1410, 1450, .5), (1700, 1712, 4.0), (2010, 2022, 4.0)]   # first frame, end, degrees a frame
RETURNS = [(950, 1050), (1500, 1600), (1800, 1900), (2150, 2250)]                   # the wrist turns back to level


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--source', type=pathlib.Path, default=pathlib.Path('vrmod/test_env/home-ranged-animation-20261010.csv'))
    p.add_argument('--output', type=pathlib.Path, default=pathlib.Path('vrmod/test_env/home-fishing-cast-20261012.csv'))
    p.add_argument('--side', action='store_true', help='the first flick (830..841) turns the wrist sideways instead of down: the cast has to go to that side (the sent camera looks along the swing, input_head_idle.hpp)')
    p.add_argument('--page-click', choices=('left', 'right'), default='left', help='which stick click swaps the action bar page while the wheel is held')
    a = p.parse_args()
    rows = [line.split(',') for line in a.source.read_text(encoding='utf-8').splitlines() if line and not line.startswith('#')]
    if len(rows) != 4300 or any(len(r) != 39 for r in rows) or rows[0][17] != '-90' or rows[0][RIGHT_PITCH] != '0':
        raise SystemExit('unexpected source script')
    for r in rows:
        for c in (LEFT_TRIGGER, LEFT_GRAB, RIGHT_TRIGGER, RIGHT_GRAB, LEFT_STICK_CLICK, RIGHT_STICK_X, RIGHT_STICK_Y, RIGHT_STICK_CLICK):
            r[c] = '0'

    def hold(column, first, end, value='1'):
        for f in range(first, end):
            rows[f - 1][column] = value

    hold(RIGHT_GRAB, 420, 540); hold(LEFT_STICK_CLICK if a.page_click == 'left' else RIGHT_STICK_CLICK, 460, 467)
    angle = math.radians(181 - 26 * 3)
    hold(RIGHT_GRAB, 600, 660)
    for f in range(620, 670):
        rows[f - 1][RIGHT_STICK_X], rows[f - 1][RIGHT_STICK_Y] = repr(math.cos(angle)), repr(math.sin(angle))
    for first, end in TRIGGERS:
        hold(RIGHT_TRIGGER, first, end)
    step = {}
    for first, end, degrees in TURNS:
        for f in range(first, end):
            step[f] = -degrees
    pitch = 0.0; side = 0.0
    for f in range(1, len(rows) + 1):
        sideways = a.side and TURNS[0][0] <= f < TURNS[0][1]
        if sideways:
            side += TURNS[0][2]
        else:
            pitch += step.get(f, 0.0)
        for first, end in RETURNS:
            if first <= f < end:
                pitch -= pitch / (end - f); side -= side / (end - f)
        rows[f - 1][RIGHT_PITCH] = repr(round(pitch, 6))
        rows[f - 1][RIGHT_YAW] = repr(round(-90 + side, 6))
    if abs(pitch) > 1e-6 or abs(side) > 1e-6:
        raise SystemExit('the wrist does not end level')
    meta = dict(page_swap=[420, 540], select=600, triggers=TRIGGERS, turns=TURNS, stop_frame=2350,
                expected=dict(cast_by_swing_at_release=[836, 2060], native_casts_without_swing_at=[1130, 1460], no_cast_at=1700))
    out = ['# Casting the fishing rod by a swing: trigger let go in the flick 836, still hand 1100..1130, slow turn 1400..1460, flick without the trigger 1700, flick then late release 2060 (tools/make_fishing_cast_fixture.py).',
           '# ' + json.dumps(meta)]
    out += [','.join(r) for r in rows]
    if a.side:
        a.output = a.output.with_name(a.output.stem + '-side.csv')
    a.output.write_text('\n'.join(out) + '\n', encoding='utf-8', newline='\n')
    print(a.output)


if __name__ == '__main__':
    main()
