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
        const auto bytes=sync::encode(body,++sequence,sync::Place{});const auto back=sync::decode(bytes);
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
