# read-only: which hooks installed and which failed in a session (view-resource-probe.jsonl): events named *_enabled,
# *_installed, *_failure, *_unavailable, *_disabled, *_removed and anything with "signature"/"unsupported" in it
import sys, json, re, collections, pathlib
path = pathlib.Path(sys.argv[1])
ok, bad, other = collections.OrderedDict(), collections.OrderedDict(), collections.Counter()
n = 0
for line in path.read_text(encoding='utf-8', errors='replace').splitlines():
    n += 1
    try:
        e = json.loads(line)
    except Exception:
        continue
    name = e.get('event', '')
    text = line
    if re.search(r'_(enabled|installed|ready|hooked)$', name):
        ok[name] = text[:160]
    elif re.search(r'_(failure|failed|unavailable|disabled|removed|skipped|changed)$', name) or 'signature' in text or 'unsupported' in text or 'exception' in name:
        bad[name] = text[:300]
    other[name] += 1
print(f'{n} lines')
print(f'\n== installed ({len(ok)})')
for k in ok: print('  ', k)
print(f'\n== failed / missing ({len(bad)})')
for k, v in bad.items(): print('  ', v)
print('\n== most frequent events')
for k, c in other.most_common(25): print(f'  {c:7d} {k}')
