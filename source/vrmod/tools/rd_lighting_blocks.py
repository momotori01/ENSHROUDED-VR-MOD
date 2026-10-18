"""After rd_lighting_inputs.py: the eyes' sun shadow mask compared with a parallax tolerance (the least difference over
shifts of -4..4 x -2..2 pixels) and their indirect light (GI) in 8x8 blocks (relative difference of the block means), on
pixels with geometry. Pictures: <out>/sun-diff.png (red: the masks differ beyond the tolerance), <out>/gi-blocks.png
(red: the left eye's block brighter, blue: the right's).

  python rd_lighting_blocks.py <the rd_lighting_inputs output dir>
"""
import json, struct, sys
from pathlib import Path
from PIL import Image, ImageChops


def e9(w):
    e = (w >> 27) & 31
    return ((w & 511) + ((w >> 9) & 511) + ((w >> 18) & 511)) * 2.0 ** (e - 24) / 3


def main():
    out = Path(sys.argv[1])
    tex = out / 'texels'
    recs = [json.loads(l) for l in (tex / 'texels.jsonl').read_text().splitlines()]
    names = ['sun_L', 'sun_R', 'vol_L', 'vol_R', 'gi_L', 'gi_R', 'depth_L', 'depth_R', 'atlas_L', 'atlas_R']
    files = {n: tex / r['file'] for n, r in zip(names, recs)}
    n = recs[0]['width']
    sl = Image.frombytes('L', (n, n), files['sun_L'].read_bytes())
    sr = Image.frombytes('L', (n, n), files['sun_R'].read_bytes())
    dl = struct.unpack('<%df' % (n * n), files['depth_L'].read_bytes())
    best = None
    for dy in range(-2, 3):
        for dx in range(-4, 5):
            d = ImageChops.difference(sl, ImageChops.offset(sr, dx, dy))
            best = d if best is None else ImageChops.darker(best, d)
    mask = best.point(lambda v: 255 if v > 77 else 0)
    geo = Image.frombytes('L', (n, n), bytes(255 if v > 0 else 0 for v in dl))
    m = ImageChops.multiply(mask, geo)
    g = geo.histogram()[255]
    print('sun mask: pixels with geometry %d, beyond the tolerance %d (%.2f %%)' % (g, m.histogram()[255], 100 * m.histogram()[255] / max(1, g)))
    base = sl.point(lambda v: v // 3)
    Image.merge('RGB', (ImageChops.lighter(base, m), base, base)).save(out / 'sun-diff.png')
    M = n // 8
    gl = [e9(w) for w in struct.unpack('<%dI' % (n * n), files['gi_L'].read_bytes())]
    gr = [e9(w) for w in struct.unpack('<%dI' % (n * n), files['gi_R'].read_bytes())]
    small = lambda v: list(Image.frombytes('F', (n, n), struct.pack('<%df' % (n * n), *v)).resize((M, M), Image.BOX).getdata())
    a, b = small(gl), small(gr)
    cover = list(Image.frombytes('F', (n, n), struct.pack('<%df' % (n * n), *[1.0 if v > 0 else 0.0 for v in dl])).resize((M, M), Image.BOX).getdata())
    idx = [i for i in range(M * M) if cover[i] > .99]
    rel = sorted(abs(a[i] - b[i]) / max(1e-6, (a[i] + b[i]) / 2) for i in idx)
    print('GI 8x8 blocks %d: median %.3f, 90th %.3f, over 20 %% %.3f' % (len(rel), rel[len(rel) // 2], rel[int(len(rel) * .9)], sum(1 for x in rel if x > .2) / len(rel)))
    mx = sorted(a)[int(len(a) * .99)] or 1
    px = []
    for i in range(M * M):
        if cover[i] < .99:
            px.append((0, 0, 0)); continue
        r = (a[i] - b[i]) / max(1e-6, (a[i] + b[i]) / 2); v = int(min(255, 90 * a[i] / mx))
        px.append((min(255, v + int(max(0, r) * 500)), v, min(255, v + int(max(0, -r) * 500))))
    im = Image.new('RGB', (M, M)); im.putdata(px); im.resize((M * 6, M * 6), Image.NEAREST).save(out / 'gi-blocks.png')


if __name__ == '__main__':
    main()
