"""Sanity-check saved native XR images against actual submitted frame IDs."""
import argparse,pathlib,json,statistics,hashlib
from check_capture import ppm

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('directory',type=pathlib.Path);a=p.parse_args();directory=a.directory
 rows=[json.loads(l) for l in (directory/'openxr.jsonl').read_text().splitlines()];frames={r['frame'] for r in rows if r['event']=='stereo_frame'};reports=[]
 for left in sorted(directory.glob('frame-*-eye-0.ppm')):
  right=left.with_name(left.name.replace('eye-0','eye-1'));w,h,l=ppm(left);rw,rh,r=ppm(right);frame=int(left.name.split('-')[1])
  checks=dict(dimensions_match=(w,h)==(rw,rh),captured_submitted_frame=frame in frames,left_nonblack=statistics.mean(l[::17])>3,right_nonblack=statistics.mean(r[::17])>3,left_nonuniform=statistics.pvariance(l[::51])>4,right_nonuniform=statistics.pvariance(r[::51])>4,stereo_distinct=l!=r)
  reports.append(dict(frame=frame,width=w,height=h,checks=checks,left_sha256=hashlib.sha256(left.read_bytes()).hexdigest(),right_sha256=hashlib.sha256(right.read_bytes()).hexdigest()))
 good=bool(reports) and all(all(r['checks'].values()) for r in reports)
 report=dict(status='PASS' if good else 'FAIL',pairs=reports,limit='Saved submitted image pairs only; no image-based motion or comfort claim.');(directory/'native-image-checks.json').write_text(json.dumps(report,indent=2)+'\n');print(report);return int(not good)
if __name__=='__main__':raise SystemExit(main())
