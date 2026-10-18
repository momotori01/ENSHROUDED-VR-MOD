"""Verify the launch-time full save snapshot against an explicitly restored reference."""
import argparse,json
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__);p.add_argument('launch',type=Path);p.add_argument('reference',type=Path);a=p.parse_args()
launch=json.loads((a.launch/'launch.json').read_text(encoding='utf-8-sig'));snapshot=Path(launch['save_backup']['path'])
def manifest(path):
 entries=json.loads((path/'manifest.json').read_text(encoding='utf-8-sig'))
 if len({e['relative'] for e in entries})!=len(entries):raise ValueError('duplicate manifest files')
 # 2026-10-16 (the 1.0 update): only the flat save files; the game's own backup\ and migrated\ folders beside them
 # change at every start and are not restored (restore_scenery_backup.ps1).
 return {e['relative']:e['sha256'].lower() for e in entries if '\\' not in e['relative'] and '/' not in e['relative']}
actual,expected=manifest(snapshot),manifest(a.reference);differences=sorted(k for k in set(actual)|set(expected) if actual.get(k)!=expected.get(k))
r=dict(status='FAIL' if differences else 'PASS',files=len(actual),reference=str(a.reference),launch_snapshot=str(snapshot),same_saved_state=not differences,differences=differences,limitation='Identical saved bytes; running simulation time, weather and exposure still require image verification.')
(a.launch/'save-reference-check.json').write_text(json.dumps(r,indent=2)+'\n',encoding='utf-8');print(json.dumps(r,indent=2));raise SystemExit(int(bool(differences)))
