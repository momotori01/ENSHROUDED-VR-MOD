#include <evr/shared_culling_camera.hpp>
#include <iostream>
using namespace evr;
int main(){
    unsigned passed=0,failed=0;
    auto check=[&](bool yes,const char* name){(yes?passed:failed)++;std::cout<<(yes?"PASS ":"FAIL ")<<name<<'\n';};
    using Boundary=game::SharedRenderBoundary;
    check(game::sharedRenderBoundary(true,true,0,0,0,false)==Boundary::Deferred,"exact native empty preparation return defers");
    check(game::sharedRenderBoundary(true,false,1,2,3,false)==Boundary::Rendered,"complete native camera remains required");
    check(game::sharedRenderBoundary(true,true,1,2,3,false)==Boundary::Rendered,"later empty result still validates completed camera");
    check(game::sharedRenderBoundary(false,true,0,0,0,false)==Boundary::Invalid,"source mutation must never be a preparation wait");
    check(game::sharedRenderBoundary(true,true,0,0,0,true)==Boundary::Invalid,"failed native frustum must never be deferred");
    check(game::sharedRenderBoundary(true,false,0,0,0,false)==Boundary::Invalid,"missing projection with output is rejected");
    for(auto state:std::array<std::array<unsigned,3>,5>{{{{1,0,0}},{{0,1,1}},{{0,0,1}},{{2,2,3}},{{1,2,1}}}})
        check(game::sharedRenderBoundary(true,true,state[0],state[1],state[2],false)==Boundary::Invalid,"partial or duplicate camera work rejected");
    for(float yaw:{0.f,pi/2,pi,-pi/2})for(float pitch:{0.f,-1.4f,1.4f}){
        Pose head{euler(yaw,pitch,.3f),{.2f,1.72f,-.4f}};
        std::array<XrView,2> eyes{{{XR_TYPE_VIEW},{XR_TYPE_VIEW}}};
        for(unsigned eye=0;eye<2;eye++){
            eyes[eye].pose=compose(head,Pose{euler(eye?-.08f:.1f,.015f,0),{eye?.035f:-.031f,.002f,eye?.003f:-.004f}});
            eyes[eye].fov={eye?-.72f:-.91f,eye?.94f:.76f,.87f,-.78f};
        }
        const auto before=eyes;const auto c=game::sharedCullingCamera(head,eyes,.01f,10000.f);
        bool contained=true;
        for(const auto& eye:eyes)for(unsigned iz=0;iz<=8;iz++)for(unsigned ix=0;ix<=8;ix++)for(unsigned iy=0;iy<=8;iy++){
            const float d=.01f*std::pow(1000000.f,float(iz)/8);
            const float x=std::tan(eye.fov.angleLeft)+(std::tan(eye.fov.angleRight)-std::tan(eye.fov.angleLeft))*ix/8;
            const float y=std::tan(eye.fov.angleDown)+(std::tan(eye.fov.angleUp)-std::tan(eye.fov.angleDown))*iy/8;
            const auto world=eye.pose.position+rotate(eye.pose.orientation,{d*x,d*y,-d});
            const auto local=rotate(inverse(c.view.pose.orientation),world-c.view.pose.position);
            const float depth=-local.z;
            contained &= depth>=c.nearZ-2e-5f&&depth<=c.farZ*1.00001f&&
                local.x/depth>=std::tan(c.view.fov.angleLeft)-2e-5f&&local.x/depth<=std::tan(c.view.fov.angleRight)+2e-5f&&
                local.y/depth>=std::tan(c.view.fov.angleDown)-2e-5f&&local.y/depth<=std::tan(c.view.fov.angleUp)+2e-5f;
        }
        check(contained,"asymmetric canted stereo volume enclosed at wide head rotation");
        check(std::memcmp(eyes.data(),before.data(),sizeof(eyes))==0&&c.retreat>0&&c.retreat<.1f,"runtime eyes unchanged; helper retreat derived from eye offsets");
    }
    std::array<XrView,2> eyes{{{XR_TYPE_VIEW},{XR_TYPE_VIEW}}};
    for(auto& eye:eyes){eye.pose=identityPose();eye.fov={-.8f,.8f,.8f,-.8f};}
    eyes[1].pose.orientation=euler(pi,0,0);
    try{game::sharedCullingCamera(identityPose(),eyes,.01f,1000.f);check(false,"opposed frusta refused");}
    catch(const std::invalid_argument&){check(true,"opposed frusta refused");}
    eyes[1].pose=identityPose();eyes[1].fov.angleLeft=std::numeric_limits<float>::quiet_NaN();
    try{game::sharedCullingCamera(identityPose(),eyes,.01f,1000.f);check(false,"invalid runtime FOV refused");}
    catch(const std::invalid_argument&){check(true,"invalid runtime FOV refused");}
    std::cout<<"PASS "<<passed<<" FAIL "<<failed<<'\n';return failed?1:0;
}
