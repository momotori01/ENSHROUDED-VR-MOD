"""toggles.py launchdir [out.png]: flicker = pixels that change between consecutive frames and come back (A->B->A).
Wind sway changes smoothly; a toggle needs |a-b|>40 and |a-c|<12 on the max channel. Reports per eye; optional heatmap of eye 1."""
import sys,pathlib,struct,zlib
d=pathlib.Path(sys.argv[1])/'native-xr';out=pathlib.Path(sys.argv[2]) if len(sys.argv)>2 else None
def ppm(p):
    with open(p,'rb') as fh:
        fh.readline();w,h=map(int,fh.readline().split());fh.readline();return w,h,fh.read()
frames=sorted({int(p.name.split('-')[1]) for p in d.glob('frame-*-eye-0.ppm')})
# the longest run of consecutive frames
best=[];cur=[]
for f in frames:
    if cur and f==cur[-1]+1:cur.append(f)
    else:cur=[f]
    if len(cur)>len(best):best=list(cur)
print('consecutive frames',best[0] if best else None,'..',best[-1] if best else None,len(best))
S=2
for e in (0,1):
    imgs=[ppm(d/f'frame-{f}-eye-{e}.ppm') for f in best];w,h,_=imgs[0];tw,th=w//S,h//S
    heat=[0]*(tw*th);tog=0;n=0
    for k in range(len(imgs)-2):
        A,B,C=imgs[k][2],imgs[k+1][2],imgs[k+2][2]
        for y in range(th):
            r=(y*S)*w
            for x in range(tw):
                i=(r+x*S)*3
                ab=max(abs(A[i]-B[i]),abs(A[i+1]-B[i+1]),abs(A[i+2]-B[i+2]));ac=max(abs(A[i]-C[i]),abs(A[i+1]-C[i+1]),abs(A[i+2]-C[i+2]))
                n+=1
                if ab>40 and ac<12:tog+=1;heat[y*tw+x]+=1
    print(f'eye {e}: toggle pixels {100*tog/max(n,1):.3f}% over {len(imgs)-2} triples')
    if out and e==1:
        base=imgs[0][2];rows=[]
        for y in range(th):
            line=bytearray()
            for x in range(tw):
                i=(y*S*w+x*S)*3;v=heat[y*tw+x]
                if v:line+=bytes((255,min(255,40*v),0))
                else:g=(base[i]+base[i+1]+base[i+2])//6;line+=bytes((g,g,g))
            rows.append(bytes(line))
        def chunk(t,p):return struct.pack('>I',len(p))+t+p+struct.pack('>I',zlib.crc32(t+p)&0xffffffff)
        out.write_bytes(b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>IIBBBBB',tw,th,8,2,0,0,0))+chunk(b'IDAT',zlib.compress(b''.join(b'\0'+r for r in rows)))+chunk(b'IEND',b''))
