#!/usr/bin/env python3
"""Mock head script for the player IK (-BodyIk): the still home head pose, with the head looking down at the body,
turning, crouching in the room and looking up. Columns as the source: frame, x, y, z, yaw, pitch, roll, ...

The head turns about a point in the neck, as a person's does: the eyes are .13 m above and .10 m in front of it
(include/evr/body_ik.hpp, eyeFromPivot; fitted to the first real-device run). The first script (kept in
game-20261011-122822) turned the head about the eyes, which no neck can do, and so lifted the chest by 18 cm.

   250..600    looks down 70 degrees (holds 330..520)        -> chest, arms, legs and feet in view
   650..900    turns the head 70 degrees (holds 730..820)    -> the body turns with the headset
   950..1230   crouches 50 cm, looking down 45 (1030..1150)  -> hips down, knees forward, feet stay
   1300..1500  looks up 30 degrees (holds 1380..1450)
   1550..1850  crouches 35 cm and looks down 70 (1650..1750)
"""
import argparse, json, math, pathlib

X, Y, Z, YAW, PITCH = 1, 2, 3, 4, 5
UP, FORWARD = .13, .10
PARTS = dict(look_down=(250, 330, 520, 600), turn=(650, 730, 820, 900), crouch=(950, 1030, 1150, 1230), look_up=(1300, 1380, 1450, 1500),
             crouch_look_down=(1550, 1650, 1750, 1850), stop_frame=1950)


def eye_from_pivot(yaw, pitch):
    """OpenXR axes (-Z forward at yaw 0, +Y up); pitch above the horizon is positive."""
    a, p = math.radians(yaw), math.radians(pitch)
    up, back = UP * math.cos(p) + FORWARD * math.sin(p), UP * math.sin(p) - FORWARD * math.cos(p)
    return back * math.sin(a), up, back * math.cos(a)


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--source', type=pathlib.Path, default=pathlib.Path('vrmod/test_env/home-mill-20261010.csv'))
    p.add_argument('--output', type=pathlib.Path, default=pathlib.Path('vrmod/test_env/home-body-ik-head-20261011.csv'))
    a = p.parse_args()
    lines = a.source.read_text(encoding='utf-8').splitlines()
    rows = [s.split(',') for s in lines if s and not s.startswith('#')]
    if len(rows) != 6000 or any(len(r) != 20 for r in rows) or any(r[1:7] != rows[0][1:7] for r in rows):
        raise SystemExit('unexpected source head script')
    start = [float(v) for v in rows[0][X:Z + 1]]; yaw0, pitch0 = float(rows[0][YAW]), float(rows[0][PITCH])
    pivot = [s - e for s, e in zip(start, eye_from_pivot(yaw0, pitch0))]

    def ramp(frame, part):
        begin, held, release, end = PARTS[part]
        if frame < begin or frame > end:
            return 0.0
        t = 1.0 if held <= frame <= release else (frame - begin) / (held - begin) if frame < held else (end - frame) / (end - release)
        return t * t * (3 - 2 * t)

    for r in rows:
        f = int(r[0])
        pitch = pitch0 + (-70 - pitch0) * ramp(f, 'look_down') + (-45 - pitch0) * ramp(f, 'crouch') + (30 - pitch0) * ramp(f, 'look_up') + (-70 - pitch0) * ramp(f, 'crouch_look_down')
        yaw = yaw0 + 70 * ramp(f, 'turn')
        drop = .5 * ramp(f, 'crouch') + .35 * ramp(f, 'crouch_look_down')
        eye = [c + e for c, e in zip(pivot, eye_from_pivot(yaw, pitch))]; eye[1] -= drop
        if pitch != pitch0 or yaw != yaw0 or drop:          # the still rows stay exactly as recorded
            r[PITCH], r[YAW] = repr(round(pitch, 6)), repr(round(yaw, 6))
            r[X], r[Y], r[Z] = (repr(round(v, 6)) for v in eye)
    out = ['# Head script for the player IK (tools/make_body_ik_pose_fixture.py): looks down, turns, crouches, looks up; the head turns about the neck.',
           '# ' + json.dumps(PARTS)]
    out += [','.join(r) for r in rows]
    a.output.write_text('\n'.join(out) + '\n', encoding='utf-8', newline='\n')
    print(a.output)


if __name__ == '__main__':
    main()
