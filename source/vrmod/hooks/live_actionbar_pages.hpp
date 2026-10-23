// Read-only: is the Shroud's timer out? The player's keen::ecs::FogResistanceState holds one float, accumulatedValue:
// what the Shroud has taken of the time the player may stay in it (the HUD shows what is left of it in the
// compass's place). While it is above zero the layer is told to keep the compass shown (evrSetUiCompass; the HUD
// auto-hide would fade it with the rest). About ten times a second, in whichever of the worlds holds the component.
// EVR_FOG_TIMER_TEST=1 in the game's environment tells the layer so always (for the Mock, whose home has no Shroud).
namespace fogTimer {
using CompassFn=int(*)(int);
inline CompassFn setCompass=nullptr;inline bool resolved=false,forced=false;
struct World {uintptr_t world=0;int type=-1;};inline std::array<World,4> worlds{};inline unsigned nextWorld=0;
inline uint64_t lastRead=0,lastLog=0;inline int told=-1;inline unsigned lines=0;inline float lastValue=-1;
// A look at the Shroud (2026-10-13, user after the headset run game-20261013-105615: "the Shroud's effect did not
// seem to reach the edge of the view the way the damage effect does - widen it"). Which effect that is could not be
// told from that run's records, and the game's HUD has no vignette of its own for the Shroud (only low health,
// freezing and blur: 0xaf3290, 0xaa9270, 0xa45e00). So the next run takes the pictures itself: both eyes and the UI
// image once 3 s after the timer comes out and once more 45 s after it (the layer's one-shot capture, the same that
// every headset run already takes three times; two per session at the most). EVR_FOG_CAPTURE=0 leaves them out.
using CaptureFn=int(*)(uint32_t);
inline CaptureFn capture=nullptr;inline bool captureWanted=true;inline uint64_t outSince=0;inline unsigned captures=0;
constexpr std::array<uint64_t,2> captureAfter{3000,45000};
inline void tick(uintptr_t world,uint32_t local){
 const auto now=GetTickCount64();if(now-lastRead<100)return;lastRead=now;
 try{
  if(!resolved){resolved=true;
   if(auto layer=GetModuleHandleW(L"evr_diagnostic.dll"))setCompass=reinterpret_cast<CompassFn>(GetProcAddress(layer,"evrSetUiCompass"));
   char value[8]{};const auto n=GetEnvironmentVariableA("EVR_FOG_TIMER_TEST",value,sizeof value);forced=n&&n<sizeof value&&value[0]=='1';
   {char off[8]{};const auto m=GetEnvironmentVariableA("EVR_FOG_CAPTURE",off,sizeof off);captureWanted=!(m&&m<sizeof off&&off[0]=='0');
    if(captureWanted)if(auto layer=GetModuleHandleW(L"evr_diagnostic.dll"))capture=reinterpret_cast<CaptureFn>(GetProcAddress(layer,"evrRequestNativeXrCapture"));}
   logFile->event("fog_timer_enabled",std::string("\"layer_keeps_the_compass\":")+(setCompass?"true":"false")+",\"forced_for_a_test\":"+(forced?"true":"false")+",\"read_only\":true");}
  World* known=nullptr;for(auto& w:worlds)if(w.world==world){known=&w;break;}
  if(!known){known=&worlds[nextWorld];nextWorld=(nextWorld+1)%unsigned(worlds.size());*known={world,-1};
   for(unsigned i=0;i<nativeImpactComponents::componentCapacity;++i){
    try{const auto t=nativeImpactComponents::componentType(world,i);if(t.qualifiedName=="keen::ecs::FogResistanceState"&&t.bytes==4){known->type=int(i);break;}}
    catch(const std::exception&){break;}}
   if(lines<40){++lines;logFile->event("fog_timer_world","\"world\":"+hex(world)+",\"component_type\":"+std::to_string(known->type));}}
  // The player's own state says it first: keen::actor::StateFlag InFog (bit 13 of the actor's state, the one
  // input_head_idle.hpp reads for walking). The timer's value follows a moment later, and while the game slides the
  // timer in, the part of it below the compass showed alone, cut off (headset run game-20261013-141010: "on entering
  // the Shroud the cut-off Shroud label under the compass comes out of the hidden area").
  // In the water (keen::actor::StateFlag FloatingOnWater 43, UnderWater 44): the game shows what is left of the
  // player's breath in the same place under the compass, and slides it in the same way (headset run 20261014-124515,
  // user: "keep the timer of the oxygen left under water shown, the same area as the Shroud's"; "on getting into the
  // water a little of the label under the compass shows out of the hidden UI, cut off, as it did with the Shroud").
  bool inFog=false,inWater=false;{std::lock_guard<std::mutex> lock(inputHeadIdle::mutex);const auto& s=inputHeadIdle::latest;
   const bool fresh=s.actorKnown&&s.milliseconds&&now>=s.milliseconds&&now-s.milliseconds<=1000;
   inFog=fresh&&(s.actorState&(1ull<<13))!=0;inWater=fresh&&(s.actorState&((1ull<<43)|(1ull<<44)))!=0;}
  float value=0;bool present=false;
  if(known->type>=0){const auto player=nativeImpactComponents::entityAtWorld(world,local);
   if(const auto state=nativeImpactComponents::component(player,unsigned(known->type),4)){value=read<float>(state);present=std::isfinite(value);}}
  if(!present&&!forced&&!inFog&&!inWater)return;     // this world does not hold it: the other one tells
  const bool out=forced||value>0||inFog;
  if(setCompass&&(out||inWater))setCompass(1);
  if(!out)outSince=0;
  else{if(!outSince)outSince=now;
   if(capture&&captures<captureAfter.size()&&now-outSince>=captureAfter[captures]){
    const int taken=capture(5);                       // 1: another capture is waiting - the next tick asks again
    if(taken==0){++captures;logFile->event("fog_timer_capture","\"frame\":"+std::to_string(liveFrame.frame)+",\"number\":"+std::to_string(captures)+",\"accumulated\":"+std::to_string(value)+
     ",\"timer_out_for_ms\":"+std::to_string(now-outSince)+",\"tag\":5");}}}
  if((int(out)+2*int(inWater)!=told||((out||inWater)&&now-lastLog>=10000))&&lines<400){++lines;told=int(out)+2*int(inWater);lastLog=now;lastValue=value;
   logFile->event("water_timer_state","\"frame\":"+std::to_string(liveFrame.frame)+",\"in_water\":"+(inWater?"true":"false")+",\"compass_kept\":"+(setCompass&&inWater?"true":"false"));
   logFile->event("fog_timer_state","\"frame\":"+std::to_string(liveFrame.frame)+",\"accumulated\":"+std::to_string(value)+",\"timer_out\":"+(out?"true":"false")+",\"in_fog_flag\":"+(inFog?"true":"false")+",\"compass_kept\":"+(setCompass&&out?"true":"false")+",\"read_only\":true");}
 }catch(const std::exception&){}
}
}
// Read-only: what the two action bar pages hold and which slot is selected, on record whenever it changes.
// SlotSelection: +216 the two action bar entities, +248 the selected slot; each bar's Inventory is eight
// {item, count, entity} of 12 bytes (docs/checkpoint_20261005_motion.md, 18:00). No game call, no write.
namespace liveActionbars {
inline uintptr_t knownWorld=0;inline int selectionType=-1,inventoryType=-1;inline uint64_t lastRead=0;inline unsigned events=0;
inline std::array<uint8_t,208> lastBytes{};
// The selected slot's item for the frontend (controller_ui_fixture.hpp HandItem): refreshed about ten times a second.
inline std::mutex selectedMutex;inline controllerUiFixture::HandItem selected;inline uint64_t selectedAt=0;inline uint32_t describedItem=UINT32_MAX;
inline controllerUiFixture::HandItem hand(uint64_t now){
 std::lock_guard<std::mutex> lock(selectedMutex);auto out=selected;out.valid=out.valid&&pauseClock::fresh(selectedAt,now,1000);return out;
}
// The items of both bars' 16 slots (the holsters: where the worn bow is on the bar, hooks/holster_actions.hpp).
inline std::array<uint32_t,16> barItems{};inline uint64_t barItemsAt=0;
// The bars' entities, each slot's item category (keen ItemCategory: 7 Consumables) and the selected slot - for the consumable
// held from the quick wheel (hooks/input_head_idle.hpp consumableHoldInput).
inline std::array<uint32_t,2> barEntities{};inline std::array<uint8_t,16> barCategories{};inline int selectedNow=-1;
inline std::unordered_map<uint32_t,uint8_t> categoryOf;   // item -> category, read once (0xff: not readable yet)
inline uint32_t consumableAt(const game::InventorySlotBytes& slot){
 uint32_t entity=0;std::memcpy(&entity,slot.data(),4);const uint8_t index=slot[4];
 std::lock_guard<std::mutex> lock(selectedMutex);if(!entity||index>=8||!pauseClock::fresh(barItemsAt,GetTickCount64(),1000))return 0;
 for(unsigned b=0;b<2;++b)if(barEntities[b]==entity){const unsigned i=b*8+index;return barCategories[i]==7?barItems[i]:0;}
 return 0;
}
inline int selectedSlot(){std::lock_guard<std::mutex> lock(selectedMutex);return pauseClock::fresh(barItemsAt,GetTickCount64(),1000)?selectedNow:-1;}
inline uint8_t slotOf(uint32_t item,uint64_t now){
 std::lock_guard<std::mutex> lock(selectedMutex);if(!item||!pauseClock::fresh(barItemsAt,now,1000))return 0xff;
 for(uint8_t i=0;i<16;++i)if(barItems[i]==item)return i;
 return 0xff;
}
inline void tick(uintptr_t world,uint32_t local){
 const auto now=GetTickCount64();if(now-lastRead<100)return;lastRead=now;
 try{
  if(world!=knownWorld){knownWorld=world;selectionType=inventoryType=-1;
   for(unsigned i=0;i<nativeImpactComponents::componentCapacity;++i){
    try{const auto t=nativeImpactComponents::componentType(world,i);
     if(t.qualifiedName=="keen::ecs::SlotSelection"&&t.bytes==216)selectionType=int(i);
     else if(t.qualifiedName=="keen::ecs::Inventory"&&t.bytes==96)inventoryType=int(i);
    }catch(const std::exception&){break;}
   }
  }
  if(selectionType<0||inventoryType<0)return;
  const auto player=nativeImpactComponents::entityAtWorld(world,local);const auto selection=nativeImpactComponents::component(player,unsigned(selectionType),216);if(!selection)return;
  // 1.0: the bars at +0xa0, the selected slot at +0xc0 (June +216 / +248; live_equipment.hpp)
  const auto bars=read<std::array<uint32_t,2>>(selection+0xa0);const auto slot=read<uint8_t>(selection+0xc0);
  std::array<uint8_t,208> bytes{};std::memcpy(bytes.data(),bars.data(),8);bytes[8]=slot;
  for(unsigned b=0;b<2;++b){
   if(!bars[b])continue;
   const auto bar=nativeImpactComponents::entityAtWorld(world,bars[b]);const auto inventory=nativeImpactComponents::component(bar,unsigned(inventoryType),96);if(!inventory)return;
   const auto raw=read<std::array<uint8_t,96>>(inventory);std::memcpy(bytes.data()+16+96*b,raw.data(),96);
  }
  {std::array<uint32_t,16> items{};for(unsigned i=0;i<16;++i)if(bars[i/8])std::memcpy(&items[i],bytes.data()+16+96*(i/8)+12*(i%8),4);
   std::array<uint8_t,16> categories{};categories.fill(0xff);
   if(const auto registry=attachmentObserver::itemRegistry.load())for(unsigned i=0;i<16;++i){if(!items[i])continue;
    auto c=categoryOf.find(items[i]);
    if(c==categoryOf.end()||c->second==0xff){auto reader=inputRanged::readItemBytes;const auto found=game::lookupItem(reader,registry,items[i]);game::ItemMetadata m;
     const uint8_t category=game::readItemMetadata(reader,found,items[i],m)?uint8_t(m.category):uint8_t(0xff);
     if(categoryOf.size()<4096)c=categoryOf.insert_or_assign(items[i],category).first;}
    if(c!=categoryOf.end())categories[i]=c->second;}
   {std::lock_guard<std::mutex> lock(selectedMutex);barItems=items;barItemsAt=now;barEntities=bars;barCategories=categories;selectedNow=slot;}}
  {uint32_t item=0,count=0;
   if(slot<16){std::memcpy(&item,bytes.data()+16+96*(slot/8)+12*(slot%8),4);std::memcpy(&count,bytes.data()+16+96*(slot/8)+12*(slot%8)+4,4);}
   std::lock_guard<std::mutex> lock(selectedMutex);
   if(item!=describedItem){
    describedItem=item;selected={};selected.item=item;
    const auto registry=attachmentObserver::itemRegistry.load();
    if(item&&registry){auto reader=inputRanged::readItemBytes;const auto found=game::lookupItem(reader,registry,item);game::ItemMetadata m;
     if(game::readItemMetadata(reader,found,item,m)){selected.category=m.category;selected.itemSlot=m.slot;
      if(m.category==2){const auto wc=game::itemWeaponCategory(reader,registry,found,item);selected.typeKnown=wc.status==game::ItemLookupStatus::Found;selected.weaponType=wc.type;}}
     else describedItem=UINT32_MAX;   // not readable yet: again on the next tick
    }
    if(describedItem==item)logFile->event("actionbar_selected_item","\"frame\":"+std::to_string(liveFrame.frame)+",\"slot\":"+std::to_string(slot)+",\"item\":"+std::to_string(item)+",\"category\":"+std::to_string(selected.category)+
     ",\"item_slot\":"+std::to_string(selected.itemSlot)+",\"weapon_type\":"+std::to_string(selected.weaponType)+",\"weapon_type_known\":"+(selected.typeKnown?"true":"false")+",\"read_only\":true");
   }
   // (A slot above 15 is "no slot" - the holsters' bare hands, hooks/holster_actions.hpp: an empty hand, as an empty slot is.)
   selected.count=count;selected.valid=describedItem==item;selectedAt=now;
  }
  if(bytes==lastBytes||events>=160)return;lastBytes=bytes;++events;
  std::ostringstream out;out<<"\"frame\":"<<liveFrame.frame<<",\"selected_slot\":"<<unsigned(slot)<<",\"bar_entities\":["<<bars[0]<<','<<bars[1]<<"],\"pages\":[";
  for(unsigned b=0;b<2;++b){out<<(b?",[":"[");
   for(unsigned k=0;k<8;++k){uint32_t item=0,count=0,entity=0;std::memcpy(&item,bytes.data()+16+96*b+12*k,4);std::memcpy(&count,bytes.data()+16+96*b+12*k+4,4);std::memcpy(&entity,bytes.data()+16+96*b+12*k+8,4);
    out<<(k?",":"")<<'['<<item<<','<<count<<','<<entity<<']';}
   out<<']';}
  out<<"],\"read_only\":true";logFile->event("actionbar_pages",out.str());
 }catch(const std::exception&){}
}
}
