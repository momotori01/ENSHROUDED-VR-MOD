"""Locate swapchain render-pass candidates in an existing local GPU capture.

Draw counts are candidates only: image differences/shader state must establish
which pass is native UI, scene compositing, cursor, or a third-party overlay.
"""
import argparse,json,pathlib

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('inventory',type=pathlib.Path);p.add_argument('--output',type=pathlib.Path,required=True);a=p.parse_args()
    actions=[json.loads(s) for s in (a.inventory/'actions.jsonl').read_text().splitlines()]
    present=next(e for e in reversed(actions) if e['name'].startswith('vkQueuePresentKHR'))['copy_destination']
    passes=[];current=None
    for e in actions:
        if e['name'].startswith(('vkCmdBeginRendering(','vkCmdBeginRenderPass(')):
            if current:raise ValueError('nested render pass or incomplete action inventory')
            current=dict(begin=e['event'],end=None,name=e['name'],outputs=e['outputs'],draws=[])
        elif current and e['name'].startswith(('vkCmdEndRendering(','vkCmdEndRenderPass(')):
            current['end']=e['event'];passes.append(current);current=None
        elif current and e['flags']&2:current['draws'].append(e['event'])
    candidates=[e for e in passes if present in e['outputs']]
    report=dict(scope=__doc__,inventory=str(a.inventory),present_texture_index=present,passes=candidates)
    a.output.parent.mkdir(parents=True,exist_ok=True);a.output.write_text(json.dumps(report,indent=2)+'\n')
    for e in candidates:print('candidate',e['begin'],e['end'],'draws',len(e['draws']),'first/last',e['draws'][:3],e['draws'][-3:])
if __name__=='__main__':main()
