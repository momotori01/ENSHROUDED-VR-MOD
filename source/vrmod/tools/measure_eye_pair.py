#!/usr/bin/env python3
"""Measure how the two eyes' pictures of a trial differ where they show the same thing.

  measure_eye_pair.py <game folder under vrmod/output> [--first <frame>] [--last <frame>]

User, 2026-10-13: "nothing is wrong as such, but there is a slight feeling that something differs between the left
and the right view ... is one eye a frame late, or anything like that?" This looks for the differences a pair of
eye pictures can have without anything being missing:
  vertical offset   the two eyes look the same way (the pair's native_capture_pose says so), so a thing lies at the
                    same height of the view in both. The two pictures do not have the same field of view (the headset's
                    eyes differ a little, and each picture is drawn with its own symmetric one), so the same height is
                    not the same row: the row it should be on is worked out from the two fields of view, and what is
                    measured is how far the thing is from there ("row error"). A constant error is a shifted picture,
                    one that grows towards the edges a picture of another size or a turned one.
  far things        a thing far away lies in the same direction for both eyes; "column error" is how far the best
                    fit is from that column (a close thing lies further left in the right picture: negative)
  brightness        mean of the right picture's block over the left one's
  sharpness         spread of the fine detail of the right picture's block over the left one's (one eye softer)
Both pictures at half their size; the left one in blocks of 48 x 48 pixels with enough detail; each block is looked
for in the right picture (fine detail only, so a difference in brightness does not decide the place): first along its
row, then two pixels around that with rows up to six above and below. Blocks that fit badly (hands, things that hide
other things, leaves at arm's length) are left out.
"""
import argparse, json, math, pathlib, re, statistics, sys

sys.path.insert(0, str(pathlib.Path(__file__).parent / 'python'))
from PIL import Image, ImageChops, ImageFilter, ImageStat  # noqa: E402

BLOCK, ROW_SHIFTS, AROUND, ROWS, DETAIL, FIT = 48, range(-60, 13, 2), 2, 6, 7., .42


def fine(im):
    """The picture without its slow changes (128 = none)."""
    return ImageChops.add(im, ImageChops.invert(im.filter(ImageFilter.GaussianBlur(6))), 2., 0)


def measure(left, right):
    w, h = left.size; a, b = fine(left), fine(right); cache = {}

    def differing(dx, dy):
        if (dx, dy) not in cache:
            cache[(dx, dy)] = ImageChops.difference(a, ImageChops.offset(b, dx, dy))
        return cache[(dx, dy)]
    out = []
    for y in range(BLOCK, h - 2 * BLOCK + 1, BLOCK):
        for x in range(2 * BLOCK, w - 2 * BLOCK + 1, BLOCK):
            box = (x, y, x + BLOCK, y + BLOCK)
            spread = ImageStat.Stat(a.crop(box)).stddev[0]
            if spread < DETAIL:
                continue
            best = min((ImageStat.Stat(differing(dx, 0).crop(box)).mean[0], dx) for dx in ROW_SHIFTS)
            fit, dx, dy = min((ImageStat.Stat(differing(ex, ey).crop(box)).mean[0], ex, ey)
                              for ex in range(best[1] - AROUND, best[1] + AROUND + 1) for ey in range(-ROWS, ROWS + 1))
            if fit > FIT * spread or abs(dy) == ROWS:
                continue
            # The right picture's block: the left block's place less the shift that was applied to the right picture.
            there = (x - dx, y - dy, x - dx + BLOCK, y - dy + BLOCK)
            if there[0] < 0 or there[1] < 0 or there[2] > w or there[3] > h:
                continue
            ml, mr = ImageStat.Stat(left.crop(box)).mean[0], ImageStat.Stat(right.crop(there)).mean[0]
            sr = ImageStat.Stat(b.crop(there)).stddev[0]
            out.append(dict(x=x + BLOCK / 2, y=y + BLOCK / 2, dx=dx, dy=dy, bright=mr / ml if ml > 4 else None, sharp=sr / spread))
    return out, (w, h)


