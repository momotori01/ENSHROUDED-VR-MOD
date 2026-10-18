"""Corrupt copies of real item-observer evidence; never changes the source log."""
import argparse,copy,json,pathlib
from check_input_direction import check_input_direction

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('probe',type=pathlib.Path);a=p.parse_args()
    source=[json.loads(line) for line in (a.probe/'view-resource-probe.jsonl').read_text().splitlines()]
    baseline,_=check_input_direction(source)
    if not all(baseline.values()):raise ValueError('negative controls require a passing source')
    cases=[('catalog_pointer','input_item_catalog_entry',lambda e:e.update(pointer_matches=False),'input_item_catalog_identity_and_types'),
           ('catalog_classification','input_item_catalog_entry',lambda e:e['metadata'].update(ranged_weapon=not e['metadata']['ranged_weapon']),'input_item_catalog_identity_and_types'),
           ('catalog_mutation','input_item_catalog',lambda e:e.update(stable_header=False),'input_item_catalog_read_only_stable'),
           ('catalog_count','input_item_catalog',lambda e:e.update(entries=e['entries']+1),'input_item_catalog_read_only_stable'),
           ('catalog_locality','input_item_catalog_entry',lambda e:e.update(entity=e['entity']+1),'input_item_catalog_identity_and_types'),
           ('selected_identity','input_ranged_sample',lambda e:e['weapon_instance'].update(item_id=123),'input_item_selected_identity_and_types')]
    results=[]
    for name,event,mutate,check in cases:
        rows=copy.deepcopy(source);mutate(next(e for e in rows if e['event']==event));checks,_=check_input_direction(rows)
        results.append(dict(case=name,expected_failed_check=check,status='PASS' if not checks[check] else 'FAIL'))
    output=pathlib.Path('docs/analysis/item_metadata_negative_controls.json')
    output.write_text(json.dumps(dict(source=str(a.probe),source_unchanged=True,cases=results),indent=2)+'\n')
    print(sum(r['status']=='PASS' for r in results),'PASS /',sum(r['status']=='FAIL' for r in results),'FAIL');return int(any(r['status']!='PASS' for r in results))
if __name__=='__main__':raise SystemExit(main())
