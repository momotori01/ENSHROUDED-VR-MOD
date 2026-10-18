#include <evr/motion_actions.hpp>
#include <evr/test_renderer.hpp>
#include <iostream>
#include <limits>
using namespace evr;
static unsigned checks=0;
static void require(bool value,const char* name){if(!value)throw std::runtime_error(name);++checks;std::cout<<"PASS "<<name<<'\n';}
template<class F> static void reject(F f,const char* name){bool rejected=false;try{f();}catch(const std::exception&){rejected=true;}require(rejected,name);}
int main(){try{
 for(unsigned cycle=1;cycle<=2;cycle++){
    Log log("motion-xr-client-"+std::to_string(cycle)+".jsonl",true);OpenXrClient xr(log);xr.init(1);
    motion::Actions input;input.open(&xrGetInstanceProcAddr,xr.instance,xr.session,cycle);
    require(input.unsupportedProfiles.empty(),"all four standard suggested profiles accepted by mock");
    require(!input.vibrate(0,.5f,20000000),"no vibration before a focused tracked sample");
    reject([&]{input.vibrate(2,.5f,20000000);},"reject invalid haptic hand");
    reject([&]{input.vibrate(0,std::numeric_limits<float>::quiet_NaN(),20000000);},"reject nonfinite vibration");
    reject([&]{input.vibrate(0,.5f,XR_INFINITE_DURATION);},"reject unbounded vibration");
    auto end=std::chrono::steady_clock::now()+std::chrono::seconds(10);
    while(!xr.active()){require(xr.poll(),"session events remain active");if(std::chrono::steady_clock::now()>end)throw std::runtime_error("motion XR session timeout");}
    {
      TestRenderer renderer(xr);
      for(unsigned n=1;n<=16;n++){
        require(xr.beginFrame(),"begin simultaneous stereo frame");const auto frame=xr.pendingFrame();
        const uint64_t origin=n<10?1:2;
        auto sample=input.sample(xr.local,frame.predictedDisplayTime,frame.frame,origin);
        require(sample.frame==n&&sample.predictedTime==frame.predictedDisplayTime&&sample.sessionGeneration==cycle&&sample.originGeneration==origin,"hand sample frame/time/session/origin identity");
        require(sample.focused==(n!=6),"focus loss produces empty motion input");
        if(n==6){require(!sample.hands[0].gripActive&&!sample.hands[1].gripActive&&!sample.hands[0].grab&&!sample.hands[0].primaryPressed&&!sample.hands[1].secondaryPressed,"no stale hands on focus loss");require(!input.vibrate(0,.5f,20000000),"no haptic without focus");}
        else for(unsigned h=0;h<2;h++){
          const auto& v=sample.hands[h];bool active=!(n==5&&h==1),tracked=active&&!(n==4&&h==1);
          require(v.gripActive==active&&v.aimActive==active&&v.triggerActive==active&&v.grabActive==active,"independent hand action availability");
          require(v.tracked()==tracked,"per-hand loss does not poison other hand");
          require(v.stickActive==active&&v.stickClickActive==active,"stick and click action availability");
          const bool faceActive=active&&n<=8;
          require(v.primaryActive==faceActive&&v.secondaryActive==faceActive,"face actions available only on bound profiles");
          require(v.primaryPressed==(faceActive&&(n+2*h)%4==1)&&v.secondaryPressed==(faceActive&&(n+h)%3==1),"independent face buttons from CSV and inactive clearing");
          if(!active)require(v.stick.x==0&&v.stick.y==0&&!v.stickClick,"disconnect clears sticks and clicks");
          if(active){
            require(std::abs(v.stick.x-(int((n+h)%5)-2)*.4f)<1e-6f&&std::abs(v.stick.y-(int((n+2*h)%3)-1)*.75f)<1e-6f&&v.stickClick==((n+h)%3==0),"distinct CSV stick axes and clicks");
            require(std::abs(v.trigger-float((n+h)%5)*.25f)<1e-6f&&v.grab==((n+h)%2==0),"CSV trigger and grip values");
            const char* names[]{"oculus/touch_controller","valve/index_controller","htc/vive_controller","microsoft/motion_controller"};
            require(input.profileName(v.profile)==std::string("/interaction_profiles/")+names[(n-1)/4],"runtime controller profile changes independently of HMD");
          }else require(!v.profile&&!v.grab&&v.trigger==0,"disconnect clears profile/buttons");
          if(tracked){
            const Vec3 expected{h?(.25f-.005f*float(n)):(-.25f+.01f*float(n)),1.2f+.002f*float(n),-.4f};
            require(length(v.grip.position-expected)<1e-5f&&length(v.aim.position-expected)<1e-5f,"distinct left/right local position");
            const float yaw=float(n)*(h?-7:5)*pi/180;Vec3 forward=rotate(v.grip.orientation,{0,0,-1});
            require(std::abs(forward.x+std::sin(yaw))<1e-5f&&std::abs(forward.z+std::cos(yaw))<1e-5f,"controller yaw orientation independently checked");
            require(length(rotate(v.aim.orientation,{0,0,-1})-forward)>.02f,"aim pose distinct from grip model axis");
          }
          require(input.vibrate(h,.4f,20000000)==tracked,"only tracked active hands receive bounded haptics");
        }
        reject([&]{input.sample(xr.local,frame.predictedDisplayTime,frame.frame,origin);},"one immutable hand sample per XR frame");
        if(n==8)require(input.stop(0)&&input.stop(1),"explicit haptic cancellation");
        for(unsigned eye=0;eye<2;eye++){renderer.draw(eye,frame.imageIndices[eye],frame.views[eye],"");xr.markEyeRendered(eye,frame.frame);}
        xr.finishFrame();
      }
      require(xr.draws[0]==16&&xr.draws[1]==16,"paired rendering continues with controller lifecycle changes");
    }
    require(input.close(),"action spaces and set released before XR session");require(input.close(),"idempotent action cleanup");
    require(!input.vibrate(0,.5f,20000000),"closed action state cannot vibrate");xr.shutdown();require(!xr.instance&&!xr.session,"session and instance released");
 }
 std::cout<<"RESULT "<<checks<<" PASS 0 FAIL (Mock actions/stereo protocol; no game attacks or weapon rendering)\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
