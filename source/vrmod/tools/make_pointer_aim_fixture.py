#!/usr/bin/env python3
"""Mock hand script for placing by hand (include/evr/pointer_aim_camera.hpp): the piece follows the right hand's pointer.

On the default (derived) home save the script starts with the bow of the first action bar page.
  420..539    wheel held, right stick click 460..466: second page
  --mode piece (default): 600 wheel towards the crucible (slot 7 of the page), a building piece in the hand
  --mode build:           600 wheel towards the building hammer (slot 5), left trigger 700..729: building mode
  --mode pickaxe:         no change of page; 600 wheel towards the pickaxe of the first page (slot 7): a swung tool,
                          which aims with the right hand's pointer too (user decision 2026-10-12); no trigger
  then the right hand's pointer turns (the Mock's aim columns of the hand: yaw, pitch; yaw -90 is straight ahead like
  the head's, negative pitch is down; the other scripts leave the pointer at 0 0 0, which points to the left):
    800   straight ahead, level
    900   30 degrees down
    1000  30 degrees down, 30 degrees to one side (yaw -60)
    1100  30 degrees down, 30 degrees to the other side (yaw -120)
    1200  the same aim; left stick forward 1210..1239: the walk has to go where the head looks, not where the hand points
    1300  45 degrees down, straight ahead; right trigger 1350..1359: place (piece) / build (building mode)
"""
import argparse, json, math, pathlib

RIGHT_YAW, RIGHT_PITCH = 20, 21   # the aim pose of the right hand (17, 18 are the grip's)
LEFT_TRIGGER, LEFT_GRAB, RIGHT_TRIGGER, RIGHT_GRAB, LEFT_STICK_X, LEFT_STICK_Y, RIGHT_STICK_X, RIGHT_STICK_Y, RIGHT_STICK_CLICK = 10, 11, 23, 24, 29, 30, 32, 33, 34
AIMS = [(800, -90, 0), (900, -90, -30), (1000, -60, -30), (1100, -120, -30), (1300, -90, -45)]


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--mode', choices=('piece', 'build', 'pickaxe'), default='piece')
    p.add_argument('--source', type=pathlib.Path, default=pathlib.Path('vrmod/test_env/home-ranged-animation-20261010.csv'))
    p.add_argument('--output', type=pathlib.Path)
    a = p.parse_args()
    output = a.output or pathlib.Path(f'vrmod/test_env/home-pointer-aim-{a.mode}-20261012.csv')
    rows = [line.split(',') for line in a.source.read_text(encoding='utf-8').splitlines() if line and not line.startswith('#')]
    if len(rows) != 4300 or any(len(r) != 39 for r in rows) or rows[0][RIGHT_YAW] != '0' or rows[0][RIGHT_PITCH] != '0' or rows[0][17] != '-90':
        raise SystemExit('unexpected source script')
    for r in rows:
        for c in (LEFT_TRIGGER, LEFT_GRAB, RIGHT_TRIGGER, RIGHT_GRAB, LEFT_STICK_X, LEFT_STICK_Y, RIGHT_STICK_X, RIGHT_STICK_Y, RIGHT_STICK_CLICK):
            r[c] = '0'

    def hold(column, first, end, value='1'):
        for f in range(first, end):
            rows[f - 1][column] = value

    if a.mode != 'pickaxe':
        hold(RIGHT_GRAB, 420, 540); hold(RIGHT_STICK_CLICK, 460, 467)
    angle = math.radians(181 - 26 * {'piece': 6, 'build': 4, 'pickaxe': 6}[a.mode])
    hold(RIGHT_GRAB, 600, 660)
    for f in range(620, 670):
        rows[f - 1][RIGHT_STICK_X], rows[f - 1][RIGHT_STICK_Y] = repr(math.cos(angle)), repr(math.sin(angle))
    if a.mode == 'build':
        hold(LEFT_TRIGGER, 700, 730)
    # the hand turns over 20 frames into each aim and holds it
    yaw, pitch = -90.0, 0.0
    targets = {first: (y, pi) for first, y, pi in AIMS}
    plan = {}
    for first, (y, pi) in sorted(targets.items()):
        for i in range(20):
            t = (i + 1) / 20
            plan[first + i] = (yaw + (y - yaw) * t, pitch + (pi - pitch) * t)
        yaw, pitch = float(y), float(pi)
    current = (-90.0, 0.0)
    for f in range(1, len(rows) + 1):
        current = plan.get(f, current)
        if f >= 1500:
            current = (-90.0, 0.0)
        rows[f - 1][RIGHT_YAW], rows[f - 1][RIGHT_PITCH] = repr(round(current[0], 6)), repr(round(current[1], 6))
    hold(LEFT_STICK_Y, 1210, 1240)
    if a.mode != 'pickaxe':
        hold(RIGHT_TRIGGER, 1350, 1360)
    meta = dict(mode=a.mode, page_swap=[420, 540], select=600, aims=AIMS, walk=[1210, 1240], use=[1350, 1360], stop_frame=1500)
    out = [f'# Placing by hand ({a.mode}): aims 800 ahead, 900 down 30, 1000 yaw -60, 1100 yaw -120, walk 1210, 1300 down 45, trigger 1350 (tools/make_pointer_aim_fixture.py).', '# ' + json.dumps(meta)]
    out += [','.join(r) for r in rows]
    output.write_text('\n'.join(out) + '\n', encoding='utf-8', newline='\n')
    print(output)


if __name__ == '__main__':
    main()
