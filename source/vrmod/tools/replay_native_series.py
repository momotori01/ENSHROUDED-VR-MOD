"""Sequential local RenderDoc replay of the three bounded native series captures."""
import argparse, json, subprocess
from pathlib import Path
from renderdoc_fixture import ROOT, MOD, RD


def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('probe',type=Path);a=p.parse_args()
    for label,stem in [('first','native_pair'),('quarter','native_series_quarter'),('final','native_series_final')]:
        capture=(a.probe/(stem+'_capture.rdc')).resolve();out=(a.probe/('replay-'+label)).resolve()
        if not capture.is_file() or out.exists():raise ValueError('capture missing or replay output already exists: '+str(out))
        with (a.probe/('replay-'+label+'.log')).open('w') as log:
            result=subprocess.run([str(MOD/'build/bin/Release/evr_renderdoc.exe'),str(RD/'renderdoc.dll'),'replay',str(capture),str(out)],
                                  cwd=ROOT,stdout=log,stderr=subprocess.STDOUT,timeout=180,creationflags=subprocess.CREATE_NO_WINDOW)
        if result.returncode:raise RuntimeError('replay failed: '+label)
        print(label,json.loads((out/'result.json').read_text()),flush=True)
    return 0


if __name__=='__main__':raise SystemExit(main())
