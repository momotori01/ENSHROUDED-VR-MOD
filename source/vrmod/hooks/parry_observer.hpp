// The game's own parry, observed (2026-10-15, headset run 9, user: "the parry does not come out when I swing").
// A parry is not the block: it is the client-only input Parry (ClientOnlyPlayerInputType 72). The task
// client_player_trigger_parry (264f70) takes it once a press (3cc6f0 / 3cc690 at 264ff0 / 265003), and only when the
// player can block (AbilityFlag CanBlock 6 at 264fde) chooses up to four enemies to parry (261c90, called at 265029);
// with one or more it writes them into the client's input (ClientPlayerInputData +214, its version +210). The record of
// the player's own state has no parry opportunity (ParryOportunity, bit 4, never came up in headset runs 8 and 9 -
// it looks like the attacker's), so what is put on record here is the choice itself: a call means the press reached
// the parry with the block allowed, the count says whether an enemy could be parried at that moment. Mock regression
// game-20261015-012942: every press of the block reached the choice on the next frame.
// EVR_PARRY_OBSERVER=0 leaves it out.
//
// The swing parry (user 2026-10-15: "while guarding, a quick swing of the left hand parries at that moment - a parry
// you make intuitively by swinging the shield during the block"). The game parries only a press of the block made
// while no block runs: on the gameplay side the parry's sequence (trigger 9) is asked for by 277390, which leaves when
// the actor is in the block's sequence (BaseActor +1b1 bit 0 and +18 == 8, at 27743d / 27745a) and wants the input
// Block (28) pressed and not yet taken (3d2c60: the consumer's bits, taken +0, pressed +8). A swing while guarding
// (live_guard.hpp swingParry, controllerUiFixture::swingParryAt) is therefore made a new press for one call each:
//   client: before 264f70 runs, Parry 72 is pressed and not taken in the client consumer (taken +8, pressed +18, bit
//           8; the consumer is the one 261c90's row names, +48) - the game chooses the enemies itself;
//   gameplay: after that, for the local player's actor only, the calls of 277390 see the actor out of the block's
//           sequence (+18 written 0 for the call, 8 again afterwards unless the call changed it) and Block pressed and
//           not taken - the game asks for its parry itself, or not (no enemy to parry: all as it was after the call).
//           Every call of the window tries until one asks: the enemies the client chose may reach this side a frame or
//           two later. Mock regression game-20261015-020135 (no enemy): the client's choice was made at every swing, the
//           block's sequence passed, nothing asked - as for the game's own presses there (none asked a parry).
// Everything else - whether an enemy can be parried, the stun, the stamina - stays the game's. EVR_SWING_PARRY=0 leaves
// the swing parry out (the observation stays). The swing lets the block go for 0.35 s (live_guard.hpp swingReleaseMs,
// the risk of a missed parry); the block the guard then presses again by itself (repress) is no parry press: the
// client's Parry 72 of it is marked taken before 264f70 runs, so no enemy is chosen for it.
namespace parryObserver {
// 1.0 (2026-10-16): ClientOnlyPlayerInputType gained VoiceOverPlayback 57 - Parry is 73 (72; the bit in the consumer's second
// word 9, not 8 - 8 is UiSocial now), and PlayerInputType Block is 30 (28; 28 is QuickUse6_Release now). With June's bits
// the swing parry pressed and took the wrong inputs on 1.0.
constexpr uint64_t parryBit=1ull<<9,blockBit=1ull<<30;
using Choose=uint64_t(__fastcall*)(void*,void*);
using Task=void(__fastcall*)(void*);
using Start=bool(__fastcall*)(void*,void*,void*,void*);
inline Choose original=nullptr;inline Task originalClient=nullptr;inline Start originalServer=nullptr;
inline bool enabled=false,clientHooked=false,serverHooked=false,swingParry=true;
constexpr uintptr_t rva=0x2acf60,callerRva=0x2b0ba3;
constexpr const char* pattern="48 89 4C 24 08 55 53 56 57 48 8D AC 24 C8 FD FF FF 48 81 EC 38 03 00 00";
constexpr uintptr_t clientRva=0x2b0ae0;constexpr const char* clientPattern="48 89 7C 24 18 55 48 8D 6C 24 A9 48 81 EC F0 00 00 00 41 B8";
constexpr uintptr_t serverRva=0x2c7740,serverCallerRva=0x2dbb9b;
constexpr const char* serverPattern="4C 89 4C 24 20 48 89 4C 24 08 55 41 54 41 55 41 57 48 8D AC 24 F8 FE FF";
constexpr uint64_t clientWindowMs=150,serverWindowMs=300;
// Where the rows keep what is written (2026-10-17, read in the build 25988036 and the same in 1.0.0.1): until then June's
// places were read - the client's row +0x48 (in 1.0 a word and a byte: the swing parry wrote through 0x5a and the game
// crashed, auto-start Mock 20261017-012619, ACCESS_VIOLATION writing 0x62) and the gameplay row +0x98 / +0x158 / +0x138.
//   client task 0x2b0ae0: the consumer of ClientOnlyPlayerInput is row +0x70 ([rbp+0x17], row = rbp-0x59), passed with
//     Parry 0x49 to 0x46c2e0 (pressed and not taken: taken words +0/+8, pressed +0x10/+0x18) and 0x46c280 (take);
//   parry sequence 0x2c7740 (row = rdx): pending [row+0x298]+0x44, the consumer of Block 0x1e row +0x2f8 (0x472560 ->
//     0x46c310: taken +0, pressed +8), the actor row +0xb8 (its +0x1b1 bit 0 and +0x18 == 8: in the block's sequence).
constexpr size_t clientConsumerAt=0x70,serverActorAt=0xb8,serverConsumerAt=0x2f8,serverPendingAt=0x298;
inline bool plausible(uintptr_t p){return p>=0x10000&&p<0x7ff000000000ull&&!(p&7);}
inline std::atomic<uint64_t> calls{0},withTargets{0},lastTargets{0},armed{0},natural{0};inline std::atomic<unsigned> inside{0};
inline std::atomic<uintptr_t> clientConsumer{0};
inline std::atomic<uint64_t> clientArm{0},serverArm{0},serverTried{0};   // the swing (its time) the client took / the gameplay side asked for / tried
inline std::atomic<unsigned> tries{0},suppressed{0};
struct Inside {Inside(){++inside;}~Inside(){--inside;}};
// Why the game's own parry (277390) does not ask for its sequence (headset run 10, 2026-10-15: 24 presses of the block, the
// client chose an enemy twice, the sequence was never asked). 277390 (read in full on 2026-10-15): no sequence pending
// (row+138 -> +44), Block pressed and not taken (3d2c60, taken later at 2776b6), not in the block's sequence, CanBlock,
// then three questions whose answers are put on record here when 277390 runs for the local player: 307790 (its status
// byte, 0 = may start: the two entity references the caller passes, sequences 8 / 9), 333a70 (its status byte, 0 = may
// start) and 337fb0 - the parry target: the enemy found within 8 m of the player (row+20, a Q32 position) whose forward
// (a velocity-like vector, read at +0/+8 of the entity's second component) faces the player within 60 degrees, who stands
// within 0.8 m + its radius of the player's forward line (1.1 m up or down), and who reaches the player within 0.4 s
// ((distance - 0.6 - radius) / speed). None found: no parry. These are read-only (the originals run unchanged; only
// their results are read).
using AbilityStart=void*(__fastcall*)(void*,void*,void*,uint64_t,uint64_t,uint64_t,uint64_t,uint64_t);
using SecondCheck=void*(__fastcall*)(void*,void*,void*,void*,uint64_t);
using FindTarget=void*(__fastcall*)(void*,void*,float);
inline AbilityStart originalAbilityStart=nullptr;inline SecondCheck originalSecondCheck=nullptr;inline FindTarget originalFindTarget=nullptr;
inline bool conditionsHooked=false;
constexpr uintptr_t abilityStartRva=0x37eff0,abilityStartCaller=0x2c795f,secondCheckRva=0x3b16c0,secondCheckCaller=0x2c79d1,findTargetRva=0x3b6090,findTargetCaller=0x2c7a20;
constexpr const char* abilityStartPattern="48 89 5C 24 08 55 56 57 41 54 41 55 41 56 41 57 48 8D 6C 24 F9 48 81 EC 00 01 00 00";
constexpr const char* secondCheckPattern="48 89 6C 24 08 48 89 74 24 10 48 89 7C 24 18 41 56 48 83 EC 30 83 7A 0C 00 49";
constexpr const char* findTargetPattern="48 8B C4 48 89 58 20 48 89 50 10 55 56 57 41 54 41 55 41 56 41 57 48 8D A8 F8 FD FF FF";
struct Conditions {bool local=false;int abilityStatus=-1,secondStatus=-1,targetFound=-1;float radius=0;std::array<double,3> player{};};
inline thread_local Conditions conditions;
inline std::atomic<unsigned> refusedLogged{0};
void* __fastcall abilityStart(void* out,void* a,void* b,uint64_t c,uint64_t s1,uint64_t s2,uint64_t s3,uint64_t s4){
    const Inside counted;const auto r=originalAbilityStart(out,a,b,c,s1,s2,s3,s4);
    if(conditions.local&&reinterpret_cast<uintptr_t>(_ReturnAddress())==baseAddress+abilityStartCaller)try{conditions.abilityStatus=read<uint8_t>(reinterpret_cast<uintptr_t>(out));}catch(...){}
    return r;
}
void* __fastcall secondCheck(void* out,void* a,void* b,void* c,uint64_t s1){
    const Inside counted;const auto r=originalSecondCheck(out,a,b,c,s1);
    if(conditions.local&&reinterpret_cast<uintptr_t>(_ReturnAddress())==baseAddress+secondCheckCaller)try{conditions.secondStatus=read<uint8_t>(reinterpret_cast<uintptr_t>(out));}catch(...){}
    return r;
}
void* __fastcall findTarget(void* parameters,void* position,float radius){
    const Inside counted;const auto r=originalFindTarget(parameters,position,radius);
    if(conditions.local&&reinterpret_cast<uintptr_t>(_ReturnAddress())==baseAddress+findTargetCaller)try{
        conditions.targetFound=r?1:0;conditions.radius=radius;
        const auto p=reinterpret_cast<uintptr_t>(position);for(size_t i=0;i<3;++i)conditions.player[i]=double(read<int64_t>(p+i*8))/4294967296.0;
    }catch(...){}
    return r;
}
uint64_t __fastcall choose(void* targets,void* row){
    const auto caller=reinterpret_cast<uintptr_t>(_ReturnAddress());
    const Inside counted;
    const auto result=original(targets,row);
    if(caller!=baseAddress+callerRva)return result;
    const ObserverLastError preserve;
    try{
        {const auto c=read<uintptr_t>(reinterpret_cast<uintptr_t>(row)+clientConsumerAt);clientConsumer=plausible(c)?c:0;}lastTargets=result;
        const auto n=++calls;if(result)++withTargets;
        if(n<=2000)logFile->event("parry_targets_chosen","\"frame\":"+std::to_string(liveFrame.frame)+",\"targets\":"+std::to_string(result)+
            ",\"calls\":"+std::to_string(n)+",\"with_targets\":"+std::to_string(withTargets.load())+",\"read_only\":true");
    }catch(...){}
    return result;
}
inline bool fresh(uint64_t at,uint64_t window){const auto now=GetTickCount64();return at&&now>=at&&now-at<=window;}
void __fastcall clientTask(void* task){
    const Inside counted;
    const auto at=controllerUiFixture::swingParryAt.load();const auto consumer=clientConsumer.load();
    if(swingParry&&plausible(consumer)&&(clientArm.load()==at||!fresh(at,clientWindowMs))&&fresh(controllerUiFixture::guardRepressAt.load(),clientWindowMs)){
        try{*reinterpret_cast<uint64_t*>(consumer+8)|=parryBit;}catch(...){}   // the guard's own press again: Parry taken
        if(suppressed.fetch_add(1)<20)try{logFile->event("parry_input_taken_on_repress","\"frame\":"+std::to_string(liveFrame.frame));}catch(...){}
        originalClient(task);return;
    }
    if(!swingParry||!consumer||clientArm.load()==at||!fresh(at,clientWindowMs)){originalClient(task);return;}
    clientArm=at;
    const auto takenAt=consumer+8,pressedAt=consumer+0x18;constexpr uint64_t bit=parryBit;
    uint64_t taken=0,pressed=0;bool written=false;
    try{taken=read<uint64_t>(takenAt);pressed=read<uint64_t>(pressedAt);
        *reinterpret_cast<uint64_t*>(takenAt)=taken&~bit;*reinterpret_cast<uint64_t*>(pressedAt)=pressed|bit;written=true;}catch(...){}
    const auto before=calls.load();
    originalClient(task);
    const ObserverLastError preserve;
    try{
        if(written&&!(pressed&bit))*reinterpret_cast<uint64_t*>(pressedAt)&=~bit;
        const bool chosen=calls.load()!=before;
        logFile->event("swing_parry_client","\"frame\":"+std::to_string(liveFrame.frame)+",\"swing_ms_ago\":"+std::to_string(GetTickCount64()-at)+
            ",\"parry_taken_before\":"+((taken&bit)?"true":"false")+",\"parry_pressed_before\":"+((pressed&bit)?"true":"false")+
            ",\"written\":"+(written?"true":"false")+",\"choice_made\":"+(chosen?"true":"false")+",\"targets\":"+std::to_string(chosen?lastTargets.load():0));
    }catch(...){}
}
bool __fastcall serverParry(void* out,void* row,void* a,void* b){
    const Inside counted;
    const auto caller=reinterpret_cast<uintptr_t>(_ReturnAddress());
    const auto at=controllerUiFixture::swingParryAt.load();
    const auto r=reinterpret_cast<uintptr_t>(row);
    uintptr_t actor=0,consumer=0;
    try{if(caller==baseAddress+serverCallerRva){actor=read<uintptr_t>(r+serverActorAt);consumer=read<uintptr_t>(r+serverConsumerAt);if(!plausible(actor)||!plausible(consumer))actor=consumer=0;}}catch(...){actor=0;}
    const bool local=actor&&actor==controllerUiFixture::playerBaseActor.load();
    struct Marked {explicit Marked(bool l){conditions=Conditions{};conditions.local=l;}~Marked(){conditions=Conditions{};}} marked(local);
    if(!swingParry||!local||!consumer||clientArm.load()!=at||serverArm.load()==at||!fresh(at,serverWindowMs)){
        uint16_t sequence=0;uint8_t flags=0;uint64_t taken=0,pressed=0;uint32_t pending=0;bool known=false;
        if(local)try{sequence=read<uint16_t>(actor+0x18);flags=read<uint8_t>(actor+0x1b1);taken=read<uint64_t>(consumer);pressed=read<uint64_t>(consumer+8);
            pending=read<uint32_t>(read<uintptr_t>(r+serverPendingAt)+0x44);known=true;}catch(...){known=false;}
        const bool result=originalServer(out,row,a,b);
        if(result&&local){const ObserverLastError preserve;const auto n=++natural;
            if(n<=200)try{logFile->event("parry_sequence_asked","\"frame\":"+std::to_string(liveFrame.frame)+",\"by\":\"the game\",\"count\":"+std::to_string(n)+
                ",\"ability_status\":"+std::to_string(conditions.abilityStatus)+",\"second_status\":"+std::to_string(conditions.secondStatus)+",\"target_found\":"+std::to_string(conditions.targetFound));}catch(...){}}
        else if(local&&known){const ObserverLastError preserve;constexpr uint64_t bit=blockBit;
            // The block's press reached 277390 (pressed, not taken, no sequence pending, not in the block's sequence): why no parry
            const bool pressNew=(pressed&bit)&&!(taken&bit),inBlock=(flags&1)&&sequence==8;
            if(pressNew&&!pending&&!inBlock&&refusedLogged.fetch_add(1)<150)try{
                std::ostringstream f;f<<std::setprecision(5)<<"\"frame\":"<<liveFrame.frame<<",\"ability_status\":"<<conditions.abilityStatus<<",\"second_status\":"<<conditions.secondStatus
                    <<",\"target_found\":"<<conditions.targetFound<<",\"radius\":"<<conditions.radius<<",\"player\":["<<conditions.player[0]<<','<<conditions.player[1]<<','<<conditions.player[2]<<"],\"sequence\":"<<sequence;
                logFile->event("parry_sequence_refused",f.str());}catch(...){}}
        return result;
    }
    uint32_t pending=0;try{const auto p=read<uintptr_t>(r+serverPendingAt);pending=plausible(p)?read<uint32_t>(p+0x44):1;}catch(...){pending=1;}
    if(pending)return originalServer(out,row,a,b);   // a sequence is being asked for already: the next call
    if(serverTried.exchange(at)!=at)tries=0;const auto attempt=++tries;
    uint16_t sequence=0;uint8_t flags=0;uint64_t taken=0,pressed=0;bool written=false,sequenceWritten=false;constexpr uint64_t bit=blockBit;
    try{
        sequence=read<uint16_t>(actor+0x18);flags=read<uint8_t>(actor+0x1b1);taken=read<uint64_t>(consumer);pressed=read<uint64_t>(consumer+8);
        if((flags&1)&&sequence==8){*reinterpret_cast<uint16_t*>(actor+0x18)=0;sequenceWritten=true;}
        *reinterpret_cast<uint64_t*>(consumer)=taken&~bit;*reinterpret_cast<uint64_t*>(consumer+8)=pressed|bit;written=true;
    }catch(...){}
    const bool result=originalServer(out,row,a,b);
    const ObserverLastError preserve;
    try{
        if(sequenceWritten&&read<uint16_t>(actor+0x18)==0)*reinterpret_cast<uint16_t*>(actor+0x18)=sequence;
        if(written){
            if(!result&&(taken&bit))*reinterpret_cast<uint64_t*>(consumer)|=bit;   // not asked for: as it was
            if(!(pressed&bit))*reinterpret_cast<uint64_t*>(consumer+8)&=~bit;
        }
        if(result){serverArm=at;++armed;}
        if(attempt==1||result)logFile->event("swing_parry_gameplay","\"frame\":"+std::to_string(liveFrame.frame)+",\"swing_ms_ago\":"+std::to_string(GetTickCount64()-at)+
            ",\"sequence\":"+std::to_string(sequence)+",\"in_sequence\":"+((flags&1)?"true":"false")+",\"block_taken_before\":"+((taken&bit)?"true":"false")+
            ",\"block_pressed_before\":"+((pressed&bit)?"true":"false")+",\"written\":"+(written?"true":"false")+",\"parry_asked\":"+(result?"true":"false")+
            ",\"attempt\":"+std::to_string(attempt)+",\"parries_asked\":"+std::to_string(armed.load())+
            ",\"ability_status\":"+std::to_string(conditions.abilityStatus)+",\"second_status\":"+std::to_string(conditions.secondStatus)+",\"target_found\":"+std::to_string(conditions.targetFound));
    }catch(...){}
    return result;
}
inline void hook(uintptr_t at,const char* bytesText,void* detour,void** trampoline,const char* name){
    const auto bytes=parsePattern(bytesText);const auto site=baseAddress+at;
    if(scan(reinterpret_cast<const uint8_t*>(site),bytes.size(),bytes)!=std::vector<size_t>{0})throw std::runtime_error(std::string(name)+" signature changed");
    if(MH_CreateHook(reinterpret_cast<void*>(site),detour,trampoline)!=MH_OK||MH_EnableHook(reinterpret_cast<void*>(site))!=MH_OK)
        throw std::runtime_error(std::string(name)+" hook failed");
}
inline void install() noexcept {
    try{
        char value[8]{};const auto n=GetEnvironmentVariableA("EVR_PARRY_OBSERVER",value,sizeof value);
        if(n==1&&value[0]=='0')return;
        const auto call=baseAddress+callerRva-5;
        if(read<uint8_t>(call)!=0xe8||uintptr_t(int64_t(call+5)+read<int32_t>(call+1))!=baseAddress+rva)throw std::runtime_error("parry target choice caller changed");
        hook(rva,pattern,reinterpret_cast<void*>(&choose),reinterpret_cast<void**>(&original),"parry target choice");
        enabled=true;
        logFile->event("parry_observer_enabled","\"rva\":"+hex(rva)+",\"caller\":"+hex(callerRva)+",\"input\":\"ClientOnlyPlayerInputType Parry 73\"");
        try{
            for(auto [site,target]:{std::pair<uintptr_t,uintptr_t>{abilityStartCaller,abilityStartRva},{secondCheckCaller,secondCheckRva},{findTargetCaller,findTargetRva}}){
                const auto c=baseAddress+site-5;if(read<uint8_t>(c)!=0xe8||uintptr_t(int64_t(c+5)+read<int32_t>(c+1))!=baseAddress+target)throw std::runtime_error("parry condition caller changed");}
            hook(abilityStartRva,abilityStartPattern,reinterpret_cast<void*>(&abilityStart),reinterpret_cast<void**>(&originalAbilityStart),"parry ability start");
            hook(secondCheckRva,secondCheckPattern,reinterpret_cast<void*>(&secondCheck),reinterpret_cast<void**>(&originalSecondCheck),"parry second check");
            hook(findTargetRva,findTargetPattern,reinterpret_cast<void*>(&findTarget),reinterpret_cast<void**>(&originalFindTarget),"parry target search");
            conditionsHooked=true;logFile->event("parry_conditions_observed","\"ability_start\":"+hex(abilityStartRva)+",\"second_check\":"+hex(secondCheckRva)+",\"target_search\":"+hex(findTargetRva)+",\"read_only\":true");
        }catch(const std::exception& e){try{logFile->event("parry_conditions_failure","\"reason\":"+jsonString(e.what()));}catch(...){}}
        {char v[8]{};const auto m=GetEnvironmentVariableA("EVR_SWING_PARRY",v,sizeof v);swingParry=!(m==1&&v[0]=='0');}
        if(swingParry){
            const auto serverCall=baseAddress+serverCallerRva-5;
            if(read<uint8_t>(serverCall)!=0xe8||uintptr_t(int64_t(serverCall+5)+read<int32_t>(serverCall+1))!=baseAddress+serverRva)throw std::runtime_error("parry sequence caller changed");
            hook(clientRva,clientPattern,reinterpret_cast<void*>(&clientTask),reinterpret_cast<void**>(&originalClient),"client parry task");clientHooked=true;
            hook(serverRva,serverPattern,reinterpret_cast<void*>(&serverParry),reinterpret_cast<void**>(&originalServer),"parry sequence");serverHooked=true;
            logFile->event("swing_parry_enabled","\"client_task\":"+hex(clientRva)+",\"parry_sequence\":"+hex(serverRva)+",\"client_window_ms\":"+std::to_string(clientWindowMs)+
                ",\"gameplay_window_ms\":"+std::to_string(serverWindowMs));
        }else logFile->event("swing_parry_off","\"reason\":\"EVR_SWING_PARRY=0\"");
    }catch(const std::exception& e){
        swingParry=false;
        try{logFile->event("parry_observer_failure","\"reason\":"+jsonString(e.what())+",\"swing_parry\":false");}catch(...){}
    }
}
inline void disable() noexcept {
    if(!enabled)return;enabled=false;swingParry=false;
    const auto status=MH_DisableHook(reinterpret_cast<void*>(baseAddress+rva));
    const auto clientStatus=clientHooked?MH_DisableHook(reinterpret_cast<void*>(baseAddress+clientRva)):MH_OK;
    const auto serverStatus=serverHooked?MH_DisableHook(reinterpret_cast<void*>(baseAddress+serverRva)):MH_OK;
    if(conditionsHooked){conditionsHooked=false;for(auto rva:{abilityStartRva,secondCheckRva,findTargetRva})MH_DisableHook(reinterpret_cast<void*>(baseAddress+rva));}
    const auto deadline=GetTickCount64()+500;while(inside.load()&&GetTickCount64()<deadline)std::this_thread::sleep_for(std::chrono::milliseconds(1));
    try{logFile->event("parry_observer_removed","\"status\":"+std::to_string(status)+",\"client_status\":"+std::to_string(clientStatus)+",\"gameplay_status\":"+std::to_string(serverStatus)+
        ",\"calls\":"+std::to_string(calls.load())+",\"with_targets\":"+std::to_string(withTargets.load())+",\"swing_parries_asked\":"+std::to_string(armed.load())+
        ",\"parries_asked_by_the_game\":"+std::to_string(natural.load()));}catch(...){}
}
}
