#include <evr/stable_frustum.hpp>
#include <evr/vfx_culling_union.hpp>
#include <iostream>
#include <vector>
using namespace evr;
namespace {
Vec3 world(WorldPositionQ32 p){return {float(relativeQ32(p.x,0)),float(relativeQ32(p.y,0)),float(relativeQ32(p.z,0))};}
// Largest signed plane distance of the points (<= 0 means inside every plane).
float worst(const game::FrustumPlanes& planes,const std::vector<Vec3>& points){
    float result=-1e30f;
    for(const auto& p:points)for(const auto& q:planes)result=std::max(result,q[0]*p.x+q[1]*p.y+q[2]*p.z+q[3]);
    return result;
}
}
int main(){
    unsigned passed=0,failed=0;
    auto check=[&](bool yes,const char* name){(yes?passed:failed)++;std::cout<<(yes?"PASS ":"FAIL ")<<name<<'\n';};
    const float nearZ=.01f,farZ=10000.f,minDistance=.5f;
    for(float yaw:{0.f,1.1f,-2.3f})for(float cant:{0.f,.08f}){
        // Head at 1.6 m, eyes 64 mm apart; optional outward cant per eye (canted-display headsets).
        const Pose head{euler(yaw,.2f,0),{.3f,1.6f,-.4f}};
        std::array<XrView,2> views{{{XR_TYPE_VIEW},{XR_TYPE_VIEW}}};
        views[0].pose=compose(head,Pose{euler(cant,0,0),{-.032f,0,0}});
        views[1].pose=compose(head,Pose{euler(-cant,0,0),{.032f,0,0}});
        views[0].fov={-.88f,.82f,.8f,-.85f};views[1].fov={-.82f,.88f,.8f,-.85f};
        game::StableAnchor anchor;anchor.root={int64_t(3740)<<32,int64_t(845)<<32,int64_t(1428)<<32};
        const auto cams=game::stereoCameras(anchor,identityPose(),views,3,1,1,anchor.root,nearZ,farZ);
        const auto bounds=game::unionTangents(views[0],views[1],minDistance,farZ*.5f);
        const auto wide=game::widenedDescriptor(cams.eyes[0].descriptor,bounds);
        // Rebuild the view relative to the world origin so plane distances use the same space as the points.
        const auto viewAtOrigin=game::cameraRelativeView(cams.eyes[0].position,cams.eyes[0].orientation,{});
        const auto native=game::stableFrustum(viewAtOrigin,cams.eyes[0].descriptor);
        const auto widened=game::stableFrustum(viewAtOrigin,wide);
        // Other-eye frustum points (game camera space: +z forward) from the parallax distance outward.
        std::vector<Vec3> other,owner;
        const auto o=cams.eyes[1],s=cams.eyes[0];
        for(float depth:{minDistance,1.f,5.f,100.f,4000.f})
            for(float x:{std::tan(o.fov.angleLeft),std::tan(o.fov.angleRight)})
                for(float y:{std::tan(o.fov.angleDown),std::tan(o.fov.angleUp)}){
                    other.push_back(world(o.position)+rotate(o.orientation,{x*depth,y*depth,depth}));
                    const float sx=x==std::tan(o.fov.angleLeft)?std::tan(s.fov.angleLeft):std::tan(s.fov.angleRight);
                    owner.push_back(world(s.position)+rotate(s.orientation,{sx*depth,y*depth,depth}));
                }
        // Tolerance: float planes at 3.7 km world coordinates.
        check(worst(widened,other)<=.05f,"widened owner planes contain the other eye beyond the parallax distance");
        check(worst(widened,owner)<=.05f,"widened owner planes still contain the owner frustum");
        check(worst(native,other)>.01f,"native owner planes miss part of the other eye (the case being fixed)");
        check(bounds.right>std::tan(views[1].fov.angleRight)&&bounds.left<=std::tan(views[0].fov.angleLeft),"union is at least the outer edge of each eye");
        std::array<float,6> before{},after{};std::memcpy(before.data(),cams.eyes[0].descriptor.bytes.data()+4,24);std::memcpy(after.data(),wide.bytes.data()+4,24);
        check(after[4]==before[4]&&after[5]==before[5]&&wide.bytes[0x48]==1,"near/far and reverse depth unchanged");
    }
    // Rejections keep the native planes.
    std::array<XrView,2> bad{{{XR_TYPE_VIEW},{XR_TYPE_VIEW}}};
    bad[0].pose=identityPose();bad[1].pose=Pose{euler(3.14159f,0,0),{0,0,0}};
    bad[0].fov=bad[1].fov={-.8f,.8f,.8f,-.8f};
    bool rejected=false;try{game::unionTangents(bad[0],bad[1],.5f,100.f);}catch(const std::invalid_argument&){rejected=true;}
    check(rejected,"an eye facing away is rejected");
    game::ProjectionDescriptor unsupported{};
    rejected=false;try{game::widenedDescriptor(unsupported,{-1,1,-1,1});}catch(const std::invalid_argument&){rejected=true;}
    check(rejected,"a non tag-3 descriptor is rejected");
    std::cout<<passed<<" passed, "<<failed<<" failed\n";
    return failed?1:0;
}
