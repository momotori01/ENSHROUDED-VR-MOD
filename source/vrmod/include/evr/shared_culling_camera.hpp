#pragma once
#include <evr/game_stereo.hpp>
#include <limits>

namespace evr::game {
// Native f7c50 can return its exact empty result before camera preparation
// while renderer resources are being rebuilt. Partial rendering is not a wait.
enum class SharedRenderBoundary { Rendered, Deferred, Invalid };
inline SharedRenderBoundary sharedRenderBoundary(bool sourceUnchanged,bool nativeEmpty,
    unsigned projections,unsigned frustums,unsigned mask,bool frustumFailed){
    if(!sourceUnchanged||frustumFailed)return SharedRenderBoundary::Invalid;
    if(projections==1&&frustums==2&&mask==3)return SharedRenderBoundary::Rendered;
    if(nativeEmpty&&projections==0&&frustums==0&&mask==0)return SharedRenderBoundary::Deferred;
    return SharedRenderBoundary::Invalid;
}
// A helper view for shared simulation/culling. Never submit this as an XR eye.
// Its retreat is derived from Runtime eye offsets; tracking poses stay intact.
struct SharedCullingCamera {
    XrView view{XR_TYPE_VIEW};
    float nearZ=0,farZ=0,retreat=0;
};
inline SharedCullingCamera sharedCullingCamera(Pose head,const std::array<XrView,2>& eyes,float nearZ,float farZ){
    head.orientation=normalized(head.orientation);
    if(!finite(head.position)||!std::isfinite(nearZ)||!std::isfinite(farZ)||nearZ<=0||farZ<=nearZ)
        throw std::invalid_argument("invalid shared culling camera pose/clip range");
    std::array<Vec3,16> corners{};size_t next=0;
    float retreat=0,minNearDepth=std::numeric_limits<float>::infinity();
    for(const auto& eye:eyes){
        projection(eye.fov,nearZ,farZ);
        if(!finite(eye.pose.position))throw std::invalid_argument("invalid culling eye position");
        const auto local=compose(inverse(head),eye.pose);
        retreat=std::max(retreat,length(local.position));
        for(float depth:{nearZ,farZ})for(float x:{std::tan(eye.fov.angleLeft),std::tan(eye.fov.angleRight)})
            for(float y:{std::tan(eye.fov.angleDown),std::tan(eye.fov.angleUp)}){
                const auto point=local.position+rotate(local.orientation,{x*depth,y*depth,-depth});
                if(!finite(point))throw std::invalid_argument("culling frustum overflow");
                corners[next++]=point;
                if(depth==nearZ)minNearDepth=std::min(minNearDepth,-point.z);
                // A single forward perspective camera cannot represent a view
                // spanning behind the HMD. Refuse instead of moving it far away.
                if(depth==farZ&&-point.z<=nearZ)throw std::invalid_argument("stereo frusta exceed one forward culling camera");
            }
    }
    retreat=std::max(retreat,nearZ-minNearDepth);
    float left=std::numeric_limits<float>::infinity(),right=-left,down=left,up=-left,maxDepth=nearZ;
    for(const auto& corner:corners){
        const float depth=retreat-corner.z;
        if(!std::isfinite(depth)||depth<=0)throw std::invalid_argument("invalid enclosing culling depth");
        left=std::min(left,corner.x/depth);right=std::max(right,corner.x/depth);
        down=std::min(down,corner.y/depth);up=std::max(up,corner.y/depth);maxDepth=std::max(maxDepth,depth);
    }
    SharedCullingCamera result;
    result.retreat=retreat;
    result.nearZ=nearZ;
    result.farZ=std::nextafter(maxDepth,std::numeric_limits<float>::infinity());
    result.view.pose=compose(head,Pose{{0,0,0,1},{0,0,retreat}});
    // Outward rounding keeps the mathematical hull conservative at its edges.
    result.view.fov={std::nextafter(std::atan(left),-std::numeric_limits<float>::infinity()),
                     std::nextafter(std::atan(right),std::numeric_limits<float>::infinity()),
                     std::nextafter(std::atan(up),std::numeric_limits<float>::infinity()),
                     std::nextafter(std::atan(down),-std::numeric_limits<float>::infinity())};
    projection(result.view.fov,result.nearZ,result.farZ);
    return result;
}
}
