"""The eyes' indirect light (GI) compared where they see the same point (2026-10-17, layer/gi_reproject.hpp).
Input: an output dir of rd_lighting_inputs.py (the two lighting dispatches' bindings and texels). For each right pixel:
the camera-relative position from the right depth and the right lighting constants (_32_m6, the lighting shader's own
formula), + the right camera (_32_m7) = world; the left constants' _32_m5 (world -> clip) give the left pixel and the
depth it should have there; where the left depth agrees within 3 %, the two irradiance words are compared.
Prints the share of such pixels, the relative difference of the two (median, 90th) and per 8x8 block, and writes
<dir>/gi-matched.png: left GI | right GI | right GI made from the left (where matched) for the right half of the picture.
Before layer/gi_reproject.hpp the matched pixels differ as the eyes' GI do; with it they must be the same word.

  python gi_reproject_offline.py <rd_lighting_inputs output dir>
"""
import json, struct, sys
from pathlib import Path
from PIL import Image


def e9(w):
    e = (w >> 27) & 31
    return ((w & 511) + ((w >> 9) & 511) + ((w >> 18) & 511)) * 2.0 ** (e - 24) / 3


def main():
    D = Path(sys.argv[1])
    report = json.loads((D / 'report.json').read_text())
    left, right = report['left_event'], report['right_event']

    def consts(e):
        rows = [json.loads(l) for l in (D / f'bind-{e}' / 'bindings.jsonl').read_text().splitlines()]
        c = [r for r in rows if r['category'] == 'constant' and r['size'] == 2048][0]
        f = struct.unpack('<512f', (D / f'bind-{e}' / c['file']).read_bytes())
        return dict(m2=f[10:12], m5=f[28:44], m6=f[44:60], m7=f[60:63])

    L, R = consts(left), consts(right)
    recs = [json.loads(l) for l in (D / 'texels' / 'texels.jsonl').read_text().splitlines()]
    by = {}
    for r in recs:
        by.setdefault((r['event'], r['format'] if r['format'] != 'R32_UINT' else 'GI'), r)
    N = recs[0]['width']
    load = lambda r, code: struct.unpack('<%d%s' % (N * N, code), (D / 'texels' / r['file']).read_bytes())
    dl, dr = load(by[(left, 'D32')], 'f'), load(by[(right, 'D32')], 'f')
    gl, gr = load(by[(left, 'GI')], 'I'), load(by[(right, 'GI')], 'I')

    def rel(c, x, y, d):
        m = c['m6']
        nx = (x * 2 + 1) * c['m2'][0] - 1
        ny = (y * 2 + 1) * c['m2'][1] - 1
        v = (nx, -ny, d)
        w = m[12] * v[0] + m[13] * v[1] + m[14] * v[2] + m[15]
        return [(m[r * 4] * v[0] + m[r * 4 + 1] * v[1] + m[r * 4 + 2] * v[2] + m[r * 4 + 3]) / w for r in range(3)]

    def project(c, p):
        m = c['m5']
        cl = [m[r * 4] * p[0] + m[r * 4 + 1] * p[1] + m[r * 4 + 2] * p[2] + m[r * 4 + 3] for r in range(4)]
        return cl[0] / cl[3], cl[1] / cl[3], cl[2] / cl[3], cl[3]

    made = list(gr)
    diffs, same, geometry = [], 0, 0
    blocks = {}
    for y in range(N):
        for x in range(N):
            i = y * N + x
            d = dr[i]
            if d <= 0:
                continue
            geometry += 1
            p = rel(R, x, y, d)
            w = [p[k] + R['m7'][k] for k in range(3)]
            nx, ny, nz, cw = project(L, w)
            lx, ly = int(round((nx + 1) / (2 * L['m2'][0]) - .5)), int(round((-ny + 1) / (2 * L['m2'][1]) - .5))
            if not (0 <= lx < N and 0 <= ly < N):
                continue
            d2 = dl[ly * N + lx]
            if d2 <= 0 or abs(d2 - nz) > 0.03 * nz:
                continue
            a, b = e9(gl[ly * N + lx]), e9(gr[i])
            same += gl[ly * N + lx] == gr[i]
            made[i] = gl[ly * N + lx]
            diffs.append(abs(a - b) / max(1e-6, (a + b) / 2))
            k = (x // 8, y // 8)
            s = blocks.setdefault(k, [0.0, 0.0, 0])
            s[0] += a; s[1] += b; s[2] += 1
    diffs.sort()
    bl = sorted(abs(s[0] - s[1]) / max(1e-6, (s[0] + s[1]) / 2) for s in blocks.values() if s[2] >= 48)
    print('matched %d of %d geometry pixels (%.1f %%); identical words %.1f %%' % (len(diffs), geometry, 100 * len(diffs) / max(1, geometry), 100 * same / max(1, len(diffs))))
    print('pixel relative difference: median %.3f, 90th %.3f' % (diffs[len(diffs) // 2], diffs[int(len(diffs) * .9)]))
    print('8x8 blocks (>= 48 matched pixels) %d: median %.3f, 90th %.3f, over 20 %% %.3f' % (len(bl), bl[len(bl) // 2], bl[int(len(bl) * .9)], sum(1 for v in bl if v > .2) / len(bl)))
    m = sorted(e9(v) for v in gl)[int(N * N * .99)] or 1
    im = lambda v: Image.frombytes('L', (N, N), bytes(min(255, int(255 * (e9(x) / m) ** .5)) for x in v))
    box = (N * 5 // 8, N // 4, N, N * 5 // 8)
    w, h = box[2] - box[0], box[3] - box[1]
    s = Image.new('L', (w * 3 + 8, h), 255)
    for k, v in enumerate((gl, gr, made)):
        s.paste(im(v).crop(box), (k * (w + 4), 0))
    s.save(D / 'gi-matched.png')


if __name__ == '__main__':
    main()
