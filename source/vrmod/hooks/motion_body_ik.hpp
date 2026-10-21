// Player IK on the player's own model (2026-10-11, user decision: drive the model itself as VHVR does, no copies).
// The local body part models are re-bound from the game's animation pose to a RenderPose owned
// by the mod: the native bind (dca70) first detaches a model from the pose it is on. Every frame
// that pose receives this frame's animated joints with the head, chest, arms, hips and knees solved
// from the headset and the controllers (include/evr/body_ik.hpp). The game's own pose is never
// written, and re-binding the parts to it undoes everything; that also happens whenever the view is
// not first person or the hands are not shown, so the game's animation is seen there.
// Explicit policy only (world-body-ik.txt).
namespace motionBodyIk {
inline bool requested=false,ready=false;
inline uint32_t poseHandle=0,nativePose=0;inline uintptr_t poseResource=0,scene=0;
inline std::vector<uint32_t> bound;inline uint64_t serial=0,updates=0,releases=0;
inline bool shown(uint64_t renderSerial){return requested&&ready&&serial==renderSerial;}
// The copy of the bare hands (motion_hand_visual.hpp) stood on a pose of its own, at the controllers, while the body's
// gloves stood on this pose. Their wrists differ a little, and the fingers showed beside the plates of the gauntlets
// (headset run game145015: "the hands look drawn twice and flicker, like bare hands inside the gauntlets"; Mock
// game154945 at full size). While the body is driven the copy is bound to this pose at the body's transform: one
// skeleton for gloves and hands, as in the game. EVR_BODY_IK_PRIVATE_HANDS=1 in the game's environment keeps the
// copy on its own pose (for a comparison; no launch flag).
inline bool handsOnBody=true;inline unsigned handLends=0;
// The caught fish held as the game's animation holds it (2026-10-19 19:55, user: "the fish is held on the line of the
// wrist - hold its tail fin in the palm; the fish models cannot all be caught: check them all from the game data and put
// the part really held on the left palm"; then "in multiplayer I want to raise it: share it with the IK"). The game
// places the caught fish itself (it is attached to nothing, headset runs 20261019-191819 / -193001) where the victory
// animation's left hand holds it; that hold - the fish relative to the animated left hand socket (joint 64) - is put on
// the solved left socket, for every fish and every character whose body the mod solves (the own one here,
// other players' in remote_body.hpp from their datagrams' hands). A fish farther than 1 m from the animated socket is
// not in that hand yet (it flies out of the water first) and stays the game's. EVR_FISH_IN_HAND=0: off.
inline bool fishHoldWanted(){static const bool on=[]{char v[8]{};const auto n=GetEnvironmentVariableA("EVR_FISH_IN_HAND",v,sizeof v);return !(n==1&&v[0]=='0');}();return on;}
inline bool holdFish(uintptr_t s,uint32_t fish,const std::vector<hand::Joint>& animated,const std::vector<hand::Joint>& solved,const std::vector<uint16_t>& parents,
                     const WorldPositionQ32& modelPosition,Quat modelQ,Vec3 modelScale,uint32_t character){
    if(!fishHoldWanted()||!fish||!s)return false;
    const auto now=GetTickCount64();attachmentObserver::watchFish(fish,now);
    attachmentObserver::FishModel m;{std::lock_guard<std::mutex> lock(attachmentObserver::followMutex);if(const auto* f=attachmentObserver::fishModelOf(fish,false))m=*f;}
    if(m.entity!=fish||!m.handle||m.scene!=s||!m.gamePose||!m.at||now<m.at||now-m.at>500)return false;
    const auto object=motionVisual::currentObject(m.handle);if(!object)return false;
    WorldPositionQ32 fp;Quat fq;std::memcpy(&fp,m.pose.data(),24);std::memcpy(&fq,m.pose.data()+24,16);
    const float n=fq.x*fq.x+fq.y*fq.y+fq.z*fq.z+fq.w*fq.w;if(!std::isfinite(n)||std::abs(n-1)>.05f)return false;
    const Vec3 delta{float(relativeQ32(fp.x,modelPosition.x)),float(relativeQ32(fp.y,modelPosition.y)),float(relativeQ32(fp.z,modelPosition.z))};
    if(!finite(delta)||length(delta)>8.f)return false;
    const auto local=rotate(inverse(modelQ),delta);
    const Pose inModel{normalized(multiply(inverse(modelQ),normalized(fq))),{local.x/modelScale.x,local.y/modelScale.y,local.z/modelScale.z}};
    constexpr unsigned socket=64;   // the left hand socket (include/evr/body_ik.hpp)
    const auto ga=bodyik::globals(animated,parents),gs=bodyik::globals(solved,parents);
    if(ga.size()<=socket||gs.size()<=socket)return false;
    const auto inHand=compose(inverse(ga[socket]),inModel);
    if(!finite(inHand.position)||length(inHand.position)>1.f)return false;
    // By the tail (2026-10-20, user after the headset run: "the fish's body, not its tail fin, was on the palm"): the victory
    // animation holds the fish by its middle. The tail end is put on the left socket and the fish hangs from it, head down -
    // as a fish is held up by its tail. The fish model's extent (resource descriptor +0x30, minimum +16, maximum +32, as
    // the weapons' - motion_visual_bridge.hpp modelExtentY): its longest axis is its length, the tail at that axis's minimum
    // (guess: the game's models face +Z as the characters do - include/evr/body_ik.hpp levelHeading - so a fish's head is at
    // the maximum), at the tail fin's base (below). EVR_FISH_TAIL_GRIP=0: the animation's hold as before.
    static const bool tailGrip=[]{char v[8]{};const auto n=GetEnvironmentVariableA("EVR_FISH_TAIL_GRIP",v,sizeof v);return !(n==1&&v[0]=='0');}();
    // Only what swam (2026-10-20 14:20, user: "a boot was caught - where it is held is odd"): a boot swam 0 m during the
    // fight (a fish 30..41 m), its long axis was its height, and it was held 20 % from one end as a fish by the tail. What
    // did not swim half a metre keeps the game's own hold, on the tracked hand (below).
    const bool swamAsFish=m.swimMetres>=.5f;
    if(tailGrip&&swamAsFish)try{
        const auto resource=read<uintptr_t>(object+layout::modelResource);const auto descriptor=read<uintptr_t>(resource+layout::resourceDescriptor);
        const auto lo=read<Vec3>(descriptor+16),hi=read<Vec3>(descriptor+32);Vec3 scale{1,1,1};std::memcpy(&scale,m.pose.data()+40,12);
        const Vec3 extent=hi-lo;
        if(finite(lo)&&finite(hi)&&finite(scale)&&extent.x>=0&&extent.y>=0&&extent.z>=0&&std::max({extent.x,extent.y,extent.z})>.05f&&std::max({extent.x,extent.y,extent.z})<4.f&&
           scale.x>.1f&&scale.x<10.f&&scale.y>.1f&&scale.y<10.f&&scale.z>.1f&&scale.z<10.f){
            const int axis=extent.z>=extent.x&&extent.z>=extent.y?2:extent.x>=extent.y?0:1;
            const float e[3]{extent.x,extent.y,extent.z},l[3]{lo.x,lo.y,lo.z},h[3]{hi.x,hi.y,hi.z};float tail[3]{(l[0]+h[0])*.5f,(l[1]+h[1])*.5f,(l[2]+h[2])*.5f};
            // Which end is the head: where the fish swam during the fight (attachment_observer.hpp swim: its moves along its own
            // axes, head first), when it swam 0.5 m or more and mostly along this axis; else the guess above (the maximum).
            // (Headset run 20261020-100726: swum 41 m toward the maximum, and the user: "it does hold the tail fin".)
            const float swum=m.swimMetres,along=m.swim[size_t(axis)];const bool bySwim=swum>=.5f&&std::abs(along)>=.3f*swum;
            const bool headAtMaximum=bySwim?along>0:true;
            // Where the fist holds it: the tail fin's base, 20 % of the length in from the tail's end (2026-10-20, user after
            // the same run: "it holds the tail fin, but not the fin's base - it floats"; 8 % before, inside the fin: the fin's
            // base hung about 0.1 m under the fist. A carp's tail fin is about a fifth of its length - a guess, not measured
            // on the model). EVR_FISH_GRIP_PERCENT=<2..45>: another share.
            static const float gripShare=[]{char v[16]{};const auto n=GetEnvironmentVariableA("EVR_FISH_GRIP_PERCENT",v,sizeof v);const float x=n&&n<sizeof v?float(std::atof(v)):20.f;return (x>=2.f&&x<=45.f?x:20.f)/100.f;}();
            tail[axis]=headAtMaximum?l[axis]+gripShare*e[axis]:h[axis]-gripShare*e[axis];
            Vec3 lengthwise{0,0,0},up{0,1,0};(axis==0?lengthwise.x:axis==1?lengthwise.y:lengthwise.z)=headAtMaximum?1.f:-1.f;if(axis==1)up={0,0,1};
            // The palm (the left socket) in the world.
            Vec3 palm=rotate(modelQ,Vec3{gs[socket].position.x*modelScale.x,gs[socket].position.y*modelScale.y,gs[socket].position.z*modelScale.z});
            // Fixed in the hand (2026-10-20, user: "can its angle follow the hand too"): the socket is the controller's grip
            // (include/evr/body_ik.hpp puts it on the target), whose -Z a held sword's blade follows. The head toward the
            // grip's -Y and the back toward its +Z (11:40, user after the headset run: "with the hand turned on its side it
            // should hang as a fish held up by its tail fin - turn the head 90 degrees down"; 11:10 the head went out of the
            // fist's little-finger side, +Z, the back +Y); the fish turns with the hand. EVR_FISH_HANG=1: it hangs straight
            // down from the palm, its back toward the character's forward.
            static const bool hang=[]{char v[8]{};const auto n=GetEnvironmentVariableA("EVR_FISH_HANG",v,sizeof v);return n==1&&v[0]=='1';}();
            const auto handQ=normalized(multiply(modelQ,gs[socket].orientation));
            const Vec3 down{0,-1,0};const auto forward=rotate(modelQ,Vec3{0,0,1});
            // 12:20 (user after the headset run: "the tail fin's base should come up to the thumb's base, about 5 cm higher;
            // I hold the fish with the hand as in the last 10 seconds - tilt it 20 to 30 degrees more, the head down"):
            // bug report 121352, its last 10 s (376 rows): straight down in the grip's frame was (0.116, -0.898, 0.425) on
            // average - 26 degrees from the grip's -Y (25..30). The head goes that way and the back turns with it, and the
            // fish 5 cm toward the grip's "up" of that hold (EVR_FISH_RAISE_CM=<0..15>).
            // 13:10 (user after the headset run: "the fish tilts the wrong way"): the left socket's frame is the controller's
            // grip turned half round its up axis (guess - x and z the other way), so the head goes to (-0.116, -0.898,
            // -0.425) and the 5 cm up with it.
            static const Vec3 headInGrip=[]{const Vec3 v{-.116f,-.898f,-.425f};return v*(1/length(v));}();
            static const Vec3 backInGrip=rotate(motion::alignDirections(Vec3{0,-1,0},headInGrip),Vec3{0,0,1});
            static const float raise=[]{char v[16]{};const auto n=GetEnvironmentVariableA("EVR_FISH_RAISE_CM",v,sizeof v);const float x=n&&n<sizeof v?float(std::atof(v)):5.f;return (x>=0&&x<=15?x:5.f)/100.f;}();
            if(!hang)palm=palm+rotate(handQ,headInGrip*-raise);
            const auto towardHead=hang?down:rotate(handQ,headInGrip),backToward=hang?forward:rotate(handQ,backInGrip);
            const auto first=motion::alignDirections(lengthwise,towardHead);
            // then about that line, the back toward its side (both taken square to the line)
            const auto upNow=rotate(first,up);
            const auto square=[&](Vec3 v){return v-towardHead*dot(v,towardHead);};
            Vec3 a=square(upNow),b=square(backToward);
            Quat o=first;
            if(length(a)>.05f&&length(b)>.05f)o=normalized(multiply(motion::alignDirections(a*(1/length(a)),b*(1/length(b))),first));
            const Vec3 tailScaled{tail[0]*scale.x,tail[1]*scale.y,tail[2]*scale.z};
            const auto origin=palm-rotate(o,tailScaled);
            if(finite(origin)&&length(origin)<4.f){
                WorldPositionQ32 q{modelPosition.x+int64_t(double(origin.x)*4294967296.),modelPosition.y+int64_t(double(origin.y)*4294967296.),modelPosition.z+int64_t(double(origin.z)*4294967296.)};
                auto pose=m.pose;std::memcpy(pose.data(),&q,24);std::memcpy(pose.data()+24,&o,16);
                auto handle=m.handle;std::array<uintptr_t,2> ids{reinterpret_cast<uintptr_t>(&handle),1},poses{reinterpret_cast<uintptr_t>(pose.data()),1};
                motionVisual::change(reinterpret_cast<void*>(s),ids.data(),poses.data());
                {std::lock_guard<std::mutex> lock(attachmentObserver::followMutex);if(auto* f=attachmentObserver::fishModelOf(fish,false))f->written=pose;}
                static unsigned tailLogs=0;if(tailLogs++<8||tailLogs%600==0)logFile->event("fishing_fish_tail_grip","\"fish\":"+std::to_string(fish)+",\"character\":"+std::to_string(character)+
                    ",\"bounds_min\":["+std::to_string(lo.x)+","+std::to_string(lo.y)+","+std::to_string(lo.z)+"],\"bounds_max\":["+std::to_string(hi.x)+","+std::to_string(hi.y)+","+std::to_string(hi.z)+
                    "],\"scale\":["+std::to_string(scale.x)+","+std::to_string(scale.y)+","+std::to_string(scale.z)+"],\"long_axis\":"+std::to_string(axis)+",\"tail_at_minimum\":"+(headAtMaximum?"true":"false")+",\"head_by\":\""+(hang?"hang":bySwim?"swim":"guess")+"\",\"grip_share\":"+std::to_string(gripShare)+",\"swum_m\":"+std::to_string(swum)+",\"swum_along_axis_m\":"+std::to_string(along)+",\"holds\":"+std::to_string(tailLogs));
                return true;}
        }
    }catch(const std::exception&){}
    // Not a fish - a boot in a net and the like (2026-10-20 17:40, user after the headset run 20261020-172049: "put the boot
    // where the hand turns the reel's handle; check the model from the data - what is held is the net the boot is in, not
    // the boot, and the boot and the water weeds hang down along gravity"; the game's hold put it 0.17..0.30 m from the
    // palm, fishing_fish_held in_animated_hand_m): its top in the left fist - the socket, where a handle is held, as the
    // reel's handle when reeling - and the rest hanging straight down, turned with the wrist. The model is taken as made
    // hanging, +Y up (the boot's extent: Y -0.51 .. +0.26, its longest; Enshrouded's models stand on +Y), held a share of
    // its height under its top (EVR_CATCH_GRIP_PERCENT=<0..45>, default 10). EVR_CATCH_HANG=0: the game's hold as before.
    static const bool catchHang=[]{char v[8]{};const auto n=GetEnvironmentVariableA("EVR_CATCH_HANG",v,sizeof v);return !(n==1&&v[0]=='0');}();
    if(catchHang&&!swamAsFish)try{
        const auto resource=read<uintptr_t>(object+layout::modelResource);const auto descriptor=read<uintptr_t>(resource+layout::resourceDescriptor);
        const auto lo=read<Vec3>(descriptor+16),hi=read<Vec3>(descriptor+32);Vec3 scale{1,1,1};std::memcpy(&scale,m.pose.data()+40,12);
        const Vec3 extent=hi-lo;
        if(finite(lo)&&finite(hi)&&finite(scale)&&extent.x>=0&&extent.y>.02f&&extent.z>=0&&std::max({extent.x,extent.y,extent.z})<4.f&&extent.y>=extent.x&&extent.y>=extent.z&&   // made standing: its height the longest
           
           scale.x>.1f&&scale.x<10.f&&scale.y>.1f&&scale.y<10.f&&scale.z>.1f&&scale.z<10.f){
            static const float share=[]{char v[16]{};const auto n=GetEnvironmentVariableA("EVR_CATCH_GRIP_PERCENT",v,sizeof v);const float x=n&&n<sizeof v?float(std::atof(v)):10.f;return (x>=0.f&&x<=45.f?x:10.f)/100.f;}();
            Vec3 grip{(lo.x+hi.x)*.5f,hi.y-share*extent.y,(lo.z+hi.z)*.5f};
            // The two models of what is not a fish (game data 1.0, docs/analysis/fishing_catches_20261020: the boot in its net,
            // fishing_prop_fishing_shoe_02_minigame, Y -0.513 .. 0.262; the treasure sack, fishing_prop_lootcontainer_01_minigame,
            // Y -0.613 .. 0.357 - guess: what the trash and treasure tables give): one skeleton, every vertex on joint 3, the
            // net's neck at Y 0.14 .. 0.16 (2.3 / 4.3 cm thick) with its tuft above and the sack, the boot and the weeds below.
            // Held at the neck, joint 3's rest place (0, 0.154, -0.008); another model by the share above.
            const auto about=[](float a,float b){return std::abs(a-b)<.02f;};
            if((about(lo.y,-.513f)&&about(hi.y,.262f))||(about(lo.y,-.613f)&&about(hi.y,.357f)))grip={0.f,.154f,-.008f};
            const Vec3 palm=rotate(modelQ,Vec3{gs[socket].position.x*modelScale.x,gs[socket].position.y*modelScale.y,gs[socket].position.z*modelScale.z});
            // upright; its front (+Z) the way the fist points (the grip's -Z), level
            const auto handQ=normalized(multiply(modelQ,gs[socket].orientation));
            Vec3 ahead=rotate(handQ,Vec3{0,0,-1});ahead.y=0;
            if(length(ahead)<.2f){ahead=rotate(modelQ,Vec3{0,0,1});ahead.y=0;}
            const float yaw=std::atan2(ahead.x,ahead.z);
            const Quat o=normalized(Quat{0,std::sin(yaw/2),0,std::cos(yaw/2)});
            const Vec3 gripScaled{grip.x*scale.x,grip.y*scale.y,grip.z*scale.z};
            const auto origin=palm-rotate(o,gripScaled);
            if(finite(origin)&&length(origin)<4.f){
                WorldPositionQ32 q{modelPosition.x+int64_t(double(origin.x)*4294967296.),modelPosition.y+int64_t(double(origin.y)*4294967296.),modelPosition.z+int64_t(double(origin.z)*4294967296.)};
                auto pose=m.pose;std::memcpy(pose.data(),&q,24);std::memcpy(pose.data()+24,&o,16);
                auto handle=m.handle;std::array<uintptr_t,2> ids{reinterpret_cast<uintptr_t>(&handle),1},poses{reinterpret_cast<uintptr_t>(pose.data()),1};
                motionVisual::change(reinterpret_cast<void*>(s),ids.data(),poses.data());
                {std::lock_guard<std::mutex> lock(attachmentObserver::followMutex);if(auto* f=attachmentObserver::fishModelOf(fish,false))f->written=pose;}
                static unsigned hangLogs=0;if(hangLogs++<8||hangLogs%600==0)logFile->event("fishing_catch_hung","\"fish\":"+std::to_string(fish)+",\"character\":"+std::to_string(character)+
                    ",\"bounds_min\":["+std::to_string(lo.x)+","+std::to_string(lo.y)+","+std::to_string(lo.z)+"],\"bounds_max\":["+std::to_string(hi.x)+","+std::to_string(hi.y)+","+std::to_string(hi.z)+
                    "],\"grip\":["+std::to_string(grip.x)+","+std::to_string(grip.y)+","+std::to_string(grip.z)+"],\"swum_m\":"+std::to_string(m.swimMetres)+",\"yaw_deg\":"+std::to_string(yaw*57.29578f)+",\"holds\":"+std::to_string(hangLogs));
                return true;}
        }
    }catch(const std::exception&){}
    const auto held=compose(gs[socket],inHand);
    const auto worldOffset=rotate(modelQ,Vec3{held.position.x*modelScale.x,held.position.y*modelScale.y,held.position.z*modelScale.z});
    if(!finite(worldOffset))return false;
    WorldPositionQ32 q{modelPosition.x+int64_t(double(worldOffset.x)*4294967296.),modelPosition.y+int64_t(double(worldOffset.y)*4294967296.),modelPosition.z+int64_t(double(worldOffset.z)*4294967296.)};
    const auto o=normalized(multiply(modelQ,held.orientation));
    auto pose=m.pose;std::memcpy(pose.data(),&q,24);std::memcpy(pose.data()+24,&o,16);
    auto handle=m.handle;std::array<uintptr_t,2> ids{reinterpret_cast<uintptr_t>(&handle),1},poses{reinterpret_cast<uintptr_t>(pose.data()),1};
    motionVisual::change(reinterpret_cast<void*>(s),ids.data(),poses.data());
    {std::lock_guard<std::mutex> lock(attachmentObserver::followMutex);if(auto* f=attachmentObserver::fishModelOf(fish,false))f->written=pose;}
    static unsigned logs=0;if(logs++<8||logs%600==0)logFile->event("fishing_fish_held","\"fish\":"+std::to_string(fish)+",\"character\":"+std::to_string(character)+",\"handle\":"+std::to_string(handle)+
        ",\"in_animated_hand_m\":"+std::to_string(length(inHand.position))+",\"moved_m\":"+std::to_string(length(delta-worldOffset))+",\"holds\":"+std::to_string(logs));
    return true;
}
// In the water (set by the policy: the local actor's FloatingOnWater / UnderWater state, a fresh sample only).
// EVR_BODY_IK_SWIM=0 in the game's environment: solve as on land (for a comparison).
inline bool (*swimming)(uint64_t)=nullptr;
// What the other players are sent (online_sync.hpp): the body this frame's solve gave and where the character stands.
struct Published {bool have=false;uint64_t serial=0;sync::Body body;sync::Place place{};};
inline Published published;
// The self test of the other players' side (EVR_SYNC_SELF): the own character is driven by what came back through the
// datagrams (remote_body.hpp), so the solve here is made and published and nothing of it is put on the model.
inline bool remoteDrivesOwn=false;
inline void returnHands(uintptr_t s){
    if(!motionHandVisual::lentPose)return;
    if(motionHandVisual::handle&&motionHandVisual::poseHandle){
        auto id=motionHandVisual::handle;std::array<uintptr_t,2> ids{reinterpret_cast<uintptr_t>(&id),1};
        reinterpret_cast<motionHandVisual::Bind>(baseAddress+0x110880)(reinterpret_cast<void*>(s),ids.data(),motionHandVisual::poseHandle);
        if(read<uint32_t>(motionVisual::object(motionHandVisual::handle)+layout::modelPose)!=motionHandVisual::poseHandle)throw std::runtime_error("body ik: the hands did not return to their pose");
    }
    motionHandVisual::lentPose=0;
}
inline void bind(uintptr_t s,uint32_t handle,uint32_t pose){
    auto id=handle;std::array<uintptr_t,2> ids{reinterpret_cast<uintptr_t>(&id),1};
    reinterpret_cast<motionHandVisual::Bind>(baseAddress+0x110880)(reinterpret_cast<void*>(s),ids.data(),pose);
}
// Back onto the game's pose while the body is not driven; prepare() takes the parts again.
inline void release(const char* reason){
    if(!poseHandle)return;
    returnHands(scene);
    unsigned restored=0;
    for(const auto handle:bound){
        const auto object=motionVisual::currentObject(handle);
        if(!object||read<uint32_t>(object+layout::modelPose)!=poseHandle)continue;
        bind(scene,handle,nativePose);
        if(read<uint32_t>(motionVisual::currentObject(handle)+layout::modelPose)!=nativePose)throw std::runtime_error("body ik: release failed");
        ++restored;
    }
    if(restored&&releases++<200)logFile->event("motion_body_ik_released","\"frame\":"+std::to_string(liveFrame.frame)+",\"reason\":"+jsonString(reason)+",\"restored\":"+std::to_string(restored));
}
inline void prepare(const game::StableAnchor& anchor,uint64_t renderSerial){
    if(!requested)return;ready=false;published.have=false;
    // Only inside this frame's proven native mutation boundary, with the hand rig the hand visual built.
    if(!motionVisual::contextReady||motionVisual::contextSerial!=renderSerial||motionVisual::contextFrame!=liveFrame.frame||!motionHandVisual::rig||!motionHandVisual::sourceHandle)return;
    if(!rootCamera::firstPerson(rootCamera::snapshot())){release("not first person");return;}
    if(!motionHandVisual::ready||motionHandVisual::serial!=renderSerial||!finite(liveFrame.head.position)){release("hands not shown");return;}
    const auto s=motionVisual::scene;const auto& hands=shownHands;
    equipmentObserver::Snapshot equipment;{std::lock_guard<std::mutex> lock(equipmentObserver::snapshotMutex);equipment=equipmentObserver::latest;}
    const auto handSource=motionVisual::object(motionHandVisual::sourceHandle);
    // The bare-hand model (part 11) stays on the game's pose: the hand visual reads it there every frame.
    const auto native=read<uint32_t>(handSource+layout::modelPose);
    if(!native)return;
    if(poseHandle&&native!=nativePose)throw std::runtime_error("body ik: the game's pose changed");
    const auto record=handPoseMetadata::pose(s,native),owner=read<uintptr_t>(record);
    const auto animatedBytes=motionHandVisual::snapshot(motionHandVisual::joints(record),102*48);
    std::vector<hand::Joint> animated(102);std::memcpy(animated.data(),animatedBytes.data(),animatedBytes.size());
    if(!poseHandle){
        const auto hierarchy=read<std::array<uint32_t,4>>(owner);
        poseHandle=reinterpret_cast<motionHandVisual::PoseCreate>(baseAddress+0xfb850)(reinterpret_cast<void*>(s),hierarchy.data());
        if(!poseHandle||poseHandle==native||poseHandle==motionHandVisual::poseHandle){poseHandle=0;throw std::runtime_error("body ik: pose creation failed/aliased");}
        nativePose=native;poseResource=owner;scene=s;
        std::string parts;
        for(unsigned part=0;part<equipment.handles.size();part++)if(equipment.handles[part]){
            const auto object=motionVisual::currentObject(equipment.handles[part]);
            parts+=std::string(parts.empty()?"":",")+"["+std::to_string(part)+","+std::to_string(object?read<uint32_t>(object+layout::modelPose):0)+","+std::to_string(equipment.masks[part])+"]";}
        logFile->event("motion_body_ik_created","\"pose_handle\":"+std::to_string(poseHandle)+",\"native_pose\":"+std::to_string(nativePose)+
            ",\"native_pose_models\":"+std::to_string(read<uint64_t>(record+0x58))+",\"parts_pose_mask\":["+parts+"],\"native_pose_written\":false,\"solver\":\"neck_pivot_chest_arms_hips_knees\"");
    }
    // The body models stand at the character's own transform (origin at the feet). Headset and controllers go
    // through the world: anchor camera -> world -> that model's space.
    const auto transform=read<std::array<uint8_t,56>>(handSource+0x38);
    WorldPositionQ32 modelPosition;Quat modelQ;Vec3 modelScale;
    std::memcpy(&modelPosition,transform.data(),24);std::memcpy(&modelQ,transform.data()+24,16);std::memcpy(&modelScale,transform.data()+40,12);
    modelQ=normalized(modelQ);
    if(!finite(modelScale)||modelScale.x<.5f||modelScale.y<.5f||modelScale.z<.5f||anchor.worldUnitsPerMeter!=1)throw std::runtime_error("body ik: model scale unexpected");
    const auto facing=normalized(anchor.orientation);
    const auto camera=cameraPosition(anchor.root,rotate(facing,anchor.cameraOffsetGameUnits),1);
    // The game leans the whole model while the character swims and dives; the solver's space is the model's stood
    // upright (include/evr/body_ik.hpp uprightPart). A model that stands is solved in its own space, as before.
    const float leanRadians=bodyik::leanAngle(modelQ);const bool leaning=leanRadians>1e-3f;
    const auto leanQ=bodyik::leanPart(modelQ),solveQ=leaning?bodyik::uprightPart(modelQ):modelQ;
    const auto inModel=[&](Pose xr){
        const auto tracking=game::trackingPose(compose(liveTrackingToAnchor,xr));
        const auto world=cameraPosition(camera,rotate(facing,tracking.position),anchor.worldUnitsPerMeter);
        const Vec3 delta{float(relativeQ32(world.x,modelPosition.x)),float(relativeQ32(world.y,modelPosition.y)),float(relativeQ32(world.z,modelPosition.z))};
        const auto local=rotate(inverse(solveQ),delta);
        return Pose{normalized(multiply(inverse(solveQ),multiply(facing,tracking.orientation))),{local.x/modelScale.x,local.y/modelScale.y,local.z/modelScale.z}};
    };
    const auto& rig=*motionHandVisual::rig;const auto& parents=rig.parents();
    bodyik::Targets targets;targets.head=inModel(liveFrame.head);
    for(unsigned h=0;h<2;h++){auto grip=hands.hands[h].grip;
        if(h==1&&motionVisual::bowDrawHandValid)grip.position=motionVisual::bowDrawHand; // as the private hands
        targets.socket[h]=inModel(grip);}
    static const bool swimWanted=[]{char value[8]{};const auto n=GetEnvironmentVariableA("EVR_BODY_IK_SWIM",value,sizeof value);return !(n&&n<sizeof value&&value[0]=='0');}();
    targets.swim=swimWanted&&swimming&&swimming(GetTickCount64());
    // The double jump: the legs hang under the hips (include/evr/body_ik.hpp legsRest), in over 0.1 s and out over 0.2 s
    // after it (landing). EVR_BODY_IK_JUMP_LEGS=0 in the game's environment: the animation's legs as before.
    {static const bool wanted=[]{char value[8]{};const auto n=GetEnvironmentVariableA("EVR_BODY_IK_JUMP_LEGS",value,sizeof value);return !(n==1&&value[0]=='0');}();
     static float weight=0;static uint64_t last=0;static unsigned logs=0;const auto now=GetTickCount64();
     const bool on=wanted&&!targets.swim&&inputActor::localJump::doubleJump(now);const float dt=last&&now>last?std::min(float(now-last)/1000.f,.1f):0.f;last=now;
     const float before=weight;weight=on?std::min(1.f,weight+dt/.1f):std::max(0.f,weight-dt/.2f);targets.legsRest=weight;
     if((before==0)!=(weight==0)&&logs++<200)logFile->event("motion_body_ik_jump_legs","\"frame\":"+std::to_string(liveFrame.frame)+",\"resting\":"+(weight>0?"true":"false")+
         ",\"jump_count\":"+std::to_string(inputActor::localJump::count.load())+",\"jump_type\":"+std::to_string(inputActor::localJump::type.load()));}
    if(leaning)bodyik::lean(animated,parents,leanQ);
    bodyik::Report report;
    auto joints=bodyik::solve(animated,rig.neutral(),parents,targets,{},{},&report);
    const auto upright=joints; // in the solver's space, for the record below
    // The other players' side of the body, tried on the own one (EVR_SYNC_LOOPBACK=1 in the game's environment; no
    // launch flag, nothing of it is shown): what would be sent goes through the datagram (include/evr/player_sync.hpp)
    // and the same solve is done again from it alone, as on another machine with this frame's animation. On record:
    // how far that body is from the own one (the datagram's rounding), and how far behind the receiver's stepping is.
    static const bool loopback=[]{char value[8]{};const auto n=GetEnvironmentVariableA("EVR_SYNC_LOOPBACK",value,sizeof value);return n&&n<sizeof value&&value[0]=='1';}();
    if(loopback){
        static sync::Receiver receiver;static uint16_t sequence=0;static unsigned logs=0;static float worstEver=0;
        const sync::Model model{solveQ,modelScale};
        const auto body=sync::fromSolve(model,targets,report.hips,motionHandVisual::lastCurls);
        const auto bytes=sync::encode(body,++sequence,sync::Place{},uint16_t(GetTickCount64()&0xffff));const auto back=sync::decode(bytes);
        if(!back||back->sequence!=sequence)throw std::runtime_error("player sync loopback: the datagram was not taken");
        const auto again=bodyik::globals(bodyik::solve(animated,rig.neutral(),parents,sync::targets(model,back->body)),parents),own=bodyik::globals(upright,parents);
        float worst=0;unsigned worstJoint=0;
        for(size_t j=0;j<own.size();j++){const float d=length(again[j].position-own[j].position);if(d>worst){worst=d;worstJoint=unsigned(j);}}
        worstEver=std::max(worstEver,worst);
        const double now=double(GetTickCount64())*1e-3;const bool taken=receiver.offer(*back,now);const auto shown=receiver.update(now);
        float behind=0;if(shown&&shown->ik)behind=std::max({length(shown->head.position-back->body.head.position),length(shown->hand[0].position-back->body.hand[0].position),length(shown->hand[1].position-back->body.hand[1].position)});
        if(updates%30==0&&logs++<600){std::ostringstream o;o<<std::setprecision(6)<<"\"frame\":"<<liveFrame.frame<<",\"bytes\":"<<bytes.size()<<",\"sequence\":"<<sequence<<",\"taken\":"<<(taken?"true":"false")
            <<",\"worst_joint_m\":"<<worst<<",\"worst_joint\":"<<worstJoint<<",\"worst_so_far_m\":"<<worstEver<<",\"hips_m\":"<<length(again[1].position-own[1].position)
            <<",\"sockets_m\":["<<length(again[64].position-own[64].position)<<','<<length(again[94].position-own[94].position)<<"],\"ankles_m\":["<<length(again[4].position-own[4].position)<<','<<length(again[8].position-own[8].position)
            <<"],\"stepping_behind_m\":"<<behind<<",\"swim\":"<<(targets.swim?"true":"false")<<",\"shown\":false";
            logFile->event("player_sync_loopback",o.str());}
    }
    published.body=sync::fromSolve(sync::Model{solveQ,modelScale},targets,report.hips,motionHandVisual::lastCurls);
    published.body.leaning=leaning;published.body.lean=leaning?leanQ:Quat{0,0,0,1};   // the receiver's copy may not lean (sync::Body lean)
    published.body.bowDrawn=motionVisual::bowDrawHandValid;
    // The held items relative to the hand sockets (online 2.1): the copies shown this frame or the last, about the
    // character's position like the sockets, so that the receiver puts them on its copy's hands the same way.
    published.body.item=published.body.offhandItem=false;
    for(unsigned h=0;h<2;h++){const auto& s=attachmentObserver::shownItem[h];
        if(!s.frame||liveFrame.frame<s.frame||liveFrame.frame-s.frame>1)continue;
        WorldPositionQ32 q;Quat o;std::memcpy(&q,s.transform.data(),24);std::memcpy(&o,s.transform.data()+24,16);
        const Vec3 p{float(relativeQ32(q.x,modelPosition.x)),float(relativeQ32(q.y,modelPosition.y)),float(relativeQ32(q.z,modelPosition.z))};
        const float norm=o.x*o.x+o.y*o.y+o.z*o.z+o.w*o.w;
        if(!finite(p)||length(p)>4.f||!std::isfinite(norm)||std::abs(norm-1)>.02f)continue;
        const auto rel=sync::inHand(published.body.hand[h],Pose{normalized(o),p});
        if(!finite(rel.position)||length(rel.position)>2.f)continue;
        if(h){published.body.item=true;published.body.itemInHand=rel;}else{published.body.offhandItem=true;published.body.offhandInHand=rel;}}
    published.place=sync::place({double(modelPosition.x)/4294967296.,double(modelPosition.y)/4294967296.,double(modelPosition.z)/4294967296.});
    published.serial=renderSerial;published.have=true;
    if(remoteDrivesOwn)return;
    if(leaning)bodyik::lean(joints,parents,inverse(leanQ));
    // The own caught fish, from the catch (35) until it is taken or let go (36..38).
    if(!leaning){const auto st=controllerUiFixture::fishingState.load();const auto at=controllerUiFixture::fishingStateMs.load();const auto t=GetTickCount64();
        if(st>=35&&st<=38&&at&&t>=at&&t-at<=500)try{holdFish(s,controllerUiFixture::fishingFishId.load(),animated,joints,parents,modelPosition,modelQ,modelScale,rootCamera::snapshot().entity);}
        catch(const std::exception& e){static unsigned told=0;if(told++<3)logFile->event("fishing_fish_hold_failure","\"reason\":"+jsonString(e.what()));}}
    // Fingers as the private hands show them (curl from the controls), so gloves on this pose match.
    if(motionHandVisual::lastOutput.size()==joints.size())for(unsigned h=0;h<2;h++){const auto wrist=rig.branches()[h].wrist;
        for(size_t j=wrist+1u;j<joints.size();j++){bool under=false;for(auto p=parents[j];p!=UINT16_MAX;p=parents[p])if(p==wrist){under=true;break;}
            if(under){joints[j].orientation=motionHandVisual::lastOutput[j].orientation;joints[j].position=motionHandVisual::lastOutput[j].position;}}}
    std::array<uintptr_t,2> span{reinterpret_cast<uintptr_t>(joints.data()),joints.size()};
    reinterpret_cast<motionHandVisual::PoseUpdate>(baseAddress+0x110f80)(reinterpret_cast<void*>(s),poseHandle,span.data());
    // Every local body part still on the game's pose moves to the mod's pose (new parts after an equipment change too).
    unsigned moved=0;
    for(unsigned part=0;part<equipment.handles.size();part++)if(const auto handle=equipment.handles[part]){
        if(part==11||handle==motionHandVisual::sourceHandle)continue;
        const auto object=motionVisual::currentObject(handle);
        if(!object||read<uint32_t>(object+layout::modelPose)!=nativePose)continue;
        bind(s,handle,poseHandle);
        if(read<uint32_t>(motionVisual::currentObject(handle)+layout::modelPose)!=poseHandle)throw std::runtime_error("body ik: bind failed");
        if(std::find(bound.begin(),bound.end(),handle)==bound.end())bound.push_back(handle);++moved;
    }
    if(handsOnBody&&motionHandVisual::handle){
        static const bool keepPrivate=[]{char value[8]{};const auto n=GetEnvironmentVariableA("EVR_BODY_IK_PRIVATE_HANDS",value,sizeof value);return n&&n<sizeof value&&value[0]=='1';}();
        if(keepPrivate)handsOnBody=false;
        else{
            auto id=motionHandVisual::handle;std::array<uintptr_t,2> ids{reinterpret_cast<uintptr_t>(&id),1},transforms{reinterpret_cast<uintptr_t>(transform.data()),1};
            if(read<uint32_t>(motionVisual::object(id)+layout::modelPose)!=poseHandle){
                reinterpret_cast<motionHandVisual::Bind>(baseAddress+0x110880)(reinterpret_cast<void*>(s),ids.data(),poseHandle);
                if(read<uint32_t>(motionVisual::object(id)+layout::modelPose)!=poseHandle)throw std::runtime_error("body ik: the hands did not take the body's pose");
                motionHandVisual::lentPose=poseHandle;
                if(handLends++<8)logFile->event("motion_body_ik_hands","\"frame\":"+std::to_string(liveFrame.frame)+",\"hand_model\":"+std::to_string(id)+",\"pose\":"+std::to_string(poseHandle)+",\"private_pose\":"+std::to_string(motionHandVisual::poseHandle)+",\"at_body_transform\":true");
            }
            // The hand visual has just put its copy at the headset's origin; on this pose it belongs at the body's transform.
            motionVisual::change(reinterpret_cast<void*>(s),ids.data(),transforms.data());
        }
    }
    // The game's pose must be as it was read: nothing here writes it.
    if(motionHandVisual::snapshot(motionHandVisual::joints(handPoseMetadata::pose(s,nativePose)),102*48)!=animatedBytes)throw std::runtime_error("body ik: the game's pose changed during the update");
    ready=true;serial=renderSerial;++updates;
    static unsigned jumpPoses=0;   // every frame of a double jump too (the first 400)
    if(moved||updates%30==1||(targets.legsRest>0&&jumpPoses++<400)){
        const auto g=bodyik::globals(upright,parents),a=bodyik::globals(animated,parents);
        const auto v=[](Vec3 p){std::ostringstream o;o<<std::setprecision(6)<<'['<<p.x<<','<<p.y<<','<<p.z<<']';return o.str();};
        const auto forward=rotate(targets.head.orientation,{0,0,1});
        std::ostringstream o;o<<std::setprecision(6)<<"\"frame\":"<<liveFrame.frame<<",\"serial\":"<<renderSerial<<",\"moved\":"<<moved<<",\"bound\":"<<bound.size()
            <<",\"pose_models\":"<<read<uint64_t>(handPoseMetadata::pose(s,poseHandle)+0x58)<<",\"native_pose_models\":"<<read<uint64_t>(handPoseMetadata::pose(s,nativePose)+0x58)
            <<",\"eyes\":"<<v(targets.head.position)<<",\"view\":"<<v(forward)<<",\"grips\":["<<v(targets.socket[0].position)<<','<<v(targets.socket[1].position)<<']'
            <<",\"pivot\":"<<v(report.pivot)<<",\"head\":"<<v(g[17].position)<<",\"chest\":"<<v(g[15].position)<<",\"hips\":"<<v(g[1].position)<<",\"animated_hips\":"<<v(a[1].position)
            <<",\"shoulders\":["<<v(g[40].position)<<','<<v(g[70].position)<<"],\"elbows\":["<<v(g[42].position)<<','<<v(g[72].position)<<"],\"wrists\":["<<v(g[44].position)<<','<<v(g[74].position)
            <<"],\"sockets\":["<<v(g[64].position)<<','<<v(g[94].position)<<"],\"knees\":["<<v(g[3].position)<<','<<v(g[7].position)<<"],\"ankles\":["<<v(g[4].position)<<','<<v(g[8].position)
            <<"],\"animated_ankles\":["<<v(a[4].position)<<','<<v(a[8].position)<<"],\"torso\":"<<report.torso<<",\"hips_below_animated\":"<<report.hipsBelowAnimated
            <<",\"swim\":"<<(targets.swim?"true":"false")<<",\"legs_rest\":"<<targets.legsRest<<",\"model_lean_deg\":"<<leanRadians*57.29578f<<",\"solved_upright\":"<<(leaning?"true":"false")
            <<",\"socket_error\":["<<report.socketError[0]<<','<<report.socketError[1]<<"],\"arm_stretch\":["<<report.armStretch[0]<<','<<report.armStretch[1]
            <<"],\"leg_short\":["<<report.legShort[0]<<','<<report.legShort[1]<<"],\"native_pose_unchanged\":true";
        logFile->event("motion_body_ik_pose",o.str());
    }
}
// Back onto the game's pose (when it still exists), then the mod's pose is removed.
inline void dispose(uintptr_t disposed){
    if(!poseHandle||disposed!=scene)return;
    unsigned restored=0,gone=0;bool nativeAlive=true;
    try{returnHands(scene);}catch(...){motionHandVisual::lentPose=0;}
    try{handPoseMetadata::pose(scene,nativePose);}catch(...){nativeAlive=false;}
    for(const auto handle:bound){
        const auto object=motionVisual::currentObject(handle);
        if(!object||read<uint32_t>(object+layout::modelPose)!=poseHandle){++gone;continue;}
        bind(scene,handle,nativeAlive?nativePose:0);++restored;
    }
    bound.clear();
    const auto left=read<uint64_t>(handPoseMetadata::pose(scene,poseHandle)+0x58);
    if(!left)reinterpret_cast<motionHandVisual::PoseRemove>(baseAddress+0x10e180)(reinterpret_cast<void*>(scene),poseHandle);
    logFile->event("motion_body_ik_disposed","\"restored\":"+std::to_string(restored)+",\"already_gone\":"+std::to_string(gone)+",\"native_pose_alive\":"+(nativeAlive?"true":"false")+
        ",\"models_left_on_pose\":"+std::to_string(left)+",\"updates\":"+std::to_string(updates)+",\"releases\":"+std::to_string(releases));
    poseHandle=0;ready=false;
}
}
