#!/usr/bin/env python3
"""Mock hand script for the cooldown between attacks (include/evr/live_melee.hpp LiveCooldown: VHVR's cooldown with the
user's rule of 2026-10-12 - the next stroke attacks when 80 % of it has passed, the hand vibrates until then).

The cooldown runs in real time, the script in frames (23 ms each at 43 pairs a second, but 19..30 ms from run to run),
so the second stroke is not placed at a frame: after the first stroke the hand keeps circling fast enough to attack
(3.6 m/s). While the cooldown waits no attack comes; the first sample after the wait attacks. What the log then
shows is the time between the two presses (the wait: 80 % of what the action said) and that the game starts the next
attack of the chain at once (the abilities to attack were handed over early).

On the default (derived) home save the script starts with the bow of the first action bar page.
  600..669    wheel towards the one-hand sword (slot 3 of the page, 129 degrees; item 1125155603, first attack
              2496979158: the next attack may begin after 0.55 s, so the wait is 0.44 s)
  840..1100   the left hand hangs (0.9 m high, 0.75 m under the head): the home save's sword comes with a shield, and
              with the hands' usual place of these scripts the live guard takes the shield for raised as soon as the
              sword hand crosses to the left (the body's front is taken from the hands) - the block then cancels the
              chain's attack (run game211056)
  900..907    a stroke across (8 frames of 0.05 m along -Z, 4.5 m/s)
  908..910    the hand still (the stroke is over: the next one is armed)
  911..940    the hand circles (radius 0.154 m, 0.26 rad a frame: 3.6 m/s) - one press when the wait is over
  941..1000   back to where it started, slowly
  1300..1307  one more stroke, alone: the cooldown is long over
Stop at 1600.

--wait N keeps the hand still for N more frames before it circles (the circling then starts at 911+N). With 50 the
press comes about 1.3 s into the sword's first attack: that attack clears the chain's request at 1133 ms and lasts
1667 ms, and a press in between used to start nothing (headset run game-20261013-005905; the clearing is now withheld
for a stroke's action, motion_event_selection_observer.hpp chainStays). Expected: the chain's second attack
(394546384) right after that press.
"""
import argparse, json, math, pathlib

LEFT_Y, RIGHT_Y, RIGHT_Z = 2, 15, 16
LEFT_TRIGGER, LEFT_GRAB, RIGHT_TRIGGER, RIGHT_GRAB, LEFT_STICK_X, LEFT_STICK_Y, LEFT_STICK_CLICK, RIGHT_STICK_X, RIGHT_STICK_Y, RIGHT_STICK_CLICK = 10, 11, 23, 24, 29, 30, 31, 32, 33, 34
SWORD_DEGREES, RADIUS, STEP = 129.0, .154, .26


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument('--source', type=pathlib.Path, default=pathlib.Path('vrmod/test_env/home-ranged-animation-20261010.csv'))
    p.add_argument('--output', type=pathlib.Path, default=pathlib.Path('vrmod/test_env/home-cooldown-20261012.csv'))
    p.add_argument('--circle', type=int, default=30, help='frames of the circling')
    p.add_argument('--wait', type=int, default=0, help='more frames with the hand still before the circling')
    a = p.parse_args()
    rows = [line.split(',') for line in a.source.read_text(encoding='utf-8').splitlines() if line and not line.startswith('#')]
    if len(rows) != 4300 or any(len(r) != 39 for r in rows) or rows[0][RIGHT_Y] != '1.25' or rows[0][RIGHT_Z] != '0.25':
        raise SystemExit('unexpected source script')
    for r in rows:
        for c in (LEFT_TRIGGER, LEFT_GRAB, RIGHT_TRIGGER, RIGHT_GRAB, LEFT_STICK_X, LEFT_STICK_Y, LEFT_STICK_CLICK, RIGHT_STICK_X, RIGHT_STICK_Y, RIGHT_STICK_CLICK):
            r[c] = '0'
    angle = math.radians(SWORD_DEGREES)
    for f in range(600, 660):
        rows[f - 1][RIGHT_GRAB] = '1'
    for f in range(620, 670):
        rows[f - 1][RIGHT_STICK_X], rows[f - 1][RIGHT_STICK_Y] = repr(math.cos(angle)), repr(math.sin(angle))
    y0, z0 = 1.25, .25
    place = {}                                   # frame -> (y, z) of the right hand
    y, z = y0, z0
    for f in range(900, 908):
        z -= .05; place[f] = (y, z)
    still = 910 + a.wait
    for f in range(908, still + 1):
        place[f] = (y, z)
    cy, cz = y, z
    for i in range(1, a.circle + 1):
        t = STEP * i
        y, z = cy + RADIUS * (1 - math.cos(t)), cz + RADIUS * math.sin(t)
        place[still + i] = (y, z)
    end = still + a.circle
    for i in range(1, 61):
        s = i / 60
        place[end + i] = (y + (y0 - y) * s, z + (z0 - z) * s)
    yy, zz = y0, z0
    for f in range(1300, 1308):
        zz -= .05; place[f] = (yy, zz)
    for i in range(1, 41):
        place[1407 + i] = (yy, zz + (z0 - zz) * i / 40)
    last = (y0, z0)
    for f in range(1, len(rows) + 1):
        if f in place:
            last = place[f]
        elif 1308 <= f <= 1407:
            last = (yy, zz)
        elif f > end + 60 and f < 1300 or f > 1447:
            last = (y0, z0)
        rows[f - 1][RIGHT_Y], rows[f - 1][RIGHT_Z] = repr(round(last[0], 6)), repr(round(last[1], 6))
    for f in range(840, 1121):                    # the left hand down, and up again at the end
        s = min(1.0, (f - 839) / 20) if f <= 1100 else max(0.0, 1 - (f - 1100) / 20)
        rows[f - 1][LEFT_Y] = repr(round(1.25 - .35 * s, 5))
    if end + 60 >= 1300:
        raise SystemExit('the circling ends too late for the lone stroke at 1300')
    meta = dict(wheel=[600, 670], stroke=[900, 908], calm=[908, still + 1], circle=[still + 1, end + 1], circle_speed=round(2 * RADIUS * math.sin(STEP / 2) * 90, 2), lone_stroke=[1300, 1308],
                stop_frame=1600, item=1125155603, first_attack=2496979158, next_attack_ms=550, wait_ms=440)
    out = ['# The cooldown between attacks: sword 600, stroke 900, then circling at 3.6 m/s %d..%d (one press when 80 %% of the cooldown has passed), a lone stroke 1300 (tools/make_cooldown_fixture.py).' % (still + 1, end),
           '# ' + json.dumps(meta)]
    out += [','.join(r) for r in rows]
    a.output.write_text('\n'.join(out) + '\n', encoding='utf-8', newline='\n')
    print(a.output, json.dumps(meta))


if __name__ == '__main__':
    main()
