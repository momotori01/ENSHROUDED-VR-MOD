// Another player's VR body: the same IK as the own one (motion_body_ik.hpp), on that player's character, from what
// that player sent (include/evr/player_sync.hpp). As there: the character's part models are re-bound from the game's
// animation pose to a pose owned by the mod, which every frame takes that character's animated joints with head,
// chest, arms and pelvis from the datagram; the game's pose is never written, and re-binding the parts to it undoes
// everything - that happens as soon as no data comes any more, the sender's IK pauses, or the character is gone.
// VHVR does the same with a second VRIK on the other player's object (player_sync.md 4.5).
// The character's parts are found the way the own ones are (equipment_observer.hpp: the game's update of a dressed
// character runs for every such character). Only characters on the player's own skeleton are driven.
// Nothing here may end the player's own VR: every failure ends that one character's driving and goes on record.
namespace remoteBody {
// After a failure a character is left alone for some hundred frames, and for good after the fifth.
// weight (2026-10-20, spec_20261015.md 2.6): how far the body stands in the sent pose instead of the game's animation -
// up from 0 over fadeSeconds when the driving begins, down again when the data stops (the last body is driven on while
// it fades), so that the arms do not jump. The last body, the parts and "own" for that.
struct Driven {uint32_t poseHandle=0,nativePose=0;std::vector<uint32_t> bound;uint64_t updates=0,drivenSerial=0,retryFrame=0;unsigned releases=0,failures=0;WorldPositionQ32 root{};
 float weight=0;uint64_t weightMs=0;bool haveLast=false,lastOwn=false;sync::Body last{};std::array<uint32_t,24> lastHandles{};};
inline constexpr double fadeSeconds=.3;
inline void failedNow(Driven& d){++d.failures;d.retryFrame=liveFrame.frame+600;}
inline std::map<uint32_t,Driven> bodies;inline uintptr_t scene=0;inline unsigned logs=0,failureLogs=0;
inline bool poseAlive(uintptr_t s,uint32_t pose){try{handPoseMetadata::pose(s,pose);return true;}catch(...){return false;}}
// Back onto the game's pose (when it still exists); the mod's pose stays for the next time.
inline void release(uint32_t entity,Driven& d,const char* reason) noexcept {
    if(!d.poseHandle||d.bound.empty())return;
    try{
        const bool alive=poseAlive(scene,d.nativePose);unsigned restored=0;
        for(const auto handle:d.bound){const auto object=motionVisual::currentObject(handle);
            if(!object||read<uint32_t>(object+layout::modelPose)!=d.poseHandle)continue;
            motionBodyIk::bind(scene,handle,alive?d.nativePose:0);++restored;}
        d.bound.clear();d.weight=0;d.haveLast=false;
        if(restored&&d.releases++<40)logFile->event("remote_body_released","\"entity\":"+std::to_string(entity)+",\"frame\":"+std::to_string(liveFrame.frame)+",\"reason\":"+jsonString(reason)+",\"restored\":"+std::to_string(restored)+
            ",\"native_pose_alive\":"+(alive?"true":"false"));
    }catch(const std::exception& e){failedNow(d);d.bound.clear();if(failureLogs++<20)try{logFile->event("remote_body_failure","\"entity\":"+std::to_string(entity)+",\"at\":\"release\",\"reason\":"+jsonString(e.what()));}catch(...){}}
}
inline void retire(uint32_t entity,Driven& d,const char* reason) noexcept {
    release(entity,d,reason);
    try{if(d.poseHandle&&poseAlive(scene,d.poseHandle)&&!read<uint64_t>(handPoseMetadata::pose(scene,d.poseHandle)+0x58))
            reinterpret_cast<motionHandVisual::PoseRemove>(baseAddress+0x10e260)(reinterpret_cast<void*>(scene),d.poseHandle);}catch(...){}
    d.poseHandle=0;d.nativePose=0;
}
// One character, this frame. handles: its part models (equipment observer). own: the local player's character (the
// self test) - its bare-hand part stays on the game's pose, where the hand visual reads it. True: driven.
inline bool drive(uint32_t entity,const std::array<uint32_t,24>& handles,bool own,const sync::Body& body,uint64_t renderSerial,bool fadingOut=false) noexcept {
    auto& d=bodies[entity];if(d.failures>=5||liveFrame.frame<d.retryFrame)return false;
    try{
        if(!motionHandVisual::rig||!motionHandVisual::poseResource)return false;
        const auto s=motionVisual::scene;if(!s)return false;
        if(scene&&scene!=s){for(auto& [id,other]:bodies){other.poseHandle=0;other.nativePose=0;other.bound.clear();}}   // another Scene: its poses went with it
        scene=s;
        const auto& rig=*motionHandVisual::rig;const auto& parents=rig.parents();
        // The game's pose: the one the parts stand on that is not the mod's. All parts of a character share it.
        // (the body's twenty parts; rings, shield and main hand, where they are models of the character at all, are
        // taken along only when they stand on that pose too).
        uint32_t native=0;uintptr_t anchor=0;
        for(unsigned part=0;part<20;part++)if(const auto handle=handles[part]){const auto object=motionVisual::currentObject(handle);if(!object)continue;
            if(!anchor)anchor=object;const auto pose=read<uint32_t>(object+layout::modelPose);
            if(pose&&pose!=d.poseHandle){if(native&&pose!=native)throw std::runtime_error("the character's parts stand on more than one pose");native=pose;}}
        if(!anchor)return false;
        if(!native)native=d.nativePose;
        if(!native)return false;
        if(d.poseHandle&&native!=d.nativePose)retire(entity,d,"the game gave the character another pose");
        const auto record=handPoseMetadata::pose(s,native),owner=read<uintptr_t>(record);
        if(owner!=motionHandVisual::poseResource)throw std::runtime_error("the character is not on the player's skeleton");
        const auto animatedBytes=motionHandVisual::snapshot(motionHandVisual::joints(record),102*48);
        std::vector<hand::Joint> animated(102);std::memcpy(animated.data(),animatedBytes.data(),animatedBytes.size());
        if(!d.poseHandle){
            const auto hierarchy=read<std::array<uint32_t,4>>(owner);
            d.poseHandle=reinterpret_cast<motionHandVisual::PoseCreate>(baseAddress+0xfb8e0)(reinterpret_cast<void*>(s),hierarchy.data());
            if(!d.poseHandle||d.poseHandle==native||d.poseHandle==motionHandVisual::poseHandle||d.poseHandle==motionBodyIk::poseHandle){d.poseHandle=0;throw std::runtime_error("pose creation failed/aliased");}
            d.nativePose=native;
            if(logs++<60)logFile->event("remote_body_created","\"entity\":"+std::to_string(entity)+",\"own_character\":"+(own?"true":"false")+",\"pose_handle\":"+std::to_string(d.poseHandle)+",\"native_pose\":"+std::to_string(native)+
                ",\"native_pose_models\":"+std::to_string(read<uint64_t>(record+0x58))+",\"native_pose_written\":false");
        }
        const auto transform=read<std::array<uint8_t,56>>(anchor+0x38);std::memcpy(&d.root,transform.data(),24);
        Quat modelQ;Vec3 modelScale;std::memcpy(&modelQ,transform.data()+24,16);std::memcpy(&modelScale,transform.data()+40,12);modelQ=normalized(modelQ);
        if(!finite(modelScale)||modelScale.x<.5f||modelScale.y<.5f||modelScale.z<.5f||modelScale.x>2||modelScale.y>2||modelScale.z>2)throw std::runtime_error("model scale unexpected");
        // A leaning model (swimming, diving) is solved upright, as the own one. The lean is the owner's when it was sent
        // (sync::Body lean, 2026-10-21: this copy of a swimming guest did not lean, the body stood out of the water);
        // otherwise this copy's own. EVR_REMOTE_SENT_LEAN=0: always the copy's own.
        static const bool sentLean=[]{char v[8]{};const auto n=GetEnvironmentVariableA("EVR_REMOTE_SENT_LEAN",v,sizeof v);return !(n==1&&v[0]=='0');}();
        const bool useSent=sentLean&&body.leaning;
        const auto ownLean=bodyik::leanPart(modelQ);
        const auto leanQ=useSent?normalized(body.lean):ownLean;
        const float leanRadians=bodyik::leanAngle(useSent?multiply(bodyik::uprightPart(modelQ),leanQ):modelQ);const bool leaning=leanRadians>1e-3f;
        const auto solveQ=leaning||useSent?bodyik::uprightPart(modelQ):modelQ;
        const auto targets=sync::targets(sync::Model{solveQ,modelScale},body);
        if(leaning)bodyik::lean(animated,parents,leanQ);
        bodyik::Report report;
        auto joints=bodyik::solve(animated,rig.neutral(),parents,targets,{},{},&report);
        if(leaning)bodyik::lean(joints,parents,inverse(leanQ));
        // The caught fish of that player in that player's left hand, as the own one (motion_body_ik.hpp holdFish): the
        // character's NetworkFishing (state, fish) as this game has it.
        if(!leaning&&motionBodyIk::fishHoldWanted())try{
            const int type=controllerUiFixture::fishingComponentType.load();const auto world=controllerUiFixture::fishingComponentWorld.load();
            if(type>=0&&world){const auto fishing=nativeImpactComponents::component(nativeImpactComponents::entityAtWorld(world,entity),unsigned(type),96);
                if(fishing){const auto state=read<uint8_t>(fishing);const auto fish=read<uint32_t>(fishing+0x34);
                    WorldPositionQ32 root;std::memcpy(&root,transform.data(),24);
                    if(state>=22&&state<=30&&fish)attachmentObserver::markFishFighting(fish,GetTickCount64());
                    if(state>=35&&state<=38&&fish)motionBodyIk::holdFish(s,fish,animated,joints,parents,root,modelQ,modelScale,entity);}}}
        catch(const std::exception& e){static unsigned told=0;if(told++<3)logFile->event("remote_fish_hold_failure","\"entity\":"+std::to_string(entity)+",\"reason\":"+jsonString(e.what()));}
        // Fingers from the curl amounts, as the sender's own hands are made.
        if(rig.curlEnabled()){const auto fingers=rig.locate({identityPose(),identityPose()},&body.fingers);
            for(unsigned h=0;h<2;h++){const auto wrist=rig.branches()[h].wrist;
                for(size_t j=wrist+1u;j<joints.size();j++){bool under=false;for(auto p=parents[j];p!=UINT16_MAX;p=parents[p])if(p==wrist){under=true;break;}
                    if(under){joints[j].orientation=fingers[j].orientation;joints[j].position=fingers[j].position;}}}}
        // The fade (2.6): at most 50 ms of it a frame, so that a long pause does not jump either.
        {const auto nowMs=GetTickCount64();const double dt=d.weightMs&&nowMs>=d.weightMs?std::min(double(nowMs-d.weightMs)*1e-3,.05):.016;d.weightMs=nowMs;
         if(d.bound.empty()&&!fadingOut)d.weight=std::min(d.weight,0.f);
         d.weight=float(std::clamp(double(d.weight)+(fadingOut?-dt:dt)/fadeSeconds,0.,1.));}
        if(d.weight<1.f)for(size_t j=0;j<joints.size()&&j<animated.size();++j){
            joints[j].orientation=bodyik::blend(animated[j].orientation,joints[j].orientation,d.weight);
            joints[j].position=animated[j].position+(joints[j].position-animated[j].position)*d.weight;}
        if(!fadingOut){d.last=body;d.haveLast=true;d.lastOwn=own;d.lastHandles=handles;}
        std::array<uintptr_t,2> span{reinterpret_cast<uintptr_t>(joints.data()),joints.size()};
        reinterpret_cast<motionHandVisual::PoseUpdate>(baseAddress+0x111060)(reinterpret_cast<void*>(s),d.poseHandle,span.data());
        unsigned moved=0;
        for(unsigned part=0;part<handles.size();part++)if(const auto handle=handles[part]){
            if(own&&(part==11||handle==motionHandVisual::sourceHandle))continue;
            const auto object=motionVisual::currentObject(handle);
            if(!object||read<uint32_t>(object+layout::modelPose)!=native)continue;
            motionBodyIk::bind(s,handle,d.poseHandle);
            if(read<uint32_t>(motionVisual::currentObject(handle)+layout::modelPose)!=d.poseHandle)throw std::runtime_error("bind failed");
            if(std::find(d.bound.begin(),d.bound.end(),handle)==d.bound.end())d.bound.push_back(handle);++moved;
        }
        if(motionHandVisual::snapshot(motionHandVisual::joints(handPoseMetadata::pose(s,native)),102*48)!=animatedBytes)throw std::runtime_error("the game's pose changed during the update");
        d.drivenSerial=renderSerial;++d.updates;
        if((moved||d.updates%60==1)&&logs++<600){
            const auto v=[](Vec3 p){std::ostringstream o;o<<std::setprecision(6)<<'['<<p.x<<','<<p.y<<','<<p.z<<']';return o.str();};
            std::ostringstream o;o<<std::setprecision(6)<<"\"entity\":"<<entity<<",\"frame\":"<<liveFrame.frame<<",\"serial\":"<<renderSerial<<",\"moved\":"<<moved<<",\"bound\":"<<d.bound.size()<<",\"updates\":"<<d.updates
                <<",\"eyes\":"<<v(targets.head.position)<<",\"sockets\":["<<v(targets.socket[0].position)<<','<<v(targets.socket[1].position)<<"],\"hips\":"<<v(report.hips.position)
                <<",\"socket_error\":["<<report.socketError[0]<<','<<report.socketError[1]<<"],\"swim\":"<<(targets.swim?"true":"false")<<",\"model_lean_deg\":"<<leanRadians*57.29578f<<",\"native_pose_unchanged\":true";
            logFile->event("remote_body_pose",o.str());}
        return true;
    }catch(const std::exception& e){
        failedNow(d);release(entity,d,"failure");
        if(failureLogs++<20)try{logFile->event("remote_body_failure","\"entity\":"+std::to_string(entity)+",\"at\":\"drive\",\"reason\":"+jsonString(e.what()));}catch(...){}
        return false;
    }
}
// The characters that were not driven this frame go back to the game's animation.
inline void releaseOthers(uint64_t renderSerial,const char* reason) noexcept {
    for(auto& [entity,d]:bodies)if(d.drivenSerial!=renderSerial&&!d.bound.empty()){
        // Fading out on the last body first (2.6); back on the game's pose once the weight is gone.
        if(d.haveLast&&d.weight>.02f){const auto last=d.last;const auto handles=d.lastHandles;
            if(drive(entity,handles,d.lastOwn,last,renderSerial,true)&&d.weight>.02f)continue;}
        release(entity,d,reason);}
}
inline bool driven(uint32_t entity,uint64_t renderSerial){const auto found=bodies.find(entity);return found!=bodies.end()&&found->second.drivenSerial==renderSerial&&renderSerial!=0;}
// With the Scene: every character back onto the game's pose, the mod's poses removed.
inline void dispose(uintptr_t disposed) noexcept {
    if(!scene||disposed!=scene)return;unsigned count=0;
    for(auto& [entity,d]:bodies){if(d.poseHandle){retire(entity,d,"scene disposal");++count;}}
    bodies.clear();scene=0;
    try{logFile->event("remote_body_disposed","\"characters\":"+std::to_string(count));}catch(...){}
}
}
