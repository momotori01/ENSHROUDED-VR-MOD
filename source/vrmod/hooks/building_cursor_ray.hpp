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
        // The tool's blue ring (where it digs) left out of the picture while a tool is in the right hand (the layer's
        // pipeline_census.hpp; 2026-10-30, user: "hide the blue ring"). EVR_TOOL_RING=1: shown.
        {using Hide=void(*)(int);static Hide hide=nullptr;static bool resolved=false;static int shown=-1;
         static const bool keep=[]{char v[8]{};const auto n=GetEnvironmentVariableA("EVR_TOOL_RING",v,sizeof v);return n==1&&v[0]=='1';}();
         if(!resolved){resolved=true;if(auto layer=GetModuleHandleW(L"evr_diagnostic.dll"))hide=reinterpret_cast<Hide>(GetProcAddress(layer,"evrSetToolRingHidden"));}
         uint64_t heldAt=0;{std::lock_guard<std::mutex> l(controllerUiFixture::toolAimMutex);heldAt=controllerUiFixture::toolAim.heldAt;}
         const int want=!keep&&heldAt&&now>=heldAt&&now-heldAt<=300?1:0;
         if(hide&&want!=shown){hide(want);shown=want;static unsigned told=0;if(told++<200)logFile->event("tool_ring_hidden","\"hidden\":"+std::string(want?"true":"false"));}}
        // A tool in the right hand (controller_ui_fixture.hpp ToolAim; 2026-10-30, user: "the pickaxe digs the ground below"): the
        // game digs where this ray meets the ground. While the tool swings, the ray goes along its head's way from 0.3 m behind
        // the head; for 0.4 s after the head met a surface, from 0.4 m before that place along its way - the game works where
        // the head strikes. Otherwise the ray as it was (the pointer's, or the game's): the blue ring where the hand points.
        // EVR_TOOL_STRIKE_CURSOR=0: the ray as it was always.
        {static const bool toolCursor=[]{char v[8]{};const auto n=GetEnvironmentVariableA("EVR_TOOL_STRIKE_CURSOR",v,sizeof v);return !(n==1&&v[0]=='0');}();
         static int toolState=0;int stateNow=0;
         controllerUiFixture::ToolAim aim;{std::lock_guard<std::mutex> l(controllerUiFixture::toolAimMutex);aim=controllerUiFixture::toolAim;}
         if(toolCursor&&aim.heldAt&&now>=aim.heldAt&&now-aim.heldAt<=300){
            std::array<double,3> from{},way{};
            if(aim.strikeAt&&now>=aim.strikeAt&&now-aim.strikeAt<=400){way=aim.strikeWay;for(size_t k=0;k<3;k++)from[k]=aim.strike[k]-way[k]*.4;stateNow=2;}
            else if(aim.swingAt&&now>=aim.swingAt&&now-aim.swingAt<=80){way=aim.way;for(size_t k=0;k<3;k++)from[k]=aim.head[k]-way[k]*.3;stateNow=1;}
            // Diagnostic (EVR_TOOL_CURSOR_BLINK=<calls>, Mock): every other run of that many calls the ray into the sky from 2 km
            // above the camera - the ring there and gone by turns, for the layer's census of its pipelines (pipeline_census.hpp).
            {static const unsigned blink=[]{char v[16]{};const auto n=GetEnvironmentVariableA("EVR_TOOL_CURSOR_BLINK",v,sizeof v);const unsigned b=n&&n<sizeof v?unsigned(std::strtoul(v,nullptr,10)):0u;return b<=100000?b:0u;}();
             if(blink&&!stateNow&&((calls.load()/blink)&1u)){WorldPositionQ32 c0{};std::memcpy(&c0,block,sizeof c0);
                from={meters(c0.x),meters(c0.y)+2000,meters(c0.z)};way={0,1,0};stateNow=3;}}
            const double wl=std::sqrt(way[0]*way[0]+way[1]*way[1]+way[2]*way[2]);
            if(stateNow&&wl>.5){
                game::CursorCameraBytes native,laid;std::memcpy(native.data(),block,native.size());laid=native;
                WorldPositionQ32 camera{};Quat q0{};std::array<float,3> scale{};float distance0=0;std::memcpy(&camera,native.data(),sizeof camera);std::memcpy(&q0,native.data()+0x18,sizeof q0);
                std::memcpy(scale.data(),native.data()+0x28,sizeof scale);std::memcpy(&distance0,native.data()+0x38,sizeof distance0);
                const float norm=q0.x*q0.x+q0.y*q0.y+q0.z*q0.z+q0.w*q0.w;
                const double cx=meters(camera.x),cy=meters(camera.y),cz=meters(camera.z);
                const double fromCamera=std::sqrt((from[0]-cx)*(from[0]-cx)+(from[1]-cy)*(from[1]-cy)+(from[2]-cz)*(from[2]-cz));
                if(scale==std::array<float,3>{1,1,1}&&std::isfinite(norm)&&std::abs(norm-1)<1e-3f&&std::isfinite(distance0)&&distance0>=0&&distance0<=64&&(fromCamera<6||stateNow==3)){
                    const float dx=float(way[0]/wl),dy=float(way[1]/wl),dz=float(way[2]/wl);
                    const Quat q=euler(std::atan2(dx,dz),std::atan2(-dy,std::hypot(dx,dz)),0);
                    WorldPositionQ32 at=camera;at.x=int64_t(from[0]*4294967296.0);at.y=int64_t(from[1]*4294967296.0);at.z=int64_t(from[2]*4294967296.0);
                    const float atTheStart=0;
                    std::memcpy(laid.data(),&at,sizeof at);std::memcpy(laid.data()+0x18,&q,sizeof q);std::memcpy(laid.data()+0x38,&atTheStart,sizeof atTheStart);
                    if(!game::onlyCursorRayChanged(native,laid))throw std::runtime_error("tool cursor ray changed more than the camera");
                    std::memcpy(block,laid.data(),laid.size());
                    if(stateNow==2&&toolState!=2){static unsigned told=0;if(told++<300){std::ostringstream o;o<<std::setprecision(9)<<"\"point\":["<<aim.strike[0]<<','<<aim.strike[1]<<','<<aim.strike[2]
                        <<"],\"way\":["<<dx<<','<<dy<<','<<dz<<"],\"since_ms\":"<<(now-aim.strikeAt)<<",\"from_camera_m\":"<<fromCamera<<",\"cursor_before\":["<<meters(before.x)<<','<<meters(before.y)<<','<<meters(before.z)<<"]";
                        logFile->event("tool_strike_cursor",o.str());}}
                }else stateNow=0;
            }else stateNow=0;
         }
         if(stateNow!=toolState){static unsigned told=0;if(told++<600)logFile->event("tool_cursor","\"state\":\""+std::string(stateNow==3?"sky":stateNow==2?"strike":stateNow==1?"swing":"as_it_was")+"\",\"cursor_before\":["+std::to_string(meters(before.x))+","+std::to_string(meters(before.y))+","+std::to_string(meters(before.z))+"]");toolState=stateNow;}}
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
