#!/usr/bin/env python3
"""Mock scripts (hands and head) that show whether the shadow of the body follows the arm of the player IK (-BodyIk).

The character walks out of the home into the sun (the path of make_outdoor_shadow_fixture.py), turns its back to the
sun and looks 50 degrees down, so that its own shadow lies straight ahead. Then the right hand takes three poses, each
held for 140 frames: at rest in front of the body, straight out to the right at shoulder height, straight up. The
shadow has to show the arm as a bar to the right, then as a bar beside the head. The hands turn with the head (the
body faces where the headset looks), the left hand stays at rest. Use with -NoSceneryReplay, without -BodyHeading.

   500..(walk end)   left stick: right until the wall, then forward
   look .. look+59   head turns (about the neck pivot) and looks down, hands turn with it
   rest / side / up  the three poses of the right hand, 40 frames of travel between them
"""
import argparse, json, math, pathlib

LEFT_STICK_X, LEFT_STICK_Y = 29, 30
RIGHT_GRAB, RIGHT_STICK_X, RIGHT_STICK_Y, LEFT_GRAB, LEFT_TRIGGER, RIGHT_TRIGGER, RIGHT_SECONDARY = 24, 32, 33, 11, 10, 23, 38
LEFT, LEFT_YAW, RIGHT, RIGHT_YAW = (1, 2, 3), 4, (14, 15, 16), 17
X, Y, Z, YAW, PITCH = 1, 2, 3, 4, 5
UP, FORWARD = .13, .10
# Right, up and forward of the hand in metres. REST: forward of the eyes' start, as in the source script.
# POSES: forward of the neck pivot (shoulder at right 0.18, up 1.39; the arm reaches about 0.6 m).
REST = dict(left=(-.25, 1.25, .38), right=(.25, 1.25, .38))
POSES = (('rest', None), ('side', (.73, 1.40, 0.0)), ('up', (.22, 1.90, .05)))


def eye_from_pivot(yaw, pitch):
    a, p = math.radians(yaw), math.radians(pitch)
    up, back = UP * math.cos(p) + FORWARD * math.sin(p), UP * math.sin(p) - FORWARD * math.cos(p)
    return back * math.sin(a), up, back * math.cos(a)


