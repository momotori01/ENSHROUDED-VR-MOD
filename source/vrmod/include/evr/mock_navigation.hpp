#pragma once
#include <cmath>
#include <cstdint>
#include <locale>
#include <sstream>
#include <string>

// Test-only input. Timeout uses the monotonic clock and includes delivery delay.
namespace evr::mock {
struct NavigationCommand {
    int64_t sequence=0, issuedMs=0;
    int holdMs=0;
    float yaw=0, pitch=0, leftX=0, leftY=0, rightX=0;
    int jump=0;
};
inline bool parseNavigation(const std::string& text, NavigationCommand& out) {
    if(text.size()>512) return false;
    std::istringstream in(text); in.imbue(std::locale::classic());
    std::string magic,extra; NavigationCommand c;
    if(!(in>>magic>>c.sequence>>c.issuedMs>>c.holdMs>>c.yaw>>c.pitch>>c.leftX>>c.leftY>>c.rightX>>c.jump)||
       magic!="EVR_MOCK_NAV_1"||(in>>extra)) return false;
    if(c.sequence<=0||c.issuedMs<=0||c.holdMs<0||c.holdMs>2000||c.jump<0||c.jump>1) return false;
    for(float v:{c.yaw,c.pitch,c.leftX,c.leftY,c.rightX}) if(!std::isfinite(v)) return false;
    if(std::abs(c.yaw)>360||std::abs(c.pitch)>85||std::abs(c.leftX)>1||std::abs(c.leftY)>1||std::abs(c.rightX)>1) return false;
    out=c; return true;
}
struct NavigationState {
    NavigationCommand command{};
    int64_t acceptedSteadyMs=0;
    int remainingMs=0;
    bool valid=false;
    bool update(const std::string& text,int64_t wallMs,int64_t steadyMs) {
        NavigationCommand next;
        if(!parseNavigation(text,next)) {valid=false;return false;}
        if(next.sequence<=command.sequence) return false;
        if(next.issuedMs>wallMs||wallMs-next.issuedMs>2000) {valid=false;return false;}
        command=next; acceptedSteadyMs=steadyMs;
        remainingMs=next.holdMs-int(wallMs-next.issuedMs); valid=true;
        return true;
    }
    bool active(int64_t steadyMs) const {
        return valid&&remainingMs>0&&steadyMs>=acceptedSteadyMs&&steadyMs-acceptedSteadyMs<remainingMs;
    }
};
}
