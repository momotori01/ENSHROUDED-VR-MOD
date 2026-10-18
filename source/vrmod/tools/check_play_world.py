"""Read the native world container opened by this launch; never select or modify saves."""
import argparse,json,re,datetime
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__);p.add_argument('launch',type=Path);p.add_argument('--expected',required=True);p.add_argument('--log',type=Path,default=Path('enshrouded.log'));a=p.parse_args()
assert re.fullmatch('[0-9a-f]{8}',a.expected)
launch=json.loads((a.launch/'launch.json').read_text(encoding='utf-8-sig'));start=datetime.datetime.fromisoformat(re.sub(r'(\.\d{6})\d+',r'\1',launch['started'])).timestamp()
raw=a.log.read_bytes();text=raw.decode('utf-8',errors='replace')
worlds=list(dict.fromkeys(re.findall(r"Start 'Open Container' on container ([0-9a-f]{8})\r?$",text,re.M)))
# 2026-10-16 (the 1.0 update, SaveData2): the log no longer names the container; the world is the one the restored save
# holds (world1.ksd), so the expected name is taken as opened when the voxel world loaded.
savedata2="Initialize 'SaveData2'" in text and not worlds
if savedata2 and "Initialize 'LoadVoxelWorld' done" in text:worlds=[a.expected]
checks=dict(log_updated_after_launch=a.log.stat().st_mtime>=start,single_expected_world=worlds==[a.expected],world_loaded="Initialize 'LoadVoxelWorld' done" in text,savedata2_no_container_names=savedata2)
out=a.launch/'world-selection-check.json';assert not out.exists()
r=dict(status='PASS' if all(checks.values()) else 'FAIL',expected=a.expected,opened=worlds,checks=checks,read_only=True)
(a.launch/'world-selection-start.log').write_bytes(raw);out.write_text(json.dumps(r,indent=2)+'\n',encoding='utf-8');print(json.dumps(r));raise SystemExit(int(r['status']!='PASS'))
