"""Summarize dynamic-rendering targets and bindings in a bounded Vulkan trace.

This is not a complete render graph: compute writes, aliasing, image transitions,
and classic render passes are outside this report. Rendering area is not extent.
"""
import argparse
import collections
import hashlib
import json
import pathlib
import xml.etree.ElementTree as ET


def analyze(events, formats):
    scopes = []
    active = {}
    bound = {}
    images = {}
    for event in events:
        kind = event.get('event')
        command = event.get('cmd')
        if kind == 'render_image' and event.get('image') and event.get('result') == 0:
            images[event['image']] = [event['width'], event['height']]
        elif kind == 'render_image_destroyed':
            images.pop(event['image'], None)
        elif kind == 'trace_rendering_attachments':
            row = dict(event, completed=False, bound_pipeline_at_begin=bound.get(command), graphics_bindings_seen=[])
            row['known_image_area_checks'] = []
            for attachment in [*row['colors'], row.get('depth'), row.get('stencil')]:
                if not attachment: continue
                target = attachment['target']
                if target.get('image') not in images: continue  # WSI images need a separate source of creation metadata.
                extent = [max(1, value >> target['base_mip']) for value in images[target['image']]]
                fits = all(o >= 0 and o + size <= limit for o, size, limit in zip(row['offset'], [row['width'], row['height']], extent))
                row['known_image_area_checks'].append(dict(image=target['image'], mip_extent=extent, fits=fits))
            active[command] = row
            scopes.append(row)
        elif kind == 'trace_rendering_end':
            row = active.pop(command, None)
            if row: row['completed'] = True
        elif kind == 'trace_pipeline' and event['bind_point'] == 0:
            bound[command] = event['pipeline']
            if command in active: active[command]['graphics_bindings_seen'].append(event['pipeline'])
    def targets(row):
        return [a['target'] for a in [*row['colors'], row.get('depth'), row.get('stencil')] if a and a['target']['view'] != '0x0']
    known = [t for row in scopes for t in targets(row)]
    checks = dict(dynamic_rendering_observed=bool(scopes),
                  nonempty_render_areas=bool(scopes) and all(row['width'] > 0 and row['height'] > 0 for row in scopes),
                  attachment_metadata_complete=bool(known) and all('image' in t and 'generation' in t for t in known),
                  attachment_arrays_not_truncated=bool(scopes) and all(not row['truncated'] for row in scopes),
                  completed_scopes_observed=any(row['completed'] for row in scopes))
    area_checks = [check for row in scopes for check in row['known_image_area_checks']]
    if area_checks: checks['known_image_render_areas_fit'] = all(check['fits'] for check in area_checks)
    groups = collections.Counter()
    for row in scopes:
        color = tuple(a['target'].get('format') for a in row['colors'])
        depth = row['depth']['target'].get('format') if row.get('depth') else None
        groups[(row['width'], row['height'], color, depth)] += 1
    return dict(checks={k: 'PASS' if v else 'FAIL' for k, v in checks.items()}, scope_count=len(scopes),
                completed_scope_count=sum(row['completed'] for row in scopes),
                known_image_area_check_count=len(area_checks),
                image_count=len({t.get('image') for t in known}),
                configurations=[dict(render_area=[w, h], color_formats=[formats.get(f, str(f)) for f in color],
                                     depth_format=formats.get(depth, str(depth)) if depth else None, observations=count)
                                for (w, h, color, depth), count in groups.most_common()], scopes=scopes,
                limits=[__doc__, 'Bindings seen within a scope are not proof of draws or a shader pass name.',
                        'A bounded capture may start/end within a scope; incomplete edges are retained, not counted as crashes.',
                        'No eye, frame, image ownership, or game pass identity is inferred from dimensions alone.'])


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('trace', type=pathlib.Path)
    p.add_argument('--output', type=pathlib.Path)
    args = p.parse_args()
    registry = ET.parse('references/Vulkan-Headers/registry/vk.xml')
    formats = {int(e.get('value')): e.get('name') for e in registry.findall("./enums[@name='VkFormat']/enum") if e.get('value')}
    raw = args.trace.read_bytes()
    report = analyze([json.loads(line) for line in raw.decode('utf-8-sig').splitlines() if line.strip()], formats)
    report.update(trace=str(args.trace.resolve()), sha256=hashlib.sha256(raw).hexdigest())
    output = args.output or args.trace.with_name('attachment-analysis.json')
    output.write_text(json.dumps(report, indent=2), encoding='utf8')
    for name, status in report['checks'].items(): print(status, name)
    print('Scopes:', report['scope_count'], 'complete:', report['completed_scope_count'], 'images:', report['image_count'])
    for row in report['configurations']: print(row)
    return int(any(s == 'FAIL' for s in report['checks'].values()))


if __name__ == '__main__':
    raise SystemExit(main())
