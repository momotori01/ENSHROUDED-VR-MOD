"""Validate bounded Windows PSS observations on our own disposable process."""
import json
import pathlib
import subprocess
import time

mod = pathlib.Path(__file__).resolve().parents[1]
fixture_exe = mod / 'build/bin/Release/evr_process_probe.exe'
snapshot_exe = mod / 'build/bin/Release/evr_thread_snapshot.exe'
out = mod / 'output' / time.strftime('snapshot-selftest-%Y%m%d-%H%M%S')
out.mkdir(parents=True)
fixture = subprocess.Popen([str(fixture_exe), '--fixture'], stdout=subprocess.PIPE, text=True)
try:
    pid, rva = fixture.stdout.readline().split()
    command = [str(snapshot_exe), '--pid', pid, '--count', '3', '--output', str(out / 'snapshots.jsonl'),
               '--expected-path', str(fixture_exe)]
    result = subprocess.run(command, capture_output=True, text=True, timeout=15)
    print(result.stdout, result.stderr, end='')
    assert result.returncode == 0
    rows = [json.loads(line) for line in (out / 'snapshots.jsonl').read_text().splitlines()]
    complete = [r for r in rows if r['event'] == 'snapshot_complete']
    threads = [r for r in rows if r['event'] == 'thread_snapshot']
    assert len(complete) == 3 and all(r['threads'] > 0 for r in complete)
    assert any(r['stack_read'] and r['stack_candidates'] for r in threads)
    assert all(r['suspend_count'] == 0 for r in threads), 'unexpected retained suspension'
    assert fixture.poll() is None, 'fixture did not survive'
    wrong = command.copy()
    wrong[-1] = str(snapshot_exe)
    rejected = subprocess.run(wrong, capture_output=True, text=True, timeout=5)
    assert rejected.returncode != 0 and 'mismatch' in rejected.stderr
    assert fixture.poll() is None
    print('PASS contexts, bounded stack candidates, no retained suspension, target alive, identity mismatch rejected')
    print('Capture microseconds:', [r['capture_us'] for r in complete])
finally:
    fixture.terminate()
    fixture.wait(timeout=5)
