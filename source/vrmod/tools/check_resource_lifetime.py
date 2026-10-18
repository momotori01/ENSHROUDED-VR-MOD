"""Correlate a bounded native allocation/import probe with actual Vulkan cleanup.

Unused images only: does not prove synchronization of rendered eye images.
Matches generations and timestamps because driver handles are recycled.
"""
import argparse
import json
import pathlib


def read(path):
    return [json.loads(line) for line in path.read_text(encoding='utf-8-sig').splitlines() if line.strip()]


def analyze(probe, gpu):
    created = [e for e in probe if e['event'] == 'offscreen_target_created']
    completed = [e for e in probe if e['event'] == 'offscreen_target_probe_complete']
    restored = [e for e in probe if e['event'] == 'render_resource_hook_complete']
    imported = [e for e in probe if e['event'] == 'offscreen_external_import']
    alias_done = [e for e in probe if e['event'] == 'offscreen_external_release_complete']
    checks = dict(two_distinct_unused_targets=len(created) == 2 and
                  len({e['image'] for e in created}) == 2 and all(e['rendered'] is False for e in created),
                  native_handles_and_source_preserved=len(completed) == 1 and all(
                      completed[0].get(k) is True for k in ['distinct_images', 'handles_released', 'source_unchanged']),
                  graph_free_count_restored=len(completed) == 1 and completed[0]['free_before'] == completed[0]['free_after'],
                  instructions_restored=len(restored) == 1 and restored[0]['original_bytes_restored'] and restored[0]['result'] == 0)
    if imported:
        checks['two_external_aliases'] = len(imported) == 2 and len({e['index'] for e in imported}) == 2 and all(
            e['flags'] & 0x04000000 and not e['flags'] & 0x02000000 for e in imported)
        checks['external_release_preserves_owner'] = len(alias_done) == 1 and alias_done[0]['owned_textures_unchanged'] and alias_done[0]['aliases_cleared']
    details = []
    for target in created:
        slot = target['slot']
        # The native factory finishes before the probe records the returned handle.
        begins = [e for e in gpu if e['event'] == 'render_image' and e.get('image') == target['image'] and
                  e.get('result') == 0 and e['us'] <= target['us']]
        begin = max(begins, key=lambda e: e['us']) if begins else None
        checks[f'slot_{slot}_native_vk_creation'] = begin is not None and target['us'] - begin['us'] < 1000000
        ends = [e for e in gpu if e['event'] == 'render_image_destroyed' and e.get('image') == target['image'] and begin and
                e.get('device') == begin.get('device') and e.get('generation') == begin.get('generation') and e['us'] >= target['us']]
        end = ends[0] if len(ends) == 1 else None
        checks[f'slot_{slot}_vk_image_destroyed_once'] = end is not None
        views = [e for e in gpu if e['event'] == 'image_view_destroyed' and e.get('view') == target['view'] and
                 target['us'] <= e['us'] and end and e['us'] <= end['us']]
        checks[f'slot_{slot}_view_destroyed_before_image'] = len(views) == 1
        boundary = alias_done[0]['us'] if alias_done else target['us']
        checks[f'slot_{slot}_cleanup_after_alias_release'] = end is not None and end['us'] > boundary and all(e['us'] > boundary for e in views)
        checks[f'slot_{slot}_bounded_cleanup'] = end is not None and 0 <= end['us'] - target['us'] < 2000000
        details.append(dict(slot=slot, image=target['image'], generation=begin.get('generation') if begin else None,
                            cleanup_delay_us=end['us'] - target['us'] if end else None,
                            destruction_stack=end.get('game_rva_stack') if end else None))
    return dict(checks={k: 'PASS' if v else 'FAIL' for k, v in checks.items()}, targets=details, limit=__doc__)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('probe', type=pathlib.Path)
    parser.add_argument('--negative-checks', action='store_true')
    args = parser.parse_args()
    probe = read(args.probe/'render-resource-probe.jsonl')
    gpu = read(args.probe.parent/'vulkan.jsonl')
    result = analyze(probe, gpu)
    if args.negative_checks:
        missing = analyze(probe, [e for e in gpu if e['event'] != 'render_image_destroyed'])
        wrong_generation = analyze(probe, [dict(e, generation=-1) if e['event'] == 'render_image_destroyed' else e for e in gpu])
        result['checks']['reject_missing_destruction'] = 'PASS' if 'FAIL' in missing['checks'].values() else 'FAIL'
        result['checks']['reject_reused_wrong_generation'] = 'PASS' if 'FAIL' in wrong_generation['checks'].values() else 'FAIL'
    (args.probe/'resource-lifetime-check.json').write_text(json.dumps(result, indent=2), encoding='utf8')
    for name, status in result['checks'].items(): print(status, name)
    print('Targets:', result['targets'])
    return int('FAIL' in result['checks'].values())


if __name__ == '__main__':
    raise SystemExit(main())
