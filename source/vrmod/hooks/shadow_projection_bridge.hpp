// Native shadow cascade setup assumes tag-0 perspective fields (13b630).
// Supply a local conversion only at faf4a inside an owned eye render.
// 1.0 (Hotfix 1.0.0.1 addresses): the setup is 0x1674a0 (June 0x13fa30), called once at 0x12d9a0 from the View's render
// with the View's shadow object (+0x990, June +0x728) and the camera block it has just copied to View+0x9d0 (June
// +0x760; the projection descriptor first, as before; the View's own projection is +0x908, June +0x6a0).
namespace shadowProjection {
constexpr uintptr_t rva=0x1674e0,callReturn=0x12d9e5,ownAt=0x990,parametersAt=0x9d0;
constexpr const char* pattern="40 55 53 56 48 8D AC 24 10 EE FF FF B8 F0 12 00 00 E8";
using Prepare=void(__fastcall*)(void*,const void*);
inline Prepare original=nullptr;
inline bool requested=false,enabled=false,failed=false;
inline std::array<uint64_t,2> converted{},readyChecks{},warmupCalls{};
inline uint64_t failures=0,mismatches=0;
struct Context {PairResources* pair;unsigned eye;unsigned calls=0;};
inline thread_local Context* current=nullptr;
struct Scope {
    Context context;Context* previous;
    Scope(PairResources& p,unsigned e):context{&p,e},previous(current){if(requested)current=&context;}
    ~Scope(){current=previous;}
};
inline void failure(const char* reason){failed=true;++failures;try{logFile->event("shadow_projection_failure","\"reason\":"+jsonString(reason));}catch(...){} }
// One set of sun cascades for both eyes (2026-10-17, headset reports 5, 16 and 20: "the shadows differ a little between the
// eyes", "in the LOD distance the shadows' shapes differ", "one eye's shadow reacts late"). Each eye fitted its own cascades
// to its own camera (RenderDoc game-20261017-034858: a 1024x1024x4 shadow map per eye; corrected 14:30 - the eyes redraw
// their maps only now and then, both in the same frames, see layer/shadow_share.hpp), so
// the cascades' bounds and texel grids differed by the eyes' distance apart. The right eye's setup now takes the left eye's
// converted camera block of the same frame. Off by default - it changed nothing that matters: the cascades were already
// aligned (RenderDoc game-20261017-034858, without it: texels differing by more than 0.01 in depth 1.7 % / 6.8 % of cascades
// 0 / 2; game-20261017-052808, with it: 2.0 % / 3.6 %); what differs is what casts the shadows (cascade 2: a round caster in
// one eye only - the eyes' own LOD / culling of the casters).
// 2026-10-17 evening: on by default again, with layer/shadow_share.hpp (the second eye's map is the first eye's copy, so its
// cascades must be the first eye's too; headset: "every shadow switches about twice a second" - the eyes' maps are redrawn
// some 2.8 times a second while the head moves, and a copy under the other eye's cascade fit is wrong until its next
// redraw, guess). EVR_SHARE_SHADOW_CASCADES=0 turns it off.
inline const bool shareCascades=[]{char v[8]{};const auto n=GetEnvironmentVariableA("EVR_SHARE_SHADOW_CASCADES",v,sizeof v);return !(n==1&&v[0]=='0');}();
// The right eye's cascades are the left eye's - and with them the left eye's camera (2026-10-18). The setup keeps the
// camera's place in each cascade (shadow object + 0x100 + 0x200 k), and the shadow pass turns the eye's camera-relative
// position into the cascade's map (shaders/.../shadow_receiver.comp: the screen-to-camera matrix, then the cascade's 3x4).
// So the right eye looked its shadows up 6.5 cm (the eyes' distance) off: its shadows had the left eye's place in its
// picture, no depth of their own (Mock, the altar of the 03:06 save: the sun spots 6 px left of the tiles' far edge with
// the cascades shared, 5 px right of it with each eye's own - the user: "with both eyes the shadow seems farther than the
// tile"; "the vertical shadow is at another place in each eye").
// In a cascade's block (dump of game-20261018-111546, cascade 0): +0x170, +0x180, +0x190 the map's 3x3 - what a metre
// along the world's x, y, z moves in the map's coordinates (the light's axes over the cascade's half size 23.357 m, the
// depth over -11239) - and +0x1a0 (a copy at +0x120) its translation (-0.347, -0.186, 0.568) = minus the cascade's
// centre (8.107, 4.347 m from the camera along the light's axes) over the half size: camera-relative. For the right eye
// the translation takes the 3x3 times (right camera - left camera). Tried first, no change (Mock C1 game-20261018-113232,
// C2 -113510: sun spots still -6 px): the six bounding planes at +0x90..+0xef and the camera at +0x10 - not what the
// shadow pass reads. EVR_SHADOW_EYE_FIX: matrix (default), planes, position (bits 4 / 1 / 2 may be combined as a
// number), 0 = as before.
inline const int shadowEyeFix=[]{char v[16]{};const auto n=GetEnvironmentVariableA("EVR_SHADOW_EYE_FIX",v,sizeof v);
    if(!n||n>=sizeof v)return 4;const std::string s(v,n);if(s=="matrix")return 4;if(s=="planes")return 1;if(s=="position")return 2;return std::clamp(std::atoi(s.c_str()),0,7);}();
inline std::atomic<uint64_t> eyeFixes{0};
// The camera block the setup reads: to +0x170 (2026-10-18, the setup 0x1674e0 read: +0x15c / +0x160 the cascade
// parameters, +0x168 a frame number that 0x161e20 compares with each cascade's last redraw, against the shadow object's
// table +0x4c of periods and phases - cascade 0 every frame, 1 every 2nd, 2 and 3 every 4th). The copy was 0x160 bytes,
// so the eyes' setups read the frame number from beyond it (the stack, unchanging): the eyes' cascades were redrawn
// only when the head or the light moved (Mock: some 50 frames apart; headset: "my and the trees' shadows switch at a
// regular interval"), the desktop View's every frame.
constexpr size_t blockSize=0x170;
struct SharedBlock {uint64_t serial=0;bool valid=false;alignas(16) std::array<uint8_t,blockSize> block{};};
inline SharedBlock leftBlock;inline std::array<uint64_t,2> sharedUses{};
void __fastcall prepare(void* object,const void* parameters){
    const auto caller=reinterpret_cast<uintptr_t>(_ReturnAddress());
    if(!current||caller!=baseAddress+callReturn){original(object,parameters);return;}
    auto& c=*current;auto& pair=*c.pair;
    alignas(16) std::array<uint8_t,blockSize> source{},local{};
    bool use=false;uintptr_t own=0;
    try{
        const ObserverLastError preserve;
        if(failed||c.eye>1||GetCurrentThreadId()!=pair.renderThread||++c.calls!=1||!eyeProjection)
            throw std::runtime_error("shadow projection private scope differs");
        const auto view=pair.created[c.eye];own=read<uintptr_t>(view+ownAt);
        if(!own||reinterpret_cast<uintptr_t>(object)!=own||reinterpret_cast<uintptr_t>(parameters)!=view+parametersAt||
           own==read<uintptr_t>(pair.originalView+ownAt)||own==read<uintptr_t>(pair.created[1-c.eye]+ownAt))
            throw std::runtime_error("shadow projection owner topology differs");
        source=read<std::array<uint8_t,blockSize>>(reinterpret_cast<uintptr_t>(parameters));local=source;
        game::ProjectionDescriptor descriptor;std::memcpy(descriptor.bytes.data(),source.data(),80);
        // 1.0 (Mock game-20261016-112850): the camera block's +0x4c is not the descriptor's (a value 13 there on some
        // frames, 0 in the eye's and the View's descriptors) - the first 0x4c bytes are compared and the rest is kept.
        constexpr size_t compared=0x4c;
        const auto viewDescriptor=read<game::ProjectionDescriptor>(view+layout::viewProjection);
        if(std::memcmp(descriptor.bytes.data(),eyeProjection->bytes.data(),compared)||std::memcmp(viewDescriptor.bytes.data(),descriptor.bytes.data(),compared)){
            // 1.0 (Mock game-20261016-111408): one frame of 6500 differed. That frame keeps the game's own cascades (as
            // without this bridge) and counts as handled; the three descriptors go to the record (the first 8 times).
            ++mismatches;++converted[c.eye];
            if(mismatches<=8)try{auto text=[](const std::array<uint8_t,80>& b){std::string s;char q[4];for(auto v:b){std::snprintf(q,sizeof q,"%02x",v);s+=q;}return s;};
                logFile->event("shadow_projection_mismatch","\"eye\":"+std::to_string(c.eye)+",\"frame\":"+std::to_string(liveFrame.frame)+",\"serial\":"+std::to_string(pair.serial)+
                    ",\"count\":"+std::to_string(mismatches)+",\"parameters\":\""+text(descriptor.bytes)+"\",\"eye_projection\":\""+text(eyeProjection->bytes)+
                    "\",\"view_projection\":\""+text(read<game::ProjectionDescriptor>(view+layout::viewProjection).bytes)+"\"");}catch(...){}
            original(object,parameters);return;
        }
        auto clean=descriptor;std::fill(clean.bytes.begin()+compared,clean.bytes.end(),uint8_t(0));
        const auto native=game::shadowPerspectiveDescriptor(clean);
        std::memcpy(local.data(),native.bytes.data(),compared);use=true;
    }catch(const std::exception& e){failure(e.what());}
    bool shared=false;
    if(use&&shareCascades){
        if(c.eye==0){leftBlock.serial=pair.serial;leftBlock.block=local;leftBlock.valid=true;}
        else if(leftBlock.valid&&leftBlock.serial==pair.serial){local=leftBlock.block;shared=true;
            const auto n=++sharedUses[1];if(n<=3||n%600==0)try{logFile->event("shadow_cascades_shared","\"serial\":"+std::to_string(pair.serial)+",\"count\":"+std::to_string(n)+",\"right_eye_uses_left_camera_block\":true");}catch(...){}}
    }
    // Exactly one original call. 13fa30 consumes this block synchronously and
    // copies the resulting cascade data into its own native object.
    original(object,use?local.data():parameters);
    if(shared&&shadowEyeFix)try{
        // the eyes' offset in the blocks' own frame (+0x80: the camera's place)
        const auto ownCamera=read<std::array<float,3>>(reinterpret_cast<uintptr_t>(parameters)+0x80),leftCamera=read<std::array<float,3>>(reinterpret_cast<uintptr_t>(local.data())+0x80);
        const float dx=ownCamera[0]-leftCamera[0],dy=ownCamera[1]-leftCamera[1],dz=ownCamera[2]-leftCamera[2];
        if(!std::isfinite(dx)||!std::isfinite(dy)||!std::isfinite(dz)||std::abs(dx)+std::abs(dy)+std::abs(dz)>1.f)throw std::runtime_error("eyes' offset out of range");
        unsigned cascades=0;const auto base=reinterpret_cast<uintptr_t>(object);
        for(unsigned k=0;k<8;++k){
            const uintptr_t cascade=base+0x100+0x200*uintptr_t(k);
            if(read<std::array<float,3>>(cascade+0x10)!=leftCamera)break;   // only the cascades that keep the left camera
            if(shadowEyeFix&1)for(unsigned p=0;p<6;++p){
                auto plane=read<std::array<float,4>>(cascade+0x90+0x10*p);plane[3]+=plane[0]*dx+plane[1]*dy+plane[2]*dz;
                *reinterpret_cast<std::array<float,4>*>(cascade+0x90+0x10*p)=plane;}
            if(shadowEyeFix&2)*reinterpret_cast<std::array<float,3>*>(cascade+0x10)=ownCamera;
            if(shadowEyeFix&4){
                const auto cx=read<std::array<float,4>>(cascade+0x170),cy=read<std::array<float,4>>(cascade+0x180),cz=read<std::array<float,4>>(cascade+0x190);
                const std::array<float,3> shift{cx[0]*dx+cy[0]*dy+cz[0]*dz,cx[1]*dx+cy[1]*dy+cz[1]*dz,cx[2]*dx+cy[2]*dy+cz[2]*dz};
                for(const uintptr_t at:{cascade+0x1a0,cascade+0x120}){auto t=read<std::array<float,4>>(at);for(unsigned i=0;i<3;++i)t[i]+=shift[i];*reinterpret_cast<std::array<float,4>*>(at)=t;}
            }
            ++cascades;
        }
        const auto n=++eyeFixes;
        if(n<=3||n%600==0){std::ostringstream s;s<<std::setprecision(6)<<"\"frame\":"<<liveFrame.frame<<",\"count\":"<<n<<",\"cascades\":"<<cascades<<",\"offset\":["<<dx<<','<<dy<<','<<dz<<"],\"mode\":"<<shadowEyeFix;
            logFile->event("shadow_eye_offset_fixed",s.str());}
    }catch(const std::exception& e){failure(e.what());}
    // Diagnostic (2026-10-18; headset: "my and the trees' shadows switch at a regular interval"): the eyes' maps are
    // redrawn only now and then (layer shadow_share_redraw), the desktop View's every frame. EVR_SHADOW_OBJECT_DUMP=1
    // writes the first eye's shadow object (0x800 bytes) after every setup for 400 frames, to find what tells a redraw.
    {static const bool dump=[]{char v[4]{};return GetEnvironmentVariableA("EVR_SHADOW_OBJECT_DUMP",v,sizeof v)==1&&v[0]=='1';}();static unsigned dumps=0;
     static const unsigned from=[]{char v[16]{};return GetEnvironmentVariableA("EVR_SHADOW_OBJECT_DUMP_FROM",v,sizeof v)?unsigned(std::strtoul(v,nullptr,10)):0u;}();
     // 2026-10-18 (headset: "the shadows look a little farther than the tiles"): both eyes, with the camera block each setup
     // was given (`given`: the left eye's for the right one while the cascades are shared) and the eye's own (`own`), to
     // see whether the cascades keep the camera's place (then the right eye looks its shadows up from the left eye's place).
     if(dump&&liveFrame.frame>=from&&dumps<400)try{++dumps;const auto bytes=read<std::array<uint8_t,0x800>>(reinterpret_cast<uintptr_t>(object));
        auto hexOf=[](const uint8_t* p,size_t n){std::string t;t.reserve(n*2);char q[3];for(size_t i=0;i<n;++i){std::snprintf(q,sizeof q,"%02x",p[i]);t+=q;}return t;};
        const auto ownBlock=read<std::array<uint8_t,blockSize>>(reinterpret_cast<uintptr_t>(parameters));
        logFile->event("shadow_object_dump","\"frame\":"+std::to_string(liveFrame.frame)+",\"eye\":"+std::to_string(c.eye)+",\"shared\":"+(shared?"true":"false")+",\"serial\":"+std::to_string(pair.serial)+
            ",\"object\":"+hex(reinterpret_cast<uintptr_t>(object))+",\"given\":\""+hexOf(use?local.data():ownBlock.data(),blockSize)+"\",\"own\":\""+hexOf(ownBlock.data(),blockSize)+"\",\"bytes\":\""+hexOf(bytes.data(),bytes.size())+"\"");}catch(...){}}
    if(!use)return;
    try{
        const ObserverLastError preserve;
        if(read<std::array<uint8_t,blockSize>>(reinterpret_cast<uintptr_t>(parameters))!=source)
            throw std::runtime_error("shadow source parameters changed");
        const auto count=++converted[c.eye];
        if(count<=3||count%120==0){
            auto values=read<std::array<float,4>>(reinterpret_cast<uintptr_t>(local.data())+4);
            std::ostringstream s;s<<std::setprecision(9)<<"\"eye\":"<<c.eye<<",\"frame\":"<<liveFrame.frame<<",\"serial\":"<<pair.serial
                <<",\"count\":"<<count<<",\"owner\":"<<hex(own)<<",\"aspect_near_far_fov\":["<<values[0]<<','<<values[1]<<','<<values[2]<<','<<values[3]
                <<"],\"source_unchanged\":true,\"scope\":\"private_eye\"";
            logFile->event("shadow_projection_converted",s.str());}
    }catch(const std::exception& e){failure(e.what());}
}
inline void auditEye(unsigned eye,bool before,bool after,uint64_t countBefore){
    if(!requested)return;
    const auto calls=converted.at(eye)-countBefore;
    if(before&&after){if(calls!=1)throw std::runtime_error("ready eye omitted native shadow projection");++readyChecks[eye];}
    else warmupCalls[eye]+=calls;
}
inline void install(){
    const auto bytes=parsePattern(pattern);const auto target=baseAddress+rva;
    if(scan(reinterpret_cast<const uint8_t*>(target),bytes.size(),bytes)!=std::vector<size_t>{0})
        throw std::runtime_error("shadow setup signature differs");
    const auto* call=reinterpret_cast<const uint8_t*>(baseAddress+callReturn-5);int32_t delta=0;std::memcpy(&delta,call+1,4);
    if(*call!=0xe8||reinterpret_cast<uintptr_t>(call+5)+delta!=target)throw std::runtime_error("shadow setup callsite differs");
    if(MH_CreateHook(reinterpret_cast<void*>(target),reinterpret_cast<void*>(&prepare),reinterpret_cast<void**>(&original))!=MH_OK||
       MH_EnableHook(reinterpret_cast<void*>(target))!=MH_OK)throw std::runtime_error("shadow setup hook failed");
    enabled=true;logFile->event("shadow_projection_enabled","\"private_eye_only\":true,\"source_unchanged\":true,\"rva\":"+hex(rva));
}
inline void disable(){
    if(!enabled)return;
    if(MH_DisableHook(reinterpret_cast<void*>(baseAddress+rva))!=MH_OK)throw std::runtime_error("shadow setup hook disable failed");enabled=false;
    logFile->event("shadow_projection_disabled","\"left\":"+std::to_string(converted[0])+",\"right\":"+std::to_string(converted[1])+
        ",\"ready_checks\":["+std::to_string(readyChecks[0])+","+std::to_string(readyChecks[1])+"],\"warmup_calls\":["+
        std::to_string(warmupCalls[0])+","+std::to_string(warmupCalls[1])+"],\"failures\":"+std::to_string(failures)+",\"hook_disabled\":true");
}
}
