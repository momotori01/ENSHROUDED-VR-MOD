"""Run only the verified game's tiny queue try-lock leaf in an isolated CPU emulator."""
import hashlib,json,struct,sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).parent/'python'))
import pefile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_64
from unicorn.x86_const import UC_X86_REG_RCX,UC_X86_REG_RSP,UC_X86_REG_RAX,UC_X86_REG_RIP

root=Path(__file__).resolve().parents[2];raw=(root/'enshrouded.exe').read_bytes()
if hashlib.sha256(raw).hexdigest()!='af2f5a1227911d8aa06b3908d6bd0211838211cae14ea91099cb57d0df990781':raise ValueError('unsupported game')
pe=pefile.PE(data=raw);code=pe.get_data(0x7df330,0x28)
if code[:10]!=bytes.fromhex('33 d2 41 b8 00 00 00 80 8b 01') or code[-3:]!=bytes.fromhex('b0 01 c3'):raise ValueError('queue lock leaf changed')
results=[]
for value in (0,1,0x80000000,0xffffffff):
    vm=Uc(UC_ARCH_X86,UC_MODE_64)
    for address in (0x100000,0x200000,0x300000):vm.mem_map(address,4096)
    vm.mem_write(0x100000,code);vm.mem_write(0x200000,struct.pack('<I',value));vm.mem_write(0x300800,struct.pack('<Q',0x100100))
    vm.reg_write(UC_X86_REG_RCX,0x200000);vm.reg_write(UC_X86_REG_RSP,0x300800)
    vm.emu_start(0x100000,0x100100,timeout=100000,count=3000)
    changed=struct.unpack('<I',vm.mem_read(0x200000,4))[0];returned=vm.reg_read(UC_X86_REG_RAX)&255
    passed=vm.reg_read(UC_X86_REG_RIP)==0x100100 and returned==int(value==0) and changed==(0x80000000 if value==0 else value)
    results.append(dict(initial=value,after=changed,returned=returned,status='PASS' if passed else 'FAIL'))
    print(results[-1]['status'],hex(value),'->',hex(changed),'acquired',returned)
(root/'docs/analysis/native_queue_lock/emulation.json').write_text(json.dumps(results,indent=2))
raise SystemExit(int(any(r['status']=='FAIL' for r in results)))
