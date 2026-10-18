"""Audit live render-only weapon switches and Scene-owned clone disposal."""
import argparse,collections,json
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__);p.add_argument('probe',type=Path);a=p.parse_args()
e=[json.loads(x) for x in (a.probe/'view-resource-probe.jsonl').read_text().splitlines() if x.strip()]
created=[x for x in e if x['event']=='motion_visual_created'];poses=[x for x in e if x['event']=='motion_visual_pose']
disposed=[x for x in e if x['event']=='motion_visual_disposed'];switches=[x for x in e if x['event']=='motion_visual_equipment_switch']
owned={x['handle']:x['item'] for x in created};dc=collections.Counter(x['handle'] for x in disposed)
checks={
 'multiple_items_created':len(set(owned.values()))>=2,
 'unique_clone_handles':len(owned)==len(created),
 'resource_references_acquired':all(x['references_after']==x['references_before']+1 for x in created),
 'originals_unchanged':bool(poses) and all(x['source_unchanged'] for x in poses),
 'poses_match_owned_item':all(owned.get(x['handle'])==x['item'] for x in poses),
 'only_private_visibility':all(x['native_mask']==0 and x['sdf_handle']==0 for x in poses),
 'return_reuses_clone':any(len({x['source_handle'] for x in poses if x['handle']==h})>1 for h in owned) or any(x['reused'] and x['next_handle'] in owned for x in switches),
 'every_clone_disposed_once':bool(owned) and set(dc)==set(owned) and all(n==1 for n in dc.values()),
 'disposal_verified':all(x['generation_invalidated'] and x['reference_released'] and x['scene_disposal'] for x in disposed),
 'no_failure':not any('failure' in x['event'] for x in e),
 'hook_completed':any(x['event']=='view_resource_hook_complete' and x['result']==0 for x in e),
}
r=dict(checks={k:'PASS' if v else 'FAIL' for k,v in checks.items()},status='PASS' if all(checks.values()) else 'FAIL',created=created,switches=switches,disposed=disposed,pose_counts=dict(collections.Counter(x['item'] for x in poses)),dormant_reasons=dict(collections.Counter(x['reason'] for x in e if x['event']=='motion_visual_dormant')),limitations='Native display/lifetime evidence only. Does not establish grip calibration, bow animation, VFX relocation or combat support.')
(a.probe/'live-weapon-cycle-check.json').write_text(json.dumps(r,indent=2)+'\n')
print(json.dumps({k:v for k,v in r.items() if k not in ('created','switches','disposed')},indent=2))
raise SystemExit(not all(checks.values()))
