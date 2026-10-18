// Read-only local camera observation after native construction, before shake.
// No component pointer survives the callback. Diagnostic first-person opt-in.
namespace rootCamera {
using Build=void(__fastcall*)(void*,void*);
inline Build original=nullptr;
inline void* target=nullptr;
inline bool enabled=false;
inline unsigned mode=0; // 0 off, 1 observation, 2 bounded first-person diagnostic
inline std::atomic<bool> accepting{false};
inline std::atomic<unsigned> callbacks{0},failures{0};
struct Sample {
    uint64_t number=0,milliseconds=0;
    uint64_t renderEntryMilliseconds=0,renderSerial=0;
    DWORD thread=0;
    uint32_t entity=0,tick=0;uintptr_t world=0;
    float fraction=0,height=0,distance=0;
    game::RootHistory history;
    game::RootTransform stable,unshaken;
    WorldPositionQ32 controllerRoot,targetPosition;
    Vec3 shoulder;
    uint8_t cameraId=255,presentationId=255;bool syntheticPresentation=false;cameraControl::Sample controls;
};
inline std::mutex mutex;
inline Sample latest;
using WorldResolver=uintptr_t(*)(uint32_t,uintptr_t,unsigned) noexcept;inline std::atomic<WorldResolver> worldResolver{nullptr};
inline thread_local const Sample* frameSample=nullptr;
inline std::string lastFailure;
// 2026-10-09 real-device run game-20261009-103959: Select opened a full-screen native UI (frontend screen 2, the map)
// that stops the player camera update; the root sample went stale and the whole series failed after 1 s. With uiHold
// (set for live play input only) the last sample is kept while the frontend reports a non-HUD screen: the player does
// not move there. Every other staleness still fails. The render thread logs each hold span.
inline bool(*uiHold)()=nullptr;
inline std::atomic<uint64_t> heldRenders{0};
inline bool holdNow(const Sample& s,uint64_t now){return s.number&&now>=s.milliseconds&&now-s.milliseconds>1000&&uiHold&&uiHold();}
inline uint32_t calibratedEntity=0;
inline float calibratedHeight=0,lastYaw=0;
// Height calibration waits for a settled native target height of the idle camera;
// until then the anchor follows the native height and is provisional (warmup only).
// The native target height depends on the camera mode (game163158: PlayerCameraDefault
// 1.75 m for ~6 s after spawn, then a blend to PlayerCameraIdle 1.65 m), so a stable
// value of another camera is not an eye-height calibration.
constexpr uint8_t calibrationCameraId=19; // keen::CameraId::PlayerCameraIdle
inline game::HeightSettle heightSettle;inline uint32_t settlingEntity=0;inline float firstSettlingHeight=0;
inline std::atomic<uint64_t> provisionalAnchors{0};inline std::atomic<bool> requireCalibrated{false},heightReady{false};
// The start by itself (user, 2026-10-14: "30 seconds after entering is too late"): the settled height needs the
// game's idle camera, which comes only some 20 to 35 s after a world is entered (game starts 20261014-103739: 21 s,
// -115547: 33 s after the session's start) - and it is the same number every time (1.65 m). With EVR_KNOWN_HEIGHT_FILE
// in the game's environment (the loader names a file of the installation) the height that settled in an earlier
// session is taken at once, so the first pair of eyes waits for the Views' warm-up only. The measuring goes on beside
// it as before; what settles is written to the file for the next session, and does not change the running one.
inline float knownHeight=0;inline bool knownHeightUsed=false,heightMeasured=false;inline std::wstring knownHeightFile;
inline void readKnownHeight(){
    wchar_t name[1024]{};const auto n=GetEnvironmentVariableW(L"EVR_KNOWN_HEIGHT_FILE",name,DWORD(std::size(name)));
    if(!n||n>=std::size(name))return;
    knownHeightFile=name;const std::filesystem::path path{knownHeightFile};std::ifstream file{path};std::string magic;float h=0;
    if(file>>magic>>h&&magic=="EVR_KNOWN_HEIGHT_1"&&std::isfinite(h)&&h>=1.2f&&h<=2.2f)knownHeight=h;
    logFile->event("root_camera_known_height_file","\"read\":"+std::string(knownHeight>0?"true":"false")+",\"height\":"+std::to_string(knownHeight));
}
inline void writeKnownHeight(float h) noexcept {
    try{if(knownHeightFile.empty())return;const std::filesystem::path path(knownHeightFile);std::error_code ec;std::filesystem::create_directories(path.parent_path(),ec);
        std::ofstream file(path,std::ios::trunc);file<<"EVR_KNOWN_HEIGHT_1 "<<std::setprecision(9)<<h<<"\n";}catch(...){}
}
inline bool cameraModes=false,modeFixture=false;inline game::StableHeading stableHeading;
// 1.0 (2026-10-16): the camera update was rebuilt (0xeaa940, half the size of June's 0xce99d0) and sums the layers'
// rotation input in registers; camera_control_observer.hpp reads the sums at their join. While the camera-control
// observer cannot be installed the heading is the native camera's own (as without camera modes: the player's turn and
// the game's turns both turn the view), the presentation by camera id stays.
inline bool stableHeadingUsed=true;
// Heading realignment (include/evr/camera_mode.hpp). The reference is published by the render thread when the stable
// heading is calibrated; the game's thread measures the native camera against it after every camera update and asks
// the camera-control hook for the next update's correction. EVR_HEADING_REALIGN=0 in the game's environment: measure
// only. EVR_HEADING_REALIGN_TEST="<camera sample> <radians>": at that sample, turn the native camera by that much
// without counting it as the player's turn (what the game does on leaving the glider), to see the correction work.
inline std::mutex headingMutex;inline double headingReference=0;inline uint32_t headingReferenceEntity=0;
inline void realignHeading(const Sample& s){
    static const struct Options {bool correct=true;uint64_t testSample=0;float testTurn=0;
        Options(){char value[64]{};auto n=GetEnvironmentVariableA("EVR_HEADING_REALIGN",value,sizeof value);if(n&&n<sizeof value&&value[0]=='0')correct=false;
            n=GetEnvironmentVariableA("EVR_HEADING_REALIGN_TEST",value,sizeof value);
            if(n&&n<sizeof value){std::istringstream in(value);unsigned long long at=0;float turn=0;if(in>>at>>turn&&at&&std::isfinite(turn)&&std::abs(turn)<=1){testSample=at;testTurn=turn;}}}
    } options;
    static game::HeadingRealignWatch watch;static uint64_t since=0,count=0;static unsigned lines=0;   // the game's camera thread only
    if(!cameraModes||!s.controls.valid){cameraControl::realignEnabled.store(false);cameraControl::realignRequest.store(0);return;}
    double reference=0;{std::lock_guard<std::mutex> lock(headingMutex);if(headingReferenceEntity!=s.entity)return;reference=headingReference;}
    const float error=game::headingError(heading(normalized(s.unshaken.orientation),0),s.controls.gameplayTurn,reference);
    const auto step=watch.update(error,s.cameraId,options.correct);
    cameraControl::realignEnabled.store(options.correct&&!watch.stopped());
    cameraControl::realignRequest.store(step.correction);
    if(options.testSample&&s.number==options.testSample){cameraControl::disturbRequest.store(options.testTurn);
        logFile->event("camera_heading_test_turn","\"sample\":"+std::to_string(s.number)+",\"radians\":"+std::to_string(options.testTurn)+",\"camera_id\":"+std::to_string(unsigned(s.cameraId)));}
    using Event=game::HeadingRealignWatch::Event;
    if(step.event==Event::Begin)since=s.milliseconds;
    const char* what=step.event==Event::Begin?"begin":step.event==Event::End?"end":step.event==Event::Rest?"rest_the_difference_grew":
        step.event==Event::Held?"held_not_converging":step.event==Event::Stopped?"stopped_growing":++count%1800==0?"sample":nullptr;
    if(!what||lines++>=600)return;
    std::ostringstream o;o<<std::setprecision(6)<<"\"what\":\""<<what<<"\",\"sample\":"<<s.number<<",\"camera_id\":"<<unsigned(s.cameraId)<<",\"error\":"<<error
        <<",\"first\":"<<watch.first()<<",\"peak\":"<<watch.peak()<<",\"samples\":"<<watch.corrected()<<",\"milliseconds\":"<<(since?s.milliseconds-since:0)
        <<",\"correcting\":"<<(step.correction!=0?"true":"false")<<",\"corrections\":"<<cameraControl::realignApplied.load();
    logFile->event("camera_heading_realign",o.str());
}
// EVR_LAYOUT_DUMP=1: the first local camera call's controller, query slots and root history go to layout-camera-*.bin
// (raw bytes, read only) - to place the 1.0 controller's fields.
inline bool cameraDumped=false;
inline void dumpCamera(uintptr_t callerRsp,uintptr_t controller,const std::array<uintptr_t,10>& slots) noexcept {
    static const bool wanted=[]{char v[8]{};const auto n=GetEnvironmentVariableA("EVR_LAYOUT_DUMP",v,sizeof v);return n==1&&v[0]=='1';}();
    if(cameraDumped||!wanted)return;cameraDumped=true;
    auto region=[&](const char* name,uintptr_t address,size_t bytes){std::vector<uint8_t> data(bytes);
        for(size_t at=0;at<bytes;at+=0x100){SIZE_T got=0;const auto n=std::min<size_t>(0x100,bytes-at);
            if(!address||!ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(address+at),data.data()+at,n,&got))std::fill_n(data.data()+at,n,uint8_t(0));}
        std::ofstream(calibrationDirectory/(std::string("layout-camera-")+name+".bin"),std::ios::binary).write(reinterpret_cast<const char*>(data.data()),std::streamsize(bytes));};
    try{region("controller",controller,0x1600);region("slots",callerRsp+0x60,0x50);region("history",slots[2],0x200);region("entity",slots[3],0x40);
        uintptr_t state=0;SIZE_T got=0;if(slots[0])ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(slots[0]),&state,8,&got);
        region("state",state,0x900);region("caller-frame",callerRsp,0x400);
        logFile->event("layout_camera_dump_written","\"controller\":"+hex(controller)+",\"history\":"+hex(slots[2]));}catch(...){}
}
inline void observe(uintptr_t callerRsp,void* output,void* controller){
    // 1.0: the caller (0x2b2640) keeps its query at rsp+0x60 (+0x50 before; the same ten slots, controller 5, output 6);
    // the task state's entity table moved +0x7e0 -> +0x828 (9b2800 against 8d3490), the tick table next to it likewise.
    const auto slots=read<std::array<uintptr_t,10>>(callerRsp+0x60);
    dumpCamera(callerRsp,reinterpret_cast<uintptr_t>(controller),slots);
    if(slots[5]!=reinterpret_cast<uintptr_t>(controller)||slots[6]!=reinterpret_cast<uintptr_t>(output)||!slots[0]||!slots[2]||!slots[3])
        throw std::runtime_error("local camera caller query does not match native arguments");
    const auto state=read<uintptr_t>(slots[0]);
    const auto query=read<uint32_t>(slots[0]+8);
    if(!state||!query)throw std::runtime_error("missing local camera task state/index");
    const auto ticks=read<uintptr_t>(state+0x818),entities=read<uintptr_t>(state+0x828);
    const uintptr_t index=query-1;
    if(!ticks||!entities||ticks>UINTPTR_MAX-index*4||entities>UINTPTR_MAX-index*16)
        throw std::runtime_error("invalid local camera entity/tick table");
    Sample s;s.milliseconds=GetTickCount64();s.thread=GetCurrentThreadId();
    s.entity=read<uint32_t>(entities+index*16);
    if(const auto resolve=worldResolver.load())s.world=resolve(s.entity,slots[2],176);
    if(!s.entity||s.entity!=read<uint32_t>(slots[3]+4))throw std::runtime_error("camera entity is not the local owner");
    s.tick=read<uint32_t>(ticks+index*4);s.fraction=read<float>(state+0x88);
    s.history=read<game::RootHistory>(slots[2]);s.stable=game::stableRoot(s.history,s.tick,s.fraction);
    s.unshaken=read<game::RootTransform>(slots[6]);
    const auto c=reinterpret_cast<uintptr_t>(controller);
    // 1.0: the camera controller grew (0xe7ebf0 against 0xcb9c10): the camera id +0xea8 -> +0x1460, the target +0xeb0 ->
    // +0x1468, the shoulder +0xed4 -> +0x148c, the distance +0xef0 -> +0x14a4; the id is read back to June's numbering.
    /* 1.0: the root +0x40 -> +0x50 (the dump of game-20261016-053206) */ s.controllerRoot=read<WorldPositionQ32>(c+0x50);s.targetPosition=read<WorldPositionQ32>(c+0x1468);
    s.shoulder=read<Vec3>(c+0x148c);s.distance=read<float>(c+0x14a4);
    s.cameraId=game::legacyCameraId(read<uint8_t>(c+0x1460));s.presentationId=s.cameraId;s.controls=cameraControl::snapshot(c,s.entity,s.cameraId,s.milliseconds);
    s.height=float(relativeQ32(s.targetPosition.y,s.controllerRoot.y));
    normalized(s.unshaken.orientation);
    if(!std::isfinite(s.height)||!finite(s.shoulder)||!std::isfinite(s.distance))throw std::runtime_error("invalid native camera anchor values");
    {std::lock_guard<std::mutex> lock(mutex);s.number=latest.number+1;latest=s;lastFailure.clear();}
    try{realignHeading(s);}catch(...){cameraControl::realignRequest.store(0);}
}
__declspec(noinline) void __fastcall build(void* output,void* controller){
    const auto caller=reinterpret_cast<uintptr_t>(_ReturnAddress());
    const auto callerRsp=reinterpret_cast<uintptr_t>(_AddressOfReturnAddress())+sizeof(uintptr_t);
    callbacks.fetch_add(1,std::memory_order_acq_rel);
    original(output,controller);
    {const ObserverLastError lastError;
     if(accepting.load(std::memory_order_acquire)&&caller==baseAddress+0x2b2af7){
        try{observe(callerRsp,output,controller);}
        catch(const std::exception& error){
            failures.fetch_add(1);std::lock_guard<std::mutex> lock(mutex);lastFailure=error.what();
        }
     }}
    callbacks.fetch_sub(1,std::memory_order_acq_rel);
}
// True once the root camera has produced any sample (2026-10-09): observers that start before the first
// sample (real-device run game-20261009-103033: 95 attachment failures in the first 0.6 s) wait instead of failing.
// A recorded failure or a sample that later goes stale still fails through snapshot().
inline bool holdLogged=false;inline uint64_t holdBegin=0,holdRenders=0;
inline bool sampled(){
    if(frameSample)return frameSample->number!=0;
    std::lock_guard<std::mutex> lock(mutex);return latest.number!=0||!lastFailure.empty();
}
// Copy and validate in one operation: a separate usable()+snapshot() check
// races the 1000 ms boundary. Real observation errors are still propagated.
inline bool trySnapshot(Sample& copy){
    if(frameSample){copy=*frameSample;return copy.number!=0;}
    {std::lock_guard<std::mutex> lock(mutex);
        if(!lastFailure.empty())throw std::runtime_error(lastFailure);copy=latest;}
    const auto now=GetTickCount64();
    return game::rootSampleUsable(copy.number,copy.milliseconds,now,holdNow(copy,now));
}
inline Sample snapshot(){
    Sample copy;
    if(!trySnapshot(copy))throw std::runtime_error(frameSample?"stable root snapshot missing/stale at render entry":"stable root snapshot missing/stale");
    return copy;
}
// Freeze copied data at the original renderer's entry, before blocking capture
// setup or GPU waits. The 1000 ms freshness bound is still applied at that point.
// Scope lifetime and serial prohibit reusing this snapshot in a later frame.
struct FrameSnapshotScope {
    const Sample* previous=frameSample;
    Sample sample;
    FrameSnapshotScope(bool use,void* context,uint32_t completedPairs=0) noexcept {
        if(!use)return;
        try{
            const auto serial=read<uint64_t>(read<uintptr_t>(reinterpret_cast<uintptr_t>(context)+8)+0x18);
            Sample copy;bool failed;{std::lock_guard<std::mutex> lock(mutex);copy=latest;failed=!lastFailure.empty();}
            const auto now=GetTickCount64();
            const bool held=!failed&&holdNow(copy,now);
            if(!failed&&copy.number&&(now-copy.milliseconds<=1000||held)){
                sample=copy;sample.renderEntryMilliseconds=now;sample.renderSerial=serial;
                if(modeFixture){sample.syntheticPresentation=true;sample.presentationId=game::cameraModeFixture(completedPairs,sample.cameraId);}
            }
            if(held){++heldRenders;++holdRenders;if(!holdLogged){holdLogged=true;holdBegin=now;holdRenders=1;
                logFile->event("root_camera_ui_hold_begin","\"sample\":"+std::to_string(copy.number)+",\"sample_age_ms\":"+std::to_string(now-copy.milliseconds)+",\"serial\":"+std::to_string(serial));}}
            else if(holdLogged&&now-copy.milliseconds<=1000){holdLogged=false;
                logFile->event("root_camera_ui_hold_end","\"milliseconds\":"+std::to_string(now-holdBegin)+",\"renders\":"+std::to_string(holdRenders)+",\"sample\":"+std::to_string(copy.number)+",\"serial\":"+std::to_string(serial));}
        }catch(...){}
        frameSample=&sample;
    }
    ~FrameSnapshotScope(){frameSample=previous;}
};
inline void logSample(const Sample& s,uint64_t frame){
    std::ostringstream out;out<<std::setprecision(10);
    auto position=[&](const char* name,WorldPositionQ32 p){out<<",\""<<name<<"\":["<<p.x<<','<<p.y<<','<<p.z<<']';};
    out<<"\"frame\":"<<frame<<",\"sample\":"<<s.number<<",\"age_ms\":"<<GetTickCount64()-s.milliseconds
       <<",\"render_entry_age_ms\":"<<(s.renderEntryMilliseconds?s.renderEntryMilliseconds-s.milliseconds:GetTickCount64()-s.milliseconds)
       <<",\"render_serial\":"<<s.renderSerial<<",\"copied_at_render_entry\":"<<(s.renderEntryMilliseconds?"true":"false")
       <<",\"thread\":"<<s.thread<<",\"entity\":"<<s.entity<<",\"tick\":"<<s.tick<<",\"fraction\":"<<s.fraction
       <<",\"source_tick\":"<<s.history.sourceTick<<",\"newest_tick\":"<<s.history.newestTick<<",\"height\":"<<s.height
       <<",\"presentation_id\":"<<unsigned(s.presentationId)<<",\"synthetic_presentation\":"<<(s.syntheticPresentation?"true":"false")
       <<",\"camera_id\":"<<unsigned(s.cameraId)<<",\"control_sample\":"<<s.controls.number<<",\"control_valid\":"<<(s.controls.valid?"true":"false")
       <<",\"gameplay_turn\":"<<s.controls.gameplayTurn<<",\"camera_modes\":"<<(cameraModes?"true":"false")
       <<",\"weighted_rotation\":["<<s.controls.weighted[0]<<','<<s.controls.weighted[1]<<']'
       <<",\"distance\":"<<s.distance<<",\"shoulder\":["<<s.shoulder.x<<','<<s.shoulder.y<<','<<s.shoulder.z<<']'
       <<",\"unshaken_orientation\":["<<s.unshaken.orientation.x<<','<<s.unshaken.orientation.y<<','<<s.unshaken.orientation.z<<','<<s.unshaken.orientation.w<<']'
       <<",\"stable_orientation\":["<<s.stable.orientation.x<<','<<s.stable.orientation.y<<','<<s.stable.orientation.z<<','<<s.stable.orientation.w<<']';   // the body's turn (2026-10-17)
    position("source_q32",s.history.source.position);position("target_q32",s.history.target.position);
    position("rendered_q32",s.history.rendered.position);position("stable_q32",s.stable.position);
    position("controller_root_q32",s.controllerRoot);position("camera_target_q32",s.targetPosition);position("unshaken_q32",s.unshaken.position);
    logFile->event("root_camera_sample",out.str());
}
inline bool firstPerson(const Sample& s){return !cameraModes||game::cameraPresentation(s.presentationId)==game::CameraPresentation::FirstPerson;}
inline game::StableAnchor anchor(const Sample& s){
    if(!firstPerson(s))throw std::runtime_error("first-person anchor requested during native camera mode");
    if(cameraModes&&stableHeadingUsed&&!s.controls.valid)throw std::runtime_error("stable heading requires current same-controller input observation");
    if(calibratedEntity&&calibratedEntity!=s.entity)throw std::runtime_error("local player changed during bounded first-person diagnostic");
    if(!calibratedEntity&&knownHeight>0&&s.height>0&&!requireCalibrated.load()){
        calibratedEntity=s.entity;calibratedHeight=knownHeight;heightReady=true;knownHeightUsed=true;
        std::ostringstream out;out<<std::setprecision(9)<<"\"entity\":"<<s.entity<<",\"height\":"<<knownHeight<<",\"native_height_now\":"<<s.height<<",\"sample\":"<<s.number<<",\"camera_id\":"<<unsigned(s.cameraId)
            <<",\"from\":\"an earlier session's settled height\"";
        logFile->event("root_camera_known_height",out.str());
    }
    float height=calibratedHeight;
    if(knownHeightUsed&&!heightMeasured&&calibratedEntity==s.entity){
        // The measurement beside it: for the next session and for the record.
        if(s.cameraId!=calibrationCameraId)heightSettle.reset();
        else if(heightSettle.add(s.number,s.milliseconds,game::HeightSettle::Values{s.height,s.distance,s.shoulder.x,s.shoulder.y,s.shoulder.z}).settled){
            heightMeasured=true;writeKnownHeight(s.height);
            std::ostringstream out;out<<std::setprecision(9)<<"\"entity\":"<<s.entity<<",\"measured\":"<<s.height<<",\"used\":"<<calibratedHeight<<",\"difference_m\":"<<s.height-calibratedHeight<<",\"sample\":"<<s.number;
            logFile->event("root_camera_height_measured",out.str());
        }
    }
    if(!calibratedEntity){
        if(s.height<=0)throw std::runtime_error("native camera target height not suitable for first-person calibration");
        if(requireCalibrated.load())throw std::runtime_error("first-person height did not settle before live pairs");
        if(settlingEntity&&settlingEntity!=s.entity)throw std::runtime_error("local player changed during first-person height settling");
        if(!settlingEntity){settlingEntity=s.entity;firstSettlingHeight=s.height;}
        if(s.cameraId!=calibrationCameraId)heightSettle.reset();
        const auto settle=s.cameraId==calibrationCameraId?heightSettle.add(s.number,s.milliseconds,game::HeightSettle::Values{s.height,s.distance,s.shoulder.x,s.shoulder.y,s.shoulder.z}):game::HeightSettle::State{};
        if(settle.settled){
            calibratedEntity=s.entity;calibratedHeight=s.height;height=calibratedHeight;heightReady=true;heightMeasured=true;writeKnownHeight(s.height);
            std::ostringstream out;out<<std::setprecision(9)<<"\"entity\":"<<s.entity<<",\"height\":"<<std::to_string(calibratedHeight)<<",\"height_precise\":"<<calibratedHeight
                <<",\"settle_samples\":"<<settle.samples<<",\"settle_span_ms\":"<<settle.spanMs<<",\"settle_spread_m\":"<<settle.spread
                <<",\"first_settling_height\":"<<firstSettlingHeight<<",\"provisional_anchors\":"<<provisionalAnchors.load()<<",\"sample\":"<<s.number<<",\"camera_id\":"<<unsigned(s.cameraId)<<",\"distance\":"<<s.distance<<",\"shoulder\":["<<s.shoulder.x<<','<<s.shoulder.y<<','<<s.shoulder.z<<"],\"settle_values\":\"height_distance_shoulder\"";
            logFile->event("root_camera_calibrated",out.str());
        }else{
            height=s.height;
            if(!provisionalAnchors.fetch_add(1)){std::ostringstream out;out<<std::setprecision(9)<<"\"entity\":"<<s.entity<<",\"height\":"<<s.height<<",\"sample\":"<<s.number<<",\"camera_id\":"<<unsigned(s.cameraId);
                logFile->event("root_camera_height_settling",out.str());}
        }
    }
    if(cameraModes&&stableHeadingUsed&&!stableHeading.calibrated()){
        const auto initial=stableHeading.value(s.entity,s.unshaken.orientation,s.controls.gameplayTurn);
        std::ostringstream out;out<<std::setprecision(17)<<"\"entity\":"<<s.entity<<",\"yaw\":"<<initial<<",\"gameplay_turn\":"<<s.controls.gameplayTurn<<",\"turn_sign\":-1";
        logFile->event("root_heading_calibrated",out.str());
        double reference=0;
        if(stableHeading.reference(s.entity,reference)){std::lock_guard<std::mutex> lock(headingMutex);headingReference=reference;headingReferenceEntity=s.entity;}
    }
    lastYaw=cameraModes&&stableHeadingUsed?stableHeading.value(s.entity,s.unshaken.orientation,s.controls.gameplayTurn):heading(normalized(s.unshaken.orientation),lastYaw);
    game::StableAnchor a;a.root=s.stable.position;a.orientation=euler(lastYaw,0,0);a.cameraOffsetGameUnits={0,height,0};
    return a;
}
inline game::StableAnchor renderAnchor(const Sample& s,WorldPositionQ32 nativePosition,Quat nativeOrientation){
    return firstPerson(s)?anchor(s):game::nativeCameraAnchor(nativePosition,nativeOrientation);
}
inline void install(){
    try{cameraControl::install();}catch(const std::exception& e){stableHeadingUsed=false;
        logFile->event("optional_hook_skipped","\"hook\":\"camera_control\",\"reason\":"+jsonString(e.what())+",\"heading\":\"native_camera\"");}
    const char* signature="48 8B C4 48 89 58 08 48 89 78 10 55 48 8D 68 A1 48 81 EC F0 00 00 00 0F 10 82 B0";
    auto bytes=parsePattern(signature);
    const uint8_t call[]{0xe8,0x99,0xc7,0xbc,0x00};   // build 25988036: call 0xe7f290 at 0x2b2af2 (1.0.0.1: e8 99 c4 bc 00, 0xe7ec70 at 0x2b27d2; 1.0: e8 79 c4 bc 00)
    target=reinterpret_cast<void*>(baseAddress+0xe7f290);
    if(scan(static_cast<const uint8_t*>(target),bytes.size(),bytes)!=std::vector<size_t>{0}||
       std::memcmp(reinterpret_cast<void*>(baseAddress+0x2b2af2),call,sizeof(call)))throw std::runtime_error("unshaken camera constructor/caller signature changed");
    if(MH_CreateHook(target,reinterpret_cast<void*>(&build),reinterpret_cast<void**>(&original))!=MH_OK)throw std::runtime_error("unshaken camera hook creation failed");
    accepting=true;
    if(MH_EnableHook(target)!=MH_OK){accepting=false;throw std::runtime_error("unshaken camera hook enable failed");}
    enabled=true;
    logFile->event("root_camera_hook_enabled","\"rva\":\"0xe7f290\",\"caller_rva\":\"0x2b2af7\",\"query_stack_offset\":96,\"mode\":"+std::to_string(mode)+",\"pattern\":"+jsonString(signature));
    // 2026-10-16: the game updates its camera only while its window is in front (two Mock auto starts ended here, the
    // window came to the front 4 and 12 s after the world). The three seconds count only while it is; two minutes at most.
    const auto start=GetTickCount64();auto deadline=start+3000;uint64_t behind=0;
    const auto inFront=[]{DWORD process=0;const auto window=GetForegroundWindow();
        return window&&GetWindowThreadProcessId(window,&process)&&process==GetCurrentProcessId();};
    while(GetTickCount64()<deadline){
        {std::lock_guard<std::mutex> lock(mutex);if(latest.number){
            if(behind)logFile->event("root_camera_waited_for_window","\"behind_ms\":"+std::to_string(behind)+",\"total_ms\":"+std::to_string(GetTickCount64()-start));
            return;}}
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
        if(!inFront()&&GetTickCount64()-start<120000){behind+=2;deadline=std::max(deadline,GetTickCount64()+3000);}
    }
    throw std::runtime_error("no valid unshaken local camera after three seconds");
}
inline void disable(){
    accepting=false;cameraControl::disable();if(!enabled)return;
    if(MH_DisableHook(target)!=MH_OK)throw std::runtime_error("unshaken camera hook disable failed");
    enabled=false;
    const auto deadline=GetTickCount64()+500;
    while(callbacks.load()&&GetTickCount64()<deadline)std::this_thread::sleep_for(std::chrono::milliseconds(1));
    const auto bytes=parsePattern("48 8B C4 48 89 58 08 48 89 78 10 55 48 8D 68 A1 48 81 EC F0 00 00 00");
    const bool restored=scan(static_cast<const uint8_t*>(target),bytes.size(),bytes)==std::vector<size_t>{0};
    logFile->event("root_camera_hook_removed","\"original_bytes_restored\":"+std::string(restored?"true":"false")+
        ",\"remaining_callbacks\":"+std::to_string(callbacks.load())+",\"read_failures\":"+std::to_string(failures.load()));
    if(!restored)throw std::runtime_error("unshaken camera original bytes not restored");
}
}
