"""Native pool ownership evidence, distinct from physical GPU memory range checks."""
import collections


def check_pools(records, resolution=True, transient=False):
    # transient (2026-10-06): eye-private transient resolution pools (world-eye-private-transient.txt, default for live XR).
    created=[e for e in records if e['event']=='private_native_memory_pool_created']
    released=[e for e in records if e['event']=='private_native_memory_pool_released']
    expected={(eye,kind) for eye in (0,1) for kind in (('feature','resolution') if resolution else ('feature',))+(('transient',) if transient else ())}
    identities=lambda events:collections.Counter((e['eye'],e['kind'],e['pool']) for e in events)
    originals={e['source_pool'] for e in created}
    return {
        'private_native_pool_ownership':len(created)==len(expected) and
            {(e['eye'],e['kind']) for e in created}==expected and
            len({e['pool'] for e in created})==len(expected) and
            all(e['pool'] and e['pool'] not in originals for e in created),
        'private_native_pool_configuration':bool(created) and
            all(e['configuration_copied_from_native'] for e in created),
        'private_native_pools_released_once':bool(created) and identities(created)==identities(released) and
            all(sum(r['pool']==e['pool'] and r['us']>e['us'] for r in released)==1 for e in created),
    }
