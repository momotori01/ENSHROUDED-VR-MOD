"""Deterministic tracking-loss inputs for isolated Mock, never host VR settings."""
import csv,json,pathlib
from pose_sequences import row

def main():
    root=pathlib.Path('vrmod/test_env');root.mkdir(exist_ok=True)
    with (root/'tracking-protocol.csv').open('w',newline='') as f:
        csv.writer(f).writerows(row(i/90,tracked=int(i<2 or i>=92),width=64,height=72) for i in range(94))
    with (root/'world-vertical-pose.csv').open() as f:source=list(csv.reader(line for line in f if not line.startswith('#')))
    ranges=[(81,120),(461,580)] # one-based xrWaitFrame indices: warmup and active stereo
    with (root/'world-tracking-recovery.csv').open('w',newline='') as f:
        writer=csv.writer(f)
        for i in range(1280):
            r=source[i%len(source)].copy();r[0]=str(i/90);r[8]=str(int(not any(lo<=i+1<=hi for lo,hi in ranges)));writer.writerow(r)
    (root/'world-tracking-recovery.json').write_text(json.dumps(dict(scope=__doc__,frames=1280,lost_frame_ranges=ranges,
        expected_lost_frames=160,nominal_clock_only=True,source='world-vertical-pose.csv'),indent=2)+'\n')
    with (root/'world-runtime-loss.csv').open('w',newline='') as f:
        writer=csv.writer(f)
        for i in range(1280):
            r=source[i%len(source)].copy();r[0]=str(i/90);r[9]=str(int(i<540));writer.writerow(r)
    print('Generated GPU protocol, 160-frame temporary world tracking loss, and permanent disconnect at frame541.')
if __name__=='__main__':main()
