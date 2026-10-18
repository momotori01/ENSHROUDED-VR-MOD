"""Read native allocator vtables from one live, identity-checked View observation.

No game memory writes, remote calls, thread suspension or allocator execution.
Function RVAs are local-build evidence; this does not prove allocation semantics.
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct
from read_game_memory import Process
from inject_camera_observer import creation_filetime


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('probe', type=Path)
    a = p.parse_args()
    launch = json.loads((a.probe.parent/'launch.json').read_text(encoding='utf-8-sig'))
    events = [json.loads(s) for s in (a.probe/'view-resource-probe.jsonl').read_text().splitlines() if s.strip()]
    views = [e for e in events if e['event'] == 'native_view_created']
    if not views or len({e['scene'] for e in views}) != 1 or len({e['original_view'] for e in views}) != 1:
        raise ValueError('one observed Scene and original View required')
    game = Path(__file__).resolve().parents[2]/'enshrouded.exe'
    expected = 'af2f5a1227911d8aa06b3908d6bd0211838211cae14ea91099cb57d0df990781'
    if hashlib.sha256(game.read_bytes()).hexdigest() != expected:
        raise ValueError('unsupported game executable')
    process = Process(launch['pid'], game)
    try:
        if creation_filetime(process.handle) != int(launch['creation_filetime']):
            raise ValueError('managed PID has been reused')
        def read(address, size):
            result = process.read(address, size)
            if result is None:
                raise ValueError(f'unreadable metadata {address:x}')
            return result
        def q(address):
            return struct.unpack('<Q', read(address, 8))[0]
        scene = int(views[0]['scene'], 16)
        original = int(views[0]['original_view'], 16)
        if q(scene+0x28) != original or q(original+8) != scene:
            raise ValueError('observed View ownership changed')
        renderer = q(scene+8)
        detail = q(renderer+0x14398)
        allocators = [('renderer', q(renderer))]
        if detail:
            allocators.append(('detail_manager', q(detail)))
        records = []
        for name, allocator in allocators:
            table = q(allocator)
            functions = []
            for offset in (0, 8, 16, 24):
                address = q(table+offset)
                rva = address-process.base
                functions.append(dict(slot=hex(offset), address=hex(address),
                    rva=hex(rva) if 0 <= rva < 47869952 else None,
                    bytes=read(address, 32).hex() if 0 <= rva < 47869952 else None))
            records.append(dict(kind=name, allocator=hex(allocator), vtable=hex(table), functions=functions))
        if q(scene+8) != renderer or q(scene+0x28) != original:
            raise ValueError('Scene ownership changed during reads')
        report = dict(pid=launch['pid'], creation_filetime=launch['creation_filetime'], base=hex(process.base),
            scene=hex(scene), renderer=hex(renderer), detail_manager=hex(detail), allocators=records, limitation=__doc__)
        output = a.probe/'allocator-metadata.json'
        output.write_text(json.dumps(report, indent=2)+'\n', encoding='utf8')
        print(json.dumps(report, indent=2))
    finally:
        process.close()


if __name__ == '__main__':
    main()
