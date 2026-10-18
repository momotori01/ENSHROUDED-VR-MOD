"""Offline-only transparent background experiment for one captured post-upscale shader.

This does not implement a live UI hook. It replaces the sole image-write value
with the shader's existing float4 zero, leaving resource interfaces intact.
Game-derived input/output stays local; only this recipe is original source.
"""
import argparse,hashlib,json,pathlib,struct

EXPECTED='8e2736b877b40e31e2ab16d67f36b69b9487cd8b17b8fa5b81ac58e6d271e597'

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('source',type=pathlib.Path);p.add_argument('output',type=pathlib.Path)
    a=p.parse_args();raw=a.source.read_bytes()
    if hashlib.sha256(raw).hexdigest()!=EXPECTED:raise ValueError('unrecognised captured shader')
    words=list(struct.unpack('<'+'I'*(len(raw)//4),raw))
    floats=set();vectors=set();zeros=set();zero_vectors=[];writes=[];i=5
    while i<len(words):
        count,op=words[i]>>16,words[i]&65535
        if not count or i+count>len(words):raise ValueError('instruction bounds')
        args=words[i+1:i+count]
        if op==22 and args[1]==32:floats.add(args[0])
        elif op==23 and args[1] in floats and args[2]==4:vectors.add(args[0])
        elif op==43 and args[0] in floats and args[2:]==[0]:zeros.add(args[1])
        elif op==44 and args[0] in vectors and len(args)==6 and all(x in zeros for x in args[2:]):zero_vectors.append(args[1])
        elif op==99:writes.append(i)
        i+=count
    if len(zero_vectors)!=1 or len(writes)!=1:raise ValueError('ambiguous zero or image write')
    at=writes[0]+3;old=words[at];words[at]=zero_vectors[0]
    patched=struct.pack('<'+'I'*len(words),*words)
    a.output.parent.mkdir(parents=True,exist_ok=True);a.output.write_bytes(patched)
    a.output.with_suffix('.json').write_text(json.dumps(dict(offline_only=True,source=str(a.source),
        original_sha256=EXPECTED,patched_sha256=hashlib.sha256(patched).hexdigest(),
        changed_word=at,old_value_id=old,new_zero_id=zero_vectors[0],image_write_count=1),indent=2)+'\n')
    print('PASS one image-write value replaced by existing float4 zero; offline replay only')

if __name__=='__main__':main()
