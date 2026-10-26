// The nocked arrow on the bow in the player's hand, as VHVR shows the held arrow: at the string point, pointing at the
// arrow rest (docs/analysis/vhvr_spec_20261010/bow.md 4). The game's own arrow (ShowAmmunition) is not an equipment
// attachment: it is found as the unposed model that moves with the game's bow. That instance stays untouched and out
// of the eyes; a Scene-owned copy of the same model is drawn on the string (mask/SDF 0), in the private slot the
// shield does not use while a bow is held.
namespace motionArrowVisual {
inline bool requested=false,ready=false;
inline uint32_t handle=0,source=0;inline uintptr_t resource=0,sourceResource=0,measuredResource=0,scene=0;inline DWORD thread=0;
inline uint64_t serial=0,frame=0,updates=0,hiddenSerial=0,hiddenFrame=0,lastScan=0,candidatesAt=0;
inline unsigned retired=0,adoptions=0,scans=0,losses=0;
inline uint64_t lastSeenBow=0,lookFastUntil=0,knownTaken=0;   // the bow just came out or shot: looked for every 60 ms
inline std::array<uint64_t,2> payloadSerial{};
struct Seen {uint32_t handle=0;WorldPositionQ32 position{};};
inline std::vector<Seen> candidates;
// Where the nock is on the arrow model: the model axis that points along the shot (from how the game holds its own
// arrow on its bow) and the signed metres from the model origin to the rear end of that axis (from the model's own
// extent; measured on the game's nocked arrow when the extent is not the long thin shape of an arrow).
inline bool measured=false,extentKnown=false;inline float nockDistance=0;inline Vec3 axis{};inline unsigned failures=0;
// The arrow on the string from the grab (2026-10-21; user after the online session: "nocking the bow responds badly" -
// in the logs the game's arrow came 0.3 .. 0.4 s after the hand took the string, as a guest and alone alike: the game's
// nocking movement; VHVR shows the arrow at the grab). Until the game's arrow shows, the clone of the last arrow stands
// on the string with the turn it had against the game's bow the last time (relative) and the rest of its transform;
// for earlyMs at the most (without arrows the game shows none, and the clone goes). EVR_ARROW_EARLY=0: as before.
inline const bool earlyWanted=[]{char v[8]{};const auto n=GetEnvironmentVariableA("EVR_ARROW_EARLY",v,sizeof v);return !(n==1&&v[0]=='0');}();
inline constexpr uint64_t earlyMs=700;
inline bool haveRelative=false,wasHeld=false;inline Quat relative{0,0,0,1};inline std::array<uint8_t,56> lastTransform{};inline uint64_t heldSince=0,earlyUpdates=0,lastHeldMs=0;inline unsigned earlyLogs=0;
// The guest's own arrow flies on its PC (2026-10-21, user 04:15: "what concerns the player's action is done on each PC;
// never wait for the host's answer before the own action shows" - 04:25: "the host may not even see the arrow, it is
// fast"; 04:30: "the host needs only the hit's damage"). A guest's arrows are made by the host's server and reached the
// guest a round trip after the release. Now at the release the clone that stood on the string flies on, along the
// shot's line with the game's arrow speed and gravity (Flight::speed from the bow, flightGravity: measured, see below), until it meets
// a kept creature or thing (live_melee_targets.hpp segmentHit - there it stops and the hit goes to the host,
// flightReport) or the ground ahead (aimReach's level ground) or 1.5 s pass; a new draw ends it. Only as a guest (a
// host's and a single player's arrows are made at once by their own server). EVR_ARROW_FLIGHT=0: off; =2: also alone
// (a test: the clone flies beside the game's own arrow).
// The game's arrow, measured (headset session 2026-10-21 10:24, EVR_EFFECT_CENSUS=1, three shots of the bow 327157552):
// 89.9 - 90.3 m/s as it leaves, its keen::ecs::Gravity (0,-1,0) x 25 on - the vertical speed fell 25 m/s each second -
// and no drag (the level speed stayed the same). Before, a guess of 60 m/s and 9.81.
// The speed is the bow's (2026-10-21, the game data: the bow's placedEntityLocalSpawnVelocity, 75 / 80 / 90 / 100 m/s over
// the 62 bows; no skill, perk, buff or arrow changes the speed, the gravity or the drag - docs/analysis/
// arrow_flight_skills_20261021): the shown ranged weapon's item in include/evr/bow_speed_catalog.hpp, 90 for one not
// in it. EVR_ARROW_FLIGHT_SPEED=<m/s> sets one speed for every bow (a test).
struct Flight {bool on=false,stuck=false,reported=false,strong=false,lookOnly=false;float speed=90,gravity=25;uint32_t item=0;uint64_t serial=0,start=0,stuckAt=0;std::array<uint8_t,56> transform{};Vec3 dir{};Quat rotation{0,0,0,1};float reach=1e9f;std::array<int64_t,3> last{};
    uint64_t firstAsk=0,lastAnswer=0;bool rayHeard=false,wallKnown=false;std::array<int64_t,3> wall{};};
inline Flight flight;inline uint64_t aimSeen=0;inline unsigned flightLogs=0;
inline const int flightMode=[]{char v[8]{};const auto n=GetEnvironmentVariableA("EVR_ARROW_FLIGHT",v,sizeof v);return n==1&&v[0]=='0'?0:n==1&&v[0]=='2'?2:1;}();
inline const float flightSpeedSet=[]{char v[16]{};const auto n=GetEnvironmentVariableA("EVR_ARROW_FLIGHT_SPEED",v,sizeof v);const float f=n&&n<sizeof v?std::strtof(v,nullptr):0.f;return std::isfinite(f)&&f>1?f:0.f;}();
inline const float flightGravity=[]{char v[16]{};const auto n=GetEnvironmentVariableA("EVR_ARROW_FLIGHT_GRAVITY",v,sizeof v);const float f=n&&n<sizeof v?std::strtof(v,nullptr):-1.f;return std::isfinite(f)&&f>=0?f:25.f;}();
// The host's and a single player's shot too, for the eyes only (2026-10-26, user after the headset: "at the shot the original arrow
// shows for a moment, and the shot leaves from the original model's place"). The game's arrow itself starts by the hand (headset run
// 20261026-150250: 11 shots, the spawn 0.2 m below the hand's aim), but the arrow the game nocked stays on the original model's hand
// for about 0.3 s after the release (motion_arrow_source_lost 0.3 s after bow_tap_released) and was in sight from where it stopped
// being the arrow at the game's bow. Now the clone on the string flies on at the release as a guest's does, and the game's arrows of
// the shot - that nocked one among them (lostSource) - stay out of the eyes meanwhile (foreignArrows). Only the look: no hit is told
// to anyone (flight.reported from the start) - the game's own arrow hits.
// Off by default since 2026-10-26 16:20 (headset run 20261026-155936: the game crashed in its renderer - a null in a list of what the
// frame used, exe+0xa209ff - right as the 7th such flight began; not reproduced in Mock, the cause not known. And foreignArrows hid
// every arrow of that model in sight, those stuck in the ground and walls from earlier shots too). Instead the game's nocked arrow
// left on the original model's hand after the release (lostSource) is kept out of the eyes for a second, as the nocked one always
// was (lostHidden*). EVR_ARROW_FLIGHT_LOCAL=1: the look-only flight; =<frame>: from that XR frame on (a diagnostic for Mock).
inline const uint64_t localFlightFrom=[]{char v[24]{};const auto n=GetEnvironmentVariableA("EVR_ARROW_FLIGHT_LOCAL",v,sizeof v);
    if(n==1&&v[0]=='1')return 0ull;if(n&&n<sizeof v){const auto f=std::strtoull(v,nullptr,10);if(f>1)return uint64_t(f);}return UINT64_MAX;}();
inline uint32_t lostSource=0;inline uint64_t lostSourceAt=0,lostHiddenSerial=0,lostHiddenFrame=0;
inline bool (*flightHit)(const std::array<int64_t,3>&,const std::array<int64_t,3>&,float&,std::array<int64_t,3>&,bool&,uint32_t&)=nullptr;
inline void (*flightReport)(const std::array<int64_t,3>&,bool,uint32_t)=nullptr;
inline float (*flightReach)(WorldPositionQ32,Vec3,uint64_t)=nullptr;
// The world's surfaces (2026-10-22, user: "online, an arrow that hits a wall shows no effect"): each frame of the flight
// the arrow asks the game's own ray test (world_ray.hpp, cast on the game's thread inside its own client ray) along its
// way, 0.15 s ahead, and stops where the newest answer puts a wall, the ground or a building - once it has passed that
// place. The level ground (flightReach) is used only while no answer has come. The place goes to the host
// (flightSurface: online_sync.hpp, kind 8), whose parked arrow is put just before it and hits it there - the game's own
// effect and the arrow in the wall, for everyone.
inline uint64_t (*flightAsk)(const float*,const float*)=nullptr;
inline bool (*flightAnswer)(uint64_t&,bool&,std::array<int64_t,3>&)=nullptr;
inline void (*flightSurface)(const std::array<int64_t,3>&,Vec3)=nullptr;
inline WorldPositionQ32 moved(const WorldPositionQ32& p,const Vec3& m){return {p.x+int64_t(double(m.x)*4294967296.),p.y+int64_t(double(m.y)*4294967296.),p.z+int64_t(double(m.z)*4294967296.)};}
// The flight's frame: true while the clone is shown flying (or stuck where it hit).
inline bool flyFrame(uint64_t now,uint64_t currentSerial){
    controllerUiFixture::BowAim b;{std::lock_guard<std::mutex> lock(controllerUiFixture::fishingAimMutex);b=controllerUiFixture::bowAim;}
    if(b.serial!=aimSeen){aimSeen=b.serial;
        const bool client=nativeImpactComponents::clientNow(now);
        const bool wanted=flightMode==2||(flightMode==1&&(client||liveFrame.frame>=localFlightFrom));
        // the release of this bow's string (a throw sets the same aim: not an arrow)
        if(wanted&&b.serial&&handle&&haveRelative&&lastHeldMs&&now>=lastHeldMs&&now-lastHeldMs<=300){
            Quat q;std::memcpy(&q,b.aim.data()+24,16);q=normalized(q);
            flight={};flight.on=true;flight.serial=b.serial;flight.start=now;flight.transform=lastTransform;flight.dir=rotate(q,{0,0,1});
            flight.lookOnly=flight.reported=flightMode==1&&!client;   // the host's and a single player's: the look only
            {std::lock_guard<std::mutex> lock(controllerUiFixture::gestureMutex);flight.item=controllerUiFixture::gestureRangedItem;}
            {const float bow=evr::motion::catalogBowSpeed(flight.item);flight.speed=flightSpeedSet>0?flightSpeedSet:bow>0?bow:90.f;}
            // the strong shot's arrow does not drop (its template has no gravity component; the user: "it does not fall")
            {const auto strong=controllerUiFixture::strongShotAt.load();flight.strong=strong&&now>=strong&&now-strong<=1500;flight.gravity=flight.strong?0.f:flightGravity;}
            std::memcpy(&flight.rotation,lastTransform.data()+24,16);flight.rotation=normalized(flight.rotation);
            WorldPositionQ32 p0;std::memcpy(&p0,lastTransform.data(),24);flight.last={p0.x,p0.y,p0.z};
            if(flightReach){const float r=flightReach(p0,flight.dir,now);if(std::isfinite(r)&&r>0)flight.reach=r;}
            if(flightLogs++<300)logFile->event("arrow_flight_start","\"frame\":"+std::to_string(liveFrame.frame)+",\"serial\":"+std::to_string(b.serial)+",\"item\":"+std::to_string(flight.item)+",\"speed\":"+std::to_string(flight.speed)+",\"strong\":"+(flight.strong?"true":"false")+
                ",\"gravity\":"+std::to_string(flight.gravity)+",\"look_only\":"+(flight.reported?"true":"false")+",\"reach\":"+std::to_string(flight.reach)+",\"dir\":["+std::to_string(flight.dir.x)+","+std::to_string(flight.dir.y)+","+std::to_string(flight.dir.z)+"]");}}
    if(!flight.on||!handle)return false;
    const double t=double(now-flight.start)/1000.0;
    // the look only: where it stuck it stays until the game's own arrows come back in sight (foreignArrows, 2 s from the release) -
    // gone at 1 s, nothing showed where the arrow had hit for a second (Mock 20261026-155047)
    if(t>1.5&&!(flight.lookOnly&&flight.stuck&&t<=2.0)){flight.on=false;return false;}
    if(flight.stuck&&!flight.lookOnly&&now-flight.stuckAt>1000){flight.on=false;return false;}
    WorldPositionQ32 p0;std::memcpy(&p0,flight.transform.data(),24);
    auto transform=flight.transform;
    if(!flight.stuck){
        Vec3 disp=flight.dir*float(flight.speed*t);disp.y-=float(.5*flight.gravity*t*t);
        Vec3 vel=flight.dir*flight.speed;vel.y-=float(flight.gravity*t);
        auto p=moved(p0,disp);std::array<int64_t,3> now3{p.x,p.y,p.z};
        float along=0;std::array<int64_t,3> at{};bool character=false;uint32_t entity=0;
        const auto rel=[](const std::array<int64_t,3>& a,const std::array<int64_t,3>& b){return Vec3{float(double(a[0]-b[0])/4294967296.),float(double(a[1]-b[1])/4294967296.),float(double(a[2]-b[2])/4294967296.)};};
        const Vec3 seg=rel(now3,flight.last);const float segLength=length(seg),speedNow=length(vel);const Vec3 heading=speedNow>1e-3f?vel*(1/speedNow):flight.dir;
        if(flightAsk&&flightAnswer){
            uint64_t serial=0;bool wall=false;std::array<int64_t,3> place{};
            if(flightAnswer(serial,wall,place)&&flight.firstAsk&&serial>=flight.firstAsk&&serial>flight.lastAnswer){flight.lastAnswer=serial;flight.rayHeard=true;flight.wallKnown=wall;if(wall)flight.wall=place;}
            const double q=1.0/4294967296.0;const Vec3 ahead=heading*std::max(speedNow*.15f,2.f);
            const float from[3]{float(double(p.x)*q),float(double(p.y)*q),float(double(p.z)*q)},to[3]{from[0]+ahead.x,from[1]+ahead.y,from[2]+ahead.z};
            const auto asked=flightAsk(from,to);if(!flight.firstAsk)flight.firstAsk=asked;}
        float wallAlong=2;   // where on this frame's way the known surface lies (0..1) once the arrow is past it; 2: not
        if(flight.wallKnown&&dot(rel(now3,flight.wall),heading)>=0)wallAlong=segLength>1e-4f?std::clamp(dot(rel(flight.wall,flight.last),seg)/(segLength*segLength),0.f,1.f):0.f;
        const bool ground=!flight.rayHeard&&length(disp)>=flight.reach;
        const bool creature=flightHit&&flightHit(flight.last,now3,along,at,character,entity);
        if(!(creature&&along<=wallAlong)&&wallAlong<=1){
            const WorldPositionQ32 w{flight.wall[0],flight.wall[1],flight.wall[2]};p=moved(w,heading*-.05f);flight.stuck=true;flight.stuckAt=now;
            if(flightSurface&&!flight.reported){flight.reported=true;flightSurface(flight.wall,heading);}
            if(flightLogs++<300)logFile->event("arrow_flight_surface","\"frame\":"+std::to_string(liveFrame.frame)+",\"seconds\":"+std::to_string(t)+",\"answers\":"+std::to_string(flight.lastAnswer-flight.firstAsk+1)+
                ",\"heading\":["+std::to_string(heading.x)+","+std::to_string(heading.y)+","+std::to_string(heading.z)+"]");
        }else if(creature){
            const WorldPositionQ32 l{flight.last[0],flight.last[1],flight.last[2]};p=moved(l,seg*along);flight.stuck=true;flight.stuckAt=now;
            if(flightReport&&!flight.reported){flight.reported=true;flightReport(at,character,entity);}
            if(flightLogs++<300)logFile->event("arrow_flight_hit","\"frame\":"+std::to_string(liveFrame.frame)+",\"entity\":"+std::to_string(entity)+",\"character\":"+(character?"true":"false")+",\"seconds\":"+std::to_string(t));
        }else if(ground){flight.stuck=true;flight.stuckAt=now;
            if(flightLogs++<300)logFile->event("arrow_flight_ground","\"frame\":"+std::to_string(liveFrame.frame)+",\"seconds\":"+std::to_string(t)+",\"reach\":"+std::to_string(flight.reach));}
        const auto rotation=speedNow>1e-3f?normalized(multiply(motion::alignDirections(flight.dir,heading),flight.rotation)):flight.rotation;
        std::memcpy(transform.data(),&p,24);std::memcpy(transform.data()+24,&rotation,16);
        flight.last={p.x,p.y,p.z};
        if(flight.stuck)flight.transform=transform;   // it stays where it stopped
    }
    std::array<uintptr_t,2> ids{reinterpret_cast<uintptr_t>(&handle),1},poses{reinterpret_cast<uintptr_t>(transform.data()),1};
    motionVisual::change(reinterpret_cast<void*>(scene),ids.data(),poses.data());
    const auto clone=motionVisual::object(handle);
    if(read<uintptr_t>(clone+layout::modelResource)!=resource||read<uint32_t>(clone+layout::modelMask)||read<uint32_t>(clone+layout::modelSdf)||read<uint32_t>(clone+layout::modelPose))throw std::runtime_error("flying arrow clone invariant failed");
    serial=currentSerial;frame=liveFrame.frame;ready=true;return true;
}
constexpr float findRange=.35f,keepRange=.6f,nockedRange=.15f;
inline double metres(const WorldPositionQ32& a,const WorldPositionQ32& b){
    const double x=relativeQ32(a.x,b.x),y=relativeQ32(a.y,b.y),z=relativeQ32(a.z,b.z);return std::sqrt(x*x+y*y+z*z);
}
inline Vec3 offset(const WorldPositionQ32& a,const WorldPositionQ32& b){return {float(relativeQ32(a.x,b.x)),float(relativeQ32(a.y,b.y)),float(relativeQ32(a.z,b.z))};}
// Visible unposed models next to the game's bow. The Scene fills its model table from the low indices, so the walk
// ends after eight empty blocks.
inline std::vector<Seen> scan(const WorldPositionQ32& bow){
    std::vector<Seen> out;static std::vector<uint8_t> block;
    const auto objects=read<uintptr_t>(scene+0x1b0),generations=read<uintptr_t>(scene+0x1c8);
    const auto total=size_t(std::min<uint64_t>(std::min<uint64_t>(read<uint64_t>(scene+0x1b8)/0x1b0,read<uint64_t>(scene+0x1d0)),65536));
    constexpr size_t step=2048;block.resize(step*0x1b0);unsigned empty=0;
    for(size_t first=0;first<total&&empty<8&&out.size()<8;first+=step){
        const auto count=std::min<size_t>(step,total-first);SIZE_T got=0;
        if(!ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(objects+first*0x1b0),block.data(),count*0x1b0,&got)||got!=count*0x1b0)break;
        bool any=false;
        for(size_t n=0;n<count&&out.size()<8;++n){const auto* object=block.data()+n*0x1b0;
            uintptr_t res=0;uint32_t pose=0,mask=0;WorldPositionQ32 position;std::memcpy(&res,object+layout::modelResource,8);if(!res)continue;
            any=true;std::memcpy(&pose,object+layout::modelPose,4);std::memcpy(&mask,object+layout::modelMask,4);std::memcpy(&position,object+0x38,24);
            if(pose||!mask||!(metres(position,bow)<findRange))continue;
            try{if(read<uint8_t>(res+0x14)!=1)continue;
                out.push_back({(uint32_t(read<uint8_t>(generations+first+n))<<24)|uint32_t(first+n),position});
            }catch(const std::exception&){}
        }
        empty=any?0:empty+1;
    }
    ++scans;return out;
}
inline void retire(const char* event,const std::string& detail){
    const auto clone=motionVisual::object(handle);
    if(read<uintptr_t>(clone+layout::modelResource)!=resource||read<uint32_t>(clone+layout::modelMask)||read<uint32_t>(clone+layout::modelSdf))throw std::runtime_error("arrow retirement invariant differs");
    const auto id=handle,refs=read<uint32_t>(resource+16);std::array<uintptr_t,2> ids{reinterpret_cast<uintptr_t>(&id),1};
    reinterpret_cast<motionVisual::Remove>(baseAddress+0x10e000)(reinterpret_cast<void*>(scene),ids.data());handle=0;ready=false;
    const bool invalid=read<uint8_t>(read<uintptr_t>(scene+0x1c8)+(id&0xffffff))!=uint8_t(id>>24);
    const bool released=refs==1||(refs>1&&read<uint32_t>(resource+16)==refs-1);
    logFile->event(event,"\"handle\":"+std::to_string(id)+",\"updates\":"+std::to_string(updates)+",\"generation_invalidated\":"+(invalid?"true":"false")+
        ",\"reference_released\":"+(released?"true":"false")+detail);
    if(!invalid||!released)throw std::runtime_error("arrow retirement failed");
}
// The game's arrows of the own shot (2026-10-22; user after the online session of 10-22: "an arrow flying diagonally up
// can be seen, and the shot is late"). A guest's arrow is made by the host's server: the guest's log of that session showed,
// 0.35..0.44 s after each release, an arrow 300 m above the player - where the host puts it out of sight
// (live_melee_targets.hpp steerArrows) - while the own clone had flown from the release (arrow_flight_start). The game's
// copy reaching the guest that late, and going up to where the host put it, was the arrow seen. For 2 s from the release
// the visible unposed models of the arrow's model other than the clone and the arrow at the bow are kept out of the eyes
// (head_visibility_bridge.hpp), looked for every 100 ms (250 once found); none on the player's body (a quiver) or within 0.8 m of the bow.
// Only where the own flight runs (a guest; since 2026-10-26 the host and a single player too, localFlightFrom). EVR_HIDE_GAME_ARROWS=0: shown.
inline const bool hideGameArrows=[]{char v[8]{};const auto n=GetEnvironmentVariableA("EVR_HIDE_GAME_ARROWS",v,sizeof v);return !(n==1&&v[0]=='0');}();
inline std::array<uint32_t,6> foreign{};inline size_t foreignCount=0;inline uint64_t foreignScan=0,foreignSerial=0,foreignFrame=0,foreignShot=0;inline unsigned foreignLogs=0,foreignSeen=0;
inline void foreignArrows(uint64_t now,uint64_t currentSerial,const WorldPositionQ32& bow){
    if(!hideGameArrows||!flightMode||!scene||!flight.start||now<flight.start||now-flight.start>2000||!(resource||sourceResource)){foreignCount=0;return;}
    if(foreignShot!=flight.serial){foreignShot=flight.serial;foreignSeen=0;foreignCount=0;}
    // every 100 ms, every 250 ms once the shot's copy is found: a look reads the Scene's model table to its end (some MB in a
    // big world, on the render thread)
    // every frame for the first 0.7 s (2026-10-22, user after 0.9.8: "the arrow shot diagonally up shows more" - the host's
    // mod without VR now puts the guest's arrows 300 m up, and the copy is drawn on its way there between two looks)
    if(now-flight.start<=700||now-foreignScan>=(foreignSeen?250u:100u))try{foreignScan=now;   // (review W9: a failed read ends this look only)
        rootCamera::Sample root;const bool body=rootCamera::trySnapshot(root)&&root.number;
        const auto objects=read<uintptr_t>(scene+0x1b0),generations=read<uintptr_t>(scene+0x1c8);
        const auto total=size_t(std::min<uint64_t>(std::min<uint64_t>(read<uint64_t>(scene+0x1b8)/0x1b0,read<uint64_t>(scene+0x1d0)),65536));
        static std::vector<uint8_t> block;constexpr size_t step=2048;block.resize(step*0x1b0);unsigned empty=0;size_t found=0;double nearestClone=-1;
        WorldPositionQ32 clonePosition{};const bool clone=handle&&motionVisual::currentObject(handle);if(clone)clonePosition=read<WorldPositionQ32>(motionVisual::object(handle)+0x38);
        for(size_t first=0;first<total&&empty<8&&found<foreign.size();first+=step){
            const auto count=std::min<size_t>(step,total-first);SIZE_T got=0;
            if(!ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(objects+first*0x1b0),block.data(),count*0x1b0,&got)||got!=count*0x1b0)break;
            bool any=false;
            for(size_t n=0;n<count&&found<foreign.size();++n){const auto* object=block.data()+n*0x1b0;
                uintptr_t res=0;uint32_t pose=0,mask=0;WorldPositionQ32 position;std::memcpy(&res,object+layout::modelResource,8);if(!res)continue;
                any=true;if(res!=resource&&res!=sourceResource)continue;
                std::memcpy(&pose,object+layout::modelPose,4);std::memcpy(&mask,object+layout::modelMask,4);std::memcpy(&position,object+0x38,24);
                if(pose||!mask)continue;
                const uint32_t h=(uint32_t(read<uint8_t>(generations+first+n))<<24)|uint32_t(first+n);
                if(h==handle||h==source||metres(position,bow)<.8)continue;
                if(body){const double dx=relativeQ32(position.x,root.stable.position.x),dy=relativeQ32(position.y,root.stable.position.y),dz=relativeQ32(position.z,root.stable.position.z);
                    if(dx*dx+dz*dz<.7*.7&&dy>-.3&&dy<2.2)continue;}
                foreign[found++]=h;
                if(clone){const double d=metres(position,clonePosition);if(nearestClone<0||d<nearestClone)nearestClone=d;}
            }
            empty=any?0:empty+1;
        }
        foreignCount=found;
        if(found&&!foreignSeen&&foreignLogs++<300){foreignSeen=1;
            logFile->event("game_arrow_hidden","\"frame\":"+std::to_string(liveFrame.frame)+",\"shot\":"+std::to_string(flight.serial)+",\"after_release_ms\":"+std::to_string(now-flight.start)+
                ",\"count\":"+std::to_string(found)+",\"nearest_to_clone_m\":"+std::to_string(nearestClone)+",\"same_model\":"+(resource==sourceResource?"true":"false"));}
    }catch(const std::exception& x){foreignCount=0;static unsigned told=0;if(told++<5)try{logFile->event("game_arrow_hide_failure","\"reason\":"+jsonString(x.what()));}catch(...){}}
    catch(...){foreignCount=0;}
    else{   // between looks: those still there
        size_t kept=0;for(size_t k=0;k<foreignCount;++k){const auto o=motionVisual::currentObject(foreign[k]);if(!o)continue;const auto res=read<uintptr_t>(o+layout::modelResource);
            if(res==resource||res==sourceResource)foreign[kept++]=foreign[k];}
        foreignCount=kept;}
    // the game's nocked arrow of this shot, left where the original model holds it after the release (lostSource)
    if(lostSource&&lostSource!=source&&now>=lostSourceAt&&now-lostSourceAt<=1500&&lostSourceAt>=flight.start&&foreignCount<foreign.size()&&motionVisual::currentObject(lostSource)&&
       std::find(foreign.begin(),foreign.begin()+ptrdiff_t(foreignCount),lostSource)==foreign.begin()+ptrdiff_t(foreignCount))foreign[foreignCount++]=lostSource;
    foreignSerial=currentSerial;foreignFrame=liveFrame.frame;
}
inline void update(const game::StableAnchor& anchor,uint64_t currentSerial){
    const auto& g=motionVisual::bowGeometry;const auto& string=motionBowVisual::drawnString;
    if(!motionVisual::poseReady||motionVisual::updatedSerial!=currentSerial||motionVisual::updatedFrame!=liveFrame.frame||
       !g.binding||g.frame!=shownHands.frame||string.serial!=currentSerial||string.frame!=liveFrame.frame)return;
    if(scene&&(scene!=motionVisual::scene||thread!=GetCurrentThreadId()))throw std::runtime_error("arrow Scene/thread changed");
    scene=motionVisual::scene;thread=GetCurrentThreadId();
    const auto now=GetTickCount64();
    const auto bow=read<std::array<uint8_t,56>>(motionVisual::object(motionVisual::sourceHandle)+0x38);
    WorldPositionQ32 bowPosition;Quat bowOrientation;std::memcpy(&bowPosition,bow.data(),24);std::memcpy(&bowOrientation,bow.data()+24,16);
    bowOrientation=normalized(bowOrientation);
    if(!lastSeenBow||now-lastSeenBow>500)lookFastUntil=now+2000;lastSeenBow=now;
    foreignArrows(now,currentSerial,bowPosition);
    if(source){
        const auto object=motionVisual::currentObject(source);
        if(!object||read<uintptr_t>(object+layout::modelResource)!=sourceResource||read<uint32_t>(object+layout::modelPose)||!(metres(read<WorldPositionQ32>(object+0x38),bowPosition)<keepRange)){
            // Shot, put away or replaced: the instance is no longer the arrow on the game's bow.
            logFile->event("motion_arrow_source_lost","\"source_handle\":"+std::to_string(source)+",\"frame\":"+std::to_string(liveFrame.frame)+",\"removed\":"+(object?"false":"true"));
            lostSource=object?source:0;lostSourceAt=now;source=0;++losses;lookFastUntil=now+2000;
        }
    }
    // the nocked arrow just lost (a shot, or the bow put away): out of the eyes for a second more, while it still exists
    if(lostSource&&lostSource!=source&&now>=lostSourceAt&&now-lostSourceAt<=1000&&motionVisual::currentObject(lostSource)){
        if(lostHiddenSerial+1<currentSerial||!lostHiddenSerial){static unsigned logs=0;if(logs++<300)logFile->event("game_nocked_arrow_hidden","\"frame\":"+std::to_string(liveFrame.frame)+",\"handle\":"+std::to_string(lostSource)+",\"after_loss_ms\":"+std::to_string(now-lostSourceAt));}
        lostHiddenSerial=currentSerial;lostHiddenFrame=liveFrame.frame;}
    else lostSource=0;
    // Looked for while the bow is out, not only while the string is held: the game shows its arrow on the hand of the
    // original model as soon as the bow is in the hand and again after each shot, and there it floated at the hip
    // (headset run 20261014-124515, user: "taking the bow from the left shoulder, an arrow floats at about the waist;
    // the original model holds it probably"). Without the string in the hand the look is slower.
    if(!source&&now-lastScan>=(string.held||now<lookFastUntil?60u:250u)){
        lastScan=now;auto found=scan(bowPosition);
        const auto attached=attachmentObserver::heldHandles(g.entity,scene,now,true);
        found.erase(std::remove_if(found.begin(),found.end(),[&](const Seen& s){return s.handle==motionVisual::sourceHandle||s.handle==handle||
            std::find(attached.begin(),attached.end(),s.handle)!=attached.end();}),found.end());
        // The arrow moves with the hand that holds it; scenery that happens to stand at the bow does not.
        const Seen* pick=nullptr;double best=findRange;
        // An arrow of the model the game showed on its bow before is taken at the first look (headset run
        // 20261014-162900, user: "taking the bow from the shoulder, for an instant the arrow still shows at the belly":
        // the second look, for movement with the bow, left it in sight that long).
        if(sourceResource)for(const auto& f:found)if(read<uintptr_t>(motionVisual::object(f.handle)+layout::modelResource)==sourceResource){
            const double d=metres(f.position,bowPosition);if(d<best){best=d;pick=&f;}}
        if(pick)++knownTaken;
        else if(candidatesAt&&now-candidatesAt<=400)for(const auto& f:found)for(const auto& c:candidates)if(c.handle==f.handle&&std::memcmp(&c.position,&f.position,24)){
            const double d=metres(f.position,bowPosition);if(d<best){best=d;pick=&f;}}
        if(pick){
            const auto object=motionVisual::object(pick->handle),res=read<uintptr_t>(object+layout::modelResource);
            const auto guid=read<std::array<uint32_t,4>>(res+0x18);
            source=pick->handle;sourceResource=res;++adoptions;
            if(measuredResource!=res){measured=false;measuredResource=0;}
            logFile->event("motion_arrow_source","\"source_handle\":"+std::to_string(source)+",\"frame\":"+std::to_string(liveFrame.frame)+",\"metres\":"+std::to_string(best)+
                ",\"model\":"+std::to_string(read<uint16_t>(res+layout::resourceModel))+",\"guid\":["+std::to_string(guid[0])+","+std::to_string(guid[1])+","+std::to_string(guid[2])+","+std::to_string(guid[3])+
                "],\"candidates\":"+std::to_string(found.size())+",\"scans\":"+std::to_string(scans)+",\"moves_with_bow\":true,\"read_only\":true");
            candidates.clear();candidatesAt=0;
        }else{candidates=std::move(found);candidatesAt=now;}
    }
    if(string.held&&!wasHeld)heldSince=now;wasHeld=string.held;if(string.held)lastHeldMs=now;
    if(string.held&&flight.on)flight.on=false;   // a new draw: the string's arrow again
    if(!string.held&&flightMode&&scene&&flyFrame(now,currentSerial)){if(source){hiddenSerial=currentSerial;hiddenFrame=liveFrame.frame;}return;}   // the game's arrow at the bow stays out of sight
    if(!source){
        // (the strong shot's draw, by the right grip, aims as the trigger's since 2026-10-24: the game's own arrow comes as with it)
        if(earlyWanted&&string.held&&handle&&measured&&measuredResource==resource&&haveRelative&&heldSince&&now-heldSince<=earlyMs&&anchor.worldUnitsPerMeter==1){
            const auto facing=normalized(anchor.orientation);
            const auto origin=cameraPosition(anchor.root,rotate(facing,anchor.cameraOffsetGameUnits),1);
            auto world=[&](Vec3 tracking){return cameraPosition(origin,rotate(facing,game::trackingPose(compose(liveTrackingToAnchor,Pose{{0,0,0,1},tracking})).position),anchor.worldUnitsPerMeter);};
            const auto& rest=motion::BowPhysicalTrial::restString;
            const auto nock=world(string.hand),arrowRest=world(g.model.position+rotate(g.model.orientation,{0,rest.y*g.scale.y,0}));
            auto shot=offset(arrowRest,nock);const float span=length(shot);
            if(!std::isfinite(span)||span<.05f)return;
            shot=shot*(1/span);
            const auto transplanted=normalized(multiply(g.worldOrientation,relative));
            const auto rotation=normalized(multiply(motion::alignDirections(rotate(transplanted,axis),shot),transplanted));
            const auto position=cameraPosition(nock,shot*-nockDistance,1);
            auto transform=lastTransform;std::memcpy(transform.data(),&position,24);std::memcpy(transform.data()+24,&rotation,16);
            std::array<uintptr_t,2> ids{reinterpret_cast<uintptr_t>(&handle),1},poses{reinterpret_cast<uintptr_t>(transform.data()),1};
            motionVisual::change(reinterpret_cast<void*>(scene),ids.data(),poses.data());
            const auto clone=motionVisual::object(handle);
            if(read<uintptr_t>(clone+layout::modelResource)!=resource||read<uint32_t>(clone+layout::modelMask)||read<uint32_t>(clone+layout::modelSdf)||read<uint32_t>(clone+layout::modelPose))throw std::runtime_error("early arrow clone invariant failed");
            serial=currentSerial;frame=liveFrame.frame;ready=true;++earlyUpdates;
            if(earlyLogs<200&&now-heldSince<=20){++earlyLogs;logFile->event("motion_arrow_early","\"frame\":"+std::to_string(liveFrame.frame)+",\"handle\":"+std::to_string(handle)+",\"held_ms\":"+std::to_string(now-heldSince)+",\"early_updates\":"+std::to_string(earlyUpdates));}
        }
        return;
    }
    hiddenSerial=currentSerial;hiddenFrame=liveFrame.frame;
    const auto object=motionVisual::object(source);
    const auto original=read<std::array<uint8_t,432>>(object);
    if(!string.held||!read<uint32_t>(object+layout::modelMask))return;   // nothing on the player's string
    auto transform=read<std::array<uint8_t,56>>(object+0x38);
    WorldPositionQ32 arrowPosition;Quat arrowOrientation;std::memcpy(&arrowPosition,transform.data(),24);std::memcpy(&arrowOrientation,transform.data()+24,16);
    arrowOrientation=normalized(arrowOrientation);
    if(anchor.worldUnitsPerMeter!=1)throw std::runtime_error("arrow world scale differs");
    // The game's bow shoots along its +Z and draws its string to -Z (bow_physical_trial.hpp, reflected from XR).
    const auto nativeShot=rotate(bowOrientation,{0,0,1});
    const auto& rest=motion::BowPhysicalTrial::restString;
    const Vec3 nativeString{0,rest.y*g.scale.y,-(rest.z+motionVisual::bowDrawProgress*motion::knownBowDrawKeys.back().pull)*g.scale.z};
    const auto fromNock=offset(arrowPosition,bowPosition)-rotate(bowOrientation,nativeString);
    const float along=-dot(fromNock,nativeShot),lateral=length(fromNock+nativeShot*along);
    const auto shotInArrow=rotate(inverse(arrowOrientation),nativeShot);
    if(measuredResource!=sourceResource){
        // Same descriptor as the bow profile reads: extent minimum at +16, maximum at +32 (trace_model_catalog.py).
        const auto model=read<uintptr_t>(sourceResource+layout::resourceDescriptor);const auto lo=read<Vec3>(model+16),hi=read<Vec3>(model+32),scale=read<Vec3>(object+0x38+40);
        const std::array<float,3> size{hi.x-lo.x,hi.y-lo.y,hi.z-lo.z},low{lo.x,lo.y,lo.z},high{hi.x,hi.y,hi.z},shotAxis{shotInArrow.x,shotInArrow.y,shotInArrow.z},scales{scale.x,scale.y,scale.z};
        const size_t k=size[0]>size[1]?(size[0]>size[2]?0:2):(size[1]>size[2]?1:2),a=(k+1)%3,b=(k+2)%3;
        const bool arrowShape=finite(lo)&&finite(hi)&&finite(scale)&&size[k]>.3f&&size[k]<2.5f&&size[a]<size[k]*.25f&&size[b]<size[k]*.25f&&scales[k]>.5f&&scales[k]<1.5f;
        // Not along the game's bow yet: its hand is still bringing the arrow to the string.
        if(arrowShape&&std::abs(shotAxis[k])>=.866f){
            const float sign=shotAxis[k]>0?1.f:-1.f;axis={k==0?sign:0.f,k==1?sign:0.f,k==2?sign:0.f};
            nockDistance=(sign>0?low[k]:-high[k])*scales[k];measured=extentKnown=true;measuredResource=sourceResource;
        }else if(!arrowShape&&lateral<nockedRange&&std::abs(along)<1.5f&&motionVisual::bowDrawProgress>=.99f){
            Vec3 snapped=shotInArrow;const float ax=std::abs(shotInArrow.x),ay=std::abs(shotInArrow.y),az=std::abs(shotInArrow.z);
            if(ax>=.866f)snapped={shotInArrow.x>0?1.f:-1.f,0,0};else if(ay>=.866f)snapped={0,shotInArrow.y>0?1.f:-1.f,0};else if(az>=.866f)snapped={0,0,shotInArrow.z>0?1.f:-1.f};
            nockDistance=along;axis=snapped;measured=true;extentKnown=false;measuredResource=sourceResource;
        }
        if(measuredResource==sourceResource){
            std::ostringstream row;row<<std::setprecision(6)<<"\"source_handle\":"<<source<<",\"frame\":"<<liveFrame.frame<<",\"extent_min\":["<<lo.x<<','<<lo.y<<','<<lo.z<<"],\"extent_max\":["<<hi.x<<','<<hi.y<<','<<hi.z
                <<"],\"model_axis\":["<<axis.x<<','<<axis.y<<','<<axis.z<<"],\"nock_distance\":"<<nockDistance<<",\"from_model_extent\":"<<(extentKnown?"true":"false")
                <<",\"native_along\":"<<along<<",\"native_lateral\":"<<lateral<<",\"native_progress\":"<<motionVisual::bowDrawProgress;
            logFile->event("motion_arrow_model",row.str());
        }
    }
    if(!measured)return;
    // The player's bow: string point at the drawing hand as shown, arrow rest on the handle (bow_draw.hpp).
    const auto facing=normalized(anchor.orientation);
    const auto origin=cameraPosition(anchor.root,rotate(facing,anchor.cameraOffsetGameUnits),1);
    auto world=[&](Vec3 tracking){return cameraPosition(origin,rotate(facing,game::trackingPose(compose(liveTrackingToAnchor,Pose{{0,0,0,1},tracking})).position),anchor.worldUnitsPerMeter);};
    const auto nock=world(string.hand),arrowRest=world(g.model.position+rotate(g.model.orientation,{0,rest.y*g.scale.y,0}));
    auto shot=offset(arrowRest,nock);const float span=length(shot);
    if(!std::isfinite(span)||span<.05f)return;
    shot=shot*(1/span);
    const auto transplanted=normalized(multiply(g.worldOrientation,multiply(inverse(bowOrientation),arrowOrientation)));
    const auto rotation=normalized(multiply(motion::alignDirections(rotate(transplanted,axis),shot),transplanted));
    const auto position=cameraPosition(nock,shot*-nockDistance,1);
    std::memcpy(transform.data(),&position,24);std::memcpy(transform.data()+24,&rotation,16);
    relative=normalized(multiply(inverse(bowOrientation),arrowOrientation));haveRelative=true;lastTransform=transform;
    if(handle&&resource!=sourceResource){retire("motion_arrow_retired",",\"another_arrow\":true");++retired;}
    if(!handle){
        motionVisual::validate();const auto refs=read<uint32_t>(sourceResource+16);if(!refs||refs==UINT32_MAX)throw std::runtime_error("arrow resource reference bound");
        const auto guid=read<std::array<uint32_t,4>>(sourceResource+0x18);resource=sourceResource;
        handle=reinterpret_cast<motionVisual::Create>(baseAddress+0xfb4e0)(reinterpret_cast<void*>(scene),guid.data(),transform.data());
        if(!handle||handle==source)throw std::runtime_error("arrow creation failed/aliased");
        auto clone=motionVisual::object(handle);
        reinterpret_cast<motionVisual::Mask>(baseAddress+0xf70c0)(reinterpret_cast<void*>(scene),reinterpret_cast<void*>(clone),0);
        clone=motionVisual::object(handle);
        if(read<uintptr_t>(clone+layout::modelResource)!=resource||read<uint32_t>(resource+16)!=refs+1||read<uint32_t>(clone+layout::modelMask)||read<uint32_t>(clone+layout::modelAuxiliary)||read<uint32_t>(clone+layout::modelPose))throw std::runtime_error("arrow clone resource/pose differs");
        const auto sdfId=read<uint32_t>(clone+layout::modelSdf);
        if(sdfId){
            if(sdfId==read<uint32_t>(motionVisual::object(source)+layout::modelSdf))throw std::runtime_error("arrow SDF aliases source");
            const auto sdf=read<uintptr_t>(scene+layout::sceneSdf);const auto count=read<uint64_t>(sdf+0x78);if(!count)throw std::runtime_error("arrow SDF count");
            using RemoveSdf=void(__fastcall*)(void*,uint32_t);reinterpret_cast<RemoveSdf>(baseAddress+0x15a640)(reinterpret_cast<void*>(sdf),sdfId);
            *reinterpret_cast<uint32_t*>(motionVisual::object(handle)+layout::modelSdf)=0;
            if(read<uint64_t>(sdf+0x78)!=count-1)throw std::runtime_error("arrow SDF removal failed");
        }
        logFile->event("motion_arrow_created","\"handle\":"+std::to_string(handle)+",\"source_handle\":"+std::to_string(source)+",\"model\":"+std::to_string(read<uint16_t>(resource+layout::resourceModel))+
            ",\"references_before\":"+std::to_string(refs)+",\"references_after\":"+std::to_string(read<uint32_t>(resource+16))+",\"mask\":0,\"sdf\":0");
    }else{
        std::array<uintptr_t,2> ids{reinterpret_cast<uintptr_t>(&handle),1},poses{reinterpret_cast<uintptr_t>(transform.data()),1};
        motionVisual::change(reinterpret_cast<void*>(scene),ids.data(),poses.data());
    }
    const auto clone=motionVisual::object(handle);
    if(read<std::array<uint8_t,432>>(motionVisual::object(source))!=original||read<uintptr_t>(clone+layout::modelResource)!=resource||
       read<uint32_t>(clone+layout::modelMask)||read<uint32_t>(clone+layout::modelSdf)||read<uint32_t>(clone+layout::modelPose)||
       std::memcmp(read<std::array<uint8_t,56>>(clone+0x38).data(),transform.data(),52))throw std::runtime_error("arrow source/pose invariant failed");
    serial=currentSerial;frame=liveFrame.frame;ready=true;++updates;
    std::ostringstream out;out<<std::setprecision(6)<<"\"serial\":"<<serial<<",\"frame\":"<<frame<<",\"handle\":"<<handle<<",\"source_handle\":"<<source
        <<",\"q32\":["<<position.x<<','<<position.y<<','<<position.z<<"],\"orientation\":["<<rotation.x<<','<<rotation.y<<','<<rotation.z<<','<<rotation.w
        <<"],\"shot\":["<<shot.x<<','<<shot.y<<','<<shot.z<<"],\"string_to_rest\":"<<span<<",\"nock_distance\":"<<nockDistance<<",\"native_along\":"<<along<<",\"native_lateral\":"<<lateral
        <<",\"native_progress\":"<<motionVisual::bowDrawProgress<<",\"model_axis\":["<<axis.x<<','<<axis.y<<','<<axis.z<<"],\"from_model_extent\":"<<(extentKnown?"true":"false")<<",\"shot_in_arrow\":["<<shotInArrow.x<<','<<shotInArrow.y<<','<<shotInArrow.z
        <<"],\"bow_axis_agreement\":"<<dot(shot,rotate(g.worldOrientation,{0,0,1}))<<",\"source_unchanged\":true,\"mask\":0,\"sdf\":0";
    logFile->event("motion_arrow_pose",out.str());
}
// A surprise here must not take the frame's other preparations (body, effects) with it: the arrow is switched off
// for the session and the game's own arrow shows again.
inline void prepare(const game::StableAnchor& anchor,uint64_t currentSerial){
    if(!requested)return;ready=false;
    try{update(anchor,currentSerial);}
    catch(const std::exception& e){ready=false;source=0;requested=false;++failures;
        logFile->event("motion_arrow_failure","\"reason\":"+jsonString(e.what())+",\"frame\":"+std::to_string(liveFrame.frame)+",\"arrow_display_off\":true");}
}
inline void dispose(uintptr_t s){
    if(!scene||s!=scene)return;
    if(handle){
        if(thread!=GetCurrentThreadId())throw std::runtime_error("arrow disposal thread differs");
        retire("motion_arrow_disposed",",\"adoptions\":"+std::to_string(adoptions)+",\"scans\":"+std::to_string(scans)+",\"scene_disposal\":true");
    }
    source=0;candidates.clear();candidatesAt=0;scene=0;
}
}
