#include <evr/head_gesture_feedback.hpp>
#include <iostream>
using namespace evr::motion;
int main(){unsigned n=0;try{
 auto check=[&](bool v){++n;if(!v)throw std::runtime_error("gesture feedback "+std::to_string(n));};
 auto send=[](GestureFeedback& m,const HandFrame& f,HeadGestureResult g,const Feedback& a,uint64_t now){
  g.frame=f.frame;g.session=f.sessionGeneration;g.origin=f.originGeneration;g.time=f.predictedTime;return m.update(f,g,a,now);
 };
 HandFrame f;f.frame=f.sessionGeneration=f.originGeneration=1;f.predictedTime=1000000000;f.focused=true;
 for(auto& h:f.hands){h.gripActive=h.grabActive=h.grab=true;h.gripFlags=15;h.profile=1;}
 HeadGestureResult g;g.valid=g.active=g.entered=true;g.pulseAmplitude=.15f;g.pulseDuration=30000000;
 Feedback attack{1,1,1,1000,{.4f,.6f}};GestureFeedback mix;
 auto out=send(mix,f,g,attack,1000);check(out.fresh&&out.amplitude[0]==.4f&&out.amplitude[1]==.6f);
 check(!send(mix,f,g,attack,1000).fresh);
 g.entered=false;f.frame++;f.predictedTime+=10000000;attack.frame=f.frame;attack.amplitude[1]=.1f;
 out=send(mix,f,g,attack,1001);check(out.amplitude[1]==.15f);
 f.frame++;f.predictedTime+=20000000;attack.frame=f.frame;out=send(mix,f,g,attack,1002);check(out.amplitude[1]==.1f&&out.amplitude[0]==.4f);
 // No grip required for a temple confirmation pulse. Native recovery still
 // keeps its own grab requirement, and pulses never extend on later frames.
 GestureFeedback pulse;f.frame++;g.entered=true;f.hands[1].grab=false;out=send(pulse,f,g,{},1003);
 check(out.amplitude[1]==.15f&&out.duration[1]==30000000);g.entered=false;
 for(int i=1;i<=4;i++){f.frame++;f.predictedTime+=10000000;out=send(pulse,f,g,{},1004+i);check(out.amplitude[1]==(i<3?.15f:0));if(i<3)check(out.duration[1]==30000000-10000000*i);}
 for(unsigned cause=0;cause<5;cause++){
  GestureFeedback m;auto h=f;h.hands[1].grab=true;auto start=g;start.entered=true;send(m,h,start,{},1010);h.frame++;h.predictedTime+=10000000;start.entered=false;
  if(cause==0)h.focused=false;if(cause==1)h.hands[1].gripFlags=3;if(cause==2)h.originGeneration++;if(cause==3)h.hands[1].profile++;if(cause==4)start.left=true;
  out=send(m,h,start,{},1011);check(out.fresh&&out.amplitude[1]==0);
 }
 {GestureFeedback stale;auto h=f;auto old=g;old.entered=true;
  old.frame=h.frame;old.session=h.sessionGeneration;old.origin=h.originGeneration;old.time=h.predictedTime;
  stale.update(h,old,{},1020);h.frame++;h.predictedTime+=10000000;
  check(stale.update(h,old,{},1021).amplitude[1]==0);
 }
 // Left temple entry: the confirmation pulse goes to the left hand only.
 {GestureFeedback left;auto h=f;h.frame+=10;h.predictedTime+=100000000;auto start=g;start.entered=true;start.enteredHand=0;
  auto o=send(left,h,start,{},1030);check(o.amplitude[0]==.15f&&o.duration[0]==30000000&&o.amplitude[1]==0);
  start.entered=false;h.frame++;h.predictedTime+=10000000;o=send(left,h,start,{},1031);check(o.amplitude[0]==.15f&&o.amplitude[1]==0);
  h.frame++;h.predictedTime+=10000000;h.hands[0].gripFlags=3;o=send(left,h,start,{},1032);check(o.amplitude[0]==0);}
 {GestureFeedback none;auto h=f;h.frame+=20;h.predictedTime+=200000000;auto start=g;start.entered=true;start.enteredHand=0;h.hands[0].gripFlags=3;
  auto o=send(none,h,start,{},1040);check(o.amplitude[0]==0&&o.amplitude[1]==0);}
 std::cout<<"PASS "<<n<<" gesture/recovery feedback checks\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
