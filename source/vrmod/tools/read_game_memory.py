"""Bounded, read-only discovery using a code-pointer anchor recovered from a local dump.
No debugger, thread suspension, remote execution, WriteProcessMemory, or registry changes.
Addresses are ephemeral; matched data is a candidate, not a writable game ABI.
"""
import argparse
import ctypes as c
from ctypes import wintypes as w
import hashlib
import json
import math
import pathlib
import struct
import time
from analyze_camera_dump import transform

k = c.WinDLL('kernel32', use_last_error=True)
k.OpenProcess.argtypes = [w.DWORD, w.BOOL, w.DWORD]
k.OpenProcess.restype = w.HANDLE
k.CloseHandle.argtypes = [w.HANDLE]
k.ReadProcessMemory.argtypes = [w.HANDLE, c.c_void_p, c.c_void_p, c.c_size_t, c.POINTER(c.c_size_t)]
k.ReadProcessMemory.restype = w.BOOL
k.QueryFullProcessImageNameW.argtypes = [w.HANDLE, w.DWORD, w.LPWSTR, c.POINTER(w.DWORD)]
k.QueryFullProcessImageNameW.restype = w.BOOL
k.VirtualQueryEx.argtypes = [w.HANDLE, c.c_void_p, c.c_void_p, c.c_size_t]
k.VirtualQueryEx.restype = c.c_size_t
k.CreateToolhelp32Snapshot.argtypes = [w.DWORD, w.DWORD]
k.CreateToolhelp32Snapshot.restype = w.HANDLE


class Module(c.Structure):
    _fields_ = [('size', w.DWORD), ('moduleId', w.DWORD), ('processId', w.DWORD),
                ('globalCount', w.DWORD), ('processCount', w.DWORD), ('base', c.c_void_p),
                ('imageSize', w.DWORD), ('module', w.HMODULE), ('name', w.WCHAR*256), ('path', w.WCHAR*260)]


class Region(c.Structure):
    _fields_ = [('base', c.c_void_p), ('allocationBase', c.c_void_p), ('allocationProtect', w.DWORD),
                ('partition', w.WORD), ('regionSize', c.c_size_t), ('state', w.DWORD),
                ('protect', w.DWORD), ('type', w.DWORD)]


k.Module32FirstW.argtypes = [w.HANDLE, c.POINTER(Module)]
k.Module32FirstW.restype = w.BOOL


class Process:
    def __init__(self, pid, expected):
        self.handle = k.OpenProcess(0x0400 | 0x0010, False, pid)
        if not self.handle:
            raise OSError(c.get_last_error(), 'OpenProcess read-only')
        try:
            name = c.create_unicode_buffer(32768)
            length = w.DWORD(len(name))
            if not k.QueryFullProcessImageNameW(self.handle, 0, name, c.byref(length)):
                raise OSError(c.get_last_error(), 'QueryFullProcessImageNameW')
            if not pathlib.Path(name.value).samefile(expected):
                raise ValueError('target executable mismatch')
            snapshot = k.CreateToolhelp32Snapshot(0x8, pid)
            if snapshot == c.c_void_p(-1).value:
                raise OSError(c.get_last_error(), 'module snapshot')
            try:
                module = Module()
                module.size = c.sizeof(module)
                if not k.Module32FirstW(snapshot, c.byref(module)):
                    raise OSError(c.get_last_error(), 'module lookup')
                self.base = module.base
            finally:
                k.CloseHandle(snapshot)
        except BaseException:
            self.close()
            raise

    def close(self):
        if self.handle:
            k.CloseHandle(self.handle)
            self.handle = None

    def read(self, address, size):
        data = c.create_string_buffer(size)
        actual = c.c_size_t()
        if not k.ReadProcessMemory(self.handle, address, data, size, c.byref(actual)) or actual.value != size:
            return None
        return data.raw

    def regions(self):
        address = 0
        while address < 0x7ffffffffffF:
            r = Region()
            if not k.VirtualQueryEx(self.handle, address, c.byref(r), c.sizeof(r)):
                break
            end = (r.base or 0) + r.regionSize
            if end <= address:
                break
            if r.state == 0x1000 and r.type == 0x20000 and r.protect & 0xff in (2,4,8,0x20,0x40,0x80) and not r.protect & 0x100:
                yield r.base, r.regionSize
            address = end


def camera(data):
    if data is None or len(data) < 64:
        return None
    position = struct.unpack_from('<3q', data)
    q = struct.unpack_from('<4f', data, 24)
    distance, fov, aspect, near, far = struct.unpack_from('<5f', data, 40)
    if not all(math.isfinite(x) for x in q + (distance, fov, aspect, near, far)):
        return None
    norm = sum(x*x for x in q)
    if not (.95 < norm < 1.05 and .1 < fov < 3.1 and .2 < aspect < 6 and 0 < near < far < 1e8):
        return None
    return dict(position_q32=position, position=[x/2**32 for x in position], orientation=q,
                distance=distance, fov_y=fov, aspect=aspect, near=near, far=far)


