// Read-only observation of the native movement camera quaternion consumers.
// Never changes input, ActorInput, player transforms, or network packets.
#include "input_pipeline_observer.hpp"
#include "input_ui_observer.hpp"
#include "compass_head.hpp"
#include "input_head_idle.hpp"
#include "glide_launch_bridge.hpp"
#include "building_cursor_ray.hpp"
#include "fishing_charge.hpp"
#include "input_shadow_identity.hpp"
#include "input_actor_observer.hpp"
#include "input_picking_observer.hpp"
namespace inputDirection {
using Convert=void(__fastcall*)(const void*,void*);
using Copy=void*(__fastcall*)(void*,const void*);
inline Convert original=nullptr;
inline Copy originalCopy=nullptr;
inline bool requested=false,enabled=false,copyEnabled=false;
inline std::atomic<bool> accepting{false};
inline std::atomic<unsigned> callbacks{0},samples{0},failures{0},calls{0};
inline std::atomic<unsigned> copyCalls{0},copySamples{0};
inline unsigned movementInterval=16,senderInterval=8;
struct CallbackScope{CallbackScope(){callbacks.fetch_add(1);}~CallbackScope(){callbacks.fetch_sub(1);}};
constexpr uintptr_t rva=0x42880;
constexpr uintptr_t copyRva=0xb0abb0,copyCaller=0x3a2580;
constexpr const char* copyPattern="8B 02 89 01 8B 42 04 89 41 04 8B 42 08 89 41 08";
constexpr const char* pattern="48 8B C4 48 83 EC 68 F3 0F 10 61 0C F3 0F 10 05";
__declspec(noinline) void __fastcall convert(const void* quaternion,void* matrix){
    const auto caller=reinterpret_cast<uintptr_t>(_ReturnAddress());
    const auto callerRsp=reinterpret_cast<uintptr_t>(_AddressOfReturnAddress())+8;
    const CallbackScope callback;
    original(quaternion,matrix);
    {const ObserverLastError preserve;
     if(accepting.load()&&(caller==baseAddress+0x2dce7f||caller==baseAddress+0x2dcf0e)){
        const bool logThis=calls.fetch_add(1)%movementInterval==0&&samples.load()<256;
        if(!logThis&&!inputHeadIdle::requested)return;
        try{
            // 2873a0: RBP=callerRSP+100, query starts at RBP-50.
            // 1.0: the query of 2dbd80 is 0x170 bytes at rsp+0x80 (input_actor_observer.hpp actorQuery*).
            const auto slots=read<std::array<uintptr_t,inputActor::actorQuerySlots>>(callerRsp+inputActor::actorQueryAt);
            const auto input=slots[inputActor::slotInput],actor=slots[inputActor::slotActor],playerState=slots[inputActor::slotPlayerState];
            if(!input||!actor||!playerState||reinterpret_cast<uintptr_t>(quaternion)!=input+0x40||!slots[0])
                throw std::runtime_error("movement query arguments mismatch");
            const auto state=read<uintptr_t>(slots[0]);const auto query=read<uint32_t>(slots[0]+8);
            if(!state||!query)throw std::runtime_error("movement query context missing");
            const auto entities=read<uintptr_t>(state+0x828);
            if(!entities||query>0x1000000)throw std::runtime_error("movement entity table bounds");
            const auto entity=read<uint32_t>(entities+uintptr_t(query-1)*16);
            rootCamera::Sample local;
            {std::lock_guard<std::mutex> lock(rootCamera::mutex);local=rootCamera::latest;}
            if(!local.number||GetTickCount64()-local.milliseconds>1000)return;
            if(entity!=local.entity)return;
            const auto q=read<Quat>(input+0x40);normalized(q);
            const auto position=read<WorldPositionQ32>(input+0x28);
            const auto stick=read<std::array<float,2>>(input+game::playerInputStick);
            const auto desired=read<Vec3>(actor+0x18c);
            const auto flags=read<uint32_t>(playerState+0x34);
            inputHeadIdle::observeState(entity,flags,slots[9],slots[2]);
            if(!logThis)return;
            const auto tickTable=read<uintptr_t>(state+0x818);
            const auto tick=tickTable?read<uint32_t>(tickTable+uintptr_t(query-1)*4):0;
            const auto inputMode=read<uint8_t>(input+game::playerInputMode),mask=read<uint8_t>(input+game::playerInputMask);
            const auto mat=read<std::array<float,12>>(reinterpret_cast<uintptr_t>(matrix));
            std::ostringstream out;out<<std::setprecision(10)<<"\"sample\":"<<samples.fetch_add(1)+1
                <<",\"caller\":"<<hex(caller-baseAddress)<<",\"entity\":"<<entity<<",\"local_entity\":"<<local.entity<<",\"tick\":"<<tick
                <<",\"input_mode\":"<<unsigned(inputMode)<<",\"client_state_mask\":"<<unsigned(mask)<<",\"player_flags\":"<<flags
                <<",\"camera_q32\":["<<position.x<<','<<position.y<<','<<position.z<<"]"
                <<",\"camera_orientation\":["<<q.x<<','<<q.y<<','<<q.z<<','<<q.w<<"]"
                <<",\"movement_stick\":["<<stick[0]<<','<<stick[1]<<"],\"desired_local\":["<<desired.x<<','<<desired.y<<','<<desired.z<<"]"
                <<",\"native_basis\":[";
            for(unsigned row=0;row<3;row++)for(unsigned col=0;col<3;col++){
                if(row||col)out<<',';const auto v=mat[row*4+col];if(!std::isfinite(v))throw std::runtime_error("nonfinite movement basis");out<<v;
            }
            out<<"],\"read_only\":true";logFile->event("input_direction_sample",out.str());
        }catch(const std::exception& e){
            if(failures.fetch_add(1)==0)logFile->event("input_direction_failure","\"reason\":"+jsonString(e.what()));
        }
     }}
}
__declspec(noinline) void* __fastcall copyInput(void* output,const void* input){
    const auto caller=reinterpret_cast<uintptr_t>(_ReturnAddress());
    const auto callerRsp=reinterpret_cast<uintptr_t>(_AddressOfReturnAddress())+8;
    const CallbackScope callback;
    auto result=originalCopy(output,input);
    const ObserverLastError preserve;
    if(caller==baseAddress+copyCaller)inputPipeline::pending={};
    if(!accepting.load()||caller!=baseAddress+copyCaller||copyCalls.fetch_add(1)%senderInterval||copySamples.load()>=256)return result;
    try{
        // 326f70 copies ClientPlayerInputData before prediction update and
        // history append/network serialization. Its original R9 is saved here.
        // 1.0: the builder (0x3a2150) pushes eight registers and takes 0xf28 (June seven and 0xde0): its r9 home is at
        // the copy call's rsp +0xf88, its r8d (the tick) at +0xf80 (June +0xe38 / +0xe30).
        const auto context=read<uintptr_t>(callerRsp+0xf88);
        if(!context)throw std::runtime_error("input sender context missing");
        const auto owner=context+0x648;const auto entity=read<uint32_t>(owner+4);   // 1.0: +0x5c0 -> +0x648 (entity at +4 still)
        const auto tick=read<uint32_t>(callerRsp+0xf80);
        rootCamera::Sample local;
        {std::lock_guard<std::mutex> lock(rootCamera::mutex);local=rootCamera::latest;}
        if(!local.number||GetTickCount64()-local.milliseconds>1000||entity!=local.entity)return result;
        const auto in=reinterpret_cast<uintptr_t>(input),out=reinterpret_cast<uintptr_t>(output);
        const auto camera=read<std::array<uint8_t,64>>(in+0x20);
        const auto q=read<Quat>(in+0x38);normalized(q);
        const auto position=read<WorldPositionQ32>(in+0x20);
        const bool cameraCopied=camera==read<std::array<uint8_t,64>>(out+0x20);
        const auto ownerCamera=read<std::array<uint8_t,64>>(owner+0x60);
        const bool localCamera=camera==ownerCamera;
        const auto stick=read<std::array<float,2>>(in+game::clientStick);
        const auto copiedStick=read<std::array<float,2>>(out+game::clientStick);
        const auto mask=read<uint8_t>(in+game::clientMask);
        const bool copied=result==output&&cameraCopied&&stick==copiedStick&&mask==read<uint8_t>(out+game::clientMask)&&
            read<uint64_t>(in+game::clientDigital)==read<uint64_t>(out+game::clientDigital);
        const auto sample=copySamples.fetch_add(1)+1;
        std::ostringstream fields;fields<<std::setprecision(10)<<"\"sample\":"<<sample
            <<",\"entity\":"<<entity<<",\"local_entity\":"<<local.entity<<",\"tick\":"<<tick<<",\"context\":"<<hex(context)
            <<",\"input_mode\":"<<unsigned(read<uint8_t>(in+game::clientInputMode))<<",\"client_state_mask\":"<<unsigned(mask)
            <<",\"building_camera\":"<<(read<uint8_t>(in+game::clientBuildingCamera)?"true":"false")
            <<",\"ready_for_gameplay\":"<<(read<uint8_t>(context+0x2c51)?"true":"false")
            <<",\"camera_q32\":["<<position.x<<','<<position.y<<','<<position.z<<"]"
            <<",\"camera_orientation\":["<<q.x<<','<<q.y<<','<<q.z<<','<<q.w<<"]"
            <<",\"movement_stick\":["<<stick[0]<<','<<stick[1]<<"],\"native_copy_matches\":"<<(copied?"true":"false")
            <<",\"camera_matches_local_data\":"<<(localCamera?"true":"false")<<",\"read_only\":true";
        auto bytes=[&](const char* name,const auto& values){
            fields<<",\""<<name<<"\":\"";const char* digits="0123456789abcdef";
            for(auto v:values)fields<<digits[v>>4]<<digits[v&15];fields<<'\"';
        };
        bytes("camera_source_bytes",camera);bytes("camera_owner_bytes",ownerCamera);
        logFile->event("input_sender_sample",fields.str());
        inputUi::observe(sample,entity,tick,context);
        inputPose::observe(entity,tick,sample,stick);
        inputPipeline::start(in,callerRsp,sample,entity,tick);
    }catch(const std::exception& e){
        if(failures.fetch_add(1)==0)logFile->event("input_direction_failure","\"reason\":"+jsonString(e.what()));
    }
    return result;
}
inline void install(){
    inputUi::verify();
    inputPose::enabled=true;
    const auto bytes=parsePattern(pattern);
    if(scan(reinterpret_cast<const uint8_t*>(baseAddress+rva),bytes.size(),bytes)!=std::vector<size_t>{0})
        throw std::runtime_error("movement quaternion signature changed");
    for(uintptr_t after:{uintptr_t(0x2dce7f),uintptr_t(0x2dcf0e)}){
        const auto call=baseAddress+after-5;
        if(read<uint8_t>(call)!=0xe8||uintptr_t(int64_t(call+5)+read<int32_t>(call+1))!=baseAddress+rva)
            throw std::runtime_error("movement quaternion caller changed");
    }
    if(MH_CreateHook(reinterpret_cast<void*>(baseAddress+rva),reinterpret_cast<void*>(&convert),reinterpret_cast<void**>(&original))!=MH_OK)
        throw std::runtime_error("movement observer creation failed");
    accepting=true;
    if(MH_EnableHook(reinterpret_cast<void*>(baseAddress+rva))!=MH_OK){accepting=false;throw std::runtime_error("movement observer enable failed");}
    enabled=true;logFile->event("input_direction_observer_enabled","\"rva\":"+hex(rva)+",\"pattern\":"+jsonString(pattern));
    // 1.0 (2026-10-16): the sender's copy of ClientPlayerInputData (0x240790 in June) is inlined into the builder
    // (0x3a20f0, the loop at 0x3a2200); the copy observer and the read-only pipeline correlation behind it are left out.
    // The observers after it are installed one by one: one whose check of the new build fails is noted and left out.
    auto optionalPart=[](const char* name,auto&& body){try{body();}catch(const std::exception& e){
        logFile->event("optional_hook_skipped","\"hook\":"+jsonString(name)+",\"reason\":"+jsonString(e.what()));}};
    optionalPart("input_sender_copy",[&]{
        const auto copyBytes=parsePattern(copyPattern);const auto call=baseAddress+copyCaller-5;
        if(scan(reinterpret_cast<const uint8_t*>(baseAddress+copyRva),copyBytes.size(),copyBytes)!=std::vector<size_t>{0}||
           read<uint8_t>(call)!=0xe8||uintptr_t(int64_t(call+5)+read<int32_t>(call+1))!=baseAddress+copyRva)
            throw std::runtime_error("input sender copy signature/caller changed");
        inputPipeline::install();
        if(MH_CreateHook(reinterpret_cast<void*>(baseAddress+copyRva),reinterpret_cast<void*>(&copyInput),reinterpret_cast<void**>(&originalCopy))!=MH_OK||
           MH_EnableHook(reinterpret_cast<void*>(baseAddress+copyRva))!=MH_OK)throw std::runtime_error("input sender copy hook failed");
        copyEnabled=true;logFile->event("input_sender_observer_enabled","\"rva\":"+hex(copyRva)+",\"caller\":"+hex(copyCaller)+",\"pattern\":"+jsonString(copyPattern));});
    optionalPart("input_shadow",[&]{inputShadow::install();});
    optionalPart("input_actor",[&]{inputActor::sampleInterval=inputHeadIdle::fixtureRequested?8:16;inputActor::install();});
    optionalPart("input_picking",[&]{inputPicking::install();});
    optionalPart("glide_launch",[&]{glideLaunch::install();});
    optionalPart("building_cursor_ray",[&]{buildingCursorRay::install();});
    optionalPart("fishing_charge",[&]{fishingCharge::install();});
    optionalPart("compass_head",[&]{compassHead::install();});
    optionalPart("menu_pointer",[&]{menuPointer::install();});
    optionalPart("aim_mark",[&]{aimMark::install(controllerUiFixture::bowDrawRequested||controllerUiFixture::rangedAimRequested);});
    optionalPart("key_script",[&]{keyScript::install();});
}
inline void disable(bool handoffInput=false,bool retainAppearance=false){
    // Stop new input observations first. Keep their UI/Pose dependencies alive
    // until the original callbacks have returned; otherwise shutdown can leave
    // an input sender record with no matching UI snapshot.
    accepting=false;const bool hadCopy=copyEnabled,hadMovement=enabled;bool copyRestored=true;
    inputShadow::disable();
    if(copyEnabled){
        if(MH_DisableHook(reinterpret_cast<void*>(baseAddress+copyRva))!=MH_OK)throw std::runtime_error("input sender observer disable failed");
        copyEnabled=false;
        const auto bytes=parsePattern(copyPattern);
        copyRestored=scan(reinterpret_cast<const uint8_t*>(baseAddress+copyRva),bytes.size(),bytes)==std::vector<size_t>{0};
    }
    if(enabled&&MH_DisableHook(reinterpret_cast<void*>(baseAddress+rva))!=MH_OK)throw std::runtime_error("movement observer disable failed");
    enabled=false;const auto deadline=GetTickCount64()+500;
    while(callbacks.load()&&GetTickCount64()<deadline)std::this_thread::sleep_for(std::chrono::milliseconds(1));
    if(callbacks.load())throw std::runtime_error("input observer callbacks did not quiesce");
    // Parent callbacks are quiescent. A diagnostic policy failure must not
    // skip sibling teardown, Pose closure, or the original-hook restoration log.
    std::exception_ptr cleanupError;
    auto cleanup=[&](auto action){try{action();}catch(...){if(!cleanupError)cleanupError=std::current_exception();}};
    cleanup([]{glideLaunch::disable();});
    cleanup([]{menuPointer::disable();});
    cleanup([]{aimMark::disable();});
    cleanup([]{keyScript::disable();});
    cleanup([]{buildingCursorRay::disable();});
    cleanup([]{fishingCharge::disable();});
    cleanup([]{compassHead::disable();});
    cleanup([]{inputPipeline::disable();});
    cleanup([&]{if(retainAppearance)inputActor::retainAppearanceOnly();else inputActor::disable();});
    cleanup([]{inputPicking::disable();});
    cleanup([&]{inputUi::disable(handoffInput);});
    inputPose::enabled=false;cleanup([]{inputPose::close();});
    if(hadCopy)logFile->event("input_sender_observer_removed","\"restored\":"+std::string(copyRestored?"true":"false")+
        ",\"samples\":"+std::to_string(copySamples.load())+",\"calls\":"+std::to_string(copyCalls.load()));
    if(!hadMovement){if(cleanupError)std::rethrow_exception(cleanupError);return;}
    const auto bytes=parsePattern(pattern);
    const bool restored=scan(reinterpret_cast<const uint8_t*>(baseAddress+rva),bytes.size(),bytes)==std::vector<size_t>{0};
    logFile->event("input_direction_observer_removed","\"restored\":"+std::string(restored?"true":"false")+
        ",\"samples\":"+std::to_string(samples.load())+",\"calls\":"+std::to_string(calls.load())+
        ",\"failures\":"+std::to_string(failures.load())+",\"remaining_callbacks\":"+std::to_string(callbacks.load()));
    if(!restored||!copyRestored)throw std::runtime_error("input observer code restoration failed");
    if(cleanupError)std::rethrow_exception(cleanupError);
}
}
