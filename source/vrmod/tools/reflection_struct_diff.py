#!/usr/bin/env python3
"""Where the fields of the game's reflected structures went in a new build (read-only; 2026-10-16, the 1.0 update).

The game describes its structures (ECS components, settings) in .rdata: a field record is 0x30 bytes, {name pointer,
name length, type pointer, offset, ..., ...}, the records of one structure follow each other. This scans both
executables for such runs, names each run by the typedef that points at it (its qualified name, kfc-parser's layout:
name at +32/+40) when one is found, matches the runs of the two builds by their field names and prints the fields whose
offset changed.

    reflection_struct_diff.py --old <old.exe> --new <new.exe> [--struct ClientPlayerInputData] [--field fromClientInputMode]
        [--all] [--json out.json]
"""
import argparse, json, pathlib, struct, sys
sys.path.insert(0, str(pathlib.Path(__file__).parent / 'python'))
import pefile  # noqa: E402


class Image:
    def __init__(self, path):
        self.raw = pathlib.Path(path).read_bytes()
        self.pe = pefile.PE(data=self.raw, fast_load=True)
        self.base = self.pe.OPTIONAL_HEADER.ImageBase
        self.size = self.pe.OPTIONAL_HEADER.SizeOfImage
        self.sections = [(s.VirtualAddress, s.VirtualAddress + s.SizeOfRawData, s.PointerToRawData, s.Name.rstrip(b'\0').decode()) for s in self.pe.sections]

    def off(self, rva):
        for lo, hi, raw, _ in self.sections:
            if lo <= rva < hi:
                return raw + rva - lo
        return None

    def string(self, va, n):
        if not (self.base < va < self.base + self.size) or not 0 < n < 128:
            return None
        o = self.off(va - self.base)
        if o is None:
            return None
        b = self.raw[o:o + n]
        if len(b) != n or not all(32 <= c < 127 for c in b) or (o + n < len(self.raw) and self.raw[o + n] != 0):
            return None
        return b.decode()

    def data_sections(self):
        for lo, hi, raw, name in self.sections:
            if name in ('.rdata', '.data'):
                yield lo, hi, raw

    def runs(self):
        """Contiguous field-record runs: [(start rva, [(name, offset, type va)])]."""
        out = []
        for lo, hi, raw in self.data_sections():
            data = self.raw[raw:raw + (hi - lo)]
            pos = 0
            n = len(data) - 0x30
            while pos < n:
                rec = self._record(data, pos)
                if not rec:
                    pos += 8
                    continue
                start = pos
                fields = []
                while pos < n:
                    rec = self._record(data, pos)
                    if not rec:
                        break
                    fields.append(rec)
                    pos += 0x30
                if len(fields) >= 2:
                    out.append((lo + start, fields))
                else:
                    pos = start + 8
        return out

    def _record(self, data, pos):
        name_va, length, type_va, offset = struct.unpack_from('<QQQQ', data, pos)
        if not (0 < length < 96) or offset >= 0x200000 or not (self.base < type_va < self.base + self.size):
            return None
        name = self.string(name_va, length)
        if not name or not (name[0].isalpha() or name[0] == '_'):
            return None
        return (name, offset, type_va)

    def run_names(self, runs):
        """The typedef that points at a run: its qualified name (+32 pointer, +40 length)."""
        starts = {self.base + rva: rva for rva, _ in runs}
        names = {}
        for lo, hi, raw in self.data_sections():
            data = self.raw[raw:raw + (hi - lo)]
            for pos in range(0, len(data) - 8, 8):
                v = struct.unpack_from('<Q', data, pos)[0]
                rva = starts.get(v)
                if rva is None or rva in names:
                    continue
                for back in range(8, 0x100, 8):
                    p = pos - back
                    if p < 0:
                        break
                    nva, nlen = struct.unpack_from('<QQ', data, p + 32) if p + 48 <= len(data) else (0, 0)
                    s = self.string(nva, nlen)
                    if s and ('::' in s or s[:1].isupper()):
                        names[rva] = s
                        break
        return names


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--old', required=True); ap.add_argument('--new', required=True)
    ap.add_argument('--struct', action='append', default=[]); ap.add_argument('--field', action='append', default=[])
    ap.add_argument('--all', action='store_true', help='print unchanged fields too'); ap.add_argument('--json')
    a = ap.parse_args()
    old, new = Image(a.old), Image(a.new)
    oruns, nruns = old.runs(), new.runs()
    onames, nnames = old.run_names(oruns), new.run_names(nruns)
    print(f'runs: old {len(oruns)} new {len(nruns)}', file=sys.stderr)
    index = {}
    for rva, fields in nruns:
        for f in fields:
            index.setdefault(f[0], []).append(rva)
    nmap = {rva: fields for rva, fields in nruns}
    result = []
    for rva, fields in oruns:
        names = {f[0] for f in fields}
        votes = {}
        for nm in names:
            for r in index.get(nm, ()):
                votes[r] = votes.get(r, 0) + 1
        if not votes:
            continue
        best = max(votes, key=lambda r: (votes[r] / len(names | {f[0] for f in nmap[r]}), votes[r]))
        score = votes[best] / len(names | {f[0] for f in nmap[best]})
        if score < 0.5:
            continue
        label = onames.get(rva) or nnames.get(best) or ''
        if a.struct and not any(s in label for s in a.struct):
            if not a.field or not (names & set(a.field)):
                continue
        newf = {f[0]: f[1] for f in nmap[best]}
        rows = []
        for name, off, _ in fields:
            if a.field and a.struct == [] and name not in a.field:
                continue
            nof = newf.get(name)
            if a.all or nof != off:
                rows.append((name, off, nof))
        added = [n for n in newf if n not in names]
        if rows or (a.all and added):
            result.append(dict(struct=label, old=hex(rva), new=hex(best), score=round(score, 2),
                               fields=[dict(name=n, old=hex(o), new=hex(x) if x is not None else None) for n, o, x in rows], added=added))
    for r in result:
        print(f"== {r['struct'] or '?'} (old {r['old']} new {r['new']}, match {r['score']})")
        for f in r['fields']:
            print(f"   {f['name']:<40} {f['old']:>8} -> {f['new'] or 'gone'}")
        if r['added']:
            print(f"   added: {', '.join(r['added'][:20])}")
    if a.json:
        pathlib.Path(a.json).write_text(json.dumps(result, indent=1))


if __name__ == '__main__':
    main()
