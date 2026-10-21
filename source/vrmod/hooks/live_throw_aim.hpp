// A throw of the hand waits in tracking space (controller_ui_fixture.hpp); with this frame's anchor it becomes a line
// in the world, for the game's spawn of the thrown item (bow_spawn_observer.hpp).
namespace liveThrowAim {
inline uint64_t converted=0;
inline void prepare(const game::StableAnchor& anchor){
    if(!controllerUiFixture::liveHandUseRequested)return;
    controllerUiFixture::ThrowRequest r;{std::lock_guard<std::mutex> lock(controllerUiFixture::throwMutex);r=controllerUiFixture::throwRequest;}
    if(!r.serial||r.serial==converted)return;converted=r.serial;
    try{
        const auto now=GetTickCount64();
        if(now<r.milliseconds||now-r.milliseconds>500)throw std::runtime_error("throw request stale");
        if(anchor.worldUnitsPerMeter!=1)throw std::runtime_error("throw world scale differs");
        const auto facing=normalized(anchor.orientation);
        const auto origin=cameraPosition(anchor.root,rotate(facing,anchor.cameraOffsetGameUnits),1);
        const auto position=cameraPosition(origin,rotate(facing,game::trackingPose(compose(liveTrackingToAnchor,Pose{{0,0,0,1},r.origin})).position),1);
        auto direction=rotate(facing,game::trackingVector(rotate(liveTrackingToAnchor.orientation,r.direction)));
        const float size=length(direction);if(!std::isfinite(size)||size<1e-4f)throw std::runtime_error("throw direction");
        direction=direction*(1/size);
        controllerUiFixture::ThrowWorldAim w;w.serial=r.serial;w.milliseconds=r.milliseconds;w.aim={position,motion::lookAlong(direction,{0,1,0}),direction};
        {std::lock_guard<std::mutex> lock(controllerUiFixture::throwMutex);controllerUiFixture::throwWorldAim=w;}
        // The throw as the sent camera too (2026-10-19, online: the friend's throws "could not be thrown"; a client's thrown
        // item is made by the host's server from the camera of the input this player's game sends, controllerUiFixture::BowAim).
        // The throwables' spawn camera is the throw's line itself: in 49 throws of three headset sessions (throw_aim_call)
        // the camera stood at the hand and looked along the throw, no muzzle offset.
        {std::lock_guard<std::mutex> lock(controllerUiFixture::fishingAimMutex);auto& b=controllerUiFixture::bowAim;
         b.until=now+600;++b.serial;std::memcpy(b.camera.data(),&w.aim.origin,24);std::memcpy(b.camera.data()+24,&w.aim.orientation,16);b.aim=controllerUiFixture::aimBytes(w.aim);}
        std::ostringstream s;s<<std::setprecision(10)<<"\"serial\":"<<r.serial<<",\"frame\":"<<liveFrame.frame<<",\"request_frame\":"<<r.frame<<",\"age_ms\":"<<now-r.milliseconds
            <<",\"origin_q32\":["<<position.x<<','<<position.y<<','<<position.z<<"],\"direction\":["<<direction.x<<','<<direction.y<<','<<direction.z<<"]";
        logFile->event("live_throw_world",s.str());
    }catch(const std::exception& e){logFile->event("live_throw_failure","\"serial\":"+std::to_string(r.serial)+",\"reason\":"+jsonString(e.what()));}
}
}
