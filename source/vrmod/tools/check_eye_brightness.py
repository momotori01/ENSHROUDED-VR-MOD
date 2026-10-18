#!/usr/bin/env python3
"""Where the two eyes show the same thing at a different brightness (fog thicker in one eye, a wall lit more brightly).

  check_eye_brightness.py <game folder under vrmod/output> <output directory> [--first <frame>]

User, 2026-10-14 (headset run 20261014-162900): "in places the brightness or the fog (or shadow?) differs between
the eyes: compare the screenshots - 16:40 the thickness of the fog, 16:45 the brightness of the walls in the house".
tools/check_eye_difference.py finds things that one eye shows and the other does not; this one looks for the same
picture at another brightness. For every pair of eye pictures (native-xr/frame-N-eye-0.ppm / -eye-1.ppm), at a
quarter of their size and a little blurred, the left picture is cut into blocks of 16 x 16 pixels (64 of the full
size). Each block is looked for in the right picture along the same rows (shifts of -14 .. +2, the left eye sees a
thing further right) by its shape alone - each block less its own mean - and at the best place the two means are
compared. A block whose shape fits (mean absolute difference of the shapes at most 4) and whose brightness differs
by more than 6 of 255 is marked: red where the left eye is brighter, blue where the right one is. The whole
pictures' mean brightness is reported too (a difference there is the exposure of one eye).
Writes one sheet per pair (left eye, right eye, marks) and a list.
"""
import argparse, pathlib, re, sys

sys.path.insert(0, str(pathlib.Path(__file__).parent / 'python'))
from PIL import Image, ImageChops, ImageDraw, ImageFilter, ImageStat  # noqa: E402

SCALE, BLOCK, SHIFTS, SHAPE, BRIGHT = 4, 16, range(-14, 3), 4.0, 6.0


def luma(path):
    return Image.open(path).convert('L')


def small(image):
    w, h = image.size
    return image.resize((w // SCALE, h // SCALE), Image.BILINEAR).filter(ImageFilter.GaussianBlur(.8))


def compare(a, b):
    """Every block of a: (x, y, brightness of a less b) where the shapes fit."""
    w, h = a.size; out = []
    for y in range(0, h - BLOCK + 1, BLOCK):
        for x in range(0, w - BLOCK + 1, BLOCK):
            ca = a.crop((x, y, x + BLOCK, y + BLOCK)); ma = ImageStat.Stat(ca).mean[0]
            best = None
            for s in SHIFTS:
                xb = x + s
                if xb < 0 or xb + BLOCK > w: continue
                cb = b.crop((xb, y, xb + BLOCK, y + BLOCK)); mb = ImageStat.Stat(cb).mean[0]
                moved = cb.point(lambda v, o=ma - mb: v + o)   # b's block at a's mean: the shapes alone are compared
                shape = ImageStat.Stat(ImageChops.difference(ca, moved)).mean[0]
                if best is None or shape < best[0]: best = (shape, ma - mb)
            if best and best[0] <= SHAPE: out.append((x, y, best[1]))
    return out


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument('game'); p.add_argument('output', type=pathlib.Path); p.add_argument('--first', type=int, default=0)
    a = p.parse_args()
    folder = pathlib.Path('vrmod/output') / a.game / 'native-xr'
    frames = sorted(int(re.search(r'frame-(\d+)-eye-0', f.name).group(1)) for f in folder.glob('frame-*-eye-0.ppm'))
    frames = [f for f in frames if f >= a.first and (folder / f'frame-{f}-eye-1.ppm').exists()]
    a.output.mkdir(parents=True, exist_ok=True); lines = []
    for f in frames:
        left = luma(folder / f'frame-{f}-eye-0.ppm'); right = luma(folder / f'frame-{f}-eye-1.ppm')
        ml, mr = ImageStat.Stat(left).mean[0], ImageStat.Stat(right).mean[0]
        sl, sr = small(left), small(right)
        blocks = compare(sl, sr)
        marked = [(x, y, d) for x, y, d in blocks if abs(d) > BRIGHT]
        lb = sum(1 for *_, d in marked if d > 0); rb = len(marked) - lb
        mean_abs = sum(abs(d) for *_, d in blocks) / max(1, len(blocks))
        line = (f'frame {f}: whole picture mean left {ml:.1f} right {mr:.1f}; blocks compared {len(blocks)}, '
                f'brighter left {lb}, brighter right {rb}, mean |difference| {mean_abs:.2f}')
        print(line); lines.append(line)
        if marked:
            w, h = sl.size
            sheet = Image.new('RGB', (w * 2, h), (0, 0, 0))
            sheet.paste(sl.convert('RGB'), (0, 0)); sheet.paste(sr.convert('RGB'), (w, 0))
            draw = ImageDraw.Draw(sheet)
            for x, y, d in marked:
                c = (255, 40, 40) if d > 0 else (60, 120, 255)
                draw.rectangle((x, y, x + BLOCK - 1, y + BLOCK - 1), outline=c)
                draw.text((x + 2, y + 2), f'{d:+.0f}', fill=c)
            sheet.save(a.output / f'brightness-{f}.png')
    (a.output / 'brightness.txt').write_text('\n'.join(lines) + '\n', encoding='utf-8')


if __name__ == '__main__':
    main()
