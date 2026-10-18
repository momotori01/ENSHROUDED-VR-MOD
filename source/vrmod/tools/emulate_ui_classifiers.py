"""Execute the game's pure UI classifiers in isolated Unicorn; no UI operations."""
import json,pathlib,struct
from emulate_camera_math import CameraMath,SHA256
from unicorn.x86_const import UC_X86_REG_RAX

def main():
    e=CameraMath(pathlib.Path('enshrouded.exe'));e.allowed=[(0xfb7800,0xfb7829),(0xfb7880,0xfb7893)]
    names={v['value']:v['name'] for v in json.loads(pathlib.Path('docs/analysis/hud_screen_enum.json').read_text())['types'][0]['values']}
    rows=[]
    for value in range(256):
        e.invoke(0xfb7800,value);fullscreen=e.cpu.reg_read(UC_X86_REG_RAX)&255
        e.invoke(0xfb7880,value);loot=e.cpu.reg_read(UC_X86_REG_RAX)&255
        rows.append(dict(value=value,name=names.get(value,'unknown'),native_fullscreen=bool(fullscreen),native_loot=bool(loot),
                         proposed_gameplay_allowed=value==1,boolean_results=fullscreen in (0,1) and loot in (0,1)))
    table=bytes(e.cpu.mem_read(e.base+0xfb7834,50));dest=struct.unpack('<2I',e.cpu.mem_read(e.base+0xfb782c,8))
    checks={
        'all_256_results_are_boolean':all(r['boolean_results'] for r in rows),
        'native_branch_table_matches_execution':all(rows[i]['native_fullscreen']==(dest[table[i]]==0xfb7823) for i in range(50)),
        'native_loot_only_two_chest_screens':[r['value'] for r in rows if r['native_loot']]==[20,21],
        'normal_hud_is_not_native_fullscreen':not rows[1]['native_fullscreen'] and names[1]=='InGame',
        'menus_and_character_inventory_are_fullscreen':all(rows[i]['native_fullscreen'] for i in (2,3,5,6,7,8,9,10,25,40)),
        'negative_control_non_fullscreen_does_not_mean_gameplay':all(not rows[i]['native_fullscreen'] and not rows[i]['proposed_gameplay_allowed'] for i in (0,20,28,30,31,33,42,44)),
        'unknown_enum_values_are_not_accepted':[r['value'] for r in rows if r['proposed_gameplay_allowed']]==[1],
    }
    report=dict(exe_sha256=SHA256,scope=__doc__,checks=checks,rows=rows,substitutions=e.calls)
    pathlib.Path('docs/analysis/emulated_ui_classifiers.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf8')
    for k,v in checks.items():print('PASS' if v else 'FAIL',k)
    print(sum(checks.values()),'PASS /',len(checks)-sum(checks.values()),'FAIL');return int(not all(checks.values()))

if __name__=='__main__':raise SystemExit(main())
