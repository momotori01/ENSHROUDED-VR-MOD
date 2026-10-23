#include <evr/consumable_hold.hpp>
#include <iostream>
#include <stdexcept>
#include <string>
// The consumable held from the quick wheel (include/evr/consumable_hold.hpp): the wheel's consume of a consumable is kept
// back - the action sent stays the one before - and use() sends a consume of the same slot with the mod's own version.
int main(){try{unsigned n=0;auto ck=[&](bool b){++n;if(!b)throw std::runtime_error(std::to_string(n));};
 using namespace evr::game;
 const auto action=[](uint32_t entity,uint8_t index,uint32_t version,uint8_t type){ItemActionBytes a{};std::memcpy(a.data(),&entity,4);a[4]=index;std::memcpy(a.data()+itemActionVersion,&version,4);a[itemActionType]=type;return a;};
 const auto input=[](const ItemActionBytes& a){ClientInputBytes in{};std::memcpy(in.data()+inputItemAction,a.data(),itemActionBytes);return in;};
 const auto sent=[](const ClientInputBytes& out){ItemActionBytes a{};std::memcpy(a.data(),out.data()+inputItemAction,itemActionBytes);return a;};
 const auto potionSlot=[](const InventorySlotBytes& s){uint32_t entity=0;std::memcpy(&entity,s.data(),4);return entity==7976&&s[4]==1?3102924359u:0u;};
 ConsumableHold h;h.seed(100);ClientInputBytes out{};
 const auto before=action(7976,3,5,0);
 auto r=h.apply(input(before),out,potionSlot);ck(!r.deferred&&sent(out)==before&&!h.held());          // the first input goes as it is
 r=h.apply(input(before),out,potionSlot);ck(sent(out)==before);
 // the wheel's consume of the potion (slot 1 of the first bar): kept back, the action before still sent
 const auto wheel=action(7976,1,6,itemActionConsume);
 r=h.apply(input(wheel),out,potionSlot);ck(r.deferred&&r.item==3102924359u&&h.held()&&sent(out)==before);
 for(int i=0;i<5;++i){r=h.apply(input(wheel),out,potionSlot);ck(!r.deferred&&!r.used&&sent(out)==before);}   // still held
 // at the mouth: the consume of that slot with the mod's version, sent tick after tick
 h.use();r=h.apply(input(wheel),out,potionSlot);
 {const auto s=sent(out);uint32_t entity=0,version=0;std::memcpy(&entity,s.data(),4);std::memcpy(&version,s.data()+itemActionVersion,4);
  ck(r.used&&r.item==3102924359u&&!h.held()&&entity==7976&&s[4]==1&&version==101&&s[itemActionType]==itemActionConsume);
  for(int i=0;i<3;++i){h.apply(input(wheel),out,potionSlot);ck(sent(out)==s);}}
 // the game's next action of its own goes out as it is
 const auto weapon=action(7976,3,7,0);r=h.apply(input(weapon),out,potionSlot);ck(r.passedGameAction&&sent(out)==weapon);
 // not a consumable (a consume of another slot): the game's action goes as it is
 const auto other=action(7977,2,8,itemActionConsume);r=h.apply(input(other),out,potionSlot);ck(r.passedGameAction&&!h.held()&&sent(out)==other);
 // dropped: nothing is sent for it
 const auto wheel2=action(7976,1,9,itemActionConsume);r=h.apply(input(wheel2),out,potionSlot);ck(r.deferred&&sent(out)==other);
 h.drop();h.use();r=h.apply(input(wheel2),out,potionSlot);ck(!r.used&&!h.held()&&sent(out)==other);
 std::cout<<"consumable hold tests passed: "<<n<<"\n";return 0;}
 catch(const std::exception& e){std::cerr<<"consumable hold test failed: "<<e.what()<<"\n";return 1;}}
