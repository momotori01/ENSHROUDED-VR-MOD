"""Load an entire hand CSV in the built Mock runtime before starting the game."""
import argparse,datetime,json,os,subprocess
from pathlib import Path

def main():
 a=argparse.ArgumentParser(description=__doc__);a.add_argument('hands',type=Path);v=a.parse_args()
 root=Path(__file__).resolve().parents[2];fixture=v.hands.resolve();assert fixture.is_file(),'Hand fixture missing'
 binary=root/'vrmod/build/bin/Release/evr_xr_probe.exe';manifest=root/'vrmod/build/bin/Release/evr_mock_runtime.json'
 if not binary.is_file() or not manifest.is_file():raise RuntimeError('Build Release before hand preflight')
 # A new child process and explicit Mock manifest: no installed runtime/settings changes.
 out=root/'vrmod/output'/('mock-hands-preflight-'+datetime.datetime.now().strftime('%Y%m%d-%H%M%S-%f'));out.mkdir()
 env=os.environ.copy();env['XR_RUNTIME_JSON']=str(manifest);env['EVR_MOCK_HAND_SEQUENCE']=str(fixture);env['EVR_MOCK_LOG']=str(out/'runtime.jsonl')
 for key in ['EVR_MOCK_NAV_FILE','EVR_POSE_SEQUENCE']:env.pop(key,None)
 r=subprocess.run([str(binary),'--output',str(out),'--frames','2','--scale','.1','--capture-every','0','--quiet'],env=env,cwd=root,capture_output=True,text=True,timeout=40)
 (out/'result.txt').write_text(r.stdout+r.stderr,encoding='utf-8')
 report=dict(status='PASS' if r.returncode==0 and 'RESULT PASS' in r.stdout else 'FAIL',fixture=str(fixture),exit_code=r.returncode,output=str(out),scope='Mock CSV parse and two tiny test-scene frames, not gameplay evidence')
 (out/'check.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8');print(json.dumps(report));return report['status']!='PASS'
if __name__=='__main__':raise SystemExit(main())
