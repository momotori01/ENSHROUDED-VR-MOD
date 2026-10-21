// Native impact_simulation query at completed-entity boundary. Ownership fields
// are the exact inputs read by getParent/fe78d0 and getRoot/fe7990.
namespace impactHierarchyObserver {
inline bool requested=false;inline std::atomic<unsigned> samples{0},calls{0},failures{0};
inline thread_local bool actorImpact=false;   // 1.0: the advance hook names this query's slots for the actor's impacts (query census)
inline std::mutex quotaMutex;inline std::map<std::pair<uint32_t,bool>,unsigned> quotas;
// 1.0 (Hotfix 1.0.0.1): the impact simulation's query is 0x498 bytes (June 0x4c0), its first advance at 0x3ef1d0 and the
// loop's at 0x3ef3e7 (return 0x3ef3ec; June 0x36b628) in 0x3ef190. The slots are placed by the query census first.
constexpr uintptr_t callerRva=0x3efd7c;
// The query census (Mock game-20261016-112850, three impacts of the world): DespawnLink 3, ImpactHierarchy 4 (16 bytes),
// Impact 8 (24 bytes; June 20 read). The optional item config (44 bytes) and sequence event (24) were absent there: they
// are taken as 5 and 6 (June: hierarchy 9, item 10, sequence 13, impact 17) until a swing of the player's shows them -
// `impact_hierarchy_slots` puts the filled slots 1..13 of the actor's impacts on record.
constexpr size_t queryBytes=0x498,queryAt=0x30,slotCount=queryBytes/8,slotHierarchy=4,slotItem=5,slotSequence=6,slotImpact=8;
inline void capture(uintptr_t sp,void* context,void* query,uint32_t bytes){
 // The component binding learned below is needed for every new World, also after the sample log is full.
 ++calls;if(!actionSequenceObserver::recording.load())return;const bool record=samples.load()<1024;
 try{
  if(bytes!=queryBytes||reinterpret_cast<uintptr_t>(query)!=sp+queryAt)throw std::runtime_error("impact hierarchy query ABI differs");
  const auto slots=read<std::array<uintptr_t,slotCount>>(reinterpret_cast<uintptr_t>(query));
  if(slots[0]!=reinterpret_cast<uintptr_t>(context)||!slots[slotImpact])throw std::runtime_error("impact hierarchy query identity missing");
  const auto index=read<uint32_t>(slots[0]+8);if(!index||index>0x1000000)throw std::runtime_error("impact entity index bounds");
  const auto state=read<uintptr_t>(slots[0]),entityTable=read<uintptr_t>(state+0x828);const auto entity=read<uint32_t>(entityTable+uintptr_t(index-1)*16);
  const auto hierarchy=slots[slotHierarchy]?read<std::array<uint8_t,16>>(slots[slotHierarchy]):std::array<uint8_t,16>{};uint32_t root=entity,parent=0;
  if(slots[slotHierarchy]){std::memcpy(&parent,hierarchy.data(),4);std::memcpy(&root,hierarchy.data()+4,4);}
  const auto sequence=slots[slotSequence]?read<std::array<uint8_t,24>>(slots[slotSequence]):std::array<uint8_t,24>{};uint32_t sequenceId=0;std::memcpy(&sequenceId,sequence.data(),4);
  actionSequenceObserver::Status actor;{std::lock_guard<std::mutex> lock(actionSequenceObserver::mutex);actor=actionSequenceObserver::latest;}
  if(!actor.entity||!(root==actor.entity||(actor.active&&sequenceId&&sequenceId==actor.sequence)))return;
  actorImpact=true;
  {static std::atomic<unsigned> told{0};if(told.fetch_add(1)<8){std::string filled;for(size_t k=1;k<=13&&k<slots.size();++k)if(slots[k]&&slots[k]>0x10000)filled+=(filled.empty()?"":",")+std::to_string(k);
     logFile->event("impact_hierarchy_slots","\"entity\":"+std::to_string(entity)+",\"root\":"+std::to_string(root)+",\"sequence\":"+std::to_string(sequenceId)+",\"filled\":["+filled+"]");}}
  nativeImpactComponents::learn(state,entity,slots[slotHierarchy],slots[slotItem]);
  if(!record)return;
  {std::lock_guard<std::mutex> guard(quotaMutex);if(quotas[{entity,actor.active}]++>=(actor.active?128u:4u))return;}
  const auto impact=read<std::array<uint8_t,24>>(slots[slotImpact]);const auto item=slots[slotItem]?read<std::array<uint8_t,44>>(slots[slotItem]):std::array<uint8_t,44>{};
  const bool stable=slots==read<std::array<uintptr_t,slotCount>>(reinterpret_cast<uintptr_t>(query))&&impact==read<std::array<uint8_t,24>>(slots[slotImpact])&&(!slots[slotHierarchy]||hierarchy==read<std::array<uint8_t,16>>(slots[slotHierarchy]))&&(!slots[slotItem]||item==read<std::array<uint8_t,44>>(slots[slotItem]))&&(!slots[slotSequence]||sequence==read<std::array<uint8_t,24>>(slots[slotSequence]));
  std::ostringstream o;o<<"\"sample\":"<<++samples<<",\"entity\":"<<entity<<",\"root_entity\":"<<root<<",\"parent_entity\":"<<parent<<",\"sequence\":"<<sequenceId
   <<",\"hierarchy_present\":"<<(slots[slotHierarchy]?"true":"false")<<",\"hierarchy_bytes\":"<<jsonString(sequenceColliderObserver::bytesHex(hierarchy))
   <<",\"sequence_event_bytes\":"<<jsonString(sequenceColliderObserver::bytesHex(sequence))<<",\"impact_bytes\":"<<jsonString(sequenceColliderObserver::bytesHex(impact))
   <<",\"item_config_bytes\":"<<jsonString(sequenceColliderObserver::bytesHex(item))<<",\"item_config_present\":"<<(slots[slotItem]?"true":"false")
   <<",\"stable_read\":"<<(stable?"true":"false")<<",\"latest_actor_entity\":"<<actor.entity<<",\"latest_actor_sequence\":"<<actor.sequence<<",\"latest_actor_item\":"<<actor.usedItem
   <<",\"latest_actor_active\":"<<(actor.active?"true":"false")<<",\"latest_actor_ms\":"<<actor.milliseconds<<",\"milliseconds\":"<<GetTickCount64()<<",\"thread\":"<<GetCurrentThreadId()<<",\"read_only\":true";
  logFile->event("impact_hierarchy_sample",o.str());
 }catch(const std::exception& e){if(failures.fetch_add(1)<3)logFile->event("impact_hierarchy_failure","\"reason\":"+jsonString(e.what()));}
}
inline void removed(bool restored,unsigned callbacks){if(requested)logFile->event("impact_hierarchy_removed","\"restored\":"+std::string(restored?"true":"false")+",\"callbacks\":"+std::to_string(callbacks)+",\"samples\":"+std::to_string(samples.load())+",\"calls\":"+std::to_string(calls.load())+",\"failures\":"+std::to_string(failures.load()));}
}
