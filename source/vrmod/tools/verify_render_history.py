"""Compare captured game history with the original game's isolated CPU math.
No running game required. This validates CPU camera/history layout, not stereo.
"""
import argparse
import json
import math
import pathlib
from emulate_camera_math import CameraMath, SHA256


def ppm_aspect(path):
    with path.open('rb') as stream:
        tokens=[]
        while len(tokens)<4:
            line=stream.readline()
            if not line:raise ValueError('incomplete PPM header')
            tokens.extend(line.split(b'#',1)[0].split())
    if tokens[0]!=b'P6' or tokens[3]!=b'255':raise ValueError('unsupported PPM')
    width,height=map(int,tokens[1:3])
    if width<=0 or height<=0:raise ValueError('invalid PPM extent')
    return width/height


def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('--observation',type=pathlib.Path,required=True)
    parser.add_argument('--mirror',type=pathlib.Path,help='fallback aspect evidence when texture metadata was not captured')
    args=parser.parse_args()
    report=json.loads((args.observation/'analysis.json').read_text())
    if not report['original_bytes_restored'] or report['invalid_samples']:
        raise ValueError('incomplete observation')
    engine=CameraMath(pathlib.Path('enshrouded.exe'))
    checks=[];previous={}
    def check(name,actual,expected,tolerance=2e-5):
        ok=len(actual)==len(expected) and all(math.isfinite(x) for x in actual+expected)
        error=max(abs(a-b) for a,b in zip(actual,expected))
        ok=ok and error<=tolerance
        checks.append(dict(name=name,status='PASS' if ok else 'FAIL',max_error=error))
    for row in report['samples']:
        address=row['view_address'];prior=previous.get(address);previous[address]=row
        history=row.get('history',{})
        if not prior or not history.get('valid'):continue
        camera=prior['camera'];prefix=f"sample_{row['sample']}_"
        old=history['previous_camera']
        # Exact integer comparison; never float-convert Q32 values for equality.
        checks.append(dict(name=prefix+'previous_camera_position',status='PASS' if old['position_q32']==camera['position_q32'] else 'FAIL'))
        check(prefix+'previous_camera_orientation',old['orientation'],camera['orientation'])
        if not all(abs(x-1)<1e-6 for x in old['scale']):raise ValueError('non-unit scale is outside this fixture')
        expected=engine.view(old['position_q32'],old['orientation'],old['position_q32'])
        indices=[i for i in range(16) if i%4!=3]
        check(prefix+'camera_centered_view',[history['matrices']['camera_centered_view'][i] for i in indices],
              [expected[i] for i in indices])
        extent=prior.get('target_extent')
        if extent:
            aspect=extent['width']/extent['height']
            source='captured texture metadata'
        elif args.mirror:
            aspect=ppm_aspect(args.mirror)
            source='captured mirror aspect; assumes the observed view fills that aspect'
        else:raise ValueError('no independent target-aspect evidence')
        if camera['projection_type']!=0:raise ValueError('history projection comparison currently requires perspective')
        expected=engine.projection(0,[aspect,camera['near'],camera['far'],camera['fov_y_radians']],True)
        check(prefix+'projection',history['matrices']['projection'],expected)
        indices=[i for i in range(16) if i not in (8,9)]
        check(prefix+'jitter_preserves_other_coefficients',[history['matrices']['jittered_projection'][i] for i in indices],
              [history['matrices']['projection'][i] for i in indices])
    if not checks:raise ValueError('no consecutive per-view history to compare')
    result=dict(executable_sha256=SHA256,scope='CPU history and camera math only; native stereo unverified',
                aspect_source=source,checks=checks,passed=sum(c['status']=='PASS' for c in checks),failed=sum(c['status']=='FAIL' for c in checks))
    (args.observation/'history-verification.json').write_text(json.dumps(result,indent=2),encoding='utf8')
    print(f"{result['passed']} PASS / {result['failed']} FAIL",result['scope'])
    return int(result['failed']>0)


if __name__=='__main__':raise SystemExit(main())
