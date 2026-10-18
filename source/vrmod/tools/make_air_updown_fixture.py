#!/usr/bin/env python3
"""Mock hand script with the one-hand sword of the home save: a stroke straight UP, then one straight DOWN, each meant
for the air (a key script jumps 14 frames before: test_env/jump-updown-20261015.keys; or 30 frames before, when the
character is already falling: jump-updown-late-20261015.keys). The airborne attacks by the stroke's direction
(2026-10-15 afternoon, user: "swing up in the air for the rising slash, down for the crash-down"):
  600..669   wheel towards the third slot (129 degrees): the sword (item 1125155603)
  800..819   the hand goes up 20 frames of 0.05 m (4.5 m/s, 1.0 m): the rising slash (SlashUp) expected
  1100..1119 the hand goes down 20 frames of 0.05 m: the crash-down (CrashDown) expected
Each is followed by a slow return. Built on home-ranged-animation-20261010.csv like make_heavy_attack_fixture.py.
"""
import argparse, json, math, pathlib

RIGHT_X, RIGHT_Y, RIGHT_Z = 14, 15, 16
RIGHT_GRAB, RIGHT_STICK_X, RIGHT_STICK_Y = 24, 32, 33
SWORD = 129.0
MOVES = [(800, 20, (0.0, .05, 0.0)), (1100, 20, (0.0, -.05, 0.0))]   # first frame, frames, metres a frame


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--source', type=pathlib.Path, default=pathlib.Path('vrmod/test_env/home-ranged-animation-20261010.csv'))
    p.add_argument('--output', type=pathlib.Path, default=pathlib.Path('vrmod/test_env/home-air-updown-20261015.csv'))
    a = p.parse_args()
    rows = [line.split(',') for line in a.source.read_text(encoding='utf-8').splitlines() if line and not line.startswith('#')]
    if len(rows) != 4300 or any(len(r) != 39 for r in rows) or (rows[0][RIGHT_X], rows[0][RIGHT_Y], rows[0][RIGHT_Z]) != ('0.38', '1.25', '0.25'):
        raise SystemExit('unexpected source script')
    for r in rows:
        r[RIGHT_GRAB] = '0'; r[RIGHT_STICK_X] = '0'; r[RIGHT_STICK_Y] = '0'
    angle = math.radians(SWORD)
    for f in range(600, 670):
        rows[f - 1][RIGHT_GRAB] = '1'
    for f in range(620, 670):
        rows[f - 1][RIGHT_STICK_X], rows[f - 1][RIGHT_STICK_Y] = repr(math.cos(angle)), repr(math.sin(angle))
    step = {}
    for first, frames, delta in MOVES:
        for f in range(first, first + frames):
            step[f] = delta
        back = tuple(-c * frames / 120 for c in delta)          # the return: 120 frames, far below the swing speed
        for f in range(first + frames + 60, first + frames + 180):
            step[f] = back
    position = [0.38, 1.25, 0.25]
    for f in range(1, len(rows) + 1):
        d = step.get(f)
        if d:
            position = [position[i] + d[i] for i in range(3)]
        rows[f - 1][RIGHT_X], rows[f - 1][RIGHT_Y], rows[f - 1][RIGHT_Z] = (repr(round(c, 6)) for c in position)
    if max(abs(position[i] - (0.38, 1.25, 0.25)[i]) for i in range(3)) > 1e-4:
        raise SystemExit('the hand does not end where it started')
    meta = dict(select=600, moves=[dict(first=m[0], frames=m[1], metres_per_frame=m[2]) for m in MOVES], stop_frame=1500,
                sword=dict(item=1125155603), expected=dict(up=800, down=1100))
    out = ['# Airborne attacks by the stroke direction with the sword: up 800, down 1100 (tools/make_air_updown_fixture.py).', '# ' + json.dumps(meta)]
    out += [','.join(r) for r in rows]
    a.output.write_text('\n'.join(out) + '\n', encoding='utf-8', newline='\n')
    print(a.output)


if __name__ == '__main__':
    main()
