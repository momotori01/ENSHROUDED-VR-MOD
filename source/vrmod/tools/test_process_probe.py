"""Exercise debugger attach/hardware breakpoints/restore on our own child fixture."""
import json,pathlib,subprocess,time
mod=pathlib.Path(__file__).resolve().parents[1];exe=mod/'build/bin/Release/evr_process_probe.exe'
out=mod/'output'/time.strftime('debugger-selftest-%Y%m%d-%H%M%S');out.mkdir(parents=True)
fixture=subprocess.Popen([str(exe),'--fixture'],stdout=subprocess.PIPE,text=True)
try:
    pid,rva=fixture.stdout.readline().split()
    run=subprocess.run([str(exe),'--pid',pid,'--rva',rva,'--samples','20','--seconds','5','--fixture-pointers','--output',str(out/'probe.jsonl')],capture_output=True,text=True,timeout=15)
    (out/'result.txt').write_text(run.stdout+run.stderr)
    print(run.stdout,run.stderr,end='')
    if run.returncode:raise RuntimeError('debugger failed')
    rows=[json.loads(s) for s in (out/'probe.jsonl').read_text().splitlines()]
    samples=[r for r in rows if r['event']=='camera_query_sample']
    assert len(samples)==20,'sample count'
    assert all(r['query_read'] and r['components'][0]['bytes'].startswith('7856341200000000') for r in samples),'fixture memory mismatch'
    time.sleep(.25)
    assert fixture.poll() is None,'fixture did not survive detach'
    print('PASS 20 hardware-breakpoint snapshots match fixture; target alive after detach')
    timed=subprocess.run([str(exe),'--pid',pid,'--rva',rva,'--samples','500','--seconds','1','--fixture-pointers','--output',str(out/'timed.jsonl')],capture_output=True,text=True,timeout=10)
    print(timed.stdout,timed.stderr,end='')
    assert timed.returncode==0,'timer detach failed'
    timedrows=[json.loads(s) for s in (out/'timed.jsonl').read_text().splitlines()]
    assert 0<len([r for r in timedrows if r['event']=='camera_query_sample'])<500,'timer bound not exercised'
    time.sleep(.25)
    assert fixture.poll() is None,'fixture did not survive timer detach'
    print('PASS timer shutdown, second attach, and debug-register restoration')
finally:
    fixture.terminate();fixture.wait(timeout=5)
