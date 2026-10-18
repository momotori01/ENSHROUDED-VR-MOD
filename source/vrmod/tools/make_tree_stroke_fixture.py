#!/usr/bin/env python3
"""Mock hand script that strikes the tree in front of the home with the sword: hit evidence for the live melee contact.

With -LiveMeleeContact -LiveMeleeEarly the attack's hit volume is on the tracked weapon. Inside the home nothing can
be hit (the floor plants are no targets), so the queue of hits always stayed empty. Here tools/mock_walk_to.py first
walks the character against the trunk of the tree outside (-MockNavigate), then this script strikes:

   400        quick wheel: sword (129 degrees)
   500..1400  nothing (the walk)
   1500..     stroke 1: blade level, pointing forward, right to left across the trunk
   1800..     stroke 2: the same, left to right, a little higher and nearer
   2100..     stroke 3: the same, right to left, lower and farther
   2400..     stroke 4, the control: hand behind the body, blade level, pointing backward, away from the trunk
Every stroke moves 6 cm a frame (5.4 m/s) for 34 frames; the hand then returns slowly. With the observers on
(-LiveMeleeContactObserve) strokes 1 to 3 should fill the game's own queue of hits, stroke 4 should not.
"""
import argparse, json, math, pathlib
import make_tool_select_fixture as select

RIGHT, RIGHT_ROT = (14, 15, 16), (17, 18, 19)
LEFT_STICK_X, LEFT_STICK_Y, RIGHT_SECONDARY = 29, 30, 38
STEP, FRAMES = .06, 34
LEVEL = -55.0   # hand pitch at which the mounted weapon lies level (it points 55 degrees above the hand's forward)
STROKES = [dict(start=1500, forward=.38, height=1.15, yaw=-90, sign=-1, target=True), dict(start=1800, forward=.25, height=1.30, yaw=-90, sign=1, target=True),
           dict(start=2100, forward=.55, height=1.00, yaw=-90, sign=-1, target=True), dict(start=2400, forward=-.25, height=1.15, yaw=90, sign=-1, target=False)]
PARTS = dict(wheel=400, walk=(500, 1400), strokes=[s['start'] for s in STROKES], on_target=[s['target'] for s in STROKES], stroke_frames=FRAMES, stop_frame=2700)


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--source', type=pathlib.Path, default=pathlib.Path('vrmod/test_env/home-ranged-animation-20261010.csv'))
    p.add_argument('--slot', choices=tuple(select.SLOTS), default='sword')
    p.add_argument('--output', type=pathlib.Path, default=pathlib.Path('vrmod/test_env/home-tree-stroke-20261011.csv'))
    a = p.parse_args()
    rows = [line.split(',') for line in a.source.read_text(encoding='utf-8').splitlines() if line and not line.startswith('#')]
    if len(rows) != 4300 or any(len(r) != 39 for r in rows) or any(r[14:20] != ['0.38', '1.25', '0.25', '-90', '0', '0'] for r in rows):
        raise SystemExit('unexpected source script')
    for r in rows:
        r[select.LEFT_TRIGGER] = r[select.RIGHT_TRIGGER] = r[select.LEFT_GRAB] = r[select.RIGHT_GRAB] = r[select.RIGHT_STICK_X] = r[select.RIGHT_STICK_Y] = r[LEFT_STICK_X] = r[LEFT_STICK_Y] = r[RIGHT_SECONDARY] = '0'
    angle = math.radians(select.SLOTS[a.slot])
    for frame in range(400, 460):
        rows[frame - 1][select.RIGHT_GRAB] = '1'
    for frame in range(420, 470):
        rows[frame - 1][select.RIGHT_STICK_X], rows[frame - 1][select.RIGHT_STICK_Y] = repr(math.cos(angle)), repr(math.sin(angle))
    rest = (0.38, 1.25, 0.25, -90.0, 0.0)       # forward, up, right, yaw, pitch of the right hand at rest
    pose = {}
    for s in STROKES:
        half = STEP * FRAMES / 2; z0 = -s['sign'] * half     # the stroke is centred in front of the body
        first = (s['forward'], s['height'], z0, float(s['yaw']), LEVEL)
        last = (s['forward'], s['height'], z0 + s['sign'] * STEP * FRAMES, float(s['yaw']), LEVEL)
        approach, back = 90, 100                 # slow: under 1 m/s, so that only the stroke itself presses the attack
        for i in range(approach):
            t = (i + 1) / approach
            pose[s['start'] - approach + i] = tuple(u + (v - u) * t for u, v in zip(rest, first))
        for i in range(FRAMES + 1):
            pose[s['start'] + i] = (s['forward'], s['height'], z0 + s['sign'] * STEP * i, float(s['yaw']), LEVEL)
        for i in range(60):                      # hold: the hit volume outlives the stroke
            pose[s['start'] + FRAMES + 1 + i] = last
        for i in range(back):
            t = (i + 1) / back
            pose[s['start'] + FRAMES + 61 + i] = tuple(u + (v - u) * t for u, v in zip(last, rest))
    for frame, (x, y, z, yaw, pitch) in pose.items():
        r = rows[frame - 1]
        r[RIGHT[0]], r[RIGHT[1]], r[RIGHT[2]] = repr(round(x, 6)), repr(round(y, 6)), repr(round(z, 6))
        r[RIGHT_ROT[0]], r[RIGHT_ROT[1]] = repr(round(yaw, 6)), repr(round(pitch, 6))
    out = [f'# {a.slot} struck across the tree trunk in front of the home (tools/make_tree_stroke_fixture.py): hit evidence for the live melee contact.', '# ' + json.dumps(PARTS)]
    out += [','.join(r) for r in rows]
    a.output.write_text('\n'.join(out) + '\n', encoding='utf-8', newline='\n')
    print(a.output, json.dumps(PARTS))


if __name__ == '__main__':
    main()
