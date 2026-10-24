// Read-only bridge from native XR rendering to the game's input observer.
// Gameplay application remains disabled until native UI and aim routing are proven.
namespace inputPose {
inline game::InputPoseChannel channel;
inline game::InputPoseToken token;
inline std::atomic<bool> enabled{false};
constexpr uint64_t observationMaxAgeMs=200; // freshness bound, never an HMD deadzone
inline void publish(const diagnostic::NativeXrFrame& frame,const Pose& origin,uint64_t epoch){
    if(!enabled.load()||rootCamera::mode!=2)return;
    const game::InputPoseToken next{frame.sessionGeneration,epoch};
    if(!(next==token)){channel.reset(next);token=next;}
    const auto root=rootCamera::snapshot();
    if(!rootCamera::firstPerson(root)){channel.invalidate();
        logFile->event("input_pose_native_camera","\"frame\":"+std::to_string(frame.frame)+",\"serial\":"+std::to_string(root.renderSerial)+",\"invalidated\":true");return;}
    const auto anchor=rootCamera::anchor(root);
    // A provisional (unsettled) eye height is for warmup rendering only; gameplay
    // input never receives a camera derived from a transitional native height.
    if(!rootCamera::calibratedEntity){channel.invalidate();
        logFile->event("input_pose_height_settling","\"frame\":"+std::to_string(frame.frame)+",\"serial\":"+std::to_string(root.renderSerial)+",\"invalidated\":true");return;}
    game::HeadInputSnapshot h;h.anchor=anchor;h.head=frame.head;h.trackingToAnchor=origin;
    // The right hand's pointer of the same frame, for placing by hand (include/evr/pointer_aim_camera.hpp).
    {const auto& hand=frame.hands.hands[1];if(frame.hands.focused&&hand.aimActive&&finite(hand.aim.position)){h.pointer=hand.aim;h.pointerFlags=hand.aimFlags;}}
    h.flags=frame.headFlags;h.xrFrame=frame.frame;h.predictedDisplayTime=frame.time;h.originEpoch=epoch;
    const auto now=GetTickCount64();
    if(!channel.publish(token,root.entity,h,now,root.milliseconds))throw std::runtime_error("input pose publication identity/clock rejected");
    logFile->event("input_pose_published","\"frame\":"+std::to_string(h.xrFrame)+",\"time\":"+std::to_string(h.predictedDisplayTime)+
        ",\"session_generation\":"+std::to_string(token.session)+",\"origin_epoch\":"+std::to_string(token.origin)+
        ",\"entity\":"+std::to_string(root.entity)+",\"head_flags\":"+std::to_string(h.flags)+
        ",\"published_ms\":"+std::to_string(now)+",\"root_ms\":"+std::to_string(root.milliseconds));
}
inline void observe(uint32_t entity,uint32_t tick,unsigned sample,const std::array<float,2>& stick){
    if(!enabled.load())return;
    const auto value=channel.read(entity,GetTickCount64(),observationMaxAgeMs);
    std::ostringstream out;out<<std::setprecision(10)<<"\"sample\":"<<sample<<",\"entity\":"<<entity<<",\"tick\":"<<tick
        <<",\"status\":"<<jsonString(game::poseStatusName(value.status))<<",\"frame\":"<<value.head.xrFrame
        <<",\"time\":"<<value.head.predictedDisplayTime<<",\"session_generation\":"<<value.token.session
        <<",\"origin_epoch\":"<<value.token.origin<<",\"head_flags\":"<<value.head.flags
        <<",\"head_age_ms\":"<<value.headAgeMs<<",\"root_age_ms\":"<<value.rootAgeMs<<",\"max_age_ms\":"<<observationMaxAgeMs;
    // Synthetic permission state permits the value-only geometry calculation.
    // It must never be mistaken for observed native UI/gameplay authorization.
    game::NativeInputState geometry;geometry.enabled=geometry.known=geometry.local=geometry.ready=true;
    geometry.inputMode=1;geometry.playerFlags=1;geometry.stick=stick;
    thread_local float yaw=0;thread_local game::InputPoseToken last;
    if(!(last==value.token)){yaw=0;last=value.token;}
    const auto p=game::proposeInput(geometry,value.head,yaw);
    out<<",\"geometry_fallback\":"<<jsonString(game::fallbackName(p.fallback));
    if(p.active()){
        yaw=p.yaw;
        out<<",\"yaw\":"<<p.yaw<<",\"aim_forward\":["<<p.aimForward.x<<','<<p.aimForward.y<<','<<p.aimForward.z<<']'
           <<",\"world_movement\":["<<p.worldMovement.x<<','<<p.worldMovement.y<<','<<p.worldMovement.z<<']'
           <<",\"camera_q32\":["<<p.cameraPosition.x<<','<<p.cameraPosition.y<<','<<p.cameraPosition.z<<']';
    }
    out<<",\"geometry_preview_only\":true,\"gameplay_state_validated\":false,\"applied\":false";
    logFile->event("input_pose_sample",out.str());
}
// The token goes with it (2026-10-24): the XR session kept for the plate keeps its generation and the origin its epoch, so the
// eyes begun again after a failure brought the same token; it was not reset, and the channel closed here refused every publication
// ("input pose publication identity/clock rejected" on every try - trial auto-start-20261024-144618: VR never came back).
inline void close(){channel.close();token={};}
}
