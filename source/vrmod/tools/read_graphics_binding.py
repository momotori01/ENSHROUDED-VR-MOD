"""Read bounded Vulkan backend metadata from a fresh managed render observation.

No writes, queue locks, native function calls, or XR session creation.
"""
import argparse, hashlib, json, struct, time
from pathlib import Path
from inject_camera_observer import Process, creation_filetime


def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--launch',required=True,type=Path);p.add_argument('--observation',required=True,type=Path);a=p.parse_args()
    root=Path(__file__).resolve().parents[2];game=root/'enshrouded.exe'
    if hashlib.sha256(game.read_bytes()).hexdigest()!='af2f5a1227911d8aa06b3908d6bd0211838211cae14ea91099cb57d0df990781':raise ValueError('unsupported game build')
    launch=json.loads((a.launch/'launch.json').read_text(encoding='utf-8-sig'))
    audit=json.loads((a.observation/'loader-audit.json').read_text())
    if audit['pid']!=launch['pid'] or int(audit['expected_creation_filetime'])!=int(launch['creation_filetime']):raise ValueError('observation process identity differs')
    if time.time()-(a.launch/'vulkan.jsonl').stat().st_mtime>10:raise ValueError('stale render telemetry')
    samples=[json.loads(s) for s in (a.observation/'render-observer.jsonl').read_text().splitlines()]
    sample=next(e for e in samples if e['event']=='render_query_sample');scene=int(sample['components'][5]['address'],16)
    proc=Process(launch['pid'],game)
    try:
        if creation_filetime(proc.handle)!=int(launch['creation_filetime']):raise ValueError('PID reused')
        def read(address,size):
            data=proc.read(address,size)
            if data is None:raise ValueError('unreadable bounded graphics metadata')
            return data
        def ptr(address):return struct.unpack('<Q',read(address,8))[0]
        view=int(sample['components'][2]['address'],16)
        if ptr(view+8)!=scene or ptr(scene+0x28)!=view:raise ValueError('Scene/View changed since observation')
        renderer=ptr(scene+8);graphics=ptr(renderer+0x10);backend=ptr(graphics+0x278)
        context=ptr(backend+0x1118)
        data={'backend':(backend,read(backend,0x1140)),'platform':(context,read(context,0xe10))}
        log=[]
        for s in (a.launch/'vulkan.jsonl').read_text().splitlines():
            try:log.append(json.loads(s))
            except json.JSONDecodeError:pass
        handles={int(e[k],16):k for e in log for k in ('instance','device','physical_device','queue') if k in e and isinstance(e[k],str)}
        result=dict(scene=hex(scene),view=hex(view),renderer=hex(renderer),graphics=hex(graphics),backend=hex(backend),platform=hex(context),matches=[])
        for label,(address,raw) in data.items():
            (a.observation/(label+'-binding.bin')).write_bytes(raw)
            for offset in range(0,len(raw)-7,8):
                value=struct.unpack_from('<Q',raw,offset)[0]
                if value in handles:result['matches'].append(dict(structure=label,offset=hex(offset),value=hex(value),kind=handles[value]))
        result['platform_queue_header']={hex(i):hex(struct.unpack_from('<Q',data['platform'][1],i)[0]) for i in range(0xd80,0xe10,8)}
        result['backend_context_header']={hex(i):hex(struct.unpack_from('<Q',data['backend'][1],i)[0]) for i in range(0x10f0,0x1128,8)}
        queue_table=ptr(context+0xd98);queue_count=ptr(context+0xda0)
        if queue_count<1 or queue_count>16:raise ValueError('queue array count outside diagnostic bound')
        queues=read(queue_table,queue_count*16);(a.observation/'queue-binding.bin').write_bytes(queues)
        result['queues']=[dict(index=i,handle=hex(struct.unpack_from('<Q',queues,i*16)[0]),lock=struct.unpack_from('<I',queues,i*16+8)[0]) for i in range(queue_count)]
        result['queue_table']=hex(queue_table)
        (a.observation/'graphics-binding.json').write_text(json.dumps(result,indent=2));print(json.dumps(result,indent=2))
    finally:proc.close()


if __name__=='__main__':main()
