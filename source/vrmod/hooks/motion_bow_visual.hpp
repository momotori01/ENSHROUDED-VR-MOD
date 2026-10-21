// Explicit known-asset diagnostic: private ranged RenderPose, native local joints
// copied at the completed CPU boundary. No source model/pose/action writes.
namespace motionBowVisual {
inline bool physicalPoseRequested=false;
// The string of the drawn bow as shown this frame (tracking space), for what sits on it (motion_arrow_visual.hpp).
struct DrawnString {uint64_t serial=0,frame=0;bool held=false;Vec3 hand{};};inline DrawnString drawnString;
struct Profile {unsigned count=0,skin=0,type=0;std::array<uint8_t,16> hierarchy{};std::vector<uint32_t> names;std::vector<uint16_t> parents,skinMap;};
inline const Profile* profile(){
 if(!motionVisual::bowAnimationRequested)return nullptr;
 const auto asset=motion::catalogRangedAsset(motionVisual::item,motionVisual::guid);
 if(!asset)return nullptr;
 // Wider offline-reviewed set is limited to the explicit Mock ranged trial.
 // BowAnimation alone retains the single bow tested before the catalog work.
 if(!motionVisual::rangedAnimationRequested&&!motion::reviewedBowFacing(motionVisual::item,motionVisual::guid))return nullptr;
 static const auto profiles=[](){
  std::array<Profile,motion::catalogRangedRigs.size()> result{};
  for(size_t i=0;i<result.size();++i){const auto& r=motion::catalogRangedRigs[i];auto& p=result[i];
   p.count=r.count;p.skin=r.skin;p.type=r.type;p.hierarchy=r.hierarchy;
   p.names.assign(r.names.begin(),r.names.begin()+r.count);p.parents.assign(r.parents.begin(),r.parents.begin()+r.count);
   p.skinMap.assign(r.skinMap.begin(),r.skinMap.begin()+r.skin);
  }return result;
 }();
 return &profiles.at(asset->rig);
}

struct Owned {uint32_t model=0,pose=0,item=0;unsigned type=0;uintptr_t owner=0;uint64_t updates=0;};
inline std::array<Owned,motion::retainedVisualCapacity> owned{};
inline uintptr_t jointAddress(uintptr_t scene,uint32_t pose,unsigned expectedCount){
 const auto record=handPoseMetadata::pose(scene,pose);
 const auto first=read<uint32_t>(record+8),count=read<uint32_t>(record+12);
 const auto data=read<uintptr_t>(scene+layout::sceneJoints),capacity=read<uint64_t>(scene+layout::sceneJointCapacity);
 if(!data||count!=expectedCount||capacity>1000000||uint64_t(first)+count>capacity)throw std::runtime_error("bow joint storage differs");
 return data+size_t(first)*48;
}
inline void prepare(uint64_t serial){
 if(!motionVisual::poseReady||motionVisual::updatedSerial!=serial||motionVisual::updatedFrame!=liveFrame.frame)return;
 // The left-hand bow can remain tracked while the right hand mesh is unavailable.
 // Physical cancellation still needs a fresh rest pose; do not retain old draw.
 // The draw route takes every catalog bow shown by the ranged display route (shared skeleton, weapon_mount.hpp).
 const bool drawAsset=motion::bowDrawAsset(motionVisual::item,motionVisual::guid,motionVisual::rangedAnimationRequested&&controllerUiFixture::bowDrawRequested);
 const bool physicalLeft=physicalPoseRequested&&drawAsset&&
    shownHands.focused&&shownHands.hands[0].tracked();
 if(!physicalLeft&&(!motionHandVisual::ready||motionHandVisual::serial!=serial))return;
 const auto spec=profile();if(!spec)return;
 const std::string eventPrefix=spec->type==4?"motion_bow_":"motion_ranged_";
 const auto scene=motionVisual::scene;const auto modelId=motionVisual::handle,sourceId=motionVisual::sourceHandle;
 const auto source=motionVisual::object(sourceId),res=read<uintptr_t>(source+layout::modelResource);
 const auto original=read<std::array<uint8_t,0x1b0>>(source);
 const auto sourcePose=read<uint32_t>(source+layout::modelPose);const auto sourceRecord=handPoseMetadata::pose(scene,sourcePose);
 const auto owner=read<uintptr_t>(sourceRecord),version=read<uint64_t>(sourceRecord+24);
 const auto before=motionHandVisual::snapshot(jointAddress(scene,sourcePose,spec->count),spec->count*48);
 std::vector<hand::Joint> joints(spec->count);std::memcpy(joints.data(),before.data(),before.size());
 const bool physical=physicalPoseRequested&&drawAsset;
 bool physicalHeld=false;float physicalPull=0;uint64_t controlFrame=0;Vec3 physicalNock{},physicalHand{};
 // The native bow's own string (source joint 5) shows the game's time-based draw.
 // The limit applied to this frame came from the previous frame's value.
 const float limitProgress=motionVisual::bowDrawProgress;const bool restricted=motionVisual::bowDrawHandValid;
 if(physical){
  motionVisual::bowDrawProgress=std::clamp((-joints.at(5).position.z-motion::BowPhysicalTrial::restString.z)/motion::knownBowDrawKeys.back().pull,0.f,1.f);
  controllerUiFixture::BowPhysicalPublished state;
  {std::lock_guard<std::mutex> lock(controllerUiFixture::bowPhysicalMutex);state=controllerUiFixture::bowPhysicalPublished;}
  const auto& g=motionVisual::bowGeometry;const auto& hands=shownHands;const auto now=GetTickCount64();
  controlFrame=state.frame;
  physicalHeld=state.held&&state.binding==g.binding&&g.frame==hands.frame&&state.frame<=hands.frame&&hands.frame-state.frame<=8&&
   state.milliseconds&&now>=state.milliseconds&&now-state.milliseconds<=200&&hands.focused&&hands.hands[0].tracked()&&hands.hands[1].tracked()&&
   hands.hands[1].triggerActive&&hands.hands[1].trigger>.2f;
  // The draw route already posed this frame's bow from both hands; use that same decision.
  if(controllerUiFixture::bowDrawRequested)physicalHeld=motionVisual::bowDrawHeld&&g.binding&&g.frame==hands.frame&&hands.focused&&hands.hands[0].tracked();
  if(physicalHeld){
   // Held back by the native draw: the string stops where the drawing hand is shown.
   const auto shown=controllerUiFixture::bowDrawRequested&&restricted?motionVisual::bowDrawHand:hands.hands[1].grip.position;
   physicalHand=shown;
   const auto right=rotate(inverse(g.model.orientation),shown-g.model.position);
   physicalNock={right.x/g.scale.x,right.y/g.scale.y,right.z/g.scale.z};
   physicalPull=std::clamp(right.z/g.scale.z-motion::BowPhysicalTrial::restString.z,0.f,motion::knownBowDrawKeys.back().pull);
  }
  const auto drawn=physicalHeld?motion::knownBowNockPose(physicalPull,physicalNock):motion::knownBowDrawPose(0);joints.assign(drawn.begin(),drawn.end());
 }
 std::vector<uint8_t> expected(before.size());std::memcpy(expected.data(),joints.data(),expected.size());
 for(const auto& j:joints){const auto q=j.orientation;const float norm=q.x*q.x+q.y*q.y+q.z*q.z+q.w*q.w;
  if(!finite(j.position)||!finite(j.scale)||length(j.position)>4||!std::isfinite(norm)||norm<.98f||norm>1.02f||
     j.scale.x<=0||j.scale.y<=0||j.scale.z<=0||j.scale.x>4||j.scale.y>4||j.scale.z>4)throw std::runtime_error("bow local joint bounds");}
 Owned* entry=nullptr;for(auto& e:owned)if(e.model==modelId){entry=&e;break;}
 if(!entry)for(auto& e:owned)if(!e.model){entry=&e;break;}
 if(!entry)throw std::runtime_error("private bow pose capacity");
 bool created=false;
 if(!entry->pose){
  motionHandVisual::validate();
  const auto hierarchy=read<uintptr_t>(owner+layout::poseOwnerHierarchy),model=read<uintptr_t>(res+layout::resourceDescriptor);
  const auto& expected=spec->hierarchy;
  if(!hierarchy||!model||read<std::array<uint8_t,16>>(owner)!=expected||read<std::array<uint8_t,16>>(hierarchy)!=expected||
     read<std::array<uint8_t,16>>(model+layout::renderModelHierarchy)!=expected||read<uint32_t>(hierarchy+48)!=spec->count||read<uint16_t>(model+layout::renderModelHierarchyJoints)!=spec->count)
    throw std::runtime_error("unreviewed ranged hierarchy");
  if(motionHandVisual::array<uint32_t>(hierarchy+16,spec->count)!=spec->names||
     motionHandVisual::array<uint16_t>(hierarchy+24,spec->count)!=spec->parents||
     motionHandVisual::array<uint16_t>(model+layout::renderModelSkinningMap,spec->skin)!=spec->skinMap)throw std::runtime_error("ranged hierarchy/skin profile differs");
  if(read<uint32_t>(motionVisual::object(modelId)+layout::modelPose))throw std::runtime_error("bow clone already has a pose");
  const auto refs=read<uint32_t>(owner+16);if(!refs||refs==UINT32_MAX)throw std::runtime_error("bow pose reference bound");
  entry->model=modelId;entry->owner=owner;entry->type=spec->type;entry->item=motionVisual::item;
  entry->pose=reinterpret_cast<motionHandVisual::PoseCreate>(baseAddress+0xfb8e0)(reinterpret_cast<void*>(scene),expected.data());
  if(!entry->pose||entry->pose==sourcePose){entry->pose=0;throw std::runtime_error("bow private pose failed/aliased");}
  if(read<uintptr_t>(handPoseMetadata::pose(scene,entry->pose))!=owner||read<uint32_t>(owner+16)!=refs+1)throw std::runtime_error("bow pose owner/reference differs");
  std::array<uintptr_t,2> ids{reinterpret_cast<uintptr_t>(&entry->model),1};
  reinterpret_cast<motionHandVisual::Bind>(baseAddress+0x110960)(reinterpret_cast<void*>(scene),ids.data(),entry->pose);
  if(read<uint32_t>(motionVisual::object(modelId)+layout::modelPose)!=entry->pose||read<uint64_t>(handPoseMetadata::pose(scene,entry->pose)+0x58)!=1)throw std::runtime_error("bow private pose binding differs");
  created=true;
  logFile->event(eventPrefix+"created","\"handle\":"+std::to_string(modelId)+",\"pose_handle\":"+std::to_string(entry->pose)+
   ",\"source_handle\":"+std::to_string(sourceId)+",\"source_pose\":"+std::to_string(sourcePose)+",\"item\":"+std::to_string(entry->item)+",\"type\":"+std::to_string(entry->type)+",\"joints\":"+std::to_string(spec->count)+",\"skin_joints\":"+std::to_string(spec->skin)+",\"private_pose\":true,\"references_before\":"+std::to_string(refs)+",\"references_after\":"+std::to_string(read<uint32_t>(owner+16)));
 }
 if(entry->owner!=owner||entry->pose==sourcePose||entry->type!=spec->type||entry->item!=motionVisual::item)throw std::runtime_error("bow source identity changed");
 const auto target=read<std::array<uint8_t,56>>(motionVisual::object(modelId)+0x38);
 std::array<uintptr_t,2> span{reinterpret_cast<uintptr_t>(joints.data()),spec->count};
 reinterpret_cast<motionHandVisual::PoseUpdate>(baseAddress+0x111060)(reinterpret_cast<void*>(scene),entry->pose,span.data());
 if(created){
  // The weapon clone was created at its final rigid transform. Initialize
  // dynamic skinning by moving it through the source transform after binding,
  // then restore the exact tracked transform before the original upload.
  const auto initial=read<std::array<uint8_t,56>>(motionVisual::object(sourceId)+0x38);
  if(!std::memcmp(initial.data(),target.data(),52))throw std::runtime_error("bow dynamic initialization has no transform delta");
  std::array<uintptr_t,2> ids{reinterpret_cast<uintptr_t>(&entry->model),1},from{reinterpret_cast<uintptr_t>(initial.data()),1},to{reinterpret_cast<uintptr_t>(target.data()),1};
  motionVisual::change(reinterpret_cast<void*>(scene),ids.data(),from.data());
  motionVisual::change(reinterpret_cast<void*>(scene),ids.data(),to.data());
 }
 const auto clone=motionVisual::object(modelId),record=handPoseMetadata::pose(scene,entry->pose);
 const auto after=motionHandVisual::snapshot(jointAddress(scene,entry->pose,spec->count),before.size());
 if(read<uint32_t>(clone+layout::modelMask)||read<uint32_t>(clone+layout::modelSdf)||read<uint32_t>(clone+layout::modelAuxiliary)||read<uint32_t>(clone+layout::modelPose)!=entry->pose||
    read<uint32_t>(clone+layout::modelPaletteCount)!=spec->skin||!read<uint32_t>(clone+layout::modelPaletteHandle)||read<uint64_t>(record+0x58)!=1||after!=expected||
    std::memcmp(read<std::array<uint8_t,56>>(clone+0x38).data(),target.data(),52)||read<std::array<uint8_t,0x1b0>>(motionVisual::object(sourceId))!=original||
    read<uint64_t>(handPoseMetadata::pose(scene,sourcePose)+24)!=version||motionHandVisual::snapshot(jointAddress(scene,sourcePose,spec->count),before.size())!=before)
  throw std::runtime_error("bow pose/palette/source isolation invariant failed");
 ++entry->updates;
 // The bow for the other players (2026-10-19, online: "the friend's bow shows at the waist, moving as the hidden model does"):
 // the shown bow is the left hand's item of the body's datagram (motion_body_ik.hpp), the receiver puts its native bow
 // (attachment slot 9, seen in the friend's log) there (attachment_observer.hpp remoteItemFollow). Bows only (type 4).
 if(spec->type==4){auto& shown=attachmentObserver::shownItem[0];shown.serial=serial;shown.frame=liveFrame.frame;std::memcpy(shown.transform.data(),target.data(),40);}
 drawnString={serial,liveFrame.frame,physicalHeld&&spec->type==4&&controllerUiFixture::bowDrawRequested,physicalHand};
 std::array<uint8_t,288> raw{},sourceRaw{};std::memcpy(raw.data(),joints.data(),before.size());std::memcpy(sourceRaw.data(),before.data(),before.size());
 logFile->event(eventPrefix+"pose","\"serial\":"+std::to_string(serial)+",\"frame\":"+std::to_string(liveFrame.frame)+",\"time\":"+std::to_string(liveFrame.time)+
  ",\"handle\":"+std::to_string(modelId)+",\"pose_handle\":"+std::to_string(entry->pose)+",\"source_handle\":"+std::to_string(sourceId)+",\"source_pose\":"+std::to_string(sourcePose)+
  ",\"source_version\":"+std::to_string(version)+",\"source_unchanged\":true,\"owned_equals_source\":"+std::string(expected==before?"true":"false")+",\"mask\":0,\"sdf\":0,\"palette_count\":"+std::to_string(spec->skin)+",\"item\":"+std::to_string(entry->item)+",\"type\":"+std::to_string(entry->type)+",\"joints_bytes\":"+jsonString(nativeImpactComponents::asHex(raw).substr(0,before.size()*2)));
 if(physical)logFile->event("motion_bow_physical_pose","\"serial\":"+std::to_string(serial)+",\"frame\":"+std::to_string(liveFrame.frame)+
  ",\"control_frame\":"+std::to_string(controlFrame)+",\"binding\":"+std::to_string(motionVisual::bowGeometry.binding)+
  ",\"held\":"+(physicalHeld?"true":"false")+",\"model_pull\":"+std::to_string(physicalPull)+
  ",\"tracked_string_contact\":"+(physicalHeld?"true":"false")+
  ",\"native_progress\":"+std::to_string(motionVisual::bowDrawProgress)+",\"limit_progress\":"+std::to_string(limitProgress)+",\"restricted\":"+(restricted&&physicalHeld?"true":"false")+
  ",\"source_joints_bytes\":"+jsonString(nativeImpactComponents::asHex(sourceRaw).substr(0,before.size()*2))+
  ",\"source_unchanged\":true,\"private_pose_only\":true");
}
inline void dispose(uintptr_t scene){
 if(scene!=motionVisual::scene)return;
 for(auto& e:owned)if(e.pose){
  const auto id=e.pose;const auto record=handPoseMetadata::pose(scene,id);const auto refs=read<uint32_t>(e.owner+16);
  if(read<uint64_t>(record+0x58))throw std::runtime_error("private bow pose still bound");
  reinterpret_cast<motionHandVisual::PoseRemove>(baseAddress+0x10e260)(reinterpret_cast<void*>(scene),id);e.pose=0;
  bool invalid=false;try{handPoseMetadata::pose(scene,id);}catch(...){invalid=true;}
  const bool released=refs==1||(refs>1&&read<uint32_t>(e.owner+16)==refs-1);
  logFile->event(e.type==4?"motion_bow_pose_disposed":"motion_ranged_pose_disposed","\"handle\":"+std::to_string(e.model)+",\"pose_handle\":"+std::to_string(id)+",\"generation_invalidated\":"+(invalid?"true":"false")+
   ",\"reference_released\":"+(released?"true":"false")+",\"updates\":"+std::to_string(e.updates));
  if(!invalid||!released)throw std::runtime_error("bow pose retirement failed");e={};
 }
}
}
