#include <evr/holster.hpp>
#include <iostream>
#include <stdexcept>
#include <string>
using namespace evr;using namespace evr::motion;using namespace evr::game;
int main(){try{unsigned n=0;auto ck=[&](bool b){++n;if(!b)throw std::runtime_error(std::to_string(n));};
 const Vec3 head{0,1.65f,0},ahead{0,0,-1};   // looking along -Z: +X is the right
 // The shoulders (VHVR Pose.cs:675, 689-692, 710-720): beside or behind the head, the hand's own side only.
 ck(holsterPlace(head,ahead,{-.2f,1.6f,.1f},0)==HolsterPlace::LeftShoulder);
 ck(holsterPlace(head,ahead,{.2f,1.6f,.05f},1)==HolsterPlace::RightShoulder);
 ck(holsterPlace(head,ahead,{-.2f,1.6f,.1f},1)==HolsterPlace::None&&holsterPlace(head,ahead,{.2f,1.6f,.05f},0)==HolsterPlace::None);   // the other hand's side
 ck(holsterPlace(head,ahead,{.2f,1.6f,-.1f},1)==HolsterPlace::None);                // ahead of the head
 ck(holsterPlace(head,ahead,{.2f,1.65f,0},1)==HolsterPlace::RightShoulder);         // level with it
 ck(holsterPlace(head,ahead,{.2f,1.8f,.05f},1)==HolsterPlace::None&&holsterPlace(head,ahead,{.2f,1.77f,.05f},1)==HolsterPlace::RightShoulder);   // 0.125 above at the most
 ck(holsterPlace(head,ahead,{.2f,1.41f,.05f},1)==HolsterPlace::RightShoulder&&holsterPlace(head,ahead,{.2f,1.38f,.05f},1)==HolsterPlace::None);     // 0.25 below
 ck(holsterPlace(head,ahead,{.55f,1.6f,.05f},1)==HolsterPlace::None);               // too far out
 // The hips (Pose.cs:676, 689-692, 721-724): beside and behind.
 ck(holsterPlace(head,ahead,{-.25f,1.15f,.25f},0)==HolsterPlace::LeftHip&&holsterPlace(head,ahead,{.25f,1.15f,.25f},1)==HolsterPlace::RightHip);
 ck(holsterPlace(head,ahead,{-.25f,1.15f,0},0)==HolsterPlace::None);                // level with the head's plane: not behind
 ck(holsterPlace(head,ahead,{-.25f,1.15f,.15f},0)==HolsterPlace::None&&holsterPlace(head,ahead,{-.25f,1.15f,.19f},0)==HolsterPlace::LeftHip);   // s + 0.75 |l| <= 0
 ck(holsterPlace(head,ahead,{-.05f,1.15f,.1f},0)==HolsterPlace::LeftHip&&holsterPlace(head,ahead,{.1f,1.15f,.1f},0)==HolsterPlace::None);       // left of l = -0.5 s
 ck(holsterPlace(head,ahead,{-.25f,1.3f,.25f},0)==HolsterPlace::None&&holsterPlace(head,ahead,{-.25f,.89f,.25f},0)==HolsterPlace::None);       // between the two heights, and below
 // The head turned: ahead along +X, the right is +Z. Looking down: the forward laid level; straight down: none.
 ck(holsterPlace(head,{1,0,0},{-.1f,1.6f,-.2f},0)==HolsterPlace::LeftShoulder&&holsterPlace(head,{1,0,0},{-.1f,1.6f,.2f},1)==HolsterPlace::RightShoulder);
 ck(holsterPlace(head,{0,-.87f,-.5f},{.2f,1.6f,.05f},1)==HolsterPlace::RightShoulder&&holsterPlace(head,{0,-1,0},{.2f,1.6f,.05f},1)==HolsterPlace::None);
 ck(holsterAct(HolsterPlace::LeftShoulder)==HolsterAct::Ranged&&holsterAct(HolsterPlace::RightShoulder)==HolsterAct::Weapon&&holsterAct(HolsterPlace::LeftHip)==HolsterAct::Shield&&holsterAct(HolsterPlace::RightHip)==HolsterAct::None);
 // The grips: once, where the press begins.
 {HolsterGrips g;HandFrame f;f.focused=true;f.frame=1;const Pose at{{0,0,0,1},head};
  for(auto& h:f.hands){h.gripActive=h.grabActive=true;h.gripFlags=15;h.grip={{0,0,0,1},{0,1.2f,-.3f}};}
  ck(g.update(f,at,true).act==HolsterAct::None&&!g.owns(0)&&!g.owns(1));
  f.hands[1].grip.position={.2f,1.6f,.05f};ck(g.update(f,at,true).act==HolsterAct::None);              // there, not pressed
  f.hands[1].grab=true;{const auto p=g.update(f,at,true);ck(p.act==HolsterAct::Weapon&&p.hand==1&&p.place==HolsterPlace::RightShoulder&&g.owns(1)&&!g.owns(0));}
  ck(g.update(f,at,true).act==HolsterAct::None&&g.owns(1));                                            // held: not again
  f.hands[1].grip.position={.3f,1.2f,-.3f};ck(g.update(f,at,true).act==HolsterAct::None&&g.owns(1));   // carried away, still the holster's
  f.hands[1].grab=false;ck(g.update(f,at,true).act==HolsterAct::None&&!g.owns(1));
  f.hands[1].grab=true;ck(g.update(f,at,true).act==HolsterAct::None&&!g.owns(1));                      // pressed elsewhere
  f.hands[1].grip.position={.2f,1.6f,.05f};ck(g.update(f,at,true).act==HolsterAct::None&&!g.owns(1));  // ... and brought in: nothing
  f.hands[1].grab=false;g.update(f,at,true);
  f.hands[1].grab=true;ck(g.update(f,at,false).act==HolsterAct::None);                                 // a menu: nothing begins
  ck(g.update(f,at,true).act==HolsterAct::None&&!g.owns(1));                                           // ... and not afterwards either
  f.hands[1].grab=false;g.update(f,at,true);
  f.hands[0].grip.position={-.2f,1.6f,.1f};f.hands[0].grab=true;ck(g.update(f,at,true).act==HolsterAct::Ranged&&g.owns(0));
  f.hands[0].grab=false;g.update(f,at,true);f.hands[0].grip.position={-.25f,1.15f,.25f};f.hands[0].grab=true;ck(g.update(f,at,true).act==HolsterAct::Shield);
  f.hands[0].grab=false;g.update(f,at,true);f.hands[0].gripFlags=3;f.hands[0].grab=true;ck(g.update(f,at,true).act==HolsterAct::None);   // not tracked
  f.hands[0].gripFlags=15;ck(g.update(f,at,true).act==HolsterAct::Shield);                             // tracked again with the grip down: a press
  f.hands[0].grab=false;g.update(f,at,true);f.focused=false;f.hands[0].grab=true;ck(g.update(f,at,true).act==HolsterAct::None);}
 // What a press asks for.
 {HolsterMemory m;HolsterGame g;g.known=true;g.selected=2;g.rangedWorn=true;g.shieldWorn=true;
  auto o=holsterOrder(HolsterAct::Weapon,g,m);ck(o.change&&o.select==16&&!o.arm&&o.shield==-1&&m.slot==2);          // the sword off
  g.selected=16;o=holsterOrder(HolsterAct::Weapon,g,m);ck(o.change&&o.select==2&&!o.arm);                          // ... and on again
  g.selected=2;o=holsterOrder(HolsterAct::Ranged,g,m);ck(o.change&&o.arm&&o.select==-1&&m.armed);                  // the bow of the equipment
  o=holsterOrder(HolsterAct::Ranged,g,m);ck(o.change&&!o.arm&&o.select==-1&&!m.armed&&m.slot==2);                  // the bow away: the sword of slot 2 again
  g.selected=16;o=holsterOrder(HolsterAct::Ranged,g,m);ck(o.change&&o.arm&&o.select==-1);                          // from bare hands
  o=holsterOrder(HolsterAct::Weapon,g,m);ck(o.change&&!o.arm&&o.select==-1&&!m.armed);                             // the right shoulder puts it away too
  o=holsterOrder(HolsterAct::Weapon,g,m);ck(o.change&&o.select==2);
  g.selected=1;g.selectedIsRanged=true;o=holsterOrder(HolsterAct::Ranged,g,m);ck(o.change&&o.select==16&&!o.arm&&m.slot==1);   // a bow of the action bar, nothing before it
  g.weaponSlot=3;o=holsterOrder(HolsterAct::Ranged,g,m);ck(o.change&&o.select==3&&!o.arm&&m.slot==3);             // ... and with slot 3 before it
  g.weaponSlot=1;o=holsterOrder(HolsterAct::Ranged,g,m);ck(o.change&&o.select==16&&m.slot==1);g.weaponSlot=0xff;  // the bow's own slot is no way back
  g.selected=16;g.selectedIsRanged=false;g.rangedWorn=false;o=holsterOrder(HolsterAct::Ranged,g,m);ck(!o.change&&!o.arm);     // no ranged weapon worn
  // The worn bow is also on the action bar (slot 8): its slot is selected, the ranged-combat key stays up (2026-10-18).
  {HolsterGame b;b.known=true;b.selected=2;b.rangedWorn=true;b.rangedBarSlot=8;b.weaponSlot=2;HolsterMemory bm;
   auto bo=holsterOrder(HolsterAct::Ranged,b,bm);ck(bo.change&&bo.select==8&&!bo.arm&&!bm.armed);
   b.selected=8;b.selectedIsRanged=true;bo=holsterOrder(HolsterAct::Ranged,b,bm);ck(bo.change&&bo.select==2&&!bo.arm);   // away: the sword of slot 2 again
   b.selected=16;b.selectedIsRanged=false;bo=holsterOrder(HolsterAct::Ranged,b,bm);ck(bo.change&&bo.select==8&&!bo.arm);   // from bare hands too
   b.rangedBarSlot=0xff;bo=holsterOrder(HolsterAct::Ranged,b,bm);ck(bo.change&&bo.arm&&bo.select==-1);}                  // not on the bar: the key
  o=holsterOrder(HolsterAct::Shield,g,m);ck(o.change&&o.shield==1&&o.select==-1);
  g.shieldWorn=false;o=holsterOrder(HolsterAct::Shield,g,m);ck(!o.change);
  g.shieldKept=true;o=holsterOrder(HolsterAct::Shield,g,m);ck(o.change&&o.shield==0);
  HolsterMemory fresh;o=holsterOrder(HolsterAct::Weapon,g,fresh);ck(o.change&&o.select==0);                        // nothing remembered
  g.known=false;o=holsterOrder(HolsterAct::Weapon,g,fresh);ck(!o.change);
  // The armed state is kept in what every order says.
  HolsterMemory armed;armed.armed=true;g.known=true;g.shieldWorn=true;o=holsterOrder(HolsterAct::Shield,g,armed);ck(o.change&&o.arm&&o.shield==1);}
 // What goes into the input that is sent.
 {HolsterSender s;ClientInputBytes source{},out{};const uint32_t gameVersion=0x18af;std::memcpy(source.data()+inputBarAction,&gameVersion,4);source[inputBarAction+4]=6;
  HolsterRequest r;out=source;auto a=s.apply(r,source,out);ck(out==source&&!a.barOn&&!a.arm);                       // nothing asked
  r.serial=1;r.select=16;out=source;a=s.apply(r,source,out);
  uint32_t version=0;std::memcpy(&version,out.data()+inputBarAction,4);
  ck(a.barBegan&&a.barOn&&version!=gameVersion&&version>0x45560000&&out[inputBarAction+4]==32&&out[inputBarAction+5]==0&&onlyHolsterAndCameraChanged(source,out)&&!onlyHeadCameraChanged(source,out));
  ClientInputBytes again=source;a=s.apply(r,source,again);ck(again==out&&!a.barBegan&&a.barOn);                    // the same action, tick after tick
  r.serial=2;r.select=2;out=source;a=s.apply(r,source,out);uint32_t second=0;std::memcpy(&second,out.data()+inputBarAction,4);ck(second==version+1&&out[inputBarAction+4]==2);
  ClientInputBytes wheel=source;const uint32_t next=0x1b4a;std::memcpy(wheel.data()+inputBarAction,&next,4);wheel[inputBarAction+4]=5;   // the game's own selection
  out=wheel;a=s.apply(r,wheel,out);ck(out==wheel&&a.barEnded&&!a.barOn);
  out=wheel;a=s.apply(r,wheel,out);ck(out==wheel&&!a.barEnded);
  r.serial=3;r.select=-1;r.arm=true;out=wheel;a=s.apply(r,wheel,out);uint64_t digital=0;std::memcpy(&digital,out.data()+inputDigital,8);
  ck(a.arm&&digital==inputRangedCombatArm&&!a.barOn&&onlyHolsterAndCameraChanged(wheel,out));
  ClientInputBytes busy=wheel;const uint64_t held=(1ull<<30)|(1ull<<0);std::memcpy(busy.data()+inputDigital,&held,8);out=busy;s.apply(r,busy,out);
  std::memcpy(&digital,out.data()+inputDigital,8);ck(digital==(held|inputRangedCombatArm)&&onlyHolsterAndCameraChanged(busy,out));
  r.serial=4;r.arm=false;r.equip=1;r.equipmentSlot=equipmentSlotShield;out=wheel;a=s.apply(r,wheel,out);
  std::memcpy(&digital,out.data()+inputDigital,8);uint32_t ev=0;std::memcpy(&ev,out.data()+inputEquipAction+8,4);
  ck(!a.arm&&digital==0&&a.equipBegan&&ev==second+1&&out[inputEquipAction+12]==15&&out[inputEquipAction+13]==1&&out[inputEquipAction+4]==0xff&&onlyHolsterAndCameraChanged(wheel,out));
  r.serial=5;r.equip=0;r.entity=0x12345678;r.index=3;out=wheel;a=s.apply(r,wheel,out);uint32_t entity=0;std::memcpy(&entity,out.data()+inputEquipAction,4);
  ck(a.equipBegan&&entity==0x12345678&&out[inputEquipAction+4]==3&&out[inputEquipAction+13]==0&&out[inputEquipAction+12]==15);
  ClientInputBytes menu=wheel;menu[inputEquipAction+8]=7;out=menu;a=s.apply(r,menu,out);ck(out==menu&&a.equipEnded);   // the inventory screen's own change
  out=wheel;out[0x100]^=1;ck(!onlyHolsterAndCameraChanged(wheel,out));out=wheel;out[inputDigital]^=1;ck(!onlyHolsterAndCameraChanged(wheel,out));
  // AreaLoot (bit 8) for a short while from the left trigger's pull (2026-10-20): set until areaLootUntilMs, allowed by the check
  {auto o=wheel;areaLootUntilMs=1000;ck(applyAreaLoot(o,999)&&!applyAreaLoot(o,1000));uint64_t d=0;std::memcpy(&d,o.data()+inputDigital,8);
   ck((d&inputAreaLoot)&&inputAreaLootBit==8&&onlyHolsterAndCameraChanged(wheel,o));o[inputDigital+1]^=2;ck(!onlyHolsterAndCameraChanged(wheel,o));areaLootUntilMs=0;}
  // Diving by the strokes (2026-10-20): SwimStartDiving 34, DiveAscend 35, DiveDescend 36 while swimInputUntilMs lasts
  {auto o=wheel;swimInputBits=inputDiveDescend;swimInputUntilMs=500;ck(applySwimInput(o,499)&&!applySwimInput(o,500));uint64_t d=0;std::memcpy(&d,o.data()+inputDigital,8);
   ck((d&inputDiveDescend)&&!(d&inputDiveAscend)&&onlyHolsterAndCameraChanged(wheel,o));
   auto p=wheel;swimInputBits=inputSwimBits|inputAreaLoot;ck(applySwimInput(p,10));std::memcpy(&d,p.data()+inputDigital,8);ck((d&inputSwimBits)==inputSwimBits&&onlyHolsterAndCameraChanged(wheel,p));
   p[inputDigital+4]^=0x20;ck(!onlyHolsterAndCameraChanged(wheel,p));swimInputBits=0;swimInputUntilMs=0;ck(!applySwimInput(o,10));}
  out=wheel;out[inputDigital+5]^=0x40;ck(!onlyHolsterAndCameraChanged(wheel,out));
  r.serial=6;r.equip=-1;r.arm=true;out=wheel;s.apply(r,wheel,out);s.reset();out=wheel;a=s.apply(r,wheel,out);ck(out==wheel&&!a.arm);}
 std::cout<<"holster "<<n<<" PASS\n";return 0;}
 catch(const std::exception& e){std::cerr<<"holster FAIL "<<e.what()<<"\n";return 1;}}
