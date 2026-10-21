// Diagnostic, read-only (2026-10-21; user: "what the guest does is decided on the guest's PC - the sparks and the sound
// too, without waiting for the host"): which entities with an effect, a sound or a notifier appear in a World and when,
// so that a melee hit's sparks and sound can be told apart: made by the server and replicated to the client (then a
// guest sees them a round trip late), or made by the client itself. Every 100 ms the World's entity map is read; an id
// not seen before is looked at: its components keen::ecs::NotifierComponent (sfx +60, vfx +76, notifier +40),
// VfxComponent, AudioComponent, and where it stands (CurrentTransform). Two Worlds: the camera's (the client's) and the
// one the live targets read (the server's in a single player game). EVR_EFFECT_CENSUS=1: every 100 ms, both Worlds.
// As a guest of an online game (live_melee_targets.hpp guestWorld) it runs by itself, lightly - the client's World only,
// every 500 ms, 1500 records at the most - so that the next online session tells whether the host's hits reach the guest
// as such entities (EVR_EFFECT_CENSUS=0: never).
namespace effectEntityCensus {
inline int state=-1;inline uint64_t lastAt=0;inline unsigned logged=0;
struct WorldState {uintptr_t world=0,registry=0;int notifier=-1,vfx=-1,audio=-1,current=-1,velocity=-1,gravity=-1;unsigned notifierBytes=0,vfxBytes=0,audioBytes=0;
    std::unordered_map<uint32_t,uint8_t> known;bool primed=false;uint64_t passes=0;};
inline std::array<WorldState,2> worlds;
// Flying things followed after they are first seen (2026-10-21, for the guest's own arrow and bomb flights: the game's
// speed, gravity and whether the speed falls off in the air): their place and speed each pass for 3 s, and when they go.
struct Followed {uintptr_t world=0;uint32_t entity=0;uint64_t since=0,last=0;WorldPositionQ32 start{};const char* label="";};
inline std::array<Followed,8> followed{};inline unsigned followLogs=0;
inline std::vector<uint32_t> keys;inline std::vector<uint64_t> bits;
inline void learn(WorldState& w,uintptr_t world){
    w={};w.world=world;w.registry=read<uintptr_t>(world);
    for(unsigned i=0;i<nativeImpactComponents::componentCapacity;++i){nativeImpactComponents::ComponentType t;
        try{t=nativeImpactComponents::componentType(world,i);}catch(const std::exception&){break;}
        const auto& n=t.qualifiedName;
        if(n=="keen::ecs::NotifierComponent"){w.notifier=int(i);w.notifierBytes=t.bytes;}
        else if(n=="keen::ecs::VfxComponent"){w.vfx=int(i);w.vfxBytes=t.bytes;}
        else if(n=="keen::ecs::AudioComponent"){w.audio=int(i);w.audioBytes=t.bytes;}
        else if(n=="keen::ecs::CurrentTransform"&&t.bytes==56)w.current=int(i);
        else if(n=="keen::ecs::Velocity"&&t.bytes==12)w.velocity=int(i);
        else if(n=="keen::ecs::Gravity"&&t.bytes==20)w.gravity=int(i);}
    logFile->event("effect_census_world","\"world\":"+hex(world)+",\"notifier\":"+std::to_string(w.notifier)+",\"notifier_bytes\":"+std::to_string(w.notifierBytes)+
        ",\"vfx\":"+std::to_string(w.vfx)+",\"vfx_bytes\":"+std::to_string(w.vfxBytes)+",\"audio\":"+std::to_string(w.audio)+",\"audio_bytes\":"+std::to_string(w.audioBytes)+",\"current\":"+std::to_string(w.current));
}
inline bool has(const std::array<uint64_t,10>& b,int id){return id>=0&&(b[unsigned(id)/64]>>(unsigned(id)%64)&1);}
inline void pass(WorldState& w,uintptr_t world,const char* label,const WorldPositionQ32& origin){
    if(!world)return;
    if(w.world!=world||w.registry!=read<uintptr_t>(world))learn(w,world);
    const auto map=world+0x148;const auto bitmap=read<uintptr_t>(map+8),capacity=read<uint64_t>(map+0x10),keyArray=read<uintptr_t>(map+0x28),values=read<uintptr_t>(map+0x40);
    if(!bitmap||!keyArray||!values||!capacity||capacity>0x100000||(capacity&(capacity-1)))return;
    keys.resize(size_t(capacity));bits.resize(size_t((capacity+63)/64));SIZE_T got=0;
    if(!ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(bitmap),bits.data(),bits.size()*8,&got)||got!=bits.size()*8)return;
    if(!ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(keyArray),keys.data(),keys.size()*4,&got)||got!=keys.size()*4)return;
    ++w.passes;std::unordered_map<uint32_t,uint8_t> now;now.reserve(w.known.size()+64);
    for(uint64_t i=0;i<capacity;++i){if(!(bits[size_t(i/64)]>>(i%64)&1))continue;const auto id=keys[size_t(i)];now.emplace(id,uint8_t(1));
        if(!w.primed||w.known.count(id))continue;
        try{const auto record=read<uintptr_t>(values+i*8);if(!record)continue;
            nativeImpactComponents::Entity e;e.world=world;e.record=record;e.layout=read<uintptr_t>(record+0x18);e.data=read<uintptr_t>(record+0x20);e.row=read<uint32_t>(record+0x30);
            if(!e.layout||!e.data)continue;const auto b=read<std::array<uint64_t,10>>(e.layout);
            const bool n=has(b,w.notifier),v=has(b,w.vfx),a=has(b,w.audio),moving=has(b,w.velocity)&&has(b,w.gravity);if(!n&&!v&&!a&&!moving)continue;
            std::ostringstream s;s<<std::setprecision(4)<<"\"world\":\""<<label<<"\",\"frame\":"<<liveFrame.frame<<",\"entity\":"<<id<<",\"notifier\":"<<n<<",\"vfx\":"<<v<<",\"audio\":"<<a;
            // A thing that flies (an arrow, a thrown item, a wand's shot): its velocity and gravity as first seen.
            if(moving){const auto vc=nativeImpactComponents::component(e,unsigned(w.velocity),12),gc=nativeImpactComponents::component(e,unsigned(w.gravity),20);
                if(vc&&gc){const auto vel=read<Vec3>(vc);const auto gdir=read<Vec3>(gc);const auto gval=read<float>(gc+12);const auto gon=read<uint8_t>(gc+16);
                    s<<",\"velocity\":["<<vel.x<<','<<vel.y<<','<<vel.z<<"],\"speed\":"<<length(vel)<<",\"gravity\":["<<gdir.x<<','<<gdir.y<<','<<gdir.z<<"],\"gravity_value\":"<<gval<<",\"gravity_on\":"<<unsigned(gon);}}
            if(has(b,w.current)){const auto c=nativeImpactComponents::component(e,unsigned(w.current),56);if(c){const auto p=read<WorldPositionQ32>(c);
                s<<",\"from_player_m\":["<<double(p.x-origin.x)/4294967296.0<<','<<double(p.y-origin.y)/4294967296.0<<','<<double(p.z-origin.z)/4294967296.0<<']';}}
            if(n&&w.notifierBytes>=92){const auto c=nativeImpactComponents::component(e,unsigned(w.notifier),w.notifierBytes);
                if(c)s<<",\"notifier_bytes\":"<<jsonString(nativeImpactComponents::asHex(read<std::array<uint8_t,92>>(c)));}
            int count=0;std::string names;for(unsigned t=0;t<nativeImpactComponents::componentCapacity&&count<24;++t)if(has(b,int(t))){
                try{auto name=nativeImpactComponents::componentType(world,t).qualifiedName;if(name.rfind("keen::ecs::",0)==0)name=name.substr(11);names+=(count?",":"")+jsonString(name);++count;}catch(...){}}
            s<<",\"components\":["<<names<<']';
            if(logged++<4000)logFile->event("effect_census_new",s.str());
            if(moving&&state==1&&has(b,w.current)){Followed* slot=&followed[0];for(auto& f:followed)if(!f.entity||f.since<slot->since){slot=&f;if(!f.entity)break;}
                const auto c=nativeImpactComponents::component(e,unsigned(w.current),56);if(c)*slot={world,id,GetTickCount64(),GetTickCount64(),read<WorldPositionQ32>(c),label};}
        }catch(...){}
    }
    w.known.swap(now);w.primed=true;
    // the followed things of this World: where and how fast, until 3 s or until they are gone
    const auto t=GetTickCount64();
    for(auto& f:followed){if(!f.entity||f.world!=world)continue;
        if(!w.known.count(f.entity)){if(followLogs++<3000)logFile->event("effect_census_flight_gone","\"world\":\""+std::string(f.label)+"\",\"entity\":"+std::to_string(f.entity)+",\"after_ms\":"+std::to_string(f.last-f.since));f.entity=0;continue;}
        if(t-f.since>3000){f.entity=0;continue;}
        try{const auto e=nativeImpactComponents::entityAtWorld(world,f.entity);
            const auto vc=nativeImpactComponents::component(e,unsigned(w.velocity),12),cc=nativeImpactComponents::component(e,unsigned(w.current),56);if(!vc||!cc)continue;
            const auto v=read<Vec3>(vc);const auto p=read<WorldPositionQ32>(cc);f.last=t;
            std::ostringstream s;s<<std::setprecision(5)<<"\"world\":\""<<f.label<<"\",\"entity\":"<<f.entity<<",\"ms\":"<<t-f.since<<",\"from_start_m\":["<<double(p.x-f.start.x)/4294967296.0<<','<<double(p.y-f.start.y)/4294967296.0<<','<<double(p.z-f.start.z)/4294967296.0
             <<"],\"velocity\":["<<v.x<<','<<v.y<<','<<v.z<<"],\"speed\":"<<length(v);
            if(followLogs++<3000)logFile->event("effect_census_flight",s.str());}catch(...){}}
}
// Called each frame by the render thread (view_resource_probe.cpp, beside the emitter census).
inline bool guestLogged=false;
inline void observe(uintptr_t clientWorld,uintptr_t serverWorld,const WorldPositionQ32& player,bool guest){
    if(state<0){char value[8]{};const auto n=GetEnvironmentVariableA("EVR_EFFECT_CENSUS",value,sizeof value);state=n==1&&value[0]=='1'?1:n==1&&value[0]=='0'?0:2;
        if(state==1)logFile->event("effect_census_enabled","\"diagnostic\":true,\"read_only\":true,\"every_ms\":100");}
    if(state==0)return;const auto now=GetTickCount64();
    if(state==2){if(!guest||logged>=1500||now-lastAt<500)return;lastAt=now;
        if(!guestLogged){guestLogged=true;logFile->event("effect_census_enabled","\"diagnostic\":true,\"read_only\":true,\"every_ms\":500,\"as_a_guest\":true");}
        try{pass(worlds[0],clientWorld,"client",player);}catch(...){}return;}
    if(now-lastAt<100)return;lastAt=now;
    try{pass(worlds[0],clientWorld,"client",player);}catch(...){}
    if(serverWorld&&serverWorld!=clientWorld)try{pass(worlds[1],serverWorld,"server",player);}catch(...){}
}
}
