"""Compare explicitly selected live image ranges from RenderDoc memory API exports.

Resource IDs belong to one capture. Range lengths must be established from the
image format/extent or Vulkan requirements. This does not certify all resources,
temporal lifetime, synchronization, or image quality.
"""
import argparse
import itertools
import json
from pathlib import Path


def objects(node):
    yield node
    for child in node['children']:
        yield from objects(child)


def image_bindings(records):
    result={}
    for record in records:
        chunk=record['chunk']
        if chunk['name'] not in ('vkBindImageMemory','vkBindImageMemory2','vkBindImageMemory2KHR'):
            continue
        candidates=[chunk] if chunk['name']=='vkBindImageMemory' else (
            obj for obj in objects(chunk) if obj['type']=='VkBindImageMemoryInfo' and obj['basic']==1)
        for obj in candidates:
            fields={child['name']:child.get('value') for child in obj['children']}
            if not all(key in fields for key in ('image','memory','memoryOffset')):
                raise ValueError('incomplete image binding')
            image,memory,offset=(int(fields[key]) for key in ('image','memory','memoryOffset'))
            binding=dict(memory=memory,offset=offset,chunk=record['chunk_index'])
            if image in result and result[image]!=binding:
                raise ValueError('multiple bindings for an image require manual lifetime analysis')
            result[image]=binding
    return result


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('directory',type=Path)
    parser.add_argument('--resource-ranges',nargs='+',required=True,help='ResourceId:byte_length for simultaneously live images')
    args=parser.parse_args()
    ranges={}
    for token in args.resource_ranges:
        image,length=map(int,token.split(':'))
        if image in ranges or image<=0 or not 1<=length<=16*1024**3:
            raise ValueError('invalid or repeated resource range')
        ranges[image]=length
    if len(ranges)<2:raise ValueError('at least two simultaneous resources are required')
    bindings=image_bindings([json.loads(s) for s in (args.directory/'memory-api.jsonl').read_text().splitlines()])
    checks={'all_selected_images_bound':all(image in bindings for image in ranges)}
    conflicts=[]
    for left,right in itertools.combinations(ranges,2):
        if left not in bindings or right not in bindings:continue
        a,b=bindings[left],bindings[right]
        overlap=a['memory']==b['memory'] and max(a['offset'],b['offset'])<min(a['offset']+ranges[left],b['offset']+ranges[right])
        checks[f'images_{left}_{right}_disjoint']=not overlap
        if overlap:conflicts.append([left,right])
    report=dict(scope=__doc__,checks={k:'PASS' if v else 'FAIL' for k,v in checks.items()},
        selected={str(image):dict(bytes=length,binding=bindings.get(image)) for image,length in ranges.items()},conflicts=conflicts)
    (args.directory/'selected-memory-checks.json').write_text(json.dumps(report,indent=2))
    for key,value in report['checks'].items():print(value,key)
    print('RESULT',sum(checks.values()),'PASS',len(checks)-sum(checks.values()),'FAIL')
    return int(not all(checks.values()))


if __name__=='__main__':raise SystemExit(main())
