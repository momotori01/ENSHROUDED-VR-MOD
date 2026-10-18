#!/usr/bin/env python3
"""Mock scripts (hands and head) that walk the character out of the home and look down around it, for its shadow.

The player IK (-BodyIk) could only be judged indoors, where no shadow of the body is visible. The headset note for the
home save is "outdoors: right 2 s, then forward 3 s". The hand script holds the left stick that way; the head script
then looks 50 degrees down in four directions, a quarter turn apart, and the right hand is raised to the side so that
its shadow is recognisable. Use with -NoSceneryReplay (the view follows) and without -BodyHeading.

The path of the user's own keyboard walk (game-20261011-154010/keyboard-walk-path.json): right until the wall stops
the character at x = 6.17 m, then straight forward along that wall through the door, 1 m down at z = 14..15, stop at
z = 19 m. Hence --path "1,0,110;0,1,190" --wheel none: the right leg is longer than needed (the wall ends it at any
frame rate), and the quick wheel is left out because its stick turns the view by 13 degrees, which bends the path.

   300..369    quick wheel: sword
   500..(500+right)        stick right
   then ..(+forward)       stick forward
   look frames: four views of 150 frames each, starting 100 frames after the walk ends
"""
import argparse, json, math, pathlib

LEFT_STICK_X, LEFT_STICK_Y = 29, 30
RIGHT_GRAB, RIGHT_STICK_X, RIGHT_STICK_Y, LEFT_GRAB, LEFT_TRIGGER, RIGHT_TRIGGER = 24, 32, 33, 11, 10, 23
RIGHT = (14, 15, 16)
X, Y, Z, YAW, PITCH = 1, 2, 3, 4, 5
UP, FORWARD = .13, .10
SWORD_DEGREES = 129


def eye_from_pivot(yaw, pitch):
    a, p = math.radians(yaw), math.radians(pitch)
    up, back = UP * math.cos(p) + FORWARD * math.sin(p), UP * math.sin(p) - FORWARD * math.cos(p)
    return back * math.sin(a), up, back * math.cos(a)


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--hands-source', type=pathlib.Path, default=pathlib.Path('vrmod/test_env/home-ranged-animation-20261010.csv'))
    p.add_argument('--pose-source', type=pathlib.Path, default=pathlib.Path('vrmod/test_env/home-mill-20261010.csv'))
    p.add_argument('--path', default='1,0,90;0,1,135', help='left stick legs "x,y,frames;..." from frame 500 (default: the headset note, right then forward; in Mock without the scenery replay the start faces another way: game144444 ended at a wall)')
    p.add_argument('--wheel', choices=('before', 'none'), default='before', help='select the sword on the quick wheel before the walk (its stick turns the view by 13 degrees), or keep the item of the save in hand')
    p.add_argument('--name', default='home-outdoor-shadow-20261011')
    a = p.parse_args()
    rows = [line.split(',') for line in a.hands_source.read_text(encoding='utf-8').splitlines() if line and not line.startswith('#')]
    if len(rows) != 4300 or any(len(r) != 39 for r in rows) or any(r[14:17] != ['0.38', '1.25', '0.25'] for r in rows):
        raise SystemExit('unexpected hand source')
    for r in rows:
        r[LEFT_TRIGGER] = r[RIGHT_TRIGGER] = r[LEFT_GRAB] = r[RIGHT_GRAB] = r[RIGHT_STICK_X] = r[RIGHT_STICK_Y] = r[LEFT_STICK_X] = r[LEFT_STICK_Y] = '0'
    angle = math.radians(SWORD_DEGREES)
    if a.wheel == 'before':
        for frame in range(300, 360):
            rows[frame - 1][RIGHT_GRAB] = '1'
        for frame in range(320, 370):
            rows[frame - 1][RIGHT_STICK_X], rows[frame - 1][RIGHT_STICK_Y] = repr(math.cos(angle)), repr(math.sin(angle))
    legs = [tuple(float(v) for v in leg.split(',')) for leg in a.path.split(';')]
    forward_end = 500
    for x, y, frames in legs:
        for frame in range(forward_end, forward_end + int(frames)):
            rows[frame - 1][LEFT_STICK_X], rows[frame - 1][LEFT_STICK_Y] = repr(x), repr(y)
        forward_end += int(frames)
    look = forward_end + 100
    # The right hand goes out to the side and up (0.6 m to the right, 1.5 m high) while the head looks around.
    for i in range(60):
        t = (i + 1) / 60
        r = rows[look - 60 + i - 1]; r[RIGHT[0]], r[RIGHT[1]], r[RIGHT[2]] = repr(round(.38 - .18 * t, 4)), repr(round(1.25 + .25 * t, 4)), repr(round(.25 + .35 * t, 4))
    for frame in range(look, look + 620):
        r = rows[frame - 1]; r[RIGHT[0]], r[RIGHT[1]], r[RIGHT[2]] = '0.2', '1.5', '0.6'
    views = [(look + 150 * i, look + 150 * i + 149, 90.0 * i) for i in range(4)]
    parts = dict(wheel=300 if a.wheel == 'before' else None, path=a.path, walk=(500, forward_end - 1), views=views, stop_frame=look + 640)
    hands = pathlib.Path('vrmod/test_env') / (a.name + '.csv')
    hands.write_text('\n'.join(['# Walks out of the home (left stick), right hand raised (tools/make_outdoor_shadow_fixture.py).', '# ' + json.dumps(parts)] + [','.join(r) for r in rows]) + '\n', encoding='utf-8', newline='\n')
    poses = [line.split(',') for line in a.pose_source.read_text(encoding='utf-8').splitlines() if line and not line.startswith('#')]
    if len(poses) != 6000 or any(len(r) != 20 for r in poses):
        raise SystemExit('unexpected head source')
    start = [float(v) for v in poses[0][X:Z + 1]]; yaw0, pitch0 = float(poses[0][YAW]), float(poses[0][PITCH])
    pivot = [s - e for s, e in zip(start, eye_from_pivot(yaw0, pitch0))]
    for first, last, turn in views:
        for frame in range(first, last + 1):
            ramp = min(1.0, (frame - first + 1) / 30)
            previous = turn - 90 if turn else 0
            yaw = yaw0 + previous + (turn - previous) * ramp
            pitch = pitch0 + (-50 - pitch0) * (ramp if not turn else 1.0)
            eye = [c + e for c, e in zip(pivot, eye_from_pivot(yaw, pitch))]
            r = poses[frame - 1]; r[YAW], r[PITCH] = repr(round(yaw, 6)), repr(round(pitch, 6)); r[X], r[Y], r[Z] = (repr(round(v, 6)) for v in eye)
    head = pathlib.Path('vrmod/test_env') / (a.name + '-head.csv')
    head.write_text('\n'.join(['# Head script for the outdoor shadow views (tools/make_outdoor_shadow_fixture.py): 50 degrees down, four quarter turns.', '# ' + json.dumps(parts)] + [','.join(r) for r in poses]) + '\n', encoding='utf-8', newline='\n')
    print(hands, head, json.dumps(parts))


if __name__ == '__main__':
    main()
