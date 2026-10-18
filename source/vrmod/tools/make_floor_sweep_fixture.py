#!/usr/bin/env python3
"""Mock hand script that sweeps the great axe low over the floor in front of the home spawn.

With the live melee contact (-LiveMeleeContact -LiveMeleeEarly) the attack's hit volume is on the tracked weapon,
so a stroke can be aimed at something that the game's own swing (at chest height in front of the character) does not
reach: the grass and flowers growing through the floor about a metre ahead. The script is a probe for hit evidence
(motion_owned_vm queue counts with the observers on, and the captured images before and after).

   400        quick wheel: great axe (51 degrees)
   600..      stroke 1: weapon 35 degrees below level, hand at 0.70 m, right to left
   900..      stroke 2: the same, left to right, hand at 0.55 m
   1200..     stroke 3: weapon 55 degrees below level, hand at 0.90 m, right to left, nearer
   1500..     stroke 4: at chest height with the weapon up (the usual stroke): must not reach the floor
Every stroke moves 6 cm a frame (5.4 m/s) for 34 frames; the hand then returns slowly.
"""
import argparse, json, pathlib
import make_tool_select_fixture as select

RIGHT, RIGHT_ROT = (14, 15, 16), (17, 18, 19)
STEP, FRAMES = .06, 34
STROKES = [dict(start=600, height=.70, pitch=-90, sign=-1), dict(start=900, height=.55, pitch=-90, sign=1),
           dict(start=1200, height=.90, pitch=-110, sign=-1), dict(start=1500, height=1.25, pitch=0, sign=-1)]
PARTS = dict(strokes=[s['start'] for s in STROKES], stroke_frames=FRAMES, stop_frame=1800)


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--source', type=pathlib.Path, default=pathlib.Path('vrmod/test_env/home-ranged-animation-20261010.csv'))
    p.add_argument('--slot', choices=tuple(select.SLOTS), default='axe', help='quick wheel item swung (axe, or pickaxe for the tool route)')
    p.add_argument('--output', type=pathlib.Path)
    a = p.parse_args()
    output = a.output or pathlib.Path('vrmod/test_env/home-floor-sweep-20261011.csv' if a.slot == 'axe' else f'vrmod/test_env/home-floor-sweep-{a.slot}-20261011.csv')
    rows = [line.split(',') for line in a.source.read_text(encoding='utf-8').splitlines() if line and not line.startswith('#')]
    if len(rows) != 4300 or any(len(r) != 39 for r in rows) or any(r[14:17] != ['0.38', '1.25', '0.25'] for r in rows):
        raise SystemExit('unexpected source script')
    for r in rows:
        r[select.LEFT_TRIGGER] = r[select.RIGHT_TRIGGER] = r[select.LEFT_GRAB] = r[select.RIGHT_GRAB] = r[select.RIGHT_STICK_X] = r[select.RIGHT_STICK_Y] = '0'
    import math
    angle = math.radians(select.SLOTS[a.slot])
    for frame in range(400, 460):
        rows[frame - 1][select.RIGHT_GRAB] = '1'
    for frame in range(420, 470):
        rows[frame - 1][select.RIGHT_STICK_X], rows[frame - 1][select.RIGHT_STICK_Y] = repr(math.cos(angle)), repr(math.sin(angle))
    rest = (0.38, 1.25, 0.25)
    pose = {}                                   # frame -> (x, y, z, pitch)
    for s in STROKES:
        half = STEP * FRAMES / 2; z0 = -s['sign'] * half     # the stroke is centred in front of the body
        approach = 90                            # slow move to the start of the stroke (under 1 m/s with the turn of the hand; 40 frames reached 3.4 m/s and pressed the attack with a one-hand tool, game133102)
        for i in range(approach):
            t = (i + 1) / approach
            pose[s['start'] - approach + i] = (rest[0], rest[1] + (s['height'] - rest[1]) * t, rest[2] + (z0 - rest[2]) * t, s['pitch'] * t)
        for i in range(FRAMES + 1):
            pose[s['start'] + i] = (rest[0], s['height'], z0 + s['sign'] * STEP * i, s['pitch'])
        end = z0 + s['sign'] * STEP * FRAMES; back = 100   # done before the next approach starts (300 frames between strokes)
        for i in range(60):                      # hold: the hit volume outlives the stroke
            pose[s['start'] + FRAMES + 1 + i] = (rest[0], s['height'], end, s['pitch'])
        for i in range(back):
            t = (i + 1) / back
            pose[s['start'] + FRAMES + 61 + i] = (rest[0], s['height'] + (rest[1] - s['height']) * t, end + (rest[2] - end) * t, s['pitch'] * (1 - t))
    for frame, (x, y, z, pitch) in pose.items():
        r = rows[frame - 1]
        r[RIGHT[0]], r[RIGHT[1]], r[RIGHT[2]] = repr(round(x, 6)), repr(round(y, 6)), repr(round(z, 6))
        r[RIGHT_ROT[1]] = repr(round(pitch, 6))
    out = [f'# {a.slot} swept low over the floor (tools/make_floor_sweep_fixture.py): a probe for hit evidence of the live melee contact.', '# ' + json.dumps(PARTS)]
    out += [','.join(r) for r in rows]
    output.write_text('\n'.join(out) + '\n', encoding='utf-8', newline='\n')
    print(output)


if __name__ == '__main__':
    main()
