#include <evr/stable_frustum.hpp>
#include <iostream>
using namespace evr;
int main(){
    unsigned passed=0,failed=0;
    auto check=[&](bool yes,const char* name){(yes?passed:failed)++;std::cout<<(yes?"PASS ":"FAIL ")<<name<<'\n';};
    const XrFovf fov{-.88f,.93f,.8f,-.85f};
    const auto descriptor=game::projectionDescriptor(fov,.01f,10000.f);
    for(Vec3 position:std::array<Vec3,3>{{{0,0,0},{3740.128f,845.261f,1428.298f},{1e6f,-1e6f,1e6f}}})
        for(float yaw:{0.f,pi/2,pi,-pi/2})for(float pitch:{0.f,1.4f,-1.4f}){
        const auto orientation=euler(yaw,pitch,.25f);
        const auto q32=cameraPosition({},position,1);
        const auto matrix=game::cameraRelativeView(q32,orientation,{});
        const auto planes=game::stableFrustum(matrix,descriptor);
        bool inside=true,normal=true;
        for(float depth:{.01f,1.f,10000.f})for(float x:{std::tan(fov.angleLeft),std::tan(fov.angleRight)})
            for(float y:{std::tan(fov.angleDown),std::tan(fov.angleUp)}){
            const auto point=position+rotate(orientation,{x*depth,y*depth,depth});
            for(const auto& plane:planes){
                const float distance=plane[0]*point.x+plane[1]*point.y+plane[2]*point.z+plane[3];
                inside&=distance<=.002f; // independent float quaternion and view calculations
            }
        }
        for(const auto& plane:planes)normal&=std::abs(length({plane[0],plane[1],plane[2]})-1)<2e-7;
        check(inside,"wide pose and world origin corners conservatively inside");
        check(normal,"outward plane normals remain unit length");
        const auto center=position+rotate(orientation,{0,0,1});
        const auto beyond=position+rotate(orientation,{0,0,20000});
        check(planes[5][0]*beyond.x+planes[5][1]*beyond.y+planes[5][2]*beyond.z+planes[5][3]>1000&&
              planes[0][0]*center.x+planes[0][1]*center.y+planes[0][2]*center.z+planes[0][3]<0,
              "far exterior rejected and forward interior retained");
    }
    // Captured world matrix previously produced a 7.70m error at one far corner.
    game::MatrixRows recorded{{.999894202f,-.0145440698f,-3.86498868e-8f,0,.00318916468f,.219255209f,-.97566247f,0,
        .0141901094f,.975559235f,.219278395f,0,-3762.6958f,-1524.32056f,511.49472f,1}};
    auto d=game::projectionDescriptor({-.879999757f,.879999757f,.799998462f,-.849998474f},.01f,10000.0332f);
    const auto planes=game::stableFrustum(recorded,d);
    // Analytic bottom normal should be parallel to -viewY + tan(down)*viewZ.
    auto expected=Vec3{-recorded.values[1]+std::tan(-.849998474f)*recorded.values[2],
        -recorded.values[5]+std::tan(-.849998474f)*recorded.values[6],
        -recorded.values[9]+std::tan(-.849998474f)*recorded.values[10]};
    expected=expected*(1/length(expected));
    check(length(Vec3{planes[2][0],planes[2][1],planes[2][2]}-expected)<2e-7f,"captured plane orientation avoids inverse cancellation");
    auto bad=d;bad.bytes[0]=0;
    try{game::stableFrustum(recorded,bad);check(false,"unsupported descriptor refused");}catch(const std::invalid_argument&){check(true,"unsupported descriptor refused");}
    recorded.values[0]=2;
    try{game::stableFrustum(recorded,d);check(false,"nonrigid matrix refused");}catch(const std::invalid_argument&){check(true,"nonrigid matrix refused");}
    std::cout<<"PASS "<<passed<<" FAIL "<<failed<<'\n';return failed?1:0;
}
