#include <evr/equip_planner.hpp>
#include <iostream>
#include <stdexcept>
#include <string>
using namespace evr;using namespace evr::motion;
// game175428 actionbar 7313 (item, count, entity) with ItemInfo category/equipment slot.
ActionbarSnapshot observed(uint8_t selected=5){
 ActionbarSnapshot a;a.valid=true;a.selected=selected;a.barEntity=7313;a.milliseconds=1000;
 a.slots={ActionbarSlot{3436474234u,2,0,7,37,true},ActionbarSlot{327157552u,1,7328,2,5,true},ActionbarSlot{3102924359u,1,0,7,37,true},
          ActionbarSlot{3474096535u,1,7331,2,1,true},ActionbarSlot{3851016100u,1,7327,2,4,true},ActionbarSlot{1940764111u,1,7326,2,1,true},
          ActionbarSlot{3936779840u,1,7329,2,1,true},ActionbarSlot{}};
 return a;
}
int main(){unsigned n=0;try{
 auto check=[&](bool b){++n;if(!b)throw std::runtime_error("equip planner invariant "+std::to_string(n));};
 // Role -> slot on the observed bar.
 {auto a=observed(5);
  auto p=planEquip(a,EquipRequest::Staff);check(p.result==EquipPlanResult::Target&&p.slot==4);
  p=planEquip(a,EquipRequest::Bow);check(p.result==EquipPlanResult::Target&&p.slot==1);
  p=planEquip(a,EquipRequest::MainWeapon,5);check(p.result==EquipPlanResult::AlreadySelected&&p.slot==5);
  p=planEquip(a,EquipRequest::Shield);check(p.result==EquipPlanResult::NoMatchingSlot&&p.slot==255);}
 // Main weapon prefers the remembered melee slot over the first melee item on the bar.
 {auto a=observed(4);auto p=planEquip(a,EquipRequest::MainWeapon,5);check(p.result==EquipPlanResult::Target&&p.slot==5);
  p=planEquip(a,EquipRequest::MainWeapon,255);check(p.result==EquipPlanResult::Target&&p.slot==3);
  p=planEquip(a,EquipRequest::MainWeapon,1);check(p.result==EquipPlanResult::Target&&p.slot==3);} // remembered slot no longer melee
 // Consumables and stacks are never targets, even if metadata were mislabelled as a weapon.
 {auto a=observed(5);a.slots[0].category=2;a.slots[0].equipmentSlot=5;a.slots[1]={};
  check(planEquip(a,EquipRequest::Bow).result==EquipPlanResult::NoMatchingSlot); // count 2, entity 0
  a.slots[2].category=2;a.slots[2].equipmentSlot=5;check(planEquip(a,EquipRequest::Bow).result==EquipPlanResult::NoMatchingSlot);} // entity 0
 // Missing metadata, invalid snapshot or selection index refuse to plan.
 {auto a=observed(5);a.slots[1].metadata=false;check(planEquip(a,EquipRequest::Bow).result==EquipPlanResult::NoMatchingSlot);
  a=observed(5);a.valid=false;check(planEquip(a,EquipRequest::Bow).result==EquipPlanResult::Unknown);
  a=observed(9);check(planEquip(a,EquipRequest::Bow).result==EquipPlanResult::Unknown);
  check(planEquip(observed(5),EquipRequest::None).result==EquipPlanResult::Unknown);}
 // Wheel geometry matches the survey: slot 1 -65, 3 -13, 4 +13, 5 +39, 6 +65 degrees.
 {for(auto [slot,deg]:{std::pair{1,-65.f},std::pair{3,-13.f},std::pair{4,13.f},std::pair{5,39.f},std::pair{6,65.f}}){
   const auto d=wheelSlotDirection(uint8_t(slot));const float r=deg*3.14159265f/180;check(std::abs(d.x-std::sin(r))<1e-5f&&std::abs(d.y-std::cos(r))<1e-5f);}
  bool threw=false;try{wheelSlotDirection(8);}catch(const std::invalid_argument&){threw=true;}check(threw);}
 // WheelSelect: open -> aim -> release with aim held -> close -> confirmed selection.
 const XrDuration ms=1000000,dt=11111111;WheelSelectConfig c{300*ms,330*ms,200*ms,150*ms,1500*ms};
 {WheelSelect w(c);XrTime t=0;w.start(4,t);bool sawRelease=false,sawNeutral=false;unsigned frame=0;WheelSelect::Step s;
  auto overlayAt=[&](unsigned f){return f>=2&&f<34;};
  for(;frame<200&&w.active();frame++){t+=dt;
   const uint8_t selected=frame>=40?4:5;s=w.update(t,overlayAt(frame),selected);
   if(s.phase==WheelSelect::Phase::Aiming)check(s.buttons==0x0200&&std::abs(s.stick.x-std::sin(13*3.14159265f/180))<1e-5f);
   if(s.phase==WheelSelect::Phase::Releasing){sawRelease=true;check(s.buttons==0&&s.stick.y>.9f);}
   if(s.phase==WheelSelect::Phase::Settling){sawNeutral=true;check(s.buttons==0&&s.stick.x==0&&s.stick.y==0);}
  }
  check(w.phase()==WheelSelect::Phase::Done&&sawRelease&&sawNeutral&&frame>=50&&frame<=60);}
 // Failures: never opens, closes early, never closes, wrong final selection, reopens.
 {WheelSelect w(c);XrTime t=0;w.start(4,t);WheelSelect::Step s;for(int i=0;i<40&&w.active();i++){t+=dt;s=w.update(t,false,5);}check(w.phase()==WheelSelect::Phase::Failed&&std::string(s.error)=="wheel did not open"&&s.buttons==0);}
 {WheelSelect w(c);XrTime t=0;w.start(4,t);WheelSelect::Step s;for(int i=0;i<40&&w.active();i++){t+=dt;s=w.update(t,i>=2&&i<10,5);}check(w.phase()==WheelSelect::Phase::Failed&&std::string(s.error)=="wheel closed before selection");}
 {WheelSelect w(c);XrTime t=0;w.start(4,t);WheelSelect::Step s;for(int i=0;i<100&&w.active();i++){t+=dt;s=w.update(t,i>=2,5);}check(w.phase()==WheelSelect::Phase::Failed&&std::string(s.error)=="wheel did not close");}
 {WheelSelect w(c);XrTime t=0;w.start(4,t);WheelSelect::Step s;for(int i=0;i<300&&w.active();i++){t+=dt;s=w.update(t,i>=2&&i<34,5);}check(w.phase()==WheelSelect::Phase::Failed&&std::string(s.error)=="selection not confirmed");}
 {WheelSelect w(c);XrTime t=0;w.start(4,t);WheelSelect::Step s;for(int i=0;i<100&&w.active();i++){t+=dt;s=w.update(t,(i>=2&&i<34)||i==45,i>=40?4:5);}check(w.phase()==WheelSelect::Phase::Failed&&std::string(s.error)=="wheel reopened while settling");}
 // Flicker (new, old, new) during settling restarts the stability window.
 {WheelSelect w(c);XrTime t=0;w.start(4,t);unsigned frame=0;
  for(;frame<300&&w.active();frame++){t+=dt;const uint8_t sel=frame<40?5:(frame==42||frame==43)?5:4;w.update(t,frame>=2&&frame<34,sel);}
  check(w.phase()==WheelSelect::Phase::Done&&frame>=57);}
 {WheelSelect w(c);w.start(4,0);bool threw=false;try{w.start(5,1);}catch(const std::logic_error&){threw=true;}check(threw);}
 {bool threw=false;try{WheelSelect w(WheelSelectConfig{});}catch(const std::invalid_argument&){threw=true;}check(threw);}
 std::cout<<"PASS "<<n<<" equip planner checks\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<"\n";return 1;}}
