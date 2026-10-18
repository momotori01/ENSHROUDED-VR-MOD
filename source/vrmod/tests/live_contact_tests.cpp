#include <evr/live_contact.hpp>
#include <iostream>
#include <stdexcept>
#include <string>
using namespace evr::motion;
int main(){try{unsigned n=0;auto ck=[&](bool b,int line=__builtin_LINE()){++n;if(!b)throw std::runtime_error(std::to_string(n)+" at line "+std::to_string(line));};
 auto collider=[](uint32_t entity,uint8_t sub=0){ColliderKey k;k.world=0x1000;k.entity=entity;k.event[0]=7;k.event[4]=sub;return k;};
 auto pose=[](uint8_t v){std::array<uint8_t,56> t{};t.fill(v);return t;};

 // The collider: the first update starts the sweep at the weapon, later updates sweep from the previous pose.
 {LiveColliderLedger<4> l;const auto k=collider(10);
  ck(!l.owns(k,1000)&&l.size(1000)==0);
  auto d=l.apply(k,true,1000,pose(1));ck(d.track&&!d.continuous&&d.previous==pose(1)&&l.owns(k,1000)&&l.size(1000)==1);
  d=l.apply(k,true,1016,pose(2));ck(d.track&&d.continuous&&d.previous==pose(1));
  d=l.apply(k,true,1033,pose(3));ck(d.continuous&&d.previous==pose(2));
  // Not eligible (no fresh tracked pose): nothing is applied and nothing is recorded for that update.
  d=l.apply(k,false,1050,pose(9));ck(!d.track&&!d.continuous);
  d=l.apply(k,true,1066,pose(4));ck(d.continuous&&d.previous==pose(3));
  // After a pause longer than the continuity window the sweep starts again at the weapon.
  ck(l.owns(k,1300)&&!l.owns(k,1400));
  d=l.apply(k,true,1500,pose(5));ck(d.track&&!d.continuous&&d.previous==pose(5));
  ck(l.returned(k,77)&&!l.returned(collider(11),1));
  // Another event of the same entity is another collider.
  d=l.apply(collider(10,1),true,1510,pose(6));ck(d.track&&!d.continuous&&l.size(1510)==2);
  ck(!l.apply(ColliderKey{},true,1510,pose(1)).track&&!l.apply(k,true,0,pose(1)).track);}

 // Unlimited attacks: forgotten entries are reused, and when all are fresh the one seen longest ago goes.
 {LiveColliderLedger<4> l;
  for(uint32_t attack=0;attack<1000;attack++){const auto k=collider(100+attack);const uint64_t t=10000+attack*600;
   auto d=l.apply(k,true,t,pose(1));ck(d.track&&!d.continuous);d=l.apply(k,true,t+16,pose(2));ck(d.continuous&&d.previous==pose(1));ck(l.size(t+16)<=4);}
  LiveColliderLedger<4> busy;for(uint32_t e=0;e<4;e++)ck(busy.apply(collider(1+e),true,5000+e,pose(uint8_t(e))).track);
  ck(busy.size(5010)==4&&busy.apply(collider(9),true,5010,pose(9)).track&&busy.size(5010)==4);
  ck(!busy.owns(collider(1),5010)&&busy.owns(collider(2),5010)&&busy.owns(collider(9),5010));   // the oldest was replaced
  ck(!busy.apply(collider(1),true,5020,pose(7)).continuous);}

 auto action=[](int64_t trigger,uint32_t sequence=55){EventActionKey k;k.world=0x1000;k.entity=5;k.sequence=sequence;k.item=900;k.trigger=trigger;return k;};
 auto event=[](uintptr_t variant){EventIdentity e;e.variant=variant;e.guid[0]=uint8_t(variant);return e;};

 // The press: the next action of that entity and item with a new trigger time, within one second.
 {LiveAttackPress p{5,900,1111,20000,1};
  ck(p.starts(action(2222),20100)&&p.starts(action(2222),21000)&&!p.starts(action(2222),21001)&&!p.starts(action(2222),19999));
  ck(!p.starts(action(1111),20100));                                  // the action that was already running at the press
  auto other=action(2222);other.item=901;ck(!p.starts(other,20100));other=action(2222);other.entity=6;ck(!p.starts(other,20100));
  ck(!LiveAttackPress{}.starts(action(2222),20100)&&!p.starts(EventActionKey{},20100));}

 // Early Impact: appended once on the first tick, the authored occurrence withheld, then forgotten after the action.
 {LiveEventLedger<4> l;const auto k=action(2222);const auto e=event(0x5000);using D=LiveEventLedger<4>::Decision;
  ck(l.consider(k,e,true,false,2,8,30000)==D::Ignore);               // not started by a swing
  ck(l.consider(k,e,false,true,2,8,30000)==D::Ignore);               // a due event of an attack nobody retimed
  ck(l.consider(k,e,true,true,8,8,30000)==D::Ignore);                // the native array is full
  ck(l.consider(k,e,true,true,2,8,30000)==D::Append&&!l.owns(k,30000));
  ck(l.commit(k,e,30000,1200)&&l.owns(k,30000)&&l.size(30000)==1&&!l.commit(k,e,30000,1200));
  ck(l.consider(k,e,true,true,2,8,30016)==D::Ignore);                // never appended twice
  ck(l.consider(k,e,false,false,2,8,30400)==D::Suppress);            // its authored time
  ck(l.consider(k,event(0x6000),false,false,2,8,30400)==D::Ignore);  // other events of the action are the game's
  ck(l.consider(action(3333),e,false,false,2,8,30400)==D::Ignore);   // the same event in another action too
  ck(l.owns(k,34200)&&!l.owns(k,34201)&&l.consider(k,e,false,false,2,8,34201)==D::Ignore);
  ck(!l.commit(EventActionKey{},e,30000,100)&&!l.commit(action(4444),EventIdentity{},30000,100));}
 // A full ledger of running actions leaves a further attack on its authored timing; time frees the entries.
 {LiveEventLedger<4> l;using D=LiveEventLedger<4>::Decision;
  for(int i=0;i<4;i++)ck(l.commit(action(100+i),event(0x5000),40000+i,1000));
  ck(l.consider(action(200),event(0x5000),true,true,1,8,40010)==D::Ignore&&!l.commit(action(200),event(0x5000),40010,1000));
  ck(l.consider(action(200),event(0x5000),true,true,1,8,44004)==D::Append&&l.commit(action(200),event(0x5000),44004,1000)&&l.size(44004)==1);
  // A swing every half second for ever: the default ledger always has room (each entry lasts 3.4 s here).
  LiveEventLedger<> live;using L=LiveEventLedger<>::Decision;
  for(int i=0;i<1000;i++){const auto k=action(1000+i);const uint64_t t=50000+uint64_t(i)*500;
   ck(live.consider(k,event(0x7000),true,true,1,8,t)==L::Append&&live.commit(k,event(0x7000),t,400));ck(live.consider(k,event(0x7000),false,false,1,8,t+300)==L::Suppress);ck(live.size(t)<=8);}}
 // Several events of one action: both are withheld at their authored time, a third event of the action is not.
 {LiveEventLedger<4> l;using D=LiveEventLedger<4>::Decision;const auto k=action(5555);const EventIdentity both[2]{event(0x5000),event(0x5100)};
  ck(l.consider(k,both[0],true,true,2,8,60000)==D::Append&&l.commit(k,both,2,60000,1800)&&l.owns(k,60000)&&l.size(60000)==1);
  ck(l.consider(k,both[0],false,false,3,8,60500)==D::Suppress&&l.consider(k,both[1],false,false,3,8,60500)==D::Suppress);
  ck(l.consider(k,event(0x5200),false,false,3,8,60500)==D::Ignore&&l.consider(k,both[1],true,true,3,8,60016)==D::Ignore);
  const EventIdentity five[5]{event(1),event(2),event(3),event(4),event(5)};const EventIdentity hole[2]{event(1),EventIdentity{}};
  ck(!l.commit(action(5556),five,5,60000,100)&&!l.commit(action(5556),both,0,60000,100)&&!l.commit(action(5556),nullptr,1,60000,100)&&!l.commit(action(5556),hole,2,60000,100));
  ck(l.commit(action(5556),five,4,60000,100)&&l.consider(action(5556),five[3],false,false,1,8,60050)==D::Suppress);}
 // Which events go early: the ones authored first, together with those of the same time.
 {std::array<EarlyEvent,maximumEarlyEvents> out{};
  const EarlyEvent one[1]{{0x10,216666667,150000000}};ck(selectEarly(one,1,out)==1&&out[0].variant==0x10&&out[0].duration==150000000);
  const EarlyEvent pair[2]{{0x10,533333333,233333333},{0x20,533333333,233333333}};ck(selectEarly(pair,2,out)==2&&out[0].variant==0x10&&out[1].variant==0x20);
  const EarlyEvent flurry[3]{{0x30,700000000,100000000},{0x10,300000000,150000000},{0x20,300000000,150000000}};
  ck(selectEarly(flurry,3,out)==2&&out[0].variant==0x10&&out[1].variant==0x20);                      // the later hit keeps its time
  const EarlyEvent twice[2]{{0x10,100,1},{0x10,100,1}};ck(selectEarly(twice,2,out)==1);
  const EarlyEvent many[5]{{1,5,1},{2,5,1},{3,5,1},{4,5,1},{5,5,1}};ck(selectEarly(many,5,out)==0);          // more at once than the route holds
  const EarlyEvent none[2]{{0,5,1},{0x10,-1,1}};ck(selectEarly(none,2,out)==0&&selectEarly(nullptr,3,out)==0&&selectEarly(one,0,out)==0);
  const EarlyEvent closeBy[2]{{0x10,100000000,1},{0x20,100900000,1}};ck(selectEarly(closeBy,2,out)==2&&selectEarly(closeBy,2,out,500000)==1);}
 std::cout<<"live contact tests passed ("<<n<<")\n";return 0;
}catch(const std::exception& e){std::cerr<<"live contact test failed: "<<e.what()<<"\n";return 1;}}
