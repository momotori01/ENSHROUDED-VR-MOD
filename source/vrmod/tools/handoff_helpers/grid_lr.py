"""grid_lr.py out.png launchdir k1 k2 ...: stack several captured frames (left|right) at 1/5 scale."""
import sys,pathlib,struct,zlib
out,launch=pathlib.Path(sys.argv[1]),pathlib.Path(sys.argv[2]);picks=[int(x) for x in sys.argv[3:]]
d=launch/'native-xr';frames=sorted({int(p.name.split('-')[1]) for p in d.glob('frame-*-eye-0.ppm')})
def ppm(p):
    with open(p,'rb') as fh:
        fh.readline();w,h=map(int,fh.readline().split());fh.readline();return w,h,fh.read()
s=5;rows=[]
for k in picks:
    f=frames[min(k,len(frames))-1];imgs=[ppm(d/f'frame-{f}-eye-{e}.ppm') for e in (0,1)]
    w,h,_=imgs[0];tw,th=w//s,h//s
    for y in range(th):
        line=b''
        for (iw,ih,px) in imgs:
            r=(y*s)*iw;line+=b''.join(px[(r+x*s)*3:(r+x*s)*3+3] for x in range(tw))+b'\xff\x00\x00'*4
        rows.append(line)
    rows.append(rows[-1][:0]+b'\xff\xff\xff'*(len(rows[-1])//3))
def chunk(t,p):return struct.pack('>I',len(p))+t+p+struct.pack('>I',zlib.crc32(t+p)&0xffffffff)
W=len(rows[0])//3;rows=[r if len(r)==W*3 else r+b'\x00'*(W*3-len(r)) for r in rows]
out.write_bytes(b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>IIBBBBB',W,len(rows),8,2,0,0,0))+chunk(b'IDAT',zlib.compress(b''.join(b'\0'+r for r in rows)))+chunk(b'IEND',b''))
print(out,[frames[min(k,len(frames))-1] for k in picks])
