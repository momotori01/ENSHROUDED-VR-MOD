// Bounded read-only model-handle observation; no visibility changes.
#include "hand_pose_metadata.hpp"
namespace equipmentObserver {
inline bool requested=false,enabled=false;
inline bool continuous=false;
struct Snapshot {
    uint64_t number=0,milliseconds=0;uint32_t entity=0;uintptr_t scene=0;
    std::array<uint32_t,24> handles{},masks{};
    std::array<uint16_t,24> models{};
    std::array<uint8_t,24> ready{};
};
inline Snapshot latest;
inline std::mutex snapshotMutex;
inline std::atomic<bool> accepting{false};
inline std::atomic<unsigned> callbacks{0},samples{0},failures{0};
using Update=void(__fastcall*)(void*);
inline Update original=nullptr;
inline void* target=nullptr;
inline constexpr const char* patternText="40 55 53 56 57 41 54 41 55 41 56 41 57 48 8D AC 24 18 FB FF FF";   // 1.0 (June: F8 FA FF FF)
inline const std::array<const char*,24> names{{"head","hair","eyebrow","beard","neck","collar","chest","waist","crotch","upper_arms","lower_arms","hands","upper_legs","lower_legs","feet","head_armour","upper_armour","arm_armour","lower_armour","foot_armour","ring0","ring1","shield","main_hand"}};
// The same update runs for every character the game dresses this way, not for the local player alone: other players'
// characters online, and whoever else has such parts. For showing other players' VR bodies their part models and the
// pose those stand on are found here, the way the own ones are. EVR_EQUIPMENT_CENSUS=1 in the game's environment:
// the first samples of every other entity go on record (read-only; nothing is done with them).
inline std::mutex otherMutex;inline std::map<uint32_t,unsigned> otherSeen;
// For the other players' VR bodies (online_sync.hpp): every other character's part models as of its last update.
struct Other {std::array<uint32_t,24> handles{};uint64_t milliseconds=0;uintptr_t scene=0;};
inline bool otherWorldsPass=false;
inline std::atomic<bool> trackOthers{false};inline std::map<uint32_t,Other> others;   // under otherMutex
inline void other(uint32_t entity,uint32_t local,const std::array<uintptr_t,20>& values) noexcept {
    if(trackOthers.load())try{
        const auto equipment=values[0x58/8],owner=values[0x60/8];
        if(equipment&&owner){Other o;o.handles=read<std::array<uint32_t,24>>(equipment);o.scene=read<uintptr_t>(owner+layout::ownerScene);o.milliseconds=GetTickCount64();
            std::lock_guard<std::mutex> lock(otherMutex);if(others.count(entity)||others.size()<64)others[entity]=o;}
    }catch(...){}
    static const bool wanted=[]{char value[8]{};const auto n=GetEnvironmentVariableA("EVR_EQUIPMENT_CENSUS",value,sizeof value);return n&&n<sizeof value&&value[0]=='1';}();
    if(!wanted)return;
    try{
        {std::lock_guard<std::mutex> lock(otherMutex);if(!otherSeen.count(entity)&&otherSeen.size()>=48)return;auto& n=otherSeen[entity];if(n>=3)return;++n;}
        const auto equipment=values[0x58/8],owner=values[0x60/8];if(!equipment||!owner)return;
        const auto scene=read<uintptr_t>(owner+layout::ownerScene);const auto handles=read<std::array<uint32_t,24>>(equipment);
        const auto objects=read<uintptr_t>(scene+0x1b0),generations=read<uintptr_t>(scene+0x1c8);
        const auto objectBytes=read<uint64_t>(scene+0x1b8),generationCount=read<uint64_t>(scene+0x1d0);
        if(!objects||!generations||objectBytes%0x1b0)return;
        std::ostringstream out;out<<"\"entity\":"<<entity<<",\"local_entity\":"<<local<<",\"world_scene\":"<<(scene==provenWorldScene?"true":"false")
            <<",\"hide_mask\":"<<read<uint32_t>(equipment+0x300)<<",\"parts\":[";
        bool first=true,placed=false;std::array<int64_t,3> q32{};std::array<float,4> facing{};
        for(unsigned i=0;i<24;i++)if(const auto handle=handles[i]){
            const auto index=handle&0xffffff;if(index>=objectBytes/0x1b0||index>=generationCount||read<uint8_t>(generations+index)!=(handle>>24))continue;
            const auto object=objects+uintptr_t(index)*0x1b0,resource=read<uintptr_t>(object+layout::modelResource);
            if(!placed){placed=true;q32=read<std::array<int64_t,3>>(object+0x38);facing=read<std::array<float,4>>(object+0x50);}
            out<<(first?"":",")<<'['<<i<<','<<handle<<','<<read<uint32_t>(object+layout::modelPose)<<','<<read<uint32_t>(object+layout::modelMask)<<','<<(resource?read<uint16_t>(resource+layout::resourceModel):0)<<']';first=false;
        }
        out<<"],\"part_fields\":\"part,model_handle,pose_handle,mask,model\",\"first_part_q32\":["<<q32[0]<<','<<q32[1]<<','<<q32[2]<<"],\"first_part_orientation\":["<<facing[0]<<','<<facing[1]<<','<<facing[2]<<','<<facing[3]<<"],\"read_only\":true";
        logFile->event("equipment_other_entity",out.str());
    }catch(...){}
}
inline void capture(void* context){
    if(samples.load()>=20&&!continuous)return;
    const auto values=read<std::array<uintptr_t,20>>(reinterpret_cast<uintptr_t>(context));
    const auto task=values[0],state=read<uintptr_t>(task);const auto query=read<uint32_t>(task+8);
    if(!state||!query)throw std::runtime_error("equipment observation lacks task entity");
    const auto entity=read<uint32_t>(read<uintptr_t>(state+0x828)+uintptr_t(query-1)*16);
    rootCamera::Sample camera;try{camera=rootCamera::snapshot();}catch(...){if(otherWorldsPass)return;throw;}   // the mod without VR: the local player is not known yet
    if(entity!=camera.entity){other(entity,camera.entity,values);return;}
    // Native c0160/c46f0 unwrap the public RenderScene at +70. This is
    // the same owner wrapper whose +78 selects the original native View.
    const auto equipment=values[0x58/8],owner=values[0x60/8];
    if(!equipment||!owner)throw std::runtime_error("equipment lacks native Scene owner");
    const auto scene=read<uintptr_t>(owner+layout::ownerScene),ownerView=read<uintptr_t>(owner+layout::ownerView);
    if(scene!=provenWorldScene||ownerView!=provenWorldView){
        if(otherWorldsPass)return;   // the mod without VR: no world found yet, or the player is between two worlds
        throw std::runtime_error("equipment owner does not contain proven world Scene/View");}
    const auto bytes=read<std::array<uint8_t,788>>(equipment);
    const auto handles=read<std::array<uint32_t,24>>(equipment);
    const auto objects=read<uintptr_t>(scene+0x1b0),generations=read<uintptr_t>(scene+0x1c8);
    const auto objectBytes=read<uint64_t>(scene+0x1b8),generationCount=read<uint64_t>(scene+0x1d0);
    if(objectBytes%0x1b0)throw std::runtime_error("equipment model table byte size is not a multiple of stride");
    const auto objectCount=objectBytes/0x1b0;
    if(!objects||!generations)throw std::runtime_error("equipment model tables unavailable");
    Snapshot next;next.milliseconds=GetTickCount64();next.entity=entity;next.scene=scene;next.handles=handles;
    for(unsigned i=0;i<24;i++)if(handles[i]){
        const auto index=handles[i]&0xffffff;
        if(index>=objectCount||index>=generationCount||read<uint8_t>(generations+index)!=(handles[i]>>24))
            throw std::runtime_error("continuous equipment handle generation/range differs");
        const auto object=objects+uintptr_t(index)*0x1b0,resource=read<uintptr_t>(object+layout::modelResource);
        next.masks[i]=read<uint32_t>(object+layout::modelMask);
        if(resource){next.models[i]=read<uint16_t>(resource+layout::resourceModel);next.ready[i]=read<uint8_t>(resource+0x14);}
    }
    {std::lock_guard<std::mutex> lock(snapshotMutex);next.number=latest.number+1;latest=next;}
    if(samples.load()>=20)return;
    const unsigned n=samples.fetch_add(1);if(n>=20)return;
    std::ostringstream out;out<<"\"sample\":"<<n<<",\"entity\":"<<entity<<",\"scene\":"<<hex(scene)
        <<",\"owner\":"<<hex(owner)<<",\"owner_view\":"<<hex(ownerView)
        <<",\"object_count\":"<<objectCount<<",\"generation_count\":"<<generationCount
        <<",\"root_sample\":"<<camera.number<<",\"hide_mask\":"<<read<uint32_t>(equipment+0x300)
        <<",\"ui_hide_mask\":"<<read<uint32_t>(equipment+0x304)<<",\"models\":[";
    for(unsigned i=0;i<24;i++){
        if(i)out<<',';const auto handle=handles[i];const auto index=handle&0xffffff;
        out<<"{\"part\":"<<i<<",\"name\":"<<jsonString(names[i])<<",\"handle\":"<<handle;
        if(handle){
            if(index>=objectCount||index>=generationCount)throw std::runtime_error("equipment model index outside native tables");
            if(read<uint8_t>(generations+index)!=(handle>>24))throw std::runtime_error("equipment model generation mismatch");
            const auto object=objects+uintptr_t(index)*0x1b0;
            const auto payload=read<std::array<uint8_t,0x1b0>>(object);
            out<<",\"object\":"<<hex(object)<<",\"mask\":"<<read<uint32_t>(object+layout::modelMask);
            const auto modelResource=read<uintptr_t>(object+layout::modelResource);
            if(modelResource)out<<",\"shader_model_index\":"<<read<uint16_t>(modelResource+layout::resourceModel)
                <<",\"model_ready\":"<<std::to_string(read<uint8_t>(modelResource+0x14));
            if(n==0){
                const auto folder=calibrationDirectory/"equipment-models";std::filesystem::create_directories(folder);
                std::ofstream file(folder/(std::to_string(i)+"-"+names[i]+".bin"),std::ios::binary);
                file.write(reinterpret_cast<const char*>(payload.data()),payload.size());if(!file)throw std::runtime_error("model observation file failed");
                // 1.0 layout search (EVR_LAYOUT_DUMP=1): the model resource and its descriptor (resource+0x30 before 1.0).
                static const bool layoutDump=[]{char v[8]{};const auto k=GetEnvironmentVariableA("EVR_LAYOUT_DUMP",v,sizeof v);return k==1&&v[0]=='1';}();
                if(layoutDump&&modelResource)for(const auto& region:{std::pair<const char*,uintptr_t>{"-resource.bin",modelResource},{"-descriptor.bin",read<uintptr_t>(modelResource+layout::resourceDescriptor)}}){
                    std::array<uint8_t,0x200> bytes{};SIZE_T got=0;
                    if(region.second)ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(region.second),bytes.data(),bytes.size(),&got);
                    std::ofstream(folder/(std::to_string(i)+"-"+names[i]+region.first),std::ios::binary).write(reinterpret_cast<const char*>(bytes.data()),bytes.size());
                }
                if(i==11)try{handPoseMetadata::capture(scene,object);}catch(const std::exception& e){
                    logFile->event("hand_pose_metadata_failure","\"reason\":"+jsonString(e.what())+",\"read_only\":true");}
            }
        }
        out<<'}';
    }
    out<<']';logFile->event("local_equipment_observation",out.str());
    if(n==0){std::ofstream file(calibrationDirectory/"equipment-models"/"render-equipment.bin",std::ios::binary);file.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());}
}
__declspec(noinline) void __fastcall update(void* context){
    const auto caller=reinterpret_cast<uintptr_t>(_ReturnAddress());callbacks.fetch_add(1);
    original(context);
    {const ObserverLastError lastError;
     if(accepting.load()&&caller==baseAddress+0x38314b){
        try{capture(context);}catch(const std::exception& error){{std::lock_guard<std::mutex> lock(snapshotMutex);latest.milliseconds=0;}if(failures.fetch_add(1)<3)try{
            logFile->event("equipment_observer_read_failure","\"reason\":"+jsonString(error.what()));}catch(...){} }
     }}
    callbacks.fetch_sub(1);
}
inline void install(){
    const char* text=patternText;
    const auto bytes=parsePattern(text);target=reinterpret_cast<void*>(baseAddress+0x383830);
    const auto call=baseAddress+0x383146;
    if(scan(static_cast<const uint8_t*>(target),bytes.size(),bytes)!=std::vector<size_t>{0}||read<uint8_t>(call)!=0xe8||
       uintptr_t(int64_t(call+5)+read<int32_t>(call+1))!=reinterpret_cast<uintptr_t>(target))throw std::runtime_error("equipment observer signature mismatch");
    if(MH_CreateHook(target,reinterpret_cast<void*>(&update),reinterpret_cast<void**>(&original))!=MH_OK)throw std::runtime_error("equipment observer hook creation failed");
    accepting=true;if(MH_EnableHook(target)!=MH_OK){accepting=false;throw std::runtime_error("equipment observer enable failed");}enabled=true;
    logFile->event("equipment_observer_enabled","\"rva\":\"0x383830\",\"caller_rva\":\"0x38314b\",\"max_samples\":20,\"continuous\":"+std::string(continuous?"true":"false")+",\"read_only\":true,\"pattern\":"+jsonString(text));
}
inline void disable(){
    accepting=false;if(!enabled)return;
    if(MH_DisableHook(target)!=MH_OK)throw std::runtime_error("equipment observer disable failed");enabled=false;
    const auto deadline=GetTickCount64()+500;while(callbacks.load()&&GetTickCount64()<deadline)std::this_thread::sleep_for(std::chrono::milliseconds(1));
    const auto bytes=parsePattern(patternText);
    const bool restored=scan(static_cast<const uint8_t*>(target),bytes.size(),bytes)==std::vector<size_t>{0};
    logFile->event("equipment_observer_removed","\"original_bytes_restored\":"+std::string(restored?"true":"false")+
        ",\"remaining_callbacks\":"+std::to_string(callbacks.load())+",\"read_failures\":"+std::to_string(failures.load())+",\"samples\":"+std::to_string(samples.load()));
    if(!restored)throw std::runtime_error("equipment observer original bytes not restored");
}
}
