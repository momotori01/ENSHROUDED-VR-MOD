"""Finite native quick-wheel choreographies derived from world-quick-wheel-hands.csv.
Only the right grip and right stick columns change; each selection is released while
the stick is still held, then the stick returns to physical neutral. Diagnostic only;
frame placement is not a product long-press timing.
  rebind:     up-right / up / up-right (weapon -> other -> weapon)
  consumable: down / up-right (v1; down kept the prior selection, see game165733)
  consumable2: right / up-right (consumable -> weapon), never any face button
  survey:     bow / torch / staff / mace / axe slot centres, never a consumable
  router:     right grip long/short/zone presses for GripRouter (RB is never a CSV column)
  equip:      right hip / left shoulder / right shoulder zone presses executed via WheelSelect
"""
import argparse,math
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
BASE=ROOT/'test_env/world-quick-wheel-hands.csv'
DIAGONAL=round(math.sqrt(.5),8)
# mode -> (output, header, cycles as (grip first, grip last, stick first, stick last, x, y))
MODES={
 'rebind':(ROOT/'test_env/world-quick-wheel-rebind-hands.csv','# Finite native wheel rebind: grips 450..529/580..659/710..789, up-right/up/up-right held across each release, then physical neutral.',
  [(450,529,480,539,DIAGONAL,DIAGONAL),(580,659,610,669,0.0,1.0),(710,789,740,799,DIAGONAL,DIAGONAL)]),
 'consumable':(ROOT/'test_env/world-quick-wheel-consumable-hands.csv','# Finite native wheel consumable observation: grips 450..529/580..659, down then up-right held across each release, then physical neutral. No face buttons.',
  [(450,529,480,539,0.0,-1.0),(580,659,610,669,DIAGONAL,DIAGONAL)]),
 # game165733: straight down is outside the half-ring and kept the axe; the berry is at ~91 deg.
 'consumable2':(ROOT/'test_env/world-quick-wheel-consumable2-hands.csv','# Finite native wheel consumable observation v2: grips 450..529/580..659, right (berry ~91 deg) then up-right held across each release, then physical neutral. No face buttons.',
  [(450,529,480,539,1.0,0.0),(580,659,610,669,DIAGONAL,DIAGONAL)]),
 # Weapon/tool slot survey at the visual slot centres seen in the wheel images:
 # bow -65, torch -13, staff +13, mace +65, axe +39 degrees clockwise from up.
 # GripRouter: long press 450..549 (+up-right), short press 600..609, right hand moved to the
 # synthetic right-shoulder zone 650..709 with a press 660..699, long press 730..819 (+up-right).
 'router':(ROOT/'test_env/world-grip-router-wheel-hands.csv','# Finite GripRouter wheel: right grip long 450..549, short 600..609, shoulder-zone press 660..699 (hand at zone 650..709), long 730..819; up-right held across each long release. RB comes only from the router.',
  [(450,549,500,559,DIAGONAL,DIAGONAL),(600,609,0,-1,0.0,0.0),(660,699,0,-1,0.0,0.0),(730,819,770,829,DIAGONAL,DIAGONAL)],[(650,709,0.22,1.45,0.05)]),
 # Router equip: left grip released by default; right hand at the right-hip zone 450..489
 # (press 460..469), left hand at the left-shoulder zone 540..579 (press 550..559), right hand
 # at the right-shoulder zone 640..679 (press 650..659). Stick never moved by the CSV.
 'equip':(ROOT/'test_env/world-grip-router-equip-hands.csv','# Finite GripRouter equip: right-hip press 460..469, left-shoulder press 550..559, right-shoulder press 650..659; wheel RB/stick come only from WheelSelect toward planned weapon slots.',
  [(460,469,0,-1,0.0,0.0),(650,659,0,-1,0.0,0.0)],[(450,489,0.22,1.00,0.0),(640,679,0.22,1.45,0.05)],
  dict(left_grip_default=0,left_presses=[(550,559)],left_positions=[(540,579,-0.22,1.45,0.05)])),
 'survey':(ROOT/'test_env/world-quick-wheel-survey-hands.csv','# Finite native wheel slot survey: grips 450/530/610/690/770 for 60 frames, bow/torch/staff/mace/axe directions held across each release, then physical neutral. No consumables, no face buttons.',
  [(450,509,465,519,-0.90630779,0.42261826),(530,589,545,599,-0.22495105,0.97437006),(610,669,625,679,0.22495105,0.97437006),(690,749,705,759,0.90630779,0.42261826),(770,829,785,839,0.62932039,0.77714596)]),
}
RIGHT_GRIP,RIGHT_X,RIGHT_Y=24,32,33
RIGHT_POSITION=(14,15,16)
LEFT_POSITION,LEFT_GRIP=(1,2,3),11

def text(v):
 return str(int(v)) if float(v).is_integer() else repr(float(v))

def generate(mode='rebind'):
 _,header,cycles,*rest=MODES[mode];positions=rest[0] if rest else [];left=rest[1] if len(rest)>1 else {}
 rows=[l.split(',') for l in BASE.read_text().splitlines() if l and not l.startswith('#')]
 if len(rows)!=1000 or any(len(r)!=35 for r in rows) or any(int(r[0])!=i+1 for i,r in enumerate(rows)):raise ValueError('unexpected base wheel fixture')
 base_static=rows[0]
 for r in rows:
  r[RIGHT_GRIP]='0';r[RIGHT_X]='0';r[RIGHT_Y]='0'
  if r!=[str(r[0])]+base_static[1:]:raise ValueError('base fixture has motion outside right grip/stick')
 if 'left_grip_default' in left:
  for r in rows:r[LEFT_GRIP]=text(left['left_grip_default'])
 for grip0,grip1,stick0,stick1,x,y in cycles:
  for f in range(grip0,grip1+1):rows[f-1][RIGHT_GRIP]='1'
  for f in range(stick0,stick1+1):rows[f-1][RIGHT_X]=text(x);rows[f-1][RIGHT_Y]=text(y)
 for first,last,x,y,z in positions:
  for f in range(first,last+1):
   for column,value in zip(RIGHT_POSITION,(x,y,z)):rows[f-1][column]=text(value)
 for first,last in left.get('left_presses',[]):
  for f in range(first,last+1):rows[f-1][LEFT_GRIP]='1'
 for first,last,x,y,z in left.get('left_positions',[]):
  for f in range(first,last+1):
   for column,value in zip(LEFT_POSITION,(x,y,z)):rows[f-1][column]=text(value)
 return [header]+[','.join(r) for r in rows]

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--verify',action='store_true');p.add_argument('--mode',choices=sorted(MODES),default='rebind');a=p.parse_args()
 output=MODES[a.mode][0];lines=generate(a.mode);content='\n'.join(lines)+'\n'
 if a.verify:
  if output.read_text()!=content:raise SystemExit(f'FAIL {a.mode} fixture differs from generator')
  print(f'PASS {a.mode} fixture matches generator',len(lines)-1,'rows');return
 if output.exists():raise SystemExit('fixture exists; use --verify instead of overwriting evidence input')
 output.write_text(content);print('WROTE',output,len(lines)-1,'rows')

if __name__=='__main__':main()
