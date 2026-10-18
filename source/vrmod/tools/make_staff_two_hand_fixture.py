#!/usr/bin/env python3
"""Mock hand script for the staff held with both hands (-TwoHandWieldVhvr with -RangedAim), from the still-hand home script.

The right hand stays at (0.38, 1.25, 0.25), yaw -90; on the plain mount the staff then points up and ahead
(FORWARD). Its pointer ray is level, straight ahead, so a shot along the staff and a shot along the pointer
differ by 55 degrees. A cast is a right trigger hold of 90 frames; the game casts about 63 frames in.
   400        quick wheel: staff (left grip up)
   520..559   left hand glides to 40 cm behind the right hand on the staff
   580        left grip                            -> two hands, left hand behind (the left grip alone, user rule 2026-10-11)
   620..649   left hand glides aside and up: the staff turns about the right hand
 1 700..789   cast                                  -> along the line between the hands (rear hand -> front hand)
   900        left grip up; 920..959 left hand home
   1000..1039 left hand glides to 35 cm ahead of the right hand on the staff
   1060       left grip                            -> two hands, right hand behind (the staff slides forward)
 2 1140..1229 cast                                  -> along the staff
   1300       left grip up; 1320..1359 left hand home
 3 1420..1509 cast with one hand                    -> along the pointer ray, as before
   1560..1599 left hand behind again; 1620 left grip
 4 1700..1789 cast                                  -> along the staff, if the game still casts: in run game013903 the
                                                       fourth cast of a session did not start (native state stayed 0 with
                                                       the trigger delivered), whichever hold it used
   1860       left grip up; 1880..1919 left hand home
"""
import argparse, json, pathlib

LEFT, RIGHT = (1, 2, 3), (14, 15, 16)
LEFT_GRAB, RIGHT_GRAB, RIGHT_STICK_X, RIGHT_STICK_Y, LEFT_TRIGGER, RIGHT_TRIGGER, LEFT_PRIMARY = 11, 24, 32, 33, 10, 23, 35
STAFF = ('-0.9998476951563913', '-0.017452406437283477')
FORWARD = (0.5735764363510462, 0.8191520442889918, 0.0)   # right grip yaw -90 with the -35 degree mount pitch
HOME_LEFT, HOME_RIGHT = (0.38, 1.25, -0.25), (0.38, 1.25, 0.25)
BEHIND = tuple(r - .40 * f for r, f in zip(HOME_RIGHT, FORWARD))
ASIDE = (0.10, 1.20, 0.05)
AHEAD = tuple(r + .35 * f for r, f in zip(HOME_RIGHT, FORWARD))
PARTS = dict(select=400, left_behind=[(580, 899), (1620, 1859)], right_behind=(1060, 1299), casts=[(700, 789), (1140, 1229), (1420, 1509), (1700, 1789)], stop_frame=1980)


def main():
    p = argparse.ArgumentParser()
    p.add_argument('--source', type=pathlib.Path, default=pathlib.Path('vrmod/test_env/home-ranged-animation-20261010.csv'))
    p.add_argument('--output', type=pathlib.Path, default=pathlib.Path('vrmod/test_env/home-staff-two-hand-leftgrip-20261011.csv'))
    a = p.parse_args()
    rows = [line.split(',') for line in a.source.read_text(encoding='utf-8').splitlines() if line and not line.startswith('#')]
    if len(rows) != 4300 or any(len(r) != 39 for r in rows) or any(r[1:5] != ['0.38', '1.25', '-0.25', '-90'] or r[14:17] != ['0.38', '1.25', '0.25'] for r in rows):
        raise SystemExit('unexpected source script')
    for r in rows:
        r[LEFT_TRIGGER] = r[RIGHT_TRIGGER] = r[LEFT_GRAB] = r[RIGHT_GRAB] = r[RIGHT_STICK_X] = r[RIGHT_STICK_Y] = r[LEFT_PRIMARY] = '0'
        r[20], r[21] = '-90', '0'   # right pointer: level, straight ahead
    moves = {}

    def glide(first, frames, start, end):
        for frame in range(first, first + frames):
            moves[frame] = tuple((e - s) / frames for s, e in zip(start, end))

    def hold(column, first, last):
        for frame in range(first, last + 1):
            rows[frame - 1][column] = '1'

    hold(RIGHT_GRAB, 400, 459)
    for frame in range(420, 470):
        rows[frame - 1][RIGHT_STICK_X], rows[frame - 1][RIGHT_STICK_Y] = STAFF
    glide(520, 40, HOME_LEFT, BEHIND)
    hold(LEFT_GRAB, 580, 899)
    glide(620, 30, BEHIND, ASIDE)
    glide(920, 40, ASIDE, HOME_LEFT)
    glide(1000, 40, HOME_LEFT, AHEAD)
    hold(LEFT_GRAB, 1060, 1299)
    glide(1320, 40, AHEAD, HOME_LEFT)
    glide(1560, 40, HOME_LEFT, BEHIND)
    hold(LEFT_GRAB, 1620, 1859)
    glide(1880, 40, BEHIND, HOME_LEFT)
    for first, last in PARTS['casts']:
        hold(RIGHT_TRIGGER, first, last)
    left = list(HOME_LEFT)
    for frame in range(1, 4301):
        if frame in moves:
            left = [v + s for v, s in zip(left, moves[frame])]
        for column, v in zip(LEFT, left):
            rows[frame - 1][column] = repr(round(v, 9))
    if max(abs(u - v) for u, v in zip(left, HOME_LEFT)) > 1e-6:
        raise SystemExit(f'left hand does not return: {left}')
    out = ['# Staff held with both hands, -TwoHandWieldVhvr with -RangedAim (see tools/make_staff_two_hand_fixture.py): staff selected at 400.',
           '# ' + json.dumps(PARTS)]
    out += [','.join(r) for r in rows]
    a.output.write_text('\n'.join(out) + '\n', encoding='utf-8', newline='\n')
    print(a.output)


if __name__ == '__main__':
    main()
