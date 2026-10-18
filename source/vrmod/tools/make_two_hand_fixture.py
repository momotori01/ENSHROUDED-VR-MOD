#!/usr/bin/env python3
"""Mock hand script for -TwoHandWieldVhvr with -LiveMelee -LiveGuard, from the still-hand home script.

The Mock head is at (0, 1.65, 0) looking along +X. The right hand stays at (0.38, 1.25, 0.25), where the
measured axe points up and ahead (FORWARD below, from run game005703). Mock hand frames are 1/90 s apart,
so 0.05 m per frame is 4.5 m/s.
   400        quick wheel: axe (left grip up, as in play)
   560..599   left hand glides to 40 cm behind the right hand on the weapon axis
   620        left grip                           -> two hands, left hand behind (user rule 2026-10-11: the left grip
                                                    alone; the weapon is in the right hand anyway)
 A 700..707   both hands across at 4.23 m/s       -> nothing (two-hand weapons need more than 4.5 m/s)
 B 800..807   both hands across at 4.95 m/s       -> attack press
 C 900..907   left hand alone at 4.95 m/s         -> attack press (VHVR two-hand rule: the faster hand counts)
 D 1000..1039 left hand glides to its home point: the weapon lies level across the body -> block after 150 ms calm
 E 1200..1207 both hands push ahead at 4.95 m/s   -> attack press; the block gives way and returns
 F 1300..1339 left hand glides back behind and below -> block released
 W 1345..1374 right grip while two-handed (no stick) -> the quick wheel opens as always and the hold pauses; it
                                                    returns when the wheel closes, the axe still in hand
   1400       left grip up -> one hand again
 G 1440..1479 left hand glides to 35 cm ahead of the right hand on the weapon axis
   1500       left grip                            -> two hands, right hand behind
   1600       left grip up; 1620..1659 left hand home
 H 1690..1769 left grip held with the hands across the weapon; 1700 right grip + stick: quick wheel to the sword
                                                    -> no two-hand hold; the wheel works as usual
"""
import argparse, json, pathlib

LEFT, RIGHT = (1, 2, 3), (14, 15, 16)
LEFT_GRAB, RIGHT_GRAB, RIGHT_STICK_X, RIGHT_STICK_Y, LEFT_TRIGGER, RIGHT_TRIGGER = 11, 24, 32, 33, 10, 23
SWORD = ('-0.6293203910498374', '0.7771459614569709')
AXE = ('0.6293203910498374', '0.7771459614569709')
# WRONG GUESS, kept as the record of run game041042: the five known wheel directions are 26 degrees apart
# (staff 181, bow 155, sword 129, wand 77, axe 51 degrees), and the hammer was assumed two slots further at
# -1 degree. That direction selected nothing: the bow stayed in hand and the whole script ran without a
# two-hand weapon. The hammer's wheel direction has to be read from the game before --weapon hammer is used.
HAMMER = ('0.9998476951563913', '-0.017452406437283477')
FORWARD = (0.59403253, 0.80372822, 0.033858031)   # the axe's handle-to-tip axis with the right hand at rest (live_melee_press, game005703)
HOME_LEFT, HOME_RIGHT = (0.38, 1.25, -0.25), (0.38, 1.25, 0.25)
BEHIND = tuple(r - .40 * f for r, f in zip(HOME_RIGHT, FORWARD))
AHEAD = tuple(r + .35 * f for r, f in zip(HOME_RIGHT, FORWARD))
PARTS = dict(select=400, left_behind=(620, 1399), wheel_pause=(1345, 1374), quiet_stroke=700, strokes=[800, 900, 1200], guard_glide=(1000, 1039), guard_leave=(1300, 1339),
             right_behind=(1500, 1599), wheel_with_left_grip=(1700, 1759), stop_frame=1900)


