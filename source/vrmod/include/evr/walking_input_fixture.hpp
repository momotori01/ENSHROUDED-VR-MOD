#pragma once
#include <evr/idle_head_camera.hpp>

namespace evr::game {
// Opt-in TEST ONLY. Small, one-shot controller packets for native prediction /
// locomotion testing without a physical gamepad. Never enabled by VR itself.
// Travel bounds constrain the automated test, not the HMD tracking pose.
enum class WalkFixtureState {Waiting,Running,Complete,Aborted};
struct WalkFixtureCommand {
    WalkFixtureState state=WalkFixtureState::Waiting;
    bool synthetic=false;
    unsigned phase=0;
    uint64_t elapsedMs=0;
    std::array<float,2> stick{};
    const char* reason="waiting";
};
inline bool nativeControlsIdle(const ClientInputBytes& source){
    std::array<float,2> stick;uint64_t digital;
    std::memcpy(stick.data(),source.data()+clientStick,sizeof(stick));std::memcpy(&digital,source.data()+clientDigital,sizeof(digital));
    return stick[0]==0&&stick[1]==0&&!digital&&source[clientInputMode]<=1;
}
class WalkingInputFixture {
    WalkFixtureState state=WalkFixtureState::Waiting;
    uint64_t firstMs=0,startMs=0,lastMs=0,epoch=0,session=0;
    unsigned ready=0;
    WorldPositionQ32 initialRoot{};
    const char* reason="waiting";
public:
    static constexpr uint64_t waitLimitMs=15000,settleMs=1000,phaseMs=850,pulseMs=250;
    static constexpr uint64_t runMs=settleMs+phaseMs*4+1000;
    static constexpr double travelLimitM=.75;
    WalkFixtureCommand update(uint64_t now,uint64_t originEpoch,WorldPositionQ32 root,double worldUnitsPerMeter,
                              bool gateReady,bool userIdle,uint64_t generation=1){
        WalkFixtureCommand command;
        auto abort=[&](const char* why){state=WalkFixtureState::Aborted;reason=why;};
        if(state==WalkFixtureState::Complete||state==WalkFixtureState::Aborted){command.state=state;command.reason=reason;return command;}
        if(!firstMs){firstMs=now;lastMs=now;}
        if(!now||now<lastMs)abort("clock");
        else if(!userIdle)abort("native_control_active");
        else if(!std::isfinite(worldUnitsPerMeter)||worldUnitsPerMeter<=0)abort("world_scale");
        else if(state==WalkFixtureState::Waiting){
            if(now-firstMs>=waitLimitMs)abort("ready_timeout");
            else if(gateReady&&originEpoch&&generation){
                if(++ready>=16){state=WalkFixtureState::Running;startMs=now;initialRoot=root;epoch=originEpoch;session=generation;reason="running";}
            }else ready=0;
        }else{
            if(!gateReady)abort("gameplay_or_tracking_guard");
            else if(originEpoch!=epoch||generation!=session)abort("origin_or_session_changed");
            else{
                const double dx=(double(root.x)-double(initialRoot.x))/4294967296./worldUnitsPerMeter;
                const double dy=(double(root.y)-double(initialRoot.y))/4294967296./worldUnitsPerMeter;
                const double dz=(double(root.z)-double(initialRoot.z))/4294967296./worldUnitsPerMeter;
                if(dx*dx+dy*dy+dz*dz>travelLimitM*travelLimitM)abort("travel_limit");
                else if(now-startMs>=runMs){state=WalkFixtureState::Complete;reason="complete";}
            }
        }
        lastMs=now;command.state=state;command.reason=reason;
        if(state!=WalkFixtureState::Running)return command;
        command.synthetic=true;command.elapsedMs=now-startMs;
        if(command.elapsedMs>=settleMs&&command.elapsedMs<settleMs+4*phaseMs){
            const auto step=(command.elapsedMs-settleMs)/phaseMs;
            command.phase=unsigned(step+1);
            if((command.elapsedMs-settleMs)%phaseMs<pulseMs){
                constexpr std::array<std::array<float,2>,4> controls{{{{0,.25f}},{{0,-.25f}},{{.25f,0}},{{-.25f,0}}}};
                command.stick=controls[step];
            }
        }
        return command;
    }
};
inline void writeFixtureControls(ClientInputBytes& data,const WalkFixtureCommand& command){
    if(!command.synthetic)return;
    std::memcpy(data.data()+clientStick,command.stick.data(),sizeof(command.stick));data[clientInputMode]=1;
}
inline bool onlyFixtureAndCameraChanged(const ClientInputBytes& a,const ClientInputBytes& b){
    for(size_t i=0;i<a.size();++i)if((i<0x20||i>=0x4c)&&(i<clientStick||i>=clientStick+8)&&i!=clientInputMode&&a[i]!=b[i])return false;
    return true;
}
} // namespace evr::game
