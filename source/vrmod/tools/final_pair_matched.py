"""The eyes' finished pictures (after fog, transparents, post and DLSS) compared where they see the same point (2026-10-17).
Depth and lighting constants from a RenderDoc capture of the same still place (rd_lighting_inputs.py output dir); the
pictures are a Mock run's native-xr frame-<n>-eye-<e>.ppm at the output size. Each right output pixel goes to render
coordinates, takes the right depth, becomes a world point and lands in the left picture; luminance is compared in 16x16
blocks of the output (only blocks where at least 3/4 of the pixels matched within 3 % depth).

  python final_pair_matched.py <rd_lighting_inputs dir> <game dir> <frame> [<game dir> <frame> ...]
"""
import json, struct, sys
from pathlib import Path
from PIL import Image


def load_view(D):
    report = json.loads((D / 'report.json').read_text())
    left, right = report['left_event'], report['right_event']

    def consts(e):
        rows = [json.loads(l) for l in (D / f'bind-{e}' / 'bindings.jsonl').read_text().splitlines()]
        c = [r for r in rows if r['category'] == 'constant' and r['size'] == 2048][0]
        f = struct.unpack('<512f', (D / f'bind-{e}' / c['file']).read_bytes())
        return dict(m2=f[10:12], m5=f[28:44], m6=f[44:60], m7=f[60:63])

    recs = [json.loads(l) for l in (D / 'texels' / 'texels.jsonl').read_text().splitlines()]
    depth = {r['event']: r for r in recs if r['format'] == 'D32'}
    N = recs[0]['width']
    dl = struct.unpack('<%df' % (N * N), (D / 'texels' / depth[left]['file']).read_bytes())
    dr = struct.unpack('<%df' % (N * N), (D / 'texels' / depth[right]['file']).read_bytes())
    return consts(left), consts(right), dl, dr, N


def mapping(L, R, dl, dr, N):
    def rel(c, x, y, d):
        m = c['m6']
        v = ((x * 2 + 1) * c['m2'][0] - 1, -((y * 2 + 1) * c['m2'][1] - 1), d)
        w = m[12] * v[0] + m[13] * v[1] + m[14] * v[2] + m[15]
        return [(m[r * 4] * v[0] + m[r * 4 + 1] * v[1] + m[r * 4 + 2] * v[2] + m[r * 4 + 3]) / w for r in range(3)]

    def project(c, p):
        m = c['m5']
        cl = [m[r * 4] * p[0] + m[r * 4 + 1] * p[1] + m[r * 4 + 2] * p[2] + m[r * 4 + 3] for r in range(4)]
        return cl[0] / cl[3], cl[1] / cl[3], cl[2] / cl[3]

    out = {}
    for y in range(N):
        for x in range(N):
            d = dr[y * N + x]
            if d <= 0:
                continue
            p = rel(R, x, y, d)
            nx, ny, nz = project(L, [p[k] + R['m7'][k] for k in range(3)])
            fx, fy = (nx + 1) / (2 * L['m2'][0]) - .5, (-ny + 1) / (2 * L['m2'][1]) - .5
            lx, ly = int(round(fx)), int(round(fy))
            if not (0 <= lx < N and 0 <= ly < N):
                continue
            d2 = dl[ly * N + lx]
            if d2 > 0 and abs(d2 - nz) <= 0.03 * nz:
                out[(x, y)] = (fx, fy)
    return out


def main():
    D = Path(sys.argv[1])
    L, R, dl, dr, N = load_view(D)
    m = mapping(L, R, dl, dr, N)
    runs = sys.argv[2:]
    for k in range(0, len(runs), 2):
        game, frame = Path(runs[k]), runs[k + 1]
        a = Image.open(game / 'native-xr' / f'frame-{frame}-eye-0.ppm').convert('L')
        b = Image.open(game / 'native-xr' / f'frame-{frame}-eye-1.ppm').convert('L')
        W = a.size[0]
        s = W / N
        pa, pb = a.load(), b.load()
        blocks = {}
        for oy in range(0, W, 2):
            for ox in range(0, W, 2):
                rx, ry = int(ox / s), int(oy / s)
                hit = m.get((rx, ry))
                key = (ox // 16, oy // 16)
                t = blocks.setdefault(key, [0, 0, 0, 0])
                t[3] += 1
                if hit is None:
                    continue
                lx, ly = int(round((hit[0] + .5) * s - .5 + (ox - (rx + .5) * s + .5))), int(round((hit[1] + .5) * s - .5 + (oy - (ry + .5) * s + .5)))
                if not (0 <= lx < W and 0 <= ly < W):
                    continue
                t[0] += pa[lx, ly]; t[1] += pb[ox, oy]; t[2] += 1
        rel = [(t[0] - t[1]) / max(1.0, (t[0] + t[1]) / 2) for t in blocks.values() if t[2] >= .75 * t[3] and t[2] > 0]
        v = sorted(abs(x) for x in rel)
        print('%s %s: blocks %d, median %.3f, 90th %.3f, over 10 %% %.3f, over 20 %% %.3f, mean signed (left - right) %+.3f' % (
            game.name, frame, len(v), v[len(v) // 2], v[int(len(v) * .9)], sum(1 for x in v if x > .1) / len(v), sum(1 for x in v if x > .2) / len(v), sum(rel) / len(rel)))


if __name__ == '__main__':
    main()
