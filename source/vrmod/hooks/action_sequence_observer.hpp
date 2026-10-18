// Read-only copies at the proved local BaseActor query boundary. No retained
// ECS pointers, native calls, action writes, or interpretation as cooldown.
namespace actionSequenceObserver {
inline bool requested=false;
inline std::mutex mutex;
inline unsigned samples=0,unstable=0,failures=0;
inline std::atomic<bool> recording{true};
struct Status {uintptr_t world=0;uint32_t entity=0,usedItem=0,sequence=0;bool active=false,alive=false,ready=false,canBlock=false,blocking=false,canPrimary=false;uint64_t activeSamples=0,milliseconds=0,digital=0;uint16_t trigger=0;
 int64_t triggerTime=0,playedTime=0,currentTime=0,length=0;};
inline Status latest;
inline std::map<std::pair<uintptr_t,uint32_t>,Status> qualified;
inline Status forWorld(uintptr_t world,uint32_t entity){std::lock_guard<std::mutex> lock(mutex);const auto i=qualified.find({world,entity});return i==qualified.end()?Status{}:i->second;}
inline void capture(uintptr_t actor,uint32_t entity,uint32_t local,uint32_t tick,uintptr_t input,uintptr_t actorInput,uintptr_t world,const std::array<uintptr_t,46>& querySlots){
    if(!requested)return;
    std::lock_guard<std::mutex> lock(mutex);
    try{
        if(!entity||entity!=local)throw std::runtime_error("action snapshot must be local");
        const auto first=read<std::array<uint8_t,0xc20>>(actor);
        const auto second=read<std::array<uint8_t,0xc20>>(actor);
        if(first!=second){++unstable;return;}
        latest.world=world;latest.entity=entity;latest.active=(first[0x1b1]&1)!=0;latest.milliseconds=GetTickCount64();if(latest.active)++latest.activeSamples;
        // Offsets/widths checked against local reflection.json for build23966345.
        auto get=[&](auto& target,size_t at){std::memcpy(&target,first.data()+at,sizeof(target));};
        get(latest.trigger,0x18);get(latest.sequence,0);get(latest.triggerTime,8);get(latest.usedItem,0x38);
        get(latest.playedTime,0x178);get(latest.currentTime,0x180);get(latest.length,0x188);
        uint64_t state=0,add=0,remove=0;get(state,0xbf8);get(add,0xbd0);get(remove,0xbd8);
        if(latest.active)state=(state|add)&~remove;
        // Native 1dd570 bit6: active sequences intersect raw and sequence ability.
        // Verified offline with 15788 original instruction calls; read-only.
        uint32_t rawAbility=0,sequenceAbility=0;get(rawAbility,0xc00);get(sequenceAbility,0xbc8);
        latest.canPrimary=((latest.active?sequenceAbility:rawAbility)&1u)!=0;
        latest.canBlock=((latest.active?(rawAbility&sequenceAbility):rawAbility)&(1u<<6))!=0;
        latest.blocking=(state&(1ull<<2))!=0;latest.digital=read<uint64_t>(input+game::playerInputDigital);   // 1.0: +0x3b0 (+0x320)
        latest.alive=(state&(1ull<<7))==0;latest.ready=(state&(1ull<<50))!=0&&read<uint8_t>(input+game::playerInputReady)==1;   // 1.0: +0x5b8 (+0x520)
        // A World that has not been sampled for ten seconds is gone; its place goes to the current one.
        if(latest.world&&qualified.size()>=8&&!qualified.count({latest.world,entity}))
            for(auto i=qualified.begin();i!=qualified.end();)i=latest.milliseconds-i->second.milliseconds>10000?qualified.erase(i):std::next(i);
        if(latest.world&&(qualified.size()<8||qualified.count({latest.world,entity})))qualified[{latest.world,entity}]=latest;
        // Current value-only status remains live after the bounded full-record
        // log reaches capacity. Late consumers must never see a frozen sample.
        if(samples>=4096||!recording.load())return;
        // Retain the complete reflected record so an independent offline reader
        // can verify widths, padding and later-discovered fields.
        static constexpr char digits[]="0123456789abcdef";
        std::string bytes;bytes.reserve(first.size()*2);
        for(auto b:first){bytes+=digits[b>>4];bytes+=digits[b&15];}
        const auto camera=read<WorldPositionQ32>(input+0x28);const auto q=read<Quat>(input+0x40);normalized(q);
        const auto look=read<Vec3>(actorInput+game::actorInputLook);if(!finite(look))throw std::runtime_error("action actor look invalid");
        // Native 2873a0 selects camera, movement, or actor-body look. Record
        // operands at the same local query boundary; never overwrite body look
        // merely to make it equal to the HMD camera.
        // 1.0: CurrentTransform (1) and the look mode (7) keep their slots in the 46-slot query; the old slot 15 (+0x3d, the
        // look after the move) is not identified yet (0 = not read).
        constexpr size_t postLookSlot=0;
        if(!querySlots[1])throw std::runtime_error("native look query operands absent");
        const auto body=read<std::array<uint8_t,40>>(querySlots[1]);
        const uint8_t mode=0;   // 1.0: slot 7 is ServerFishing, the look mode is not identified yet
        const auto postLook=postLookSlot&&querySlots[postLookSlot]?read<uint8_t>(querySlots[postLookSlot]+0x3d):uint8_t(0);
        const auto settings=read<uint8_t>(input+game::playerInputSettings);
        const auto move=read<Vec3>(actorInput+0x198);
        if(body!=read<std::array<uint8_t,40>>(querySlots[1])||!finite(move))throw std::runtime_error("native look operands changed");
        Quat bodyQ;std::memcpy(&bodyQ,body.data()+24,16);normalized(bodyQ);
        std::string bodyBytes;for(auto b:body){bodyBytes+=digits[b>>4];bodyBytes+=digits[b&15];}
        std::ostringstream geometry;geometry<<std::setprecision(10)<<",\"camera_q32\":["<<camera.x<<','<<camera.y<<','<<camera.z<<"],\"camera_orientation\":["<<q.x<<','<<q.y<<','<<q.z<<','<<q.w<<"],\"world_look\":["<<look.x<<','<<look.y<<','<<look.z<<']';
        geometry<<",\"native_look_context\":{\"body_transform_bytes\":\""<<bodyBytes<<"\",\"mode\":"<<unsigned(mode)
            <<",\"settings\":"<<unsigned(settings)<<",\"post_look\":"<<unsigned(postLook)
            <<",\"world_move\":["<<move.x<<','<<move.y<<','<<move.z<<"],\"query_slots\":[1,7,15],\"read_only\":true}";
        logFile->event("action_sequence_snapshot","\"sample\":"+std::to_string(++samples)+
            ",\"world\":"+hex(world)+",\"entity\":"+std::to_string(entity)+",\"local_entity\":"+std::to_string(local)+
            ",\"input_digital\":"+std::to_string(read<uint64_t>(input+game::playerInputDigital))+",\"ready_for_gameplay\":"+std::to_string(read<uint8_t>(input+game::playerInputReady))+
            ",\"tick\":"+std::to_string(tick)+",\"milliseconds\":"+std::to_string(GetTickCount64())+
            ",\"thread\":"+std::to_string(GetCurrentThreadId())+
            ",\"caller\":\"0x2de5c5\",\"base_actor_bytes\":\""+bytes+"\",\"stable_read\":true,\"read_only\":true"+geometry.str());
    }catch(const std::exception& e){if(++failures==1)logFile->event("action_sequence_failure","\"reason\":"+jsonString(e.what()));}
}
inline void removed(bool restored,unsigned callbacks){
    if(!requested)return;
    std::lock_guard<std::mutex> lock(mutex);
    logFile->event("action_sequence_removed","\"samples\":"+std::to_string(samples)+",\"unstable_reads\":"+std::to_string(unstable)+
        ",\"failures\":"+std::to_string(failures)+",\"callbacks\":"+std::to_string(callbacks)+",\"restored\":"+(restored?"true":"false"));
}
}
