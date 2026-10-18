"""Read-only numeric inspection of evr_renderdoc texels exports; no image adjustment.

Sampling covers a deterministic stride through the complete image. Statistics are
sample estimates, not full-image extrema or a visual-quality PASS certificate.
"""
import argparse
import json
import math
import struct
from pathlib import Path


def summarize(directory, budget=65536):
    records = []
    for line in (directory / 'texels.jsonl').read_text().splitlines():
        record = json.loads(line)
        components = record['components']
        size = record['component_bytes']
        kind = record['component_type']
        if record.get('packed_32'):
            raise ValueError('packed words require an explicit decoder: ' + record['format'])
        if kind == 8 and record['format'] == 'D32' and size == 4 and components == 1:
            code, divisor = 'f', 1
        elif kind == 1 and size in (2, 4):
            code, divisor = {2: 'e', 4: 'f'}[size], 1
        elif kind == 2 and size in (1, 2):
            code, divisor = {1: 'B', 2: 'H'}[size], (1 << (8 * size)) - 1
        elif kind == 3 and size in (1, 2):
            code, divisor = {1: 'b', 2: 'h'}[size], (1 << (8 * size - 1)) - 1
        elif kind == 4 and size in (1, 2, 4):
            code, divisor = {1: 'B', 2: 'H', 4: 'I'}[size], 1
        else:
            raise ValueError('unsupported numeric format: ' + record['format'])
        pixels = record['width'] * record['height'] * record['depth']
        data = (directory / record['file']).read_bytes()
        unpack = struct.Struct('<' + code * components)
        if len(data) != pixels * unpack.size or len(data) != record['bytes']:
            raise ValueError('raw data extent differs from manifest')
        stride = max(1, math.ceil(pixels / budget))
        channels = [[] for _ in range(components)]
        invalid = [0] * components
        for index in range(0, pixels, stride):
            for channel, value in enumerate(unpack.unpack_from(data, index * unpack.size)):
                if math.isfinite(value):
                    normalized = value / divisor
                    channels[channel].append(max(-1, normalized) if kind == 3 else normalized)
                else:
                    invalid[channel] += 1
        stats = []
        for channel, values in enumerate(channels):
            values.sort()
            stats.append(dict(invalid=invalid[channel], finite=len(values),
                mean=math.fsum(values) / len(values) if values else None,
                min=values[0] if values else None, max=values[-1] if values else None,
                percentiles={str(p): values[round((len(values)-1)*p/100)] if values else None
                             for p in (1, 10, 50, 90, 99)}))
        records.append(dict(**record, stride=stride, sampled=math.ceil(pixels/stride),
                            sample_estimates=stats))
    return records


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('directory', type=Path)
    args = parser.parse_args()
    result = summarize(args.directory)
    (args.directory / 'statistics.json').write_text(json.dumps(result, indent=2))
    for record in result:
        print(json.dumps(dict(event=record['event'], texture=record['texture'],
            samples=record['sampled'], means=[s['mean'] for s in record['sample_estimates']],
            medians=[s['percentiles']['50'] for s in record['sample_estimates']],
            alpha=record['sample_estimates'][3] if record['components']==4 else None)))
