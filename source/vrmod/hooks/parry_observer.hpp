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
constexpr uintptr_t rva=0x2ad100,callerRva=0x2b0d43;
constexpr const char* pattern="48 89 4C 24 08 55 53 56 57 48 8D AC 24 C8 FD FF FF 48 81 EC 38 03 00 00";
constexpr uintptr_t clientRva=0x2b0c80;constexpr const char* clientPattern="48 89 7C 24 18 55 48 8D 6C 24 A9 48 81 EC F0 00 00 00 41 B8";
constexpr uintptr_t serverRva=0x2c78e0,serverCallerRva=0x2dbd3b;
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
inline bool fresh(uint64_t at,uint64_t window){const auto now=GetTickCount64();return at&&now>=at&&now-at<=window;}
inline std::atomic<uint64_t> calls{0},withTargets{0},lastTargets{0},armed{0},natural{0};inline std::atomic<unsigned> inside{0};
inline std::atomic<uintptr_t> clientConsumer{0};
// The relaxed parry (2026-10-19; user: "swinging the shield right as the enemy strikes down, no parry and I take the hit;
// swinging as it raises the weapon, the parry works - I want the first to parry", and asked for it with the game's rules
// widened, knowing it is easier than the game and only the host's game decides the gameplay side). The game parries an
// enemy only (read in 1.0.0.1):
//   client, the choice 0x2acf60 -> per enemy 0x3b5110: the enemy's state ParryOportunity (StateFlag 4, its attack's
//     opening) is up (0x21a900 with 4) and it stands within 3 m (shapes' distance less half sizes) - else not chosen;
//   gameplay, the parry's target 0x3b6090: an enemy within 8 m, facing the player within 60 degrees, within 0.8 m + its
//     radius of the player's forward line, 1.1 m up or down, which closes in fast enough to reach the player in 0.4 s
//     ((distance along its movement - 0.6 - radius) / speed <= 400000000 ns at 0x3b670f; standing still: never).
// Headset run 20261019-124800 (bug report 125505): four swings while guarding, the choice made at the swing's own frame
// each time, 0 enemies every time - the striking enemy had stopped and its opening was over. So while the enemy's
// blow is landing - the player's own BlockOpportunity (StateFlag 5) is up then, for 0.16 .. 0.29 s before the hit in
// that run - a swing parries the enemy within 3 m without its opening: the client's choice is made once more with
// StateFlag 4 taken as up (0x21a900 answers true for 4 in that call only), and where the gameplay side then finds no
// target, it takes the one the client chose (0x9a5910, the entity of that id). A swing up to relaxAfterSwingMs before
// the blow lands makes the relaxed choice when BlockOpportunity comes up (0.2 s: the user, "0.4 is long, can it be 0.2?"). For the local player only (solo, or the
// host's own); a guest's parry is decided by the host's game. EVR_PARRY_RELAX=0: the game's rules only.
inline bool relax=true;inline bool relaxHooked=false;
constexpr uint64_t relaxAfterSwingMs=200,opportunityFreshMs=80,relaxedTargetMs=500;
constexpr uintptr_t flagTestRva=0x21aaa0,candidateRva=0x3b52b0,entityRva=0x9a6cf0;
constexpr const char* flagTestPattern="4C 8B C1 0F B6 CA BA 01 00 00 00 48 D3 E2 41 F6 80 B1 01 00 00 01";
constexpr const char* candidatePattern="48 89 6C 24 10 48 89 74 24 18 57 48 83 EC 70 48 8B F2 49 8B F9 B2 04";
constexpr const char* entityPattern="40 53 48 83 EC 20 48 8B 01 89 54 24 38 48 8D 54 24 38 48 8B 58 30";
using FlagTest=bool(__fastcall*)(void*,uint8_t);
using Candidate=bool(__fastcall*)(void*,void*,void*,void*,void*,void*);
using Entity=void*(__fastcall*)(void*,uint32_t);
inline FlagTest originalFlagTest=nullptr;inline Candidate originalCandidate=nullptr;inline Entity entityOf=nullptr;
inline thread_local bool relaxChoice=false,openingTaken=false,relaxGameplay=false;
inline thread_local bool remoteGameplay=false;   // inside serverParryRemote: the parry's target is the enemy the guest parried
inline std::atomic<uint64_t> relaxArm{0},relaxPressAt{0},relaxedAt{0},swingTargets{0},relaxedCandidates{0},relaxedTargets{0};
inline std::atomic<uint32_t> relaxedId{0};
inline bool __fastcall flagTest(void* actor,uint8_t flag){
    if(openingTaken&&flag==4)return true;
    return originalFlagTest(actor,flag);
}
inline bool __fastcall candidate(void* actor,void* b,void* c,void* d,void* e,void* f){
    auto r=originalCandidate(actor,b,c,d,e,f);
    if(!r&&relaxChoice){openingTaken=true;r=originalCandidate(actor,b,c,d,e,f);openingTaken=false;if(r)++relaxedCandidates;}
    return r;
}
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
constexpr uintptr_t abilityStartRva=0x37f190,abilityStartCaller=0x2c7aff,secondCheckRva=0x3b1860,secondCheckCaller=0x2c7b71,findTargetRva=0x3b6230,findTargetCaller=0x2c7bc0;
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
    const Inside counted;auto r=originalFindTarget(parameters,position,radius);
    const bool ours=conditions.local&&reinterpret_cast<uintptr_t>(_ReturnAddress())==baseAddress+findTargetCaller;
    if(ours)try{
        conditions.targetFound=r?1:0;conditions.radius=radius;
        const auto p=reinterpret_cast<uintptr_t>(position);for(size_t i=0;i<3;++i)conditions.player[i]=double(read<int64_t>(p+i*8))/4294967296.0;
    }catch(...){}
    // A guest's parry (serverParryRemote): the enemy that guest parried on its PC, whatever the search found.
    if(remoteGameplay&&reinterpret_cast<uintptr_t>(_ReturnAddress())==baseAddress+findTargetCaller&&entityOf)try{
        const auto id=controllerUiFixture::remoteParryTarget.load();const auto world=read<uintptr_t>(reinterpret_cast<uintptr_t>(parameters));
        if(id&&plausible(world)){if(auto* e=entityOf(reinterpret_cast<void*>(world),id)){r=e;conditions.targetFound=3;}}
    }catch(...){}
    // The relaxed parry's gameplay side: no enemy closes in, so the enemy the client chose without its opening (relaxedId)
    // is the target - the entity of that id, looked up as the search itself does (0x9a5910 with the parameters' first word).
    if(!r&&ours&&relaxGameplay&&entityOf&&fresh(relaxedAt.load(),relaxedTargetMs))try{
        const auto id=relaxedId.load();const auto world=read<uintptr_t>(reinterpret_cast<uintptr_t>(parameters));
        if(id&&plausible(world)){r=entityOf(reinterpret_cast<void*>(world),id);
            if(r)conditions.targetFound=2;
            static std::atomic<unsigned> told{0};
            if(told.fetch_add(1)<200)logFile->event("parry_relaxed_gameplay","\"frame\":"+std::to_string(liveFrame.frame)+",\"enemy\":"+std::to_string(id)+
                ",\"entity_found\":"+(r?"true":"false")+",\"chosen_ms_ago\":"+std::to_string(GetTickCount64()-relaxedAt.load()));}
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
        // The relaxed choice's first enemy (the targets: {uint32 ids, capacity}, written from the start), for the gameplay side.
        if(relaxChoice&&result){const auto ids=read<uintptr_t>(reinterpret_cast<uintptr_t>(targets));
            if(plausible(ids&~uintptr_t(7))){relaxedId=read<uint32_t>(ids);relaxedAt=GetTickCount64();++relaxedTargets;}}
        const auto n=++calls;if(result)++withTargets;
        if(n<=2000)logFile->event("parry_targets_chosen","\"frame\":"+std::to_string(liveFrame.frame)+",\"targets\":"+std::to_string(result)+
            ",\"calls\":"+std::to_string(n)+",\"with_targets\":"+std::to_string(withTargets.load())+",\"read_only\":true");
    }catch(...){}
    return result;
}
void __fastcall clientTask(void* task){
    const Inside counted;
    const auto at=controllerUiFixture::swingParryAt.load();const auto consumer=clientConsumer.load();
    const bool opening=relax&&relaxHooked&&fresh(controllerUiFixture::blockOpportunityMs.load(),opportunityFreshMs);
    // The relaxed press: the swing's own press chose nobody, and the enemy's blow comes to land within relaxAfterSwingMs.
    if(swingParry&&opening&&plausible(consumer)&&clientArm.load()==at&&relaxArm.load()!=at&&swingTargets.load()==0&&fresh(at,relaxAfterSwingMs)){
        relaxArm=at;
        const auto takenAt=consumer+8,pressedAt=consumer+0x18;constexpr uint64_t bit=parryBit;
        uint64_t taken=0,pressed=0;bool written=false;
        try{taken=read<uint64_t>(takenAt);pressed=read<uint64_t>(pressedAt);
            *reinterpret_cast<uint64_t*>(takenAt)=taken&~bit;*reinterpret_cast<uint64_t*>(pressedAt)=pressed|bit;written=true;}catch(...){}
        const auto before=calls.load();const auto candidatesBefore=relaxedCandidates.load();
        relaxChoice=true;originalClient(task);relaxChoice=false;
        const ObserverLastError preserve;
        try{
            if(written&&!(pressed&bit))*reinterpret_cast<uint64_t*>(pressedAt)&=~bit;
            const bool chosen=calls.load()!=before;relaxPressAt=GetTickCount64();
            logFile->event("parry_relaxed_client","\"frame\":"+std::to_string(liveFrame.frame)+",\"swing_ms_ago\":"+std::to_string(GetTickCount64()-at)+
                ",\"at_the_swing\":false,\"choice_made\":"+(chosen?"true":"false")+",\"targets\":"+std::to_string(chosen?lastTargets.load():0)+
                ",\"without_opening\":"+std::to_string(relaxedCandidates.load()-candidatesBefore)+",\"enemy\":"+std::to_string(relaxedId.load()));
        }catch(...){}
        return;
    }
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
    const auto before=calls.load();const auto candidatesBefore=relaxedCandidates.load();
    // The blow is landing already at the swing: the relaxed choice at once (and no second press for this swing).
    if(opening){relaxArm=at;relaxChoice=true;}
    originalClient(task);
    relaxChoice=false;
    const ObserverLastError preserve;
    try{
        if(written&&!(pressed&bit))*reinterpret_cast<uint64_t*>(pressedAt)&=~bit;
        const bool chosen=calls.load()!=before;swingTargets=chosen?lastTargets.load():0;
        if(opening){relaxPressAt=GetTickCount64();
            logFile->event("parry_relaxed_client","\"frame\":"+std::to_string(liveFrame.frame)+",\"swing_ms_ago\":0,\"at_the_swing\":true,\"choice_made\":"+(chosen?"true":"false")+
                ",\"targets\":"+std::to_string(chosen?lastTargets.load():0)+",\"without_opening\":"+std::to_string(relaxedCandidates.load()-candidatesBefore)+",\"enemy\":"+std::to_string(relaxedId.load()));}
        logFile->event("swing_parry_client","\"frame\":"+std::to_string(liveFrame.frame)+",\"swing_ms_ago\":"+std::to_string(GetTickCount64()-at)+
            ",\"parry_taken_before\":"+((taken&bit)?"true":"false")+",\"parry_pressed_before\":"+((pressed&bit)?"true":"false")+
            ",\"written\":"+(written?"true":"false")+",\"choice_made\":"+(chosen?"true":"false")+",\"targets\":"+std::to_string(chosen?lastTargets.load():0));
    }catch(...){}
}
// Another player's swing parry on this host (2026-10-21; the online session of 10-21 00:25: the guest's 8 swings while
// guarding made the client's press on the guest, but the host's game - which decides - saw that player in the block's
// sequence and parried nothing). When a guest's mod tells of a swing (controllerUiFixture::remoteSwingParryAt), the
// calls of the parry for the other players' actors see that actor out of the block's sequence and Block pressed and
// not taken, as for the own swing above, until one asks for the parry or the window ends; whether an enemy can be
// parried stays the game's (its own target search; the relaxed choice of the guest's client does not reach here).
// With several guests, every guarding guest's call tries within the window: only one with an enemy to parry parries.
// EVR_REMOTE_PARRY=0: off.
inline const bool remoteParry=[]{char v[4]{};const auto n=GetEnvironmentVariableA("EVR_REMOTE_PARRY",v,sizeof v);return !(n==1&&v[0]=='0');}();
inline std::atomic<uint64_t> remoteArm{0},remoteTried{0};inline std::atomic<unsigned> remoteTries{0},remoteLogged{0};
inline bool serverParryRemote(void* out,void* row,void* a,void* b,uintptr_t actor,uintptr_t consumer,uint64_t at){
    const auto r=reinterpret_cast<uintptr_t>(row);
    uint32_t pending=0;try{const auto p=read<uintptr_t>(r+serverPendingAt);pending=plausible(p)?read<uint32_t>(p+0x44):1;}catch(...){pending=1;}
    if(pending)return originalServer(out,row,a,b);
    if(remoteTried.exchange(at)!=at)remoteTries=0;const auto attempt=++remoteTries;
    uint16_t sequence=0;uint8_t flags=0;uint64_t taken=0,pressed=0;bool written=false,sequenceWritten=false;constexpr uint64_t bit=blockBit;
    try{
        sequence=read<uint16_t>(actor+0x18);flags=read<uint8_t>(actor+0x1b1);taken=read<uint64_t>(consumer);pressed=read<uint64_t>(consumer+8);
        if((flags&1)&&sequence==8){*reinterpret_cast<uint16_t*>(actor+0x18)=0;sequenceWritten=true;}
        *reinterpret_cast<uint64_t*>(consumer)=taken&~bit;*reinterpret_cast<uint64_t*>(consumer+8)=pressed|bit;written=true;
    }catch(...){}
    remoteGameplay=true;const bool result=originalServer(out,row,a,b);remoteGameplay=false;
    const ObserverLastError preserve;
    try{
        if(sequenceWritten&&read<uint16_t>(actor+0x18)==0)*reinterpret_cast<uint16_t*>(actor+0x18)=sequence;
        if(written){
            if(!result&&(taken&bit))*reinterpret_cast<uint64_t*>(consumer)|=bit;
            if(!(pressed&bit))*reinterpret_cast<uint64_t*>(consumer+8)&=~bit;
        }
        if(result)remoteArm=at;
        if((attempt==1||result)&&remoteLogged.fetch_add(1)<300)logFile->event("swing_parry_remote","\"frame\":"+std::to_string(liveFrame.frame)+",\"swing_ms_ago\":"+std::to_string(GetTickCount64()-at)+
            ",\"actor\":"+hex(actor)+",\"sequence\":"+std::to_string(sequence)+",\"in_sequence\":"+((flags&1)?"true":"false")+",\"written\":"+(written?"true":"false")+
            ",\"parry_asked\":"+(result?"true":"false")+",\"attempt\":"+std::to_string(attempt)+",\"target_found\":"+std::to_string(conditions.targetFound));
    }catch(...){}
    return result;
}
// A guest's own parry, decided on its PC (online_sync.hpp sendParries; user 2026-10-21 04:15: "what concerns the player's
// action is done on each PC; never wait for the host's answer before the own action shows"). The guest's game runs its
// own character's parry as well (a prediction; the guest's log of 10-21: swing_parry_gameplay at each swing); given the
// enemy the guest decided on, as the relaxed parry gives its chosen enemy (relaxedId), that prediction asks for the parry
// at once - the parry's movement shows on the guest without the host. The host makes the same parry (serverParryRemote).
inline std::atomic<unsigned> localDecisions{0};
inline void localDecided(uint32_t enemy,uint64_t swingAt){
    if(!relax||!swingParry||!enemy||!swingAt)return;
    const auto now=GetTickCount64();relaxedId=enemy;relaxedAt=now;relaxArm=swingAt;relaxPressAt=now;
    if(localDecisions.fetch_add(1)<200)try{logFile->event("parry_local_decided","\"frame\":"+std::to_string(liveFrame.frame)+",\"enemy\":"+std::to_string(enemy)+
        ",\"swing_ms_ago\":"+std::to_string(now-swingAt)+",\"client_choice_for_this_swing\":"+(clientArm.load()==swingAt?"true":"false"));}catch(...){}
}
bool __fastcall serverParry(void* out,void* row,void* a,void* b){
    const Inside counted;
    const auto caller=reinterpret_cast<uintptr_t>(_ReturnAddress());
    const auto at=controllerUiFixture::swingParryAt.load();
    const auto r=reinterpret_cast<uintptr_t>(row);
    uintptr_t actor=0,consumer=0;
    try{if(caller==baseAddress+serverCallerRva){actor=read<uintptr_t>(r+serverActorAt);consumer=read<uintptr_t>(r+serverConsumerAt);if(!plausible(actor)||!plausible(consumer))actor=consumer=0;}}catch(...){actor=0;}
    const bool local=actor&&actor==controllerUiFixture::playerBaseActor.load();
    if(!local&&actor&&consumer&&remoteParry&&swingParry){const auto remoteAt=controllerUiFixture::remoteSwingParryAt.load();
        if(remoteArm.load()!=remoteAt&&fresh(remoteAt,serverWindowMs)){struct Marked {Marked(){conditions=Conditions{};}~Marked(){conditions=Conditions{};}} marked;
            return serverParryRemote(out,row,a,b,actor,consumer,remoteAt);}}
    struct Marked {explicit Marked(bool l){conditions=Conditions{};conditions.local=l;}~Marked(){conditions=Conditions{};}} marked(local);
    // The gameplay side's window runs from the relaxed press when the swing made one after its own (0.2 s at the most).
    const auto relaxedPress=relaxPressAt.load();const bool relaxedSwing=relax&&relaxArm.load()==at&&relaxedPress>=at;
    const auto windowFrom=relaxedSwing?relaxedPress:at;
    if(!swingParry||!local||!consumer||clientArm.load()!=at||serverArm.load()==at||!fresh(windowFrom,serverWindowMs)){
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
    relaxGameplay=relaxedSwing&&fresh(relaxedAt.load(),relaxedTargetMs);
    const bool result=originalServer(out,row,a,b);
    relaxGameplay=false;
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
        {char v[8]{};const auto m=GetEnvironmentVariableA("EVR_PARRY_RELAX",v,sizeof v);relax=!(m==1&&v[0]=='0');}
        if(swingParry&&relax&&conditionsHooked)try{
            const auto call=baseAddress+0x2ad4c8;   // the choice's call of the per-enemy check
            if(read<uint8_t>(call)!=0xe8||uintptr_t(int64_t(call+5)+read<int32_t>(call+1))!=baseAddress+candidateRva)throw std::runtime_error("parry candidate caller changed");
            if(scan(reinterpret_cast<const uint8_t*>(baseAddress+entityRva),parsePattern(entityPattern).size(),parsePattern(entityPattern))!=std::vector<size_t>{0})throw std::runtime_error("entity lookup signature changed");
            entityOf=reinterpret_cast<Entity>(baseAddress+entityRva);
            hook(candidateRva,candidatePattern,reinterpret_cast<void*>(&candidate),reinterpret_cast<void**>(&originalCandidate),"parry candidate check");
            hook(flagTestRva,flagTestPattern,reinterpret_cast<void*>(&flagTest),reinterpret_cast<void**>(&originalFlagTest),"actor state flag test");
            relaxHooked=true;
            logFile->event("parry_relaxed_enabled","\"candidate\":"+hex(candidateRva)+",\"flag_test\":"+hex(flagTestRva)+",\"entity\":"+hex(entityRva)+
                ",\"after_swing_ms\":"+std::to_string(relaxAfterSwingMs)+",\"opportunity_fresh_ms\":"+std::to_string(opportunityFreshMs)+",\"local_player_only\":true");
        }catch(const std::exception& e){relaxHooked=false;try{logFile->event("parry_relaxed_failure","\"reason\":"+jsonString(e.what()));}catch(...){}}
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
    if(relaxHooked){relaxHooked=false;for(auto rva:{candidateRva,flagTestRva})MH_DisableHook(reinterpret_cast<void*>(baseAddress+rva));}
    const auto deadline=GetTickCount64()+500;while(inside.load()&&GetTickCount64()<deadline)std::this_thread::sleep_for(std::chrono::milliseconds(1));
    try{logFile->event("parry_observer_removed","\"status\":"+std::to_string(status)+",\"client_status\":"+std::to_string(clientStatus)+",\"gameplay_status\":"+std::to_string(serverStatus)+
        ",\"calls\":"+std::to_string(calls.load())+",\"with_targets\":"+std::to_string(withTargets.load())+",\"swing_parries_asked\":"+std::to_string(armed.load())+
        ",\"parries_asked_by_the_game\":"+std::to_string(natural.load())+
        ",\"relaxed_targets\":"+std::to_string(relaxedTargets.load())+",\"relaxed_candidates\":"+std::to_string(relaxedCandidates.load()));}catch(...){}
}
}
