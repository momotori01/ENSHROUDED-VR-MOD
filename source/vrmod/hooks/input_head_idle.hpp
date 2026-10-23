// Explicit bounded diagnostic, default off. Only patches a private input copy
// Default paths never synthesize controls. A separate explicit walking fixture
// creates bounded test-only analog input; its evidence is marked synthetic.
// The holsters (include/evr/holster.hpp, hooks/holster_actions.hpp): what a grip at a shoulder or at the hip asked
// for goes into the input that is sent - the action bar's selection, the equipment's change, the ranged-combat bit.
namespace holsterInput {
inline bool requested=false;inline std::mutex mutex;inline game::HolsterRequest request;inline game::HolsterSender sender;
inline std::atomic<bool> armed{false};inline std::atomic<unsigned> lines{0};
inline uint64_t ask(game::HolsterRequest r){std::lock_guard<std::mutex> lock(mutex);r.serial=request.serial+1;request=r;return r.serial;}
inline void apply(const game::ClientInputBytes& source,game::ClientInputBytes& output,uint32_t tick){
    if(!requested)return;
    game::HolsterSender::Applied a;uint64_t serial=0;
    {std::lock_guard<std::mutex> lock(mutex);serial=request.serial;a=sender.apply(request,source,output);}
    const bool armChanged=armed.exchange(a.arm)!=a.arm;
    if((a.barBegan||a.barEnded||a.equipBegan||a.equipEnded||armChanged)&&lines.fetch_add(1)<600){
        const auto bytes=[](const uint8_t* p,size_t n){static const char* digits="0123456789abcdef";std::string s;for(size_t i=0;i<n;++i){s+=digits[p[i]>>4];s+=digits[p[i]&15];}return s;};
        std::array<uint8_t,8> bar{};std::memcpy(bar.data(),source.data()+game::inputBarAction,8);std::array<uint8_t,16> equip{};std::memcpy(equip.data(),source.data()+game::inputEquipAction,16);
        try{logFile->event("holster_input","\"tick\":"+std::to_string(tick)+",\"request\":"+std::to_string(serial)+",\"ranged_combat_bit\":"+(a.arm?"true":"false")+
            ",\"selection_sent\":"+(a.barOn?jsonString(bytes(a.bar.data(),8)):std::string("null"))+",\"selection_began\":"+(a.barBegan?"true":"false")+",\"selection_ended_by_the_game\":"+(a.barEnded?"true":"false")+
            ",\"equipment_sent\":"+(a.equipOn?jsonString(bytes(a.equip.data(),16)):std::string("null"))+",\"equipment_began\":"+(a.equipBegan?"true":"false")+",\"equipment_ended_by_the_game\":"+(a.equipEnded?"true":"false")+
            ",\"game_selection\":"+jsonString(bytes(bar.data(),8))+",\"game_equipment\":"+jsonString(bytes(equip.data(),16)));}catch(...){}
    }
}
}
// A consumable chosen on the quick wheel is held, not used (include/evr/consumable_hold.hpp; 2026-10-23): the wheel's consume
// of a consumable on the action bar is kept back, the right hand is emptied (the action bar's selection "no slot", as the
// holsters send it) while the item is held, and at the mouth or by the right trigger (controller_ui_fixture.hpp
// consumableUse) the consume goes out; the slot selected before comes back after the use's sequence (1.1 .. 1.5 s,
// UseItem_* of docs/analysis/attacks_1_0_20261017). Another choice on the wheel lets the held item go. Throwables are
// weapons to the game (category 2) and go as before. EVR_CONSUMABLE_HOLD=0: the game's own use at once, as before.
namespace consumableHoldInput {
inline const bool requested=[]{char v[8]{};const auto n=GetEnvironmentVariableA("EVR_CONSUMABLE_HOLD",v,sizeof v);return !(n==1&&v[0]=='0');}();
inline std::mutex mutex;inline game::ConsumableHold hold;inline bool seeded=false;
// The action bar's consumable at an inventory slot {entity, index} (0: none, or not a consumable) and the selected slot
// (0..15, 16 none; -1 unknown) - set by the probe from live_actionbar_pages.hpp.
inline uint32_t (*consumableAt)(const game::InventorySlotBytes&)=nullptr;inline int (*selectedSlot)()=nullptr;
inline int restoreSlot=-1;inline uint64_t restoreAt=0,heldChoice=0;inline std::atomic<unsigned> lines{0};
inline void apply(const game::ClientInputBytes& source,game::ClientInputBytes& output,uint32_t tick){
    if(!requested)return;
    const auto now=GetTickCount64();const auto chosen=controllerUiFixture::quickWheelChosenMs.load();
    // the game's own item actions, on record as they change (the inventory slot's form is to be seen on the headset)
    {static game::ItemActionBytes seen{};static unsigned seenLines=0;game::ItemActionBytes a{};std::memcpy(a.data(),source.data()+game::inputItemAction,game::itemActionBytes);
     if(a!=seen){seen=a;if(seenLines++<300){static const char* digits="0123456789abcdef";std::string hexBytes;for(const auto b:a){hexBytes+=digits[b>>4];hexBytes+=digits[b&15];}
        const bool fresh=chosen&&now-chosen<=600;
        try{logFile->event("item_action_seen","\"tick\":"+std::to_string(tick)+",\"bytes\":\""+hexBytes+"\",\"type\":"+std::to_string(a[game::itemActionType])+
            ",\"after_wheel\":"+(fresh?"true":"false")+",\"consumable\":"+std::to_string(consumableAt?consumableAt([&]{game::InventorySlotBytes s{};std::memcpy(s.data(),a.data(),8);return s;}()):0));}catch(...){}}}}
    game::ConsumableHold::Result r;uint32_t held=0;bool dropped=false;
    {std::lock_guard<std::mutex> lock(mutex);
     if(!seeded){seeded=true;hold.seed(uint32_t(std::time(nullptr)));}
     // another choice on the wheel that held nothing within 0.7 s: the held item is let go (a weapon or a tool was chosen)
     if(hold.held()&&chosen&&chosen!=heldChoice&&now-chosen>700){hold.drop();dropped=true;restoreSlot=-1;}
     if(controllerUiFixture::consumableUse.exchange(false))hold.use();
     r=hold.apply(source,output,[&](const game::InventorySlotBytes& slot)->uint32_t{
        if(!chosen||now-chosen>600||!consumableAt)return 0;return consumableAt(slot);});
     if(r.deferred){heldChoice=chosen;
        if(restoreSlot<0&&selectedSlot){const int s=selectedSlot();if(s>=0&&s<16){restoreSlot=s;game::HolsterRequest q;q.select=16;holsterInput::ask(q);}}}
     if(r.used&&restoreSlot>=0)restoreAt=now+1600;
     if(restoreAt&&now>=restoreAt){restoreAt=0;if(restoreSlot>=0&&!hold.held()){game::HolsterRequest q;q.select=restoreSlot;holsterInput::ask(q);}restoreSlot=-1;}
     held=hold.item();}
    controllerUiFixture::consumableHeld.store(held);
    if((r.deferred||r.used||dropped)&&lines.fetch_add(1)<400){
        uint32_t entity=0;std::memcpy(&entity,r.slot.data(),4);
        try{logFile->event(r.deferred?"consumable_held":r.used?"consumable_used":"consumable_dropped","\"tick\":"+std::to_string(tick)+",\"item\":"+std::to_string(r.item)+
            ",\"slot_entity\":"+std::to_string(entity)+",\"slot_index\":"+std::to_string(r.slot[4])+",\"restore_slot\":"+std::to_string(restoreSlot));}catch(...){}
    }
}
}
namespace inputHeadIdle {
inline bool requested=false,walkingRequested=false,fixtureRequested=false;
// The way the stick is turned back while the guard turns the sent camera. Mock game-20261014-221432: with +1 the walk
// went 40 degrees the wrong way (head 131, guard 91, walk 51), so -1 (EVR_GUARD_STICK_SIGN=1 the other way).
inline const float guardStickSign=[]{char v[8]{};const auto n=GetEnvironmentVariableA("EVR_GUARD_STICK_SIGN",v,sizeof v);return n==1&&v[0]=='1'?1.f:-1.f;}();
inline bool vrLocomotion=false; // live play input: head camera also for buttons and airborne states (walking_head_camera.hpp)
inline bool (*meleeEvidence)(uint32_t,uint64_t)=nullptr;
// Placing by hand (pointer_aim_camera.hpp): in building mode, and while a building piece or building tool is the
// selected item (heldPiece, set by the policy), the ray of the game's cursor starts at the right hand's pointer
// (building_cursor_ray.hpp). Here only the wish is published, per tick of the sent input: 1 a piece in the hand,
// 2 building mode - where the sent input also takes the head camera, as on the HUD; 3 a tool that is swung (pickaxe,
// rake): its aim is the headset's ray, not the hand's. heldPiece (set by the policy) returns 0, 1 or 3.
inline bool pointerAimRequested=false;inline uint8_t (*heldPiece)(uint64_t)=nullptr;
struct CursorAim {uint64_t milliseconds=0;uint32_t entity=0;uint8_t mode=0;};
inline CursorAim cursorAim;inline std::mutex cursorAimMutex;
inline std::atomic<unsigned> buildingHeadApplied{0};
inline game::WalkingInputFixture fixture;inline std::mutex fixtureMutex;
struct State {uint32_t entity=0,flags=0;uint64_t milliseconds=0,actorState=0;
    bool actorKnown=false,gravityPresent=false,gravityActive=false,finalActor=false;Vec3 gravityDirection{};};
inline State latest;inline std::mutex mutex;
// The player's toggleConfig of the input the game sends (a bit per keen::ecs::PlayerInputType: that input switches with
// each press instead of being held), for the sneak by crouching (2026-10-18, Sneak 55 in 1.0).
inline std::atomic<uint64_t> toggleConfig{0};inline std::atomic<bool> toggleConfigKnown{false};
inline std::atomic<unsigned> attempted{0},applied{0};
inline void observeState(uint32_t entity,uint32_t flags,uintptr_t actor,uintptr_t gravity,bool finalActor=false){
    if(!requested)return;
    State value;value.entity=entity;value.flags=flags;value.milliseconds=GetTickCount64();value.finalActor=finalActor;
    menuPointer::playerFlags.store(flags);menuPointer::playerFlagsMs.store(value.milliseconds);   // the death screen (menuPointer::uiCover)
    if(walkingRequested){
        // Publish no stale positive Actor/Gravity proof after a failed read.
        {std::lock_guard<std::mutex> lock(mutex);latest={};}
        if(!actor)throw std::runtime_error("walking actor state absent");
        const auto raw=read<uint64_t>(actor+0xbf8),add=read<uint64_t>(actor+0xbd0),remove=read<uint64_t>(actor+0xbd8);
        const auto sequence=read<uint8_t>(actor+0x1b1);
        value.actorState=sequence&1?(raw|add)&~remove:raw;value.actorKnown=true;
        value.gravityPresent=gravity!=0;value.gravityActive=gravity&&read<uint8_t>(gravity+0x10)!=0;
        if(gravity)value.gravityDirection=read<Vec3>(gravity);
        if(!finite(value.gravityDirection))throw std::runtime_error("walking gravity nonfinite");
    }
    std::lock_guard<std::mutex> lock(mutex);latest=value;
}
inline void patch(const game::ClientInputBytes& source,game::ClientInputBytes& output,
                  uintptr_t context,uint32_t entity,uint32_t tick,bool freecam){
    if(!requested){output=source;return;}
    {   // What the game itself puts into the input it sends, where it changes (read only; for the holsters of
        // 2026-10-13: which bits a hotkey of the action bar and the ranged weapon's key set, and for how long):
        // digitalInput (+318, a bit per keen::ecs::PlayerInputType), actionbarSelectionAction (+60: version, selected
        // index) and changeEquipmentAction (+17c: inventory slot, version, equipment slot, equip / unequip).
        // toggleConfig (+508): the inputs the player has set to "press again to end" instead of "hold".
        thread_local uint64_t seenDigital=~0ull;thread_local std::array<uint8_t,8> seenBar{};thread_local std::array<uint8_t,16> seenEquip{};static std::atomic<unsigned> lines{0};
        uint64_t toggles=0;std::memcpy(&toggles,source.data()+0x598,8);   // toggleConfig (0x508 before 1.0)
        toggleConfig.store(toggles);toggleConfigKnown.store(true);
        uint64_t digital=0;std::memcpy(&digital,source.data()+game::clientDigital,8);std::array<uint8_t,8> bar{};std::memcpy(bar.data(),source.data()+game::inputBarAction,8);std::array<uint8_t,16> equip{};std::memcpy(equip.data(),source.data()+game::clientEquipAction,16);
        const auto bytes=[](const uint8_t* p,size_t n){static const char* digits="0123456789abcdef";std::string s;for(size_t i=0;i<n;++i){s+=digits[p[i]>>4];s+=digits[p[i]&15];}return s;};
        if((digital!=seenDigital||bar!=seenBar||equip!=seenEquip)&&lines.load()<1500){++lines;seenDigital=digital;seenBar=bar;seenEquip=equip;
            try{logFile->event("input_actions_seen","\"tick\":"+std::to_string(tick)+",\"entity\":"+std::to_string(entity)+",\"digital\":"+hex(digital)+
                ",\"actionbar_selection\":"+jsonString(bytes(bar.data(),bar.size()))+",\"change_equipment\":"+jsonString(bytes(equip.data(),equip.size()))+",\"configured_as_toggle\":"+hex(toggles)+",\"read_only\":true");}catch(...){}}
    }
    const auto now=GetTickCount64();const auto ui=inputUi::snapshot();
    State state;{std::lock_guard<std::mutex> lock(mutex);state=latest;}
    const auto value=inputPose::channel.read(entity,now,inputPose::observationMaxAgeMs);
    game::IdleHeadGate guard;guard.normalUiBound=inputUi::boundGameplay(ui,context,entity,now);   // aiming (aux 3) included: report 11 of 2026-10-18
    guard.playerStateCurrent=state.entity==entity&&state.milliseconds&&now>=state.milliseconds&&now-state.milliseconds<=200&&read<uint8_t>(context+0x2c51);   // the byte the input message builder copies after the two of +0x5a0 (1.0 0x3a2395: [context+0x2c51]; June 0x3272f7: [context+0x5c0+0xd09]). 2026-10-16: read at +0x648+0xd09 after the 1.0 port, it was never set and the head never steered the walk (headset run 20261016-134128, every Mock since 1.0)
    guard.playerFlags=state.flags;guard.freeCamera=freecam;
    thread_local game::InputPoseToken last;thread_local float yaw=0;
    if(!(last==value.token)){
        Quat q;std::memcpy(&q,source.data()+0x38,sizeof(q));yaw=heading(normalized(q),0);last=value.token;
    }
    game::WalkingHeadGate walking;walking.player=guard;
    walking.actorStateCurrent=guard.playerStateCurrent&&state.actorKnown;walking.actorState=state.actorState;
    walking.gravityPresent=state.gravityPresent;walking.gravityActive=state.gravityActive;walking.gravityDirection=state.gravityDirection;
    {static const bool swimHead=[]{char value[8]{};const auto n=GetEnvironmentVariableA("EVR_SWIM_HEAD_CAMERA",value,sizeof value);return !(n&&n<sizeof value&&value[0]=='0');}();
     walking.swimHead=swimHead;}
    const bool melee=walkingRequested&&meleeEvidence&&meleeEvidence(entity,now);
    auto result=vrLocomotion&&walkingRequested?game::patchVrLocomotionCamera(source,output,walking,value.head,yaw):
        walkingRequested?game::patchMeleeHeadCamera(source,output,walking,value.head,yaw,melee):
        game::patchIdleHeadCamera(source,output,guard,value.head,yaw);
    // The HUD's compass turns with the head: how far the sent camera looks away from the game's own (compass_head.hpp).
    // Told before the fishing rod's turn below, which is not the head's.
    if(result.status==game::IdleHeadStatus::Applied&&vrLocomotion&&compassHead::requested)compassHead::tell(source.data()+0x38,output.data()+0x38,now);
    // The fishing rod's cast: for a short while the camera of the sent input looks along the swing (the yaw only; the
    // game takes the float's direction from this camera, controller_ui_fixture.hpp FishingAim). The swing is a
    // direction in tracking space; it goes through the pointer's conversion, as the hand's ray does.
    // Whatever the walking camera did with this input: while the rod is charged the game holds the player in a state
    // (player flags / the fishing camera) in which the head's camera is not sent, and the float went where the right
    // stick had turned the game's camera - "not where I swing, not where I look, only the stick turns it" (headset run
    // 20261014-162900: the float was out 95 ms after the trigger was let go, the turned camera came at 122 ms).
    // Not even while the game shows a screen of its own: the charge of the rod binds one (its meter), and with "not while
    // a screen is bound" the swing still came 10 ms after the float (headset run 20261014-203029: let go at 998367, the
    // float at +112 ms, the swing's camera at +122 ms - "the rod's direction is not fixed"). The aim lives 800 ms after a
    // cast of the rod only; no menu sets it.
    if(vrLocomotion){
        controllerUiFixture::FishingAim aim;{std::lock_guard<std::mutex> lock(controllerUiFixture::fishingAimMutex);aim=controllerUiFixture::fishingAim;}
        if(aim.until>now){
            auto swing=value.head;const float half=std::atan2(-aim.direction.x,-aim.direction.z)*.5f;
            swing.pointer={Quat{0,std::sin(half),0,std::cos(half)},value.head.head.position};swing.pointerFlags=value.head.flags;
            const auto proposal=game::proposePointer(swing,yaw);
            static std::atomic<uint64_t> logged{0};
            if(proposal.active()){
                Quat before;std::memcpy(&before,output.data()+0x38,sizeof(Quat));
                std::memcpy(output.data()+0x38,&proposal.aimWithoutRoll,sizeof(Quat));
                if(logged.exchange(aim.serial)!=aim.serial){std::ostringstream o;o<<std::setprecision(6)<<"\"tick\":"<<tick<<",\"serial\":"<<aim.serial<<",\"swing\":["<<aim.direction.x<<','<<aim.direction.y<<','<<aim.direction.z
                    <<"],\"swing_yaw\":"<<proposal.yaw<<",\"head_orientation\":["<<before.x<<','<<before.y<<','<<before.z<<','<<before.w<<"],\"sent_orientation\":["<<proposal.aimWithoutRoll.x<<','<<proposal.aimWithoutRoll.y<<','<<proposal.aimWithoutRoll.z<<','<<proposal.aimWithoutRoll.w
                    <<"],\"aim_forward\":["<<proposal.aimForward.x<<','<<proposal.aimForward.y<<','<<proposal.aimForward.z<<"],\"walking_camera\":\""<<game::idleHeadStatusName(result.status)<<"\",\"only_the_sent_camera_orientation_changed\":true";
                    logFile->event("live_fishing_aim",o.str());}
            }
        }
    }
    // The bow's shot (2026-10-19, controllerUiFixture::BowAim): the sent camera looks the shot's way for a moment after the
    // release - a client's arrows are made by the host's server from this camera (the friend's arrows flew up).
    // Only on a client (nativeImpactComponents::clientNow): a host's or single player's own arrows are aimed on its server
    // (bow_tracked_aim_call). And only the camera's orientation: 0.9.0 sent the whole shot camera (its place at the bow,
    // first-person distance 0) to every player, and after the first shot the bow never went back to aiming - no second
    // arrow (Mock game-20261019-065035: 3 releases, 1 arrow; -065501 without it: 3 arrows; camera 7 = aiming came back
    // for each draw only there). EVR_BOW_SENT_CAMERA: 0 off, 1 (default) as above, 2 the whole camera on a client (0.9.0's).
    static const int bowSentCamera=[]{char v[4]{};return GetEnvironmentVariableA("EVR_BOW_SENT_CAMERA",v,sizeof v)==1&&v[0]>='0'&&v[0]<='2'?v[0]-'0':1;}();
    if(vrLocomotion&&bowSentCamera&&nativeImpactComponents::clientNow(now)){
        controllerUiFixture::BowAim bow;{std::lock_guard<std::mutex> lock(controllerUiFixture::fishingAimMutex);bow=controllerUiFixture::bowAim;}
        if(bow.until>now){
            if(bowSentCamera==2){std::memcpy(output.data()+0x20,bow.camera.data(),24);const float firstPersonDistance=0;std::memcpy(output.data()+0x48,&firstPersonDistance,4);}
            std::memcpy(output.data()+0x38,bow.camera.data()+24,16);
            static std::atomic<uint64_t> logged{0};
            if(logged.exchange(bow.serial)!=bow.serial)try{logFile->event("bow_sent_camera","\"tick\":"+std::to_string(tick)+",\"serial\":"+std::to_string(bow.serial)+
                ",\"camera_bytes\":"+jsonString([&]{static const char* d="0123456789abcdef";std::string s;for(const auto v:bow.camera){s+=d[v>>4];s+=d[v&15];}return s;}())+",\"walking_camera\":\""+game::idleHeadStatusName(result.status)+"\""+
                ",\"orientation_only\":"+(bowSentCamera==2?"false":"true"));}catch(...){}
        }
    }
    // The guard faces where the hand holds it (user 2026-10-14: "decide the guard by the hand's direction, not the
    // camera's"): while the live guard holds the block, the sent camera looks the way of the back of the shield hand (or
    // from the head to a weapon held across), and bodyHeading - for the guard only - makes the character look along this
    // camera; the game blocks toward where the character looks. The stick is turned back by the same angle, so walking
    // still goes where the head looks. Not while a cast of the rod is aimed.
    if(vrLocomotion&&result.status==game::IdleHeadStatus::Applied){
        controllerUiFixture::FishingAim guard,fishing;{std::lock_guard<std::mutex> lock(controllerUiFixture::fishingAimMutex);guard=controllerUiFixture::guardAim;fishing=controllerUiFixture::fishingAim;}
        if(guard.until>now&&fishing.until<=now){
            auto hand=value.head;const float half=std::atan2(-guard.direction.x,-guard.direction.z)*.5f;
            hand.pointer={Quat{0,std::sin(half),0,std::cos(half)},value.head.head.position};hand.pointerFlags=value.head.flags;
            const auto proposal=game::proposePointer(hand,yaw);
            if(proposal.active()){
                Quat before;std::memcpy(&before,output.data()+0x38,sizeof(Quat));
                const float turn=heading(normalized(before),0)-heading(normalized(proposal.aimWithoutRoll),0);
                std::array<float,2> stick;std::memcpy(stick.data(),output.data()+game::clientStick,sizeof(stick));
                const float c=std::cos(turn*guardStickSign),s=std::sin(turn*guardStickSign);
                const std::array<float,2> turned{c*stick[0]-s*stick[1],s*stick[0]+c*stick[1]};
                std::memcpy(output.data()+0x38,&proposal.aimWithoutRoll,sizeof(Quat));
                if(std::isfinite(turned[0])&&std::isfinite(turned[1]))std::memcpy(output.data()+game::clientStick,turned.data(),sizeof(turned));
                // the character's front while guarding (the game's +Z, as its hit angle takes it), level
                {auto f=rotate(normalized(proposal.aimWithoutRoll),Vec3{0,0,1});f.y=0;const float l=std::sqrt(f.x*f.x+f.z*f.z);
                 if(l>1e-4f&&std::isfinite(l)){std::lock_guard<std::mutex> lock(controllerUiFixture::fishingAimMutex);controllerUiFixture::guardWorldForward={f.x/l,0,f.z/l};controllerUiFixture::guardWorldForwardMs=now;}}
                static std::atomic<uint64_t> logged{0};static std::atomic<unsigned> lines{0};
                if(logged.exchange(guard.serial)!=guard.serial||(stick[0]!=0||stick[1]!=0)&&lines.load()<40){++lines;std::ostringstream o;o<<std::setprecision(6)<<"\"tick\":"<<tick<<",\"serial\":"<<guard.serial
                    <<",\"guard\":["<<guard.direction.x<<','<<guard.direction.z<<"],\"guard_yaw\":"<<proposal.yaw<<",\"turn\":"<<turn<<",\"stick\":["<<stick[0]<<','<<stick[1]<<"],\"sent_stick\":["<<turned[0]<<','<<turned[1]<<']';
                    logFile->event("live_guard_aim",o.str());}
            }
        }
    }
    uint8_t cursorMode=0;
    if(pointerAimRequested&&vrLocomotion&&walkingRequested&&!fixtureRequested){
        if(inputUi::boundBuilding(ui,context,entity,now)){
            // The building screen is not the HUD, so the head camera is taken here as live play takes it on the HUD.
            game::PointerAimGate aim;aim.walking=walking;aim.buildingScreenBound=true;
            result=game::patchBuildingHeadCamera(source,output,aim,value.head,yaw);
            if(result.status==game::IdleHeadStatus::Applied){cursorMode=2;++buildingHeadApplied;}
        }else if(result.status==game::IdleHeadStatus::Applied&&heldPiece)cursorMode=heldPiece(now);
        {std::lock_guard<std::mutex> lock(cursorAimMutex);cursorAim={now,entity,cursorMode};}
        static std::atomic<int> logged{-1};static std::atomic<unsigned> lines{0};
        if(logged.exchange(cursorMode)!=cursorMode&&lines.fetch_add(1)<400){
            std::ostringstream o;o<<"\"tick\":"<<tick<<",\"entity\":"<<entity<<",\"cursor_mode\":"<<unsigned(cursorMode)<<",\"building_mode\":"<<(cursorMode==2?"true":"false")
                <<",\"held_piece\":"<<(cursorMode==1?"true":"false")<<",\"swung_tool\":"<<(cursorMode==3?"true":"false")<<",\"head_status\":"<<jsonString(game::idleHeadStatusName(result.status))
                <<",\"client_mask\":"<<unsigned(source[game::clientMask])<<",\"ui_screen\":"<<unsigned(ui.screen);
            logFile->event("input_pointer_aim",o.str());}
    }
    game::WalkFixtureCommand command;auto effectiveSource=source;
    if(fixtureRequested){
        {std::lock_guard<std::mutex> lock(fixtureMutex);
         command=fixture.update(now,value.token.origin,value.head.anchor.root,value.head.anchor.worldUnitsPerMeter,
             result.status==game::IdleHeadStatus::Applied,game::nativeControlsIdle(source),value.token.session);}
        if(command.synthetic){
            game::writeFixtureControls(effectiveSource,command);
            result=game::patchWalkingHeadCamera(effectiveSource,output,walking,value.head,yaw);
            if(result.status!=game::IdleHeadStatus::Applied)throw std::runtime_error("walking fixture preflight/application disagreement");
        }
    }
    const bool changed=result.status==game::IdleHeadStatus::Applied;
    if(changed){++applied;yaw=result.proposal.yaw;}
    const auto n=++attempted;
    thread_local game::IdleHeadStatus previousStatus=game::IdleHeadStatus::Invalid;
    const bool statusChanged=result.status!=previousStatus;previousStatus=result.status;
    // All applied ticks are kept for matching actual prediction/history and
    // native movement consumers; rejected ticks are sampled to bound logging.
    if(changed||statusChanged||n<=8||n%128==0||fixtureRequested){
        std::array<float,2> stick;uint64_t digital=0;
        std::memcpy(stick.data(),source.data()+game::clientStick,sizeof(stick));std::memcpy(&digital,source.data()+game::clientDigital,sizeof(digital));
        std::ostringstream out;out<<std::setprecision(10)<<"\"attempt\":"<<n<<",\"entity\":"<<entity<<",\"tick\":"<<tick
            <<",\"status\":"<<jsonString(game::idleHeadStatusName(result.status))<<",\"status_transition\":"<<(statusChanged?"true":"false")<<",\"applied\":"<<(changed?"true":"false")
            <<",\"idle_only_diagnostic\":"<<(walkingRequested?"false":"true")<<",\"normal_ui_bound\":"<<(guard.normalUiBound?"true":"false")
            <<",\"state_from_final_actor\":"<<(state.finalActor?"true":"false")<<",\"player_state_current\":"<<(guard.playerStateCurrent?"true":"false")<<",\"player_flags\":"<<state.flags
            <<",\"ui_sample\":"<<ui.number<<",\"ui_screen\":"<<unsigned(ui.screen)
            <<",\"ui_age_ms\":"<<(now>=ui.milliseconds?now-ui.milliseconds:UINT64_MAX)
            <<",\"state_age_ms\":"<<(now>=state.milliseconds?now-state.milliseconds:UINT64_MAX)
            <<",\"frame\":"<<value.head.xrFrame<<",\"time\":"<<value.head.predictedDisplayTime
            <<",\"session_generation\":"<<value.token.session<<",\"origin_epoch\":"<<value.token.origin
            <<",\"head_flags\":"<<value.head.flags<<",\"head_age_ms\":"<<value.headAgeMs<<",\"root_age_ms\":"<<value.rootAgeMs
            <<",\"native_input_mode\":"<<unsigned(source[game::clientInputMode])<<",\"native_digital\":"<<digital
            <<",\"native_stick\":["<<stick[0]<<','<<stick[1]<<"],\"client_mask\":"<<unsigned(source[game::clientMask])
            <<",\"building_camera\":"<<unsigned(source[game::clientBuildingCamera])<<",\"free_camera\":"<<(freecam?"true":"false")
            <<",\"melee_head_evidence\":"<<(melee?"true":"false")<<",\"cursor_mode\":"<<unsigned(cursorMode)
            <<",\"native_preserved\":"<<(source==output?"true":"false")
            <<",\"only_camera_bytes_changed\":"<<(game::onlyHeadCameraChanged(source,output)?"true":"false");
        if(fixtureRequested){
            std::array<float,2> effectiveStick;std::memcpy(effectiveStick.data(),effectiveSource.data()+game::clientStick,sizeof(effectiveStick));
            out<<",\"controller_fixture_requested\":true,\"synthetic_controls\":"<<(command.synthetic?"true":"false")
               <<",\"fixture_state\":"<<unsigned(command.state)<<",\"fixture_reason\":"<<jsonString(command.reason)
               <<",\"fixture_phase\":"<<command.phase<<",\"fixture_elapsed_ms\":"<<command.elapsedMs
               <<",\"fixture_settle_ms\":"<<game::WalkingInputFixture::settleMs
               <<",\"fixture_phase_ms\":"<<game::WalkingInputFixture::phaseMs
               <<",\"fixture_pulse_ms\":"<<game::WalkingInputFixture::pulseMs
               <<",\"fixture_run_ms\":"<<game::WalkingInputFixture::runMs
               <<",\"effective_input_mode\":"<<unsigned(effectiveSource[game::clientInputMode])
               <<",\"effective_stick\":["<<effectiveStick[0]<<','<<effectiveStick[1]<<']'
               <<",\"only_fixture_and_camera_bytes_changed\":"<<(game::onlyFixtureAndCameraChanged(source,output)?"true":"false");
        }
        if(walkingRequested)out<<",\"walking_only_diagnostic\":true,\"actor_state_current\":"<<(walking.actorStateCurrent?"true":"false")
            <<",\"actor_state_effective\":"<<state.actorState<<",\"gravity_present\":"<<(state.gravityPresent?"true":"false")
            <<",\"gravity_active\":"<<(state.gravityActive?"true":"false")<<",\"gravity_direction\":["<<state.gravityDirection.x<<','
            <<state.gravityDirection.y<<','<<state.gravityDirection.z<<']';
        if(changed){
            const auto& p=result.proposal;
            out<<",\"camera_q32\":["<<p.cameraPosition.x<<','<<p.cameraPosition.y<<','<<p.cameraPosition.z<<']'
               <<",\"camera_orientation\":["<<p.aimWithoutRoll.x<<','<<p.aimWithoutRoll.y<<','<<p.aimWithoutRoll.z<<','<<p.aimWithoutRoll.w<<']'
               <<",\"aim_forward\":["<<p.aimForward.x<<','<<p.aimForward.y<<','<<p.aimForward.z<<']'<<",\"yaw\":"<<p.yaw
               <<",\"proposed_world_move\":["<<p.worldMovement.x<<','<<p.worldMovement.y<<','<<p.worldMovement.z<<']';
        }
        logFile->event(walkingRequested?"input_head_walking_sample":"input_head_idle_sample",out.str());
    }
    // the consumable first: its emptying of the hand is a holster request, which goes out with this input
    consumableHoldInput::apply(source,output,tick);
    holsterInput::apply(source,output,tick);
    if(holsterInput::requested){const auto ms=GetTickCount64();game::applyAreaLoot(output,ms);game::applySwimInput(output,ms);}
    // A watch on "the stick is pushed and the player does not move" (headset run game-20261013-105615: after the second
    // respawn the player could not walk for the rest of the run, while guard and quick slot worked; the samples above
    // had run out long before, so the record could not tell whether the stick reached the game). Read-only: the left
    // hand's own stick, the stick in the game's sent input, this patch's status and the player's state, once the
    // stick has been held for two seconds without the player's root moving 0.3 m, then every ten seconds.
    if(vrLocomotion&&value.status==game::InputPoseStatus::Current){
        thread_local uint64_t since=0,lastLine=0;thread_local WorldPositionQ32 from{};thread_local unsigned lines=0;thread_local bool told=false;
        std::array<float,2> native{};std::memcpy(native.data(),source.data()+game::clientStick,sizeof(native));
        evr::motion::HandFrame hands;{std::lock_guard<std::mutex> lock(controllerUiFixture::gestureMutex);hands=controllerUiFixture::gestureHands;}
        const auto& left=hands.hands[0];const float physical=left.stickActive?std::hypot(left.stick.x,left.stick.y):0.f;
        const auto root=value.head.anchor.root;
        if(std::max(physical,std::hypot(native[0],native[1]))<.5f){since=0;told=false;}
        else if(!since){since=now;from=root;}
        else{
            const double moved=std::hypot(relativeQ32(root.x,from.x),relativeQ32(root.z,from.z));
            if(moved>.3){since=now;from=root;if(told&&lines<60){++lines;logFile->event("movement_resumed","\"tick\":"+std::to_string(tick)+",\"entity\":"+std::to_string(entity));}told=false;}
            else if(now-since>=2000&&(!told||now-lastLine>=10000)&&lines<60){
                ++lines;told=true;lastLine=now;
                std::ostringstream o;o<<std::setprecision(6)<<"\"tick\":"<<tick<<",\"entity\":"<<entity<<",\"held_ms\":"<<now-since<<",\"moved_m\":"<<moved
                    <<",\"left_hand_stick\":["<<left.stick.x<<','<<left.stick.y<<"],\"left_hand_stick_active\":"<<(left.stickActive?"true":"false")<<",\"hands_frame\":"<<hands.frame<<",\"hands_focused\":"<<(hands.focused?"true":"false")
                    <<",\"stick_in_sent_input\":["<<native[0]<<','<<native[1]<<"],\"status\":"<<jsonString(game::idleHeadStatusName(result.status))
                    <<",\"player_flags\":"<<state.flags<<",\"player_state_current\":"<<(guard.playerStateCurrent?"true":"false")<<",\"actor_state_effective\":"<<state.actorState<<",\"actor_state_known\":"<<(state.actorKnown?"true":"false")
                    <<",\"gravity_present\":"<<(state.gravityPresent?"true":"false")<<",\"gravity_active\":"<<(state.gravityActive?"true":"false")
                    <<",\"native_input_mode\":"<<unsigned(source[game::clientInputMode])<<",\"client_mask\":"<<unsigned(source[game::clientMask])<<",\"building_camera\":"<<unsigned(source[game::clientBuildingCamera])
                    <<",\"ui_screen\":"<<unsigned(ui.screen)<<",\"normal_ui_bound\":"<<(guard.normalUiBound?"true":"false")<<",\"native_preserved\":"<<(source==output?"true":"false")<<",\"read_only\":true";
                logFile->event("movement_stalled",o.str());
            }
        }
    }
}
}
