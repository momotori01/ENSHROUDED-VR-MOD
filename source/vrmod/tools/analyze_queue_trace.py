"""Summarize bounded legacy vkQueueSubmit observations; never infer GPU completion.

Call contexts are thread-local call ancestry, not ownership of downstream layers.
Command-buffer handles may be recycled; their intersection is not a frame identity.
"""
import argparse
import collections
import hashlib
import json
import pathlib


def analyze(events):
    submissions = [e for e in events if e.get('event') == 'queue_submit']
    groups = collections.defaultdict(list)
    for e in submissions:
        groups[(e['queue'], e['family'], e['thread'], e.get('call_context', 'unclassified'),
                tuple(e['game_rva_stack']))].append(e)
    observed_types = collections.Counter(s['type'] for e in submissions for b in e['batches']
                                         for k in ('waits', 'signals') for s in b[k])
    checks = {
        'submission_observations_exist': bool(submissions),
        'recorded_submissions_accepted': bool(submissions) and all(e['result'] == 0 for e in submissions),
        'metadata_not_truncated': bool(submissions) and all(not e['truncated'] for e in submissions),
        'semaphore_types_known': bool(submissions) and 'unknown' not in observed_types,
        'call_context_classified': bool(submissions) and all('call_context' in e for e in submissions),
    }
    pipeline_commands = {e['cmd'] for e in events if e.get('event') == 'trace_pipeline'}
    result = dict(submission_count=len(submissions), semaphore_entries_by_type=dict(observed_types),
                  checks={k: 'PASS' if v else 'FAIL' for k, v in checks.items()}, groups=[])
    for (queue, family, thread, context, stack), rows in groups.items():
        commands = {c for e in rows for b in e['batches'] for c in b['commands']}
        result['groups'].append(dict(queue=queue, family=family, thread=thread, call_context=context,
            observed_modules=sorted({s['module'] for e in rows for s in e.get('module_stack', []) if s['module']}),
            game_rva_stack=list(stack), count=len(rows), first_present=min(e['last_present'] for e in rows),
            last_present=max(e['last_present'] for e in rows), command_handle_count=len(commands),
            handles_also_seen_in_pipeline_trace=len(commands & pipeline_commands),
            cpu_call_us_max=max(e['cpu_call_us'] for e in rows)))
    result['limits'] = [
        'Only bounded legacy vkQueueSubmit observations; excludes vkQueueSubmit2/KHR and vkQueueBindSparse.',
        'VK_SUCCESS proves acceptance, not fence completion or queue idle.',
        'last_present is a layer counter, not a simulation tick or a per-eye frame token.',
        'Call ancestry on this thread does not identify which downstream layer caused a call.',
        'Binary semaphore value fields are ignored by Vulkan, including non-null timeline-submit slots.',
        'Unknown or earlier semaphore signals and command-buffer reuse prevent complete dependency reconstruction.',
        'A single-thread menu sample does not prove world rendering is single-threaded.'
    ]
    return result


def self_test():
    event = dict(event='queue_submit', queue='q', family=0, thread=1, game_rva_stack=[],
                 call_context='outside_layer_present', last_present=5, result=0, truncated=False,
                 cpu_call_us=1, batches=[dict(commands=['c'], waits=[], signals=[
                     dict(semaphore='s', type='binary', value=0)])])
    report = analyze([event])
    assert all(v == 'PASS' for v in report['checks'].values())
    assert report['semaphore_entries_by_type'] == {'binary': 1}
    assert analyze([])['checks']['recorded_submissions_accepted'] == 'FAIL'
    assert analyze([{**event, 'result': -4}])['checks']['recorded_submissions_accepted'] == 'FAIL'
    assert analyze([{**event, 'truncated': True}])['checks']['metadata_not_truncated'] == 'FAIL'
    missing = {k: v for k, v in event.items() if k != 'call_context'}
    assert analyze([missing])['checks']['call_context_classified'] == 'FAIL'
    downstream = {**event, 'call_context': 'downstream_present'}
    assert len(analyze([event, downstream])['groups']) == 2
    print('PASS analyzer negative cases and call-context separation')


if __name__ == '__main__':
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('trace', nargs='?', type=pathlib.Path)
    p.add_argument('--output', type=pathlib.Path)
    p.add_argument('--self-test', action='store_true')
    args = p.parse_args()
    if args.self_test:
        self_test()
    if args.trace:
        raw = args.trace.read_bytes()
        report = analyze([json.loads(line) for line in raw.decode('utf-8-sig').splitlines() if line.strip()])
        report.update(source=str(args.trace.resolve()), sha256=hashlib.sha256(raw).hexdigest())
        destination = args.output or args.trace.with_name('queue-analysis.json')
        destination.parent.mkdir(parents=True, exist_ok=True)
        destination.write_text(json.dumps(report, indent=2), encoding='utf8')
        for check, status in report['checks'].items():
            print(status, check)
        print('Recorded submissions:', report['submission_count'], 'Results:', destination)
        raise SystemExit(0 if all(v == 'PASS' for v in report['checks'].values()) else 1)
    elif not args.self_test:
        p.error('trace or --self-test required')
