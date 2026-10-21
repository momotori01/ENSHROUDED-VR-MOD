// Placing by hand (include/evr/pointer_aim_camera.hpp): the ray that finds where a piece goes starts at the right
// hand's pointer. Only the context on the stack of the game's own task (client_cursor, 249030) is changed, just
// before 242300 makes the ray from it. No component, no camera and no sent input is written.
namespace buildingCursorRay {
using Prepare=void*(__fastcall*)(void*,void*,void*,void*);
inline Prepare original=nullptr;
inline bool requested=false,enabled=false;
inline std::atomic<bool> accepting{false};
inline std::atomic<unsigned> callbacks{0},calls{0},applied{0},failures{0},lines{0};
// 242300 has one caller: client_cursor, after the task has taken the local player's components.
constexpr uintptr_t rva=0x288180,caller=0x290c0d;
// 1.0 (Hotfix 1.0.0.1): 0x287cd0 (June 0x242300) with another prologue; one caller, 0x290758 (June 0x249c61).
constexpr const char* pattern="4C 89 44 24 18 55 41 54 41 56 41 57 48 8D AC 24 C8 E4 FF FF B8 38 1C 00 00";
constexpr uint64_t wantedMaxAgeMs=250;
struct CallbackScope{CallbackScope(){++callbacks;}~CallbackScope(){--callbacks;}};
struct Follow {float yaw=0;game::InputPoseToken token;int logged=-1;unsigned sample=0;};
inline Follow follow;inline std::mutex mutex;
inline double meters(int64_t q32){return double(q32)/4294967296.0;}
__declspec(noinline) void* __fastcall prepare(void* output,void* context,void* cursor,void* fourth){
    const auto from=reinterpret_cast<uintptr_t>(_ReturnAddress());
    if(from!=baseAddress+caller||!accepting.load())return original(output,context,cursor,fourth);
    const CallbackScope callback;
    {const ObserverLastError preserve;
     try{
        ++calls;const auto now=GetTickCount64();
        inputHeadIdle::CursorAim wanted;{std::lock_guard<std::mutex> lock(inputHeadIdle::cursorAimMutex);wanted=inputHeadIdle::cursorAim;}
        const bool fresh=wanted.mode&&wanted.entity&&now>=wanted.milliseconds&&now-wanted.milliseconds<=wantedMaxAgeMs;
        game::CursorRayResult result;const char* reason=fresh?"pose":"not_wanted";int state=0;
        const auto block=reinterpret_cast<uint8_t*>(context)+game::cursorContextCamera;
        // The task's result of the frame before (its copy of ClientCursor's first transform), to see the piece follow.
        const auto before=read<WorldPositionQ32>(reinterpret_cast<uintptr_t>(context)+0x608);   // June +0x558 (the context +0xb0 since)
        if(fresh){
            const auto value=inputPose::channel.read(wanted.entity,now,inputPose::observationMaxAgeMs);
            if(value.status==game::InputPoseStatus::Current){
                std::lock_guard<std::mutex> lock(mutex);
                if(!(follow.token==value.token)){follow.yaw=0;follow.token=value.token;}
                game::CursorCameraBytes native,pointed;std::memcpy(native.data(),block,native.size());
                result=game::pointCursorRay(native,pointed,value.head,follow.yaw,wanted.mode==3?game::CursorRaySource::Head:game::CursorRaySource::Pointer);reason=result.reason;
                if(result.applied){
                    if(!game::onlyCursorRayChanged(native,pointed))throw std::runtime_error("cursor ray changed more than the camera");
                    std::memcpy(block,pointed.data(),pointed.size());follow.yaw=result.pointer.yaw;++applied;state=wanted.mode;
                }else if(std::string(reason)=="layout")throw std::runtime_error("cursor context does not hold a camera where expected");
            }else reason=game::poseStatusName(value.status);
        }
        bool log=false;unsigned sample=0;
        {std::lock_guard<std::mutex> lock(mutex);sample=++follow.sample;
         if(follow.logged!=state){follow.logged=state;log=true;}else if(state&&sample%30==0)log=true;}
        if(log&&lines.fetch_add(1)<600){
            std::ostringstream o;o<<std::setprecision(9)<<"\"call\":"<<calls.load()<<",\"mode\":"<<unsigned(fresh?wanted.mode:0)<<",\"applied\":"<<(result.applied?"true":"false")
                <<",\"reason\":"<<jsonString(reason)<<",\"cursor_before\":["<<meters(before.x)<<','<<meters(before.y)<<','<<meters(before.z)<<']';
            if(result.applied||std::string(reason)=="tracking"||std::string(reason)=="invalid"){
                const auto f=rotate(result.nativeOrientation,{0,0,1});
                o<<",\"native_position\":["<<meters(result.nativePosition.x)<<','<<meters(result.nativePosition.y)<<','<<meters(result.nativePosition.z)
                 <<"],\"native_forward\":["<<f.x<<','<<f.y<<','<<f.z<<"],\"native_distance\":"<<result.nativeDistance;}
            if(result.applied){
                const auto& p=result.pointer;
                // How far the piece of the frame before lies from the ray of this frame (the hand moves little in a frame).
                const Vec3 to{float(meters(before.x-p.cameraPosition.x)),float(meters(before.y-p.cameraPosition.y)),float(meters(before.z-p.cameraPosition.z))};
                const float along=dot(to,p.aimForward);const float off=length(to-p.aimForward*along);
                o<<",\"pointer_position\":["<<meters(p.cameraPosition.x)<<','<<meters(p.cameraPosition.y)<<','<<meters(p.cameraPosition.z)
                 <<"],\"pointer_forward\":["<<p.aimForward.x<<','<<p.aimForward.y<<','<<p.aimForward.z<<"],\"pointer_yaw\":"<<p.yaw
                 <<",\"cursor_before_along_m\":"<<along<<",\"cursor_before_off_ray_m\":"<<off;}
            logFile->event("building_cursor_ray",o.str());
        }
     }catch(const std::exception& e){if(failures.fetch_add(1)==0)logFile->event("building_cursor_ray_failure","\"reason\":"+jsonString(e.what()));}
    }
    return original(output,context,cursor,fourth);
}
inline void install(){
    if(!requested)return;
    inputPipeline::verify(rva,pattern,caller);
    if(MH_CreateHook(reinterpret_cast<void*>(baseAddress+rva),reinterpret_cast<void*>(&prepare),reinterpret_cast<void**>(&original))!=MH_OK)
        throw std::runtime_error("cursor ray hook creation failed");
    accepting=true;if(MH_EnableHook(reinterpret_cast<void*>(baseAddress+rva))!=MH_OK){accepting=false;throw std::runtime_error("cursor ray hook enable failed");}
    enabled=true;logFile->event("building_cursor_ray_enabled","\"rva\":"+hex(rva)+",\"caller\":"+hex(caller)+",\"pattern\":"+jsonString(pattern)+",\"context_only\":true");
}
inline void disable(){
    accepting=false;if(!enabled)return;
    if(MH_DisableHook(reinterpret_cast<void*>(baseAddress+rva))!=MH_OK)throw std::runtime_error("cursor ray hook disable failed");
    enabled=false;const auto deadline=GetTickCount64()+500;
    while(callbacks.load()&&GetTickCount64()<deadline)std::this_thread::sleep_for(std::chrono::milliseconds(1));
    inputPipeline::verify(rva,pattern,caller);
    logFile->event("building_cursor_ray_removed","\"restored\":true,\"calls\":"+std::to_string(calls.load())+",\"applied\":"+std::to_string(applied.load())+
        ",\"failures\":"+std::to_string(failures.load())+",\"callbacks\":"+std::to_string(callbacks.load()));
    if(callbacks.load())throw std::runtime_error("cursor ray callbacks did not quiesce after hook removal");
}
}
