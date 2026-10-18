"""Validate bounded native game calibration, not a playable OpenXR integration.

CPU evidence covers every pair; GPU evidence covers three captured frames only.
Image changes are consistent with commanded motion, not proof isolating motion
from game animation. Resource usage traversal is conservative (see imported tool).
"""
import argparse
import collections
import json
import math
from pathlib import Path
from check_native_calibration import evidence, Image, ImageChops, ImageStat
from export_recorded_xr import read_recording


def rows(path):
    return [json.loads(s) for s in path.read_text(encoding='utf-8-sig').splitlines() if s.strip()]


def lifetime(probe, gpu):
    names = [e for e in gpu if e['event'] == 'calibration_image_named']
    disposal = [e for e in probe if e['event'] == 'calibration_scene_disposal_complete']
    checks = {'two_named_generations': len(names) == 2 and
              {e['name'] for e in names} == {'EVR calibration eye 0', 'EVR calibration eye 1'} and
              len({(e['device'], e['generation']) for e in names}) == 2 and all(e['generation'] for e in names),
              'native_scene_disposal': len(disposal) == 1 and disposal[0]['views_destroyed'] == 2 and
              disposal[0]['owned_groups_cleared'] and disposal[0]['original_view_selected']}
    details = []
    for i, name in enumerate(names):
        key = tuple(name[k] for k in ('device', 'image', 'generation'))
        matching = [e for e in gpu if tuple(e.get(k) for k in ('device', 'image', 'generation')) == key]
        created = [e for e in matching if e['event'] == 'render_image' and e.get('result') == 0 and e['us'] < name['us']]
        destroyed = [e for e in matching if e['event'] == 'render_image_destroyed' and e['us'] > name['us']]
        device_end = [e for e in gpu if e['event'] == 'destroy_device' and e['device'] == name['device']]
        checks[f'eye{i}_created_once'] = len(created) == 1
        checks[f'eye{i}_destroyed_once_after_scene_before_device'] = (len(destroyed) == len(device_end) == len(disposal) == 1 and
                disposal[0]['us'] < destroyed[0]['us'] < device_end[0]['us'])
        details.append(dict(name=name, destroyed=destroyed))
    return checks, details


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('probe', type=Path)
    args = parser.parse_args()
    probe = rows(args.probe/'view-resource-probe.jsonl')
    gpu = rows(args.probe.parent/'vulkan.jsonl')
    checks, cleanup = lifetime(probe, gpu)
    pairs = [e for e in probe if e['event'] == 'calibration_pair_recorded']
    transforms = [e for e in probe if e['event'] == 'calibration_eye_transform']
    records = [e for e in probe if e['event'] == 'calibration_eye_recorded']
    begins = [e for e in probe if e['event'] == 'calibration_eye_begin']
    serials = [e['serial'] for e in pairs]
    checks['300_consecutive_pairs'] = (len(pairs) == 300 and [e['pair'] for e in pairs] == list(range(1, 301)) and
                                       serials == list(range(serials[0], serials[0]+300)))
    by_serial = collections.defaultdict(list)
    for t in transforms:
        by_serial[t['serial']].append(t)
    checks['each_pair_two_eye_transforms'] = len(transforms) == 600 and set(by_serial) == set(serials) and all(
        len(v) == 2 and {t['eye'] for t in v} == {0, 1} for v in by_serial.values())
    checks['balanced_300_render_calls'] = collections.Counter(e['eye'] for e in records) == {0: 300, 1: 300}
    checks['same_native_serial_per_pair'] = all(e['serial'] == e['view_serial'] for e in records) and collections.Counter(
        (e['serial'], e['eye']) for e in records) == collections.Counter((s, eye) for s in serials for eye in (0, 1))
    checks['two_views_and_images_reused'] = len(begins) == 600 and all(
        len({e[field] for e in begins}) == 2 and all(len({e[field] for e in begins if e['eye'] == eye}) == 1 for eye in (0, 1))
        for field in ('view', 'image'))
    checks['only_two_view_allocations'] = sum(e['event'] == 'native_view_created' for e in probe) == 2
    checks['history_valid_and_pingpong_advances'] = all(e['history_valid'] == 1 for e in records) and all(
        [e['pingpong'] for e in records if e['eye'] == eye] == [1-i % 2 for i in range(300)] for eye in (0, 1))
    separations = [math.dist(a['camera_q32'], b['camera_q32'])/4294967296 for v in by_serial.values() if len(v) == 2 for a, b in [v]]
    if (args.probe/'recorded-xr.txt').exists():
        # Runtime/recorded eyes can have arbitrary IPD. Compare the actual source,
        # including its logging precision, rather than a rounded nominal 64 mm.
        source_frames=read_recording(args.probe/'recorded-xr.txt')[3]
        applied={e['serial']:e['recorded_xr_frame'] for e in probe if e['event']=='recorded_xr_eye_applied'}
        checks['recorded_eye_separation_preserved'] = len(separations)==len(source_frames)==300 and all(
            abs(separation-math.dist(source_frames[applied[serial]][1][0][:3],source_frames[applied[serial]][1][1][:3]))<1e-6
            for serial,separation in zip(by_serial,separations))
    else:
        checks['64mm_baseline_all_pairs'] = len(separations) == 300 and all(abs(s-.064) < 1e-6 for s in separations)
    checks['finite_canonical_matrices'] = len(transforms) == 600 and all(
        all(math.isfinite(v) for v in t['view_matrix']+t['projection_matrix']) and
        [t['view_matrix'][i] for i in (3, 7, 11, 15)] == [0, 0, 0, 1] for t in transforms)
    checks['position_and_rotation_changed'] = all(
        len({tuple(t[field][:12] if field == 'view_matrix' else t[field]) for t in transforms if t['eye'] == eye}) > 290
        for eye in (0, 1) for field in ('camera_q32', 'view_matrix'))
    checks['render_hook_restored'] = any(e['event'] == 'view_resource_hook_complete' and e['original_bytes_restored'] and e['result'] == 0 for e in probe)
    checks['scene_hook_disabled'] = any(e['event'] == 'calibration_scene_hook_removed' and e['status'] == 0 for e in probe)
    checks['no_forced_frame_retirement'] = not any(e['event'] == 'calibration_gpu_retired' for e in probe) and any(
        e['event'] == 'calibration_scene_disposal_begin' and e['frame_idle_forced'] is False for e in probe)
    checks['postcheck_alive_and_rendering'] = json.loads((args.probe/'probe-postcheck.json').read_text())['status'] == 'PASS'
    graphs = {}; images = {}
    for label in ('first', 'quarter', 'final'):
        directory = args.probe/('replay-'+label)
        graph = evidence(rows(directory/'textures.jsonl'), rows(directory/'actions.jsonl'), rows(directory/'usage.jsonl'))
        graphs[label] = graph
        # Visibility may differ between eyes; equal draw counts are not a stereo correctness invariant.
        checks[label+'_distinct_geometry_branches'] = (len(set(graph.get('indices', []))) == 2 and
            all(graph.get('geometry_events', [])) and len(graph.get('geometry_events', [])) == 2 and
            set(graph['geometry_events'][0]).isdisjoint(graph['geometry_events'][1]) and
            len(set(graph.get('output_writers', []))) == 2)
        if not checks[label+'_distinct_geometry_branches']:
            continue
        images[label] = [Image.open(directory/f'texture_{index}.png').convert('RGB') for index in graph['indices']]
        checks[label+'_two_nonblack_nonuniform_images'] = all(sum(ImageStat.Stat(i).mean) > 9 and sum(ImageStat.Stat(i).var) > 12 for i in images[label])
        checks[label+'_same_extents'] = images[label][0].size == images[label][1].size
        checks[label+'_eyes_differ'] = sum(ImageStat.Stat(ImageChops.difference(*images[label])).mean) > 0
    changes = {}
    for eye in (0, 1):
        for a, b in (('first', 'quarter'), ('quarter', 'final')):
            name = f'eye{eye}_{a}_to_{b}_changes'
            if a in images and b in images and images[a][eye].size == images[b][eye].size:
                changes[name] = ImageStat.Stat(ImageChops.difference(images[a][eye], images[b][eye])).mean
                checks[name] = sum(changes[name]) > 3
            else:
                checks[name] = False
    for label, modified in [('missing_destruction', [e for e in gpu if e['event'] != 'render_image_destroyed']),
                            ('wrong_generation', [dict(e, generation=-1) if e['event'] == 'render_image_destroyed' else e for e in gpu])]:
        checks['reject_'+label] = not all(lifetime(probe, modified)[0].values())
    result = dict(checks={k: 'PASS' if v else 'FAIL' for k, v in checks.items()}, cleanup=cleanup,
                  gpu_graphs=graphs, pixel_changes=changes, limits=__doc__)
    (args.probe/'native-series-checks.json').write_text(json.dumps(result, indent=2))
    for k, v in result['checks'].items(): print(v, k)
    return int(not all(checks.values()))


if __name__ == '__main__':
    raise SystemExit(main())
