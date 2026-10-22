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
// within: metres from the player a new entity must be in to be told (0: anywhere).
inline void pass(WorldState& w,uintptr_t world,const char* label,const WorldPositionQ32& origin,double within=0){
    if(!world)return;
    if(w.world!=world||w.registry!=read<uintptr_t>(world))learn(w,world);
    const auto map=world+0x148;const auto bitmap=read<uintptr_t>(map+8),capacity=read<uint64_t>(map+0x10),keyArray=read<uintptr_t>(map+0x28),values=read<uintptr_t>(map+0x40);
    if(!bitmap||!keyArray||!values||!capacity||capacity>0x100000||(capacity&(capacity-1)))return;
    keys.resize(size_t(capacity));bits.resize(size_t((capacity+63)/64));SIZE_T got=0;
    if(!ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(bitmap),bits.data(),bits.size()*8,&got)||got!=bits.size()*8)return;
    if(!ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(keyArray),keys.data(),keys.size()*4,&got)||got!=keys.size()*4)return;
    ++w.passes;std::unordered_map<uint32_t,uint8_t> now;now.reserve(w.known.size()+64);
    unsigned described=0;   // (review M3: as a guest at most 40 new things a pass - a fast travel brings thousands at once)
    for(uint64_t i=0;i<capacity;++i){if(!(bits[size_t(i/64)]>>(i%64)&1))continue;const auto id=keys[size_t(i)];now.emplace(id,uint8_t(1));
        if(!w.primed||w.known.count(id))continue;
        if(within>0&&described>=40)continue;
        try{const auto record=read<uintptr_t>(values+i*8);if(!record)continue;
            nativeImpactComponents::Entity e;e.world=world;e.record=record;e.layout=read<uintptr_t>(record+0x18);e.data=read<uintptr_t>(record+0x20);e.row=read<uint32_t>(record+0x30);
            if(!e.layout||!e.data)continue;const auto b=read<std::array<uint64_t,10>>(e.layout);
            // 2026-10-22: a thing with a velocity and no gravity too (a wand's shot may have none)
            const bool n=has(b,w.notifier),v=has(b,w.vfx),a=has(b,w.audio),moving=has(b,w.velocity);if(!n&&!v&&!a&&!moving)continue;
            if(within>0){if(!has(b,w.current))continue;const auto c=nativeImpactComponents::component(e,unsigned(w.current),56);if(!c)continue;const auto p=read<WorldPositionQ32>(c);
                const double x=double(p.x-origin.x)/4294967296.0,y=double(p.y-origin.y)/4294967296.0,z=double(p.z-origin.z)/4294967296.0;if(x*x+y*y+z*z>within*within)continue;}
            std::ostringstream s;s<<std::setprecision(4)<<"\"world\":\""<<label<<"\",\"frame\":"<<liveFrame.frame<<",\"entity\":"<<id<<",\"notifier\":"<<n<<",\"vfx\":"<<v<<",\"audio\":"<<a;
            // A thing that flies (an arrow, a thrown item, a wand's shot): its velocity and gravity as first seen.
            if(moving){const auto vc=nativeImpactComponents::component(e,unsigned(w.velocity),12),gc=has(b,w.gravity)?nativeImpactComponents::component(e,unsigned(w.gravity),20):0;
                if(vc){const auto vel=read<Vec3>(vc);s<<",\"velocity\":["<<vel.x<<','<<vel.y<<','<<vel.z<<"],\"speed\":"<<length(vel);}
                if(gc){const auto gdir=read<Vec3>(gc);const auto gval=read<float>(gc+12);const auto gon=read<uint8_t>(gc+16);
                    s<<",\"gravity\":["<<gdir.x<<','<<gdir.y<<','<<gdir.z<<"],\"gravity_value\":"<<gval<<",\"gravity_on\":"<<unsigned(gon);}}
            if(has(b,w.current)){const auto c=nativeImpactComponents::component(e,unsigned(w.current),56);if(c){const auto p=read<WorldPositionQ32>(c);
                s<<",\"from_player_m\":["<<double(p.x-origin.x)/4294967296.0<<','<<double(p.y-origin.y)/4294967296.0<<','<<double(p.z-origin.z)/4294967296.0<<']';}}
            if(n&&w.notifierBytes>=92){const auto c=nativeImpactComponents::component(e,unsigned(w.notifier),w.notifierBytes);
                if(c)s<<",\"notifier_bytes\":"<<jsonString(nativeImpactComponents::asHex(read<std::array<uint8_t,92>>(c)));}
            int count=0;std::string names;for(unsigned t=0;t<nativeImpactComponents::componentCapacity&&count<24;++t)if(has(b,int(t))){
                try{auto name=nativeImpactComponents::componentType(world,t).qualifiedName;if(name.rfind("keen::ecs::",0)==0)name=name.substr(11);names+=(count?",":"")+jsonString(name);++count;}catch(...){}}
            s<<",\"components\":["<<names<<']';
            ++described;if(logged++<4000)logFile->event("effect_census_new",s.str());
            if(moving&&has(b,w.current)){Followed* slot=&followed[0];for(auto& f:followed)if(!f.entity||f.since<slot->since){slot=&f;if(!f.entity)break;}
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
// The breakable things as a client sees them (2026-10-22, diagnostic, read-only; EVR_DESTRUCTIBLE_TWINS=1 in a game of
// one): a guest's World holds no Destructible - a pot and a wall are both a StaticClientCollider with a
// CollisionFeedbackMaterialComponent - so what tells them apart there? Once, 20 s after both Worlds are known: the
// server's things with a Destructible within 15 m and the same id in the client's World, and the client's collider things
// within 15 m and whether the server holds a Destructible of that id: their components, the feedback material's bytes
// (material, itemId +0x10, propDamageSusceptibility +0x24), the ModelComponent's first word (the Scene's model handle).
namespace twins {
inline int wanted=-1;inline uint64_t bothSince=0;inline bool done=false;
struct Ids {int destructible=-1,current=-1,collider=-1,feedback=-1,model=-1;unsigned feedbackBytes=0,modelBytes=0;};
inline Ids ids(uintptr_t world){Ids r;
    for(unsigned i=0;i<nativeImpactComponents::componentCapacity;++i){nativeImpactComponents::ComponentType t;try{t=nativeImpactComponents::componentType(world,i);}catch(const std::exception&){break;}
        const auto& n=t.qualifiedName;
        if(n=="keen::ecs::Destructible")r.destructible=int(i);else if(n=="keen::ecs::CurrentTransform"&&t.bytes==56)r.current=int(i);
        else if(n=="keen::ecs::StaticClientCollider")r.collider=int(i);else if(n=="keen::ecs::CollisionFeedbackMaterialComponent"){r.feedback=int(i);r.feedbackBytes=t.bytes;}
        else if(n=="keen::ecs::ModelComponent"){r.model=int(i);r.modelBytes=t.bytes;}}
    return r;}
inline std::string names(uintptr_t world,const std::array<uint64_t,10>& b){std::string out;int count=0;
    for(unsigned t=0;t<nativeImpactComponents::componentCapacity&&count<48;++t)if(has(b,int(t))){
        try{auto name=nativeImpactComponents::componentType(world,t).qualifiedName;if(name.rfind("keen::ecs::",0)==0)name=name.substr(11);out+=(count?",":"")+jsonString(name);++count;}catch(...){}}
    return out;}
inline bool entityBits(uintptr_t world,uint32_t id,nativeImpactComponents::Entity& e,std::array<uint64_t,10>& b){
    try{e=nativeImpactComponents::entityAtWorld(world,id);b=read<std::array<uint64_t,10>>(e.layout);return true;}catch(...){return false;}}
inline void run(uintptr_t client,uintptr_t server,const WorldPositionQ32& origin){
    const auto ci=ids(client),si=ids(server);
    logFile->event("destructible_twins_types","\"client\":{\"collider\":"+std::to_string(ci.collider)+",\"feedback\":"+std::to_string(ci.feedback)+",\"feedback_bytes\":"+std::to_string(ci.feedbackBytes)+
        ",\"model\":"+std::to_string(ci.model)+",\"model_bytes\":"+std::to_string(ci.modelBytes)+",\"destructible\":"+std::to_string(ci.destructible)+"},\"server\":{\"destructible\":"+std::to_string(si.destructible)+"}");
    const auto within15=[&](uintptr_t world,const nativeImpactComponents::Entity& e,const std::array<uint64_t,10>& b,int current,double& m){
        if(!has(b,current))return false;const auto c=nativeImpactComponents::component(e,unsigned(current),56);if(!c)return false;const auto p=read<WorldPositionQ32>(c);
        const double x=double(p.x-origin.x)/4294967296.0,y=double(p.y-origin.y)/4294967296.0,z=double(p.z-origin.z)/4294967296.0;m=std::sqrt(x*x+y*y+z*z);(void)world;return m<=15.;};
    const auto describe=[&](uintptr_t world,const Ids& i,const nativeImpactComponents::Entity& e,const std::array<uint64_t,10>& b){std::string s;
        if(has(b,i.feedback)&&i.feedbackBytes&&i.feedbackBytes<=64){const auto c=nativeImpactComponents::component(e,unsigned(i.feedback),i.feedbackBytes);
            if(c){std::array<uint8_t,64> raw{};SIZE_T got=0;ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(c),raw.data(),i.feedbackBytes,&got);
                s+=",\"feedback_bytes\":"+jsonString(nativeImpactComponents::asHex(raw).substr(0,size_t(i.feedbackBytes)*2));}}
        if(has(b,i.model)&&i.modelBytes>=4){const auto c=nativeImpactComponents::component(e,unsigned(i.model),i.modelBytes);if(c){const auto h=read<uint32_t>(c);s+=",\"model_handle\":"+std::to_string(h);
            // the model resource's GUID (+0x18) and number, as the guest's break looks them up (include/evr/breakable_models.hpp)
            try{const auto o=motionVisual::scene&&h?motionVisual::currentObject(h):0;const auto r=o?read<uintptr_t>(o+layout::modelResource):0;
                if(r){const auto g=read<std::array<uint32_t,4>>(r+0x18);s+=",\"model_number\":"+std::to_string(read<uint16_t>(r+layout::resourceModel))+",\"model_guid\":["+std::to_string(g[0])+","+std::to_string(g[1])+","+std::to_string(g[2])+","+std::to_string(g[3])+"]";}}catch(...){}}}
        return s+",\"components\":["+names(world,b)+"]";};
    for(int side=0;side<2;++side){const auto world=side?client:server;const auto& i=side?ci:si;
        const auto map=world+0x148;const auto bitmap=read<uintptr_t>(map+8),capacity=read<uint64_t>(map+0x10),keyArray=read<uintptr_t>(map+0x28);
        if(!bitmap||!keyArray||!capacity||capacity>0x100000||(capacity&(capacity-1)))continue;
        std::vector<uint32_t> k(static_cast<size_t>(capacity));std::vector<uint64_t> bb(size_t((capacity+63)/64));SIZE_T got=0;
        if(!ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(bitmap),bb.data(),bb.size()*8,&got)||!ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(keyArray),k.data(),k.size()*4,&got))continue;
        unsigned told=0;
        for(uint64_t n=0;n<capacity&&told<40;++n){if(!(bb[size_t(n/64)]>>(n%64)&1))continue;const auto id=k[size_t(n)];
            nativeImpactComponents::Entity e;std::array<uint64_t,10> b{};if(!entityBits(world,id,e,b))continue;
            const bool pick=side?(has(b,i.collider)&&has(b,i.feedback)):has(b,i.destructible);if(!pick)continue;double m=0;
            try{if(!within15(world,e,b,i.current,m))continue;}catch(...){continue;}
            ++told;std::ostringstream o;o<<std::setprecision(4)<<"\"side\":\""<<(side?"client":"server")<<"\",\"entity\":"<<id<<",\"metres\":"<<m;
            std::string twin;const auto otherWorld=side?server:client;const auto& oi=side?si:ci;nativeImpactComponents::Entity oe;std::array<uint64_t,10> ob{};
            const bool there=entityBits(otherWorld,id,oe,ob);
            try{logFile->event("destructible_twin",o.str()+describe(world,i,e,b)+",\"twin\":"+(there?"true":"false")+(there?",\"twin_destructible\":"+std::string(has(ob,oi.destructible)?"true":"false")+",\"twin_detail\":{\"x\":0"+describe(otherWorld,oi,oe,ob)+"}":""));}catch(...){}}}
}
inline void observe(uintptr_t client,uintptr_t server,const WorldPositionQ32& origin){
    if(wanted<0){char v[8]{};const auto n=GetEnvironmentVariableA("EVR_DESTRUCTIBLE_TWINS",v,sizeof v);wanted=n==1&&v[0]=='1'?1:0;}
    if(!wanted||done||!client||!server||client==server)return;const auto now=GetTickCount64();
    if(!bothSince){bothSince=now;return;}if(now-bothSince<20000)return;done=true;
    try{run(client,server,origin);}catch(const std::exception& x){try{logFile->event("destructible_twins_failure","\"reason\":"+jsonString(x.what()));}catch(...){}}
}
}
// Where the inputs of a hit's damage are kept (2026-10-22, diagnostic, read-only; EVR_DAMAGE_DISCOVERY=1 in a game of one;
// docs/analysis/damage_formula_20261022: the damage is round(weapon x attributes x attack x the target's susceptibility),
// and a guest must find on its own PC the weapon's level and perks, and the target's template). Once, 25 s after both
// Worlds are known, in each World: the player's worn stacks {item, count, instance} and, for an instance, the entity of
// that id - its components and the bytes of each (up to 256); the creatures (Health or NetworkHealth) within 60 m - the
// entity record's first 0x40 bytes and the bytes of their components of up to 64 bytes (a template's GUID may be there);
// and the same for two collider things with the server's Destructible (damage_discovery_*).
namespace discovery {
inline int wanted=-1;inline uint64_t bothSince=0;inline bool done=false;
inline std::string hexOf(const uint8_t* p,size_t n){static const char* d="0123456789abcdef";std::string s;s.reserve(n*2);for(size_t i=0;i<n;++i){s+=d[p[i]>>4];s+=d[p[i]&15];}return s;}
inline std::string componentsWithBytes(uintptr_t world,const nativeImpactComponents::Entity& e,const std::array<uint64_t,10>& b,unsigned maximumBytes){
    std::string out;int count=0;
    for(unsigned t=0;t<nativeImpactComponents::componentCapacity&&count<64;++t)if(has(b,int(t))){
        try{const auto type=nativeImpactComponents::componentType(world,t);auto name=type.qualifiedName;if(name.rfind("keen::ecs::",0)==0)name=name.substr(11);
            out+=std::string(count?",":"")+"{\"name\":"+jsonString(name)+",\"size\":"+std::to_string(type.bytes);
            if(type.bytes&&type.bytes<=maximumBytes){const auto c=nativeImpactComponents::component(e,t,type.bytes);
                if(c){std::vector<uint8_t> raw(type.bytes);SIZE_T got=0;if(ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(c),raw.data(),raw.size(),&got)&&got==raw.size())out+=",\"bytes\":\""+hexOf(raw.data(),raw.size())+"\"";}}
            out+="}";++count;}catch(...){}}
    return out;}
