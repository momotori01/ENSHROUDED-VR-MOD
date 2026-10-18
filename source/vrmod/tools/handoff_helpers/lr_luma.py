"""lr_luma.py launchdir: mean luma of left/right eye captures (whole, bottom half) per captured frame."""
import sys,pathlib
d=pathlib.Path(sys.argv[1])/'native-xr'
def ppm(p):
    with open(p,'rb') as fh:
        fh.readline();w,h=map(int,fh.readline().split());fh.readline();return w,h,fh.read()
def luma(w,h,px,y0=0):
    s=0;n=0
    for y in range(y0,h,8):
        r=y*w*3
        for x in range(0,w,8):
            i=r+x*3;s+=0.2126*px[i]+0.7152*px[i+1]+0.0722*px[i+2];n+=1
    return s/n
frames=sorted({int(p.name.split('-')[1]) for p in d.glob('frame-*-eye-0.ppm')})
for f in frames:
    L=ppm(d/f'frame-{f}-eye-0.ppm');R=ppm(d/f'frame-{f}-eye-1.ppm')
    lw,lh,_=L
    print(f, 'whole L %.1f R %.1f'%(luma(*L),luma(*R)), ' bottom L %.1f R %.1f'%(luma(*L,y0=lh//2),luma(*R,y0=lh//2)))
