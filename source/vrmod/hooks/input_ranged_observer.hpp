// Observe server_player_ranged_weapon immediately before its ECS iterator
// advances. Shared iterator hook/LastError/callback lifetime is owned by
// inputActor; this helper never mutates components or retains their pointers.
namespace inputRanged {
inline bool liveBowWatch=false;inline std::atomic<unsigned> bowSamples{0};
struct BowSeen {uintptr_t world=0;uint32_t item=0,sequence=0;uint64_t digital=0;uint8_t state=0,queued=0;};
inline std::mutex bowMutex;inline std::array<BowSeen,4> bowSeen{};
// Read-only identity evidence, not a lifetime/shot authorization token.
// 1.0 (2026-10-16, tools/reflection_struct_diff.py on the June and 1.0 executables): keen::ecs::RangedWeapon is 200 bytes;
// its state machine grew to 0x50 (isFireQueued still +0x39), and everything after it moved: originalTrigger 0x50 (0x40),
// currentTrigger 0x51 (0x41), triggerWeaponInstance 0x54 (0x44; its itemId +8: 0x5c, June 0x4c), triggerAmmunitionInstance
// 0x68 (0x58), usageFeedbackMask 0x7c (missingResourceMask 0x6c), shallPlayerTurnTowardsTarget 0xc4 (0x84; the aim snap
// fields came between). keen::ecs::SlotSelection: action bars +0xa0 (+216), actionItem +0xa8 (+0xe0, its itemId +0xb0),
// the selected bar slot +0xc0 (+248), the equipment before +0xa0 (live_equipment.hpp). Read at June's places, the bow's
// and the staff's item was never the reviewed one and the aim stayed the head's.
constexpr size_t rangedWeaponBytes=0xc5,rangedItem=0x5c,rangedTurn=0xc4,rangedOriginalTrigger=0x50,rangedCurrentTrigger=0x51,rangedUsageMask=0x7c,
    rangedWeaponInstance=0x54,rangedAmmoInstance=0x68,selectionBars=0xa0,selectionActionItem=0xa8,selectionActionId=0xb0,selectionSlot=0xc0;
struct BowNativeIdentity {uintptr_t world=0,input=0,weapon=0,actor=0;uint32_t entity=0;uint64_t milliseconds=0;std::array<uint8_t,rangedWeaponBytes> bytes{};};
inline std::array<BowNativeIdentity,4> bowIdentities{};
inline BowNativeIdentity bowIdentity(uintptr_t input,uintptr_t weapon){
 std::lock_guard<std::mutex> lock(bowMutex);
 for(const auto& v:bowIdentities)if(v.input==input&&v.weapon==weapon)return v;
 return {};
}


inline bool liveEquipmentWatch=false; // opt-in read-only actionbar evidence for held effects
constexpr uintptr_t callerRva=0x2e9274;
inline std::atomic<unsigned> calls{0},samples{0},failures{0},selectionEvents{0};
// keen::ecs::SlotSelection (reflection size 256): equipment 0..215 (EquipmentSlotData,
// layout not in reflection), actionbarEntityIds +216 (2 EntityId), actionItem +224
// (ItemInstanceInfo, only during use), actionbarSlotSelection +248 (InventorySlotIndex).
// Explicit wheel fixtures log every change read-only to map wheel angles to slots.
inline uint64_t lastSelectionDigest=0;
// Value-only actionbar snapshot for explicit router-equip fixtures (game175428 layout):
// Inventory(96B) of SlotSelection.actionbarEntityIds[0] = 8 x {itemId,count,itemEntity}.
inline std::mutex actionbarMutex;inline evr::motion::ActionbarSnapshot actionbar;inline uint64_t actionbarSequence=0;
inline unsigned actionbarEvents=0;
inline std::atomic<bool> catalogClaimed{false};
struct MeleeSelection {uint32_t entity=0,item=0;uint64_t milliseconds=0;bool supported=false;};
inline std::mutex meleeMutex;inline MeleeSelection meleeSelection;
inline bool readItemBytes(uint64_t p,void* out,size_t n){SIZE_T copied=0;return ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<const void*>(p),out,n,&copied)&&copied==n;}
inline void metadataJson(std::ostringstream& out,const game::ItemMetadata& m){
    out<<"{\"item_id\":"<<m.id<<",\"category\":"<<unsigned(m.category)<<",\"ammunition_type\":"<<unsigned(m.ammunition)
        <<",\"equipment_slot\":"<<unsigned(m.slot)<<",\"ranged_weapon\":"<<(m.rangedWeapon()?"true":"false")<<'}';
}
template<size_t N> std::string categoryBytes(const std::array<uint8_t,N>& b){std::ostringstream s;s<<std::hex<<std::setfill('0');for(auto x:b)s<<std::setw(2)<<unsigned(x);return s.str();}
inline void categoryCatalog(uintptr_t registry,uint32_t entity){
 auto reader=readItemBytes;game::ItemMapHeader map;
 if(!game::readWeaponCategoryMap(reader,registry,map)||!game::validItemMap(map)||map.capacity>4096||map.count>64){
  // 1.0 layout search: the small maps inside the registry (the weapon categories were at +0x2c0 before 1.0).
  {static std::atomic<bool> told{false};if(!told.exchange(true))try{std::ostringstream out;out<<"\"registry\":"<<hex(registry)<<",\"maps\":[";bool first=true;
    for(uint64_t at=0;at<0x2000;at+=8){game::ItemMapHeader h;if(!game::readItemMap(reader,registry+at,h)||!game::validItemMap(h)||!h.count)continue;
     // A weapon-category-like map: values whose first 16 bytes hash (FNV-1a) to their key, with a type byte at +84 <= 15.
     unsigned guidKeyed=0,typed=0,checked=0;
     for(uint64_t i=0;i<h.capacity&&checked<4;i++){uint64_t word=0;if(!game::itemRead(reader,h.bitmap+(i/64)*8,word)||!(word&(1ull<<(i%64))))continue;
      uint32_t key=0;uint64_t value=0;std::array<uint8_t,88> res{};++checked;
      if(game::itemRead(reader,h.keys+i*4,key)&&game::itemRead(reader,h.values+i*8,value)&&value&&game::itemRead(reader,value,res)){
       game::WeaponCategoryGuid g;std::copy(res.begin(),res.begin()+16,g.begin());if(game::weaponCategoryHash(g)==key)++guidKeyed;if(res[84]<=15)++typed;}}
     out<<(first?"":",")<<"{\"offset\":"<<(at+0x38)<<",\"capacity\":"<<h.capacity<<",\"count\":"<<h.count<<",\"guid_keyed\":"<<guidKeyed<<",\"typed\":"<<typed<<",\"checked\":"<<checked<<'}';first=false;}
    out<<']';logFile->event("weapon_category_map_search",out.str());}catch(...){}}
  throw std::runtime_error("weapon category map exceeds read-only budget");}
 unsigned entries=0;
 for(uint64_t i=0;i<map.capacity;i++){
  uint64_t bitmap=0;if(!game::itemRead(reader,map.bitmap+(i/64)*8,bitmap))throw std::runtime_error("category bitmap unreadable");
  if(!(bitmap&(1ull<<(i%64))))continue;
  uint32_t key=0;uint64_t resource=0;game::WeaponCategoryGuid guid;
  if(!game::itemRead(reader,map.keys+i*4,key)||!game::itemRead(reader,map.values+i*8,resource)||!game::itemRead(reader,resource,guid))throw std::runtime_error("category resource unreadable");
  const auto category=game::lookupWeaponCategory(reader,registry,guid);
  if(category.status!=game::ItemLookupStatus::Found||category.hash!=key||category.pointer!=resource||category.index!=i)throw std::runtime_error("category identity/lookup changed");
  std::ostringstream out;out<<"\"entity\":"<<entity<<",\"registry\":"<<hex(registry)<<",\"index\":"<<i<<",\"key\":"<<key
   <<",\"resource\":"<<hex(resource)<<",\"guid\":"<<jsonString(categoryBytes(guid))<<",\"bytes\":"<<jsonString(categoryBytes(category.resource))
   <<",\"type\":"<<unsigned(category.type)<<",\"two_hand_melee\":"<<(category.twoHandMelee()?"true":"false")<<",\"one_hand_melee\":"<<(category.oneHandMelee()?"true":"false")<<",\"read_only\":true";
  logFile->event("input_weapon_category_entry",out.str());entries++;
 }
 game::ItemMapHeader again;if(entries!=map.count||!game::readWeaponCategoryMap(reader,registry,again)||!(map==again))throw std::runtime_error("category map changed/count differs");
 logFile->event("input_weapon_category_catalog","\"capacity\":"+std::to_string(map.capacity)+",\"count\":"+std::to_string(map.count)+",\"entries\":"+std::to_string(entries)+",\"stable_header\":true,\"read_only\":true");
}
inline void catalog(uintptr_t registry,uint32_t entity){
    if(catalogClaimed.exchange(true))return;
    categoryCatalog(registry,entity);
    auto reader=readItemBytes;game::ItemMapHeader h;
    if(!game::readItemMap(reader,registry,h)||!game::validItemMap(h))throw std::runtime_error("item registry header invalid");
    unsigned entries=0,scanned=0;
    for(uint64_t i=0;i<std::min<uint64_t>(h.capacity,4096)&&entries<24;++i){
        ++scanned;uint64_t word=0;
        if(!game::itemRead(reader,h.bitmap+(i/64)*8,word))throw std::runtime_error("item catalog bitmap unreadable");
        if(!(word&(1ull<<(i%64))))continue;
        uint32_t id=0;uint64_t pointer=0;
        if(!game::itemRead(reader,h.keys+i*4,id)||!game::itemRead(reader,h.values+i*8,pointer)||!id)
            throw std::runtime_error("item catalog bucket unreadable/zero");
        const auto found=game::lookupItem(reader,registry,id);game::ItemMetadata m;
        if(found.pointer!=pointer||found.index!=i||!game::readItemMetadata(reader,found,id,m))
            throw std::runtime_error("item registry identity/metadata mismatch");
        std::ostringstream row;row<<"\"entity\":"<<entity<<",\"index\":"<<i<<",\"lookup_index\":"<<found.index<<",\"probes\":"<<found.probes
            <<",\"pointer_matches\":true,\"metadata\":";metadataJson(row,m);row<<",\"read_only\":true";
        logFile->event("input_item_catalog_entry",row.str());++entries;
        const auto category=game::itemWeaponCategory(reader,registry,found,id);std::array<uint8_t,0x108> raw{};
        if(!game::itemRead(reader,found.pointer,raw))throw std::runtime_error("item category source unreadable");
        std::ostringstream w;w<<"\"entity\":"<<entity<<",\"item\":"<<id<<",\"status\":"<<jsonString(game::itemLookupName(category.status))
         <<",\"raw_item\":"<<jsonString(categoryBytes(raw))<<",\"guid\":"<<jsonString(categoryBytes(category.guid))<<",\"hash\":"<<category.hash
         <<",\"index\":"<<category.index<<",\"resource\":"<<hex(category.pointer)<<",\"type\":"<<unsigned(category.type)<<",\"read_only\":true";
        logFile->event("input_item_weapon_category",w.str());
    }
    game::ItemMapHeader again;if(!game::readItemMap(reader,registry,again)||!(h==again))throw std::runtime_error("item catalog changed");
    logFile->event("input_item_catalog","\"entity\":"+std::to_string(entity)+",\"capacity\":"+std::to_string(h.capacity)+",\"count\":"+std::to_string(h.count)+
        ",\"entries\":"+std::to_string(entries)+",\"scanned\":"+std::to_string(scanned)+",\"stable_header\":true,\"read_only\":true");
}
// game180638: the ranged task alternates between two Worlds (client prediction and server).
// Each World keeps its own type id and last decoded snapshot; metadata is decoded only when its
// Inventory bytes change. The published snapshot is valid only while every fresh World agrees.
struct WorldActionbar {uintptr_t world=0;int type=-1;std::array<uint8_t,96> raw{};evr::motion::ActionbarSnapshot snapshot;};
inline std::array<WorldActionbar,4> worldActionbars{};inline unsigned actionbarTypeEvents=0;
inline bool sameBar(const evr::motion::ActionbarSnapshot& a,const evr::motion::ActionbarSnapshot& b){
    if(a.selected!=b.selected||a.barEntity!=b.barEntity)return false;
    for(size_t k=0;k<8;k++)if(a.slots[k].item!=b.slots[k].item||a.slots[k].count!=b.slots[k].count||a.slots[k].entity!=b.slots[k].entity)return false;
    return true;
}
inline void publishActionbar(uint32_t entity,uintptr_t state,uintptr_t selection,uintptr_t registry){
    const auto bar=read<std::array<uint32_t,2>>(selection+selectionBars);const auto selected=read<uint8_t>(selection+selectionSlot);
    uintptr_t world=read<uintptr_t>(state+0x30);
    if(!world){std::lock_guard<std::mutex> lock(rootCamera::mutex);world=rootCamera::latest.entity==entity?rootCamera::latest.world:0;}
    if(!world||!bar[0])return;
    const auto now=GetTickCount64();
    WorldActionbar* w=nullptr;
    for(auto& c:worldActionbars)if(c.world==world){w=&c;break;}
    if(!w){
        w=&worldActionbars[0];for(auto& c:worldActionbars)if(!c.world||c.snapshot.milliseconds<w->snapshot.milliseconds)w=&c;
        *w=WorldActionbar{};w->world=world;
        for(unsigned i=0;i<nativeImpactComponents::componentCapacity;i++){
            try{const auto t=nativeImpactComponents::componentType(world,i);if(t.qualifiedName=="keen::ecs::Inventory"&&t.bytes==96){w->type=int(i);break;}}catch(...){break;}
        }
        if(actionbarTypeEvents++<16)logFile->event("actionbar_inventory_type","\"world\":"+hex(world)+",\"type\":"+std::to_string(w->type));
    }
    if(w->type<0)return;
    const auto e=nativeImpactComponents::entityAtWorld(world,bar[0]);const auto p=nativeImpactComponents::component(e,unsigned(w->type),96);
    if(!p)return;
    const auto raw=read<std::array<uint8_t,96>>(p);if(raw!=read<std::array<uint8_t,96>>(p)||selected!=read<uint8_t>(selection+selectionSlot))return;
    auto& snap=w->snapshot;
    if(!snap.valid||raw!=w->raw||snap.barEntity!=bar[0]){
        evr::motion::ActionbarSnapshot fresh;fresh.valid=true;fresh.barEntity=bar[0];auto reader=readItemBytes;
        for(size_t k=0;k<8;k++){
            auto& slot=fresh.slots[k];std::memcpy(&slot.item,raw.data()+12*k,4);std::memcpy(&slot.count,raw.data()+12*k+4,4);std::memcpy(&slot.entity,raw.data()+12*k+8,4);
            if(!slot.item)continue;
            const auto found=game::lookupItem(reader,registry,slot.item);game::ItemMetadata m;
            if(game::readItemMetadata(reader,found,slot.item,m)){slot.category=m.category;slot.equipmentSlot=m.slot;slot.metadata=true;}
        }
        snap=fresh;w->raw=raw;
    }
    snap.selected=selected;snap.milliseconds=now;
    evr::motion::ActionbarSnapshot merged;unsigned worlds=0;bool agree=true;
    for(const auto& c:worldActionbars){
        if(!c.world||!c.snapshot.valid||now<c.snapshot.milliseconds||now-c.snapshot.milliseconds>500)continue;
        if(!worlds)merged=c.snapshot;else agree=agree&&sameBar(merged,c.snapshot);
        ++worlds;
    }
    merged.valid=worlds>0&&agree;merged.milliseconds=now;
    bool changed;
    {std::lock_guard<std::mutex> lock(actionbarMutex);merged.sequence=++actionbarSequence;changed=merged.valid!=actionbar.valid||!sameBar(merged,actionbar);actionbar=merged;}
    if(changed&&actionbarEvents++<128){
        std::string slots;
        for(size_t k=0;k<8;k++){const auto& t=merged.slots[k];slots+=std::string(k?",":"")+"["+std::to_string(t.item)+","+std::to_string(t.count)+","+std::to_string(t.entity)+","+std::to_string(t.category)+","+std::to_string(t.equipmentSlot)+","+(t.metadata?"1":"0")+"]";}
        logFile->event("actionbar_snapshot","\"sequence\":"+std::to_string(merged.sequence)+",\"valid\":"+(merged.valid?"true":"false")+",\"worlds\":"+std::to_string(worlds)+
            ",\"agree\":"+(agree?"true":"false")+",\"bar_entity\":"+std::to_string(merged.barEntity)+",\"selected\":"+std::to_string(merged.selected)+",\"slots\":["+slots+"],\"read_only\":true");
    }
}
inline void logSelectionIfChanged(uint32_t entity,uintptr_t selection,uintptr_t registry){
    const auto equipment=read<std::array<uint8_t,selectionBars>>(selection);const auto bar=read<std::array<uint32_t,2>>(selection+selectionBars);
    const auto slot=read<uint8_t>(selection+selectionSlot);const auto action=read<uint32_t>(selection+selectionActionId);
    uint64_t digest=1469598103934665603ull;auto mix=[&](const uint8_t* d,size_t n){for(size_t i=0;i<n;i++){digest^=d[i];digest*=1099511628211ull;}};
    mix(equipment.data(),equipment.size());mix(reinterpret_cast<const uint8_t*>(bar.data()),8);mix(&slot,1);
    if(digest==lastSelectionDigest)return;lastSelectionDigest=digest;
    std::ostringstream out;out<<"\"event_index\":"<<selectionEvents.fetch_add(1)+1<<",\"entity\":"<<entity<<",\"actionbar_slot\":"<<unsigned(slot)
        <<",\"actionbar_entities\":["<<bar[0]<<','<<bar[1]<<"],\"action_item\":"<<action<<",\"equipment_bytes\":"<<jsonString(categoryBytes(equipment))
        <<",\"frame\":"<<liveFrame.frame<<",\"milliseconds\":"<<GetTickCount64()<<",\"read_only\":true";
    logFile->event("input_slot_selection",out.str());(void)registry;
}
// A value-only witness is consumed exclusively inside the next live ranged query.
// The cached World address is a comparison key, never dereferenced as a capability.
struct SpawnWitness {uintptr_t world=0,actor=0;uint32_t owner=0,spawned=0;unsigned sample=0,attempts=0;uint64_t milliseconds=0;};
inline SpawnWitness spawnWitness;
inline void publishSpawn(uintptr_t world,uintptr_t actor,uint32_t owner,uint32_t spawned,unsigned sample){
 std::lock_guard<std::mutex> lock(bowMutex);spawnWitness={world,actor,owner,spawned,sample,0,GetTickCount64()};
}
inline void inspectSpawn(uintptr_t state,uint32_t owner,uintptr_t actor){
 SpawnWitness w;const auto world=read<uintptr_t>(state+0x30),now=GetTickCount64();
 {std::lock_guard<std::mutex> lock(bowMutex);if(!spawnWitness.sample)return;
  if(now<spawnWitness.milliseconds||now-spawnWitness.milliseconds>500||spawnWitness.attempts>=8){spawnWitness={};return;}
  if(spawnWitness.world!=world||spawnWitness.owner!=owner||spawnWitness.actor!=actor)return;
  w=spawnWitness;++spawnWitness.attempts;
 }
 try{
  const auto local=nativeImpactComponents::entityAt(state,owner);
  const auto actorType=nativeImpactComponents::identify(local,actor,3600);
  const auto ownerType=nativeImpactComponents::componentType(world,actorType);
  const auto e=nativeImpactComponents::entityAt(state,w.spawned);
  const auto mask=read<std::array<uint64_t,10>>(e.layout);
  std::ostringstream components;unsigned count=0,total=0;
  for(unsigned id=0;id<nativeImpactComponents::componentCapacity;++id){
   if(!(mask[id/64]&(1ull<<(id%64))))continue;
   if(++count>96)throw std::runtime_error("spawn component count bound");
   const auto type=nativeImpactComponents::componentType(world,id);
   const auto pointer=nativeImpactComponents::component(e,id,type.bytes);
   if(!pointer)throw std::runtime_error("spawn component stride mismatch");
   std::string bytes;
   if(type.bytes&&type.bytes<=256&&total+type.bytes<=4096){
    std::vector<uint8_t> a(type.bytes),b(type.bytes);SIZE_T got=0;
    if(!ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<const void*>(pointer),a.data(),a.size(),&got)||got!=a.size()||
       !ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<const void*>(pointer),b.data(),b.size(),&got)||got!=b.size()||a!=b)
     throw std::runtime_error("spawn component changed during snapshot");
    const char* h="0123456789abcdef";for(auto v:a){bytes+=h[v>>4];bytes+=h[v&15];}total+=type.bytes;
   }
   components<<(count>1?",":"")<<"{\"id\":"<<id<<",\"name\":"<<jsonString(type.qualifiedName)<<",\"size\":"<<type.bytes
     <<",\"address\":"<<hex(pointer)<<",\"bytes\":"<<jsonString(bytes)<<"}";
  }
  const auto after=nativeImpactComponents::entityAt(state,w.spawned);
  if(after.record!=e.record||after.layout!=e.layout||after.data!=e.data||after.row!=e.row||
    mask!=read<std::array<uint64_t,10>>(e.layout)||read<uintptr_t>(state+0x30)!=world||
    nativeImpactComponents::component(nativeImpactComponents::entityAt(state,owner),actorType,3600)!=actor)
   throw std::runtime_error("spawn query ownership changed");
  logFile->event("bow_spawn_ecs_observed","\"sample\":"+std::to_string(w.sample)+",\"world\":"+hex(world)+
    ",\"owner\":"+std::to_string(owner)+",\"actor\":"+hex(actor)+",\"actor_type\":"+jsonString(ownerType.qualifiedName)+
    ",\"spawned_entity\":"+std::to_string(w.spawned)+",\"query_state\":"+hex(state)+
    ",\"age_ms\":"+std::to_string(now-w.milliseconds)+",\"record\":"+hex(e.record)+",\"component_count\":"+std::to_string(count)+
    ",\"components\":["+components.str()+"],\"live_query_owner_match\":true,\"read_only\":true,\"stable\":true");
  {std::lock_guard<std::mutex> lock(bowMutex);if(spawnWitness.sample==w.sample)spawnWitness={};}
 }catch(const std::exception& ex){
  logFile->event("bow_spawn_ecs_attempt","\"sample\":"+std::to_string(w.sample)+",\"attempt\":"+std::to_string(w.attempts+1)+",\"reason\":"+jsonString(ex.what())+",\"ready\":false,\"read_only\":true");
 }
}
inline void capture(uintptr_t sp,void* context,void* query,uint32_t bytes){
    const auto call=calls.fetch_add(1);
    const bool periodic=!(call%16)&&samples.load()<128;
    const bool selectionWatch=(controllerUiFixture::wheelRequested||liveEquipmentWatch)&&selectionEvents.load()<256;
    const bool actionbarWatch=controllerUiFixture::routerEquipRequested||liveEquipmentWatch;
    // The identity that authorises tracked shots is refreshed for the whole session. Only the
    // log below is limited to 2048 records (combined run game004438: with the whole block
    // limited, the bow lost its shots once the log budget was spent).
    const bool bowWatch=liveBowWatch;
    if(!periodic&&!selectionWatch&&!actionbarWatch&&!bowWatch)return;
    try{
        // 1.0 (query census, Mock game-20261016-081000): 0x220 bytes; PlayerInput 7, Actor 15, RangedWeapon 39, SlotSelection 51,
        // the item registry 60 (June 0x138 bytes; 6, 14, 22, 26, 31).
        if(bytes!=0x220||reinterpret_cast<uintptr_t>(query)!=sp+0x60)
            throw std::runtime_error("ranged query ABI changed");
        const auto slots=read<std::array<uintptr_t,68>>(reinterpret_cast<uintptr_t>(query));
        if(slots[0]!=reinterpret_cast<uintptr_t>(context)||!slots[7]||!slots[15]||!slots[39]||!slots[51]||!slots[60])
            throw std::runtime_error("ranged query slots missing");
        const auto state=read<uintptr_t>(slots[0]);const auto index=read<uint32_t>(slots[0]+8);
        if(!state||!index||index>0x1000000)throw std::runtime_error("ranged query bounds");
        const auto table=read<uintptr_t>(state+0x828);if(!table)throw std::runtime_error("ranged entity table absent");
        const auto entity=read<uint32_t>(table+uintptr_t(index-1)*16);
        rootCamera::Sample local;{std::lock_guard<std::mutex> lock(rootCamera::mutex);local=rootCamera::latest;}
        if(!local.number||GetTickCount64()-local.milliseconds>1000||entity!=local.entity)return;
        const auto input=slots[7],actor=slots[15],weapon=slots[39],selection=slots[51];
        const auto registry=slots[60];
        if(liveBowWatch)inspectSpawn(state,entity,actor);
        if(selectionWatch)logSelectionIfChanged(entity,selection,registry);
        if(actionbarWatch)publishActionbar(entity,state,selection,registry);
        if(bowWatch){
            // Ends at the proven target flag (+0xc4 in 1.0); never write native charge,
            // ammunition, actor state or shot events. Both Worlds are labeled.
            const auto raw=read<std::array<uint8_t,rangedWeaponBytes>>(weapon);
            const auto camera=read<std::array<uint8_t,44>>(input+0x28);
            const auto selected=read<std::array<uint8_t,20>>(selection+selectionActionItem);
            const auto digital=read<uint64_t>(input+game::playerInputDigital);
            if(raw!=read<std::array<uint8_t,rangedWeaponBytes>>(weapon)||camera!=read<std::array<uint8_t,44>>(input+0x28)||
               selected!=read<std::array<uint8_t,20>>(selection+selectionActionItem)||digital!=read<uint64_t>(input+game::playerInputDigital))return;
            const auto world=read<uintptr_t>(state+0x30);uint32_t item=0;std::memcpy(&item,raw.data()+rangedItem,4);
            // The actor's running action (BaseActor +0, as motion_event_selection_observer.hpp reads it; 2026-10-24: does
            // the bow's strong shot 1963645007 begin with the equipment bow, not on the action bar?).
            const auto sequence=read<uint32_t>(actor);const auto sequenceActive=read<uint8_t>(actor+0x1b1)&1;
            // the bow's shot state (3) and a refused special ability for the strong release (controller_ui_fixture.hpp,
            // BowDraw::strongOutcome): RangedWeapon+0x7c usageFeedbackMask (keen::UsageFeedbackTypes of 1.0) SpecialAbility 10
            // (its gauge not full - user 2026-10-24: "the strong shot needs the gauge charged") or SkillUnlock 11 (the skill
            // not learned: Mock game-20261024-213418)
            {const auto ms=GetTickCount64();uint16_t usage=0;std::memcpy(&usage,raw.data()+rangedUsageMask,2);
             if(raw[0]==3)controllerUiFixture::bowNativeFireAt=ms;if(usage&0xc00)controllerUiFixture::bowNativeRefusedAt=ms;}
            bool changed=true;
            {std::lock_guard<std::mutex> lock(bowMutex);BowNativeIdentity* id=nullptr;
             for(auto& v:bowIdentities)if(v.world==world){id=&v;break;}
             if(!id)for(auto& v:bowIdentities)if(!v.world){id=&v;break;}
             if(id&&world)*id={world,input,weapon,actor,entity,GetTickCount64(),raw};
             BowSeen* seen=nullptr;
             for(auto& v:bowSeen)if(v.world==world){seen=&v;break;}
             if(!seen)for(auto& v:bowSeen)if(!v.world){seen=&v;break;}
             if(seen){changed=seen->world!=world||seen->item!=item||seen->sequence!=sequence||seen->digital!=digital||seen->state!=raw[0]||seen->queued!=raw[0x39];
              *seen={world,item,sequence,digital,raw[0],raw[0x39]};}}
            const auto n=(bowSamples.load()<2048&&(changed||!(call%8)))?bowSamples.fetch_add(1):2048;
            if(n<2048)logFile->event("input_bow_sample","\"sample\":"+std::to_string(n+1)+",\"frame\":"+std::to_string(liveFrame.frame)+
                ",\"entity\":"+std::to_string(entity)+",\"world\":"+hex(read<uintptr_t>(state+0x30))+
                ",\"query_state\":"+hex(state)+",\"input\":"+hex(input)+",\"weapon\":"+hex(weapon)+",\"actor\":"+hex(actor)+",\"sequence\":"+std::to_string(sequence)+",\"sequence_active\":"+std::to_string(sequenceActive)+",\"digital\":"+std::to_string(digital)+",\"weapon_bytes\":"+jsonString(categoryBytes(raw))+
                ",\"camera_bytes\":"+jsonString(categoryBytes(camera))+",\"selection_bytes\":"+jsonString(categoryBytes(selected))+
                ",\"read_only\":true,\"source_unchanged\":true");
        }
        if(!periodic)return;
        catalog(registry,entity);
        if(actionSequenceObserver::requested){
            const auto id=read<uint32_t>(selection+selectionActionId);auto reader=readItemBytes;
            const auto found=game::lookupItem(reader,registry,id);game::ItemMetadata m;
            const bool melee=game::readItemMetadata(reader,found,id,m)&&m.category==2&&m.slot==1;
            std::lock_guard<std::mutex> lock(meleeMutex);meleeSelection={entity,id,GetTickCount64(),melee||(!id&&found.status==game::ItemLookupStatus::NoItem)};
        }
        const auto current=read<uint8_t>(weapon),turn=read<uint8_t>(weapon+rangedTurn),fireQueued=read<uint8_t>(weapon+0x39);
        if(current>4||turn>1||fireQueued>1)throw std::runtime_error("ranged state enum/bool changed");
        const auto p=read<WorldPositionQ32>(input+0x28);const auto q=read<Quat>(input+0x40);normalized(q);
        const auto raw=read<uint64_t>(actor+0xbf8),add=read<uint64_t>(actor+0xbd0),remove=read<uint64_t>(actor+0xbd8);
        const auto sequence=read<uint8_t>(actor+0x1b1);const auto effective=sequence&1?(raw|add)&~remove:raw;
        std::ostringstream out;out<<std::setprecision(10)<<"\"sample\":"<<samples.fetch_add(1)+1
            <<",\"entity\":"<<entity<<",\"local_entity\":"<<local.entity<<",\"caller\":"<<hex(callerRva)
            <<",\"thread\":"<<GetCurrentThreadId()<<",\"query_state\":"<<hex(state)
            <<",\"weapon_state\":"<<unsigned(current)<<",\"fire_queued\":"<<(fireQueued?"true":"false")
            <<",\"turn_towards_target\":"<<(turn?"true":"false")
            <<",\"original_trigger\":"<<unsigned(read<uint8_t>(weapon+rangedOriginalTrigger))<<",\"current_trigger\":"<<unsigned(read<uint8_t>(weapon+rangedCurrentTrigger))
            <<",\"missing_resources\":"<<read<uint16_t>(weapon+rangedUsageMask)<<",\"actor_state_effective\":"<<effective
            <<",\"input_mode\":"<<unsigned(read<uint8_t>(input+game::playerInputMode))
            <<",\"camera_q32\":["<<p.x<<','<<p.y<<','<<p.z<<"],\"camera_orientation\":["<<q.x<<','<<q.y<<','<<q.z<<','<<q.w<<']';
        auto item=[&](const char* name,uintptr_t value){
            const auto id=read<uint32_t>(value+8);auto reader=readItemBytes;
            const auto lookup=game::lookupItem(reader,registry,id);game::ItemMetadata metadata;
            const bool valid=game::readItemMetadata(reader,lookup,id,metadata);
            out<<",\""<<name<<"\":{\"slot_owner\":"<<read<uint32_t>(value)<<",\"slot\":"<<unsigned(read<uint8_t>(value+4))
                <<",\"item_id\":"<<id<<",\"pide\":"<<read<uint32_t>(value+0xc)
                <<",\"state_mask\":"<<read<uint16_t>(value+0x10)<<",\"lookup\":"<<jsonString(game::itemLookupName(lookup.status))
                <<",\"metadata_valid\":"<<(valid?"true":"false")<<",\"metadata\":";
            if(valid)metadataJson(out,metadata);else out<<"null";out<<'}';
        };
        item("weapon_instance",weapon+rangedWeaponInstance);item("ammo_instance",weapon+rangedAmmoInstance);item("action_item",selection+selectionActionItem);
        item("sequence_item",actor+0x30);item("sequence_ammo",actor+0x44);
        out<<",\"read_only\":true";logFile->event("input_ranged_sample",out.str());
    }catch(const std::exception& e){if(failures.fetch_add(1)==0)logFile->event("input_ranged_failure","\"reason\":"+jsonString(e.what()));}
}
}
