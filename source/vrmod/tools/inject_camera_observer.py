"""Load the bounded observation DLL into a validated local process.

Remote writes contain only the DLL filename and output directory allocated by this tool.
The default observer patches one verified instruction temporarily without writing
player/camera/save data. Other bounded diagnostic exports explicitly passed to
inject() define their own scope; see run_projection_probe.py for calibration.
Never terminates a timed-out remote thread or frees its still-in-use argument buffer.
"""
import argparse
import ctypes as c
from ctypes import wintypes as w
import hashlib
import json
import pathlib
import subprocess
import sys
import time
from read_game_memory import Process, Module, k
sys.path.insert(0, str(pathlib.Path(__file__).parent/'python'))
import pefile

k.Module32NextW.argtypes=[w.HANDLE,c.POINTER(Module)]
k.Module32NextW.restype=w.BOOL
k.VirtualAllocEx.argtypes=[w.HANDLE,c.c_void_p,c.c_size_t,w.DWORD,w.DWORD]
k.VirtualAllocEx.restype=c.c_void_p
k.VirtualFreeEx.argtypes=[w.HANDLE,c.c_void_p,c.c_size_t,w.DWORD]
k.VirtualFreeEx.restype=w.BOOL
k.WriteProcessMemory.argtypes=[w.HANDLE,c.c_void_p,c.c_void_p,c.c_size_t,c.POINTER(c.c_size_t)]
k.WriteProcessMemory.restype=w.BOOL
k.CreateRemoteThread.argtypes=[w.HANDLE,c.c_void_p,c.c_size_t,c.c_void_p,c.c_void_p,w.DWORD,c.POINTER(w.DWORD)]
k.CreateRemoteThread.restype=w.HANDLE
k.WaitForSingleObject.argtypes=[w.HANDLE,w.DWORD]
k.WaitForSingleObject.restype=w.DWORD
k.GetExitCodeThread.argtypes=[w.HANDLE,c.POINTER(w.DWORD)]
k.GetExitCodeThread.restype=w.BOOL
k.GetModuleHandleW.argtypes=[w.LPCWSTR]
k.GetModuleHandleW.restype=w.HMODULE
k.GetProcAddress.argtypes=[w.HMODULE,c.c_char_p]
k.GetProcAddress.restype=c.c_void_p
k.GetModuleHandleExW.argtypes=[w.DWORD,c.c_void_p,c.POINTER(w.HMODULE)]
k.GetModuleHandleExW.restype=w.BOOL
k.GetModuleFileNameW.argtypes=[w.HMODULE,w.LPWSTR,w.DWORD]
k.GetModuleFileNameW.restype=w.DWORD


def module_snapshot(pid):
    # The loader may change the module list while TH32CS_SNAPMODULE runs.
    # Microsoft documents retry for ERROR_BAD_LENGTH only. Bound retries to
    # 200 ms; never retry injection, access-denied, or any other error here.
    for attempt in range(21):
        snapshot=k.CreateToolhelp32Snapshot(0x8,pid)
        if snapshot!=c.c_void_p(-1).value:return snapshot
        error=c.get_last_error()
        if error!=24 or attempt==20:raise OSError(error,'module snapshot')
        time.sleep(.01)
    raise AssertionError('unreachable snapshot retry')


def modules(pid):
    snapshot=module_snapshot(pid)
    result=[]
    try:
        entry=Module();entry.size=c.sizeof(entry)
        valid=k.Module32FirstW(snapshot,c.byref(entry))
        while valid:
            result.append((pathlib.Path(entry.path),entry.base))
            valid=k.Module32NextW(snapshot,c.byref(entry))
        return result
    finally:k.CloseHandle(snapshot)


def remote_export(pid, module_name, function):
    local_module=k.GetModuleHandleW(module_name)
    address=k.GetProcAddress(local_module,function.encode())
    if not address:raise ValueError('local export missing')
    owner=w.HMODULE()
    # Handles forwarded kernel32 exports by mapping the actual owning module (often KernelBase).
    if not k.GetModuleHandleExW(0x4|0x2,address,c.byref(owner)):raise OSError(c.get_last_error(),'export owner')
    filename=c.create_unicode_buffer(32768)
    if not k.GetModuleFileNameW(owner,filename,len(filename)):raise OSError(c.get_last_error(),'export owner filename')
    name=pathlib.Path(filename.value).name.lower()
    remote=next((base for path,base in modules(pid) if path.name.lower()==name),None)
    if remote is None:raise ValueError('remote system module missing')
    return remote+address-owner.value


