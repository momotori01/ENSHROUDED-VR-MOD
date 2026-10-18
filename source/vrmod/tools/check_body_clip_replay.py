"""Verify the captured upper-armour color-VS ablation; not a live MOD test.
The reviewed post-VS packing is capture/module-specific and fails closed.
"""
import argparse,json,math,pathlib,struct,hashlib

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('directory',type=pathlib.Path);p.add_argument('--recipe',type=pathlib.Path,required=True);a=p.parse_args()
 recipe=json.loads(a.recipe.read_text());rows=[json.loads(s) for s in (a.directory/'shader-comparison.jsonl').read_text().splitlines() if s.strip()]
 entries=[r for r in rows if 'entry' in r];meshes=[r for r in rows if 'mesh_event' in r];checks={}
 checks['single_color_module']=len(entries)==1 and recipe['mode']=='color' and hashlib.sha256((a.directory/'original_1.spv').read_bytes()).hexdigest()==recipe['original_sha256'] and hashlib.sha256((a.directory/'replacement_1.spv').read_bytes()).hexdigest()==recipe['patched_sha256']
 checks['reviewed_mesh_extent']=len(meshes)==5 and all((r['mesh_event'],r['vertex_stride'],r['index_stride'],r['indices'],r['base_vertex'],r['component_count'],r['component_bytes'],r['component_type'])==(8999,192,2,10734,0,4,4,1) for r in meshes)
 if not all(checks.values()):raise ValueError('unsupported color-only diagnostic extent/module')
 cases=['before','control','after','restored','restored_again'];verts={r['case']:(a.directory/r['vertices_file']).read_bytes() for r in meshes};base=verts['control'];after=verts['after']
 checks['same_vertex_extent']=all(len(v)==2429*192 for v in verts.values())
 checks['indices_preserved']=len({(a.directory/(s+'_mesh_8999_indices.bin')).read_bytes() for s in cases})==1
 checks['position_preserved']=all(base[n:n+16]==after[n:n+16] for n in range(0,len(base),192))
 checks['native_flat_output_preserved']=all(base[n+16:n+20]==after[n+20:n+24] for n in range(0,len(base),192))
 checks['native_remaining_varyings_preserved']=all(base[n+32:n+192]==after[n+32:n+192] for n in range(0,len(base),192))
 checks['original_outputs_restored']=all(verts[s]==base for s in ['before','restored','restored_again'])
 w=[struct.unpack_from('<f',base,n+12)[0] for n in range(0,len(base),192)];clip=[struct.unpack_from('<f',after,n+16)[0] for n in range(0,len(after),192)]
 distance=struct.unpack('<f',struct.pack('<f',recipe['distance']))[0];expected=[struct.unpack('<f',struct.pack('<f',v-distance))[0] for v in w]
 checks['clip_distance_exact']=clip==expected and all(math.isfinite(v) for v in clip)
 checks['part_of_mesh_retained']=0<sum(v>=0 for v in clip)<len(clip)
 textures=[r for r in rows if 'texture' in r];stats=[]
 for index in sorted({r['texture'] for r in textures}):
  image={r['case']:(a.directory/r['file']).read_bytes() for r in textures if r['texture']==index}
  if index==6:checks['native_mirror_unchanged']=len(set(image.values()))==1
  else:
   for case in ['before','after','restored','restored_again']:
    x=image['control'];y=image[case];d=[abs(v-u) for n,(v,u) in enumerate(zip(x,y)) if n%4!=3]
    stats.append(dict(texture=index,reference='control',case=case,mean_absolute_rgb=sum(d)/len(d),changed_rgb_components=sum(v!=0 for v in d),max_error=max(d)))
 report=dict(scope=__doc__,checks=checks,passed=sum(checks.values()),failed=len(checks)-sum(checks.values()),vertices=len(clip),retained_vertices=sum(v>=0 for v in clip),clipped_vertices=sum(v<0 for v in clip),distance=distance,image_stats=stats,limitations='Color-only ablation leaves native depth. Not a final rendering solution or proof of all poses; image stats include temporal replay variation.')
 (a.directory/'body-clip-check.json').write_text(json.dumps(report,indent=2)+'\n')
 for k,v in checks.items():print('PASS' if v else 'FAIL',k)
 print(report['passed'],'PASS /',report['failed'],'FAIL');return bool(report['failed'])
if __name__=='__main__':raise SystemExit(main())
