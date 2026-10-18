#pragma once
#include <evr/math.hpp>
#include <vector>
namespace evr {
// The native engine has perspective sphere/HZB paths that assume a centered projection.
// Keep every runtime ray while supplying the engine with a symmetric frustum. The exact
// render FOV must also be submitted to OpenXR; never present these pixels as the old FOV.
// OpenXR XrCompositionLayerProjectionView permits a different FOV and maps it to the display.
inline XrFovf symmetricEnvelope(XrFovf runtime) {
    projection(runtime,.01f,10000.f); // validate finite ordered angles and clip/ray contract
    const float x=std::max(std::abs(runtime.angleLeft),std::abs(runtime.angleRight));
    const float y=std::max(std::abs(runtime.angleUp),std::abs(runtime.angleDown));
    return {-x,x,y,-y};
}
// The symmetric frustum turned toward the middle of the runtime's (2026-10-19, user: "trim the parts of the picture
// that are not needed, as the EDF6 mod does"). The envelope above keeps the eye's direction and widens each narrow
// side to the wide one: for the user's headset (left eye 54.9 / 42.1 degrees left / right, 44.1 / 53.0 up / down)
// that is 1.41 times the area the lens shows. Turned toward the middle of its field (that left eye 6.4 degrees left
// and 4.5 down), the smallest symmetric frustum holding every runtime ray is about as large as the runtime's own:
// the same pixels per degree for about 30 % fewer pixels. The projection stays centred, as the engine's HZB paths
// need; the turned eye pose goes to the game's cameras and to xrEndFrame together (OpenXR maps any pose and fov).
// Holding the runtime rectangle's corners, the turned frustum saves about 17 % only (at 54 degrees the turn pushes the far
// corner out). The lens does not show those corners: given the visible area (XR_KHR_visibility_mask, tangents on z = -1
// of the runtime eye; a polygon, whose extremes seen from the turned eye are among its points as lines stay lines),
// the frustum holds those points widened by `marginRadians` (the compositor's reprojection at the edges) instead -
// never more than the corners' frustum.
struct CantedFov {Quat turn{0,0,0,1};XrFovf fov{};};
inline CantedFov cantedEnvelope(XrFovf runtime,const std::vector<XrVector2f>* visible=nullptr,float marginRadians=0.f) {
    projection(runtime,.01f,10000.f);
    // euler(): about +y, turning -z toward +x is negative; about +x, turning -z toward +y is positive
    const Quat turn=normalized(euler(-(runtime.angleLeft+runtime.angleRight)*.5f,(runtime.angleUp+runtime.angleDown)*.5f,0.f));
    const Quat back=inverse(turn);
    // The runtime's rectangle seen from the turned eye is a quadrilateral (lines stay lines): its corners bound it.
    double x=0,y=0;
    for(float a:{runtime.angleLeft,runtime.angleRight})for(float b:{runtime.angleDown,runtime.angleUp}){
        const auto d=rotate(back,Vec3{std::tan(a),std::tan(b),-1.f});
        if(!(d.z<-1e-3f))throw std::invalid_argument("runtime fov corner behind the turned eye");
        x=std::max(x,std::abs(double(d.x)/-double(d.z)));y=std::max(y,std::abs(double(d.y)/-double(d.z)));
    }
    if(visible&&visible->size()>=3){
        double vx=0,vy=0;
        for(const auto& p:*visible){
            const auto d=rotate(back,Vec3{p.x,p.y,-1.f});
            if(!(d.z<-1e-3f)||!std::isfinite(p.x)||!std::isfinite(p.y))throw std::invalid_argument("visible area point behind the turned eye");
            vx=std::max(vx,std::abs(double(d.x)/-double(d.z)));vy=std::max(vy,std::abs(double(d.y)/-double(d.z)));
        }
        const double m=std::clamp(double(marginRadians),0.0,.2);
        vx=std::tan(std::min(std::atan(vx)+m,1.5));vy=std::tan(std::min(std::atan(vy)+m,1.5));
        x=std::min(x,vx);y=std::min(y,vy);
    }
    CantedFov result;result.turn=turn;
    const float ax=std::nextafter(float(std::atan(x)),10.f),ay=std::nextafter(float(std::atan(y)),10.f);
    result.fov={-ax,ax,ay,-ay};
    projection(result.fov,.01f,10000.f);
    return result;
}
}
