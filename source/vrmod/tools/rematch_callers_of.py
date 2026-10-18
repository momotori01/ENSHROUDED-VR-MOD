# read-only: the call sites of a function in both builds (old fn rva, new fn rva), with the caller functions' sizes
# and the similarity of each old caller to each new caller (loose 4-grams), from match_functions' cached features
import sys, pathlib
sys.path.insert(0, 'vrmod/tools')
from match_functions import Image, Matcher
old = Image('vrmod/output/exe-backups/af2f5a12-20260630/enshrouded.exe'); new = Image('enshrouded.exe')
cache = pathlib.Path('vrmod/output/hook-rematch-20261016')
old.analyse(cache); new.analyse(cache)
m = Matcher(old, new)
for pair in sys.argv[1:]:
    o, n = (int(x, 16) for x in pair.split('='))
    oc = sorted(old.callers.get(o, ())); nc = sorted(new.callers.get(n, ()))
    print(f'== old {o:x}: {len(oc)} callers; new {n:x}: {len(nc)} callers')
    for c in oc:
        fe = old.features[c]; sites = [fe['addrs'][k] + fe['sizes'][k] for k, t in fe['calls'] if t == o]
        best = sorted(((m.similarity(c, d), d) for d in nc if d in new.features), reverse=True)[:3]
        print(f'  old caller {c:x} ({fe["n"]} insns) returns {[hex(s) for s in sites]} -> best new: ' + ', '.join(f'{d:x} sim {s:.2f}' for s, d in best))
    for d in nc:
        fe = new.features.get(d)
        if fe:
            sites = [fe['addrs'][k] + fe['sizes'][k] for k, t in fe['calls'] if t == n]
            print(f'  new caller {d:x} ({fe["n"]} insns) returns {[hex(s) for s in sites]}')
