"""Read-only attachment observations. Does not certify visibility or every item."""
import argparse,json,pathlib

def check(p):
 rows=[json.loads(s) for s in (p/'view-resource-probe.jsonl').read_text(encoding='utf-8').splitlines()]
 events=lambda n:[x for x in rows if x['event']==n]
 roots={x['entity'] for x in events('root_camera_sample')}
 on=events('attachment_observer_enabled');off=events('attachment_observer_removed')
 items=events('attachment_visual_item');children=events('attachment_child');models=events('attachment_model')
 gliders={x['entity'] for x in items if x['item_slot']==35}
 checks=dict(explicit_readonly_policy=(p/'world-attachments-observe.txt').read_text().strip()=='EVR_ATTACHMENTS_OBSERVE_1',
  enabled_once=len(on)==1 and on[0]['read_only'],
  no_observation_failure=not events('attachment_observer_failure'),
  shared_hook_restored=len(off)==1 and off[0]['restored'] and off[0]['callbacks']==0 and off[0]['failures']==0,
  original_parent_bound=bool(children) and all(x['parent'] in roots and x['visual_equipment_query'] and x['read_only'] for x in children),
  metadata_classified_visual_entities=bool(items) and all(x['item_id'] and 0<=x['item_slot']<=40 and x['parent'] in roots and x['read_only'] for x in items),
  actual_child_models=bool(models) and all(x['handle'] and x['parent'] in roots and 0<=x['parent_age_ms']<=1000 and x['read_only'] for x in models),
  glider_model_independently_bound=bool(gliders) and any(x['entity'] in gliders for x in models))
 report=dict(checks=checks,visual_items=items,children=children,models=models,passed=sum(checks.values()),failed=sum(not v for v in checks.values()),limitations=__doc__)
 (p/'attachment-check.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
 for k,v in checks.items():print('PASS' if v else 'FAIL',k)
 print(report['passed'],'PASS /',report['failed'],'FAIL')
 return all(checks.values())
if __name__=='__main__':
 a=argparse.ArgumentParser(description=__doc__);a.add_argument('probe',type=pathlib.Path);v=a.parse_args();raise SystemExit(not check(v.probe))
