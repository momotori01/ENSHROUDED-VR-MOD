"""cross_eye.py <usage dir> <left start> <right start> <final start>: textures written only inside one eye's event range and
read inside the other eye's range (RenderDoc ResourceUsage numbers)."""
import sys, json, collections
d = sys.argv[1]; L, R, F = int(sys.argv[2]), int(sys.argv[3]), int(sys.argv[4])
names = {}
for l in open(d + '/textures.jsonl'):
    o = json.loads(l); names[o['index']] = (o['name'], o['format'], o['width'], o['height'], o['array'])
writes = set(range(22, 31)) | {32, 33, 35, 40, 41, 43}
reads = set(range(13, 22)) | set(range(22, 31)) | {31, 39, 42}
W = collections.defaultdict(set); Rd = collections.defaultdict(list)
def where(e):
    return 'pre' if e < L else 'L' if e < R else 'R' if e < F else 'final'
for l in open(d + '/usage.jsonl'):
    o = json.loads(l); t, e, u = o['texture'], o['event'], o['usage']
    if u in writes: W[t].add(where(e))
    if u in reads and u not in writes: Rd[t].append((e, u, where(e)))
for t in sorted(W):
    w = W[t]
    for eye, other in (('L', 'R'), ('R', 'L')):
        if w == {eye} or w == {eye, 'pre'} and False:
            cross = [(e, u) for e, u, z in Rd[t] if z == other]
            if cross:
                print(t, names.get(t), 'written', sorted(w), 'read in', other, len(cross), cross[:4])
# textures written in both eyes but read only after the other eye's write (stale cross-eye reads are not caught here)