inline std::string recordBytes(uintptr_t world,uint32_t id){
    try{const auto e=nativeImpactComponents::entityAtWorld(world,id);std::array<uint8_t,0x40> r{};SIZE_T got=0;
        if(ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(e.record),r.data(),r.size(),&got)&&got==r.size())return hexOf(r.data(),r.size());}catch(...){}
    return "";}
inline int typeId(uintptr_t world,const char* name,unsigned bytes=0){
    for(unsigned i=0;i<nativeImpactComponents::componentCapacity;++i){nativeImpactComponents::ComponentType t;try{t=nativeImpactComponents::componentType(world,i);}catch(const std::exception&){break;}
        if(t.qualifiedName==name&&(!bytes||t.bytes==bytes))return int(i);}
    return -1;}
inline void run(uintptr_t client,uintptr_t server,const WorldPositionQ32& origin,uint32_t local){
    for(int side=0;side<2;++side){const auto world=side?client:server;const char* label=side?"client":"server";
        const int selection=typeId(world,"keen::ecs::SlotSelection",216),inventory=typeId(world,"keen::ecs::Inventory",96),current=typeId(world,"keen::ecs::CurrentTransform",56);
        const int health=typeId(world,"keen::ecs::Health"),networkHealth=typeId(world,"keen::ecs::NetworkHealth",8),destructible=typeId(world,"keen::ecs::Destructible");
        logFile->event("damage_discovery_world","\"side\":\""+std::string(label)+"\",\"world\":"+hex(world)+",\"slot_selection\":"+std::to_string(selection)+",\"inventory\":"+std::to_string(inventory)+",\"local\":"+std::to_string(local));
        // the player's worn stacks and their instances
        try{const auto player=nativeImpactComponents::entityAtWorld(world,local);const auto sel=selection>=0?nativeImpactComponents::component(player,unsigned(selection),216):0;
            if(sel){const auto raw=read<std::array<uint8_t,216>>(sel);
                for(unsigned slot=0;slot<20;++slot){uint32_t row=0;std::memcpy(&row,raw.data()+8*slot,4);const uint8_t index=raw[8*slot+4];if(!row||index>=8||inventory<0)continue;
                    try{const auto r=nativeImpactComponents::entityAtWorld(world,row);const auto inv=nativeImpactComponents::component(r,unsigned(inventory),96);if(!inv)continue;
                        const auto stack=read<std::array<uint8_t,12>>(inv+12*index);uint32_t item=0,count=0,instance=0;std::memcpy(&item,stack.data(),4);std::memcpy(&count,stack.data()+4,4);std::memcpy(&instance,stack.data()+8,4);
                        std::string detail;
                        if(instance){nativeImpactComponents::Entity ie;std::array<uint64_t,10> ib{};
                            try{ie=nativeImpactComponents::entityAtWorld(world,instance);ib=read<std::array<uint64_t,10>>(ie.layout);detail=",\"instance_record\":\""+recordBytes(world,instance)+"\",\"instance_components\":["+componentsWithBytes(world,ie,ib,256)+"]";}
                            catch(...){detail=",\"instance_entity\":false";}}
                        {const auto registry=attachmentObserver::itemRegistry.load();if(registry&&item)try{auto reader=inputRanged::readItemBytes;const auto found=game::lookupItem(reader,registry,item);
                            if(found.status==game::ItemLookupStatus::Found&&found.pointer){std::array<uint8_t,0x50> raw{};if(inputRanged::readItemBytes(found.pointer+0x670,raw.data(),raw.size()))detail+=",\"item_0x670\":\""+hexOf(raw.data(),raw.size())+"\"";}}catch(...){}}
                        logFile->event("damage_discovery_stack","\"side\":\""+std::string(label)+"\",\"slot\":"+std::to_string(slot)+",\"row\":"+std::to_string(row)+",\"index\":"+std::to_string(index)+
                            ",\"item\":"+std::to_string(item)+",\"count\":"+std::to_string(count)+",\"instance\":"+std::to_string(instance)+detail);}catch(...){}}}}catch(...){}
        // creatures within 60 m, and things with a Destructible on the server
        const auto map=world+0x148;const auto bitmap=read<uintptr_t>(map+8),capacity=read<uint64_t>(map+0x10),keyArray=read<uintptr_t>(map+0x28);
        if(!bitmap||!keyArray||!capacity||capacity>0x100000||(capacity&(capacity-1)))continue;
        std::vector<uint32_t> k(static_cast<size_t>(capacity));std::vector<uint64_t> bb(static_cast<size_t>((capacity+63)/64));SIZE_T got=0;
        if(!ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(bitmap),bb.data(),bb.size()*8,&got)||!ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(keyArray),k.data(),k.size()*4,&got))continue;
        unsigned creatures=0,things=0;
        for(uint64_t n=0;n<capacity&&(creatures<10||things<2);++n){if(!(bb[size_t(n/64)]>>(n%64)&1))continue;const auto id=k[size_t(n)];if(id==local)continue;
            nativeImpactComponents::Entity e;std::array<uint64_t,10> b{};try{e=nativeImpactComponents::entityAtWorld(world,id);b=read<std::array<uint64_t,10>>(e.layout);}catch(...){continue;}
            const bool creature=has(b,health)||has(b,networkHealth),thing=has(b,destructible);if(!(creature&&creatures<10)&&!(thing&&things<2))continue;
            if(!has(b,current))continue;double m=0;
            try{const auto c=nativeImpactComponents::component(e,unsigned(current),56);if(!c)continue;const auto p=read<WorldPositionQ32>(c);
                const double x=double(p.x-origin.x)/4294967296.0,y=double(p.y-origin.y)/4294967296.0,z=double(p.z-origin.z)/4294967296.0;m=std::sqrt(x*x+y*y+z*z);}catch(...){continue;}
            if(m>60)continue;if(creature)++creatures;else ++things;
            std::ostringstream o;o<<std::setprecision(4)<<"\"side\":\""<<label<<"\",\"entity\":"<<id<<",\"metres\":"<<m<<",\"kind\":\""<<(creature?"creature":"destructible")<<"\",\"record\":\""<<recordBytes(world,id)<<"\"";
            try{logFile->event("damage_discovery_entity",o.str()+",\"components\":["+componentsWithBytes(world,e,b,64)+"]");}catch(...){}
            // the same id in the other World
            const auto other=side?server:client;nativeImpactComponents::Entity oe;std::array<uint64_t,10> ob{};
            try{oe=nativeImpactComponents::entityAtWorld(other,id);ob=read<std::array<uint64_t,10>>(oe.layout);
                logFile->event("damage_discovery_entity","\"side\":\""+std::string(side?"server":"client")+"\",\"entity\":"+std::to_string(id)+",\"twin_of\":\""+label+"\",\"record\":\""+recordBytes(other,id)+"\",\"components\":["+componentsWithBytes(other,oe,ob,64)+"]");}catch(...){}}
    }
}
inline void observe(uintptr_t client,uintptr_t server,const WorldPositionQ32& origin){
    if(wanted<0){char v[8]{};const auto n=GetEnvironmentVariableA("EVR_DAMAGE_DISCOVERY",v,sizeof v);wanted=n==1&&v[0]=='1'?1:0;}
    if(!wanted||done||!client||!server||client==server)return;const auto now=GetTickCount64();
    if(!bothSince){bothSince=now;return;}if(now-bothSince<25000)return;done=true;
    rootCamera::Sample root;if(!rootCamera::trySnapshot(root)||!root.entity){done=false;return;}
    try{run(client,server,origin,root.entity);}catch(const std::exception& x){try{logFile->event("damage_discovery_failure","\"reason\":"+jsonString(x.what()));}catch(...){}}
}
}
// Called each frame by the render thread (view_resource_probe.cpp, beside the emitter census).
inline bool guestLogged=false;
inline void observe(uintptr_t clientWorld,uintptr_t serverWorld,const WorldPositionQ32& player,bool guest){
    if(state<0){char value[8]{};const auto n=GetEnvironmentVariableA("EVR_EFFECT_CENSUS",value,sizeof value);state=n==1&&value[0]=='1'?1:n==1&&value[0]=='0'?0:2;
        if(state==1)logFile->event("effect_census_enabled","\"diagnostic\":true,\"read_only\":true,\"every_ms\":100");}
    try{twins::observe(clientWorld,serverWorld,player);}catch(...){}
    try{discovery::observe(clientWorld,serverWorld,player);}catch(...){}
    if(state==0)return;const auto now=GetTickCount64();
    // as a guest (2026-10-22): new things within 40 m of the player only (the guest log of 10-22 spent its 600 records on
    // far scenery before the wand's shots), every 300 ms, 3000 records at the most - for a wand's shot as it reaches the guest
    if(state==2){if(!guest||logged>=3000||now-lastAt<300)return;lastAt=now;
        if(!guestLogged){guestLogged=true;logFile->event("effect_census_enabled","\"diagnostic\":true,\"read_only\":true,\"every_ms\":300,\"within_m\":40,\"as_a_guest\":true");}
        try{pass(worlds[0],clientWorld,"client",player,40.);}catch(...){}return;}
    if(now-lastAt<100)return;lastAt=now;
    try{pass(worlds[0],clientWorld,"client",player);}catch(...){}
    if(serverWorld&&serverWorld!=clientWorld)try{pass(worlds[1],serverWorld,"server",player);}catch(...){}
}
}