def call(process, address, text, timeout_ms, audit, phase):
    data=text.encode('utf-16-le')+b'\0\0'
    memory=k.VirtualAllocEx(process.handle,None,len(data),0x1000|0x2000,0x04)
    if not memory:raise OSError(c.get_last_error(),'argument allocation')
    thread=None;completed=False;started=False
    try:
        actual=c.c_size_t()
        if not k.WriteProcessMemory(process.handle,memory,data,len(data),c.byref(actual)) or actual.value!=len(data):
            raise OSError(c.get_last_error(),'write owned argument buffer')
        tid=w.DWORD()
        thread=k.CreateRemoteThread(process.handle,None,0,address,memory,0,c.byref(tid))
        if not thread:raise OSError(c.get_last_error(),'CreateRemoteThread')
        started=True
        audit.append(dict(phase=phase,thread=tid.value,argument=hex(memory),started=True))
        waited=k.WaitForSingleObject(thread,timeout_ms)
        if waited!=0:
            audit[-1]['pending']=True
            raise RuntimeError('Remote call not completed; argument retained and thread left running. Do not retry automatically.')
        completed=True
        result=w.DWORD()
        if not k.GetExitCodeThread(thread,c.byref(result)):raise OSError(c.get_last_error(),'thread result')
        audit[-1].update(pending=False,result=result.value)
        return result.value
    finally:
        if thread:k.CloseHandle(thread)
        if completed or not started:
            if not k.VirtualFreeEx(process.handle,memory,0,0x8000):raise OSError(c.get_last_error(),'owned argument release')


def creation_filetime(handle):
    k.GetProcessTimes.argtypes=[w.HANDLE,c.POINTER(w.FILETIME),c.POINTER(w.FILETIME),c.POINTER(w.FILETIME),c.POINTER(w.FILETIME)]
    k.GetProcessTimes.restype=w.BOOL
    values=[w.FILETIME() for _ in range(4)]
    if not k.GetProcessTimes(handle,*[c.byref(v) for v in values]):
        raise OSError(c.get_last_error(),'GetProcessTimes')
    return (values[0].dwHighDateTime<<32)|values[0].dwLowDateTime


def inject(pid, expected, output, dll, export_name=b'EVR_ObserveCamera', expected_creation=None, timeout_ms=10000, input_files=None):
    proc=Process(pid,expected)
    audit=[]
    binary_evidence={}
    output.mkdir(parents=True,exist_ok=False)
    try:
        creation=creation_filetime(proc.handle)
        if expected_creation is not None and creation!=int(expected_creation):
            raise ValueError('target creation time differs from managed launch')
        for name, content in (input_files or {}).items():
            if pathlib.Path(name).name != name or name in ('.', '..') or not isinstance(content, bytes) or len(content)>1048576:
                raise ValueError('invalid bounded diagnostic input file')
            (output/name).write_bytes(content)
        handle=k.OpenProcess(0x0400|0x0010|0x0002|0x0008|0x0020,False,pid)
        if not handle:raise OSError(c.get_last_error(),'OpenProcess observation loader')
        try:
            if creation_filetime(handle)!=creation:
                raise ValueError('target PID was reused while acquiring observation access')
        except BaseException:
            k.CloseHandle(handle)
            raise
        proc.close();proc.handle=handle
        if any(path==dll for path,base in modules(pid)):
            raise ValueError('observer DLL already loaded; repeated installation refused')
        image=dll.read_bytes()
        if not 0<len(image)<=32*1024*1024:raise ValueError('diagnostic DLL outside archive size bound')
        digest=hashlib.sha256(image).hexdigest()
        archive=output/'diagnostic-binary';archive.mkdir()
        preserved=archive/dll.name;preserved.write_bytes(image)
        if hashlib.sha256(preserved.read_bytes()).hexdigest()!=digest:raise ValueError('diagnostic DLL archive verification failed')
        binary_evidence=dict(sha256=digest,bytes=len(image),archive=str(preserved),export=export_name.decode('ascii'))
        export=next((e.address for e in pefile.PE(data=image).DIRECTORY_ENTRY_EXPORT.symbols if e.name==export_name),None)
        if export is None:raise ValueError('observer export missing')
        if hashlib.sha256(dll.read_bytes()).hexdigest()!=digest:raise ValueError('diagnostic DLL changed while preparing injection')
        call(proc,remote_export(pid,'kernel32.dll','LoadLibraryW'),str(dll),10000,audit,'load_library')
        base=next((base for path,base in modules(pid) if path==dll),None)
        if base is None:raise ValueError('DLL was not loaded into target')
        result=call(proc,base+export,str(output),timeout_ms,audit,'bounded_diagnostic')
        return result
    finally:
        (output/'loader-audit.json').write_text(json.dumps(dict(pid=pid,expected=str(expected),dll=str(dll),binary_evidence=binary_evidence,expected_creation_filetime=expected_creation,calls=audit),indent=2))
        proc.close()


