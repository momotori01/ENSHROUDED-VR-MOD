#!/usr/bin/env python3
"""Find what one eye's picture shows and the other's does not (a tree drawn for one eye only, a light, an effect).

  check_eye_difference.py <game folder under vrmod/output> <output directory> [--first <frame>]

For every pair of eye pictures of the trial (native-xr/frame-N-eye-0.ppm / -eye-1.ppm), at half their size and a
little blurred (the two eyes sample leaves and grass differently), the left picture is cut into blocks of 24 x 24
pixels. For each block the right picture is searched along the same rows for the place that fits it best (the left
eye sees a thing further to the right than the right eye does: shifts of -56 .. +8 pixels of the right picture),
which gives the block its shift and what still differs there (mean absolute difference of the luma).
  far and alike   the difference is at most 12 or 70 % of the block's own spread, with a shift of 4 pixels at the most
  differing       more than that
A thing far away fits almost without a shift. So a differing block with at least five of its eight neighbours far
and alike is a thing of the far scenery that one eye shows and the other does not - those are marked red and counted.
Differing blocks among close things (hands, grass, the leaves of a tree at arm's length: every eye sees past them
differently) are marked orange and not counted. Flat blocks (the sky) fit anywhere and count as far and alike.
The same is done from the right picture to the left one.
Writes one sheet per pair that has red marks (left eye, right eye) and a list.
"""
import argparse, pathlib, re, sys

sys.path.insert(0, str(pathlib.Path(__file__).parent / 'python'))
from PIL import Image, ImageChops, ImageDraw, ImageFilter, ImageStat  # noqa: E402

BLOCK, SHIFTS, LIMIT, FLAT, SHARE, FAR, NEIGHBOURS = 24, range(-56, 9), 12., 6., .7, 4, 5


def blocks(a, b, sign):
    """Every block of a: (differs, far and alike). sign +1: b is the right eye's picture (things lie further left there)."""
    w, h = a.size; out = {}
    shifted = {s: ImageChops.difference(a, ImageChops.offset(b, -sign * s, 0)) for s in SHIFTS}
    for y in range(0, h - BLOCK + 1, BLOCK):
        for x in range(72, w - BLOCK - 72 + 1, BLOCK):
            box = (x, y, x + BLOCK, y + BLOCK)
            spread = ImageStat.Stat(a.crop(box)).stddev[0]
            if spread < FLAT:
                out[(x, y)] = (False, True); continue
            best, shift = min((ImageStat.Stat(shifted[s].crop(box)).mean[0], abs(s)) for s in SHIFTS)
            differs = best > LIMIT and best > SHARE * spread
            out[(x, y)] = (differs, not differs and shift <= FAR)
    return out


def classify(found):
    red, orange = [], []
    for (x, y), (differs, _) in found.items():
        if not differs:
            continue
        around = [found.get((x + dx * BLOCK, y + dy * BLOCK)) for dx in (-1, 0, 1) for dy in (-1, 0, 1) if dx or dy]
        far = sum(1 for n in around if n and n[1])
        (red if far >= NEIGHBOURS else orange).append((x, y))
    return red, orange


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument('game'); p.add_argument('output', type=pathlib.Path); p.add_argument('--first', type=int, default=0)
    a = p.parse_args()
    folder = pathlib.Path('vrmod/output') / a.game / 'native-xr'
    frames = sorted(int(re.search(r'frame-(\d+)-eye-0', f.name).group(1)) for f in folder.glob('frame-*-eye-0.ppm'))
    frames = [f for f in frames if f >= a.first]
    a.output.mkdir(parents=True, exist_ok=True)
    total = 0
    for f in frames:
        left = Image.open(folder / f'frame-{f}-eye-0.ppm'); right = Image.open(folder / f'frame-{f}-eye-1.ppm')
        scale = 2 if left.size[0] > 1400 else 1
        small = [im.convert('L').resize((im.size[0] // scale, im.size[1] // scale), Image.BOX).filter(ImageFilter.GaussianBlur(2)) for im in (left, right)]
        sides = [classify(blocks(small[0], small[1], 1)), classify(blocks(small[1], small[0], -1))]
        print('frame %d: far things only in the left eye %d, only in the right eye %d (differing among close things: %d, %d)' % (
            f, len(sides[0][0]), len(sides[1][0]), len(sides[0][1]), len(sides[1][1])))
        total += len(sides[0][0]) + len(sides[1][0])
        if not sides[0][0] and not sides[1][0]:
            continue
        w, h = small[0].size
        sheet = Image.new('RGB', (w * 2, h))
        for i, (im, (red, orange)) in enumerate(zip((left, right), sides)):
            tile = im.resize((w, h), Image.BOX).convert('RGB'); draw = ImageDraw.Draw(tile)
            for x, y in orange:
                draw.rectangle((x, y, x + BLOCK - 1, y + BLOCK - 1), outline=(255, 160, 0))
            for x, y in red:
                draw.rectangle((x - 1, y - 1, x + BLOCK, y + BLOCK), outline=(255, 0, 0), width=2)
            sheet.paste(tile, (w * i, 0))
        sheet.save(a.output / f'eyes-{f}.png')
    print('pairs', len(frames), 'far things in one eye only (blocks)', total)


if __name__ == '__main__':
    main()