def main():
    p = argparse.ArgumentParser()
    p.add_argument('--source', type=pathlib.Path, default=pathlib.Path('vrmod/test_env/home-ranged-animation-20261010.csv'))
    p.add_argument('--output', type=pathlib.Path, default=pathlib.Path('vrmod/test_env/home-two-hand-leftgrip-20261011.csv'))
    p.add_argument('--weapon', choices=('axe', 'hammer'), default='axe', help='hammer: the same moves with the measured hammer (hand points from the axe axis, 2.6 degrees off)')
    a = p.parse_args()
    if a.weapon == 'hammer' and a.output.name == 'home-two-hand-leftgrip-20261011.csv':
        a.output = a.output.with_name('home-two-hand-leftgrip-hammer-20261011.csv')
    rows = [line.split(',') for line in a.source.read_text(encoding='utf-8').splitlines() if line and not line.startswith('#')]
    if len(rows) != 4300 or any(len(r) != 39 for r in rows) or any(r[1:5] != ['0.38', '1.25', '-0.25', '-90'] or r[14:17] != ['0.38', '1.25', '0.25'] for r in rows):
        raise SystemExit('unexpected source script')
    for r in rows:
        r[LEFT_TRIGGER] = r[RIGHT_TRIGGER] = r[LEFT_GRAB] = r[RIGHT_GRAB] = r[RIGHT_STICK_X] = r[RIGHT_STICK_Y] = '0'
    moves = {}   # frame -> [(columns, step)]

    def move(first, frames, columns, step):
        for frame in range(first, first + frames):
            moves.setdefault(frame, []).append((columns, step))

    def glide(first, frames, start, end):
        move(first, frames, LEFT, tuple((e - s) / frames for s, e in zip(start, end)))

    def hold(column, first, last):
        for frame in range(first, last + 1):
            rows[frame - 1][column] = '1'

    def select(start, direction):
        hold(RIGHT_GRAB, start, start + 59)
        for frame in range(start + 20, start + 70):
            rows[frame - 1][RIGHT_STICK_X], rows[frame - 1][RIGHT_STICK_Y] = direction

    def stroke(first, hands, step, back_frames):
        for columns in hands:
            move(first, 8, columns, step)
            move(first + 20, back_frames, columns, tuple(-8 * s / back_frames for s in step))

    select(400, HAMMER if a.weapon == 'hammer' else AXE)
    glide(560, 40, HOME_LEFT, BEHIND)
    hold(LEFT_GRAB, 620, 1399); hold(RIGHT_GRAB, 1345, 1374)   # W: the wheel, no stick
    stroke(700, (LEFT, RIGHT), (0, 0, -.047), 47)   # A
    stroke(800, (LEFT, RIGHT), (0, 0, -.055), 44)   # B
    stroke(900, (LEFT,), (0, 0, -.055), 44)         # C
    glide(1000, 40, BEHIND, HOME_LEFT)              # D
    stroke(1200, (LEFT, RIGHT), (.055, 0, 0), 44)   # E
    glide(1300, 40, HOME_LEFT, BEHIND)              # F
    glide(1440, 40, BEHIND, AHEAD)                  # G
    hold(LEFT_GRAB, 1500, 1599)
    glide(1620, 40, AHEAD, HOME_LEFT)
    hold(LEFT_GRAB, 1690, 1769); select(1700, SWORD)   # H
    state = {LEFT: list(HOME_LEFT), RIGHT: list(HOME_RIGHT)}
    for frame in range(1, 4301):
        for columns, step in moves.get(frame, []):
            state[columns] = [v + s for v, s in zip(state[columns], step)]
        for columns, value in state.items():
            for column, v in zip(columns, value):
                rows[frame - 1][column] = repr(round(v, 9))
    if max(abs(u - v) for columns, home in ((LEFT, HOME_LEFT), (RIGHT, HOME_RIGHT)) for u, v in zip(state[columns], home)) > 1e-6:
        raise SystemExit(f'hands do not return: {state}')
    out = ['# -TwoHandWieldVhvr with -LiveMelee -LiveGuard regression (see tools/make_two_hand_fixture.py): axe selected at 400.',
           '# ' + json.dumps(PARTS)]
    out += [','.join(r) for r in rows]
    a.output.write_text('\n'.join(out) + '\n', encoding='utf-8', newline='\n')
    a.output.with_suffix('.json').write_text(json.dumps(dict(PARTS, forward=FORWARD, behind=BEHIND, ahead=AHEAD), indent=2) + '\n', encoding='utf-8')
    print(a.output)


if __name__ == '__main__':
    main()
