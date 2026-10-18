#!/usr/bin/env python3
"""Mock scripts (hands and head) for a bomb's scorch mark on the ground outdoors, for both eyes.

Headset run game145015: "the black mark that an explosion leaves on the ground is seen with one eye only". Indoors the
floor breaks and the mark cannot be judged (game160101). The walk out of the home is the one of
make_outdoor_shadow_fixture.py (right until the wall, then forward through the door to the grass in the sun).
Run it twice, with -ShareDecals and without, both with -NoSceneryReplay and without -BodyHeading.

On the default (derived) home save the script starts with the bow of the first action bar page.
  420..539    wheel held, the left stick's click 460..466: second page (the bomb of that page is selected by it).
              Not earlier: the game takes input from about frame 360 on (the runs game192633 / game192952 swapped
              at 340 and kept the bow)
  600..709    left stick right, 710..837 forward
  1000..1044  right trigger held, the hand moves forward and a little down from 1030, release in 1045: a bomb to the ground
  head        from 1060 the head looks 35 degrees down, from 1400 55 degrees down
Stop at 1800.
"""
import argparse, json, math, pathlib

RIGHT_X, RIGHT_Y, RIGHT_Z = 14, 15, 16
LEFT_TRIGGER, LEFT_GRAB, RIGHT_TRIGGER, RIGHT_GRAB, LEFT_STICK_X, LEFT_STICK_Y, LEFT_STICK_CLICK, RIGHT_STICK_X, RIGHT_STICK_Y, RIGHT_STICK_CLICK = 10, 11, 23, 24, 29, 30, 31, 32, 33, 34
X, Y, Z, YAW, PITCH = 1, 2, 3, 4, 5
UP, FORWARD = .13, .10


def eye_from_pivot(yaw, pitch):
    a, p = math.radians(yaw), math.radians(pitch)
    up, back = UP * math.cos(p) + FORWARD * math.sin(p), UP * math.sin(p) - FORWARD * math.cos(p)
    return back * math.sin(a), up, back * math.cos(a)


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--hands-source', type=pathlib.Path, default=pathlib.Path('vrmod/test_env/home-ranged-animation-20261010.csv'))
    p.add_argument('--pose-source', type=pathlib.Path, default=pathlib.Path('vrmod/test_env/home-mill-20261010.csv'))
    p.add_argument('--name', default='home-decal-outdoor-20261012')
    p.add_argument('--throw', type=int, default=1000, help='first frame of the held trigger')
    a = p.parse_args()
    rows = [line.split(',') for line in a.hands_source.read_text(encoding='utf-8').splitlines() if line and not line.startswith('#')]
    if len(rows) != 4300 or any(len(r) != 39 for r in rows) or rows[0][RIGHT_Z] != '0.25':
        raise SystemExit('unexpected hand source')
    for r in rows:
        for c in (LEFT_TRIGGER, LEFT_GRAB, RIGHT_TRIGGER, RIGHT_GRAB, LEFT_STICK_X, LEFT_STICK_Y, LEFT_STICK_CLICK, RIGHT_STICK_X, RIGHT_STICK_Y, RIGHT_STICK_CLICK):
            r[c] = '0'

    def hold(column, first, end, value='1'):
        for f in range(first, end):
            rows[f - 1][column] = value

    hold(RIGHT_GRAB, 420, 540); hold(LEFT_STICK_CLICK, 460, 467)
    hold(LEFT_STICK_X, 600, 710, '1.0'); hold(LEFT_STICK_Y, 710, 838, '1.0')
    t = a.throw
    hold(RIGHT_TRIGGER, t, t + 45)
    x0, y0, z0 = float(rows[0][RIGHT_X]), float(rows[0][RIGHT_Y]), .25; moves = {}
    for f in range(t + 30, t + 50):
        moves[f] = (.03, -.01, 0.0)
    for f in range(t + 100, t + 180):
        moves[f] = (-.03 * 20 / 80, .01 * 20 / 80, 0.0)
    x, y, z = x0, y0, z0
    for f in range(1, len(rows) + 1):
        dx, dy, dz = moves.get(f, (0.0, 0.0, 0.0)); x += dx; y += dy; z += dz
        rows[f - 1][RIGHT_X], rows[f - 1][RIGHT_Y], rows[f - 1][RIGHT_Z] = repr(round(x, 6)), repr(round(y, 6)), repr(round(z, 6))
    if abs(x - x0) > 1e-6 or abs(y - y0) > 1e-6 or abs(z - z0) > 1e-6:
        raise SystemExit('the hand does not end where it started')
    looks = [(t + 60, 1399, -35.0), (1400, 1800, -55.0)]
    meta = dict(page_swap=[420, 540], walk=[600, 838], throw=dict(wind=t, release=t + 45), looks=looks, stop_frame=1800, items=dict(bomb=2283229289))
    hands = pathlib.Path('vrmod/test_env') / (a.name + '.csv')
    hands.write_text('\n'.join(['# A bomb to the ground outdoors, for its scorch mark in both eyes (tools/make_decal_outdoor_fixture.py).', '# ' + json.dumps(meta)] + [','.join(r) for r in rows]) + '\n', encoding='utf-8', newline='\n')
    poses = [line.split(',') for line in a.pose_source.read_text(encoding='utf-8').splitlines() if line and not line.startswith('#')]
    if len(poses) != 6000 or any(len(r) != 20 for r in poses):
        raise SystemExit('unexpected head source')
    start = [float(v) for v in poses[0][X:Z + 1]]; yaw0, pitch0 = float(poses[0][YAW]), float(poses[0][PITCH])
    pivot = [s - e for s, e in zip(start, eye_from_pivot(yaw0, pitch0))]
    before = pitch0
    for first, last, down in looks:
        for frame in range(first, last + 1):
            ramp = min(1.0, (frame - first + 1) / 30)
            pitch = before + (down - before) * ramp
            eye = [c + e for c, e in zip(pivot, eye_from_pivot(yaw0, pitch))]
            r = poses[frame - 1]; r[PITCH] = repr(round(pitch, 6)); r[X], r[Y], r[Z] = (repr(round(v, 6)) for v in eye)
        before = down
    for frame in range(looks[-1][1] + 1, len(poses) + 1):
        r = poses[frame - 1]; r[PITCH] = repr(before); r[X], r[Y], r[Z] = (repr(round(c + e, 6)) for c, e in zip(pivot, eye_from_pivot(yaw0, before)))
    head = pathlib.Path('vrmod/test_env') / (a.name + '-head.csv')
    head.write_text('\n'.join(['# Head script for the bomb mark outdoors (tools/make_decal_outdoor_fixture.py): 35 degrees down, then 55.', '# ' + json.dumps(meta)] + [','.join(r) for r in poses]) + '\n', encoding='utf-8', newline='\n')
    print(hands, head, json.dumps(meta))


if __name__ == '__main__':
    main()
