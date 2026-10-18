"""Read retained world View/streaming state from a validated managed diagnostic.

No native calls, writes, suspension, or process-wide scan. Snapshots are live,
non-atomic observations; only the three previously logged View pointers are read.
"""
import argparse
import json
import struct
import collections
import time
from pathlib import Path
from inject_camera_observer import Process, creation_filetime


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('probe',type=Path)
    p.add_argument('--entries',action='store_true',help='Read bounded native entry trees (maximum 32768 entries per View)')
    a=p.parse_args()
    launch=json.loads((a.probe.parent/'launch.json').read_text(encoding='utf-8-sig'))
    rows=[json.loads(line) for line in (a.probe/'view-resource-probe.jsonl').read_text().splitlines()]
    states={e['eye']:e for e in rows if e['event']=='world_pipeline_state'}
    if set(states)!={-1,0,1}:raise ValueError('three known world Views required')
    if any(e['event'].startswith('calibration_scene_dispos') for e in rows):raise ValueError('Scene disposal has started')
    out=a.probe/(time.strftime('streaming-entries-%Y%m%d-%H%M%S') if a.entries else 'streaming-state')
    out.mkdir(exist_ok=False)
    proc=Process(launch['pid'],Path(__file__).resolve().parents[2]/'enshrouded.exe')
    try:
        if creation_filetime(proc.handle)!=int(launch['creation_filetime']):raise ValueError('PID reused')
        result=[]
        for eye,state in sorted(states.items()):
            view=int(state['view'],16)
            data=proc.read(view,0xbc0)
            if data is None:raise ValueError('View unreadable')
            streaming=struct.unpack_from('<Q',data,0x710)[0]
            if not streaming or streaming!=int(state['view_streaming'],16):raise ValueError('streaming owner changed')
            stream=proc.read(streaming,0xc1d0)
            if stream is None:raise ValueError('streaming object unreadable')
            (out/f'eye_{eye}_view.bin').write_bytes(data)
            (out/f'eye_{eye}_streaming.bin').write_bytes(stream)
            if a.entries:
                queue=[(*struct.unpack_from('<QQ',stream,0x130),0)]
                chunks=[];seen=set();entries=[]
                while queue:
                    address,count,depth=queue.pop()
                    if not count:continue
                    if not address or address in seen or depth>12 or count+len(entries)>32768:
                        raise ValueError('entry tree bound or identity invalid')
                    seen.add(address)
                    block=proc.read(address,count*0x68)
                    if block is None:raise ValueError('entry array unreadable')
                    chunks.append(dict(address=hex(address),count=count,depth=depth,file=f'eye_{eye}_entries_{len(chunks)}.bin'))
                    (out/chunks[-1]['file']).write_bytes(block)
                    for index in range(count):
                        item=block[index*0x68:(index+1)*0x68]
                        entries.append(dict(address=hex(address+index*0x68),depth=depth,
                            job=hex(struct.unpack_from('<Q',item)[0]),ready=item[0x22],response=item[0x23],
                            state=struct.unpack_from('<I',item,0x28)[0],level=struct.unpack_from('<I',item,0x48)[0],
                            pending=list(struct.unpack_from('<III',item,0x50))))
                        child,children=struct.unpack_from('<QQ',item,0x10)
                        if children:queue.append((child,children,depth+1))
                entry_result=dict(non_atomic=True,chunks=chunks,entries=entries,
                    states=dict(collections.Counter(e['state'] for e in entries)),
                    pending_handles=sum(bool(v) for e in entries for v in e['pending']))
                manager=struct.unpack_from('<Q',stream,0x118)[0]
                header=proc.read(manager,0x290)
                if header is None:raise ValueError('terrain manager unreadable')
                (out/f'eye_{eye}_manager.bin').write_bytes(header)
                pages,page_count=struct.unpack_from('<QQ',header,0x248)
                shift=struct.unpack_from('<Q',header,0x270)[0]
                mask=struct.unpack_from('<Q',header,0x280)[0]
                if shift>16 or mask>65535 or page_count>65536:raise ValueError('terrain slot table unsupported')
                slots=[]
                for handle in sorted({v for e in entries for v in e['pending'] if v}):
                    page=(handle&65535)>>shift;index=(handle&65535)&mask
                    if page>=page_count:raise ValueError('terrain page out of range')
                    page_data=proc.read(pages+page*64,64)
                    if page_data is None:raise ValueError('terrain page unreadable')
                    slot_data,slot_count=struct.unpack_from('<QQ',page_data,8)
                    if index>=slot_count:raise ValueError('terrain slot out of range')
                    slot=proc.read(slot_data+index*0x140,0x140)
                    if slot is None:raise ValueError('terrain slot unreadable')
                    generation=struct.unpack_from('<I',slot)[0]&0x7fffffff
                    if generation!=(handle>>16):raise ValueError('terrain handle generation changed')
                    filename=f'eye_{eye}_slot_{handle:08x}.bin'
                    (out/filename).write_bytes(slot)
                    slots.append(dict(handle=handle,address=hex(slot_data+index*0x140),flags=slot[0x110],
                        ready=(slot[0x110]&0xfe)==0,file=filename))
                entry_result['slots']=slots
                (out/f'eye_{eye}_entries.json').write_text(json.dumps(entry_result,indent=2))
                print('entries',eye,len(entries),entry_result['states'],'pending',entry_result['pending_handles'])
                print('slot_flags',eye,dict(collections.Counter(s['flags'] for s in slots)))
            result.append(dict(eye=eye,view=hex(view),streaming=hex(streaming),
                view_ready=data[0x718],ready_c1bb=stream[0xc1bb],pending_c1cc=struct.unpack_from('<I',stream,0xc1cc)[0],
                non_atomic=True))
        (out/'snapshot.json').write_text(json.dumps(result,indent=2))
        print(json.dumps(result))
    finally:proc.close()


if __name__=='__main__':main()
