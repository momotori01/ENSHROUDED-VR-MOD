// Another player's held consumable (2026-10-23, user: "can the others see the item one holds, online too?"): the model of
// the item that player holds in the right hand (motion_consumable_visual.hpp on that player's PC, sent as
// sync::heldItemAction), on the right hand of the character the mod drives for that player (remote_body.hpp), where that
// player said it lies in the hand's socket (the body's hand[1], as the hand items of online 2.1). A Scene-owned model made
// by the item's icon model GUID as the own one is, seen by everybody (the game's mask kept), SDF 0. It goes when that
// player lets it go, when nothing was heard about it for a second, or when the character is not driven.
// The players without the mod, and mods older than 0.9.13, see nothing in the hand (the game shows none).
namespace remoteConsumables {
struct Heard {uint32_t item=0;Pose inHand=identityPose();float scale=1;uint64_t at=0;};
struct Copy {uint32_t handle=0,item=0,pose=0;uintptr_t resource=0;bool loaded=false,posed=false;uint64_t createdMs=0,placedSerial=0,updates=0;};
inline std::map<uint64_t,Heard> heard;      // per player
inline std::map<uint32_t,Copy> copies;      // per driven character
inline uintptr_t scene=0;inline unsigned logs=0;inline bool failed=false;
inline void hear(uint64_t from,const sync::Action& a){
    auto& h=heard[from];const bool changed=h.item!=a.item;h.item=a.item;h.inHand=a.inHand;h.scale=a.scale;h.at=GetTickCount64();
    if(changed&&logs++<200)logFile->event("remote_consumable_heard","\"player\":\""+steamLink::tail(from)+"\",\"item\":"+std::to_string(a.item)+",\"scale\":"+std::to_string(a.scale));
}
inline void remove(uint32_t entity,Copy& c,const char* why){
    if(!c.handle)return;
    const auto object=motionVisual::currentObject(c.handle);const auto id=c.handle;c.handle=0;
    if(!object||read<uintptr_t>(object+layout::modelResource)!=c.resource)throw std::runtime_error("remote consumable copy changed");
    const auto refs=read<uint32_t>(c.resource+16);std::array<uintptr_t,2> ids{reinterpret_cast<uintptr_t>(&id),1};
    reinterpret_cast<motionVisual::Remove>(baseAddress+0x10e000)(reinterpret_cast<void*>(scene),ids.data());motionConsumableVisual::unpose(scene,c.pose,c.posed);
    const bool invalid=read<uint8_t>(read<uintptr_t>(scene+0x1c8)+(id&0xffffff))!=uint8_t(id>>24);
    const bool released=refs==1||(refs>1&&read<uint32_t>(c.resource+16)==refs-1);
    if(logs++<200)logFile->event("remote_consumable_removed","\"entity\":"+std::to_string(entity)+",\"handle\":"+std::to_string(id)+",\"item\":"+std::to_string(c.item)+",\"reason\":\""+why+
        "\",\"updates\":"+std::to_string(c.updates)+",\"generation_invalidated\":"+(invalid?"true":"false")+",\"reference_released\":"+(released?"true":"false"));
    if(!invalid||!released)throw std::runtime_error("remote consumable removal failed");
}
inline void fail(const std::exception& e) noexcept {
    failed=true;try{logFile->event("remote_consumable_failure","\"reason\":"+jsonString(e.what()));}catch(...){}
    for(auto& [entity,c]:copies)try{remove(entity,c,"failure");}catch(...){}
    copies.clear();
}
// One driven character this frame: its body as drawn and the root its parts are placed from.
inline void place(uint64_t player,uint32_t entity,const sync::Body& body,const WorldPositionQ32& root,uint64_t renderSerial) noexcept {
    if(failed||!motionConsumableVisual::requested)return;
    try{
        if(scene&&scene!=motionVisual::scene)throw std::runtime_error("remote consumable Scene changed");
        const auto now=GetTickCount64();const auto it=heard.find(player);
        const bool want=it!=heard.end()&&it->second.item&&now>=it->second.at&&now-it->second.at<=1000&&body.ik;
        auto found=copies.find(entity);
        if(found!=copies.end()&&(!want||found->second.item!=it->second.item)){remove(entity,found->second,want?"another_item":"let_go");copies.erase(found);found=copies.end();}
        if(!want)return;
        const auto& h=it->second;
        const auto world=sync::fromHand(body.hand[1],h.inHand);if(!finite(world.position)||length(world.position)>4.f)return;
        const WorldPositionQ32 q{root.x+int64_t(double(world.position.x)*4294967296.),root.y+int64_t(double(world.position.y)*4294967296.),root.z+int64_t(double(world.position.z)*4294967296.)};
        const auto o=normalized(world.orientation);
        Copy& c=copies[entity];
        const bool loaded=c.handle&&read<uint8_t>(c.resource+0x14)==1&&motionConsumableVisual::pose(scene,c.handle,c.resource,c.pose,c.posed);
        const float s=loaded?h.scale:.001f;const Vec3 scale{s,s,s};
        std::array<uint8_t,56> transform{};std::memcpy(transform.data(),&q,24);std::memcpy(transform.data()+24,&o,16);std::memcpy(transform.data()+40,&scale,12);
        if(!c.handle){
            const auto g=motionConsumableVisual::iconModel(h.item);if(g==std::array<uint32_t,4>{}){copies.erase(entity);return;}
            motionVisual::validate();scene=motionVisual::scene;
            c.handle=reinterpret_cast<motionVisual::Create>(baseAddress+0xfb4e0)(reinterpret_cast<void*>(scene),g.data(),transform.data());
            if(!c.handle){copies.erase(entity);throw std::runtime_error("remote consumable creation failed");}
            c.resource=read<uintptr_t>(motionVisual::object(c.handle)+layout::modelResource);c.item=h.item;c.createdMs=now;c.loaded=false;c.updates=0;
            if(!c.resource||read<std::array<uint32_t,4>>(c.resource+0x18)!=g)throw std::runtime_error("remote consumable resource differs");
            if(logs++<200)logFile->event("remote_consumable_created","\"entity\":"+std::to_string(entity)+",\"handle\":"+std::to_string(c.handle)+",\"item\":"+std::to_string(c.item)+
                ",\"icon_model\":"+motionConsumableVisual::guidText(g)+",\"resource_state\":"+std::to_string(read<uint8_t>(c.resource+0x14))+",\"mask\":"+std::to_string(read<uint32_t>(motionVisual::object(c.handle)+layout::modelMask)));
        }else{
            std::array<uintptr_t,2> ids{reinterpret_cast<uintptr_t>(&c.handle),1},poses{reinterpret_cast<uintptr_t>(transform.data()),1};
            motionVisual::change(reinterpret_cast<void*>(scene),ids.data(),poses.data());
        }
        const auto object=motionVisual::object(c.handle);
        if(read<uintptr_t>(object+layout::modelResource)!=c.resource)throw std::runtime_error("remote consumable resource changed");
        if(const auto sdfId=read<uint32_t>(object+layout::modelSdf)){
            const auto sdf=read<uintptr_t>(scene+layout::sceneSdf);const auto count=sdf?read<uint64_t>(sdf+0x78):0;if(!count)throw std::runtime_error("remote consumable SDF count");
            using RemoveSdf=void(__fastcall*)(void*,uint32_t);reinterpret_cast<RemoveSdf>(baseAddress+0x15a640)(reinterpret_cast<void*>(sdf),sdfId);
            *reinterpret_cast<uint32_t*>(motionVisual::object(c.handle)+layout::modelSdf)=0;
            if(read<uint64_t>(sdf+0x78)!=count-1)throw std::runtime_error("remote consumable SDF removal failed");
        }
        if(loaded&&!c.loaded&&logs++<200)logFile->event("remote_consumable_loaded","\"entity\":"+std::to_string(entity)+",\"handle\":"+std::to_string(c.handle)+",\"after_ms\":"+std::to_string(now-c.createdMs)+
            ",\"mask\":"+std::to_string(read<uint32_t>(object+layout::modelMask)));
        c.loaded=loaded;c.placedSerial=renderSerial;++c.updates;
    }catch(const std::exception& e){fail(e);}
}
// The copies of characters not driven this frame go.
inline void retireUnplaced(uint64_t renderSerial) noexcept {
    if(failed)return;
    try{for(auto it=copies.begin();it!=copies.end();){if(it->second.placedSerial!=renderSerial){remove(it->first,it->second,"not_driven");it=copies.erase(it);}else ++it;}}
    catch(const std::exception& e){fail(e);}
}
inline void dispose(uintptr_t s) noexcept {
    if(s!=scene)return;
    for(auto& [entity,c]:copies)try{remove(entity,c,"scene_disposal");}catch(...){}
    copies.clear();scene=0;
}
}