def place(pivot, yaw, offset):
    """Tracking space position of a hand given as right/up/forward of the body that faces yaw (forward is -Z at yaw 0)."""
    a = math.radians(yaw); right, up, forward = offset
    return (pivot[0] + math.cos(a) * right - math.sin(a) * forward, up, pivot[2] - math.sin(a) * right - math.cos(a) * forward)


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--hands-source', type=pathlib.Path, default=pathlib.Path('vrmod/test_env/home-ranged-animation-20261010.csv'))
    p.add_argument('--pose-source', type=pathlib.Path, default=pathlib.Path('vrmod/test_env/home-mill-20261010.csv'))
    p.add_argument('--path', default='1,0,110;0,1,128', help='left stick legs "x,y,frames;..." from frame 500 (the sunlit spot 17 m in front of the home door)')
    p.add_argument('--turn', type=float, default=135.0, help='head turn in degrees (left is positive) that puts the shadow straight ahead at that spot and time of day')
    p.add_argument('--name', default='home-shadow-arm-20261011')
    a = p.parse_args()
    rows = [line.split(',') for line in a.hands_source.read_text(encoding='utf-8').splitlines() if line and not line.startswith('#')]
    if len(rows) != 4300 or any(len(r) != 39 for r in rows) or any(r[14:17] != ['0.38', '1.25', '0.25'] or r[1:4] != ['0.38', '1.25', '-0.25'] for r in rows):
        raise SystemExit('unexpected hand source')
    poses = [line.split(',') for line in a.pose_source.read_text(encoding='utf-8').splitlines() if line and not line.startswith('#')]
    if len(poses) != 6000 or any(len(r) != 20 for r in poses):
        raise SystemExit('unexpected head source')
    for r in rows:
        r[LEFT_TRIGGER] = r[RIGHT_TRIGGER] = r[LEFT_GRAB] = r[RIGHT_GRAB] = r[RIGHT_STICK_X] = r[RIGHT_STICK_Y] = r[LEFT_STICK_X] = r[LEFT_STICK_Y] = r[RIGHT_SECONDARY] = '0'
    walk_end = 500
    for leg in a.path.split(';'):
        x, y, frames = (float(v) for v in leg.split(','))
        for frame in range(walk_end, walk_end + int(frames)):
            rows[frame - 1][LEFT_STICK_X], rows[frame - 1][LEFT_STICK_Y] = repr(x), repr(y)
        walk_end += int(frames)
    look = walk_end + 100
    start = [float(v) for v in poses[0][X:Z + 1]]; yaw0, pitch0 = float(poses[0][YAW]), float(poses[0][PITCH])
    pivot = [s - e for s, e in zip(start, eye_from_pivot(yaw0, pitch0))]
    # The source's hands are 0.38 m in front of the eyes' start; the eyes start that much in front of the pivot.
    ahead = math.hypot(start[0] - pivot[0], start[2] - pivot[2])
    rest = {side: (REST[side][0], REST[side][1], REST[side][2] + ahead) for side in REST}
    chest = (pivot[0], 0.0, pivot[2])
    for side, expected in (('right', (.38, 1.25, .25)), ('left', (.38, 1.25, -.25))):
        if any(abs(u - v) > 1e-3 for u, v in zip(place(chest, yaw0, rest[side]), expected)):
            raise SystemExit('the rest pose of the source is not where this script assumes it')
    targets = [(name, rest['right'] if name == 'rest' else offset) for name, offset in POSES]
    holds, frame = {}, look + 60
    timeline = []   # (first frame, last frame, from offset, to offset)
    previous = rest['right']
    for index, (name, offset) in enumerate(targets):
        if index:
            timeline.append((frame, frame + 39, previous, offset)); frame += 40
        timeline.append((frame, frame + 139, offset, offset)); holds[name] = (frame, frame + 139); frame += 140
        previous = offset
    timeline.append((frame, frame + 39, previous, rest['right'])); frame += 40
    stop = frame + 20
    for f in range(look, len(rows) + 1):
        turn = a.turn * min(1.0, (f - look + 1) / 60)
        yaw = yaw0 + turn
        offset = rest['right']
        for first, last, begin, end in timeline:
            if first <= f <= last:
                t = (f - first + 1) / (last - first + 1) if begin != end else 1.0
                offset = tuple(u + (v - u) * t for u, v in zip(begin, end))
            elif f > last:
                offset = end
        r = rows[f - 1]
        for columns, yaw_column, value in ((LEFT, LEFT_YAW, rest['left']), (RIGHT, RIGHT_YAW, offset)):
            for column, v in zip(columns, place(chest, yaw, value)):
                r[column] = repr(round(v, 4))
            r[yaw_column] = repr(round(yaw, 4))
    for f in range(look, len(poses) + 1):
        ramp = min(1.0, (f - look + 1) / 60)
        yaw, pitch = yaw0 + a.turn * ramp, pitch0 + (-50 - pitch0) * ramp
        eye = [c + e for c, e in zip(pivot, eye_from_pivot(yaw, pitch))]
        r = poses[f - 1]; r[YAW], r[PITCH] = repr(round(yaw, 6)), repr(round(pitch, 6)); r[X], r[Y], r[Z] = (repr(round(v, 6)) for v in eye)
    parts = dict(path=a.path, walk=(500, walk_end - 1), look=look, turn=a.turn, holds=holds, captures=[last - 20 for _, last in holds.values()], stop_frame=stop)
    hands = pathlib.Path('vrmod/test_env') / (a.name + '.csv')
    hands.write_text('\n'.join(['# Walks into the sun, back to the sun, right hand at rest / out to the side / straight up (tools/make_shadow_arm_fixture.py).', '# ' + json.dumps(parts)] + [','.join(r) for r in rows]) + '\n', encoding='utf-8', newline='\n')
    head = pathlib.Path('vrmod/test_env') / (a.name + '-head.csv')
    head.write_text('\n'.join(['# Head script for the shadow of the arm (tools/make_shadow_arm_fixture.py): turned, 50 degrees down.', '# ' + json.dumps(parts)] + [','.join(r) for r in poses]) + '\n', encoding='utf-8', newline='\n')
    print(hands, head, json.dumps(parts))


if __name__ == '__main__':
    main()
