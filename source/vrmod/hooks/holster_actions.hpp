// The holsters (include/evr/holster.hpp; user, 2026-10-13): a grip pressed at a shoulder or at the hip puts something
// into the hand or away. The frontend's thread finds the press (controller_ui_fixture.hpp) and asks here; what the
// game holds comes from live_equipment.hpp and live_actionbar_pages.hpp (read about ten times a second), and what is
// asked for goes into the input that is sent (input_head_idle.hpp, holsterInput).
//   left shoulder: the ranged-combat bit, held for the player (the game's own key holds it: the ranged weapon of the
//     equipment is in the hand while the bit is set, and the shield is switched off meanwhile). With that weapon - or
//     a ranged weapon of the action bar - in the hand: bare hands.
//   right shoulder: the action bar's selection to "no slot" (the game shows bare hands), and back to the slot it was.
//   left hip: the shield of the equipment off - the game moves it into the inventory, as its own "unequip" does - and
//     on again from where it lies then.
namespace holsters {
inline std::mutex mutex;inline evr::motion::HolsterMemory memory;inline uint64_t lastOrderMs=0;inline uint8_t armedAtSlot=0xff;
inline liveEquipment::Stack shieldOff{};inline unsigned lines=0;
// The slot last selected with something other than a ranged weapon: where a bow of the action bar puts the hand back.
inline uint8_t weaponSlot=0xff;
// A selection asked of the game, asked again (a new version) until the game shows it: the game takes no change of
// the selection while the player blocks, and counts the request as taken.
inline int pendingSelect=-1;inline uint64_t pendingSince=0;inline unsigned pendingTries=0;
inline std::map<uint32_t,controllerUiFixture::HandItem> described;
// The mod's bow (include/evr/holster.hpp HolsterMemory::modBow; 2026-10-24): taken from the left shoulder with no slot of the
// action bar holding it, the equipment's bow is shown in the left hand by the mod (motion_mod_bow_visual.hpp) while the
// game's hand is bare - the ranged-combat bit is the game's aiming, held it slows the walk and floats in the air. Its item
// for the other threads; 0: none. EVR_MOD_BOW=0: the bit, as before.
inline auto& modBowItem=controllerUiFixture::modBowItem;
inline bool modBowEnabled=false;   // set where the holsters are put on: wanted, and the mod's bow can be shown
inline bool modBowWanted(){static const bool on=[]{char v[8]{};const auto n=GetEnvironmentVariableA("EVR_MOD_BOW",v,sizeof v);return !(n==1&&v[0]=='0');}();return on;}
inline void publishModBow(const liveEquipment::State& equipment){   // a moment of no fresh equipment keeps the bow
 if(!memory.modBow)modBowItem=0;else if(equipment.valid&&equipment.ranged.stack.item)modBowItem=equipment.ranged.stack.item;}
// What the mod takes the hand to hold: with the ranged-combat bit held, the equipment's ranged weapon.
inline controllerUiFixture::HandItem describe(uint32_t item){
 if(const auto found=described.find(item);found!=described.end())return found->second;
 controllerUiFixture::HandItem out;out.item=item;const auto registry=attachmentObserver::itemRegistry.load();if(!item||!registry)return out;
 try{auto reader=inputRanged::readItemBytes;const auto found=game::lookupItem(reader,registry,item);game::ItemMetadata m;
  if(!game::readItemMetadata(reader,found,item,m))return out;
  out.category=m.category;out.itemSlot=m.slot;
  if(m.category==2){const auto wc=game::itemWeaponCategory(reader,registry,found,item);out.typeKnown=wc.status==game::ItemLookupStatus::Found;out.weaponType=wc.type;}
  out.valid=true;out.count=1;if(described.size()<64)described[item]=out;
 }catch(const std::exception&){}
 return out;
}
inline controllerUiFixture::HandItem hand(uint64_t now){
 auto held=liveActionbars::hand(now);
 const bool mod=modBowItem.load()!=0;
 if(!holsterInput::armed.load()&&!mod)return held;
 if(mod&&held.valid&&held.item)return held;          // a slot was selected meanwhile: that is in the hand
 const auto equipment=liveEquipment::state(now);if(!equipment.valid||!equipment.ranged.stack.item)return held;
 std::lock_guard<std::mutex> lock(mutex);const auto ranged=describe(equipment.ranged.stack.item);return ranged.valid?ranged:held;
}
inline const char* actName(evr::motion::HolsterAct a){using evr::motion::HolsterAct;return a==HolsterAct::Ranged?"ranged":a==HolsterAct::Weapon?"weapon":a==HolsterAct::Shield?"shield":"none";}
// One press of a grip in a holster. True: something was asked of the game (the hand vibrates).
inline bool act(unsigned which,uint64_t now){
 using namespace evr::motion;
 const auto equipment=liveEquipment::state(now);const auto held=liveActionbars::hand(now);
 HolsterGame g;g.known=equipment.valid;g.selected=equipment.selected;
 g.selectedIsRanged=equipment.selected<16&&held.valid&&held.item&&(held.itemSlot==5||held.item==equipment.ranged.stack.item);
 g.rangedWorn=equipment.ranged.stack.item!=0;g.shieldWorn=equipment.shield.stack.item!=0;
 g.rangedBarSlot=g.rangedWorn?liveActionbars::slotOf(equipment.ranged.stack.item,now):evr::motion::holsterNoSlot;
 std::lock_guard<std::mutex> lock(mutex);g.weaponSlot=weaponSlot;g.modBow=modBowEnabled;
 // The shield to wear again: the one that was taken off, where it lies now; without it, the first shield there is.
 liveEquipment::Worn kept{};
 if(!g.shieldWorn){
  if(shieldOff.item)for(const auto& [w,category]:equipment.rows)if(w.stack.item==shieldOff.item&&w.stack.data==shieldOff.data){kept=w;break;}
  if(!kept.entity&&equipment.keptShield.entity)kept=equipment.keptShield;
  g.shieldKept=kept.entity!=0;
 }
 const auto before=memory;
 // The last order has to show in what is read from the game before the next press is judged.
 HolsterOrder order;if(now-lastOrderMs>=700)order=holsterOrder(HolsterAct(which),g,memory);
 if(order.change){
  game::HolsterRequest r;r.arm=order.arm;r.select=order.select;
  if(order.arm&&!before.armed)armedAtSlot=g.selected;
  if(order.shield==1){r.equip=1;r.equipmentSlot=game::equipmentSlotShield;shieldOff=equipment.shield.stack;}
  else if(order.shield==0){r.equip=0;r.equipmentSlot=game::equipmentSlotShield;r.entity=kept.entity;r.index=kept.index;}
  holsterInput::ask(r);lastOrderMs=now;
  if(order.select>=0){pendingSelect=order.select;pendingSince=now;pendingTries=1;}else pendingSelect=-1;
 }
 publishModBow(equipment);
 if(lines<400){++lines;
  std::ostringstream o;o<<"\"frame\":"<<liveFrame.frame<<",\"act\":\""<<actName(HolsterAct(which))<<"\",\"asked\":"<<(order.change?"true":"false")<<",\"state_known\":"<<(g.known?"true":"false")
   <<",\"selected_slot\":"<<unsigned(g.selected)<<",\"selected_item\":"<<held.item<<",\"selected_is_ranged\":"<<(g.selectedIsRanged?"true":"false")
   <<",\"ranged_worn\":"<<equipment.ranged.stack.item<<",\"shield_worn\":"<<equipment.shield.stack.item<<",\"shield_kept\":["<<kept.entity<<','<<unsigned(kept.index)<<','<<kept.stack.item<<"]"
   <<",\"armed_before\":"<<(before.armed?"true":"false")<<",\"armed\":"<<(memory.armed?"true":"false")<<",\"remembered_slot\":"<<unsigned(memory.slot)
   <<",\"mod_bow_before\":"<<(before.modBow?"true":"false")<<",\"mod_bow\":"<<(memory.modBow?"true":"false")<<",\"bow_from\":"<<unsigned(memory.bowFrom)
   <<",\"weapon_slot\":"<<unsigned(g.weaponSlot)<<",\"ranged_bar_slot\":"<<unsigned(g.rangedBarSlot)<<",\"select\":"<<order.select<<",\"shield\":"<<order.shield<<",\"since_last_order_ms\":"<<(lastOrderMs&&!order.change?int64_t(now-lastOrderMs):int64_t(0));
  logFile->event("holster_act",o.str());}
 return order.change;
}
// The player chose another slot of the action bar while the ranged weapon was held for them: that weapon goes.
inline void tick(uint64_t now){
 const auto equipment=liveEquipment::state(now);const auto held=equipment.valid&&equipment.selected<16?liveActionbars::hand(now):controllerUiFixture::HandItem{};
 std::lock_guard<std::mutex> lock(mutex);
 if(held.valid&&held.item&&held.itemSlot!=5&&held.item!=equipment.ranged.stack.item&&!holsterInput::armed.load())weaponSlot=equipment.selected;
 if(pendingSelect>=0&&equipment.valid){
  const bool shown=pendingSelect<16?equipment.selected==pendingSelect:equipment.selected>=16;
  if(shown)pendingSelect=-1;
  else if(now-pendingSince>=400ull*pendingTries){
   const bool again=pendingTries<4;
   if(again){game::HolsterRequest r;r.arm=memory.armed;r.select=pendingSelect;holsterInput::ask(r);lastOrderMs=now;++pendingTries;}
   if(lines<400){++lines;logFile->event("holster_act","\"frame\":"+std::to_string(liveFrame.frame)+",\"act\":\"selection_asked_again\",\"asked\":"+(again?"true":"false")+
       ",\"select\":"+std::to_string(pendingSelect)+",\"selected_slot\":"+std::to_string(unsigned(equipment.selected))+",\"tries\":"+std::to_string(pendingTries));}
   if(!again)pendingSelect=-1;
  }
 }
 // The mod's bow goes when the player selects a slot (the quick wheel), or the bow is no longer worn.
 if(memory.modBow&&equipment.valid&&now-lastOrderMs>=700&&pendingSelect<0&&(equipment.selected<16||!equipment.ranged.stack.item)){
  memory.modBow=false;
  if(lines<400){++lines;logFile->event("holster_act","\"frame\":"+std::to_string(liveFrame.frame)+",\"act\":\"mod_bow_dropped\",\"asked\":false,\"selected_slot\":"+std::to_string(unsigned(equipment.selected))+
      ",\"ranged_worn\":"+std::to_string(equipment.ranged.stack.item));}
 }
 publishModBow(equipment);
 if(!memory.armed||now-lastOrderMs<700)return;
 if(!equipment.valid||equipment.selected==armedAtSlot)return;
 memory.armed=false;game::HolsterRequest r;holsterInput::ask(r);lastOrderMs=now;
 if(lines<400){++lines;logFile->event("holster_act","\"frame\":"+std::to_string(liveFrame.frame)+",\"act\":\"selection_changed\",\"asked\":true,\"armed_before\":true,\"armed\":false,\"selected_slot\":"+std::to_string(unsigned(equipment.selected))+",\"armed_at_slot\":"+std::to_string(unsigned(armedAtSlot)));}
}
}
