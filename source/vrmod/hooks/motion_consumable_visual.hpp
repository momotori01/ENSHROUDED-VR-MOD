// The consumable held in the right hand (2026-10-23, the user: "its model comes to the right hand, and bringing it to
// the mouth uses it"; then "a model where the item has one"). While the quick wheel's consumable is held
// (consumable_hold.hpp, controllerUiFixture::consumableHeld) a Scene-owned model is drawn in the tracked right hand: the
// model the game draws the item's icon with (consumable_model.hpp - every consumable has one, the game shows none in the
// hand when it uses an item). It is made by its GUID as the hand items' copies are (0xfb4e0) with mask/SDF 0 and drawn
// through a private slot of the eyes (head_visibility_bridge.hpp).
// The icon model is not loaded while the world is played: 0xfb4e0 resolves the GUID through ffcb0, which makes a new
// resource entry (reference 1, state 0) and asks for it to be loaded, and the instance waits on the Scene's list at
// +0xa00 (read of the 1.0 code; the first Mock run puts the loading on record: consumable_model_loaded). Until the
// resource is loaded the copy is kept at a thousandth of its size and out of the eyes. EVR_CONSUMABLE_MODEL=0: off.
namespace motionConsumableVisual {
inline bool requested=false,ready=false;
inline uint32_t handle=0,item=0,failedItem=0;inline uintptr_t resource=0,scene=0;inline DWORD thread=0;
inline uint64_t serial=0,frame=0,updates=0,createdMs=0;inline bool loaded=false;inline unsigned retired=0,remasked=0,sdfRemoved=0;
inline std::array<uint32_t,4> guid{};
inline std::array<uint64_t,2> payloadSerial{};
// Shown this frame: its world transform (Q32 position, orientation) and scale; and, from motion_body_ik.hpp's sent body, where
// it lies in the right hand's socket (online_sync.hpp sends both with sync::heldItemAction).
inline std::array<uint8_t,40> worldTransform{};inline uint64_t shownFrame=0;inline float shownScale=1;
inline Pose inHand=identityPose();inline uint64_t inHandFrame=0;
inline bool wanted(){static const bool on=[]{char v[8]{};const auto n=GetEnvironmentVariableA("EVR_CONSUMABLE_MODEL",v,sizeof v);return !(n==1&&v[0]=='0');}();return on;}
inline bool shown(uint64_t s){return requested&&ready&&handle&&serial==s&&frame==liveFrame.frame;}
inline std::string guidText(const std::array<uint32_t,4>& g){return "["+std::to_string(g[0])+","+std::to_string(g[1])+","+std::to_string(g[2])+","+std::to_string(g[3])+"]";}
// The item's icon model (keen::ItemInfo +0x57c) as the registry holds it; all zero: none.
inline std::array<uint32_t,4> iconModel(uint32_t id){
    static std::map<uint32_t,std::array<uint32_t,4>> known;
    if(const auto it=known.find(id);it!=known.end())return it->second;
    // Why an item has none, once each (Mock game-20261023-185529: Food_T3_CritterBug_Bait had none here, its icon model is in the archive).
    const auto none=[&](const char* why,int status){static std::map<uint32_t,bool> told;if(!told[id]&&told.size()<64){told[id]=true;
        logFile->event("consumable_model_lookup_failed","\"item\":"+std::to_string(id)+",\"reason\":\""+why+"\",\"lookup\":\""+game::itemLookupName(game::ItemLookupStatus(status))+"\"");}return std::array<uint32_t,4>{};};
    const auto registry=attachmentObserver::itemRegistry.load();if(!registry)return none("no_registry",int(game::ItemLookupStatus::Invalid));
    auto reader=inputRanged::readItemBytes;const auto found=game::lookupItem(reader,registry,id);game::ItemMetadata m;
    if(!game::readItemMetadata(reader,found,id,m))return none("item",int(found.status));
    std::array<uint32_t,4> g{},again{};
    if(!reader(found.pointer+evr::motion::itemIconModel,g.data(),16)||!reader(found.pointer+evr::motion::itemIconModel,again.data(),16)||g!=again)return none("read",int(found.status));
    if(g==std::array<uint32_t,4>{})return none("no_icon_model",int(found.status));
    if(known.size()<256)known[id]=g;
    logFile->event("consumable_model_found","\"item\":"+std::to_string(id)+",\"category\":"+std::to_string(m.category)+",\"icon_model\":"+guidText(g));
    return g;
}
// A skinned model (the fish: their icon models are the fish characters) is drawn only with a pose: one of its own, made
// from the model's hierarchy (0xfb8e0, as the private hands' pose, motion_hand_visual.hpp) and set once to the hierarchy's
// rest pose (its parent-relative defaults at +64, 0x111060), bound to the copy (0x110960). Its hierarchy may come a few
// frames after the pose: tried each frame until it is there. The pose goes with the copy (0x10e260 once nothing is bound).
inline uint32_t poseHandle=0;inline bool posed=false;inline unsigned poseLogs=0;
inline bool pose(uintptr_t inScene,uint32_t copy,uintptr_t res,uint32_t& handleOut,bool& done){
    if(done)return true;
    const auto model=read<uintptr_t>(res+layout::resourceDescriptor);if(!model)return false;
    const auto joints=read<uint16_t>(model+layout::renderModelJointCount);if(!joints){done=true;return true;}   // not skinned
    if(!handleOut){const auto hierarchyGuid=read<std::array<uint32_t,4>>(model+layout::renderModelHierarchy);
        handleOut=reinterpret_cast<motionHandVisual::PoseCreate>(baseAddress+0xfb8e0)(reinterpret_cast<void*>(inScene),hierarchyGuid.data());
        if(!handleOut)throw std::runtime_error("consumable pose creation failed");}
    const auto record=handPoseMetadata::pose(inScene,handleOut),owner=read<uintptr_t>(record);if(!owner)return false;
    const auto hierarchy=read<uintptr_t>(owner+layout::poseOwnerHierarchy);if(!hierarchy)return false;
    const auto count=read<uint32_t>(hierarchy+48),defaults=read<uint32_t>(hierarchy+64+4);
    if(!count||count>256||defaults<count||defaults>256)throw std::runtime_error("consumable pose hierarchy shape");
    auto rest=motionHandVisual::array<hand::Joint>(hierarchy+64,defaults);rest.resize(count);
    std::array<uintptr_t,2> span{reinterpret_cast<uintptr_t>(rest.data()),rest.size()};
    reinterpret_cast<motionHandVisual::PoseUpdate>(baseAddress+0x111060)(reinterpret_cast<void*>(inScene),handleOut,span.data());
    std::array<uintptr_t,2> ids{reinterpret_cast<uintptr_t>(&copy),1};reinterpret_cast<motionHandVisual::Bind>(baseAddress+0x110960)(reinterpret_cast<void*>(inScene),ids.data(),handleOut);
    if(read<uint32_t>(motionVisual::object(copy)+layout::modelPose)!=handleOut)throw std::runtime_error("consumable pose binding failed");
    done=true;if(poseLogs++<20)logFile->event("consumable_model_posed","\"handle\":"+std::to_string(copy)+",\"pose\":"+std::to_string(handleOut)+",\"joints\":"+std::to_string(count)+",\"model_joints\":"+std::to_string(joints));
    return true;
}
inline void unpose(uintptr_t inScene,uint32_t& handleIn,bool& done) noexcept {
    try{if(handleIn&&!read<uint64_t>(handPoseMetadata::pose(inScene,handleIn)+0x58))
        reinterpret_cast<motionHandVisual::PoseRemove>(baseAddress+0x10e260)(reinterpret_cast<void*>(inScene),handleIn);}catch(...){}
    handleIn=0;done=false;
}
inline void retire(const char* event,const std::string& detail){
    const auto clone=motionVisual::object(handle);
    if(read<uintptr_t>(clone+layout::modelResource)!=resource)throw std::runtime_error("consumable retirement invariant differs");
    const auto id=handle,refs=read<uint32_t>(resource+16);std::array<uintptr_t,2> ids{reinterpret_cast<uintptr_t>(&id),1};
    reinterpret_cast<motionVisual::Remove>(baseAddress+0x10e000)(reinterpret_cast<void*>(scene),ids.data());handle=0;ready=false;
    unpose(scene,poseHandle,posed);
    const bool invalid=read<uint8_t>(read<uintptr_t>(scene+0x1c8)+(id&0xffffff))!=uint8_t(id>>24);
    const bool released=refs==1||(refs>1&&read<uint32_t>(resource+16)==refs-1);
    logFile->event(event,"\"handle\":"+std::to_string(id)+",\"item\":"+std::to_string(item)+",\"updates\":"+std::to_string(updates)+",\"loaded\":"+(loaded?"true":"false")+
        ",\"references_before\":"+std::to_string(refs)+",\"generation_invalidated\":"+(invalid?"true":"false")+",\"reference_released\":"+(released?"true":"false")+detail);
    item=0;resource=0;loaded=false;++retired;motionHandVisual::rightHold=-1;
    if(!invalid||!released)throw std::runtime_error("consumable retirement failed");
}
// How the item lies in the hand (consumable_hand_catalog.hpp; not listed: on the palm, 13 cm), EVR_CONSUMABLE_SIZE_CM overriding the size.
// grip: where the hand holds it (consumable_hand_catalog.hpp gripCentre; in the fist its depth on the palm's side and width, m).
struct Hold {evr::motion::ConsumableHold hold=evr::motion::ConsumableHold::Palm;float size=.13f;bool along=false,skinned=false,heaped=false,turnedKnown=false,gripKnown=false,plate=false;Quat turn{0,0,0,1};Vec3 turnedLo{},turnedHi{},gripCentre{};float gripDepth=0,gripWidth=0;};
inline Hold holdOf(uint32_t id){
    static const float forced=[]{char v[16]{};const auto n=GetEnvironmentVariableA("EVR_CONSUMABLE_SIZE_CM",v,sizeof v);const float cm=n&&n<sizeof v?float(std::atof(v)):0.f;
        return std::isfinite(cm)&&cm>=3&&cm<=60?cm/100:0.f;}();
    Hold h;if(const auto* c=evr::motion::consumableHand(id)){h.hold=evr::motion::ConsumableHold(c->hold);h.size=c->sizeMm/1000.f;h.along=c->alongLongest;h.skinned=c->skinned;h.heaped=c->heaped;h.turn=normalized(Quat{c->turn[0],c->turn[1],c->turn[2],c->turn[3]});
        const auto& b=c->turnedBounds;h.turnedLo={b[0],b[1],b[2]};h.turnedHi={b[3],b[4],b[5]};h.turnedKnown=h.turnedHi.x>h.turnedLo.x&&h.turnedHi.y>h.turnedLo.y&&h.turnedHi.z>h.turnedLo.z;
        h.gripKnown=c->gripKnown;h.gripCentre={c->gripCentre[0],c->gripCentre[1],c->gripCentre[2]};h.gripDepth=c->gripDepthMm/1000.f;h.gripWidth=c->gripWidthMm/1000.f;h.plate=c->plate;}
    if(forced>0){if(h.size>0){h.gripDepth*=forced/h.size;h.gripWidth*=forced/h.size;}h.size=forced;}
    return h;
}
// The item in the hand: the consumable held from the wheel, else a throwable in the hand (no model of its own in the game;
// 2026-10-23, user: "the water bomb was not in the hand") - not for the moment it is thrown.
// Diagnostic: EVR_CONSUMABLE_SHOW=<from>:<every>:<item>,<item>,... shows each item in turn, every frames from frame from
// (Mock pictures of how each lies in the hand; the input side holds nothing; up to 16 k characters: every held item, 234
// models, in one list). Never in play.
inline int shownIndex=-1;   // which of the list is shown (-1: none)
inline uint32_t shownForTest(){
    shownIndex=-1;
    static const auto list=[]{struct L{uint64_t from=0,every=0;std::vector<uint32_t> items;}l;std::vector<char> v(16384);const auto n=GetEnvironmentVariableA("EVR_CONSUMABLE_SHOW",v.data(),DWORD(v.size()));
        if(!n||n>=v.size())return l;std::string s(v.data());
        for(auto& c:s)if(c==':'||c==',')c=' ';std::istringstream in(s);in>>l.from>>l.every;uint32_t i=0;while(in>>i)l.items.push_back(i);
        if(!l.every)l.items.clear();if(!l.items.empty())logFile->event("consumable_show_test","\"from\":"+std::to_string(l.from)+",\"every\":"+std::to_string(l.every)+",\"items\":"+std::to_string(l.items.size()));return l;}();
    if(list.items.empty()||liveFrame.frame<list.from)return 0;const auto k=(liveFrame.frame-list.from)/list.every;if(k>=list.items.size())return 0;
    shownIndex=int(k);return list.items[size_t(k)];
}
inline uint32_t heldNow(){
    if(const auto shown=shownForTest())return shown;
    if(const auto c=controllerUiFixture::consumableHeld.load())return c;
    if(const auto f=controllerUiFixture::foodInHand.load())return f;   // a food the game put in the hand (controller_ui_fixture.hpp)
    const auto t=controllerUiFixture::throwableHeld.load();const auto at=controllerUiFixture::throwAtMs.load();const auto now=GetTickCount64();
    return t&&!(at&&now>=at&&now-at<400)?t:0;
}
// Where a model lies in the hand, in the game's space (the hands' rig works there; its Z is the mirror of the headset's):
// in the fist as a weapon is held, mirrored (its pitch turns the other way); on the palm from the rig's rest pose.
inline evr::motion::ConsumableMount place(const Hold& how,Pose gameGrip,Vec3 lo,Vec3 hi,bool& onPalm){
    if(how.hold==evr::motion::ConsumableHold::Tail){onPalm=false;motionHandVisual::rightCurl=.85f;   // the fist round the thin tail
        return evr::motion::mountByTail(gameGrip,lo,hi,how.size,-motionVisual::weaponPitchDegrees);}
    // turn (consumable_hand_catalog.hpp; 2026-10-24, user: "the mushroom is fitted the wrong way round", then "the grilled
    // brown one is right - the other mushrooms that way too"): each single mushroom on its side, the cap toward the fingers.
    auto r=how.along?evr::motion::longestAlong(lo,hi,how.hold):Quat{0,0,0,1};r=normalized(multiply(r,how.turn));
    // The bounds as it lies turned: from its vertices where the catalog has them (mushroom_axes.py) - the corners of its box
    // turned make a larger box when the turn is not a quarter one, the mushroom off the palm and smaller than its size.
    Vec3 tlo,thi;evr::motion::turnedBounds(r,lo,hi,tlo,thi);
    if(how.turnedKnown&&!how.along){tlo=how.turnedLo;thi=how.turnedHi;}
    Pose palmLocal;const bool palmKnown=motionHandVisual::palmInSocket(1,palmLocal);onPalm=how.hold==evr::motion::ConsumableHold::Palm&&palmKnown;
    // Held by the middle of its body, not of its box (2026-10-26, user: "the water balloon and the bombs: a little more toward
    // the thumb", then "every item should sit right in the hand"): a bomb's box takes in its fuse, a bottle's its neck, so the
    // body sat toward the little finger; a skewer by its bare stick (consumable_hand_catalog.hpp gripCentre, grip_centres.py).
    // EVR_CONSUMABLE_GRIP=0: by the middle of the box, as before.
    static const bool gripOn=[]{char v[8]{};const auto n=GetEnvironmentVariableA("EVR_CONSUMABLE_GRIP",v,sizeof v);return !(n&&n<sizeof v&&v[0]=='0');}();
    const bool grip=gripOn&&how.gripKnown;const Vec3 centre=grip?rotate(r,how.gripCentre):Vec3{};
    auto mount=onPalm?evr::motion::mountOnPalm(compose(gameGrip,palmLocal),tlo,thi,how.size,grip?&centre:nullptr):
        evr::motion::mountConsumable(gameGrip,tlo,thi,how.size,-motionVisual::weaponPitchDegrees,grip?&centre:nullptr);
    // A heap (nuts, popcorn, berries: consumable_hand_catalog.hpp heaped) lies toward the fingers, in the palm's hollow (2026-10-24,
    // user: "too far toward the wrist - 5 cm more toward the fingers"; then "it sinks well into the palm - 1 cm out of the
    // palm"); then "too far toward the thumb - 2 cm toward the little finger, its middle between the middle and the ring
    // finger" (the mushrooms lie there too). The palm's +Z runs from the fingers to the wrist, its +Y out of the palm, and
    // its +X toward the little finger (consumable_model.hpp palmFrame: x = -(index - little) for the right hand).
    // EVR_CONSUMABLE_HEAP_FORWARD_CM / _UP_CM / _SIDE_CM=<0..10>: other distances (5, 1, 2).
    static const auto heapCm=[](const char* name,float fallback){char v[16]{};const auto n=GetEnvironmentVariableA(name,v,sizeof v);const float cm=n&&n<sizeof v?float(std::atof(v)):fallback;
        return std::isfinite(cm)&&cm>=0&&cm<=10?cm/100:fallback/100;};
    static const float heapForward=heapCm("EVR_CONSUMABLE_HEAP_FORWARD_CM",5.f),heapUp=heapCm("EVR_CONSUMABLE_HEAP_UP_CM",1.f),heapSide=heapCm("EVR_CONSUMABLE_HEAP_SIDE_CM",2.f);
    // Every other food on the palm but a plate lies there too (2026-10-26, user with a photo of things on an open hand: "the place
    // for the foods not on a plate is about here - about the base of the fingers"); the heap's 1 cm out of the palm stays the heaps'.
    if(onPalm&&!how.plate)mount.pose.position=mount.pose.position+rotate(compose(gameGrip,palmLocal).orientation,Vec3{heapSide,how.heaped?heapUp:0.f,-heapForward});
    // A dish on a flat plate (consumable_hand_catalog.hpp plate; 2026-10-26, user after the Mock: "the dishes on plates further
    // out, toward the fingers - like the thumb hooked over the plate's rim", with a photo: the plate out past the fingertips,
    // the thumb on its near rim): its near rim (the wrist's side, the palm's +Z) EVR_CONSUMABLE_PLATE_RIM_CM (1) toward the
    // fingers from the palm's middle - the thumb's tip lies 2 cm that way in the rig's rest pose (hand_joints.py).
    static const float plateRim=heapCm("EVR_CONSUMABLE_PLATE_RIM_CM",1.f);
    if(onPalm&&how.plate){const float nearRim=(thi.z-(grip?centre.z:(tlo.z+thi.z)*.5f))*mount.scale;
        mount.pose.position=mount.pose.position+rotate(compose(gameGrip,palmLocal).orientation,Vec3{0,0,-(nearRim+plateRim)});}
    if(!onPalm){
        // A thick thing in the fist (Mock game-20261023-180416: the 26 cm potion's middle in the grip sank half into the hand):
        // it lies against the palm - its middle out from the grip (a weapon's handle, about 2 cm thick) by how far its surface on
        // the palm's side lies from it - and the fingers close only as far as its width there lets them. With a grip known its
        // own depth and width at the hand (a mug's handle or the crawler bomb's horns are not its body), else half its box.
        float depth,width;
        if(grip&&how.gripWidth>0){depth=how.gripDepth;width=how.gripWidth;}
        else{width=std::max(thi.x-tlo.x,thi.z-tlo.z)*mount.scale;depth=width*.5f;}
        if(palmKnown)mount.pose.position=mount.pose.position+rotate(gameGrip.orientation,rotate(palmLocal.orientation,Vec3{0,1,0}))*std::max(0.f,depth-.02f);
        motionHandVisual::rightCurl=std::clamp(.95f-(width-.03f)*4.f,.45f,.95f);
        // Toward the thumb (the held weapon's +Y): the grip point is the fist's little-finger side. In the rig's rest pose, in the
        // held item's frame, the knuckles of the index, middle, ring and little fingers lie at +6.5, +4.2, +1.6 and -0.6 cm along
        // the fist (their middle +2.9 cm), the palm's middle at +0.4 cm (motion_hand_palm palm_in_socket;
        // docs/analysis/consumable_models_20261023/hand_joints.py, 2026-10-26) - the user saw the water balloon and the bombs toward the little finger. Every item in
        // the fist 1.5 cm toward the thumb: between the palm's middle and the fingers'. EVR_CONSUMABLE_FIST_THUMB_CM=<0..10> (1.5).
        // Diagnostic: with EVR_CONSUMABLE_SHOW, EVR_CONSUMABLE_SHOW_ALT_THUMB_CM for every second item of the list (the same item
        // twice: a picture of each).
        static const float fistThumb=heapCm("EVR_CONSUMABLE_FIST_THUMB_CM",1.5f),altThumb=heapCm("EVR_CONSUMABLE_SHOW_ALT_THUMB_CM",1.5f);
        const float thumb=shownIndex>=0&&(shownIndex&1)?altThumb:fistThumb;
        if(thumb>0)mount.pose.position=mount.pose.position+rotate(mount.pose.orientation,Vec3{0,1,0})*thumb;
    }
    mount.pose.orientation=normalized(multiply(mount.pose.orientation,r));return mount;
}
inline void prepare(const game::StableAnchor& anchor,uint64_t currentSerial){
    if(!requested)return;ready=false;
    const uint32_t held=heldNow();
    if(handle&&(scene!=motionVisual::scene||thread!=GetCurrentThreadId()))throw std::runtime_error("consumable model Scene/thread changed");
    if(handle&&held!=item)retire("consumable_model_retired",",\"held\":"+std::to_string(held));
    if(!held||held==failedItem){motionHandVisual::rightHold=-1;return;}
    if(!motionVisual::contextReady||motionVisual::contextSerial!=currentSerial||motionVisual::contextFrame!=liveFrame.frame||
       !motionHandVisual::ready||motionHandVisual::serial!=currentSerial||!shownHands.focused||!shownHands.hands[1].tracked())return;
    const auto how=holdOf(held);
    // Until it is loaded: where the hand is, a thousandth of its size.
    Vec3 lo{-.05f,0,-.05f},hi{.05f,.1f,.05f};float shrink=.001f;
    if(handle&&read<uint8_t>(resource+0x14)==1){
        const auto model=read<uintptr_t>(resource+layout::resourceDescriptor);
        if(model){const auto a=read<Vec3>(model+16),b=read<Vec3>(model+32);if(finite(a)&&finite(b)&&b.x>a.x&&b.y>a.y&&b.z>a.z&&b.y-a.y<4.f){lo=a;hi=b;shrink=1;}}
    }
    bool onPalm=false;
    const auto mount=place(how,game::trackingPose(compose(liveTrackingToAnchor,shownHands.hands[1].grip)),lo,hi,onPalm);
    motionHandVisual::rightHold=onPalm?1:0;
    const auto& tracking=mount.pose;
    const auto facing=normalized(anchor.orientation);
    const auto origin=cameraPosition(anchor.root,rotate(facing,anchor.cameraOffsetGameUnits),1);
    const auto position=cameraPosition(origin,rotate(facing,tracking.position),anchor.worldUnitsPerMeter);
    const auto rotation=normalized(multiply(facing,tracking.orientation));
    const float s=mount.scale*shrink;const Vec3 scale{s,s,s};
    std::array<uint8_t,56> transform{};std::memcpy(transform.data(),&position,24);std::memcpy(transform.data()+24,&rotation,16);std::memcpy(transform.data()+40,&scale,12);
    if(!handle){
        const auto g=iconModel(held);
        if(g==std::array<uint32_t,4>{}){failedItem=held;logFile->event("consumable_model_none","\"item\":"+std::to_string(held));return;}
        motionVisual::validate();scene=motionVisual::scene;thread=GetCurrentThreadId();
        handle=reinterpret_cast<motionVisual::Create>(baseAddress+0xfb4e0)(reinterpret_cast<void*>(scene),g.data(),transform.data());
        if(!handle){failedItem=held;logFile->event("consumable_model_failed","\"item\":"+std::to_string(held)+",\"icon_model\":"+guidText(g));return;}
        auto clone=motionVisual::object(handle);resource=read<uintptr_t>(clone+layout::modelResource);
        if(!resource||read<std::array<uint32_t,4>>(resource+0x18)!=g)throw std::runtime_error("consumable model resource differs");
        reinterpret_cast<motionVisual::Mask>(baseAddress+0xf70c0)(reinterpret_cast<void*>(scene),reinterpret_cast<void*>(clone),0);
        item=held;guid=g;createdMs=GetTickCount64();updates=0;loaded=false;
        logFile->event("consumable_model_created","\"handle\":"+std::to_string(handle)+",\"item\":"+std::to_string(item)+",\"icon_model\":"+guidText(g)+
            ",\"hold\":\""+(how.hold==evr::motion::ConsumableHold::Palm?"palm":how.hold==evr::motion::ConsumableHold::Tail?"tail":"fist")+"\",\"size_m\":"+std::to_string(how.size)+",\"along_longest\":"+(how.along?"true":"false")+
            ",\"resource_state\":"+std::to_string(read<uint8_t>(resource+0x14))+",\"references\":"+std::to_string(read<uint32_t>(resource+16))+
            ",\"mask\":"+std::to_string(read<uint32_t>(motionVisual::object(handle)+layout::modelMask))+",\"sdf\":"+std::to_string(read<uint32_t>(motionVisual::object(handle)+layout::modelSdf))+
            ",\"pose\":"+std::to_string(read<uint32_t>(motionVisual::object(handle)+layout::modelPose)));
    }else{
        std::array<uintptr_t,2> ids{reinterpret_cast<uintptr_t>(&handle),1},poses{reinterpret_cast<uintptr_t>(transform.data()),1};
        motionVisual::change(reinterpret_cast<void*>(scene),ids.data(),poses.data());
    }
    auto clone=motionVisual::object(handle);
    if(read<uintptr_t>(clone+layout::modelResource)!=resource)throw std::runtime_error("consumable model resource changed");
    // The game finishes a model when its resource has come: its mask and SDF are put back to the copy's own each time.
    if(read<uint32_t>(clone+layout::modelMask)){
        reinterpret_cast<motionVisual::Mask>(baseAddress+0xf70c0)(reinterpret_cast<void*>(scene),reinterpret_cast<void*>(clone),0);clone=motionVisual::object(handle);
        if(remasked++<4)logFile->event("consumable_model_remasked","\"handle\":"+std::to_string(handle)+",\"frame\":"+std::to_string(liveFrame.frame)+",\"resource_state\":"+std::to_string(read<uint8_t>(resource+0x14)));
    }
    if(const auto sdfId=read<uint32_t>(clone+layout::modelSdf)){
        const auto sdf=read<uintptr_t>(scene+layout::sceneSdf);const auto count=sdf?read<uint64_t>(sdf+0x78):0;if(!count)throw std::runtime_error("consumable SDF count");
        using RemoveSdf=void(__fastcall*)(void*,uint32_t);reinterpret_cast<RemoveSdf>(baseAddress+0x15a640)(reinterpret_cast<void*>(sdf),sdfId);
        *reinterpret_cast<uint32_t*>(motionVisual::object(handle)+layout::modelSdf)=0;
        if(read<uint64_t>(sdf+0x78)!=count-1)throw std::runtime_error("consumable SDF removal failed");
        if(sdfRemoved++<4)logFile->event("consumable_model_sdf_removed","\"handle\":"+std::to_string(handle)+",\"frame\":"+std::to_string(liveFrame.frame));
    }
    const bool resourceIn=read<uint8_t>(resource+0x14)==1&&shrink==1;
    const bool withPose=resourceIn&&pose(scene,handle,resource,poseHandle,posed);clone=motionVisual::object(handle);
    const bool nowLoaded=resourceIn&&withPose;
    if(nowLoaded&&!loaded){
        std::ostringstream out;out<<std::setprecision(6)<<"\"handle\":"<<handle<<",\"item\":"<<item<<",\"frame\":"<<liveFrame.frame<<",\"after_ms\":"<<(GetTickCount64()-createdMs)
            <<",\"bounds\":["<<lo.x<<','<<lo.y<<','<<lo.z<<','<<hi.x<<','<<hi.y<<','<<hi.z<<"],\"scale\":"<<mount.scale<<",\"hold\":\""<<(onPalm?"palm":"fist")<<"\",\"pose\":"<<read<uint32_t>(clone+layout::modelPose);
        logFile->event("consumable_model_loaded",out.str());
    }
    loaded=nowLoaded;
    if(!loaded&&GetTickCount64()-createdMs>5000){
        static uint32_t told=0;if(told!=handle){told=handle;logFile->event("consumable_model_not_loaded","\"handle\":"+std::to_string(handle)+",\"item\":"+std::to_string(item)+
            ",\"resource_state\":"+std::to_string(read<uint8_t>(resource+0x14))+",\"references\":"+std::to_string(read<uint32_t>(resource+16))+",\"pose\":"+std::to_string(poseHandle));}
    }
    serial=currentSerial;frame=liveFrame.frame;ready=loaded&&!read<uint32_t>(clone+layout::modelMask)&&!read<uint32_t>(clone+layout::modelSdf);++updates;
    if(ready){std::memcpy(worldTransform.data(),transform.data(),40);shownFrame=liveFrame.frame;shownScale=mount.scale;}
    if(updates<=3||updates%90==0){
        std::ostringstream out;out<<std::setprecision(9)<<"\"serial\":"<<serial<<",\"frame\":"<<frame<<",\"handle\":"<<handle<<",\"item\":"<<item<<",\"loaded\":"<<(loaded?"true":"false")
            <<",\"scale\":"<<s<<",\"hold\":\""<<(onPalm?"palm":"fist")<<"\",\"q32\":["<<position.x<<','<<position.y<<','<<position.z<<"]";
        logFile->event("consumable_model_pose",out.str());
    }
}
inline void dispose(uintptr_t s){
    if(!handle||s!=scene)return;
    if(thread!=GetCurrentThreadId())throw std::runtime_error("consumable model disposal thread differs");
    retire("consumable_model_disposed",",\"scene_disposal\":true");
}
}
// Diagnostic (2026-10-23, user: "check each item's model in the game and give it a real size and hold"): the icon models of
// a list (EVR_ITEM_GALLERY=<file of "item g0 g1 g2 g3" lines>) shown six at a time 1.5 m before the head, each twice -
// standing (+Y up) and tipped 70 degrees toward the eye - its longest side 25 cm, a new batch every EVR_ITEM_GALLERY_FRAMES
// (40) frames from frame EVR_ITEM_GALLERY_FROM (300). Seen as the game draws them (the game's mask), for the key script's
// pictures; item_gallery_batch puts each batch's items on record. Never in play.
namespace itemGallery {
struct Entry {uint32_t item=0;std::array<uint32_t,4> guid{};};
struct Shown {uint32_t handle=0;uintptr_t resource=0;unsigned entry=0;bool tipped=false;};
inline std::vector<Entry> entries;inline std::vector<Shown> shown;inline unsigned batch=0;inline uint64_t batchFrame=0,from=300,every=40;inline bool requested=false,done=false,told=false;
inline void configure(){
    char path[512]{};const auto n=GetEnvironmentVariableA("EVR_ITEM_GALLERY",path,sizeof path);if(!n||n>=sizeof path)return;
    std::ifstream in(path);Entry e;while(in>>e.item>>e.guid[0]>>e.guid[1]>>e.guid[2]>>e.guid[3])entries.push_back(e);
    char v[16]{};if(const auto m=GetEnvironmentVariableA("EVR_ITEM_GALLERY_FROM",v,sizeof v);m&&m<sizeof v)from=std::strtoull(v,nullptr,10);
    if(const auto m=GetEnvironmentVariableA("EVR_ITEM_GALLERY_FRAMES",v,sizeof v);m&&m<sizeof v)every=std::max<uint64_t>(10,std::strtoull(v,nullptr,10));
    requested=!entries.empty();logFile->event("item_gallery_enabled","\"entries\":"+std::to_string(entries.size())+",\"from\":"+std::to_string(from)+",\"every\":"+std::to_string(every));
}
inline void clear(){
    for(auto& s:shown)if(s.handle&&motionVisual::currentObject(s.handle)){std::array<uintptr_t,2> ids{reinterpret_cast<uintptr_t>(&s.handle),1};
        reinterpret_cast<motionVisual::Remove>(baseAddress+0x10e000)(reinterpret_cast<void*>(motionVisual::scene),ids.data());}
    shown.clear();
}
inline void tick(const game::StableAnchor& anchor,uint64_t currentSerial){
    if(!requested||done||!motionVisual::contextReady||motionVisual::contextSerial!=currentSerial||motionVisual::contextFrame!=liveFrame.frame||liveFrame.frame<from)return;
    evr::motion::GestureHead head;{std::lock_guard<std::mutex> lock(controllerUiFixture::gestureMutex);head=controllerUiFixture::gestureHead;}
    if((head.flags&15)!=15)return;
    const auto facing=normalized(anchor.orientation);const auto origin=cameraPosition(anchor.root,rotate(facing,anchor.cameraOffsetGameUnits),1);
    // the head's yaw only: rows stand level before it
    const auto f=rotate(head.pose.orientation,Vec3{0,0,-1});const float yaw=std::atan2(-f.x,-f.z);const auto level=euler(yaw,0,0);
    const auto place=[&](const Shown& s,float scale){
        const unsigned k=s.entry%6,col=k%3,row=k/3;
        const Vec3 at{-.55f+.55f*float(col)+(s.tipped?.14f:-.14f),-.2f-.45f*float(row),-1.5f};
        Pose p;p.position=head.pose.position+rotate(level,at);p.orientation=s.tipped?normalized(multiply(level,euler(0,70.f*3.14159265f/180.f,0))):level;
        const auto t=game::trackingPose(compose(liveTrackingToAnchor,p));
        const auto position=cameraPosition(origin,rotate(facing,t.position),anchor.worldUnitsPerMeter);const auto rotation=normalized(multiply(facing,t.orientation));
        const Vec3 sc{scale,scale,scale};std::array<uint8_t,56> tr{};std::memcpy(tr.data(),&position,24);std::memcpy(tr.data()+24,&rotation,16);std::memcpy(tr.data()+40,&sc,12);return tr;};
    if(!batchFrame||liveFrame.frame>=batchFrame+every){
        clear();const unsigned first=batch*6;
        if(first>=entries.size()){done=true;logFile->event("item_gallery_done","\"frame\":"+std::to_string(liveFrame.frame)+",\"batches\":"+std::to_string(batch));return;}
        motionVisual::validate();std::string list;
        for(unsigned i=first;i<std::min<size_t>(entries.size(),first+6);++i)for(int tip=0;tip<2;++tip){Shown s;s.entry=i;s.tipped=tip!=0;
            const auto tr=place(s,.001f);s.handle=reinterpret_cast<motionVisual::Create>(baseAddress+0xfb4e0)(reinterpret_cast<void*>(motionVisual::scene),entries[i].guid.data(),tr.data());
            if(!s.handle)continue;s.resource=read<uintptr_t>(motionVisual::object(s.handle)+layout::modelResource);shown.push_back(s);
            if(!tip)list+=std::string(list.empty()?"":",")+"["+std::to_string(i-first)+","+std::to_string(entries[i].item)+"]";}
        logFile->event("item_gallery_batch","\"batch\":"+std::to_string(batch)+",\"frame\":"+std::to_string(liveFrame.frame)+",\"cells\":["+list+"],\"layout\":\"3 columns x 2 rows from the upper left; each cell standing (left) and tipped toward the eye (right)\"");
        ++batch;batchFrame=liveFrame.frame;
    }
    unsigned loaded=0;
    for(auto& s:shown){if(!motionVisual::currentObject(s.handle))continue;float scale=.001f;
        if(read<uint8_t>(s.resource+0x14)==1)if(const auto model=read<uintptr_t>(s.resource+layout::resourceDescriptor)){const auto a=read<Vec3>(model+16),b=read<Vec3>(model+32);
            const float longest=std::max({b.x-a.x,b.y-a.y,b.z-a.z});if(std::isfinite(longest)&&longest>1e-3f){scale=.24f/longest;++loaded;}}
        const auto tr=place(s,scale);std::array<uintptr_t,2> ids{reinterpret_cast<uintptr_t>(&s.handle),1},poses{reinterpret_cast<uintptr_t>(tr.data()),1};
        motionVisual::change(reinterpret_cast<void*>(motionVisual::scene),ids.data(),poses.data());}
    if(liveFrame.frame==batchFrame+every/2)logFile->event("item_gallery_loaded","\"batch\":"+std::to_string(batch-1)+",\"frame\":"+std::to_string(liveFrame.frame)+",\"loaded\":"+std::to_string(loaded)+",\"shown\":"+std::to_string(shown.size()));
}
inline void dispose(uintptr_t s){if(s==motionVisual::scene)try{clear();}catch(...){shown.clear();}}
}
