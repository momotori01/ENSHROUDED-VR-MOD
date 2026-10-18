"""Check captured native DeferredLighting GPU inputs from one replay controller.

This proves descriptor identity and captured light records, not VFX frustum
coverage, light counter correctness, gameplay quality, or eye comfort.
Independent RenderDoc event replay can reorder atomic append output: retain raw
hashes and compare the multiset of complete 64-byte records (duplicates retained).
"""
import argparse
import hashlib
import json
from pathlib import Path

DEFERRED_SHA='684d03635113ff1c33ee3a72620536fd56ef3cb517c638be0003533a0b1c9991'


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('bindings',type=Path)
    p.add_argument('events',type=int,nargs=3,help='Original, left, right DeferredLighting event IDs')
    a=p.parse_args();rows=[json.loads(s) for s in a.bindings.read_text().splitlines()]
    records=[];checks={}
    for label,event in zip(('original','left','right'),a.events):
        shader=a.bindings.parent/f'event_{event}_stage_5.spv'
        checks[label+'_native_shader']=hashlib.sha256(shader.read_bytes()).hexdigest()==DEFERRED_SHA
        matches=[e for e in rows if e['event']==event and e['stage']==5 and e['category']=='resource' and e['index']==3 and e['descriptor_type']==9]
        if len(matches)!=1:raise ValueError('expected one native point-light descriptor per event')
        entry=matches[0];data=(a.bindings.parent/entry['file']).read_bytes()
        if not data or len(data)!=entry['size'] or len(data)%64 or len(data)>64*65536:
            raise ValueError('invalid point-light snapshot extent')
        chunks=[data[i:i+64] for i in range(0,len(data),64)]
        checks[label+'_nonzero_light_records']=any(any(c) for c in chunks)
        records.append(dict(label=label,event=event,buffer=entry['buffer'],offset=entry['offset'],size=entry['size'],
                            raw_sha256=hashlib.sha256(data).hexdigest(),nonzero_records=sum(any(c) for c in chunks),
                            record_multiset_sha256=hashlib.sha256(b''.join(sorted(chunks))).hexdigest()))
    checks['same_light_buffer_range']=len({(r['buffer'],r['offset'],r['size']) for r in records})==1
    checks['same_light_record_multiset']=len({r['record_multiset_sha256'] for r in records})==1
    report=dict(checks={k:'PASS' if v else 'FAIL' for k,v in checks.items()},records=records,limitation=__doc__)
    (a.bindings.parent/'shared-point-light-checks.json').write_text(json.dumps(report,indent=2))
    for k,v in report['checks'].items():print(v,k)
    print('PASS',sum(checks.values()),'FAIL',len(checks)-sum(checks.values()))
    return int(not all(checks.values()))


if __name__=='__main__':raise SystemExit(main())
