"""Read-only save comparison against a managed launch's verified backup.

Game/Steam may save normally during world tests. Changed files are recorded,
never repaired, removed or silently treated as byte-identical.
"""
import argparse
import hashlib
import json
from pathlib import Path


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('launch', type=Path)
    a = p.parse_args()
    record = json.loads((a.launch/'launch.json').read_text(encoding='utf-8-sig'))
    backup = record['save_backup']
    if not backup['verified']:
        raise ValueError('verified launch backup required')
    root = Path(backup['path']).resolve(strict=True)
    source = Path(backup['source']).resolve(strict=True)
    manifest = json.loads((root/'manifest.json').read_text(encoding='utf-8-sig'))
    changes, missing, corrupted = [], [], []
    originals = set()
    for e in manifest:
        relative = Path(e['relative'])
        saved = (root/'remote'/relative).resolve(strict=True)
        current = (source/relative).resolve()
        if not saved.is_relative_to(root/'remote') or not current.is_relative_to(source):
            raise ValueError('manifest path escaped save roots')
        originals.add(relative)
        expected = e['sha256'].lower()
        if sha(saved) != expected:
            corrupted.append(str(relative))
        if not current.is_file():
            missing.append(str(relative))
        else:
            observed = sha(current)
            if observed != expected:
                changes.append(dict(relative=str(relative), before=expected, after=observed))
    added = sorted(str(f.relative_to(source)) for f in source.rglob('*') if f.is_file() and f.relative_to(source) not in originals)
    report = dict(backup=str(root), source=str(source), backup_verified=not corrupted,
                  backup_corruption=corrupted, changed=changes, added=added, missing=missing,
                  limitation=__doc__)
    (a.launch/'save-audit.json').write_text(json.dumps(report, indent=2)+'\n', encoding='utf8')
    print(json.dumps(report, indent=2))
    return int(bool(corrupted or missing))


if __name__ == '__main__':
    raise SystemExit(main())
