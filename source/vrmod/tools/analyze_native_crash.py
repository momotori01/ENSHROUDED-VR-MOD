"""Offline register/memory and game-generated callstack analysis. Never uploads dumps."""
import argparse
import hashlib
import json
import pathlib
import struct
from analyze_camera_dump import Dump


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('directory',type=pathlib.Path)
    p.add_argument('--output',type=pathlib.Path,required=True)
    args=p.parse_args()
    dump=Dump(args.directory/'enshrouded.dmp')
    _,offset=dump.streams[6]
    thread,_,code,flags,_,address,count,_=dump.unpack('<IIIIQQII',offset)
    size,context=dump.unpack('<II',offset+160)
    if size<256:raise ValueError('x64 context unavailable')
    names=['rax','rcx','rdx','rbx','rsp','rbp','rsi','rdi',*[f'r{i}' for i in range(8,16)],'rip']
    regs={name:dump.unpack('<Q',context+120+8*i)[0] for i,name in enumerate(names)}
    def locate(address):
        m=next((m for m in dump.modules if m['base']<=address<m['base']+m['size']),None)
        return dict(address=hex(address),module=pathlib.PureWindowsPath(m['name']).name if m else None,
                    rva=hex(address-m['base']) if m else None)
    info=json.loads((args.directory/'enshrouded.info').read_text(encoding='utf-8-sig'))
    report=dict(dump_sha256=hashlib.sha256(dump.data).hexdigest(),thread=thread,code=hex(code),fault=locate(address),
                access_parameters=[hex(x) for x in dump.unpack('<15Q',offset+40)[:count]],
                registers={k:hex(v) for k,v in regs.items()},
                game_reported_callstack=[locate(e['addr']) for e in info['callstack']],memory={})
    for name in ['rcx','rdx','rbp','rsp']:
        value=regs[name]
        data=dump.read(value,0x180)
        report['memory'][name]=dict(address=hex(value),captured=data is not None,bytes=data.hex() if data else None)
    args.output.parent.mkdir(parents=True,exist_ok=True)
    args.output.write_text(json.dumps(report,indent=2),encoding='utf8')
    for k,v in report.items():
        if k!='memory':print(k,v)


if __name__=='__main__':main()
