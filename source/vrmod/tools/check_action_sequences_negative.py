"""Binary-layout and negative checks for the read-only sequence evidence reader."""
import struct
from check_action_sequences import decode
raw=bytearray(3104)
struct.pack_into('<I',raw,0,0xfedcba98)
raw[0x170:0x174]=bytes([9,0xaa,0xbb,0xcc]);raw[0x1a:0x1c]=bytes([7,0xee])
struct.pack_into('<q',raw,0x180,-37);raw[0x1b1]=3
struct.pack_into('<Q',raw,0xbf8,5);struct.pack_into('<Q',raw,0xbd0,8);struct.pack_into('<Q',raw,0xbd8,1)
d=decode(raw)
assert d['trigger_sequenceId']==0xfedcba98 and d['subSequenceId']==9 and d['inputType']==7 and d['currentTime']==-37 and d['effectiveState']==12
print('PASS unsigned ID, byte sub-sequence/input, signed native time and effective state')
for n in [0,1,3103,3105]:
 try:decode(bytes(n))
 except ValueError:print('PASS rejected invalid byte count',n)
 else:raise AssertionError('accepted wrong size')
# All six follow-up entries, last boolean at end, no accidental pointer/stride reinterpretation.
for i in range(6):
 struct.pack_into('<q',raw,0x1d8+424*i+0x188,(-1)**i*(100+i));raw[0x1d8+424*i+0x1a4]=i%2
f=decode(raw)['followUps'];assert [x['localExecutionTime'] for x in f]==[(-1)**i*(100+i) for i in range(6)] and [x['wasTriggered'] for x in f]==[i%2 for i in range(6)]
print('PASS full follow-up array and signed timing widths')
