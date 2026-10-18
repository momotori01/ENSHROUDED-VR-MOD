# SPDX-License-Identifier: GPL-3.0-or-later
"""Read-only enum/typedef metadata. Layout reference: Brabb3l/kfc-parser.

Keep this analysis tool separate from the mod runtime. No game bytes are emitted.
"""
import argparse,hashlib,json,pathlib,struct,sys
sys.path.insert(0,str(pathlib.Path(__file__).parent/'python'))
import pefile

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('names',nargs='+');p.add_argument('--output',type=pathlib.Path,required=True)
    a=p.parse_args();raw=pathlib.Path('enshrouded.exe').read_bytes();pe=pefile.PE(data=raw,fast_load=True)
    base=pe.OPTIONAL_HEADER.ImageBase
    def offset(va,length):
        rva=va-base
        for section in pe.sections:
            delta=rva-section.VirtualAddress
            if 0<=delta and delta+length<=section.SizeOfRawData:
                pos=section.PointerToRawData+delta
                if pos+length<=len(raw):return pos
        raise ValueError('metadata range outside file-backed section')
    def decode(va,seen):
        if va in seen or len(seen)>8:raise ValueError('cyclic/too-deep typedef')
        off=offset(va,144)
        def u64(n):return struct.unpack_from('<Q',raw,off+n)[0]
        def name(ptr,n):
            if not 0<n<=512:raise ValueError('invalid metadata string length')
            o=offset(ptr,n);return raw[o:o+n].decode('utf8')
        qualified=name(u64(32),u64(40));size,align,element,count,primitive=struct.unpack_from('<IHHIB',raw,off+64)
        if count>4096:raise ValueError('oversized enum')
        result=dict(name=qualified,rva=hex(va-base),size=size,primitive=primitive)
        if primitive==17:
            result['inner']=decode(u64(56),seen|{va})
        elif primitive in (12,13,14,15,16):
            start=offset(u64(96),count*40) if count else 0
            result['values']=[]
            for i in range(count):
                ptr,n,value=struct.unpack_from('<QQQ',raw,start+i*40)
                result['values'].append(dict(name=name(ptr,n),value=value,hex=hex(value)))
        else:raise ValueError('requested type is not an enum/bitmask/typedef')
        return result
    types=json.loads(pathlib.Path('docs/analysis/reflection.json').read_text())
    results=[]
    for wanted in a.names:
        matches=[t for t in types if t['qualified_name']==wanted]
        if len(matches)!=1:raise ValueError('qualified type absent/ambiguous: '+wanted)
        results.append(decode(base+int(matches[0]['metadata_rva'],16),set()))
    report=dict(exe_sha256=hashlib.sha256(raw).hexdigest(),types=results)
    a.output.parent.mkdir(parents=True,exist_ok=True);a.output.write_text(json.dumps(report,indent=2)+'\n')
    for row in results:print(json.dumps(row))

if __name__=='__main__':main()