def main():
    p = argparse.ArgumentParser()
    p.add_argument('--launch', type=pathlib.Path, required=True)
    p.add_argument('--max-mib', type=int, default=2048)
    p.add_argument('--seconds', type=float, default=15)
    p.add_argument('--observe', type=pathlib.Path, help='Resample candidates from an earlier scan in this same process')
    p.add_argument('--samples', type=int, default=60)
    p.add_argument('--query-stacks', type=pathlib.Path, help='Find query pointer tuples in recent, same-process thread stacks')
    p.add_argument('--root-scan', action='store_true', help='Find nearby triple RenderTransform layout candidates, observation only')
    a = p.parse_args()
    if not 1 <= a.max_mib <= 8192 or not 0 < a.seconds <= 60 or not 1 <= a.samples <= 600:
        p.error('invalid read bounds')
    mod = pathlib.Path(__file__).resolve().parents[1]
    game = mod.parent / 'enshrouded.exe'
    profile = json.loads((mod/'config/profiles/build23966345.json').read_text())
    if hashlib.sha256(game.read_bytes()).hexdigest() != profile['exe_sha256']:
        raise ValueError('unsupported game build')
    launch = json.loads((a.launch/'launch.json').read_text(encoding='utf-8-sig'))
    proc = Process(launch['pid'], game)
    try:
        # This allocator callback was present at query slot 3 + 0x28 in the recorded camera dump.
        # The camera layout at object +0x60 is independently checked; matches remain candidates.
        pointer_rva = 0x324ba0
        anchor = struct.pack('<Q', proc.base + pointer_rva)
        output = a.launch / time.strftime('memory-observation-%Y%m%d-%H%M%S.json')
        deadline = time.monotonic() + a.seconds
        if a.observe:
            prior = json.loads(a.observe.read_text())
            if prior['pid'] != launch['pid'] or prior['base'] != hex(proc.base) or prior['launch_started'] != launch['started']:
                raise ValueError('candidate addresses belong to a different launch')
            candidates = prior['candidates']
            rows = []
            if a.root_scan:
                if len(candidates) != 1:
                    raise ValueError('root proximity scan requires one camera candidate')
                eye=camera(proc.read(int(candidates[0]['camera_address'],16),64))
                if not eye:
                    raise ValueError('camera candidate is no longer valid')
                found={};scanned=0
                unit_scale=struct.pack('<3f',1,1,1)
                regions=sorted(proc.regions(),key=lambda r:(not 0x30000000000<=r[0]<0x40000000000,r[0]))
                for base,size in regions:
                    if scanned>=a.max_mib*1024**2 or time.monotonic()>=deadline:
                        break
                    for offset in range(0,size,1024**2):
                        if scanned>=a.max_mib*1024**2 or time.monotonic()>=deadline:
                            break
                        length=min(1024**2+175,size-offset,a.max_mib*1024**2-scanned)
                        data=proc.read(base+offset,length);scanned+=length
                        if data is None:continue
                        at=data.find(unit_scale)
                        while at>=0:
                            start=at-40
                            if start>=0 and start+176<=len(data) and (base+offset+start)%8==0:
                                position=struct.unpack_from('<3q',data,start)
                                delta=[(x-y)/2**32 for x,y in zip(position,eye['position_q32'])]
                                if sum(x*x for x in delta)<100 and data[start:start+52]==data[start+56:start+108] and data[start:start+52]==data[start+112:start+164]:
                                    value=transform(data[start:start+56])
                                    if value and .95<value['quaternion_norm']<1.05:
                                        address=base+offset+start
                                        found[address]=dict(address=hex(address),transform=value,ticks=struct.unpack_from('<2I',data,start+168))
                            at=data.find(unit_scale,at+1)
                report=dict(pid=launch['pid'],base=hex(proc.base),launch_started=launch['started'],
                            candidates=candidates,root_candidates=list(found.values()),scanned_bytes=scanned,
                            note='Proximity/layout candidates only; entity identity is not established')
                output.write_text(json.dumps(report,indent=2,allow_nan=False),encoding='utf8')
                print('Nearby RenderTransform candidates:',len(found),'Report:',output)
                for row in found.values():print(row)
                return
            if a.query_stacks:
                snapshots = [json.loads(line) for line in a.query_stacks.read_text().splitlines()]
                identities = [s for s in snapshots if s['event'] == 'snapshot_identity']
                if not identities or identities[-1]['pid'] != launch['pid'] or identities[-1]['module_base'] != proc.base:
                    raise ValueError('stack snapshot identity mismatch')
                stacks = {s['thread']: s for s in snapshots if s['event'] == 'thread_snapshot'}
                tuples = {}
                for thread, snapshot in stacks.items():
                    if time.monotonic() >= deadline:
                        break
                    lower, upper = int(snapshot['stack_limit'],16), int(snapshot['stack_base'],16)
                    if not 0 < upper-lower <= 8*1024**2:
                        continue
                    data = proc.read(lower, upper-lower)
                    if data is None:
                        continue
                    for candidate in candidates:
                        needle = struct.pack('<Q',int(candidate['object'],16))
                        at = data.find(needle)
                        while at >= 0:
                            address = lower+at-24
                            query = proc.read(address,80)
                            if query:
                                pointers = struct.unpack('<10Q',query)
                                root = transform(proc.read(pointers[2],56))
                                eye = transform(proc.read(pointers[6],56))
                                current = camera(proc.read(pointers[3]+0x60,64))
                                if root and eye and current and pointers[2] != pointers[6] and \
                                    .95 < root['quaternion_norm'] < 1.05 and .95 < eye['quaternion_norm'] < 1.05 and \
                                    all(abs(x-1)<.01 for x in root['scale']+eye['scale']) and \
                                    sum((x-y)**2 for x,y in zip(root['position'],eye['position'])) < 100 and \
                                    sum((x-y)**2 for x,y in zip(current['position'],eye['position'])) < .01:
                                    tuples[(pointers[2],pointers[3],pointers[5],pointers[6])] = dict(
                                        thread=thread, tuple_address=hex(address), pointers=[hex(x) for x in pointers],
                                        root=root, camera_transform=eye, client_camera=current)
                            at = data.find(needle,at+1)
                report = dict(pid=launch['pid'],base=hex(proc.base),launch_started=launch['started'],
                              candidates=candidates,query_candidates=list(tuples.values()))
                output.write_text(json.dumps(report,indent=2,allow_nan=False),encoding='utf8')
                print('Query tuple candidates:',len(tuples),'Report:',output)
                for row in tuples.values():
                    print('Root',row['pointers'][2],row['root']['position'],'Camera',row['pointers'][6])
                return
            for sample in range(a.samples):
                if time.monotonic() >= deadline:
                    break
                for candidate in candidates:
                    address = int(candidate['object'], 16)
                    valid_anchor = proc.read(address+0x28, 8) == anchor
                    value = camera(proc.read(address+0x60, 64)) if valid_anchor else None
                    roots=[]
                    for query in prior.get('query_candidates',[]):
                        if int(query['pointers'][3],16)==address:
                            root=transform(proc.read(int(query['pointers'][2],16),56))
                            roots.append(dict(address=query['pointers'][2],transform=root))
                    rows.append(dict(sample=sample, time_ns=time.monotonic_ns(), object=hex(address), camera=value,roots=roots))
                time.sleep(.02)
            report = dict(pid=launch['pid'], base=hex(proc.base), launch_started=launch['started'], samples=rows)
        else:
            scanned = 0
            hits = 0
            candidates = []
            finished = True
            # Heap in the recorded dump was in this reservation. This only prioritizes scan order;
            # every other eligible private region is still considered within explicit time/byte bounds.
            regions = sorted(proc.regions(), key=lambda r: (not 0x30000000000 <= r[0] < 0x40000000000, r[0]))
            for base, size in regions:
                if scanned >= a.max_mib*1024**2 or time.monotonic() >= deadline:
                    finished = False
                    break
                for offset in range(0, size, 1024**2):
                    if scanned >= a.max_mib*1024**2 or time.monotonic() >= deadline:
                        finished = False
                        break
                    length = min(1024**2+7, size-offset, a.max_mib*1024**2-scanned)
                    data = proc.read(base+offset, length)
                    scanned += length
                    if data is None:
                        continue
                    at = data.find(anchor)
                    while at >= 0:
                        hits += 1
                        address = base+offset+at-0x28
                        value = camera(proc.read(address+0x60, 64))
                        if value and address % 8 == 0:
                            candidates.append(dict(object=hex(address), camera_address=hex(address+0x60), camera=value))
                        at = data.find(anchor, at+1)
            report = dict(pid=launch['pid'], base=hex(proc.base), launch_started=launch['started'],
                          anchor_rva=hex(pointer_rva), scanned_bytes=scanned, scan_completed=finished,
                          pointer_hits=hits, candidates=candidates)
        output.write_text(json.dumps(report, indent=2, allow_nan=False), encoding='utf8')
        print('Observation:', output)
        print({key: value for key, value in report.items() if key not in ('samples', 'candidates')})
        if 'candidates' in report:
            print('Camera candidates:', len(report['candidates']))
            for candidate in report['candidates'][:10]:
                print(candidate)
        else:
            print('Samples:', len(report['samples']), 'valid:', sum(row['camera'] is not None for row in report['samples']))
    finally:
        proc.close()


if __name__ == '__main__':
    main()
