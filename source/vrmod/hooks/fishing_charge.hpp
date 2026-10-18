// The distance of the fishing rod's cast follows the swing (VHVR: the game's draw is replaced by "speed of the rod's
// tip / 5 m/s", docs/analysis/vhvr_spec_20261010/equipment_tools.md 7.2, BowAndFishingPatches.cs:29-32).
// In the game the float lands 3 m + 15 m x StateMachine.chargeProgression ahead (30fff0: the read at 3101e8, the
// constants 12ad1c8 = 15 and 12ae488 = 3), and that charge is the time the button was held: 30f8b0, the handler of
// the state machine's charging state (one caller, the dispatcher at 31f012), writes
// min(1, seconds since the state began x 0.5) at StateMachine+44 on every update while the cast input
// (keen::fishing::Input.castInputPressed, context+19) is held, and asks for transition 1 - the cast - at the byte
// behind context+38 once it is not; letting the aim or the abort input go asks for transition 2 instead. A swing with
// a short pull of the trigger so cast 3..5 m (headset run game-20261013-005905: "it does not fly").
// After the game's own update, when that update asked for the cast and a cast by a swing has just been judged
// (controller_ui_fixture.hpp, FishingSwing), the charge becomes the larger of the game's and the swing's strength
// (evr::motion::swingCharge). Nothing else is written. The handler runs for every simulation of that tick (Mock run
// game-20261013-082211: three entries of the state for one hold), so every call inside the swing's time gets the
// same charge. It runs where the game is simulated: as a guest in another player's game the game's own charge stays.
namespace fishingCharge {
using Charging=void*(__fastcall*)(void*,uint8_t);
inline Charging original=nullptr;
inline bool requested=false,enabled=false;
inline std::atomic<bool> accepting{false};
inline std::atomic<unsigned> callbacks{0},casts{0},applied{0},failures{0};
inline std::atomic<unsigned> entered{0},updates{0},left{0},elsewhere{0};   // the handler's calls by mode, and from another caller
constexpr uintptr_t rva=0x388210,caller=0x39a577;
constexpr const char* pattern="48 89 5C 24 08 57 48 83 EC 30 48 8B 59 08 48 8B F9 0F B6 CA 84 D2 0F 84";
// context: +0 StateMachine*, +18 the tick's keen::fishing::Input, +38 the byte the handler asks a transition with.
constexpr uintptr_t machineCharge=0x44,contextWanted=0x38;constexpr uint8_t updateMode=1,castTransition=1;
struct CallbackScope{CallbackScope(){++callbacks;}~CallbackScope(){--callbacks;}};
__declspec(noinline) void* __fastcall charging(void* context,uint8_t mode){
    const auto from=reinterpret_cast<uintptr_t>(_ReturnAddress());
    const auto result=original(context,mode);
    if(from!=baseAddress+caller){++elsewhere;return result;}
    if(mode==0)++entered;else if(mode==updateMode)++updates;else ++left;
    if(mode!=updateMode||!accepting.load())return result;
    const CallbackScope callback;
    {const ObserverLastError preserve;
     try{
        const auto at=reinterpret_cast<uintptr_t>(context);
        const auto machine=read<uintptr_t>(at),wanted=read<uintptr_t>(at+contextWanted);
        if(!machine||!wanted||read<uint8_t>(wanted)!=castTransition)return result;   // still charging, or given up
        const auto now=GetTickCount64();const auto number=++casts;
        controllerUiFixture::FishingSwing swing;{std::lock_guard<std::mutex> lock(controllerUiFixture::fishingAimMutex);swing=controllerUiFixture::fishingSwing;}
        const float game=read<float>(machine+machineCharge);float charge=game;
        const bool bySwing=swing.serial&&swing.until>now;
        if(bySwing){
            charge=evr::motion::swingCharge(game,swing.strength);
            if(charge!=game){*reinterpret_cast<float*>(machine+machineCharge)=charge;++applied;}
        }
        if(number<=200){
            std::ostringstream o;o<<std::setprecision(6)<<"\"cast\":"<<number<<",\"by_swing\":"<<(bySwing?"true":"false")<<",\"swing_serial\":"<<swing.serial
                <<",\"swing_strength\":"<<swing.strength<<",\"game_charge\":"<<game<<",\"game_held_seconds\":"<<game*2<<",\"charge\":"<<charge
                <<",\"distance_m\":"<<evr::motion::castDistance(charge)<<",\"game_distance_m\":"<<evr::motion::castDistance(game)
                <<",\"machine\":"<<hex(machine)<<",\"thread\":"<<GetCurrentThreadId()<<",\"only_the_charge_changed\":true";
            logFile->event("live_fishing_charge",o.str());
        }
     }catch(const std::exception& e){if(failures.fetch_add(1)==0)logFile->event("live_fishing_charge_failure","\"reason\":"+jsonString(e.what()));}
    }
    return result;
}
inline void install(){
    if(!requested)return;
    inputPipeline::verify(rva,pattern,caller);
    if(MH_CreateHook(reinterpret_cast<void*>(baseAddress+rva),reinterpret_cast<void*>(&charging),reinterpret_cast<void**>(&original))!=MH_OK)
        throw std::runtime_error("fishing charge hook creation failed");
    accepting=true;if(MH_EnableHook(reinterpret_cast<void*>(baseAddress+rva))!=MH_OK){accepting=false;throw std::runtime_error("fishing charge hook enable failed");}
    enabled=true;logFile->event("live_fishing_charge_enabled","\"rva\":"+hex(rva)+",\"caller\":"+hex(caller)+",\"pattern\":"+jsonString(pattern)+",\"writes\":\"StateMachine.chargeProgression at the cast\"");
}
inline void disable(){
    accepting=false;if(!enabled)return;
    if(MH_DisableHook(reinterpret_cast<void*>(baseAddress+rva))!=MH_OK)throw std::runtime_error("fishing charge hook disable failed");
    enabled=false;const auto deadline=GetTickCount64()+500;
    while(callbacks.load()&&GetTickCount64()<deadline)std::this_thread::sleep_for(std::chrono::milliseconds(1));
    inputPipeline::verify(rva,pattern,caller);
    logFile->event("live_fishing_charge_removed","\"restored\":true,\"casts\":"+std::to_string(casts.load())+",\"applied\":"+std::to_string(applied.load())+
        ",\"failures\":"+std::to_string(failures.load())+",\"callbacks\":"+std::to_string(callbacks.load())+
        ",\"charging_entered\":"+std::to_string(entered.load())+",\"charging_updates\":"+std::to_string(updates.load())+",\"charging_left\":"+std::to_string(left.load())+",\"other_callers\":"+std::to_string(elsewhere.load()));
    if(callbacks.load())throw std::runtime_error("fishing charge callbacks did not quiesce after hook removal");
}
}
