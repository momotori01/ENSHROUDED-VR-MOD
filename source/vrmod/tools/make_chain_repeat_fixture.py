#!/usr/bin/env python3
"""Mock hand script with the sword's two-step chain several times over, for what shows only now and then
(the swing effect of the chain's second step: Mock runs game-20261013-095147 with it, 095623 without).

It is home-cooldown-late-20261013.csv (tools/make_cooldown_fixture.py --wait 50: a stroke at 900, a second press
about 1.3 s into the first attack, a lone stroke at 1300) with the block of frames 840..1100 - the left hand hanging,
the stroke, the wait, the circling that presses once, the way back - played again eight times, 350 frames apart, from
1540 on. Nine chains in all. Stop at 4290 (run it with -Minutes 4).
"""
import argparse, pathlib

BLOCK = (840, 1101)
STARTS = [1540, 1890, 2240, 2590, 2940, 3290, 3640, 3990]


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument('--source', type=pathlib.Path, default=pathlib.Path('vrmod/test_env/home-cooldown-late-20261013.csv'))
    p.add_argument('--output', type=pathlib.Path, default=pathlib.Path('vrmod/test_env/home-chain-repeat-20261013.csv'))
    a = p.parse_args()
    rows = [line.split(',') for line in a.source.read_text(encoding='utf-8').splitlines() if line and not line.startswith('#')]
    if len(rows) < 4300 or any(len(r) != 39 for r in rows) or rows[BLOCK[0] - 2][1:] != rows[BLOCK[1] + 60][1:]:
        raise SystemExit('unexpected source script (the block has to start and end at the hands\' usual place)')
    for start in STARTS:
        for frame in range(BLOCK[0], BLOCK[1]):
            rows[start + frame - BLOCK[0] - 1][1:] = list(rows[frame - 1][1:])
    a.output.write_text('\n'.join(['# The sword\'s chain nine times: strokes at 900, 1600, 1950, 2300, 2650, 3000, 3350, 3700, 4050, each with a second press about 1.3 s later; a lone stroke at 1300 (tools/make_chain_repeat_fixture.py). Stop at 4290.'] + [','.join(r) for r in rows]) + '\n', encoding='utf-8', newline='\n')
    print(a.output, len(rows), 'stop 4290')


if __name__ == '__main__':
    main()
