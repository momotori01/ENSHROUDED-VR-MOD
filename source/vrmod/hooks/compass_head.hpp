// The HUD's compass turns with the head (headset run 20261014-124515, user: "the compass's direction does not react
// to the movement of the head - make it react to where the head looks").
// The compass (the profiler scope "doHudCompass()", inlined in the HUD's update at 0xab8fc0) reads its heading from
// the HUD's game data, *(*(*(game+0x273f588)+0xc8))+0x28: radians at +0xb0, the bearing of the game's own camera -
// the one the right stick turns. The player's position follows at +0xe0 (x) and +0xe8 (z), and a marker at world
// x,z stands on the strip at heading + 3/2 pi - atan2(dz, dx), so the heading grows with atan2(z, x) of where the
// camera looks. In the headset the picture is the head's, and the sent input already looks where the head looks
// (input_head_idle.hpp, the VR walking camera). For the length of the HUD's update the heading is turned by that
// input's yaw away from the game's camera, and afterwards put back - unless the game wrote it meanwhile. Nothing else
// is written; without a fresh turn (menus, keyboard play, the flat game) the compass is the game's own.
namespace compassHead {
using Update=uintptr_t(__fastcall*)(void*);
inline Update original=nullptr;
inline bool requested=false,enabled=false;
inline std::atomic<bool> accepting{false};
inline std::atomic<unsigned> callbacks{0},applied{0},restored{0},kept{0},failures{0};
inline std::atomic<uint64_t> turnMs{0};inline std::atomic<float> turn{0}; // radians about the vertical, and when it was told
// 1.0 (Hotfix 1.0.0.1): the HUD's update is 0xbd1d40 ("doHudCompass()" at 0xbd1eaa); the data is
// *(*(*(game+0xd0))+0x38) (June +0xc8, +0x28) and the heading +0x1da0 (June +0xb0): the compass adds pi and pi/2 to it.
constexpr uintptr_t rva=0xbd2360,gameRva=0x281b498,headingOffset=0x1da0,holderAt=0xd0,dataAt=0x38;
constexpr const char* pattern="4C 8B DC 49 89 4B 08 55 41 55 41 57 49 8D AB A8 F5 FF FF 48 81 EC 40 0B 00 00 4C 8B 3D ? ? ? ?";
struct CallbackScope{CallbackScope(){++callbacks;}~CallbackScope(){--callbacks;}};
constexpr float pi=3.14159265358979f;
inline float wrapAround(float a){a=std::fmod(a+pi,2*pi);if(a<0)a+=2*pi;return a-pi;}          // (-pi, pi]
// The input thread: the camera the game put into the input, and the one that is sent.
inline void tell(const uint8_t* sourceCamera,const uint8_t* sentCamera,uint64_t now){
    Quat a,b;std::memcpy(&a,sourceCamera,sizeof a);std::memcpy(&b,sentCamera,sizeof b);
    const auto fa=rotate(normalized(a),{0,0,-1}),fb=rotate(normalized(b),{0,0,-1});
    if(!(fa.x*fa.x+fa.z*fa.z>.04f)||!(fb.x*fb.x+fb.z*fb.z>.04f))return;      // looking straight up or down: keep the last
    const float d=wrapAround(std::atan2(fb.z,fb.x)-std::atan2(fa.z,fa.x));
    if(!std::isfinite(d))return;
    turn.store(d);turnMs.store(now);
}
inline uintptr_t data(){
    const auto game=read<uintptr_t>(baseAddress+gameRva);if(!game)return 0;
    const auto holder=read<uintptr_t>(game+holderAt);if(!holder)return 0;
    const auto first=read<uintptr_t>(holder);if(!first)return 0;
    return read<uintptr_t>(first+dataAt);
}
__declspec(noinline) uintptr_t __fastcall update(void* hud){
    if(!accepting.load())return original(hud);
    const CallbackScope callback;
    float* heading=nullptr;float before=0,written=0;
    {const ObserverLastError preserve;
     try{
        const auto now=GetTickCount64(),at=turnMs.load();
        if(at&&now>=at&&now-at<=300){
            if(const auto d=data()){
                before=read<float>(d+headingOffset);
                if(std::isfinite(before)&&std::abs(before)<=8.f){
                    float value=before+turn.load();
                    // The game's own range: 0..2 pi, or -pi..pi.
                    value=before>=0?wrapAround(value-pi)+pi:wrapAround(value);
                    heading=reinterpret_cast<float*>(d+headingOffset);*heading=written=value;++applied;
                    // A line when the turn has changed by 0.2 rad or a second has passed (300 lines): with the player's
                    // position (June +0xe0 x, +0xe8 z; not placed in 1.0) the way walked can be set against the heading shown.
                    static std::atomic<unsigned> lines{0};static float toldTurn=100;static uint64_t toldMs=0;
                    if(lines.load()<300&&(std::abs(turn.load()-toldTurn)>.2f||now-toldMs>=1000)){++lines;toldTurn=turn.load();toldMs=now;
                        logFile->event("compass_head_turn","\"game_heading\":"+std::to_string(before)+",\"turn\":"+std::to_string(toldTurn)+",\"shown\":"+std::to_string(value)+
                            ",\"applied\":"+std::to_string(applied.load()));}
                }
            }
        }
     }catch(const std::exception& e){heading=nullptr;if(failures.fetch_add(1)<3)logFile->event("compass_head_failure","\"reason\":"+jsonString(e.what()));}
    }
    const auto result=original(hud);
    if(heading){const ObserverLastError preserve;
        if(*heading==written){*heading=before;++restored;}else ++kept;}   // the game wrote a new one meanwhile: that stays
    return result;
}
inline void install(){
    if(!requested)return;
    const auto bytes=parsePattern(pattern);
    if(scan(reinterpret_cast<const uint8_t*>(baseAddress+rva),bytes.size(),bytes)!=std::vector<size_t>{0}||
       uintptr_t(int64_t(baseAddress+rva+0x21)+read<int32_t>(baseAddress+rva+0x1d))!=baseAddress+gameRva)
        throw std::runtime_error("HUD compass update signature changed");
    if(MH_CreateHook(reinterpret_cast<void*>(baseAddress+rva),reinterpret_cast<void*>(&update),reinterpret_cast<void**>(&original))!=MH_OK)
        throw std::runtime_error("HUD compass hook creation failed");
    accepting=true;if(MH_EnableHook(reinterpret_cast<void*>(baseAddress+rva))!=MH_OK){accepting=false;throw std::runtime_error("HUD compass hook enable failed");}
    enabled=true;logFile->event("compass_head_enabled","\"rva\":"+hex(rva)+",\"pattern\":"+jsonString(pattern)+",\"heading\":\"HUD game data +0xb0, radians\",\"writes\":\"the heading for the HUD update only, put back after it\"");
}
inline void disable(){
    accepting=false;if(!enabled)return;
    if(MH_DisableHook(reinterpret_cast<void*>(baseAddress+rva))!=MH_OK)throw std::runtime_error("HUD compass hook disable failed");
    enabled=false;const auto deadline=GetTickCount64()+500;
    while(callbacks.load()&&GetTickCount64()<deadline)std::this_thread::sleep_for(std::chrono::milliseconds(1));
    const auto bytes=parsePattern(pattern);const bool same=scan(reinterpret_cast<const uint8_t*>(baseAddress+rva),bytes.size(),bytes)==std::vector<size_t>{0};
    logFile->event("compass_head_removed","\"restored\":"+std::string(same?"true":"false")+",\"applied\":"+std::to_string(applied.load())+",\"put_back\":"+std::to_string(restored.load())+
        ",\"game_wrote_meanwhile\":"+std::to_string(kept.load())+",\"failures\":"+std::to_string(failures.load())+",\"callbacks\":"+std::to_string(callbacks.load()));
    if(callbacks.load())throw std::runtime_error("HUD compass callbacks did not quiesce after hook removal");
    if(!same)throw std::runtime_error("HUD compass hook was not removed");
}
}
