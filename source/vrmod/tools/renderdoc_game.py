"""Launch only the supported game under portable RenderDoc, preserving persistent settings."""
import datetime, hashlib, json, os
from pathlib import Path
import subprocess
from renderdoc_fixture import ROOT, MOD, RD, registry_snapshot
from inject_camera_observer import Process, creation_filetime

def main():
    exe=ROOT/'enshrouded.exe'
    if hashlib.sha256(exe.read_bytes()).hexdigest()!='af2f5a1227911d8aa06b3908d6bd0211838211cae14ea91099cb57d0df990781':raise RuntimeError('Unsupported game build')
    running=subprocess.run(['powershell.exe','-NoProfile','-Command','@(Get-Process enshrouded -ErrorAction SilentlyContinue).Count'],capture_output=True,text=True,creationflags=subprocess.CREATE_NO_WINDOW,check=True)
    if running.stdout.strip()!='0':raise RuntimeError('Game already running')
    out=MOD/'output'/('renderdoc-game-'+datetime.datetime.now().strftime('%Y%m%d-%H%M%S'));out.mkdir()
    baseline=registry_snapshot();(out/'registry-before.json').write_text(json.dumps(baseline,indent=2))
    env=os.environ.copy()
    layers=out/'layers';layers.mkdir()
    (layers/'evr_layer.json').write_bytes((MOD/'build/bin/Release/evr_layer.json').read_bytes())
    env.update({'EVR_RD_CHILD_'+key:value for key,value in dict(VK_LAYER_PATH=str(layers)+';'+str(RD),VK_INSTANCE_LAYERS='VK_LAYER_RENDERDOC_Capture;VK_LAYER_EVR_diagnostic',ENABLE_VULKAN_RENDERDOC_CAPTURE='1',EVR_LAYER_OUTPUT=str(out),SteamAppId='1203620',SteamGameId='1203620',EVR_PREPARE_OPENXR='0',EVR_PREPARE_SESSION_FIXTURE='0',VK_LOADER_DEBUG='error,warn').items()})
    env.pop('DISABLE_VULKAN_RENDERDOC_CAPTURE_1_46',None)
    try:
        with (out/'capture.log').open('w') as log:
            process=subprocess.run([str(MOD/'build/bin/Release/evr_renderdoc.exe'),str(RD/'renderdoc.dll'),'capture',str(exe),str(ROOT),'',str(out),'30'],cwd=ROOT,env=env,stdout=log,stderr=subprocess.STDOUT,timeout=165,creationflags=subprocess.CREATE_NO_WINDOW)
        print('capture helper exit',process.returncode)
        return process.returncode
    finally:
        target_path=out/'renderdoc-target.json'
        if target_path.exists():
            try:
                target=json.loads(target_path.read_text());owned=Process(target['pid'],exe)
                try:(out/'launch.json').write_text(json.dumps(dict(pid=target['pid'],creation_filetime=str(creation_filetime(owned.handle)),mode='diagnostic_only',renderdoc=True,output=str(out)),indent=2))
                finally:owned.close()
            except (OSError,ValueError) as error:
                (out/'process-identity-error.txt').write_text(str(error))
        after=registry_snapshot();(out/'registry-after.json').write_text(json.dumps(after,indent=2))
        (out/'isolation.json').write_text(json.dumps({'registry_unchanged':baseline==after,'child_environment_only':True},indent=2))
        print(out)
        if baseline!=after:raise RuntimeError('Persistent settings changed; not overwriting automatically')

if __name__=='__main__':raise SystemExit(main())
