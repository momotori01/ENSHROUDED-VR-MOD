#!/usr/bin/env python3
"""How far the two eyes' pictures differ in brightness where they show the same thing (2026-10-17 afternoon).

User, 2026-10-17: "the shadow density differs, or the shadow is missing, in one eye here and there". For every pair of
eye pictures of a Mock run (native-xr/frame-N-eye-0.ppm / -eye-1.ppm, e.g. the look around of
test_env/keys-look-around-captures-20261017.txt), at half size and a little blurred, the left picture is cut into blocks
of 24 x 24 pixels and each block is searched along the same rows of the right picture (shifts -28 .. +4 at half size:
the right eye sees a thing further left). At the best place the mean brightness of the two blocks is compared:
(left - right) / mean. Blocks of the sky (flat), too dark ones (mean below 10) and close things (a best shift beyond
-8, they are seen past differently by each eye) are left out, and so is the outer 12 % of the picture (the edge
tint of the VR vignette darkens each eye's outer edge, which matches the other eye's picture further inside).

  eye_shade_survey.py <game folder> [<game folder> ...] [--sheets <dir>]

Prints per run and per pair the share of blocks more than 10 / 20 % apart and the mean signed difference; with
--sheets, one picture per pair (left eye, right eye, the blocks: red = the left eye brighter, blue = the right).
"""
import argparse, pathlib, re, sys

sys.path.insert(0, str(pathlib.Path(__file__).parent / 'python'))
from PIL import Image, ImageChops, ImageDraw, ImageFilter, ImageStat  # noqa: E402

BLOCK, SHIFTS, DARK, FLAT, NEAR, MARGIN = 24, range(-28, 5), 10., 3., -8, .12


def pair_blocks(a, b):
    w, h = a.size; out = []; m = int(w * MARGIN)
    for y in range(m, h - m - BLOCK + 1, BLOCK):
        for x in range(m, w - m - BLOCK + 1, BLOCK):
            blk = a.crop((x, y, x + BLOCK, y + BLOCK)); st = ImageStat.Stat(blk)
            if st.stddev[0] < FLAT:
                continue
            best = None
            for s in SHIFTS:
                if not 0 <= x + s <= w - BLOCK:
                    continue
                d = ImageStat.Stat(ImageChops.difference(blk, b.crop((x + s, y, x + s + BLOCK, y + BLOCK)))).mean[0]
                if best is None or d < best[0]:
                    best = (d, s)
            if best is None or best[1] < NEAR:
                continue
            la = st.mean[0]; lb = ImageStat.Stat(b.crop((x + best[1], y, x + best[1] + BLOCK, y + BLOCK))).mean[0]
            if max(la, lb) < DARK:
                continue
            out.append((x, y, (la - lb) / ((la + lb) / 2)))
    return out


def load(p):
    im = Image.open(p).convert('L')
    return im.resize((im.size[0] // 2, im.size[1] // 2), Image.BILINEAR).filter(ImageFilter.GaussianBlur(1))


def sheet(path, a, b, found):
    w, h = a.size
    c = Image.new('RGB', (w * 3 + 16, h), (255, 255, 255))
    c.paste(a.convert('RGB'), (0, 0)); c.paste(b.convert('RGB'), (w + 8, 0))
    d = ImageDraw.Draw(c)
    for x, y, r in found:
        col = (min(255, int(max(0, r) * 900)), 30, min(255, int(max(0, -r) * 900)))
        d.rectangle((2 * w + 16 + x, y, 2 * w + 16 + x + BLOCK - 1, y + BLOCK - 1), fill=col)
    c.save(path, quality=88)


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument('games', nargs='+'); p.add_argument('--sheets', type=pathlib.Path)
    a = p.parse_args()
    for g in a.games:
        d = pathlib.Path(g) / 'native-xr'
        if not d.is_dir():
            d = pathlib.Path(g)  # pictures moved out of the run (DeleteMe/<day>/ppm/<run>)
        frames = sorted({int(m.group(1)) for f in d.glob('frame-*-eye-0.ppm') if (m := re.match(r'frame-(\d+)-eye-0', f.name))})
        total = []
        for fr in frames:
            pa, pb = d / f'frame-{fr}-eye-0.ppm', d / f'frame-{fr}-eye-1.ppm'
            if not pb.exists():
                continue
            la, lb = load(pa), load(pb)
            found = pair_blocks(la, lb)
            if not found:
                continue
            v = [abs(r) for _, _, r in found]
            print('%s %5d: blocks %3d, mean luma %5.1f, over 10 %% %.3f, over 20 %% %.3f, mean signed %+.3f' % (
                pathlib.Path(g).name, fr, len(v), ImageStat.Stat(la).mean[0], sum(x > .1 for x in v) / len(v),
                sum(x > .2 for x in v) / len(v), sum(r for _, _, r in found) / len(v)))
            total += found
            if a.sheets:
                a.sheets.mkdir(parents=True, exist_ok=True)
                sheet(a.sheets / f'{pathlib.Path(g).name}-{fr}.jpg', la, lb, found)
        if total:
            v = [abs(r) for _, _, r in total]
            print('%s all: blocks %d, over 10 %% %.3f, over 20 %% %.3f, mean |diff| %.3f, mean signed %+.3f' % (
                pathlib.Path(g).name, len(v), sum(x > .1 for x in v) / len(v), sum(x > .2 for x in v) / len(v),
                sum(v) / len(v), sum(r for _, _, r in total) / len(v)))


if __name__ == '__main__':
    main()