def main():
    p=argparse.ArgumentParser()
    p.add_argument('--launch',type=pathlib.Path)
    p.add_argument('--self-test',action='store_true')
    p.add_argument('--render',action='store_true',help='Observe render_view entry instead of player camera query')
    a=p.parse_args()
    mod=pathlib.Path(__file__).resolve().parents[1]
    binary=mod/'build/bin/Release'
    kind='render' if a.render else 'camera'
    export_name=b'EVR_ObserveRender' if a.render else b'EVR_ObserveCamera'
    dll=pathlib.Path(json.loads((binary/f'evr_{kind}_observer.json').read_text())['library_path'])
    if a.self_test:
        fixture_exe=binary/'evr_process_probe.exe'
        fixture=subprocess.Popen([str(fixture_exe),'--fixture'],stdout=subprocess.PIPE,text=True,creationflags=subprocess.CREATE_NO_WINDOW)
        try:
            pid,_=fixture.stdout.readline().split()
            out=mod/'output'/time.strftime(kind+'-observer-loader-selftest-%Y%m%d-%H%M%S')
            probe=Process(int(pid),fixture_exe)
            try:created=creation_filetime(probe.handle)
            finally:probe.close()
            try:inject(int(pid),fixture_exe,out.with_name(out.name+'-stale'),dll,export_name,created+1)
            except ValueError as exc:assert 'creation time differs' in str(exc)
            else:raise AssertionError('stale creation must be rejected')
            stale=json.loads((out.with_name(out.name+'-stale')/'loader-audit.json').read_text())
            assert not stale['calls'],'stale launch must be rejected before remote execution'
            print('PASS stale process creation rejected before allocation or remote execution')
            result=inject(int(pid),fixture_exe,out,dll,export_name,created)
            loaded=json.loads((out/'loader-audit.json').read_text());proof=loaded['binary_evidence']
            archived=pathlib.Path(proof['archive'])
            assert archived.read_bytes()==dll.read_bytes() and archived.stat().st_size==proof['bytes']
            assert hashlib.sha256(archived.read_bytes()).hexdigest()==proof['sha256']
            print('PASS diagnostic binary archived byte-identically with SHA256 and export identity')
            assert result==1,'fixture must be rejected by game build gate'
            log=(out/f'{kind}-observer.jsonl').read_text()
            assert 'unsupported loaded executable build' in log
            assert fixture.poll() is None,'fixture exited unexpectedly'
            print('PASS remote DLL load, export invocation, unsupported game rejection, argument cleanup, target alive')
            print(out)
        finally:fixture.terminate();fixture.wait(timeout=5)
        return 0
    if not a.launch:p.error('--launch required')
    game=mod.parent/'enshrouded.exe'
    profile=json.loads((mod/'config/profiles/build23966345.json').read_text())
    if hashlib.sha256(game.read_bytes()).hexdigest()!=profile['exe_sha256']:raise ValueError('unsupported game hash')
    launch=json.loads((a.launch/'launch.json').read_text(encoding='utf-8-sig'))
    if launch['mode'] not in ('diagnostic_only','openxr_graphics_preparation'):raise ValueError('not a managed diagnostic launch')
    if not launch.get('creation_filetime'):raise ValueError('managed launch lacks native process creation time; observation refused')
    if time.time()-(a.launch/'vulkan.jsonl').stat().st_mtime>10:raise ValueError('game render telemetry is stale')
    out=a.launch/time.strftime(kind+'-observer-%Y%m%d-%H%M%S')
    result=inject(launch['pid'],game,out,dll,export_name,launch['creation_filetime'])
    print('Observer result:',result,'Output:',out)
    return result


if __name__=='__main__':sys.exit(main())
