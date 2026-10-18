"""Read RGB9E5 GI through its captured sampled view, preserving raw evidence.

The underlying image can be R32_UINT for shader writes. Its integer word value
is not a radiance. Require a same-event R9G9B9E5_FLOAT sampled descriptor before
decoding, and record the original manifest and binding paths in the report.
"""
import argparse
import json
import math
from pathlib import Path
import struct


def decode(word):
    scale = math.ldexp(1.0, (word >> 27) - 24)
    return tuple(((word >> shift) & 511) * scale for shift in (0, 9, 18))


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('raw', type=Path)
    p.add_argument('bindings', type=Path)
    a = p.parse_args()
    bindings = list(map(json.loads, a.bindings.read_text().splitlines()))
    output = []
    for record in map(json.loads, (a.raw / 'texels.jsonl').read_text().splitlines()):
        matches = [b for b in bindings if b['event'] == record['event'] and
                   b['texture'] == record['texture'] and b['descriptor_type'] == 4 and
                   b['format'] == 'R9G9B9E5_FLOAT']
        if not matches:
            continue
        if record['format'] not in ('R32_UINT', 'R9G9B9E5_FLOAT') or record['depth'] != 1:
            raise ValueError('unsupported underlying GI format/extent')
        pixels = record['width'] * record['height']
        data = (a.raw / record['file']).read_bytes()
        if len(data) != pixels * 4 or len(data) != record['bytes']:
            raise ValueError('GI extent does not match manifest')
        stride = max(1, math.ceil(pixels / 65536))
        values = [decode(struct.unpack_from('<I', data, i * 4)[0]) for i in range(0, pixels, stride)]
        stats = []
        for channel in zip(*values):
            channel = sorted(channel)
            stats.append(dict(mean=math.fsum(channel) / len(channel), minimum=channel[0],
                              maximum=channel[-1], median=channel[len(channel)//2],
                              zero_fraction=channel.count(0.0)/len(channel)))
        result = dict(event=record['event'], texture=record['texture'], sampled=len(values),
                      stride=stride, channels=stats, sampled_view=matches[0],
                      limitation='Different cameras; statistics alone do not prove a lighting bug.')
        output.append(result)
        print(json.dumps(result))
    if not output:
        raise ValueError('no sampled RGB9E5 view matches raw events')
    (a.raw / 'gi-statistics.json').write_text(json.dumps(dict(raw=str(a.raw), bindings=str(a.bindings),
                                                           results=output), indent=2))


if __name__ == '__main__':
    main()
