"""Portable RenderDoc fixture: only child environment changes, registry checked before/after."""
import datetime
import hashlib
import json
import os
from pathlib import Path
import subprocess
import winreg

ROOT=Path(__file__).resolve().parents[2]
MOD=ROOT/'vrmod'
RD=ROOT/'references/renderdoc/1.46/RenderDoc_1.46_64'

def registry_snapshot():
    result={}
    for root_name,root in [('HKLM',winreg.HKEY_LOCAL_MACHINE),('HKCU',winreg.HKEY_CURRENT_USER)]:
        for view_name,view in [('64',winreg.KEY_WOW64_64KEY),('32',winreg.KEY_WOW64_32KEY)]:
            for path in [r'SOFTWARE\Khronos\Vulkan\ExplicitLayers',r'SOFTWARE\Khronos\Vulkan\ImplicitLayers',r'SOFTWARE\Khronos\OpenXR\1',r'SOFTWARE\Microsoft\Windows NT\CurrentVersion\Windows']:
                key=f'{root_name}/{view_name}/{path}'
                try:
                    with winreg.OpenKey(root,path,0,winreg.KEY_READ|view) as handle:
                        values={}
                        for i in range(winreg.QueryInfoKey(handle)[1]):
                            name,value,kind=winreg.EnumValue(handle,i)
                            if path.endswith(r'CurrentVersion\Windows') and name not in ['AppInit_DLLs','LoadAppInit_DLLs','RequireSignedAppInit_DLLs']:continue
                            values[name]=[kind,value.hex() if isinstance(value,bytes) else value]
                        result[key]=values
                except FileNotFoundError:result[key]=None
    return result

def main():
    out=MOD/'output'/('renderdoc-fixture-'+datetime.datetime.now().strftime('%Y%m%d-%H%M%S'))
    out.mkdir(parents=True)
    baseline=registry_snapshot();(out/'registry-before.json').write_text(json.dumps(baseline,indent=2))
    env=os.environ.copy()
    env.update(VK_LAYER_PATH=str(RD),VK_INSTANCE_LAYERS='VK_LAYER_RENDERDOC_Capture',ENABLE_VULKAN_RENDERDOC_CAPTURE='1',XR_RUNTIME_JSON=str(MOD/'build/bin/Release/evr_mock_runtime.json'),EVR_MOCK_LOG=str(out/'mock.jsonl'),EVR_POSE_SEQUENCE='',EVR_RENDERDOC_CAPTURE=str(out/'stereo'),VK_LOADER_DEBUG='error,warn')
    env.pop('DISABLE_VULKAN_RENDERDOC_CAPTURE_1_46',None)
    code=1
    try:
        with (out/'fixture.log').open('w') as log:
            run=subprocess.run([str(MOD/'build/bin/Release/evr_xr_probe.exe'),'--frames','10','--capture-every','5','--output',str(out)],env=env,cwd=ROOT,stdout=log,stderr=subprocess.STDOUT,timeout=90,creationflags=subprocess.CREATE_NO_WINDOW)
        captures=list(out.glob('*.rdc'))
        if run.returncode or len(captures)!=1:raise RuntimeError(f'fixture exit={run.returncode}, captures={len(captures)}; see {out}')
        with (out/'replay.log').open('w') as log:
            replay=subprocess.run([str(MOD/'build/bin/Release/evr_renderdoc.exe'),str(RD/'renderdoc.dll'),'replay',str(captures[0]),str(out/'replay')],cwd=ROOT,stdout=log,stderr=subprocess.STDOUT,timeout=120,creationflags=subprocess.CREATE_NO_WINDOW)
        if replay.returncode:raise RuntimeError('replay failed; see '+str(out))
        code=0
    finally:
        after=registry_snapshot();(out/'registry-after.json').write_text(json.dumps(after,indent=2))
        same=baseline==after
        (out/'isolation.json').write_text(json.dumps({'registry_unchanged':same,'child_environment_only':True,'fixture_replay_exit':code},indent=2))
        print(('PASS' if same else 'FAIL')+' persistent Vulkan/OpenXR/AppInit registry unchanged')
        print(out)
        if not same:raise RuntimeError('Persistent settings changed; automatic overwrite intentionally refused')
    return code

if __name__=='__main__':raise SystemExit(main())
