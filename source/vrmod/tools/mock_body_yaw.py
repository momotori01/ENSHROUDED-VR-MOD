"""body_yaw.py <probe.jsonl> <from frame> <to frame> [step]: per frame the body's yaw (stable root orientation), the game
camera's yaw (unshaken orientation) and the direction the stable root moved since the last printed frame (degrees)."""
import sys, json, math
path, f0, f1 = sys.argv[1], int(sys.argv[2]), int(sys.argv[3])
step = int(sys.argv[4]) if len(sys.argv) > 4 else 5
def yaw(q):
    x, y, z, w = q
    return math.degrees(math.atan2(2 * (w * y + x * z), 1 - 2 * (x * x + y * y)))
rows = []
for line in open(path, encoding='utf-8', errors='replace'):
    if '"root_camera_sample"' not in line:
        continue
    r = json.loads(line)
    if f0 <= r['frame'] <= f1:
        rows.append(r)
prev = None
for r in rows:
    if r['frame'] % step and r is not rows[-1]:
        continue
    p = r['stable_q32']
    move = ''
    if prev:
        dx = (p[0] - prev[0]) / 2**32; dz = (p[2] - prev[2]) / 2**32
        d = math.hypot(dx, dz)
        if d > 0.01:
            move = 'moved %.2f m toward %.0f deg' % (d, math.degrees(math.atan2(dx, dz)))
    prev = p
    body = yaw(r['stable_orientation']) if 'stable_orientation' in r else float('nan')
    cam = yaw(r['unshaken_orientation'])
    print('%5d body %7.1f camera %7.1f turn %8.3f  %s' % (r['frame'], body, cam, r.get('gameplay_turn', 0), move))
