"""shared_history.py <usage dir> <left start> <right start> <final start>: textures that both eyes touch: for each, the
first access of each eye's range and whether an eye reads it before it writes it (the content then comes from the other
eye or from the frame before)."""
import sys, json, collections
d = sys.argv[1]; L, R, F = int(sys.argv[2]), int(sys.argv[3]), int(sys.argv[4])
names = {}
for l in open(d + '/textures.jsonl'):
    o = json.loads(l); names[o['index']] = (o['name'], o['format'], o['width'], o['height'], o['array'])
writes = set(range(22, 31)) | {32, 33, 35, 40, 41, 43}
reads = set(range(13, 22)) | {31, 39, 42}
acc = collections.defaultdict(list)
for l in open(d + '/usage.jsonl'):
    o = json.loads(l)
    if o['usage'] in (44, 36): continue
    acc[o['texture']].append((o['event'], o['usage']))
def zone(e): return 'L' if L <= e < R else 'R' if R <= e < F else None
for t, v in sorted(acc.items()):
    v.sort(); per = {'L': [], 'R': []}
    for e, u in v:
        z = zone(e)
        if z: per[z].append((e, u))
    if not per['L'] or not per['R']: continue
    out = []
    for z in ('L', 'R'):
        first_w = next((e for e, u in per[z] if u in writes), None)
        first_r = next((e for e, u in per[z] if u in reads or u in range(22, 31)), None)
        rb = first_r is not None and (first_w is None or first_r < first_w)
        out.append((z, first_r, first_w, rb))
    if any(x[3] for x in out) and any(x[2] is not None for x in out):
        print(t, names.get(t), out)
