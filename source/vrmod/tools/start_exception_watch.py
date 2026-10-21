"""Start a hidden, bounded exception debugger for one validated diagnostic launch.

Process-local debugger attachment, no hardware/software breakpoint patches,
no game context writes, no persistent WER settings or remote symbol/downloads.
"""
import argparse
import ctypes as c
from ctypes import wintypes as w
import hashlib
import json
from pathlib import Path
import subprocess
import time
from inject_camera_observer import Process, creation_filetime, k

k.CheckRemoteDebuggerPresent.argtypes=[w.HANDLE,c.POINTER(w.BOOL)]
k.CheckRemoteDebuggerPresent.restype=w.BOOL
ROOT=Path(__file__).resolve().parents[1]
EXE=ROOT/'build/bin/Release/evr_exception_watch.exe'


def records(directory):
    path=directory/'exceptions.jsonl'
    if not path.exists():return []
    rows=[]
    for line in path.read_text().splitlines():
        try:rows.append(json.loads(line))
        except json.JSONDecodeError:pass
    return rows


def spawn(pid,created,expected,out,seconds):
    out.parent.mkdir(parents=True,exist_ok=True)
    with (out.parent/(out.name+'-stdout.txt')).open('w') as stdout,(out.parent/(out.name+'-stderr.txt')).open('w') as stderr:
        return subprocess.Popen([str(EXE),'--pid',str(pid),'--created',str(created),'--expected-path',str(expected),
            '--output',str(out),'--seconds',str(seconds)],stdout=stdout,stderr=stderr,creationflags=subprocess.CREATE_NO_WINDOW)


def ready(watcher,out):
    deadline=time.monotonic()+10
    while time.monotonic()<deadline:
        if any(r['event']=='watch_ready' for r in records(out)):return
        if watcher.poll() is not None:raise RuntimeError('exception watcher exited before ready; see local stderr')
        time.sleep(.05)
    # The helper remains bounded and kill-on-exit is false; do not kill target.
    (out/'stop.request').write_text('stop')
    raise TimeoutError('watcher readiness timeout; requested detach')


def debugged(pid,expected):
    proc=Process(pid,expected)
    try:
        value=w.BOOL()
        if not k.CheckRemoteDebuggerPresent(proc.handle,c.byref(value)):raise OSError(c.get_last_error())
        return bool(value.value)
    finally:proc.close()


def self_test():
    out=ROOT/'output'/time.strftime('exception-watch-fixture-%Y%m%d-%H%M%S')
    out.mkdir(exist_ok=False)
    checks={}
    for mode in ('normal','handled','fastfail','watcher_exit','identity'):
        case=out/mode;case.mkdir()
        fixture=subprocess.Popen([str(EXE),'--fixture',mode,str(case)],stdout=subprocess.PIPE,text=True,creationflags=subprocess.CREATE_NO_WINDOW)
        watcher=None
        try:
            pid,created=map(int,fixture.stdout.readline().split())
            watcher=spawn(pid,created+(1 if mode=='identity' else 0),EXE,case/'watch',8)
            if mode=='identity':
                checks['identity_mismatch_refused']=watcher.wait(timeout=10)==1 and not debugged(pid,EXE) and fixture.poll() is None
                continue
            ready(watcher,case/'watch')
            checks[mode+'_attached']=debugged(pid,EXE)
            (case/'go').write_text('go')
            if mode=='watcher_exit':
                watcher.terminate();watcher.wait(timeout=5)
                checks['debugger_exit_keeps_target_alive']=fixture.poll() is None and not debugged(pid,EXE)
            elif mode=='fastfail':
                checks['fastfail_watch_exits']=watcher.wait(timeout=15)==0
                checks['fastfail_native_code_preserved']=fixture.wait(timeout=5)==0xc0000409
                rows=records(case/'watch')
                checks['fastfail_dump_saved']=any(r['event']=='watch_dump' and r['saved'] and r['context_valid'] for r in rows) and (case/'watch/exception_1.dmp').read_bytes()[:4]==b'MDMP'
                checks['fastfail_reason_recorded']=any(r['event']=='watch_exception' and r['code']==0xc0000409 and r['parameters']==[7] for r in rows)
            else:
                if mode=='handled':
                    deadline=time.monotonic()+5
                    while time.monotonic()<deadline and not (case/'handled').exists():time.sleep(.05)
                    checks['handled_exception_reaches_native_handler']=(case/'handled').exists() and fixture.poll() is None
                (case/'watch/stop.request').write_text('stop')
                checks[mode+'_detach_preserves_target']=watcher.wait(timeout=10)==0 and fixture.poll() is None and not debugged(pid,EXE)
                checks[mode+'_no_spurious_dump']=not list((case/'watch').glob('*.dmp'))
        finally:
            if watcher is not None and watcher.poll() is None:
                (case/'watch/stop.request').parent.mkdir(exist_ok=True)
                (case/'watch/stop.request').write_text('stop');watcher.wait(timeout=12)
            if fixture.poll() is None:(case/'stop').write_text('stop');fixture.wait(timeout=5)
            fixture.stdout.close()
    (out/'checks.json').write_text(json.dumps(checks,indent=2))
    for name,value in checks.items():print('PASS' if value else 'FAIL',name)
    print(out)
    return 0 if all(checks.values()) else 1


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('launch',nargs='?',type=Path)
    p.add_argument('--self-test',action='store_true')
    p.add_argument('--seconds',type=int,default=300)
    a=p.parse_args()
    if a.self_test:return self_test()
    if not a.launch or not 1<=a.seconds<=900:p.error('managed launch and 1..900 seconds required')
    launch=a.launch.resolve();state=json.loads((launch/'launch.json').read_text(encoding='utf-8-sig'))
    expected=ROOT.parent/'enshrouded.exe'
    profile=json.loads((ROOT/'config/profiles/build26082781.json').read_text())
    if hashlib.sha256(expected.read_bytes()).hexdigest()!=profile['exe_sha256']:raise ValueError('unsupported game build')
    proc=Process(state['pid'],expected)
    try:
        if creation_filetime(proc.handle)!=int(state['creation_filetime']):raise ValueError('managed PID reused')
    finally:proc.close()
    out=launch/time.strftime('exception-watch-%Y%m%d-%H%M%S')
    watcher=spawn(state['pid'],state['creation_filetime'],expected,out,a.seconds)
    ready(watcher,out)
    (out/'watcher.json').write_text(json.dumps(dict(pid=watcher.pid,target_pid=state['pid'],duration_seconds=a.seconds,
        target_creation_filetime=state['creation_filetime'],executable=str(EXE),stop_request=str(out/'stop.request'),local_only=True),indent=2))
    print('PASS exception watcher ready',out)
    return 0


if __name__=='__main__':raise SystemExit(main())
