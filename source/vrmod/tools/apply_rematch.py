#!/usr/bin/env python3
"""Write the addresses of a new game build into the hooks (2026-10-16, the 1.0 update), from the report of
match_functions.py: every 0x<old> token of the hooks' code (not the comments) becomes 0x<new>, and a byte pattern
declared for a function (a string of hex bytes within six lines before or four lines after the address) that matches
the old build's bytes at the old address is made anew from the new build at the new address, of the same length or
longer until it is unique in the new build.

    apply_rematch.py --report <match.json> --old <old.exe> --new <new.exe> [--hooks vrmod/hooks] [--write]

Without --write nothing is changed: the list says what would be, and what needs a look (unmatched sites, doubtful
alignments, weak similarities, patterns that are not unique). Addresses below 0x10000 are left alone (those of the
hooks are structure offsets, not code). A pattern is made anew only for a match that is sure (the same shape, the
same rare strings, or the call graph with a similarity of 0.5 or more; a tiny function with two or more callers'
votes). A weak match keeps its old pattern: the hook's own check at install then fails and the hook stays out,
instead of hooking a stranger.
"""
import argparse, json, pathlib, re, sys

sys.path.insert(0, 'vrmod/tools')
from match_functions import Image  # noqa: E402

BOM = b'\xef\xbb\xbf'


def parse_pattern(text):
    out, mask = [], []
    for tok in text.split():
        if tok in ('?', '??'):
            out.append(0); mask.append(False)
        else:
            out.append(int(tok, 16)); mask.append(True)
    return bytes(out), mask


def pattern_matches(img, rva, text):
    b, mask = parse_pattern(text)
    data = img.read(rva, len(b))
    return len(data) == len(b) and all((not m) or data[i] == b[i] for i, m in enumerate(mask))


def code_part(line):
    """The part of a line before a // comment (a // inside a string is not a comment)."""
    in_str = False
    i = 0
    while i < len(line) - 1:
        c = line[i]
        if c == '\\' and in_str:
            i += 2
            continue
        if c == '"':
            in_str = not in_str
        elif not in_str and line[i:i + 2] == '//':
            return line[:i], line[i:]
        i += 1
    return line, ''


