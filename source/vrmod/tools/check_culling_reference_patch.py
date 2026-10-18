"""Independent GPU output comparison for the bounded offline culling experiment."""
import argparse
import bisect
from collections import Counter
import hashlib
import json
from pathlib import Path
import struct


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('directory', type=Path)
    p.add_argument('--recipe', required=True, type=Path)
    p.add_argument('--equipment-parts',type=int,nargs='+',default=[0,1,2,3,15],help='Explicit observed parts for an offline body ablation; default remains head parts')
    p.add_argument('--equipment-probe', type=Path, help='Optional observation from the same game launch as the capture')
    p.add_argument('--expect-no-change', action='store_true', help='Negative control: this view must be outside the scoped replacement')
    p.add_argument('--baseline-is-replacement',action='store_true',help='Captured live shader already hides heads: replacement is the stock shader reference')
    a = p.parse_args()
    recipe = json.loads(a.recipe.read_text())
    ids = set(recipe['indices'])
    parts=set(a.equipment_parts)
    if not parts or not parts.issubset(range(24)):raise ValueError('equipment part bounds')
    checks = {}
    checks['reviewed_original_shader'] = hashlib.sha256((a.directory/'original.spv').read_bytes()).hexdigest() == recipe['original_sha256']
    if a.baseline_is_replacement:
        checks['reviewed_stock_reference_shader']=hashlib.sha256((a.directory/'replacement.spv').read_bytes()).hexdigest()==recipe['replacement_sha256']
    raw = {case: {(row['type'],row['index']):(a.directory/row['file']).read_bytes()
                  for row in map(json.loads,(a.directory/'buffers.jsonl').read_text().splitlines()) if row['case']==case}
           for case in ('before','after','restored')}
    const = raw['before'][(1,0)]
    checks['native_parameter_layout'] = len(const)==4144
    if not checks['native_parameter_layout']:
        raise ValueError('unknown culling constant layout')
    subviews, passes = struct.unpack_from('<2I',const,4064)
    cpu_count = struct.unpack_from('<I',const,4084)[0]
    checks['bounded_native_counts'] = 0<subviews<=11 and 0<passes<=12 and 0<cpu_count<=65536 and ids.issubset(range(cpu_count))
    if not checks['bounded_native_counts']:
        raise ValueError('unsupported culling counts')
    pass_records = [struct.unpack_from('<4I',const,3872+i*16) for i in range(passes)]
    bases = struct.unpack('<'+'I'*passes,raw['before'][(9,7)])
    checks['ordered_pass_bucket_ranges'] = list(bases)==sorted(set(bases)) and bases[0]==0
    if not checks['ordered_pass_bucket_ranges']:
        raise ValueError('ambiguous culling pass bucket ranges')
    def pass_for(row):
        return bisect.bisect_right(bases,row[0]>>16)-1
    def main_view(row):
        return pass_records[pass_for(row)][1] == recipe['subview']
    counters, counts = {}, {}
    for case, data in raw.items():
        counts[case] = struct.unpack_from('<I',data[(9,0)])[0]
        output = data[(9,9)]
        if counts[case]*8>len(output):
            raise ValueError('GPU output exceeds capacity')
        counters[case] = Counter(struct.iter_unpack('<II',output[:counts[case]*8]))
    before, after, restored = (counters[x] for x in ('before','after','restored'))
    output_restored=before==restored
    if a.baseline_is_replacement:before,after=after,before
    removed, added = before-after, after-before
    if a.expect_no_change:
        checks['excluded_view_all_rows_unchanged'] = before==after
    else:
        checks['selected_main_view_rows_removed'] = bool(removed) and all(row[1] in ids and main_view(row) for row in removed)
    checks['no_unexpected_rows_added'] = not added
    checks['all_other_instances_unchanged'] = Counter({r:n for r,n in before.items() if r[1] not in ids})==Counter({r:n for r,n in after.items() if r[1] not in ids})
    checks['all_other_subviews_unchanged'] = Counter({r:n for r,n in before.items() if not main_view(r)})==Counter({r:n for r,n in after.items() if not main_view(r)})
    shadows = Counter({r:n for r,n in before.items() if r[1] in ids and pass_records[pass_for(r)][2]==2})
    checks['selected_shadow_rows_retained'] = bool(shadows) and shadows==Counter({r:n for r,n in after.items() if r[1] in ids and pass_records[pass_for(r)][2]==2})
    checks['replacement_removal_restores_output'] = output_restored
    checks['native_constants_unchanged'] = all(raw[c][(1,0)]==const for c in raw)
    checks['all_cpu_instance_input_records_unchanged'] = all(raw[c][(9,2)][:cpu_count*132]==raw['before'][(9,2)][:cpu_count*132] for c in raw)
    checks['model_mesh_and_pass_tables_unchanged'] = all(raw[c][(9,i)]==raw['before'][(9,i)] for c in raw for i in (1,5,6,7))
    if a.equipment_probe:
        # A same-launch path plus GPU model index comparison gives independent
        # correspondence. It does not prove future handle lifetimes/equipment swaps.
        probe=a.equipment_probe.resolve()
        checks['equipment_capture_same_launch'] = probe.parent==a.directory.resolve().parent.parent
        observations=[r for r in map(json.loads,(probe/'view-resource-probe.jsonl').read_text().splitlines()) if r['event']=='local_equipment_observation']
        checks['equipment_native_observations_present'] = len(observations)==20 and len({r['entity'] for r in observations})==1
        target_ids={m['handle']&0xffffff for r in observations for m in r['models'] if m['part'] in parts and m['handle']}
        checks['recipe_uses_only_observed_selected_parts'] = target_ids==ids
        checks['body_recipe_parts_match_selection'] = recipe.get('parts',sorted(parts))==sorted(parts)
        matched=True
        for r in observations:
            for m in r['models']:
                if not m['handle']:
                    continue
                index=m['handle']&0xffffff
                if index>=cpu_count or 'shader_model_index' not in m or m.get('model_ready')!=1:
                    matched=False
                    continue
                model,mask=struct.unpack_from('<II',raw['before'][(9,2)],index*132+124)
                matched &= model==m['shader_model_index'] and mask==m['mask']
        checks['selected_and_other_gpu_models_match_native'] = matched
    report = dict(checks=checks,counts=counts,recipe=recipe,baseline_is_replacement=a.baseline_is_replacement,pass_records=pass_records,bucket_bases=bases,
                  removed=[dict(mesh=r[0]&65535,bucket=r[0]>>16,instance=r[1],count=n) for r,n in removed.items()],
                  selected_shadow_rows=sum(shadows.values()),
                  limitation='Offline capture; selected IDs are test indices, not proof of current player identity. GPU-generated tail records can vary across replay and are not byte-compared. No live game integration.')
    (a.directory/'checks.json').write_text(json.dumps(report,indent=2))
    for name, ok in checks.items():
        print(('PASS' if ok else 'FAIL'),name)
    print(f'RESULT {sum(checks.values())} PASS {len(checks)-sum(checks.values())} FAIL')
    return 0 if all(checks.values()) else 1


if __name__=='__main__':
    raise SystemExit(main())
