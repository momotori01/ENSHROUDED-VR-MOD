// Explicit single-item visual preview. This cannot authorize native combat.
namespace weaponGripPreview {
inline bool requested=false;inline uint32_t item=0,type=0,slot=0;
inline std::array<uint32_t,4> handGuid{},weaponGuid{};
inline motion::Weapon weapon;inline Vec3 scale{};
inline std::vector<hand::Joint> handPose;
inline void load(const std::filesystem::path& folder){
    std::ifstream f(folder/"world-grip-preview.txt");std::string magic;f>>magic>>item>>type>>slot;
    for(auto& v:handGuid)f>>v;for(auto& v:weaponGuid)f>>v;
    auto& p=weapon.gripToModel.position;auto& q=weapon.gripToModel.orientation;auto& off=weapon.offHandGripInModel;
    f>>p.x>>p.y>>p.z>>q.x>>q.y>>q.z>>q.w>>scale.x>>scale.y>>scale.z>>off.x>>off.y>>off.z;f>>std::ws;
    if(!f||!f.eof()||magic!="EVR_GRIP_PREVIEW_1"||item!=1940764111||type!=7||slot!=9||!finite(p)||length(p)>2||
       !finite(scale)||scale.x<.9f||scale.y<.9f||scale.z<.9f||scale.x>1.1f||scale.y>1.1f||scale.z>1.1f||!finite(off)||length(off)>2)
        throw std::runtime_error("invalid measured grip preview");
    q=normalized(q);weapon.binding=item;weapon.mainHand=1;
    std::ifstream poseFile(folder/"grip-hand-pose.bin",std::ios::binary|std::ios::ate);
    if(!poseFile||poseFile.tellg()!=102*48)throw std::runtime_error("grip hand pose shape differs");
    handPose.resize(102);poseFile.seekg(0);poseFile.read(reinterpret_cast<char*>(handPose.data()),102*48);
    if(!poseFile)throw std::runtime_error("grip hand pose unreadable");requested=true;
    logFile->event("grip_preview_enabled","\"item\":"+std::to_string(item)+",\"type\":7,\"combat_enabled\":false,\"frozen_pose\":true,\"anatomical_grip_axes\":true");
}
inline Pose locate(const motion::HandFrame& hands,const attachmentObserver::Child& source,uintptr_t resource,uint64_t serial){
    if(source.item!=item||!source.weaponCategoryKnown||source.weaponType!=type||source.slot!=slot||
       read<std::array<uint32_t,4>>(resource+24)!=weaponGuid)throw std::runtime_error("grip preview equipped asset changed");
    auto w=weapon;w.twoHanded=hands.hands[0].tracked()&&hands.hands[1].tracked()&&hands.hands[0].grabActive&&hands.hands[1].grabActive&&hands.hands[0].grab&&hands.hands[1].grab;
    const auto p=motion::weaponPose(hands,w);
    std::ostringstream row;row<<std::setprecision(9)<<"\"serial\":"<<serial<<",\"frame\":"<<hands.frame<<",\"two_hands\":"<<(w.twoHanded?"true":"false")
      <<",\"position\":["<<p.position.x<<','<<p.position.y<<','<<p.position.z<<"],\"orientation\":["<<p.orientation.x<<','<<p.orientation.y<<','<<p.orientation.z<<','<<p.orientation.w
      <<"],\"resource_verified\":true,\"combat_enabled\":false";logFile->event("grip_preview_pose",row.str());return p;
}
}