def strong(r):
    if r.get('method') in ('strict', 'medium', 'loose', 'strings'):
        return True
    note = r.get('note', '') or ''
    m = re.search(r'ngram ([0-9.]+)', note)
    sim = float(m.group(1)) if m else 0.0
    if r.get('method') in ('callers', 'graph', 'strings+ngram') and sim >= 0.5:
        return True
    if r.get('method') == 'callers' and (r.get('old_fn_insns') or 99) <= 12 and ' of ' in note:
        try:
            return int(note.split(' of ')[0].split()[-1]) >= 2
        except ValueError:
            return False
    return False


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument('--report', required=True, type=pathlib.Path)
    p.add_argument('--old', required=True, type=pathlib.Path)
    p.add_argument('--new', required=True, type=pathlib.Path)
    p.add_argument('--hooks', type=pathlib.Path, default=pathlib.Path('vrmod/hooks'))
    p.add_argument('--write', action='store_true')
    p.add_argument('--trust', action='append', default=[], help='an old address whose weak match was checked by hand: its pattern is made anew')
    a = p.parse_args()
    rows = json.loads(a.report.read_text(encoding='utf-8'))['rows']
    trusted = {int(t, 16) for t in a.trust}
    for r in rows:
        if int(r['old'], 16) in trusted:
            r['method'] = 'strict'   # counts as sure
            r['note'] = (r.get('note') or '') + ' (trusted by hand)'
    old, new = Image(a.old), Image(a.new)
    mapping, attention = {}, []
    for r in rows:
        o = int(r['old'], 16)
        if o < 0x10000:
            continue
        if not r.get('new'):
            attention.append(f"UNMATCHED {r['old']} {r['name']} ({r['file']}:{r['line']}, {r['kind']}) {r.get('method', '')}")
            continue
        n = int(r['new'], 16)
        note = r.get('note', '') or ''
        align = r.get('align', '') or ''
        weak = not strong(r) and r['kind'] in ('fn', 'leaf')
        doubt = 'DOUBT' in align or align.startswith(('deleted', 'replaced (different', 'outside'))
        if weak or doubt:
            attention.append(f"CHECK {r['old']}->{r['new']} {r['name']} ({r['file']}:{r['line']}, {r['kind']}) {r.get('method')} {note} {align}")
        if o in mapping and mapping[o] != n:
            attention.append(f"CONFLICT {r['old']} -> {hex(mapping[o])} and {r['new']}")
        mapping[o] = n
    rx = re.compile(r'0x([0-9A-Fa-f]{5,7})\b')
    pat_rx = re.compile(r'"((?:[0-9A-Fa-f]{2}|\?\??)(?: (?:[0-9A-Fa-f]{2}|\?\??))+)"')
    changed_files, replacements, pattern_changes = 0, 0, 0
    fn_new = {int(r['old'], 16): int(r['new'], 16) for r in rows if r.get('new') and r['kind'] in ('fn', 'leaf')}
    fn_strong = {int(r['old'], 16): strong(r) for r in rows if r.get('new') and r['kind'] in ('fn', 'leaf')}
    for f in sorted(list(a.hooks.glob('*.hpp')) + list(a.hooks.glob('*.cpp'))):
        raw = f.read_bytes()
        bom = raw.startswith(BOM)
        text = raw.decode('utf-8-sig')
        nl = '\r\n' if '\r\n' in text else '\n'
        lines = text.split(nl)
        codes, comments = [], []
        for line in lines:
            c, m = code_part(line)
            codes.append(c)
            comments.append(m)
        file_changes = 0
        fn_lines = {}   # line index -> [old fn rva...]
        for i, code in enumerate(codes):
            if not code.strip():
                continue
            found = []

            def sub(m):
                v = int(m.group(1), 16)
                if v in mapping:
                    if v in fn_new:
                        found.append(v)
                    return '0x%x' % mapping[v]
                return m.group(0)
            codes[i], n = rx.subn(sub, code)
            replacements += n
            file_changes += n
            if found:
                fn_lines[i] = found
        done = set()
        for i, olds in fn_lines.items():
            for o in olds:
                # the same line first (a table of rows with identical prologue patterns: each row's own string)
                for j in [i] + [k for k in range(max(0, i - 6), min(len(codes), i + 5)) if k != i]:
                    for pm in pat_rx.finditer(codes[j]):
                        if (j, pm.group(1)) in done or not pattern_matches(old, o, pm.group(1)):
                            continue
                        done.add((j, pm.group(1)))
                        if not fn_strong.get(o):
                            attention.append(f"PATTERN kept (weak match, the hook's check will fail) for {hex(o)}->{hex(fn_new[o])} in {f.name}:{j + 1}")
                            continue
                        length = len(pm.group(1).split())
                        best = None
                        for L in (length, length + 4, length + 8, length + 16, length + 32):
                            textp, hits = new.pattern(fn_new[o], L)
                            if hits == 1:
                                best = (textp, L, hits)
                                break
                            best = best or (textp, L, hits)
                        textp, L, hits = best
                        if hits != 1:
                            attention.append(f"PATTERN not unique ({hits} hits) for {hex(fn_new[o])} in {f.name}:{j + 1}")
                        lower = pm.group(1) != pm.group(1).upper()
                        textp = textp.lower() if lower else textp
                        codes[j] = codes[j].replace('"' + pm.group(1) + '"', '"' + textp + '"', 1)
                        pattern_changes += 1
                        file_changes += 1
        if file_changes:
            changed_files += 1
            print(f'{f.name}: {file_changes} changes')
            if a.write:
                f.write_bytes((BOM if bom else b'') + nl.join(c + m for c, m in zip(codes, comments)).encode('utf-8'))
    print(f'{"written" if a.write else "would write"}: {changed_files} files, {replacements} addresses, {pattern_changes} patterns')
    print('\nATTENTION')
    for s in attention:
        print(' ', s)
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
