#!/usr/bin/env python3
"""One-frame flashes in bursts of consecutive pictures (2026-10-15, headset run 9, user: "the lights flicker - the bonfire
at the start, the lantern of the next room; outdoors many places flash as black shadows about twice a second").

  check_burst_flicker.py <game folder> [--threshold 6]

The bursts (key script "BURST n", <game folder>/native-xr/frame-<n>-eye-<e>.ppm on consecutive XR frames) are taken with
a still head. Each picture is reduced to a grid of 24 x 24 block means; a frame flashes where some block differs from
both its neighbours by more than the threshold (mean of the three channels, 0..255) while the neighbours agree with each
other (within half of it) - a change that comes and goes in one frame. Blocks that change on most frames (a flame) are
left out. Printed per burst and eye: the flashing frames,
the largest such block change, and the gaps between flashes (30 = the desktop refresh).
"""
import argparse, json
from pathlib import Path
from PIL import Image

GRID = 24


def blocks(path):
    im = Image.open(path).convert('RGB').resize((GRID, GRID), Image.BOX)
    return [sum(p) / 3 for p in im.getdata()]


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument('game', type=Path); p.add_argument('--threshold', type=float, default=6)
    a = p.parse_args()
    folder = a.game / 'native-xr'
    frames = sorted(int(x.name.split('-')[1]) for x in folder.glob('frame-*-eye-0.ppm') if (folder / x.name.replace('eye-0', 'eye-1')).exists())
    runs = []
    for n in frames:
        if runs and n == runs[-1][-1] + 1: runs[-1].append(n)
        else: runs.append([n])
    runs = [r for r in runs if len(r) >= 8]
    out = []
    for run in runs:
        for eye in (0, 1):
            b = {f: blocks(folder / f'frame-{f}-eye-{eye}.ppm') for f in run}
            # Blocks that move all the time (a flame, its sparks) are left out: their usual change from frame to frame
            # (the median over the burst) must be small against the threshold.
            usual = [sorted(abs(b[x][i] - b[y][i]) for x, y in zip(run, run[1:]))[len(run) // 2] for i in range(GRID * GRID)]
            calm = [i for i in range(GRID * GRID) if usual[i] <= a.threshold / 4]
            flashes, sizes = [], []
            for prev, f, nxt in zip(run, run[1:], run[2:]):
                worst = 0
                for i in calm:
                    x, y, z = b[prev][i], b[f][i], b[nxt][i]
                    if abs(x - z) <= a.threshold / 2: worst = max(worst, min(abs(y - x), abs(y - z)))
                if worst > a.threshold: flashes.append(f); sizes.append(round(worst, 1))
            out.append(dict(first=run[0], last=run[-1], eye=eye, calm_blocks=len(calm), flashes=flashes, sizes=sizes, gaps=[y - x for x, y in zip(flashes, flashes[1:])]))
    print(json.dumps(dict(bursts=len(runs), threshold=a.threshold, flashing_frames=sum(len(o['flashes']) for o in out), runs=out), indent=1))


if __name__ == '__main__':
    main()
