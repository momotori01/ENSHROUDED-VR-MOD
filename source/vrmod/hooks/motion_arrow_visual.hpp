// The nocked arrow on the bow in the player's hand, as VHVR shows the held arrow: at the string point, pointing at the
// arrow rest (docs/analysis/vhvr_spec_20261010/bow.md 4). The game's own arrow (ShowAmmunition) is not an equipment
// attachment: it is found as the unposed model that moves with the game's bow. That instance stays untouched and out
// of the eyes; a Scene-owned copy of the same model is drawn on the string (mask/SDF 0), in the private slot the
// shield does not use while a bow is held.
namespace motionArrowVisual {
inline bool requested=false,ready=false;
inline uint32_t handle=0,source=0;inline uintptr_t resource=0,sourceResource=0,measuredResource=0,scene=0;inline DWORD thread=0;
inline uint64_t serial=0,frame=0,updates=0,hiddenSerial=0,hiddenFrame=0,lastScan=0,candidatesAt=0;
inline unsigned retired=0,adoptions=0,scans=0,losses=0;
inline uint64_t lastSeenBow=0,lookFastUntil=0,knownTaken=0;   // the bow just came out or shot: looked for every 60 ms
inline std::array<uint64_t,2> payloadSerial{};
struct Seen {uint32_t handle=0;WorldPositionQ32 position{};};
inline std::vector<Seen> candidates;
// Where the nock is on the arrow model: the model axis that points along the shot (from how the game holds its own
// arrow on its bow) and the signed metres from the model origin to the rear end of that axis (from the model's own
// extent; measured on the game's nocked arrow when the extent is not the long thin shape of an arrow).
inline bool measured=false,extentKnown=false;inline float nockDistance=0;inline Vec3 axis{};inline unsigned failures=0;
constexpr float findRange=.35f,keepRange=.6f,nockedRange=.15f;
inline double metres(const WorldPositionQ32& a,const WorldPositionQ32& b){
    const double x=relativeQ32(a.x,b.x),y=relativeQ32(a.y,b.y),z=relativeQ32(a.z,b.z);return std::sqrt(x*x+y*y+z*z);
}
inline Vec3 offset(const WorldPositionQ32& a,const WorldPositionQ32& b){return {float(relativeQ32(a.x,b.x)),float(relativeQ32(a.y,b.y)),float(relativeQ32(a.z,b.z))};}
// Visible unposed models next to the game's bow. The Scene fills its model table from the low indices, so the walk
// ends after eight empty blocks.
inline std::vector<Seen> scan(const WorldPositionQ32& bow){
    std::vector<Seen> out;static std::vector<uint8_t> block;
    const auto objects=read<uintptr_t>(scene+0x1b0),generations=read<uintptr_t>(scene+0x1c8);
    const auto total=size_t(std::min<uint64_t>(std::min<uint64_t>(read<uint64_t>(scene+0x1b8)/0x1b0,read<uint64_t>(scene+0x1d0)),65536));
    constexpr size_t step=2048;block.resize(step*0x1b0);unsigned empty=0;
    for(size_t first=0;first<total&&empty<8&&out.size()<8;first+=step){
        const auto count=std::min<size_t>(step,total-first);SIZE_T got=0;
        if(!ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(objects+first*0x1b0),block.data(),count*0x1b0,&got)||got!=count*0x1b0)break;
        bool any=false;
        for(size_t n=0;n<count&&out.size()<8;++n){const auto* object=block.data()+n*0x1b0;
            uintptr_t res=0;uint32_t pose=0,mask=0;WorldPositionQ32 position;std::memcpy(&res,object+layout::modelResource,8);if(!res)continue;
            any=true;std::memcpy(&pose,object+layout::modelPose,4);std::memcpy(&mask,object+layout::modelMask,4);std::memcpy(&position,object+0x38,24);
            if(pose||!mask||!(metres(position,bow)<findRange))continue;
            try{if(read<uint8_t>(res+0x14)!=1)continue;
                out.push_back({(uint32_t(read<uint8_t>(generations+first+n))<<24)|uint32_t(first+n),position});
            }catch(const std::exception&){}
        }
        empty=any?0:empty+1;
    }
    ++scans;return out;
}
inline void retire(const char* event,const std::string& detail){
    const auto clone=motionVisual::object(handle);
    if(read<uintptr_t>(clone+layout::modelResource)!=resource||read<uint32_t>(clone+layout::modelMask)||read<uint32_t>(clone+layout::modelSdf))throw std::runtime_error("arrow retirement invariant differs");
    const auto id=handle,refs=read<uint32_t>(resource+16);std::array<uintptr_t,2> ids{reinterpret_cast<uintptr_t>(&id),1};
    reinterpret_cast<motionVisual::Remove>(baseAddress+0x10df20)(reinterpret_cast<void*>(scene),ids.data());handle=0;ready=false;
    const bool invalid=read<uint8_t>(read<uintptr_t>(scene+0x1c8)+(id&0xffffff))!=uint8_t(id>>24);
    const bool released=refs==1||(refs>1&&read<uint32_t>(resource+16)==refs-1);
    logFile->event(event,"\"handle\":"+std::to_string(id)+",\"updates\":"+std::to_string(updates)+",\"generation_invalidated\":"+(invalid?"true":"false")+
        ",\"reference_released\":"+(released?"true":"false")+detail);
    if(!invalid||!released)throw std::runtime_error("arrow retirement failed");
}
inline void update(const game::StableAnchor& anchor,uint64_t currentSerial){
    const auto& g=motionVisual::bowGeometry;const auto& string=motionBowVisual::drawnString;
    if(!motionVisual::poseReady||motionVisual::updatedSerial!=currentSerial||motionVisual::updatedFrame!=liveFrame.frame||
       !g.binding||g.frame!=shownHands.frame||string.serial!=currentSerial||string.frame!=liveFrame.frame)return;
    if(scene&&(scene!=motionVisual::scene||thread!=GetCurrentThreadId()))throw std::runtime_error("arrow Scene/thread changed");
    scene=motionVisual::scene;thread=GetCurrentThreadId();
    const auto now=GetTickCount64();
    const auto bow=read<std::array<uint8_t,56>>(motionVisual::object(motionVisual::sourceHandle)+0x38);
    WorldPositionQ32 bowPosition;Quat bowOrientation;std::memcpy(&bowPosition,bow.data(),24);std::memcpy(&bowOrientation,bow.data()+24,16);
    bowOrientation=normalized(bowOrientation);
    if(!lastSeenBow||now-lastSeenBow>500)lookFastUntil=now+2000;lastSeenBow=now;
    if(source){
        const auto object=motionVisual::currentObject(source);
        if(!object||read<uintptr_t>(object+layout::modelResource)!=sourceResource||read<uint32_t>(object+layout::modelPose)||!(metres(read<WorldPositionQ32>(object+0x38),bowPosition)<keepRange)){
            // Shot, put away or replaced: the instance is no longer the arrow on the game's bow.
            logFile->event("motion_arrow_source_lost","\"source_handle\":"+std::to_string(source)+",\"frame\":"+std::to_string(liveFrame.frame)+",\"removed\":"+(object?"false":"true"));
            source=0;++losses;lookFastUntil=now+2000;
        }
    }
    // Looked for while the bow is out, not only while the string is held: the game shows its arrow on the hand of the
    // original model as soon as the bow is in the hand and again after each shot, and there it floated at the hip
    // (headset run 20261014-124515, user: "taking the bow from the left shoulder, an arrow floats at about the waist;
    // the original model holds it probably"). Without the string in the hand the look is slower.
    if(!source&&now-lastScan>=(string.held||now<lookFastUntil?60u:250u)){
        lastScan=now;auto found=scan(bowPosition);
        const auto attached=attachmentObserver::heldHandles(g.entity,scene,now,true);
        found.erase(std::remove_if(found.begin(),found.end(),[&](const Seen& s){return s.handle==motionVisual::sourceHandle||s.handle==handle||
            std::find(attached.begin(),attached.end(),s.handle)!=attached.end();}),found.end());
        // The arrow moves with the hand that holds it; scenery that happens to stand at the bow does not.
        const Seen* pick=nullptr;double best=findRange;
        // An arrow of the model the game showed on its bow before is taken at the first look (headset run
        // 20261014-162900, user: "taking the bow from the shoulder, for an instant the arrow still shows at the belly":
        // the second look, for movement with the bow, left it in sight that long).
        if(sourceResource)for(const auto& f:found)if(read<uintptr_t>(motionVisual::object(f.handle)+layout::modelResource)==sourceResource){
            const double d=metres(f.position,bowPosition);if(d<best){best=d;pick=&f;}}
        if(pick)++knownTaken;
        else if(candidatesAt&&now-candidatesAt<=400)for(const auto& f:found)for(const auto& c:candidates)if(c.handle==f.handle&&std::memcmp(&c.position,&f.position,24)){
            const double d=metres(f.position,bowPosition);if(d<best){best=d;pick=&f;}}
        if(pick){
            const auto object=motionVisual::object(pick->handle),res=read<uintptr_t>(object+layout::modelResource);
            const auto guid=read<std::array<uint32_t,4>>(res+0x18);
            source=pick->handle;sourceResource=res;++adoptions;
            if(measuredResource!=res){measured=false;measuredResource=0;}
            logFile->event("motion_arrow_source","\"source_handle\":"+std::to_string(source)+",\"frame\":"+std::to_string(liveFrame.frame)+",\"metres\":"+std::to_string(best)+
                ",\"model\":"+std::to_string(read<uint16_t>(res+layout::resourceModel))+",\"guid\":["+std::to_string(guid[0])+","+std::to_string(guid[1])+","+std::to_string(guid[2])+","+std::to_string(guid[3])+
                "],\"candidates\":"+std::to_string(found.size())+",\"scans\":"+std::to_string(scans)+",\"moves_with_bow\":true,\"read_only\":true");
            candidates.clear();candidatesAt=0;
        }else{candidates=std::move(found);candidatesAt=now;}
    }
    if(!source)return;
    hiddenSerial=currentSerial;hiddenFrame=liveFrame.frame;
    const auto object=motionVisual::object(source);
    const auto original=read<std::array<uint8_t,432>>(object);
    if(!string.held||!read<uint32_t>(object+layout::modelMask))return;   // nothing on the player's string
    auto transform=read<std::array<uint8_t,56>>(object+0x38);
    WorldPositionQ32 arrowPosition;Quat arrowOrientation;std::memcpy(&arrowPosition,transform.data(),24);std::memcpy(&arrowOrientation,transform.data()+24,16);
    arrowOrientation=normalized(arrowOrientation);
    if(anchor.worldUnitsPerMeter!=1)throw std::runtime_error("arrow world scale differs");
    // The game's bow shoots along its +Z and draws its string to -Z (bow_physical_trial.hpp, reflected from XR).
    const auto nativeShot=rotate(bowOrientation,{0,0,1});
    const auto& rest=motion::BowPhysicalTrial::restString;
    const Vec3 nativeString{0,rest.y*g.scale.y,-(rest.z+motionVisual::bowDrawProgress*motion::knownBowDrawKeys.back().pull)*g.scale.z};
    const auto fromNock=offset(arrowPosition,bowPosition)-rotate(bowOrientation,nativeString);
    const float along=-dot(fromNock,nativeShot),lateral=length(fromNock+nativeShot*along);
    const auto shotInArrow=rotate(inverse(arrowOrientation),nativeShot);
    if(measuredResource!=sourceResource){
        // Same descriptor as the bow profile reads: extent minimum at +16, maximum at +32 (trace_model_catalog.py).
        const auto model=read<uintptr_t>(sourceResource+layout::resourceDescriptor);const auto lo=read<Vec3>(model+16),hi=read<Vec3>(model+32),scale=read<Vec3>(object+0x38+40);
        const std::array<float,3> size{hi.x-lo.x,hi.y-lo.y,hi.z-lo.z},low{lo.x,lo.y,lo.z},high{hi.x,hi.y,hi.z},shotAxis{shotInArrow.x,shotInArrow.y,shotInArrow.z},scales{scale.x,scale.y,scale.z};
        const size_t k=size[0]>size[1]?(size[0]>size[2]?0:2):(size[1]>size[2]?1:2),a=(k+1)%3,b=(k+2)%3;
        const bool arrowShape=finite(lo)&&finite(hi)&&finite(scale)&&size[k]>.3f&&size[k]<2.5f&&size[a]<size[k]*.25f&&size[b]<size[k]*.25f&&scales[k]>.5f&&scales[k]<1.5f;
        // Not along the game's bow yet: its hand is still bringing the arrow to the string.
        if(arrowShape&&std::abs(shotAxis[k])>=.866f){
            const float sign=shotAxis[k]>0?1.f:-1.f;axis={k==0?sign:0.f,k==1?sign:0.f,k==2?sign:0.f};
            nockDistance=(sign>0?low[k]:-high[k])*scales[k];measured=extentKnown=true;measuredResource=sourceResource;
        }else if(!arrowShape&&lateral<nockedRange&&std::abs(along)<1.5f&&motionVisual::bowDrawProgress>=.99f){
            Vec3 snapped=shotInArrow;const float ax=std::abs(shotInArrow.x),ay=std::abs(shotInArrow.y),az=std::abs(shotInArrow.z);
            if(ax>=.866f)snapped={shotInArrow.x>0?1.f:-1.f,0,0};else if(ay>=.866f)snapped={0,shotInArrow.y>0?1.f:-1.f,0};else if(az>=.866f)snapped={0,0,shotInArrow.z>0?1.f:-1.f};
            nockDistance=along;axis=snapped;measured=true;extentKnown=false;measuredResource=sourceResource;
        }
        if(measuredResource==sourceResource){
            std::ostringstream row;row<<std::setprecision(6)<<"\"source_handle\":"<<source<<",\"frame\":"<<liveFrame.frame<<",\"extent_min\":["<<lo.x<<','<<lo.y<<','<<lo.z<<"],\"extent_max\":["<<hi.x<<','<<hi.y<<','<<hi.z
                <<"],\"model_axis\":["<<axis.x<<','<<axis.y<<','<<axis.z<<"],\"nock_distance\":"<<nockDistance<<",\"from_model_extent\":"<<(extentKnown?"true":"false")
                <<",\"native_along\":"<<along<<",\"native_lateral\":"<<lateral<<",\"native_progress\":"<<motionVisual::bowDrawProgress;
            logFile->event("motion_arrow_model",row.str());
        }
    }
    if(!measured)return;
    // The player's bow: string point at the drawing hand as shown, arrow rest on the handle (bow_draw.hpp).
    const auto facing=normalized(anchor.orientation);
    const auto origin=cameraPosition(anchor.root,rotate(facing,anchor.cameraOffsetGameUnits),1);
    auto world=[&](Vec3 tracking){return cameraPosition(origin,rotate(facing,game::trackingPose(compose(liveTrackingToAnchor,Pose{{0,0,0,1},tracking})).position),anchor.worldUnitsPerMeter);};
    const auto nock=world(string.hand),arrowRest=world(g.model.position+rotate(g.model.orientation,{0,rest.y*g.scale.y,0}));
    auto shot=offset(arrowRest,nock);const float span=length(shot);
    if(!std::isfinite(span)||span<.05f)return;
    shot=shot*(1/span);
    const auto transplanted=normalized(multiply(g.worldOrientation,multiply(inverse(bowOrientation),arrowOrientation)));
    const auto rotation=normalized(multiply(motion::alignDirections(rotate(transplanted,axis),shot),transplanted));
    const auto position=cameraPosition(nock,shot*-nockDistance,1);
    std::memcpy(transform.data(),&position,24);std::memcpy(transform.data()+24,&rotation,16);
    if(handle&&resource!=sourceResource){retire("motion_arrow_retired",",\"another_arrow\":true");++retired;}
    if(!handle){
        motionVisual::validate();const auto refs=read<uint32_t>(sourceResource+16);if(!refs||refs==UINT32_MAX)throw std::runtime_error("arrow resource reference bound");
        const auto guid=read<std::array<uint32_t,4>>(sourceResource+0x18);resource=sourceResource;
        handle=reinterpret_cast<motionVisual::Create>(baseAddress+0xfb450)(reinterpret_cast<void*>(scene),guid.data(),transform.data());
        if(!handle||handle==source)throw std::runtime_error("arrow creation failed/aliased");
        auto clone=motionVisual::object(handle);
        reinterpret_cast<motionVisual::Mask>(baseAddress+0xf7030)(reinterpret_cast<void*>(scene),reinterpret_cast<void*>(clone),0);
        clone=motionVisual::object(handle);
        if(read<uintptr_t>(clone+layout::modelResource)!=resource||read<uint32_t>(resource+16)!=refs+1||read<uint32_t>(clone+layout::modelMask)||read<uint32_t>(clone+layout::modelAuxiliary)||read<uint32_t>(clone+layout::modelPose))throw std::runtime_error("arrow clone resource/pose differs");
        const auto sdfId=read<uint32_t>(clone+layout::modelSdf);
        if(sdfId){
            if(sdfId==read<uint32_t>(motionVisual::object(source)+layout::modelSdf))throw std::runtime_error("arrow SDF aliases source");
            const auto sdf=read<uintptr_t>(scene+layout::sceneSdf);const auto count=read<uint64_t>(sdf+0x78);if(!count)throw std::runtime_error("arrow SDF count");
            using RemoveSdf=void(__fastcall*)(void*,uint32_t);reinterpret_cast<RemoveSdf>(baseAddress+0x15a540)(reinterpret_cast<void*>(sdf),sdfId);
            *reinterpret_cast<uint32_t*>(motionVisual::object(handle)+layout::modelSdf)=0;
            if(read<uint64_t>(sdf+0x78)!=count-1)throw std::runtime_error("arrow SDF removal failed");
        }
        logFile->event("motion_arrow_created","\"handle\":"+std::to_string(handle)+",\"source_handle\":"+std::to_string(source)+",\"model\":"+std::to_string(read<uint16_t>(resource+layout::resourceModel))+
            ",\"references_before\":"+std::to_string(refs)+",\"references_after\":"+std::to_string(read<uint32_t>(resource+16))+",\"mask\":0,\"sdf\":0");
    }else{
        std::array<uintptr_t,2> ids{reinterpret_cast<uintptr_t>(&handle),1},poses{reinterpret_cast<uintptr_t>(transform.data()),1};
        motionVisual::change(reinterpret_cast<void*>(scene),ids.data(),poses.data());
    }
    const auto clone=motionVisual::object(handle);
    if(read<std::array<uint8_t,432>>(motionVisual::object(source))!=original||read<uintptr_t>(clone+layout::modelResource)!=resource||
       read<uint32_t>(clone+layout::modelMask)||read<uint32_t>(clone+layout::modelSdf)||read<uint32_t>(clone+layout::modelPose)||
       std::memcmp(read<std::array<uint8_t,56>>(clone+0x38).data(),transform.data(),52))throw std::runtime_error("arrow source/pose invariant failed");
    serial=currentSerial;frame=liveFrame.frame;ready=true;++updates;
    std::ostringstream out;out<<std::setprecision(6)<<"\"serial\":"<<serial<<",\"frame\":"<<frame<<",\"handle\":"<<handle<<",\"source_handle\":"<<source
        <<",\"q32\":["<<position.x<<','<<position.y<<','<<position.z<<"],\"orientation\":["<<rotation.x<<','<<rotation.y<<','<<rotation.z<<','<<rotation.w
        <<"],\"shot\":["<<shot.x<<','<<shot.y<<','<<shot.z<<"],\"string_to_rest\":"<<span<<",\"nock_distance\":"<<nockDistance<<",\"native_along\":"<<along<<",\"native_lateral\":"<<lateral
        <<",\"native_progress\":"<<motionVisual::bowDrawProgress<<",\"model_axis\":["<<axis.x<<','<<axis.y<<','<<axis.z<<"],\"from_model_extent\":"<<(extentKnown?"true":"false")<<",\"shot_in_arrow\":["<<shotInArrow.x<<','<<shotInArrow.y<<','<<shotInArrow.z
        <<"],\"bow_axis_agreement\":"<<dot(shot,rotate(g.worldOrientation,{0,0,1}))<<",\"source_unchanged\":true,\"mask\":0,\"sdf\":0";
    logFile->event("motion_arrow_pose",out.str());
}
// A surprise here must not take the frame's other preparations (body, effects) with it: the arrow is switched off
// for the session and the game's own arrow shows again.
inline void prepare(const game::StableAnchor& anchor,uint64_t currentSerial){
    if(!requested)return;ready=false;
    try{update(anchor,currentSerial);}
    catch(const std::exception& e){ready=false;source=0;requested=false;++failures;
        logFile->event("motion_arrow_failure","\"reason\":"+jsonString(e.what())+",\"frame\":"+std::to_string(liveFrame.frame)+",\"arrow_display_off\":true");}
}
inline void dispose(uintptr_t s){
    if(!scene||s!=scene)return;
    if(handle){
        if(thread!=GetCurrentThreadId())throw std::runtime_error("arrow disposal thread differs");
        retire("motion_arrow_disposed",",\"adoptions\":"+std::to_string(adoptions)+",\"scans\":"+std::to_string(scans)+",\"scene_disposal\":true");
    }
    source=0;candidates.clear();candidatesAt=0;scene=0;
}
}
