"""Check native visibility-history ownership/lifecycle; GPU isolation needs RDC evidence too."""
import argparse
import collections
import json
from pathlib import Path

def check_history(rows, rendered):
    def events(name):return [e for e in rows if e['event']==name]
    made=events('culling_history_created');freed=events('culling_history_released')
    subs=events('culling_history_substituted')
    hooks=events('culling_history_hook_enabled');removed=events('culling_history_hook_removed')
    checks={
        'culling_history_two_owned_buffers':len(made)==2 and {e['eye'] for e in made}=={0,1} and len({e['object'] for e in made})==2 and all(e['object']!=e['source_object'] and e['index']!=e['source_index'] for e in made),
        'culling_history_runtime_descriptor':len(made)==2 and len({(e['manager'],e['graph'],e['bytes'],e['source_object']) for e in made})==1 and all(0<e['bytes']<=65536*4 and e['bytes']%4==0 and e['usage']==19 and e.get('source_usage')==16 for e in made),
        'culling_history_owned_buffers_released':len(made)==len(freed)==2 and {(e['eye'],e['index']) for e in made}=={(e['eye'],e['index']) for e in freed},
        'culling_history_no_failure':not events('culling_history_failure'),
        'culling_history_native_memory_owned':len(made)==2 and all(e.get('owned_allocation',0)>0 for e in made),
    }
    if rendered:
        pairs=events('live_native_pair_complete')
        ablation=events('culling_history_fill_only')
        if ablation:
            checks['culling_history_fill_only_preserved_references']=len(ablation)==2 and {e['eye'] for e in ablation}=={0,1} and not subs and all(e['descriptor_unchanged'] for e in ablation)
            checks['culling_history_fill_only_completed']=len(freed)==2 and bool(pairs) and all(e['substitutions']==0 and e['reset_commands']>=1 for e in freed)
        else:
            measured=events('culling_history_eye')
            if measured:
                # Native preparation can skip an eye before both views are ready.
                # Prove each submitted eye, and reconcile all lifetime calls,
                # rather than requiring equal totals including warmup.
                serials=[e['serial'] for e in events('calibration_pair_recorded')]
                selected=[e for e in measured if e['serial'] in serials]
                totals=collections.Counter()
                for e in measured:totals[e['eye']]+=e['substitutions']
                checks['culling_history_both_eyes_substituted']=bool(serials) and len(freed)==2 and all(e['substitutions']==1 for e in selected) and collections.Counter(
                    (e['serial'],e['eye']) for e in selected)==collections.Counter((s,i) for s in serials for i in (0,1))
                checks['culling_history_lifetime_calls_accounted']=len(freed)==2 and all(e['substitutions']==totals[e['eye']] and e['reset_commands']>=1 for e in freed)
            else:
                # Keep earlier evidence's verdict: aggregate counts alone cannot
                # establish whether an imbalance occurred only during warmup.
                checks['culling_history_both_eyes_substituted']=len(freed)==2 and bool(pairs) and len({e['substitutions'] for e in freed})==1 and all(e['substitutions']>=len(pairs) and e['reset_commands']>=1 for e in freed)
            checks['culling_history_original_descriptors_preserved']=bool(subs) and {e['eye'] for e in subs}=={0,1} and all(e['source_unchanged'] and e['other_descriptors_unchanged'] for e in subs)
        generated=events('generated_instance_count_reset')
        count_hook=any(int(str(e['rva']),16) in (0x7caa0,0xaabc0,0xaac20,0xaac50,0xaacc0) for e in hooks)   # June 7caa0, 1.0 aabc0, Hotfix 1.0.0.1 aac20, build 25988036 aac50, build 26082781 aacc0
        checks['culling_history_hooks_restored']=len(hooks)==len(removed)==(4 if count_hook else 3) and {e['rva'] for e in hooks}=={e['rva'] for e in removed} and all(e['original_bytes_restored'] for e in removed)
        if count_hook:
            # With vfx_model_bridge.hpp (-ShareVfxModels) the eye that does not own the VFX culls the VFX models itself,
            # so it starts from the game's own reset and makes its own generated submodels (culling_history_bridge.hpp
            # eyeRegeneratesModels): every eye of a frame resets and nothing is borrowed. A bridge that failed goes
            # back to borrowing, which is then not what this run was asked to show.
            # Since 2026-10-13 that eye keeps the owner's generated parts as well and appends its own (the flying rocks
            # of an explosion are the owner's; generated_instance_keep_policy): it borrows like an eye without the
            # bridge, except in the frames put on record as generated_instance_keep_fallback, where it resets.
            policy=events('generated_instance_keep_policy')
            kept=bool(policy) and policy[-1].get('keep_the_owners_parts_in_an_eye_that_culls_its_own_models') is True
            fallback={(e['serial'],e['eye']) for e in events('generated_instance_keep_fallback')}
            regenerating=bool(events('vfx_model_bridge_enabled')) and not events('vfx_model_bridge_failure') and not kept
            # Alternate-eye rendering (2026-10-14, alternate_eyes_policy): after the warm-up one eye is drawn a frame; it owns the
            # frame and resets alone, so a pair's rows are that eye's only, and the lifetime audit sees one eye a pair.
            policies=events('alternate_eyes_policy');alternate=bool(policies) and policies[-1].get('enabled') is True
            serials={e['serial'] for e in events('calibration_pair_recorded')}
            by_serial=collections.defaultdict(list)
            # 1.0 (2026-10-16): the first eye of the first frame resets as the game does (generated_instance_first_frame_reset):
            # that frame's original View was drawn before the pair began, and there is nothing to borrow. Not a pair's audit.
            first={e['serial'] for e in events('generated_instance_first_frame_reset')}
            # With the VFX model bridge working again (Hotfix 1.0.0.1, 2026-10-16) the same first eye is put on record
            # as a keep fallback instead: a frame with no row of its owner (-1) whose first eye reset as the game does.
            if generated:
                s0=min(r['serial'] for r in generated);f0=[r for r in generated if r['serial']==s0]
                if f0[0]['owner_eye']==-1 and all(r['eye']!=-1 for r in f0) and f0[0]['reset'] and (s0,f0[0]['eye']) in fallback:first.add(s0)
            for row in generated:
                if row['serial'] not in first:by_serial[row['serial']].append(row)
            # Since 2026-10-15 09:15 (culling_history_bridge.hpp, the fix of the light-only copies): in the frame that also
            # draws the game's own view (eye -1) while an eye owns the VFX, that view resets by itself first and leaves no
            # proof; the owner's and the borrower's rows follow as before. That leading row is not part of the pair's audit.
            def body(frame):
                if len(frame)>1 and frame[0]['eye']==-1 and frame[0]['owner_eye']>=0 and frame[0]['reset'] and frame[0].get('source_unchanged') is True:return frame[1:]
                return frame
            def valid_reset_frame(frame):
                frame=body(frame)
                if not frame:return False
                owner=frame[0]['owner_eye']
                expected=[-1,0,1] if owner==-1 else [0,1] if owner==0 else []
                if alternate and owner>=0 and [e['eye'] for e in frame]==[owner]:expected=[owner]
                return bool(expected) and [e['eye'] for e in frame]==expected and all(
                    e['owner_eye']==owner and e['reset']==(regenerating or e['eye']==owner or (e['serial'],e['eye']) in fallback) and
                    e.get('source_unchanged') is True and e.get('count_bytes')==4 for e in frame)
            summaries=events('generated_instance_count_restored')
            if summaries:
                summary=summaries[-1]
                calls=summary.get('calls',[])
                sample_serials=serials.intersection(by_serial)
                # Per-name cap may truncate the final three-row sample. Lifetime
                # native audits remain active and the terminal summary is uncapped.
                # (The cap is 3000 rows, or what -LogNameCap gave: a whole number of thousands.)
                if len(generated)>=3000 and len(generated)%1000==0 and sample_serials:sample_serials.discard(max(by_serial))
                checks['generated_instance_count_shared_per_pair']=bool(sample_serials) and all(valid_reset_frame(by_serial.get(s,[])) for s in sample_serials)
                checks['generated_instance_count_lifetime_audited']=len(summaries)==1 and len(calls)==3 and summary.get('source_unchanged') is True and summary.get('failed') is False and summary.get('audited_eyes',0)>=(len(serials) if alternate else 2*len(serials)) and summary.get('resets',0)>0 and (summary.get('borrows',-1)==0 and summary['resets']>=2*len(serials) if regenerating else summary.get('borrows',0)>=len(serials)-len(fallback)) and sum(calls)==summary['resets']+summary['borrows']
            else:
                checks['generated_instance_count_shared_per_pair']=bool(serials) and all(valid_reset_frame(by_serial.get(s,[])) for s in serials)
            checks['generated_instance_count_borrower_has_proof']=all(
                frame[0]['reset'] and frame[0]['eye']==frame[0]['owner_eye'] and
                sum(e['reset'] for e in frame)==(len(frame) if regenerating else 1+sum((e['serial'],e['eye']) in fallback for e in frame)) and all(e['owner_eye']==frame[0]['owner_eye'] and
                    e['reset']==(regenerating or e['eye']==e['owner_eye'] or (e['serial'],e['eye']) in fallback) for e in frame)
                for frame in (body(f) for f in by_serial.values()))
    else:
        checks['culling_history_unused_not_written']=len(freed)==2 and not subs and not hooks and not removed and all(e['substitutions']==e['reset_commands']==0 for e in freed)
    return checks

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('probe',type=Path);p.add_argument('--rendered',action='store_true')
    p.add_argument('--generated-only',action='store_true',help='Audit generated-instance reset samples plus uncapped lifetime proof; not full native visibility-history evidence')
    p.add_argument('--report',type=Path);a=p.parse_args()
    checks=check_history([json.loads(s) for s in (a.probe/'view-resource-probe.jsonl').read_text(encoding='utf-8').splitlines()],a.rendered)
    if a.generated_only:
        checks={k:v for k,v in checks.items() if k.startswith('generated_instance_') or k in ('culling_history_hooks_restored','culling_history_no_failure')}
        checks['generated_instance_lifetime_evidence_present']='generated_instance_count_lifetime_audited' in checks
    path=a.report or a.probe/('generated-instance-checks.json' if a.generated_only else 'culling-history-checks.json')
    report={k:'PASS' if v else 'FAIL' for k,v in checks.items()};path.write_text(json.dumps(report,indent=2))
    for k,v in report.items():print(v,k)
    print('PASS',sum(checks.values()),'FAIL',len(checks)-sum(checks.values()));return int(not all(checks.values()))
if __name__=='__main__':raise SystemExit(main())
