"""Read a local Windows minidump; no game process access and no dump upload.

Only the validated build's exact camera-query instruction is decoded as a query.
Pointer-slot identities are observations/candidates, never permission to write memory.
"""
import argparse
import hashlib
import json
import math
import pathlib
import struct


class Dump:
    def __init__(self, path):
        self.data = path.read_bytes()
        magic, _, count, directory = self.unpack('<IIII', 0)
        if magic != 0x504D444D:
            raise ValueError('not a Windows minidump')
        self.streams = {t: (size, offset) for t, size, offset in
                        (self.unpack('<III', directory + i * 12) for i in range(count)) if t}
        self.modules = []
        _, start = self.streams[4]
        for i in range(self.unpack('<I', start)[0]):
            base, size, _, timestamp, name = self.unpack('<QIIII', start + 4 + 108 * i)
            length = self.unpack('<I', name)[0]
            self.modules.append(dict(base=base, size=size, timestamp=timestamp,
                                     name=self.data[name + 4:name + 4 + length].decode('utf-16-le')))
        self.memory = []
        if 5 in self.streams:
            _, start = self.streams[5]
            for i in range(self.unpack('<I', start)[0]):
                address, size, offset = self.unpack('<QII', start + 4 + 16 * i)
                self.memory.append((address, size, offset))
        if 9 in self.streams:
            _, start = self.streams[9]
            count, offset = self.unpack('<QQ', start)
            for i in range(count):
                address, size = self.unpack('<QQ', start + 16 + 16 * i)
                self.memory.append((address, size, offset))
                offset += size

    def unpack(self, fmt, offset):
        return struct.unpack_from(fmt, self.data, offset)

    def read(self, address, size):
        parts = []
        while size:
            region = next(((b, n, o) for b, n, o in self.memory if b <= address < b + n), None)
            if region is None:
                return None
            base, length, offset = region
            count = min(size, base + length - address)
            parts.append(self.data[offset + address - base:offset + address - base + count])
            address += count
            size -= count
        return b''.join(parts)


def transform(data):
    if data is None or len(data) < 52:
        return None
    position = struct.unpack_from('<3q', data)
    quaternion = struct.unpack_from('<4f', data, 24)
    scale = struct.unpack_from('<3f', data, 40)
    if not all(math.isfinite(x) for x in quaternion + scale):
        return None
    return dict(position_q32=position, position=[x / 2**32 for x in position],
                orientation=quaternion, quaternion_norm=math.sqrt(sum(x*x for x in quaternion)), scale=scale)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('dump', type=pathlib.Path)
    parser.add_argument('--output', type=pathlib.Path, required=True)
    args = parser.parse_args()
    dump = Dump(args.dump)
    _, offset = dump.streams[6]
    thread, _, code, flags, _, address, count, _ = dump.unpack('<IIIIQQII', offset)
    context_size, context_offset = dump.unpack('<II', offset + 160)
    if context_size < 256:
        raise ValueError('x64 context not present')
    registers = {name: dump.unpack('<Q', context_offset + pos)[0] for name, pos in
                 [('dr0', 72), ('dr6', 104), ('dr7', 112), ('rcx', 128), ('rsp', 152), ('rbp', 160), ('rip', 248)]}
    module = next((m for m in dump.modules if m['base'] <= address < m['base'] + m['size']), None)
    report = dict(dump=str(args.dump), dump_sha256=hashlib.sha256(dump.data).hexdigest(), thread=thread,
                  exception_code=hex(code), exception_flags=flags, exception_address=hex(address),
                  parameters=[hex(x) for x in dump.unpack('<15Q', offset + 40)[:count]],
                  module=module, registers={k: hex(v) for k, v in registers.items()})
    mod = pathlib.Path(__file__).resolve().parents[1]
    profile = json.loads((mod / 'config/profiles/build23966345.json').read_text())
    executable = mod.parent / 'enshrouded.exe'
    supported = (module and pathlib.PureWindowsPath(module['name']).name.lower() == 'enshrouded.exe'
                 and module['size'] == 47869952 and module['timestamp'] == 1782724296
                 and hashlib.sha256(executable.read_bytes()).hexdigest() == profile['exe_sha256']
                 and address - module['base'] == int(profile['camera_probe']['rva'], 16))
    report['camera_query_location_matches'] = bool(supported)
    if supported:
        query = dump.read(registers['rsp'] + 0x50, 80)
        report['query'] = []
        if query:
            pointers = struct.unpack('<10Q', query)
            for index, pointer in enumerate(pointers):
                data = dump.read(pointer, 192)
                report['query'].append(dict(index=index, address=hex(pointer), captured=data is not None,
                                            bytes=data.hex() if data else None))
            root = transform(dump.read(pointers[2], 56))
            camera = transform(dump.read(pointers[6], 56))
            report['render_transform_candidate'] = root
            report['camera_transform_candidate'] = camera
            if root and camera:
                report['camera_minus_root'] = [(c-r) / 2**32 for c, r in
                    zip(camera['position_q32'], root['position_q32'])]
                copies = [transform(dump.read(pointers[2] + offset, 56)) for offset in (0, 56, 112)]
                report['render_transform_triplet'] = copies
                report['render_transform_copies_agree'] = copies[0] == copies[1] == copies[2]
            client_camera = dump.read(pointers[3] + 0x60, 64)
            if client_camera:
                report['local_data_client_camera_candidate'] = {
                    'offset': '0x60', 'position': [x/2**32 for x in struct.unpack_from('<3q', client_camera)],
                    'orientation': struct.unpack_from('<4f', client_camera, 24),
                    **dict(zip(['distance','fov_y','aspect_ratio','near','far'], struct.unpack_from('<5f',client_camera,40)))}
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, indent=2, allow_nan=False), encoding='utf8')
    print('Exception:', hex(code), 'module RVA:', hex(address-module['base']) if module else 'unknown')
    print('Matched camera query:', bool(supported))
    if 'query' in report:
        print('Captured query slots:', sum(x['captured'] for x in report['query']))
        print('Root:', report['render_transform_candidate'])
        print('Camera:', report['camera_transform_candidate'])
        print('Camera minus root:', report.get('camera_minus_root'))
        print('Client camera:', report.get('local_data_client_camera_candidate'))
    print('Report:', args.output)


if __name__ == '__main__':
    main()
