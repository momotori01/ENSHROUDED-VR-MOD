"""Compare the owned C++ Root implementation with isolated native instructions.

Only the local PE, saved observer records and synthetic vectors are used.
No game process is read or changed.
"""
import hashlib
import json
import math
from pathlib import Path
import random
import struct
import subprocess
from emulate_root_interpolation import RootMath, transform


def main():
    mod=Path(__file__).resolve().parents[1]
    out=mod/'output/root-cpp-validation';out.mkdir(parents=True,exist_ok=True)
    native=RootMath(mod.parent/'enshrouded.exe')
    rng=random.Random(1203620);vectors=[];expected=[];labels=[]
    def add(label,render,tick,fraction):
        fraction=struct.unpack('<f',struct.pack('<f',fraction))[0]
        source_tick,newest_tick=struct.unpack_from('<II',render,168)
        value,history=native.interpolate(render[112:168],render[56:112],render[112:168],source_tick,newest_tick,tick,fraction,rendered=render[:56])
        if history[56:]!=render[56:]:raise ValueError('native isolation unexpectedly advanced history')
        vectors.append(render+struct.pack('<If',tick,fraction));expected.append(value);labels.append(label)
    for n in range(240):
        source=[rng.uniform(-1e7,1e7) for _ in range(3)]
        delta=[rng.uniform(-2000,2000) if n%3 else rng.uniform(-.02,.02) for _ in range(3)]
        a0,b0=rng.uniform(-math.pi,math.pi),rng.uniform(-math.pi,math.pi)
        qa=(0,math.sin(a0/2),0,math.cos(a0/2));qb=(0,math.sin(b0/2),0,math.cos(b0/2))
        a=transform(source,qa);b=transform([x+y for x,y in zip(source,delta)],qb)
        gap=(1,2,5,17)[n%4]
        add(f'synthetic_{n}',transform((100,-200,300))+a+b+struct.pack('<II',100,100+gap),100+gap,rng.random())
    for folder in ('game-20260913-095654/camera-observer-20260913-095743','game-20260913-100923/camera-observer-20260913-101016'):
        path=mod/'output'/folder/'camera-observer.jsonl'
        rows=[json.loads(line) for line in path.read_text().splitlines()]
        for e in rows:
            if e['event']!='camera_query_sample':continue
            context=e['task_context']
            if not context['valid'] or not context['local_entity']:raise ValueError('saved context invalid')
            add(f'{folder}_{e["sample"]}',bytes.fromhex(e['components'][2]['bytes'])[:176],context['tick'],context['fraction'])
    input_path=out/'input.bin';output_path=out/'cpp-output.bin';input_path.write_bytes(b''.join(vectors))
    exe=mod/'build/bin/Release/evr_root_interpolation_tests.exe'
    subprocess.run([str(exe),'--vectors',str(input_path),str(output_path)],check=True)
    actual=output_path.read_bytes()
    if len(actual)!=56*len(vectors):raise ValueError('C++ output length mismatch')
    checks=[]
    for n,reference in enumerate(expected):
        result=actual[n*56:(n+1)*56]
        position=max(abs(a-b) for a,b in zip(struct.unpack_from('<3q',result),struct.unpack_from('<3q',reference)))
        rotation=max(abs(a-b) for a,b in zip(struct.unpack_from('<4f',result,24),struct.unpack_from('<4f',reference,24)))
        scale=max(abs(a-b) for a,b in zip(struct.unpack_from('<3f',result,40),struct.unpack_from('<3f',reference,40)))
        checks.append(dict(name=labels[n],status='PASS' if position/2**32<1e-7 and rotation<2e-5 and scale<1e-6 else 'FAIL',position_q32=position,rotation_error=rotation,scale_error=scale))
    report=dict(checks=checks,exe_sha256=hashlib.sha256((mod.parent/'enshrouded.exe').read_bytes()).hexdigest(),
        implementation_sha256=hashlib.sha256(exe.read_bytes()).hexdigest(),input_sha256=hashlib.sha256(input_path.read_bytes()).hexdigest(),
        source='240 synthetic vectors and40 saved static-world samples; native CRT sin/acos substituted with float-rounded Python functions',
        max_position_q32=max(e['position_q32'] for e in checks),max_rotation_error=max(e['rotation_error'] for e in checks))
    (out/'report.json').write_text(json.dumps(report,indent=2))
    failed=sum(e['status']=='FAIL' for e in checks)
    print('RESULT',len(checks)-failed,'PASS',failed,'FAIL','max Q32',report['max_position_q32'],'max quaternion',report['max_rotation_error'])
    return int(bool(failed))


if __name__=='__main__':raise SystemExit(main())
