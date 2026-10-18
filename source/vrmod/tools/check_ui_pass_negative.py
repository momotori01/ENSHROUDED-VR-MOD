"""Read-only negative controls: reject a monoscopic world or damaged UI as isolated HUD."""
import argparse,json,pathlib,sys
from check_ui_pass import check

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('folder',type=pathlib.Path)
    p.add_argument('--early-clear',type=pathlib.Path,required=True);p.add_argument('--output',type=pathlib.Path,required=True);a=p.parse_args()
    cases=[]
    for name,data in [('opaque_world_as_ui',(a.folder/'before.bin').read_bytes()),('early_background_clear',a.early_clear.read_bytes())]:
        result=check(a.folder,{'isolated':data});failed=[e['name'] for e in result['checks'] if e['status']=='FAIL']
        cases.append(dict(name=name,status='PASS' if failed else 'FAIL',detected_failures=failed,metrics=result['metrics']))
    report=dict(scope=__doc__,source_unchanged=True,cases=cases,passed=sum(e['status']=='PASS' for e in cases),failed=sum(e['status']=='FAIL' for e in cases))
    a.output.parent.mkdir(parents=True,exist_ok=True);a.output.write_text(json.dumps(report,indent=2)+'\n')
    print(f"{report['passed']} PASS / {report['failed']} FAIL negative controls");return bool(report['failed'])
if __name__=='__main__':sys.exit(main())
