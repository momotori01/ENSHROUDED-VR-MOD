"""Light shafts behind the trees in one eye only (headset reports 7/8 of 2026-10-17; Mock ep showed a haze beam behind
the left tree in the left eye only, `DeleteMe/20261017/ep-4804.jpg`). For the pictures of a Mock run at the tree of the
user's save: the sky and haze between the trees, per eye, in vertical strips (the right eye's strip moved by the
parallax search), and a side-by-side crop. A beam in one eye only shows as a strip much brighter in that eye.

  python shaft_pair_check.py <game dir> [frame] [out.jpg]
"""
import glob, os, sys
from PIL import Image, ImageStat, ImageEnhance


def main():
    game = sys.argv[1]
    d = os.path.join(game, 'native-xr')
    frames = sorted(int(os.path.basename(p).split('-')[1]) for p in glob.glob(os.path.join(d, 'frame-*-eye-0.ppm')))
    frame = int(sys.argv[2]) if len(sys.argv) > 2 else [f for f in frames if f > 300][0]
    a = Image.open(os.path.join(d, f'frame-{frame}-eye-0.ppm')).convert('RGB')
    b = Image.open(os.path.join(d, f'frame-{frame}-eye-1.ppm')).convert('RGB')
    W, H = a.size
    print('frame', frame, 'size', a.size)
    worst = []
    for x in range(0, W - 40, 40):
        box = (x, 0, x + 40, H // 2)
        la = sum(ImageStat.Stat(a.crop(box)).mean) / 3
        best = None
        for dx in range(-24, 4, 4):
            if x + dx < 0:
                continue
            lb = sum(ImageStat.Stat(b.crop((x + dx, 0, x + dx + 40, H // 2))).mean) / 3
            if best is None or abs(la - lb) < abs(la - best):
                best = lb
        worst.append((la - best, x, la, best))
    worst.sort(key=lambda t: -abs(t[0]))
    for d_, x, la, lb in worst[:6]:
        print('strip x %4d..%4d  left %.1f right %.1f  (left - right %+.1f)' % (x, x + 40, la, lb, d_))
    out = sys.argv[3] if len(sys.argv) > 3 else None
    if out:
        c = Image.new('RGB', (W * 2 + 8, H // 2), (255, 255, 255))
        c.paste(a.crop((0, 0, W, H // 2)), (0, 0)); c.paste(b.crop((0, 0, W, H // 2)), (W + 8, 0))
        ImageEnhance.Brightness(c.resize((c.size[0] // 2, c.size[1] // 2))).enhance(1.3).save(out, quality=88)


if __name__ == '__main__':
    main()
