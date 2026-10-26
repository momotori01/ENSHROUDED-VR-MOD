// Read-only: what the local player wears in the equipment slots that the holsters need (2026-10-13), and where the
// player's inventories are.
// keen::ecs::SlotSelection: +0 `equipment`, 27 keen::InventorySlotId {entity, slot index} in the order of
// keen::EquipmentSlot (14 RangedWeapon - the weapon the ranged-combat key arms -, 15 Shield); +244 the equipment an
// impact has switched off (keen::ImpactEquippedItems: bit 0 MainHand, bit 1 Shield). An inventory is rows of eight
// keen::ItemStack {item, 8 bytes of data} (keen::ecs::Inventory, 96 bytes), a row an entity; a row names its owner in
// keen::ecs::InventoryLink {entity, category} and the owner lists its rows in keen::ecs::InventorySetup
// (+0 sixteen entities, +64 their categories, +81 generic slots, +82 available slots). No game call, no write.
namespace liveEquipment {
inline uintptr_t knownWorld=0;inline int selectionType=-1,inventoryType=-1,linkType=-1,setupType=-1,fishingType=-1;inline uint64_t lastRead=0;inline unsigned events=0;
struct Stack {uint32_t item=0;std::array<uint8_t,8> data{};};
struct Worn {uint32_t entity=0;uint8_t index=0xff;Stack stack{};};
// arrow / spell: the ammunition slots (keen::EquipmentSlot 14 Arrow, 15 Spell - the arrows and the staff's spell in use; their
// stack's first four bytes are the count, Mock 20261026-101514: Fire Ball I 20 at the backpack's row 7250 slot 7).
struct State {bool valid=false;uint64_t at=0;Worn ranged{},shield{},keptShield{},arrow{},spell{};uint32_t disabled=0;uint8_t selected=0xff;
    // Where an item sits in the rows of the player's inventories (the first slot that holds it), for wearing it again.
    std::vector<std::pair<Worn,uint8_t>> rows;};   // slot and the row's category
inline std::mutex mutex;inline State latest;inline std::string lastLine;
// keen::EquipmentSlotType of an item (18: Shield), kept once read.
inline std::map<uint32_t,uint8_t> slotTypes;
inline uint8_t slotType(uint32_t item){
 if(const auto found=slotTypes.find(item);found!=slotTypes.end())return found->second;
 const auto registry=attachmentObserver::itemRegistry.load();if(!item||!registry)return 0;
 try{auto reader=inputRanged::readItemBytes;const auto found=game::lookupItem(reader,registry,item);game::ItemMetadata m;
  if(!game::readItemMetadata(reader,found,item,m))return 0;
  if(slotTypes.size()<4096)slotTypes[item]=m.slot;return m.slot;
 }catch(const std::exception&){return 0;}
}
inline State state(uint64_t now){std::lock_guard<std::mutex> lock(mutex);auto out=latest;out.valid=out.valid&&pauseClock::fresh(out.at,now,1000);return out;}
inline bool stackAt(uintptr_t world,uint32_t entity,uint8_t index,Stack& out){
 if(!entity||index>=8||inventoryType<0)return false;
 try{const auto row=nativeImpactComponents::entityAtWorld(world,entity);const auto inventory=nativeImpactComponents::component(row,unsigned(inventoryType),96);if(!inventory)return false;
  const auto raw=read<std::array<uint8_t,12>>(inventory+12*index);std::memcpy(&out.item,raw.data(),4);std::memcpy(out.data.data(),raw.data()+4,8);return true;
 }catch(const std::exception&){return false;}
}
// 1.0 (2026-10-19): 96 bytes, the fields unmoved (tools/reflection_struct_diff.py) - the read asked for 88 until then and found
// nothing, so the float stayed on the copied rod after a cast (headset run 20261019-160912, user: "the float stays on after
// the cast"; one fishing_rod_shown record, no fishing_state at all).
// keen::ecs::NetworkFishing: +0 keen::fishing::State, +48 the float's entity while one is out. Read about thirty times
// a second (the rod shown changes with it, motion_visual_bridge.hpp).
inline void fishing(uintptr_t world,uint32_t local,uint64_t now){
 static uint64_t last=0;if(fishingType<0||world!=knownWorld||now-last<30)return;last=now;
 controllerUiFixture::fishingComponentType=fishingType;controllerUiFixture::fishingComponentWorld=world;
 try{
  const auto player=nativeImpactComponents::entityAtWorld(world,local);const auto fishing=nativeImpactComponents::component(player,unsigned(fishingType),96);if(!fishing)return;
  const auto state=read<uint8_t>(fishing);const auto floatId=read<uint32_t>(fishing+48);if(floatId)controllerUiFixture::fishingFloatMs=now;
  controllerUiFixture::fishingState=int(state);controllerUiFixture::fishingStateMs=now;
  {const auto fish=read<uint32_t>(fishing+0x34);static uint32_t toldFish=0;controllerUiFixture::fishingFishId=fish;
   if(fish&&state>=22&&state<=30)attachmentObserver::markFishFighting(fish,now);   // its swim tells its head (motion_body_ik.hpp holdFish)
   if(fish!=toldFish){toldFish=fish;logFile->event("fishing_fish_entity","\"frame\":"+std::to_string(liveFrame.frame)+",\"state\":"+std::to_string(unsigned(state))+",\"fish\":"+std::to_string(fish));}}
  static int lastState=-1;static uint32_t lastFloat=0;static unsigned fishingEvents=0;
  if((int(state)!=lastState||floatId!=lastFloat)&&fishingEvents<300){lastState=int(state);lastFloat=floatId;++fishingEvents;
   logFile->event("fishing_state","\"frame\":"+std::to_string(liveFrame.frame)+",\"state\":"+std::to_string(unsigned(state))+",\"float\":"+std::to_string(floatId)+",\"charge\":"+std::to_string(read<float>(fishing+20))+",\"last_error\":"+std::to_string(unsigned(read<uint8_t>(fishing+1)))+",\"read_only\":true");}
  // The fight (2026-10-19, user after the headset run of 17:52: "cannot catch one - was the input toward the fish not
  // given? I cannot tell a mistake of mine from a bug"; every fight there went Outbreak 29 -> ReelIn 32, never FinalHook
  // 30). The game's own view of it (1.0 reflection, tools/reflection_struct_diff.py): +0x1 lastError
  // (keen::fishing::MinigameError: 9 QuickTimeEventFailed, 13 NoEnduranceLeft, 14 FishEscaped, ...), +0x10
  // outbreakDirection (keen::fishing::Direction 0 None, 1 Left, 2 Right), +0x18 / +0x1c the line's and the fish's
  // endurance in percent, +0x45 minigameInputState (0 None, 1 ValidDirection, 2 InvalidDirection: whether the game takes
  // the player's steering as the right way). On every change, and twice a second, while a fish is on.
  {static int lastDirection=-1,lastInput=-1,lastError=-1;static uint64_t lastFight=0;static unsigned fightEvents=0;
   const int direction=read<uint8_t>(fishing+0x10),input=read<uint8_t>(fishing+0x45),error=read<uint8_t>(fishing+1);
   const bool fight=state>=19&&state<=33;controllerUiFixture::fishingDirection=direction;controllerUiFixture::fishingInputState=input;
   {static const bool gainHaptic=[]{char v[8]{};const auto n=GetEnvironmentVariableA("EVR_FISH_GAIN_HAPTIC",v,sizeof v);return !(n==1&&v[0]=='0');}();
    static float lastFish=-1;static unsigned gainLogs=0;const float fish=read<float>(fishing+0x1c);
    if(fight&&std::isfinite(fish)&&lastFish>=0&&fish<lastFish-1e-4f){
     if(gainHaptic)controllerUiFixture::fishGainHapticUntil=now+450;   // a whole pulse of the pull (0.4 s)
     if(gainLogs<200&&(gainLogs<5||now-lastFight>=500)){++gainLogs;logFile->event("fishing_fish_tiring","\"frame\":"+std::to_string(liveFrame.frame)+",\"state\":"+std::to_string(unsigned(state))+",\"fish_endurance\":"+std::to_string(fish)+",\"was\":"+std::to_string(lastFish)+",\"haptic\":"+(gainHaptic?"true":"false"));}}
    lastFish=fight&&std::isfinite(fish)?fish:-1;}
   if((fight||error!=lastError)&&(direction!=lastDirection||input!=lastInput||error!=lastError||now-lastFight>=500)&&fightEvents<4000){
    lastDirection=direction;lastInput=input;lastError=error;lastFight=now;++fightEvents;
    logFile->event("fishing_fight","\"frame\":"+std::to_string(liveFrame.frame)+",\"state\":"+std::to_string(unsigned(state))+",\"direction\":"+std::to_string(direction)+
     ",\"input_state\":"+std::to_string(input)+",\"last_error\":"+std::to_string(error)+",\"line_endurance\":"+std::to_string(read<float>(fishing+0x18))+
     ",\"fish_endurance\":"+std::to_string(read<float>(fishing+0x1c))+",\"round\":"+std::to_string(unsigned(read<uint8_t>(fishing+0x21)))+",\"rounds\":"+std::to_string(unsigned(read<uint8_t>(fishing+0x20))));}}
 }catch(const std::exception&){}
}
// 1.0 (2026-10-16): keen::ecs::SlotSelection is 216 bytes (256): the equipment array holds 20 (27) InventorySlotId, the
// action bars at +0xa0 (+0xd8), actionItem +0xa8 (+0xe0), disabledEquipment +0xbc (+0xf4), the selected bar slot +0xc0
// (+0xf8) - tools/reflection_struct_diff.py on the June and 1.0 executables. keen::EquipmentSlot lost Accessory0/1 and
// ClimbWall: RangedWeapon 12 (14), Shield 13 (15). keen::ecs::InventoryLink is no component any more (the owner is found
// through the player's own InventorySetup). Found by June's sizes, none of this was read on 1.0.
constexpr unsigned selectionBytes=216,selectionDisabled=0xbc,selectionSelected=0xc0,selectionActionItem=0xa8,equipmentSlots=20,slotRanged=12,slotShield=13,slotArrow=14,slotSpell=15;
inline void tick(uintptr_t world,uint32_t local){
 const auto now=GetTickCount64();fishing(world,local,now);if(now-lastRead<100)return;lastRead=now;
 try{
  if(world!=knownWorld){knownWorld=world;selectionType=inventoryType=linkType=setupType=fishingType=-1;nativeImpactComponents::dumpTypes(world);
   for(unsigned i=0;i<nativeImpactComponents::componentCapacity;++i){
    try{const auto t=nativeImpactComponents::componentType(world,i);
     if(t.qualifiedName=="keen::ecs::SlotSelection"&&t.bytes==selectionBytes)selectionType=int(i);
     else if(t.qualifiedName=="keen::ecs::Inventory"&&t.bytes==96)inventoryType=int(i);
     else if(t.qualifiedName=="keen::ecs::InventoryLink"&&t.bytes==8)linkType=int(i);
     else if(t.qualifiedName=="keen::ecs::InventorySetup"&&t.bytes==84)setupType=int(i);
     else if(t.qualifiedName=="keen::ecs::NetworkFishing"&&t.bytes==96)fishingType=int(i);   // 1.0: 96 (88), the fields read unmoved
    }catch(const std::exception&){break;}
   }
  }
  if(selectionType<0||inventoryType<0)return;
  const auto player=nativeImpactComponents::entityAtWorld(world,local);
  const auto selection=nativeImpactComponents::component(player,unsigned(selectionType),selectionBytes);if(!selection)return;
  const auto raw=read<std::array<uint8_t,selectionBytes>>(selection);
  State next;next.at=now;std::memcpy(&next.disabled,raw.data()+selectionDisabled,4);next.selected=raw[selectionSelected];
  const auto worn=[&](unsigned slot){Worn w;std::memcpy(&w.entity,raw.data()+8*slot,4);w.index=raw[8*slot+4];stackAt(world,w.entity,w.index,w.stack);return w;};
  next.ranged=worn(slotRanged);next.shield=worn(slotShield);next.arrow=worn(slotArrow);next.spell=worn(slotSpell);
  // The owner of the rows: the player itself, or whoever the row of a worn item names.
  std::array<uint8_t,84> setup{};uint32_t owner=0;
  const auto setupOf=[&](uint32_t entity){
   if(setupType<0||!entity)return false;
   try{const auto e=nativeImpactComponents::entityAtWorld(world,entity);const auto p=nativeImpactComponents::component(e,unsigned(setupType),84);if(!p)return false;setup=read<std::array<uint8_t,84>>(p);owner=entity;return true;}
   catch(const std::exception&){return false;}};
  if(!setupOf(local)&&linkType>=0)for(const auto& w:{next.shield,next.ranged}){
   if(!w.entity)continue;
   try{const auto row=nativeImpactComponents::entityAtWorld(world,w.entity);const auto link=nativeImpactComponents::component(row,unsigned(linkType),8);if(link&&setupOf(read<uint32_t>(link)))break;}catch(const std::exception&){}
  }
  if(owner)for(unsigned k=0;k<16;++k){uint32_t entity=0;std::memcpy(&entity,setup.data()+4*k,4);if(!entity)continue;
   for(uint8_t i=0;i<8;++i){Worn w;w.entity=entity;w.index=i;if(!stackAt(world,entity,i,w.stack))break;next.rows.push_back({w,setup[64+k]});}}
  if(!next.shield.stack.item)for(const auto& [w,category]:next.rows)if(w.stack.item&&slotType(w.stack.item)==18){next.keptShield=w;break;}
  next.valid=true;
  {std::lock_guard<std::mutex> lock(mutex);latest=next;}
  if(events>=80)return;
  const auto bytes=[](const uint8_t* p,size_t n){static const char* digits="0123456789abcdef";std::string s;for(size_t i=0;i<n;++i){s+=digits[p[i]>>4];s+=digits[p[i]&15];}return s;};
  std::ostringstream out;out<<"\"selected_slot\":"<<unsigned(next.selected)<<",\"disabled_equipment\":"<<next.disabled<<",\"equipment\":[";
  for(unsigned slot=0;slot<equipmentSlots;++slot){const auto w=worn(slot);out<<(slot?",":"")<<'['<<w.entity<<','<<unsigned(w.index)<<','<<w.stack.item<<','<<jsonString(bytes(w.stack.data.data(),8))<<']';}
  out<<"],\"kept_shield\":["<<next.keptShield.entity<<','<<unsigned(next.keptShield.index)<<','<<next.keptShield.stack.item<<"],\"action_item\":"<<jsonString(bytes(raw.data()+selectionActionItem,20))<<",\"owner\":"<<owner<<",\"owner_is_player\":"<<(owner==local?"true":"false");
  if(owner){out<<",\"generic_slots\":"<<unsigned(setup[81])<<",\"available_slots\":"<<unsigned(setup[82])<<",\"setup_flags\":"<<unsigned(setup[80])<<",\"rows\":[";
   bool first=true;
   for(unsigned k=0;k<16;++k){uint32_t entity=0;std::memcpy(&entity,setup.data()+4*k,4);if(!entity)continue;
    out<<(first?"":",")<<"{\"entity\":"<<entity<<",\"category\":"<<unsigned(setup[64+k])<<",\"items\":[";first=false;bool any=false;
    for(const auto& [w,category]:next.rows)if(w.entity==entity){out<<(any?",":"")<<w.stack.item;any=true;}
    out<<"]}";}
   out<<']';}
  out<<",\"read_only\":true";
  const auto line=out.str();if(line==lastLine)return;lastLine=line;++events;
  logFile->event("equipment_layout","\"frame\":"+std::to_string(liveFrame.frame)+","+line);
 }catch(const std::exception&){}
}
}
