#pragma once
// Offline diagnostic input only. Does not represent a live XR session or submission.
#include <evr/game_stereo.hpp>
#include <istream>
#include <vector>

namespace evr::diagnostic {
struct RecordedXrFrame {
    uint64_t frame=0;
    XrTime time=0;
    std::array<XrView,2> views{{{XR_TYPE_VIEW},{XR_TYPE_VIEW}}};
};
struct RecordedXr {
    float renderScale=1;
    std::array<uint32_t,2> recommendedWidth{},recommendedHeight{},width{},height{};
    Pose trackingToAnchor=identityPose();
    std::vector<RecordedXrFrame> frames;
    static RecordedXr read(std::istream& input) {
        RecordedXr result;std::string magic;uint32_t count=0;
        input>>magic>>count>>result.renderScale;
        if(!input||magic!="EVR_RECORDED_XR_1"||!count||count>1000||!std::isfinite(result.renderScale)||result.renderScale<=0)
            throw std::invalid_argument("invalid recorded XR header");
        for(size_t eye=0;eye<2;eye++){
            input>>result.recommendedWidth[eye]>>result.recommendedHeight[eye];
            const double w=std::ceil(double(result.recommendedWidth[eye])*result.renderScale);
            const double h=std::ceil(double(result.recommendedHeight[eye])*result.renderScale);
            // Diagnostic allocation budget, not HMD-specific resolution constants.
            if(!input||w<1||h<1||w>8192||h>8192)throw std::invalid_argument("recorded XR extent outside diagnostic budget");
            result.width[eye]=uint32_t(w);result.height[eye]=uint32_t(h);
        }
        auto pose=[&](Pose& p){input>>p.position.x>>p.position.y>>p.position.z>>p.orientation.x>>p.orientation.y>>p.orientation.z>>p.orientation.w;
            if(!input||!finite(p.position))throw std::invalid_argument("invalid recorded XR pose");
            p.orientation=normalized(p.orientation);};
        pose(result.trackingToAnchor);
        for(uint32_t i=0;i<count;i++){
            RecordedXrFrame frame;input>>frame.frame>>frame.time;
            if(!input||!frame.frame||frame.time<=0||(i&&(frame.frame<=result.frames.back().frame||frame.time<=result.frames.back().time)))
                throw std::invalid_argument("recorded XR frame order");
            for(auto& view:frame.views){pose(view.pose);input>>view.fov.angleLeft>>view.fov.angleRight>>view.fov.angleUp>>view.fov.angleDown;
                if(!input)throw std::invalid_argument("truncated recorded XR FOV");
                game::projectionDescriptor(view.fov,.01f,1000);}
            result.frames.push_back(frame);
        }
        input>>std::ws;if(!input.eof())throw std::invalid_argument("trailing recorded XR data");
        return result;
    }
};
}
