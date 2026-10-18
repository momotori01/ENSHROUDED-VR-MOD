# SPDX-License-Identifier: GPL-3.0-or-later
"""Read-only PE/reflection analysis. Layout research: Brabb3l/kfc-parser (GPL-3.0).
No game memory is modified. File offsets, RVAs and preferred VAs are distinct.
"""
import argparse, hashlib, json, pathlib, re, struct, sys
sys.path.insert(0, str(pathlib.Path(__file__).parent / 'python'))
import pefile
from capstone import Cs, CS_ARCH_X86, CS_MODE_64

def main():
    p = argparse.ArgumentParser()
    p.add_argument('--exe', default='enshrouded.exe')
    p.add_argument('--output', default='docs/analysis')
    a = p.parse_args()
    out = pathlib.Path(a.output); out.mkdir(parents=True, exist_ok=True)
    data = pathlib.Path(a.exe).read_bytes(); pe = pefile.PE(data=data)
    base = pe.OPTIONAL_HEADER.ImageBase
    def offset(va):
        if not base <= va < base + pe.OPTIONAL_HEADER.SizeOfImage: raise ValueError('invalid VA')
        return pe.get_offset_from_rva(va-base)
    def va(off): return base + pe.get_rva_from_offset(off)
    def u64(off): return struct.unpack_from('<Q', data, off)[0]
    def string(adr, n):
        if n > 4096: raise ValueError('oversized name')
        if n == 0: return ''
        o = offset(adr)
        return data[o:o+n].decode('utf8')
    def findall(pattern):
        return [m.start() for m in re.finditer(re.escape(pattern), data)]
    ident = {'sha256':hashlib.sha256(data).hexdigest(), 'size':len(data), 'preferred_base':hex(base),
             'timestamp':pe.FILE_HEADER.TimeDateStamp, 'image_size':pe.OPTIONAL_HEADER.SizeOfImage,
             'sections':[{'name':s.Name.rstrip(b'\0').decode(), 'rva':hex(s.VirtualAddress),
                          'file_offset':hex(s.PointerToRawData),'size':s.SizeOfRawData} for s in pe.sections],
             'imports':{i.dll.decode():[f.name.decode() if f.name else str(f.ordinal) for f in i.imports]
                        for i in getattr(pe,'DIRECTORY_ENTRY_IMPORT',[])}}
    (out/'game_identity.json').write_text(json.dumps(ident,indent=2))
    pattern = bytes.fromhex('F3 0F 11 9F C4 06 00 00')
    hits = findall(pattern)
    md = Cs(CS_ARCH_X86, CS_MODE_64)
    report = {'shroudtopia_svn55812':{'pattern':pattern.hex(' '),'match_count':len(hits),
        'matches':[{'file_offset':hex(x),'rva':hex(va(x)-base),
                    'disassembly':[f'{i.address-base:08x}: {i.mnemonic} {i.op_str}'
                                   for i in md.disasm(data[x:x+100],va(x))]} for x in hits]}}
    (out/'signatures.json').write_text(json.dumps(report,indent=2))
    strings = [{'rva':hex(va(m.start())-base),'text':m.group().decode('ascii')}
               for m in re.finditer(rb'[\x20-\x7e]{5,}', data)
               if re.search(rb'camera|projection|viewmatrix|headmesh|helmet|hair|aim|swapchain|vulkan|playertransform',m.group(),re.I)]
    (out/'interesting_strings.json').write_text(json.dumps(strings,indent=2))
    # Recover metadata via name pointer + length, then validate complete records.
    # This avoids assuming the registry's placement or trusting a legacy address.
    types = {}
    # Index aligned pointer/length pairs once instead of scanning the whole PE per string.
    name_records = {}
    for section in pe.sections:
        if section.Name.rstrip(b'\0') not in (b'.rdata', b'.data'): continue
        for off in range(section.PointerToRawData, section.PointerToRawData+section.SizeOfRawData-144, 8):
            ptr,n=struct.unpack_from('<QQ',data,off)
            if base<=ptr<base+pe.OPTIONAL_HEADER.SizeOfImage and 1<n<=180:
                name_records.setdefault((ptr,n),[]).append(off)
    for m in re.finditer(rb'(?<=\x00)([A-Za-z_][A-Za-z0-9_:<> ,.*\[\]]{1,180})(?=\x00)',data):
        namebytes = m.group(1); nameva = va(m.start(1))
        for o in name_records.get((nameva,len(namebytes)),[]):
            try:
                if o%8: continue
                name = string(u64(o),u64(o+8))
                qualified = string(u64(o+32),u64(o+40))
                size,align,element_align,count,prim,flags = struct.unpack_from('<IHHIBB',data,o+64)
                if not qualified or count>4096 or align not in (1,2,4,8,16,32,64) or size>0x100000: continue
                fields = []; fp = u64(o+88)
                if fp:
                    foff=offset(fp)
                    for j in range(count):
                        fo=foff+j*48
                        fn=string(u64(fo),u64(fo+8)); ft=offset(u64(fo+16))
                        tn=string(u64(ft+32),u64(ft+40)); fieldoff=u64(fo+24)
                        if not fn or fieldoff>size: raise ValueError('field invalid')
                        fields.append({'name':fn,'type':tn,'offset':fieldoff,'offset_hex':hex(fieldoff)})
                types[qualified]={'name':name,'qualified_name':qualified,'metadata_rva':hex(va(o)-base),
                                  'size':size,'alignment':align,'flags':flags,'primitive':prim,'fields':fields}
            except (ValueError,UnicodeError,struct.error,pefile.PEFormatError): pass
    alltypes=sorted(types.values(),key=lambda t:t['qualified_name'])
    (out/'reflection.json').write_text(json.dumps(alltypes,indent=2))
    selected=[t for t in alltypes if re.search(r'camera|view|render|player|character|aim|head|hair|helmet|input',t['qualified_name'],re.I)]
    (out/'reflection_vr.json').write_text(json.dumps(selected,indent=2))
    print(json.dumps({'sha256':ident['sha256'],'legacy_signature_matches':len(hits),
                      'reflected_types':len(alltypes),'vr_types':len(selected),'interesting_strings':len(strings)}))

if __name__ == '__main__': main()
