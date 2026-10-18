"""The eyes' lit colour (the deferred lighting's output) compared where they see the same point (2026-10-17).
After rd_lighting_inputs.py (constants, depth) and a texels export of the lighting's output at both dispatches
(binding u34, RGBA16F): for each right pixel the left pixel of the same point (as gi_reproject_offline.py), then the
luminance per 8x8 block. Prints the share of blocks more than 10 / 20 % apart and writes <lit dir>/lit-blocks.png
(red: the left eye brighter, blue: the right).

  python rd_lit_matched.py <rd_lighting_inputs dir> <lit texels dir>
"""
import json, struct, sys
from pathlib import Path
from PIL import Image


def main():
    D, T = Path(sys.argv[1]), Path(sys.argv[2])
    report = json.loads((D / 'report.json').read_text())
    left, right = report['left_event'], report['right_event']

    def consts(e):
        rows = [json.loads(l) for l in (D / f'bind-{e}' / 'bindings.jsonl').read_text().splitlines()]
        c = [r for r in rows if r['category'] == 'constant' and r['size'] == 2048][0]
        f = struct.unpack('<512f', (D / f'bind-{e}' / c['file']).read_bytes())
        return dict(m2=f[10:12], m5=f[28:44], m6=f[44:60], m7=f[60:63])

    L, R = consts(left), consts(right)
    recs = [json.loads(l) for l in (D / 'texels' / 'texels.jsonl').read_text().splitlines()]
    depth = {r['event']: r for r in recs if r['format'] == 'D32'}
    N = recs[0]['width']
    dl = struct.unpack('<%df' % (N * N), (D / 'texels' / depth[left]['file']).read_bytes())
    dr = struct.unpack('<%df' % (N * N), (D / 'texels' / depth[right]['file']).read_bytes())
    lit = {r['event']: r for r in (json.loads(l) for l in (T / 'texels.jsonl').read_text().splitlines())}

    def luminance(r):
        v = struct.unpack('<%de' % (N * N * 4), (T / r['file']).read_bytes())
        return [.2126 * v[i] + .7152 * v[i + 1] + .0722 * v[i + 2] for i in range(0, len(v), 4)]

    yl, yr = luminance(lit[left]), luminance(lit[right])

    def rel(c, x, y, d):
        m = c['m6']
        v = ((x * 2 + 1) * c['m2'][0] - 1, -((y * 2 + 1) * c['m2'][1] - 1), d)
        w = m[12] * v[0] + m[13] * v[1] + m[14] * v[2] + m[15]
        return [(m[r * 4] * v[0] + m[r * 4 + 1] * v[1] + m[r * 4 + 2] * v[2] + m[r * 4 + 3]) / w for r in range(3)]

    def project(c, p):
        m = c['m5']
        cl = [m[r * 4] * p[0] + m[r * 4 + 1] * p[1] + m[r * 4 + 2] * p[2] + m[r * 4 + 3] for r in range(4)]
        return cl[0] / cl[3], cl[1] / cl[3], cl[2] / cl[3]

    blocks = {}
    for y in range(N):
        for x in range(N):
            i = y * N + x
            d = dr[i]
            if d <= 0:
                continue
            p = rel(R, x, y, d)
            nx, ny, nz = project(L, [p[k] + R['m7'][k] for k in range(3)])
            lx, ly = int(round((nx + 1) / (2 * L['m2'][0]) - .5)), int(round((-ny + 1) / (2 * L['m2'][1]) - .5))
            if not (0 <= lx < N and 0 <= ly < N):
                continue
            d2 = dl[ly * N + lx]
            if d2 <= 0 or abs(d2 - nz) > 0.03 * nz:
                continue
            s = blocks.setdefault((x // 8, y // 8), [0.0, 0.0, 0])
            s[0] += yl[ly * N + lx]; s[1] += yr[i]; s[2] += 1
    rel_d = {k: (s[0] - s[1]) / max(1e-6, (s[0] + s[1]) / 2) for k, s in blocks.items() if s[2] >= 48}
    v = sorted(abs(x) for x in rel_d.values())
    print('blocks %d: median %.3f, 90th %.3f, over 10 %% %.3f, over 20 %% %.3f, mean signed (left - right) %+.3f' % (
        len(v), v[len(v) // 2], v[int(len(v) * .9)], sum(1 for x in v if x > .1) / len(v), sum(1 for x in v if x > .2) / len(v), sum(rel_d.values()) / len(v)))
    M = N // 8
    img = Image.new('RGB', (M, M))
    px = img.load()
    for (bx, by), r in rel_d.items():
        if bx < M and by < M:
            px[bx, by] = (min(255, int(max(0, r) * 600)), 40, min(255, int(max(0, -r) * 600)))
    img.resize((M * 6, M * 6), Image.NEAREST).save(T / 'lit-blocks.png')


if __name__ == '__main__':
    main()
