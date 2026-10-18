"""Copy current source and top-level docs into a new, hash-verified local snapshot.
Existing evidence, references, builds, saves and snapshots are never modified.
"""
import argparse,hashlib,json,shutil
from datetime import datetime
from pathlib import Path

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('label');a=p.parse_args()
 if not a.label or any(c not in 'abcdefghijklmnopqrstuvwxyz0123456789-' for c in a.label):raise ValueError('simple lowercase snapshot label required')
 root=Path(__file__).resolve().parents[2];mod=root/'vrmod';out=mod/'output/source-backups'/(datetime.now().strftime('%Y%m%d-%H%M%S-')+a.label)
 if not out.resolve().is_relative_to(root):raise ValueError('snapshot destination escaped workspace')
 files=list((root/'docs').glob('*.md'))+[f for f in mod.iterdir() if f.is_file()]
 for name in ('config','driver','hooks','include','layer','loader','package','runtime','settings','shaders','test_env','tests'):   # settings: the VR setting program; loader, package: the start by itself and the package's own files (2026-10-14)
  folder=mod/name
  if folder.exists():files += [f for f in folder.rglob('*') if f.is_file() and '__pycache__' not in f.parts]
 files += [f for f in (mod/'tools').iterdir() if f.is_file()]
 files += [f for f in (mod/'tools'/'handoff_helpers').iterdir() if f.is_file()] if (mod/'tools'/'handoff_helpers').exists() else []   # the trial runners are edited too
 out.mkdir(parents=True,exist_ok=False);manifest=[]
 for f in sorted(set(files)):
  if not f.resolve().is_relative_to(root):raise ValueError('source escaped workspace')
  relative=f.relative_to(root);dest=out/relative;dest.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(f,dest)
  digest=lambda q:hashlib.sha256(q.read_bytes()).hexdigest()
  before=digest(f);after=digest(dest)
  if before!=after:raise ValueError('snapshot verification failed: '+str(relative))
  manifest.append(dict(path=relative.as_posix(),sha256=before,bytes=f.stat().st_size))
 (out/'manifest.json').write_text(json.dumps(dict(files=manifest,verified=True,excluded='Existing output/analysis/reference data remains at its original workspace location.'),indent=2)+'\n',encoding='utf-8')
 print('PASS',len(manifest),'files hash-verified:',out)
if __name__=='__main__':main()
