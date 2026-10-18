// Native glider launch: local quaternion arguments only, never ECS writes.
namespace glideLaunch {
using Rotate=void*(__fastcall*)(void*,const void*,const void*);
inline Rotate original=nullptr;
inline bool enabled=false;
inline std::atomic<bool> accepting{false};
inline std::atomic<unsigned> callbacks{0},samples{0},failures{0},applied{0};
constexpr uintptr_t rva=0x21ab70,sideCaller=0x26ab8a,forwardCaller=0x26abe7;
constexpr const char* pattern="4C 8B DC 48 83 EC 78 F2 41 0F 10 28 48 8B C1";
struct Input {
    uint32_t entity=0,tick=0,inputPrefix=0,sequence=0;
    uint64_t milliseconds=0,state=0;int64_t triggerTime=0,playedTime=0,currentTime=0;
    Quat camera{};bool valid=false;
};
inline std::mutex mutex;
inline std::map<uintptr_t,Input> inputs;
struct CallbackScope{CallbackScope(){++callbacks;}~CallbackScope(){--callbacks;}};
inline void capture(uintptr_t actor,uintptr_t input,uint32_t entity,uint32_t tick,uint64_t state,uint32_t flags,bool gravityNormal){
    if(!accepting.load())return;
    Input v;v.entity=entity;v.tick=tick;v.inputPrefix=read<uint32_t>(input);v.milliseconds=GetTickCount64();v.state=state;
    v.camera=read<Quat>(input+0x40);v.sequence=read<uint32_t>(actor);v.triggerTime=read<int64_t>(actor+8);
    v.playedTime=read<int64_t>(actor+0x178);v.currentTime=read<int64_t>(actor+0x180);
    const auto ui=inputUi::snapshot();const auto pose=inputPose::channel.read(entity,v.milliseconds,inputPose::observationMaxAgeMs);
    v.valid=inputHeadIdle::vrLocomotion&&pose.status==game::InputPoseStatus::Current&&(pose.head.flags&15)==15&&
        inputUi::boundNormal(ui,ui.senderContext,entity,v.milliseconds)&&gravityNormal&&(flags&1u)&&!(flags&((1u<<1)|(1u<<2)|(1u<<12)))&&
        read<uint8_t>(input+game::playerInputMode)==1&&!read<uint8_t>(input+game::playerInputMask)&&!read<uint8_t>(input+game::playerInputReady+2)&&read<float>(input+0x50)==0;
    std::lock_guard<std::mutex> lock(mutex);
    // Values expire across world/entity changes; saved addresses are identity
    // keys only and are never dereferenced by the consumer.
    for(auto it=inputs.begin();it!=inputs.end();){
        if(it->second.entity!=entity||(v.milliseconds>=it->second.milliseconds&&v.milliseconds-it->second.milliseconds>200))it=inputs.erase(it);
        else ++it;
    }
    if(inputs.size()<16||inputs.count(actor))inputs[actor]=v;
}
__declspec(noinline) void* __fastcall rotateBasis(void* output,const void* quaternion,const void* vector){
    const auto caller=reinterpret_cast<uintptr_t>(_ReturnAddress());
    // This is a widely used native math helper. Unrelated calls bypass all
    // logging, TLS, locks and callback counters. Trampolines remain allocated.
    if(caller!=baseAddress+sideCaller&&caller!=baseAddress+forwardCaller)return original(output,quaternion,vector);
    const auto callerRsp=reinterpret_cast<uintptr_t>(_AddressOfReturnAddress())+8;
    const CallbackScope callback;
    alignas(16) Quat chosen{};bool use=false;
    struct Pending {uintptr_t stack=0,source=0;Quat before{};game::GlideLaunchRotation rotation;};
    thread_local Pending pending;
    if(accepting.load()&&(caller==baseAddress+sideCaller||caller==baseAddress+forwardCaller)){
        const ObserverLastError preserve;
        try{
            // June 22a7a0: RBP=RSP+100, the query at RBP-70 (28 pointer slots), the transform slot 20. 1.0 (0x26a040, the
            // same frame): the query is at RBP-40 (0x140 bytes, 40 slots, made at 0x26a0bd), the context slot 0, the Actor
            // slot 18 (RBP+50), the transform slot 24 (RBP+80; its copy at RSP+60 gives the quaternion at +18, 0x26ab55).
            // Headset runs of 2026-10-17: with June's place every launch logged "glider quaternion/query identity differs"
            // and the session's end was marked failed (view_resource_hook_failure).
            constexpr size_t queryAt=0xc0,actorSlot=18,transformSlot=24;
            const auto slots=read<std::array<uintptr_t,40>>(callerRsp+queryAt);
            if(!slots[0]||!slots[actorSlot]||!slots[transformSlot]||reinterpret_cast<uintptr_t>(quaternion)!=slots[transformSlot]+0x18)
                throw std::runtime_error("glider quaternion/query identity differs");
            const auto state=read<uintptr_t>(slots[0]);const auto index=read<uint32_t>(slots[0]+8);
            if(!state||!index||index>0x1000000)throw std::runtime_error("glider query bounds");
            const auto entities=read<uintptr_t>(state+0x828),ticks=read<uintptr_t>(state+0x818);
            if(!entities)throw std::runtime_error("glider entity table missing");
            const auto entity=read<uint32_t>(entities+uintptr_t(index-1)*16),tick=ticks?read<uint32_t>(ticks+uintptr_t(index-1)*4):0;
            Input v;{std::lock_guard<std::mutex> lock(mutex);const auto i=inputs.find(slots[actorSlot]);if(i!=inputs.end())v=i->second;}
            const auto now=GetTickCount64();
            if(v.entity==entity&&v.milliseconds&&now>=v.milliseconds&&now-v.milliseconds<=200&&v.tick==tick){
                const auto q=read<Quat>(reinterpret_cast<uintptr_t>(quaternion));const auto axis=read<Vec3>(reinterpret_cast<uintptr_t>(vector));
                const auto actor=slots[actorSlot];const auto flags=read<uint8_t>(actor+0x1b1);
                const auto raw=read<uint64_t>(actor+0xbf8),add=read<uint64_t>(actor+0xbd0),remove=read<uint64_t>(actor+0xbd8);
                const auto effective=flags&1?(raw|add)&~remove:raw;
                const auto played=read<int64_t>(actor+0x178),time=read<int64_t>(actor+0x180);
                game::GlideLaunchRotation rotation;
                if(caller==baseAddress+sideCaller){
                    rotation=game::glideLaunchRotation(q,v.camera,effective,flags,played,v.valid);
                    pending={callerRsp,reinterpret_cast<uintptr_t>(quaternion),q,rotation};
                }else{
                    if(pending.stack!=callerRsp||pending.source!=reinterpret_cast<uintptr_t>(quaternion)||std::memcmp(&pending.before,&q,sizeof(q)))
                        throw std::runtime_error("glider basis pair changed");
                    rotation=pending.rotation;pending={};
                }
                chosen=rotation.orientation;use=rotation.applied;if(use)++applied;
                if(samples.load()<256||use){
                std::ostringstream o;o<<std::setprecision(10)<<"\"sample\":"<<++samples<<",\"entity\":"<<entity<<",\"caller\":"<<hex(caller-baseAddress)
                    <<",\"actor\":"<<hex(slots[actorSlot])<<",\"query_state\":"<<hex(state)<<",\"tick\":"<<tick<<",\"input_query_tick\":"<<v.tick
                    <<",\"input_prefix_diagnostic\":"<<v.inputPrefix<<",\"sequence\":"<<v.sequence<<",\"trigger_time\":"<<v.triggerTime<<",\"played_time\":"<<v.playedTime<<",\"current_time\":"<<v.currentTime
                    <<",\"input_age_ms\":"<<now-v.milliseconds<<",\"valid\":"<<(v.valid?"true":"false")<<",\"state\":"<<v.state
                    <<",\"camera_orientation\":["<<v.camera.x<<','<<v.camera.y<<','<<v.camera.z<<','<<v.camera.w<<']'
                    <<",\"body_orientation\":["<<q.x<<','<<q.y<<','<<q.z<<','<<q.w<<']'
                    <<",\"basis\":["<<axis.x<<','<<axis.y<<','<<axis.z<<"],\"applied\":"<<(use?"true":"false")
                    <<",\"native_played_time\":"<<played<<",\"native_current_time\":"<<time<<",\"native_sequence\":"<<read<uint32_t>(actor)
                    <<",\"native_trigger_time\":"<<read<int64_t>(actor+8)<<",\"native_state\":"<<effective<<",\"yaw_delta\":"<<rotation.yawDelta
                    <<",\"chosen_orientation\":["<<chosen.x<<','<<chosen.y<<','<<chosen.z<<','<<chosen.w<<"],\"source_unchanged\":true";
                logFile->event("glide_native_basis",o.str());
                }
            }
        }catch(const std::exception& e){use=false;pending={};if(failures.fetch_add(1)==0)logFile->event("glide_launch_failure","\"reason\":"+jsonString(e.what()));}
    }
    return original(output,use?&chosen:quaternion,vector);
}
inline void install(){
    if(!inputHeadIdle::vrLocomotion)return;
    inputPipeline::verify(rva,pattern,sideCaller);inputPipeline::verify(rva,pattern,forwardCaller);
    if(MH_CreateHook(reinterpret_cast<void*>(baseAddress+rva),reinterpret_cast<void*>(&rotateBasis),reinterpret_cast<void**>(&original))!=MH_OK)
        throw std::runtime_error("glider basis hook creation failed");
    accepting=true;if(MH_EnableHook(reinterpret_cast<void*>(baseAddress+rva))!=MH_OK){accepting=false;throw std::runtime_error("glider basis hook enable failed");}
    enabled=true;logFile->event("glide_launch_enabled","\"observation_only\":false,\"local_actor_only\":true,\"first_sequence_step_only\":true,\"argument_copy_only\":true");
}
inline void disable(){
    accepting=false;if(!enabled)return;
    if(MH_DisableHook(reinterpret_cast<void*>(baseAddress+rva))!=MH_OK)throw std::runtime_error("glider basis disable failed");
    enabled=false;const auto deadline=GetTickCount64()+500;
    while(callbacks.load()&&GetTickCount64()<deadline)std::this_thread::sleep_for(std::chrono::milliseconds(1));
    inputPipeline::verify(rva,pattern,sideCaller);inputPipeline::verify(rva,pattern,forwardCaller);
    logFile->event("glide_launch_removed","\"restored\":true,\"callbacks\":"+std::to_string(callbacks.load())+",\"samples\":"+std::to_string(samples.load())+",\"applied\":"+std::to_string(applied.load())+",\"failures\":"+std::to_string(failures.load()));
    if(callbacks.load()||failures.load())throw std::runtime_error("glider basis diagnostic failed after restoration");
}
}
