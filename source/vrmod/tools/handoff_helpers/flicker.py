"""flicker.py launchdir: mean abs difference between consecutive captures of the same eye (static scene => flicker)."""
import sys,pathlib
d=pathlib.Path(sys.argv[1])/'native-xr'
def ppm(p):
    with open(p,'rb') as fh:
        fh.readline();w,h=map(int,fh.readline().split());fh.readline();return w,h,fh.read()
frames=sorted({int(p.name.split('-')[1]) for p in d.glob('frame-*-eye-0.ppm')})
for a,b in zip(frames,frames[1:]):
    if b-a>40:continue
    for e in (0,1):
        w,h,A=ppm(d/f'frame-{a}-eye-{e}.ppm');_,_,B=ppm(d/f'frame-{b}-eye-{e}.ppm')
        tot=0;n=0;big=0
        for y in range(0,h,4):
            r=y*w*3
            for x in range(0,w,4):
                i=r+x*3;dv=abs(A[i]-B[i])+abs(A[i+1]-B[i+1])+abs(A[i+2]-B[i+2]);tot+=dv;n+=1;big+=dv>60
        print(f'frames {a}->{b} eye {e}: mean diff {tot/n:.2f}, pixels changed >60: {100*big/n:.2f}%')
