#!/usr/bin/env python3
"""Mock hand script for VHVR's secondary attack by motion with the pair of daggers of the home save
(include/evr/live_secondary.hpp: a hook, or both hands striking together), which becomes the game's heavy attack.

Run with -LiveMeleeTravel 1 (in the Mock room there is no target that would delay the attack until the blade arrives,
as VHVR's contact does; the stroke attacks after a metre above the swing speed).

On the default (derived) home save the script starts with the bow of the first action bar page.
  420..539    wheel held, right stick click 460..466: second page
  600         wheel towards the daggers (slot 6 of the page; item 3974938802: first attack sequence 3838423204, heavy
              attack on the ground 2257955339 - docs/analysis/weapon_mount_20261010/item-sequences)
  800..825    a hook of the right hand: 26 frames of 0.045 m sideways (+X, 4.05 m/s, 1.17 m).
              Expected: live_melee_press with secondary / hook true, then the heavy attack.
  1200..1225  both hands along their blades (the blade points along (0.574, 0.819, 0) with the script's level grip):
              right 26 frames of 0.039 m (3.5 m/s), left 26 frames of 0.0222 m (2 m/s), the hands 0.5 m apart.
              Expected: secondary / both_hands true.
  1600..1607  an ordinary short stroke of the right hand across (8 frames of 0.05 m along -Z): with the travel of a
              metre it does not attack at all - it only shows that the earlier strokes were judged by their motion.
Each is followed by a slow return.
"""
import argparse, json, math, pathlib

LEFT_X, LEFT_Y, LEFT_Z, RIGHT_X, RIGHT_Y, RIGHT_Z = 1, 2, 3, 14, 15, 16
LEFT_TRIGGER, LEFT_GRAB, RIGHT_TRIGGER, RIGHT_GRAB, RIGHT_STICK_X, RIGHT_STICK_Y, RIGHT_STICK_CLICK = 10, 11, 23, 24, 32, 33, 34
BLADE = (0.57357657, 0.819152, 0.0)
# first frame, frames, metres a frame for the right hand, for the left hand
MOVES = [(800, 26, (.045, 0.0, 0.0), (0.0, 0.0, 0.0)),
         (1200, 26, tuple(.039 * c for c in BLADE), tuple(.0222 * c for c in BLADE)),
         (1600, 8, (0.0, 0.0, -.05), (0.0, 0.0, 0.0))]


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--source', type=pathlib.Path, default=pathlib.Path('vrmod/test_env/home-ranged-animation-20261010.csv'))
    p.add_argument('--output', type=pathlib.Path, default=pathlib.Path('vrmod/test_env/home-dagger-heavy-20261012.csv'))
    a = p.parse_args()
    rows = [line.split(',') for line in a.source.read_text(encoding='utf-8').splitlines() if line and not line.startswith('#')]
    start = {'right': (0.38, 1.25, 0.25), 'left': (0.38, 1.25, -0.25)}
    if len(rows) != 4300 or any(len(r) != 39 for r in rows) or tuple(float(rows[0][c]) for c in (RIGHT_X, RIGHT_Y, RIGHT_Z)) != start['right'] \
            or tuple(float(rows[0][c]) for c in (LEFT_X, LEFT_Y, LEFT_Z)) != start['left']:
        raise SystemExit('unexpected source script')
    for r in rows:
        for c in (LEFT_TRIGGER, LEFT_GRAB, RIGHT_TRIGGER, RIGHT_GRAB, RIGHT_STICK_X, RIGHT_STICK_Y, RIGHT_STICK_CLICK):
            r[c] = '0'

    def hold(column, first, end, value='1'):
        for f in range(first, end):
            rows[f - 1][column] = value

    hold(RIGHT_GRAB, 420, 540); hold(RIGHT_STICK_CLICK, 460, 467)
    angle = math.radians(181 - 26 * 5)
    hold(RIGHT_GRAB, 600, 660)
    for f in range(620, 670):
        rows[f - 1][RIGHT_STICK_X], rows[f - 1][RIGHT_STICK_Y] = repr(math.cos(angle)), repr(math.sin(angle))
    step = {}
    for first, frames, right, left in MOVES:
        for f in range(first, first + frames):
            step[f] = (right, left)
        back = (tuple(-c * frames / 120 for c in right), tuple(-c * frames / 120 for c in left))   # far below the swing speed
        for f in range(first + frames + 100, first + frames + 220):
            step[f] = back
    right, left = list(start['right']), list(start['left'])
    for f in range(1, len(rows) + 1):
        d = step.get(f)
        if d:
            right = [right[i] + d[0][i] for i in range(3)]; left = [left[i] + d[1][i] for i in range(3)]
        rows[f - 1][RIGHT_X], rows[f - 1][RIGHT_Y], rows[f - 1][RIGHT_Z] = (repr(round(c, 6)) for c in right)
        rows[f - 1][LEFT_X], rows[f - 1][LEFT_Y], rows[f - 1][LEFT_Z] = (repr(round(c, 6)) for c in left)
    if max(abs(right[i] - start['right'][i]) + abs(left[i] - start['left'][i]) for i in range(3)) > 1e-4:
        raise SystemExit('the hands do not end where they started')
    meta = dict(page_swap=[420, 540], select=600, moves=[dict(first=m[0], frames=m[1], right=m[2], left=m[3]) for m in MOVES], stop_frame=1900,
                daggers=dict(item=3974938802, primary=3838423204, heavy_grounded=2257955339))
    out = ['# Secondary attack by motion with the daggers: hook 800, both hands 1200, short stroke 1600 (tools/make_dagger_heavy_fixture.py).', '# ' + json.dumps(meta)]
    out += [','.join(r) for r in rows]
    a.output.write_text('\n'.join(out) + '\n', encoding='utf-8', newline='\n')
    print(a.output)


if __name__ == '__main__':
    main()
