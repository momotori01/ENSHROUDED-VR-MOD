"""Control the isolated Virtual HMD pose via an atomically replaced text file."""
import argparse,csv,math,os,pathlib,time
def publish(path,text):
    path.parent.mkdir(parents=True,exist_ok=True)
    temporary=path.with_suffix('.pending')
    temporary.write_text(text+'\n')
    # Windows readers may briefly hold the file. Retry only for bounded contention.
    for attempt in range(20):
        try:os.replace(temporary,path);return
        except PermissionError:time.sleep(.005)
    raise RuntimeError('Pose file is held open')
def main():
    p=argparse.ArgumentParser();p.add_argument('--file',type=pathlib.Path,default=pathlib.Path('vrmod/test_env/steamvr/pose.txt'))
    p.add_argument('--position',type=float,nargs=3,default=[0,1.65,0]);p.add_argument('--rotation',type=float,nargs=3,default=[0,0,0],metavar=('YAW','PITCH','ROLL'))
    p.add_argument('--ipd',type=float,default=.064);p.add_argument('--tracking-lost',action='store_true');p.add_argument('--disconnect',action='store_true');p.add_argument('--auto',action='store_true');p.add_argument('--sequence',type=pathlib.Path)
    a=p.parse_args()
    if a.sequence:
        start=time.monotonic()
        with a.sequence.open() as f:
            for i,r in enumerate(csv.reader(f)):
                deadline=start+i/90
                if deadline>time.monotonic():time.sleep(deadline-time.monotonic())
                publish(a.file,'pose '+' '.join(r[1:10]))
    elif a.auto:publish(a.file,'auto')
    else:
        values=a.position+a.rotation+[a.ipd]
        if not all(map(math.isfinite,values)) or not .02<=a.ipd<=.12:p.error('invalid pose/IPD')
        publish(a.file,'pose '+' '.join(map(str,values+[int(not a.tracking_lost),int(not a.disconnect)])))
if __name__=='__main__':main()
