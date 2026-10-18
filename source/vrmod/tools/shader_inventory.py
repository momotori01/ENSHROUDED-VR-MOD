"""Read-only SPIR-V inventory and candidate reflection layout correlation.
No shader rewriting. Generated game-derived data stays in the local workspace.
Opcode/decorations: Khronos SPIR-V headers bundled in references/openvr/samples.
Matching member offsets is a candidate, not proof of an engine binding or live ABI.
"""
import argparse,collections,json,pathlib,struct

def inspect(path,reflection):
    raw=path.read_bytes()
    if len(raw)%4:raise ValueError('unaligned SPIR-V')
    words=struct.unpack('<'+'I'*(len(raw)//4),raw)
    if words[0]!=0x07230203:raise ValueError('bad SPIR-V magic')
    types={};decor=collections.defaultdict(dict);members=collections.defaultdict(dict)
    variables=[];entry=[];names={};constants={}
    i=5
    while i<len(words):
        count,op=words[i]>>16,words[i]&65535
        if not count or i+count>len(words):raise ValueError('invalid instruction bounds')
        a=words[i+1:i+count]
        if op==5:names[a[0]]=struct.pack('<'+'I'*(len(a)-1),*a[1:]).split(b'\0')[0].decode('utf8','replace')
        elif op==15:entry.append(a[0])
        elif op in (20,21,22,23,24,25,26,27,28,29,30,32):types[a[0]]=(op,a[1:])
        elif op==43:constants[a[1]]=a[2]
        elif op==59:variables.append(a[:3])
        elif op==71:decor[a[0]][a[1]]=list(a[2:])
        elif op==72:members[(a[0],a[1])][a[2]]=list(a[3:])
        i+=count
    def describe(t,depth=0):
        if depth>12:return {'kind':'recursion_limit'}
        op,a=types.get(t,(0,()))
        out={'id':t,'name':names.get(t),'opcode':op}
        if op==30:
            out['members']=[dict(index=j,decorations=members[(t,j)],type=describe(v,depth+1)) for j,v in enumerate(a)]
        elif op in (23,24):out.update(element=describe(a[0],depth+1),count=a[1])
        elif op==28:out.update(element=describe(a[0],depth+1),count=constants.get(a[1]),stride=decor[t].get(6))
        elif op==29:out.update(element=describe(a[0],depth+1),stride=decor[t].get(6))
        elif op==32:out.update(storage=a[0],element=describe(a[1],depth+1))
        elif op in (21,22):out['bits']=a[0]
        return out
    blocks=[]
    for pointer,v,storage in variables:
        if storage not in (2,9,12):continue
        op,a=types.get(pointer,(0,()))
        if op!=32:continue
        root=a[1];typ=describe(root)
        fields=typ.get('members',[])
        offsets=[f['decorations'].get(35,[None])[0] for f in fields]
        candidates=[]
        if len(offsets)>=3 and None not in offsets:
            for t in reflection:
                if [f['offset'] for f in t['fields']]==offsets:
                    candidates.append(t['qualified_name'])
        blocks.append(dict(variable=v,storage=storage,set=decor[v].get(34),binding=decor[v].get(33),
                           layout=typ,offset_candidates=candidates))
    return {'file':path.name,'bytes':len(raw),'stages':entry,'debug_name_count':len(names),'blocks':blocks}

def main():
    p=argparse.ArgumentParser();p.add_argument('directory',type=pathlib.Path);p.add_argument('--output',type=pathlib.Path,default=pathlib.Path('docs/analysis/shader_inventory.json'));a=p.parse_args()
    reflection=[t for t in json.loads(pathlib.Path('docs/analysis/reflection.json').read_text()) if not t['qualified_name'].startswith('keen::ds::') and t['fields'] and t['flags'] & 32]
    report=[inspect(path,reflection) for path in sorted(a.directory.glob('*.spv'))]
    a.output.write_text(json.dumps(report,separators=(',',':')))
    counts=collections.Counter(c for s in report for b in s['blocks'] for c in b['offset_candidates'])
    print(f'Inspected {len(report)} shaders; {sum(s["debug_name_count"] for s in report)} names; {len(counts)} candidate layouts')
    for name,n in counts.most_common(15):print(n,name)

if __name__=='__main__':main()
