#!/usr/bin/env python3
"""Read the render objects' memory that EVR_LAYOUT_DUMP=1 wrote (layout.json and layout-*.bin in the probe folder of a
Mock run, hooks/view_resource_probe.cpp namespace layoutDump) and check the 1.0 structure layout (namespace layout) on
it: the back-pointers that must agree, the camera arguments' shape (a Q32 position, a unit quaternion, near/far), the
Vulkan handles' copies. Read only.

    layout_dump_report.py <probe folder> [--scan view|owner|scene|parameters]

--scan lists every 8-byte field of a region that holds another dumped object's address or a heap-looking pointer, and
every float that looks like a quaternion component group, to find a moved field.
"""
import argparse, json, math, pathlib, struct


def load(folder):
    index = json.loads((folder / 'layout.json').read_text())
    regions = {}
    for name, info in index.items():
        data = (folder / f'layout-{name}.bin').read_bytes()
        regions[name] = (int(info['address'], 16), data)
    return regions


def q(r, name, off):
    a, d = r[name]
    return struct.unpack_from('<Q', d, off)[0] if off + 8 <= len(d) else None


def u32(r, name, off):
    return struct.unpack_from('<I', r[name][1], off)[0]


def f32(r, name, off):
    return struct.unpack_from('<f', r[name][1], off)[0]


def u8(r, name, off):
    return r[name][1][off]


def check(label, ok, detail=''):
    print(f'  [{"ok" if ok else "NO"}] {label} {detail}')


