"""Verify captured GPU particle rebase, source preservation, and draw consumption.

Only local proprietary capture exports are read. Counts/data come from actual
bindings, not from the number of CPU hook calls. Atomic append order may vary.
"""
import argparse,collections,hashlib,json,pathlib,struct


def main():
    a=argparse.ArgumentParser();a.add_argument('probe',type=pathlib.Path);a.add_argument('bindings',type=pathlib.Path)
    a.add_argument('events',nargs=3,type=int);a.add_argument('--usage',type=pathlib.Path)
    args=a.parse_args();root=args.bindings.parent
    rows=[json.loads(s) for s in args.bindings.read_text().splitlines()]
    native=[json.loads(s) for s in (args.probe/'view-resource-probe.jsonl').read_text().splitlines()]
    serial=[e for e in native if e['event']=='live_native_pair_complete'][-1]['serial']
    eye_logs={e['eye']:e for e in native if e['event']=='vfx_transparent_eye_observed' and e['serial']==serial}
    cases=[];checks={};details={}
    for event in args.events:
        r={e['index']:e for e in rows if e['event']==event and e['stage']==5 and e['category']=='resource'}
        if set(r)!={0,1,2,3,4}:raise ValueError('five captured resource bindings required')
        b={i:(root/e['file']).read_bytes() for i,e in r.items()}
        if any(len(b[i])!=r[i]['size'] for i in r):raise ValueError('short capture export')
        payload=struct.unpack('<'+'I'*(len(b[0])//4),b[0])
        shader=root/f'event_{event}_stage_5.spv'
        checks[f'event{event}_shader_recipe']=hashlib.sha256(shader.read_bytes()).hexdigest()=='c7e788778594b7d0f698c15cf97bc65a8fdfddc2f8e7d4f1154985c6db81c908'
        cases.append((r,b,payload))
    sr,sb,sp=cases[0]
    if len(sp)!=1 or sp[0]%4:raise ValueError('native source payload required')
    count=struct.unpack_from('<I',sb[4],sp[0])[0]
    if count>65536 or count*16>len(sb[1]):raise ValueError('source count beyond captured sort array')
    expected=[]
    for i in range(count):
        idx=struct.unpack_from('<I',sb[1],16*i+8)[0]
        if (idx+1)*180>len(sb[2]):raise ValueError('source index out of range')
        rec=bytearray(sb[2][idx*180:(idx+1)*180]);struct.pack_into('<I',rec,16,i);expected.append(rec)
    checks['source_native_copy_matches_sort_list']=sb[3][:count*180]==b''.join(expected)
    checks['source_has_visible_particles']=count>0
    def source_records(buffers):
        # Each SetFrameEvent may replay simulation atomics in a different order.
        # Follow that replay's sort indices, compare visible record contents as
        # a multiset, and ignore only instanceOffset (native writeback overwrites
        # this address-like field with its output slot). Inactive storage is not
        # evidence of visible-particle changes.
        result=[]
        for i in range(count):
            idx=struct.unpack_from('<I',buffers[1],16*i+8)[0]
            if (idx+1)*180>len(buffers[2]):raise ValueError('capture source index bounds')
            rec=bytearray(buffers[2][idx*180:(idx+1)*180]);struct.pack_into('<I',rec,16,0);result.append(bytes(rec))
        return result
    baseline_source=collections.Counter(source_records(sb))
    details['source_count']=count;details['serial']=serial;details['eyes']=[]
    usage=[json.loads(s) for s in args.usage.read_text().splitlines()] if args.usage else []
    for eye,(r,b,p) in enumerate(cases[1:]):
        if len(p)!=7 or p[1]!=0x45565254:raise ValueError('extended payload marker/size')
        delta=struct.unpack('<3f',struct.pack('<3I',*p[4:]));target_count=struct.unpack_from('<I',b[4],p[0])[0]
        current_source=source_records(b)
        checks[f'eye{eye}_visible_source_records_preserved']=collections.Counter(current_source)==baseline_source and struct.unpack_from('<I',b[4],sp[0])[0]==count
        checks[f'eye{eye}_count_and_counter']=p[2]==sp[0] and p[0]==eye_logs[eye]['counter_offset'] and target_count==count<=p[3]
        checks[f'eye{eye}_delta_matches_native_camera']=max(abs(x-y) for x,y in zip(delta,eye_logs[eye]['delta']))<1e-8
        desired=[]
        for rec in current_source:
            changed=bytearray(rec);pos=struct.unpack_from('<3f',rec,40)
            struct.pack_into('<3f',changed,40,*[x+y for x,y in zip(pos,delta)])
            struct.pack_into('<I',changed,16,0);desired.append(bytes(changed))
        actual=[];slot_ok=True
        for i in range(target_count):
            rec=bytearray(b[3][i*180:(i+1)*180]);slot_ok&=struct.unpack_from('<I',rec,16)[0]==i
            struct.pack_into('<I',rec,16,0);actual.append(bytes(rec))
        checks[f'eye{eye}_all_particle_fields_match_rebased_source']=collections.Counter(actual)==collections.Counter(desired) and slot_ok
        if args.usage:
            begin=args.events[eye+1];end=args.events[eye+2] if eye==0 else 2**32
            # ResourceUsage VS/PS read-write storage buffers 22/26, as exported
            # by this pinned RenderDoc 1.46 controller. Draw event IDs follow.
            draws=sorted({e['event'] for e in usage if e['buffer']==r[3]['buffer'] and begin<e['event']<end and e['usage'] in (22,26)})
            checks[f'eye{eye}_particle_buffer_used_by_graphics']=bool(draws)
        else:draws=[]
        details['eyes'].append(dict(eye=eye,count=target_count,delta=delta,draw_events=draws))
    report=dict(checks={k:'PASS' if v else 'FAIL' for k,v in checks.items()},details=details,
        limitation='GPU visible-record contents and graphics consumption; replays can reorder simulation atomics. Inactive source rows are excluded. Not complete image quality, timing or physical stereo validation.')
    (root/'vfx-writeback-checks.json').write_text(json.dumps(report,indent=2)+'\n')
    for k,v in checks.items():print('PASS' if v else 'FAIL',k)
    print('PASS',sum(checks.values()),'FAIL',sum(not v for v in checks.values()));return int(not all(checks.values()))


if __name__=='__main__':raise SystemExit(main())
