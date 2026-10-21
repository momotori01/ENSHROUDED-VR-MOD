"""Left/right brightness of a pair of eye pictures alone (a VR bug report has no depth or matrices): dense matching
by zero-mean normalised cross-correlation (blind to a brightness difference, so it does not prefer either answer)
over a 9 x 9 window at half size (--scale N: 1/N), about the whole picture's shift between the eyes (phase
correlation: a headset's eyes see different parts of the scene, about -15 % of the width on the 10-21 reports)
horizontally -8 %..+3 % of the width and vertically -2..+2; kept are pixels with
texture, a correlation of at least 0.9 and a left-right check within one pixel. Brightness is compared in linear light
(sRGB decoded) on the kept pixels: ratio of the means, mean signed (L - R) / mean, share of 32 x 32 blocks (half size)
more than 5 / 10 % apart. Checked 2026-10-21 against RenderDoc pairs whose eyes agree within 0.4 % / 1.0 % at the same
point of the world (exact reprojection, docs/analysis/house_enter_lr_20261021.md): it gave 0.0 / 0.2 % and 1 % of the
blocks over 10 % (the block search dark_lr.py gave 18.5 % and +4.2 % on the same pictures); with a tenth of one eye
made 10 % darker the blocks over 10 % went to 15 %.
usage: stereo_lr.py <left.ppm|jpg> <right.ppm|jpg> [--map out.jpg] [--scale 2]
       stereo_lr.py <pair.jpg> [--map out.jpg] [--scale 2]      (a VR bug report's bug-report/screenshots/pair-*.jpg: left | 8 px | right)
Run with a Python that has numpy and Pillow (the Codex runtime's), -I for a report's pictures.
Map: the left eye grey, blocks where the left is brighter red, darker blue (full colour at 10 %)."""
import sys
import numpy as np
def rgb(p):
    if p.lower().endswith('.ppm'):
        b = open(p, 'rb').read(); parts = b.split(b'\n', 3); w, h = map(int, parts[1].split())
        return np.frombuffer(parts[3][:w * h * 3], np.uint8).reshape(h, w, 3)
    from PIL import Image
    return np.asarray(Image.open(p).convert('RGB'))