def report(r):
    view, _ = r['view']; owner, _ = r['owner']; scene, _ = r['scene']
    print(f'view {view:x} owner {owner:x} scene {scene:x} renderer {r["renderer-head"][0]:x}')
    print('View')
    check('View+0 = owner', q(r, 'view', 0) == owner)
    check('View+8 = scene', q(r, 'view', 8) == scene)
    print(f'    features View+0x10 = {u32(r, "view", 0x10):#x}  owner+0x10 = {u32(r, "owner", 0x10):#x}')
    check('View+0x240 = View+0x268', q(r, 'view', 0x240) == view + 0x268, f'{q(r, "view", 0x240):#x}')
    print(f'    config View+0xc4..0xd8 = {r["view"][1][0xc4:0xd8].hex(" ")}')
    print(f'    serial View+0x8c8 = {q(r, "view", 0x8c8)}  frame serial (context+8)+0x18 = {q(r, "context-frame", 0x18)}')
    print(f'    ping-pong View+0x850 = {u32(r, "view", 0x850)}  ready View+0x980 = {u8(r, "view", 0x980)}  last position valid +0x970 = {u8(r, "view", 0x970)}')
    vfx_scene = q(r, 'scene', 0x950)
    check('View+0x9a8 (VFX View) = VFX scene+0x80', q(r, 'view', 0x9a8) == vfx_scene + 0x80, f'{q(r, "view", 0x9a8):#x} vs {vfx_scene:#x}+0x80')
    print(f'    upscaler kind View+0xf38 = {u8(r, "view", 0xf38)}  FSR3 +0xf40 = {q(r, "view", 0xf40):#x}  DLSS +0xf48 = {q(r, "view", 0xf48):#x}  +0xf50 = {q(r, "view", 0xf50):#x}  +0xf58 = {q(r, "view", 0xf58):#x}')
    for name, off in (('fog', 0xb40), ('water', 0xb48), ('GI', 0xed0), ('light cache', 0xee0), ('SSR 0', 0xf18), ('SSR 1', 0xf20),
                      ('post', 0x1120), ('depth of field', 0x1130), ('streaming', 0x978), ('lights', 0xf98)):
        print(f'    {name:>15} View+{off:#x} = {q(r, "view", off):#x}')
    print(f'    lights count +0xfa0 = {q(r, "view", 0xfa0)} capacity +0xfa8 = {q(r, "view", 0xfa8)}')
    print(f'    depth of field request View+0x6c = {u8(r, "view", 0x6c)}  history valid +0x1110 = {u8(r, "view", 0x1110)}')
    print(f'    projection View+0x908 = {r["view"][1][0x908:0x958].hex()}')
    print('Owner')
    check('owner+0 = renderer', q(r, 'owner', 0) == r['renderer-head'][0])
    check('owner+0x98 = scene', q(r, 'owner', 0x98) == scene, f'{q(r, "owner", 0x98):#x}')
    check('owner+0xa0 = View', q(r, 'owner', 0xa0) == view, f'{q(r, "owner", 0xa0):#x}')
    print(f'    size owner+0x1c,0x20 = {u32(r, "owner", 0x1c)} x {u32(r, "owner", 0x20)}  render scale +0x24 = {f32(r, "owner", 0x24):.3f}  +0x18 = {u32(r, "owner", 0x18)}')
    print(f'    pools +0xa8.. = {[u32(r, "owner", 0xa8 + 4 * k) for k in range(4)]}  +0xb8 = {q(r, "owner", 0xb8):#x}')
    print(f'    marker +0xd8 = {q(r, "owner", 0xd8):#x} group +0xe0 = {u32(r, "owner", 0xe0)}  resolution +0xe8 = {q(r, "owner", 0xe8):#x} group +0xf0 = {u32(r, "owner", 0xf0)}')
    print('Scene')
    check('scene+0x18 = renderer', q(r, 'scene', 0x18) == r['renderer-head'][0])
    check('scene+0x40 = View', q(r, 'scene', 0x40) == view, f'{q(r, "scene", 0x40):#x}')
    print(f'    disabled scene+0x2c = {u8(r, "scene", 0x2c)}  VFX scene +0x950 = {vfx_scene:#x}  shared +0x9d0 {q(r, "scene", 0x9d0):#x} +0xa80 {q(r, "scene", 0xa80):#x} +0xa88 {q(r, "scene", 0xa88):#x}')
    print('Renderer')
    rh = 'renderer-head'
    print(f'    graphics +0x120 = {q(r, rh, 0x120):#x} (dumped {r["graphics"][0]:#x})  builder +0x128 = {q(r, rh, 0x128):#x}  memory +0x160 = {q(r, rh, 0x160):#x}  +0x168 = {q(r, rh, 0x168):#x}')
    rt = 'renderer-tail'
    print(f'    SSR +0x16548 = {q(r, rt, 0x548):#x} +0x16550 = {q(r, rt, 0x550):#x}  +0x16480 = {q(r, rt, 0x480):#x}  views +0x16c94 = {u32(r, rt, 0xc94)} / {u32(r, rt, 0xc90)}')
    print(f'    pools renderer+0x278 = {u32(r, rh, 0x278)} +0x27c = {u32(r, rh, 0x27c)}')
    print('Backend (graphics+0x278)')
    b = 'backend'
    check('VkDevice +0x2d8 = +0x1100', q(r, b, 0x2d8) == q(r, b, 0x1100), f'{q(r, b, 0x2d8):#x} / {q(r, b, 0x1100):#x}')
    check('VkPhysicalDevice +0x2d0 = +0x1108', q(r, b, 0x2d0) == q(r, b, 0x1108), f'{q(r, b, 0x2d0):#x} / {q(r, b, 0x1108):#x}')
    print(f'    instance +0x2c8 = {q(r, b, 0x2c8):#x}  platform +0x1118 = {q(r, b, 0x1118):#x}')
    print('Target and graph')
    print(f'    target graph {q(r, "target", 0):#x} index {struct.unpack_from("<H", r["target"][1], 8)[0]} +0x10 = {u32(r, "target", 0x10)}')
    g = 'graph'
    print(f'    graph +0x68 textures {q(r, g, 0x68):#x} +0x80 table {q(r, g, 0x80):#x} +0x88 capacity {q(r, g, 0x88)} +0xe0 {q(r, g, 0xe0)}')
    print('Camera arguments')
    p = r['parameters'][1]
    pos = struct.unpack_from('<3q', p, 0x38); quat = struct.unpack_from('<4f', p, 0x50)
    print(f'    +0x38 position q32 = {[x / 2 ** 32 for x in pos]}')
    check('+0x50 unit quaternion', abs(math.sqrt(sum(c * c for c in quat)) - 1) < 1e-3, f'{quat}')
    print(f'    +0x70 near = {f32(r, "parameters", 0x70)}  +0x74 far = {f32(r, "parameters", 0x74)}  +0x78 fov = {f32(r, "parameters", 0x78)}  +0x7c byte = {p[0x7c]}')
    print(f'    post +0x250 = {[round(x, 4) for x in struct.unpack_from("<12f", p, 0x250)]}')
    print(f'    sharpness +0x308 = {[round(x, 4) for x in struct.unpack_from("<3f", p, 0x308)]}')
    print('FSR3 (View+0xf40)')
    if 'fsr' in r and r['fsr'][0]:
        f = r['fsr'][1]
        print(f'    program +0x20 = {struct.unpack_from("<I", f, 0x20)[0]}  handles: ' +
              ' '.join(f'{0x28 + 16 * n:#x}' for n in range(59) if struct.unpack_from('<Q', f, 0x28 + 16 * n)[0]))


def scan(r, name):
    base, d = r[name]
    addresses = {n: a for n, (a, _) in r.items() if a}
    for off in range(0, len(d) - 7, 8):
        v = struct.unpack_from('<Q', d, off)[0]
        hits = [n for n, a in addresses.items() if v == a]
        near = [f'{n}+{v - a:#x}' for n, a in addresses.items() if a < v < a + len(r[n][1]) and v != a]
        if hits or near:
            print(f'  {name}+{off:#x} = {v:#x}  {" ".join(hits)} {" ".join(near)}')
    for off in range(0, len(d) - 15, 4):
        c = struct.unpack_from('<4f', d, off)
        if all(math.isfinite(x) for x in c) and abs(sum(x * x for x in c) - 1) < 1e-4 and sum(abs(x) > 1e-6 for x in c) >= 2:
            print(f'  {name}+{off:#x} quaternion-like {tuple(round(x, 4) for x in c)}')


if __name__ == '__main__':
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('folder'); ap.add_argument('--scan', action='append', default=[])
    a = ap.parse_args()
    regions = load(pathlib.Path(a.folder))
    report(regions)
    for name in a.scan:
        print(f'scan {name}'); scan(regions, name)
