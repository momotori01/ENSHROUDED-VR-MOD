"""Compare bounded C++ lookup with original cb4b50/ca5bc0 on synthetic tables.

No game process, loader, inventory changes, or host calls. Native malformed-table
infinite loops are deliberately not executed; C++ rejection is tested separately.
"""
import json,pathlib,struct,subprocess
from emulate_camera_math import CameraMath,SHA256
from unicorn.x86_const import UC_X86_REG_RAX

def main():
    fixture=pathlib.Path('vrmod/build/bin/Release/evr_item_registry_tests.exe')
    vectors=json.loads(subprocess.check_output([str(fixture),'--vectors'],text=True))
    e=CameraMath(pathlib.Path('enshrouded.exe'));e.allowed=[(0xcb4b50,0xcb4b7e),(0xca5bc0,0xca5c4f)]
    registry=e.data+0x100;bitmap=e.data+0x1000;keys=e.data+0x2000;values=e.data+0x4000;info=e.data+0x6000
    rows=[]
    for case in vectors:
        source=bytearray(0x10000);capacity=case['capacity'];occupied=case['occupied']
        for off,value in [(0x140,bitmap),(0x148,capacity),(0x160,keys),(0x178,values)]:struct.pack_into('<Q',source,off,value)
        struct.pack_into('<I',source,0x18c,occupied)
        for j in range(occupied):
            ident=capacity-1+capacity*(j+1);i=(capacity-1+j)&(capacity-1)
            at=0x1000+i//64*8;word=struct.unpack_from('<Q',source,at)[0]|1<<(i%64)
            struct.pack_into('<Q',source,at,word);struct.pack_into('<I',source,0x2000+i*4,ident)
            struct.pack_into('<Q',source,0x4000+i*8,info+j*0x200)
        e.cpu.mem_write(e.data,bytes(source));e.invoke(0xcb4b50,case['id'],registry)
        actual=e.cpu.reg_read(UC_X86_REG_RAX);preserved=bytes(e.cpu.mem_read(e.data,len(source)))==bytes(source)
        rows.append(dict(**case,native_pointer=actual,source_preserved=preserved,status='PASS' if actual==case['pointer'] and preserved else 'FAIL'))
    report=dict(scope=__doc__,exe_sha256=SHA256,cases=rows,no_external_substitutions=not e.calls,
                passed=sum(x['status']=='PASS' for x in rows),failed=sum(x['status']=='FAIL' for x in rows))
    pathlib.Path('docs/analysis/emulated_item_lookup.json').write_text(json.dumps(report,indent=2)+'\n')
    print(report['passed'],'PASS /',report['failed'],'FAIL; original instructions, no substitutions:',not e.calls)
    return int(report['failed']!=0 or bool(e.calls))
if __name__=='__main__':raise SystemExit(main())
