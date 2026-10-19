#include <evr/game_input.hpp>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
using namespace evr;
int passed=0,failed=0;
void check(bool ok,const char* name){std::cout<<(ok?"PASS ":"FAIL ")<<name<<'\n';ok?++passed:++failed;}
bool near(Vec3 a,Vec3 b,float tolerance=2e-5f){return finite(a)&&finite(b)&&length(a-b)<tolerance;}
int main(int argc,char** argv){
    game::NativeInputState state;state.enabled=state.known=state.local=state.ready=true;
    state.inputMode=1;state.playerFlags=129;state.stick={0,1};
    game::HeadInputSnapshot h;h.flags=15;h.current=true;h.xrFrame=123;h.originEpoch=2;h.predictedDisplayTime=123000000;
    h.anchor.root={int64_t(1000000000)*4294967296ll+37,0,0};h.anchor.cameraOffsetGameUnits={0,1.6f,0};
    auto p=game::proposeInput(state,h,0);
    check(p.active()&&near(p.worldMovement,{0,0,1}),"runtime head -> game +Z movement");
    check(p.xrFrame==123&&p.originEpoch==2&&p.predictedDisplayTime==123000000,"proposal retains frame/time/origin identity");
    bool angles=true,unit=true,roll=true;
    for(float yaw:{-pi,-pi/2,0.f,pi/2,pi})for(float pitch:{-1.5f,-.7f,0.f,.7f,1.5f})for(float bank:{-2.f,0.f,2.f}){
        h.head.orientation=euler(yaw,pitch,bank);p=game::proposeInput(state,h,0);
        const auto wanted=game::trackingVector({-std::sin(yaw),0,-std::cos(yaw)});
        angles&=p.active()&&near(p.worldMovement,wanted);
        unit&=std::abs(length(p.worldMovement)-1)<2e-6f&&std::abs(p.worldMovement.y)<1e-6f;
        roll&=near(rotate(p.aimWithoutRoll,{0,0,1}),p.aimForward)&&near(rotate(p.aimWithoutRoll,{1,0,0}),rotate(p.movementOrientation,{1,0,0}));
    }
    check(angles&&unit,"75 yaw/pitch/roll cases keep horizontal direction and stick magnitude");
    check(roll,"roll-free aim retains HMD forward independently of movement");
    h.head.orientation=euler(.8f,pi/2,1);p=game::proposeInput(state,h,-.8f);
    check(p.active()&&std::abs(p.yaw+.8f)<1e-6f&&near(p.aimForward,{0,1,0}),"vertical up retains last yaw without losing head aim");
    h.head.orientation=euler(.8f,-pi/2,-1);p=game::proposeInput(state,h,-.8f);
    check(p.active()&&std::abs(p.yaw+.8f)<1e-6f&&near(p.aimForward,{0,-1,0}),"vertical down retains last yaw without losing head aim");
    h.head=identityPose();const auto root=h.anchor.root;
    const auto a=game::proposeInput(state,h,0);h.head.position={.003f,.01f,-.02f};p=game::proposeInput(state,h,0);
    check(std::abs(relativeQ32(p.cameraPosition.x,a.cameraPosition.x)-.003)<1e-8&&
          std::abs(relativeQ32(p.cameraPosition.y,a.cameraPosition.y)-.01)<1e-8&&
          std::abs(relativeQ32(p.cameraPosition.z,a.cameraPosition.z)-.02)<1e-8,"physical translation retains millimeters at large world coordinates");
    check(h.anchor.root.x==root.x&&h.anchor.root.y==root.y&&h.anchor.root.z==root.z&&near(p.worldMovement,a.worldMovement),"HMD translation does not alter root or move command");
    h.head={euler(.7f,.4f,-.3f),{3,4,5}};TrackingOrigin recentered;recentered.recenter(h.head);h.trackingToAnchor=recentered.trackingToAnchor;
    p=game::proposeInput(state,h,0);
    check(std::abs(relativeQ32(p.cameraPosition.x,root.x))<1e-6&&std::abs(relativeQ32(p.cameraPosition.y,root.y)-1.6)<1e-6&&near(p.worldMovement,{0,0,1}),"recenter moves tracking origin and keeps root untouched");
    bool guards=true;
    auto reject=[&](game::NativeInputState s,game::HeadInputSnapshot t,game::InputFallback why){
        auto r=game::proposeInput(s,t,0);guards&=!r.active()&&r.fallback==why&&near(r.worldMovement,{0,0,0});};
    auto s=state;s.enabled=false;reject(s,h,game::InputFallback::Disabled);
    s=state;s.known=false;reject(s,h,game::InputFallback::UnknownState);
    s=state;s.local=false;reject(s,h,game::InputFallback::NotLocal);
    s=state;s.ready=false;reject(s,h,game::InputFallback::NotReady);
    s=state;s.playerFlags&=~1u;reject(s,h,game::InputFallback::NotReady);
    for(uint8_t mode:{uint8_t(0),uint8_t(2),uint8_t(255)}){s=state;s.inputMode=mode;reject(s,h,game::InputFallback::NotGamepad);}
    for(unsigned bit:{1u,2u}){s=state;s.playerFlags|=1u<<bit;reject(s,h,game::InputFallback::Ui);}
    for(unsigned bit=0;bit<8;++bit){s=state;s.clientMask=uint8_t(1u<<bit);reject(s,h,game::InputFallback::BuildingOrCursor);}
    s=state;s.buildingCamera=true;reject(s,h,game::InputFallback::BuildingOrCursor);
    s=state;s.specialCursor=true;reject(s,h,game::InputFallback::BuildingOrCursor);
    s=state;s.playerFlags|=game::targetLockFlag;check(game::inputGate(s,h)==game::InputFallback::None,"a lock keeps the headset (lock-on movement removed)");
    s=state;s.playerFlags|=1u<<12;check(game::inputGate(s,h)==game::InputFallback::None,"1.0 WantsToWalk (bit 12) is no lock");
    for(unsigned flags=0;flags<15;++flags){auto t=h;t.flags=flags;reject(state,t,game::InputFallback::TrackingUnavailable);}
    auto t=h;t.current=false;reject(state,t,game::InputFallback::TrackingUnavailable);
    t=h;t.xrFrame=0;reject(state,t,game::InputFallback::TrackingUnavailable);
    t=h;t.originEpoch=0;reject(state,t,game::InputFallback::TrackingUnavailable);
    t=h;t.predictedDisplayTime=0;reject(state,t,game::InputFallback::TrackingUnavailable);
    check(guards,"40 UI/ownership/mode/tracking failure cases preserve native input");
    s=state;s.stick[0]=std::numeric_limits<float>::quiet_NaN();reject(s,h,game::InputFallback::InvalidInput);
    s=state;s.stick[1]=1.01f;reject(s,h,game::InputFallback::InvalidInput);
    t=h;t.head.orientation={0,0,0,0};reject(state,t,game::InputFallback::InvalidInput);
    t=h;t.head.position.x=std::numeric_limits<float>::infinity();reject(state,t,game::InputFallback::InvalidInput);
    check(guards,"malformed axes/pose rejected with no partial output");
    // Three minutes at 90 Hz. Independent trig oracle, gradual origin/root motion.
    bool sequence=true;h.trackingToAnchor=identityPose();h.anchor.orientation=euler(.3f,0,0);float previous=.3f;
    for(unsigned i=0;i<16200;++i){
        const float time=float(i)/90,yaw=std::sin(time*.8f)*pi,pitch=std::sin(time*.4f)*1.5f,bank=std::cos(time*.7f)*1.2f;
        h.head={euler(yaw,pitch,bank),{std::sin(time)*.2f,std::cos(time)*.1f,std::sin(time*.5f)*.3f}};
        h.xrFrame=i+1;h.predictedDisplayTime=1+int64_t(i)*11111111;state.stick={.3f,.7f};
        p=game::proposeInput(state,h,previous);previous=p.yaw;
        const float g=.3f-yaw;
        const Vec3 move{.3f*std::cos(g)+.7f*std::sin(g),0,-.3f*std::sin(g)+.7f*std::cos(g)};
        const Vec3 aim{std::sin(g)*std::cos(pitch),std::sin(pitch),std::cos(g)*std::cos(pitch)};
        sequence&=p.active()&&near(move,p.worldMovement)&&near(aim,p.aimForward)&&h.anchor.root.x==root.x;
    }
    check(sequence,"16200 samples: three-minute 90 Hz pose sequence matches independent trig oracle");
    if(argc==3&&std::string(argv[1])=="--vectors"){
        std::ofstream out(argv[2]);if(!out)return 2;out<<std::setprecision(10)<<'[';
        h.trackingToAnchor=identityPose();h.anchor.orientation={0,0,0,1};state.stick={.3f,.7f};
        auto vec=[&](Vec3 v){out<<'['<<v.x<<','<<v.y<<','<<v.z<<']';};
        auto quat=[&](Quat q){out<<'['<<q.x<<','<<q.y<<','<<q.z<<','<<q.w<<']';};
        for(unsigned i=0;i<64;++i){
            if(i)out<<',';h.head.orientation=euler(float(i)*.17f-3.f,std::sin(float(i))*.9f,std::cos(float(i))*.8f);
            auto r=game::proposeInput(state,h,0);
            out<<"{\"head_q\":";quat(r.cameraOrientation);out<<",\"yaw_q\":";quat(r.movementOrientation);
            out<<",\"aim_q\":";quat(r.aimWithoutRoll);out<<",\"forward\":";vec(r.aimForward);
            out<<",\"movement\":";vec(r.worldMovement);out<<",\"stick\":[0.3,0.7]}";
        }out<<"]\n";if(!out)return 2;
    }
    std::cout<<passed<<" PASS / "<<failed<<" FAIL\n";return failed?1:0;
}
