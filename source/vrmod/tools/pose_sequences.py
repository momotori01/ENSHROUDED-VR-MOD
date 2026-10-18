"""Generate deterministic OpenXR poses. Angles use RH OpenXR (+yaw turns left)."""
import argparse, csv, math, pathlib
def row(t,x=0,y=1.65,z=0,yaw=0,pitch=0,roll=0,ipd=.064,tracked=1,connected=1,width=640,height=720):
    return [t,x,y,z,yaw,pitch,roll,ipd,tracked,connected,width,height]
def generate(directory):
    directory.mkdir(parents=True,exist_ok=True)
    cases={
      '01_forward':lambda t:row(t),
      '02_yaw_plus90':lambda t:row(t,yaw=90),
      '03_yaw_minus90':lambda t:row(t,yaw=-90),
      '04_yaw180':lambda t:row(t,yaw=180),
      '05_look_up':lambda t:row(t,pitch=80),
      '06_look_down':lambda t:row(t,pitch=-90,yaw=45),
      '07_sideways':lambda t:row(t,x=.3*math.sin(t*2)),
      '08_fore_aft':lambda t:row(t,z=.3*math.sin(t*2)),
      '09_vertical':lambda t:row(t,y=1.65+.3*math.sin(t*2)),
      '10_roll':lambda t:row(t,roll=35*math.sin(t*2)),
      '11_circle':lambda t:row(t,x=.3*math.cos(t*2),z=.3*math.sin(t*2),yaw=t*45),
      '12_rapid_yaw':lambda t:row(t,yaw=80*math.sin(t*12)),
      '13_continuous':lambda t:row(t,x=.2*math.sin(t),y=1.65+.1*math.sin(t*.7),z=.2*math.cos(t*.8),yaw=80*math.sin(t*.5),pitch=40*math.sin(t*.6),roll=20*math.sin(t*.9)),
      '14_tracking_lost':lambda t:row(t,tracked=int(not .5<t<1.5)),
      '15_disconnect':lambda t:row(t,connected=int(t<1)),
      '16_ipd_sweep':lambda t:row(t,ipd=.058+.014*(.5+.5*math.sin(t*2))),
      '17_resolution':lambda t:row(t,width=800,height=900),
    }
    for name,fn in cases.items():
        count=16200 if name=='13_continuous' else 180
        with (directory/(name+'.csv')).open('w',newline='') as file:
            csv.writer(file).writerows(fn(i/90) for i in range(count))
    with (directory/'suite.csv').open('w',newline='') as file:
        writer=csv.writer(file)
        for name,fn in cases.items():
            if name in ('13_continuous','15_disconnect','17_resolution'):continue
            writer.writerows(fn(i/90) for i in range(180))
    print('Generated',len(cases)+1,'sequences in',directory)
if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--output',type=pathlib.Path,default=pathlib.Path('vrmod/config/poses'));a=p.parse_args();generate(a.output)
