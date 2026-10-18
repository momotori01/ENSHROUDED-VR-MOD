#include <evr/guard_input.hpp>
#include <iostream>
using namespace evr;using namespace evr::motion;
unsigned checks=0;void check(bool v,const char* s){++checks;if(!v)throw std::runtime_error(s);}
struct Rig {
    GuardCalibration c{.15f,.10f,.30f,.20f,.04f,.60f,.08f,20.f,30.f,100000000,100000000};
    GuardInput gate{c};GuardBodyTracker tracker{{0,-.65f,0},7};HandFrame f;GestureHead head;GuardBody b;GuardEquipment w;GuardAuthority n;GuardInputResult out;
    uint64_t now=1000,receipt=0;float front=0,height=-.1f;
    Rig(bool two=false){
        f.focused=true;f.sessionGeneration=f.originGeneration=1;
        for(auto& h:f.hands){h.profile=1;h.gripActive=h.grabActive=h.grab=true;h.gripFlags=15;}
        head.pose.position={0,1.65f,0};head.flags=15;
        w.kind=two?GuardKind::TwoHanded:GuardKind::Shield;w.binding=5;w.support.binding=5;w.support.twoHanded=two;
        w.support.offHandGripInModel={-.4f,0,0};w.support.offHandTolerance=.01f;
        n.enabled=n.known=n.local=n.alive=n.canBlock=n.canRequestParry=true;n.uiOrEvent=false;
    }
    GuardInputResult step(){
        now+=20;receipt=now;++f.frame;f.predictedTime+=20000000;
        head.frame=f.frame;head.session=f.sessionGeneration;head.origin=f.originGeneration;head.time=f.predictedTime;b=tracker.update(f,head);
        f.hands[0].grip.position=b.navel+Vec3{-.2f,height,-front};f.hands[1].grip.position=b.navel+Vec3{.2f,height,-front};
        return out=gate.update(now,f,receipt,b,w,n);
    }
    void lower(){front=0;height=-.1f;for(int i=0;i<12;i++)step();}
    void raise(){for(int i=1;i<=10;i++){front=.045f*i;height=-.1f+.035f*i;step();}}
    void enter(){lower();raise();check(out.buttons==256,"armed entry emits LB");}
    void duplicate(){out=gate.update(++now,f,receipt,b,w,n);}
};
int main(){try{
 for(bool two:{false,true}){
    Rig r(two);r.front=.45f;r.height=.25f;r.step();check(!r.out.buttons&&!r.out.edge,"startup guard cannot inject parry edge");
    r.enter();for(int i=0;i<500;i++){r.step();check(r.out.buttons==256&&!r.out.edge,"held pose stable without edges");r.duplicate();check(r.out.buttons==256&&!r.out.edge,"duplicate frame preserves hold");}
    for(unsigned why=0;why<10;why++){
        r.enter();const auto oldN=r.n;const auto oldW=r.w;const auto oldF=r.f;
        switch(why){case 0:r.n.canBlock=false;break;case 1:r.n.uiOrEvent=true;break;case 2:r.n.reservedGesture=true;break;
        case 3:r.n.alive=false;break;case 4:r.n.attackRequested=true;break;case 5:r.n.attackInProgress=true;break;
        case 6:r.receipt-=201;break;case 7:r.w.binding++;break;case 8:r.f.focused=false;break;
        case 9:if(two)r.f.hands[0].grab=false;else r.f.hands[0].gripFlags=0;break;}
        r.duplicate();check(!r.out.buttons&&r.out.released,"same-frame interruption immediately releases");
        r.n=oldN;r.w=oldW;r.f=oldF;r.receipt=r.now;r.duplicate();check(!r.out.buttons&&!r.out.edge,"same-frame recovery no queued edge");
        for(int i=0;i<30;i++){r.step();check(!r.out.buttons&&!r.out.edge,"held recovery does not become parry");}
    }
    r.enter();r.f.originGeneration++;r.step();check(!r.out.buttons,"recenter clears held input");
    r.step();check(!r.out.buttons,"recenter recovery must lower first");
    r.enter();r.f.sessionGeneration++;r.step();check(!r.out.buttons,"new session clears hold");
    r.enter();r.now-=100;r.duplicate();check(!r.out.buttons,"clock rollback clears hold");
    r.lower();r.height=.7f;for(int i=0;i<10;i++)r.step();check(!r.out.buttons,"teleport rejected instead of overhead edge");
    r.lower();for(int i=0;i<30;i++){r.height+=.035f;r.step();}check(r.out.buttons==256,"smooth overhead without forward extension");
 }
 for(float yaw:{-pi,-pi/2,0.f,pi/2,pi})for(float pitch:{-.7f,0.f,.7f})for(float roll:{-.8f,0.f,.8f}){
    Rig r;r.head.pose.orientation=euler(yaw,pitch,roll);r.step();
    check(r.b.known&&length(r.b.navel-Vec3{0,1,0})<1e-6f,"navel offset is not pitched or rolled");
    check(length(r.b.forward-rotate(euler(yaw,0,0),{0,0,-1}))<1e-5f,"body yaw only");
    r.head.pose.orientation=euler(yaw,pi/2,roll);r.step();check(r.b.known&&length(r.b.forward-rotate(euler(yaw,0,0),{0,0,-1}))<1e-5f,"vertical look retains established heading");
 }
 {Rig r;r.head.pose.orientation=euler(.7f,pi/2,0);r.step();check(!r.b.known,"vertical startup cannot invent heading");}
 std::cout<<"PASS "<<checks<<" guard input/recovery/body checks\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL after "<<checks<<": "<<e.what()<<'\n';return 1;}}
