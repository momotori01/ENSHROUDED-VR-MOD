#include <evr/guard_intent.hpp>
#include <iostream>
#include <limits>
#include <cstring>
using namespace evr;using namespace evr::motion;
static unsigned checks=0;
static void check(bool ok,const char* name){++checks;if(!ok)throw std::runtime_error(name);}
// Synthetic body dimensions / integrity limits: test values, not product defaults.
struct Rig {
    GuardCalibration config{.15f,.10f,.30f,.20f,.04f,.60f,.08f,1000.f,1000.f,100000000,100000000};
    GuardGate gate{config};HandFrame f;GuardBody body;GuardEquipment w;GuardAuthority n;GuardIntent out;
    Rig(bool two=false){
        f.focused=true;f.sessionGeneration=f.originGeneration=1;
        for(auto& h:f.hands){h.profile=1;h.gripActive=h.grabActive=h.grab=true;h.gripFlags=15;}
        body.known=true;body.calibration=1;body.navel={0,1,0};body.forward={0,0,-1};
        w.kind=two?GuardKind::TwoHanded:GuardKind::Shield;w.binding=5;
        w.support.binding=5;w.support.twoHanded=two;w.support.offHandGripInModel={-.4f,0,0};w.support.offHandTolerance=.01f;
        n.enabled=n.known=n.local=n.alive=n.canBlock=n.canRequestParry=true;n.uiOrEvent=false;
        place(.20f,0);
    }
    void place(float height,float front){f.hands[0].grip.position={-.2f,body.navel.y+height,-front};f.hands[1].grip.position={.2f,body.navel.y+height,-front};}
    GuardIntent step(XrDuration delta=10000000){
        ++f.frame;f.predictedTime+=delta;body.frame=f.frame;body.session=f.sessionGeneration;body.origin=f.originGeneration;body.time=f.predictedTime;
        const auto before=f;const auto oldBody=body;const auto native=n;
        out=gate.update(f,body,w,n);
        check(std::memcmp(&f,&before,sizeof f)==0&&std::memcmp(&body,&oldBody,sizeof body)==0&&std::memcmp(&native,&n,sizeof n)==0,"input values unchanged");
        return out;
    }
    void lower(){place(-.15f,0);for(int i=0;i<12;i++)step();}
    void enter(){place(.25f,.45f);step();}
};
int main(){try {
    bool threw=false;try{GuardGate missing{{}};}catch(const std::invalid_argument&){threw=true;}check(threw,"uncalibrated guard rejected");
    for(bool two:{false,true}){
        Rig r(two);r.enter();check(r.out.blockHeld&&!r.out.parryCandidate,"startup pose does not create parry");
        for(int i=0;i<600;i++){r.step();check(r.out.blockHeld&&!r.out.parryCandidate,"ten seconds steady guard no repeats");}
        r.lower();r.enter();check(r.out.blockHeld&&r.out.entered&&r.out.parryCandidate,"armed posture entry once");
        for(int i=0;i<80;i++){
            r.place(i%2?-.02f:.02f,two?.25f:.12f);r.step();check(r.out.blockHeld&&!r.out.parryCandidate,"boundary noise held without parry");
        }
        r.place(-.15f,0);for(int i=0;i<3;i++)r.step();r.enter();check(!r.out.parryCandidate,"short neutral cannot refresh parry");
        r.lower();r.n.canRequestParry=false;r.enter();check(r.out.blockHeld&&!r.out.parryCandidate,"native parry unavailable preserves block");
        r.n.canRequestParry=true;r.step();check(!r.out.parryCandidate,"parry never queued for permission");
        r.lower();r.n.canBlock=false;r.enter();check(r.out.blocked==GuardBlock::NativeUnavailable&&!r.out.blockHeld&&!r.out.parryCandidate,"native CanBlock wins");
        r.n.canBlock=true;r.step();check(r.out.blockHeld&&!r.out.parryCandidate,"native recovery not new posture edge");
        for(bool inProgress:{false,true})for(bool verified:{false,true}){
            r.lower();r.n.attackRequested=!inProgress;r.n.attackInProgress=inProgress;r.n.attackAndGuardVerified=verified;r.enter();
            check(r.out.blockHeld==verified&&r.out.parryCandidate==verified,"attack priority unless native overlap verified");
            if(!verified)check(r.out.blocked==GuardBlock::AttackPriority,"explicit attack priority diagnostic");
            r.n.attackRequested=r.n.attackInProgress=false;r.step();check(r.out.blockHeld&&!r.out.parryCandidate,"denied guard entry not queued behind attack");
        }
        for(unsigned reason=0;reason<10;reason++){
            r.lower();r.enter();check(r.out.parryCandidate,"arm before interruption");
            const auto oldN=r.n;const auto oldF=r.f;const auto oldW=r.w;
            switch(reason){
            case 0:r.n.uiOrEvent=true;break;case 1:r.n.reservedGesture=true;break;case 2:r.n.alive=false;break;
            case 3:r.f.focused=false;break;case 4:r.f.hands[0].gripFlags=0;break;case 5:r.f.originGeneration++;break;
            case 6:r.f.sessionGeneration++;break;case 7:r.w.binding++;break;case 8:r.f.hands[0].profile++;break;
            case 9:r.body.calibration++;break;}
            r.step();check(!r.out.blockHeld&&!r.out.parryCandidate&&r.out.released,"interruption releases guard");
            r.n=oldN;r.w=oldW;r.f.hands=oldF.hands;r.f.focused=true;
            r.step();check(r.out.blockHeld&&!r.out.parryCandidate,"recovery holds guard without fabricated parry");
        }
        // Directly above the body is allowed even with zero forward extension.
        r.lower();r.place(.7f,0);r.step();check(r.out.blockHeld&&r.out.parryCandidate,"overhead guard");
        for(float height:{.65f,1.f,1.7f,2.5f}){r.place(height,0);r.step();check(r.out.blockHeld&&!r.out.parryCandidate,"no arbitrary upper height cap");}
        r.lower();r.place(.3f,-.4f);r.step();check(!r.out.blockHeld,"behind torso excluded");
        r.lower();r.place(.7f,-.2f);r.step();check(!r.out.blockHeld,"behind overhead calibration excluded");
        r.lower();r.enter();r.f.predictedTime+=200000000;r.step();check(!r.out.blockHeld&&!r.out.parryCandidate,"large time gap releases");
        r.step();check(r.out.blockHeld&&!r.out.parryCandidate,"gap recovery no edge");
        if(two){
            for(unsigned hand=0;hand<2;hand++){
                r.lower();r.f.hands[hand].grab=false;r.enter();check(!r.out.blockHeld&&r.out.blocked==GuardBlock::Grip,"both grips required");
                r.f.hands[hand].grab=true;r.step();check(r.out.blockHeld&&!r.out.parryCandidate,"regrip no fabricated edge");
            }
            r.lower();r.f.hands[1].grip.position.x=2;r.step();check(r.out.blocked==GuardBlock::Grip,"misaligned support rejected");
        }else{
            r.lower();r.f.hands[1].gripFlags=0;r.enter();check(r.out.blockHeld&&r.out.parryCandidate,"shield does not need unrelated hand tracking");
        }
    }
    // Rotation/translation of the calibrated body frame cannot change intent.
    for(float yaw:{-pi,-pi/2,0.f,pi/2,pi})for(float navelY:{.5f,1.f,1.3f}){
        Rig r;r.body.navel={2,navelY,-3};r.body.forward=rotate(euler(yaw,0,0),{0,0,-1});
        for(int i=0;i<14;i++){r.f.hands[0].grip.position=r.body.navel+Vec3{0,-.2f,0};r.step();}
        r.f.hands[0].grip.position=r.body.navel+r.body.forward*.3f+Vec3{0,.2f,0};r.step();
        check(r.out.blockHeld&&r.out.parryCandidate,"yaw and calibrated seated heights invariant");
    }
    // Integrity rejection (instead of a high-speed tracking jump making a parry).
    {Rig r;r.config.maximumHandSpeed=10;r.gate=GuardGate{r.config};r.lower();r.enter();check(r.out.blocked==GuardBlock::Discontinuity&&!r.out.parryCandidate,"tracking jump rejected");}
    {Rig r;r.lower();r.body.forward={0,.1f,-1};r.step();check(!r.out.valid&&!r.out.blockHeld,"body pitch not accepted as horizontal heading");}
    {Rig r;r.lower();r.f.hands[0].grip.position.y=std::numeric_limits<float>::quiet_NaN();r.step();check(!r.out.valid&&!r.out.blockHeld,"nonfinite hand rejected");}
    std::cout<<"PASS "<<checks<<" guard posture/permission checks (value-only; native integration pending)\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL after "<<checks<<": "<<e.what()<<'\n';return 1;}}
