"""Execute native input-device classifier and UI context setter in isolated CPU.
No real gamepad, UI interaction, running process or OS input is simulated here.
"""
import itertools,json,pathlib,struct
from emulate_camera_math import CameraMath,SHA256
from unicorn.x86_const import UC_X86_REG_RAX

def main():
 e=CameraMath(pathlib.Path('enshrouded.exe'))
 e.allowed=[(0xce7700,0xce7763),(0xcb3490,0xcb3494),(0x9588a0,0x9588a9)]
 state=e.data;event=state+0x100;ui=state+0x200;rows=[]
 axes=[(0.,0.,0.,0.,.125),(.124,0.,0.,0.,.125),(.125,0.,0.,0.,.125),(-.126,0.,0.,0.,.125),(0.,-.126,0.,0.,.125),(1.,1.,1.,1.,.125),(1.,1.,.5,.75,.125),(0.,0.,0.,0.,0.)]
 for initial,kind in itertools.product((0,1,2),range(256)):
  for axis in (axes if kind==5 else axes[:1]):
   initial_state=bytes([initial])+bytes([0xA5])*63;initial_ui=bytes([0xC3])*64
   data=bytearray([0x5A]*64);data[12]=kind;struct.pack_into('<5f',data,0x1c,*axis)
   e.cpu.mem_write(state,initial_state);e.cpu.mem_write(event,bytes(data));e.cpu.mem_write(ui,initial_ui)
   e.invoke(0xce7700,state,event);actual=e.cpu.mem_read(state,1)[0]
   expected=initial
   if kind in (2,3,4):expected=1
   elif kind==5 and max(abs(axis[0]-axis[2]),abs(axis[1]-axis[3]))>=axis[4]:expected=1
   elif 7<=kind<=14:expected=0
   e.invoke(0xcb3490,state);getter=e.cpu.reg_read(UC_X86_REG_RAX)&255
   # Caller's reviewed cmp/sete sequence passes whether the device byte == 1.
   e.invoke(0x9588a0,ui,int(getter==1));ui_after=bytes(e.cpu.mem_read(ui,64))
   wanted=bytearray(initial_ui);wanted[9]=int(expected==1)
   checks=dict(classification=actual==expected,getter=getter==actual,source_untouched=bytes(e.cpu.mem_read(event,64))==bytes(data),device_other_bytes_untouched=bytes(e.cpu.mem_read(state+1,63))==initial_state[1:],ui_only_mode_byte=ui_after==bytes(wanted))
   rows.append(dict(initial=initial,event_type=kind,axis=axis,actual=actual,expected=expected,checks=checks,status='PASS' if all(checks.values()) else 'FAIL'))
 for v in (0,1,255):e.invoke(0x9588a0,0,v)
 report=dict(exe_sha256=SHA256,scope=__doc__,native_ranges=[[hex(a),hex(b)] for a,b in e.allowed],cases=rows,passed=sum(x['status']=='PASS' for x in rows),failed=sum(x['status']=='FAIL' for x in rows),null_context_setter_calls=3,substitutions=e.calls)
 pathlib.Path('docs/analysis/emulated_controller_ui.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf8')
 print('RESULT',report['passed'],'PASS',report['failed'],'FAIL; 3 null-context returns')
 return bool(report['failed'])
if __name__=='__main__':raise SystemExit(main())