def fields(game):
    """frame -> the two eyes' fields of view (tangents: left, right, up, down), where both eyes look the same way."""
    out = {}
    log = pathlib.Path('vrmod/output') / game / 'native-xr' / 'openxr.jsonl'
    with log.open(encoding='utf-8', errors='replace') as lines:
        for line in lines:
            if '"event":"native_capture_pose"' not in line:
                continue
            e = json.loads(line); eyes = e.get('eyes') or []
            if len(eyes) == 2 and max(abs(p - q) for p, q in zip(eyes[0]['orientation'], eyes[1]['orientation'])) < 1e-5:
                out[e['frame']] = [[math.tan(v) for v in eye['fov']] for eye in eyes]
    return out


def expected(block, size, fov):
    """Where the left picture's place lies in the right picture for a thing far away: (column, row)."""
    w, h = size; (l0, r0, u0, d0), (l1, r1, u1, d1) = fov
    tx = l0 + block['x'] / w * (r0 - l0); ty = u0 - block['y'] / h * (u0 - d0)
    return (tx - l1) / (r1 - l1) * w, (u1 - ty) / (u1 - d1) * h


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument('game'); p.add_argument('--first', type=int, default=0); p.add_argument('--last', type=int, default=10 ** 9)
    a = p.parse_args()
    folder = pathlib.Path('vrmod/output') / a.game / 'native-xr'
    frames = sorted(int(re.search(r'frame-(\d+)-eye-0', f.name).group(1)) for f in folder.glob('frame-*-eye-0.ppm'))
    frames = [f for f in frames if a.first <= f <= a.last]
    every = []; views = fields(a.game)
    for f in frames:
        pictures = [Image.open(folder / f'frame-{f}-eye-{eye}.ppm').convert('L') for eye in (0, 1)]
        size = pictures[0].size
        small = [im.resize((im.size[0] // 2, im.size[1] // 2), Image.BOX) for im in pictures]
        found, (w, h) = measure(small[0], small[1])
        if len(found) < 12 or f not in views:
            print('frame %d (%dx%d): %d blocks fit - too few, or no record of the fields of view' % (f, size[0], size[1], len(found))); continue
        for b in found:
            column, row = expected(b, (w, h), views[f])
            b['dy'] = (b['y'] - b['dy']) - row; b['far'] = (b['x'] - b['dx']) - column   # found place less expected place
        dy = [b['dy'] for b in found]
        thirds = {name: [b['dy'] for b in found if test(b)] for name, test in (
            ('left third', lambda b: b['x'] < w / 3), ('right third', lambda b: b['x'] > 2 * w / 3),
            ('upper third', lambda b: b['y'] < h / 3), ('lower third', lambda b: b['y'] > 2 * h / 3))}
        bright = [b['bright'] for b in found if b['bright']]; sharp = [b['sharp'] for b in found]
        far = sorted(b['far'] for b in found)
        print('frame %d (%dx%d): %d blocks fit | row error (half-size pixels) mean %+.2f, within one pixel in %d %%, by part: %s | column error of the furthest tenth %+.2f | brightness right/left median %.3f | fine detail right/left median %.3f' % (
            f, size[0], size[1], len(found), statistics.mean(dy), round(100 * sum(1 for v in dy if abs(v) <= 1) / len(dy)),
            ', '.join('%s %+.2f' % (k, statistics.mean(v)) for k, v in thirds.items() if len(v) >= 4),
            statistics.mean(far[-max(1, len(far) // 10):]), statistics.median(bright) if bright else float('nan'), statistics.median(sharp)))
        every += found
    if every:
        dy = [b['dy'] for b in every]; bright = [b['bright'] for b in every if b['bright']]; sharp = [b['sharp'] for b in every]
        q = statistics.quantiles
        far = sorted(b['far'] for b in every)
        print('column error (far things 0, close things negative): 99th percentile %+.2f, 90th %+.2f, median %+.2f' % (far[int(len(far) * .99)], far[int(len(far) * .9)], far[len(far) // 2]))
        print('all %d pairs, %d blocks: row error mean %+.3f (within half a pixel in %d %%, within one pixel in %d %%) | brightness right/left median %.3f (quartiles %.3f .. %.3f) | fine detail right/left median %.3f (quartiles %.3f .. %.3f)' % (
            len(frames), len(every), statistics.mean(dy), round(100 * sum(1 for v in dy if abs(v) <= .5) / len(dy)), round(100 * sum(1 for v in dy if abs(v) <= 1) / len(dy)),
            statistics.median(bright), q(bright, n=4)[0], q(bright, n=4)[2], statistics.median(sharp), q(sharp, n=4)[0], q(sharp, n=4)[2]))


if __name__ == '__main__':
    main()
