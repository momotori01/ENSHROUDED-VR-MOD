"""Decode observed query snapshots without accessing or modifying the running game."""
import argparse
import json
import pathlib
import struct
from analyze_camera_dump import transform
from read_game_memory import camera


def main():
    p=argparse.ArgumentParser()
    p.add_argument('--launch',type=pathlib.Path,required=True)
    p.add_argument('--observation',type=pathlib.Path,required=True)
    a=p.parse_args()
    launch=json.loads((a.launch/'launch.json').read_text(encoding='utf-8-sig'))
    audit=json.loads((a.observation/'loader-audit.json').read_text())
    if audit['pid']!=launch['pid']:
        raise ValueError('observation launch mismatch')
    events=[json.loads(x) for x in (a.observation/'camera-observer.jsonl').read_text().splitlines()]
    complete=[e for e in events if e['event']=='camera_hook_complete']
    if not complete or not complete[-1]['original_bytes_restored']:
        raise ValueError('observation did not verify original-byte restoration')
    rows=[]
    for event in events:
        if event['event']!='camera_query_sample':continue
        if not event['query_read'] or not all(c['readable'] for c in event['components']):
            raise ValueError('incomplete observed query')
        raw=[bytes.fromhex(c['bytes']) for c in event['components']]
        root=transform(raw[2]);eye=transform(raw[6]);client=camera(raw[3][0x60:0xa0])
        if not root or not eye or not client:raise ValueError('invalid observed camera/root')
        controller_root=struct.unpack_from('<3q',raw[5],0x40)
        rows.append(dict(sample=event['sample'],tick_ms=event['tick_ms'],thread=event['thread'],
                         pointers=[c['address'] for c in event['components']],root=root,camera_transform=eye,
                         client_camera=client,root_ticks=struct.unpack_from('<2I',raw[2],168),
                         root_source=transform(raw[2][56:112]),root_target=transform(raw[2][112:168]),
                         task_context=event.get('task_context'),
                         controller_root_position_q32=controller_root,
                         controller_minus_root=[(c-r)/2**32 for c,r in zip(controller_root,root['position_q32'])]))
    if not rows:raise ValueError('no camera samples')
    first=rows[0];last=rows[-1]
    summary=dict(samples=len(rows),duration_ms=last['tick_ms']-first['tick_ms'],
        threads=sorted({r['thread'] for r in rows}),root_addresses=sorted({r['pointers'][2] for r in rows}),
        camera_addresses=sorted({r['pointers'][6] for r in rows}),
        root_position_range=[max(r['root']['position'][i] for r in rows)-min(r['root']['position'][i] for r in rows) for i in range(3)],
        camera_position_range=[max(r['camera_transform']['position'][i] for r in rows)-min(r['camera_transform']['position'][i] for r in rows) for i in range(3)],
        camera_matches_client=all(r['camera_transform']['position_q32']==r['client_camera']['position_q32'] and
            r['camera_transform']['orientation']==r['client_camera']['orientation'] for r in rows),
        controller_root_max_error=max(abs(v) for r in rows for v in r['controller_minus_root']),
        original_bytes_restored=True)
    contexts=[r['task_context'] for r in rows if r['task_context'] is not None]
    summary['context_samples']=len(contexts)
    summary['valid_local_context_samples']=sum(c['valid'] and c['local_entity'] for c in contexts)
    summary['context_coverage']='unavailable_in_old_observer' if not contexts else 'recorded'
    report=dict(summary=summary,observations=rows)
    (a.observation/'analysis.json').write_text(json.dumps(report,indent=2,allow_nan=False),encoding='utf8')
    print(json.dumps(summary,indent=2))
    # Resampling file uses current process identity from the scanner; no old-process addresses are reused.
    scans=sorted(a.launch.glob('memory-observation-*.json'))
    matching=[]
    for scan in scans:
        data=json.loads(scan.read_text())
        if data.get('pid')==launch['pid'] and data.get('launch_started')==launch['started'] and 'base' in data:
            matching.append(data)
    if matching:
        live=dict(pid=launch['pid'],base=matching[-1]['base'],launch_started=launch['started'],
            candidates=[dict(object=first['pointers'][3],camera_address=hex(int(first['pointers'][3],16)+0x60),camera=first['client_camera'])],
            query_candidates=[first])
        (a.observation/'live-candidates.json').write_text(json.dumps(live,indent=2),encoding='utf8')


if __name__=='__main__':main()
