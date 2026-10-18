#include <evr/motion_guard_fixture.hpp>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <iostream>
using namespace evr;using namespace evr::motion;
int main(){unsigned count=0;try{
 auto check=[&](bool x,const char* name){++count;if(!x)throw std::runtime_error(name);};
 const auto data=std::filesystem::path(__FILE__).parent_path().parent_path()/"test_env";
 std::ifstream profile(data/"right-hand-speed-profile.txt");std::string magic;uint32_t item,type,slot,dummy;profile>>magic>>item>>type>>slot;
 for(int i=0;i<8;i++)profile>>dummy;Weapon w;w.twoHanded=true;w.offHandTolerance=.035f;Vec3 scale;
 auto& p=w.gripToModel.position;auto& q=w.gripToModel.orientation;auto& off=w.offHandGripInModel;
 profile>>p.x>>p.y>>p.z>>q.x>>q.y>>q.z>>q.w>>scale.x>>scale.y>>scale.z>>off.x>>off.y>>off.z;
 check(bool(profile)&&magic=="EVR_GRIP_PREVIEW_1","measured profile");
 const GuardCalibration c{.15f,.10f,.30f,.20f,.04f,.60f,.08f,12,30,100000000,100000000};
 for(unsigned fault=0;fault<5;fault++){
    game::MotionGuardFixture gate{c,w};GuardBodyTracker tracker{{0,-.65f,0},1};
    HandFrame hands;hands.focused=true;hands.sessionGeneration=hands.originGeneration=1;
    game::GuardObservation native;native.valid=native.canBlock=true;native.root=1;native.item=item;native.binding=77;
    unsigned edges=0;uint16_t previous=0;game::MotionGuardFixture::Result result;std::ifstream file(data/"guard-posture-late-hands.csv");check(bool(file),"fixture exists");std::string line;
    while(std::getline(file,line)){
        if(line.empty()||line[0]=='#')continue;std::stringstream stream(line);std::vector<double> v;std::string part;while(std::getline(stream,part,','))v.push_back(std::stod(part));check(v.size()==29,"row shape");
        const auto f=uint64_t(v[0]);if(f<364||f>670)continue;hands.frame=f;hands.predictedTime=f*11111111;
        for(unsigned h=0;h<2;h++){auto& s=hands.hands[h];unsigned k=1+13*h;s.gripActive=s.grabActive=s.grab=true;s.gripFlags=15;s.profile=1;
            s.grip.position={float(v[k]),float(v[k+1]),float(v[k+2])};s.grip.orientation=euler(float(v[k+3])*pi/180,float(v[k+4])*pi/180,float(v[k+5])*pi/180);}
        GestureHead head{Pose{{0,0,0,1},{0,1.65f,0}},15,f,1,1,hands.predictedTime};const auto b=tracker.update(hands,head);const uint64_t now=1000+f*110;
        native.blocking=previous!=0;native.digital=previous?((1ull<<30)|(1ull<<4)):0;
        if(fault&&f>=490){if(fault==1)native.root=2;if(fault==2)native.binding++;if(fault==3)native.attacking=true;if(fault==4)native.digital=16;}
        result=gate.update(now,true,true,native,hands,now,b);
        if(fault&&f>=490){check(result.phase==game::MotionGuardFixture::Phase::Aborted&&!result.buttons,"native mismatch aborts neutral");break;}
        check(result.phase!=game::MotionGuardFixture::Phase::Aborted,"valid native progression");
        if(result.input.edge){edges++;check((f>=460&&f<=480)||(f>=560&&f<=585),"edge only in planned posture entry");}
        if(f<460)check(!result.buttons,"startup held does not press LB");
        previous=result.buttons;
        const auto repeated=gate.update(now+1,true,true,native,hands,now,b);check(!repeated.input.edge&&repeated.buttons==result.buttons,"duplicate frontend sample no repeated edge");
    }
    if(!fault)check(edges==2&&gate.holds()==2&&result.phase==game::MotionGuardFixture::Phase::Complete,"two body-driven native-model holds complete");
 }
 std::cout<<"PASS "<<count<<" archived guard pose/finite adapter checks (native response simulated)\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL after "<<count<<": "<<e.what()<<'\n';return 1;}}