def linear_grey(a, n):
    a = a.astype(np.float64) / 255
    lin = np.where(a <= .04045, a / 12.92, ((a + .055) / 1.055) ** 2.4)
    y = lin[..., 0] * .2126 + lin[..., 1] * .7152 + lin[..., 2] * .0722
    y = y[:y.shape[0] // n * n, :y.shape[1] // n * n]
    return y.reshape(y.shape[0] // n, n, y.shape[1] // n, n).mean(axis=(1, 3))
def whole_shift(A, B):
    # dx, dy with B[y + dy, x + dx] ~ A[y, x] for most of the picture (phase correlation of log luminance, Hann window)
    a = np.log(A + 1e-3); b = np.log(B + 1e-3); a -= a.mean(); b -= b.mean()
    win = np.outer(np.hanning(a.shape[0]), np.hanning(a.shape[1]))
    F = np.conj(np.fft.fft2(a * win)) * np.fft.fft2(b * win); F /= np.abs(F) + 1e-9
    r = np.fft.ifft2(F).real; y, x = np.unravel_index(np.argmax(r), r.shape)
    if y > r.shape[0] // 2: y -= r.shape[0]
    if x > r.shape[1] // 2: x -= r.shape[1]
    return int(x), int(y)
def box(a, r):
    c = np.cumsum(np.cumsum(np.pad(a, ((r + 1, r), (r + 1, r)), mode='edge'), 0), 1)
    k = 2 * r + 1; return (c[k:, k:] - c[:-k, k:] - c[k:, :-k] + c[:-k, :-k]) / (k * k)
def shifted(a, dx, dy):
    out = np.full_like(a, np.nan); h, w = a.shape
    ys, yd = (slice(max(dy, 0), h + min(dy, 0)), slice(max(-dy, 0), h + min(-dy, 0)))
    xs, xd = (slice(max(dx, 0), w + min(dx, 0)), slice(max(-dx, 0), w + min(-dx, 0)))
    out[yd, xd] = a[ys, xs]; return out
def match(A, B, shifts, r=4):
    # for each pixel of A, the shift (dx, dy) whose window of B correlates best (ZNCC on log luminance)
    a = np.log(A + 1e-3); b = np.log(B + 1e-3)
    ma = box(a, r); va = box(a * a, r) - ma * ma
    best = np.full(A.shape, -2.0); bdx = np.zeros(A.shape, int); bdy = np.zeros(A.shape, int)
    for dx, dy in shifts:
        bs = shifted(b, dx, dy); valid = ~np.isnan(bs); bs = np.where(valid, bs, 0)
        mb = box(bs, r); vb = box(bs * bs, r) - mb * mb; cov = box(a * bs, r) - ma * mb
        z = cov / np.sqrt(np.maximum(va * vb, 1e-12)); z[~valid] = -2
        better = z > best; best[better] = z[better]; bdx[better] = dx; bdy[better] = dy
    return best, bdx, bdy, va
args = sys.argv[1:]; out_map = None; scale = 2
if '--map' in args: i = args.index('--map'); out_map = args[i + 1]; del args[i:i + 2]
if '--scale' in args: i = args.index('--scale'); scale = int(args[i + 1]); del args[i:i + 2]
if len(args) == 1:
    both = rgb(args[0]); half = (both.shape[1] - 8) // 2; Lrgb, Rrgb = both[:, :half], both[:, half + 8:half + 8 + half]
else: Lrgb, Rrgb = rgb(args[0]), rgb(args[1])
L, R = linear_grey(Lrgb, scale), linear_grey(Rrgb, scale); hh, ww = min(L.shape[0], R.shape[0]), min(L.shape[1], R.shape[1]); L, R = L[:hh, :ww], R[:hh, :ww]
h, w = L.shape; margin = .1
gx, gy = whole_shift(L, R); dmin, dmax = gx - int(w * .08), gx + int(w * .03) + 1
zl, dxl, dyl, varl = match(L, R, [(dx, dy) for dx in range(dmin, dmax + 1) for dy in range(gy - 2, gy + 3)])
zr, dxr, dyr, _ = match(R, L, [(-dx, -dy) for dx in range(dmin, dmax + 1) for dy in range(gy - 2, gy + 3)])
ys, xs = np.mgrid[0:h, 0:w]; tx = np.clip(xs + dxl, 0, w - 1); ty = np.clip(ys + dyl, 0, h - 1)
consistent = (np.abs(dxr[ty, tx] + dxl) <= 1) & (np.abs(dyr[ty, tx] + dyl) <= 1)
m = int(min(h, w) * margin)
keep = (zl >= .9) & consistent & (varl > 1e-3); keep[:m] = keep[-m:] = False; keep[:, :m] = keep[:, -m:] = False
a = L[keep]; b = R[ty[keep], tx[keep]]
print(f'size {w}x{h} (1/{scale}) whole shift {gx:+d},{gy:+d} kept {keep.mean():.3f} of pixels ({keep.sum()}), shift median {np.median(dxl[keep]):+.0f}')
print(f'L/R ratio of means {a.mean() / b.mean():.4f}, mean signed {np.mean((a - b) / np.maximum((a + b) / 2, 1e-6)):+.4f}')
B = max(8, 64 // scale); rel = []; marks = []
for y in range(0, h - B + 1, B):
    for x in range(0, w - B + 1, B):
        k = keep[y:y + B, x:x + B]
        if k.sum() < B * B * .2: continue
        sa = L[y:y + B, x:x + B][k].mean(); sb = R[ty[y:y + B, x:x + B][k], tx[y:y + B, x:x + B][k]].mean(); v = (sa - sb) / ((sa + sb) / 2); rel.append(v); marks.append((x, y, v))
sig = np.array(rel); rel = np.abs(sig); print(f'blocks {len(rel)} over5 {(rel > .05).mean():.3f} over10 {(rel > .1).mean():.3f}')
if out_map:
    from PIL import Image
    g = np.clip(L ** (1 / 2.2) * 255, 0, 255); img = np.repeat(g[..., None], 3, axis=2)
    for x, y, v in marks:
        c = np.array([255, 0, 0] if v > 0 else [0, 80, 255], np.float64); k = min(abs(v) / .1, 1) * .7
        img[y:y + B, x:x + B] = img[y:y + B, x:x + B] * (1 - k) + c * k
    Image.fromarray(img.astype(np.uint8)).save(out_map, quality=90); print('map', out_map)
