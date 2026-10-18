"""Audit the opt-in local sun-shadow shader load; visual quality is separate."""
import argparse,json,hashlib
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__);p.add_argument('launch',type=Path);a=p.parse_args()
launch=json.loads((a.launch/'launch.json').read_text(encoding='utf-8-sig'))
manifest=launch.get('shadow_receiver_shader') or {}
rows=[]
for line in (a.launch/'vulkan.jsonl').open(encoding='utf-8-sig'):
 e=json.loads(line)
 if e.get('event','').startswith('shadow_receiver_shader_'):rows.append(e)
patches=[e for e in rows if e['event']=='shadow_receiver_shader_patched']
checks=dict(explicit_option=bool(manifest),reviewed_source=manifest.get('source_sha256') in ('e0d6de57a4db980fcdcfd7ba61640f0db21be193ef57ffde91b43dd6efca2427','bb450d00434dca964a841dcc9995582282ce69bf49260e6d5e153ba87483177b'),history_retained=manifest.get('history_reset') is False,plane_enabled=manifest.get('control') is False,no_load_failure=not any(e['event']=='shadow_receiver_shader_failed' for e in rows),shader_loaded=bool(patches),expected_bytecode=bool(patches) and all(e.get('shader_sha256')==manifest.get('shader_sha256') and e.get('source_sha256')==manifest.get('source_sha256') for e in patches))
r=dict(status='PASS' if all(checks.values()) else 'FAIL',checks=checks,records=rows,limitation='Shader identity/load only. All views use the reviewed shader. Requires separate image inspection.')
(a.launch/'shadow-receiver-check.json').write_text(json.dumps(r,indent=2)+'\n',encoding='utf-8')
print(json.dumps(r,indent=2));raise SystemExit(int(r['status']!='PASS'))
