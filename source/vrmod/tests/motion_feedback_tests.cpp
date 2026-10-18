#include <evr/motion_feedback.hpp>
#include <iostream>
#include <limits>
using namespace evr::motion;
int main(){unsigned n=0;try{
 auto check=[&](bool v){n++;if(!v)throw std::runtime_error("feedback invariant "+std::to_string(n));};
 HandFrame f;f.frame=20;f.sessionGeneration=2;f.originGeneration=3;f.focused=true;
 for(auto& h:f.hands){h.gripActive=h.grabActive=h.grab=true;h.gripFlags=15;}
 Feedback m{19,2,3,1000,{.2f,.6f}};
 check(feedbackForFrame(m,f,1010)==m.amplitude);
 for(unsigned bad=0;bad<12;bad++){auto b=m;auto h=f;uint64_t now=1010;
 switch(bad){case 0:b.frame=0;break;case 1:b.frame=21;break;case 2:b.frame=17;break;case 3:b.session++;break;case 4:b.origin++;break;
 case 5:now=999;break;case 6:now=1201;break;case 7:b.amplitude[0]=-1;break;case 8:b.amplitude[1]=1.1f;break;case 9:b.amplitude[1]=std::numeric_limits<float>::quiet_NaN();break;case 10:h.focused=false;break;case 11:b.milliseconds=0;break;}
 check(feedbackForFrame(b,h,now)==std::array<float,2>{});}
 for(unsigned h=0;h<2;h++)for(unsigned bad=0;bad<3;bad++){auto b=f;
 if(bad==0)b.hands[h].gripFlags=3;if(bad==1)b.hands[h].grab=false;if(bad==2)b.hands[h].grabActive=false;
 auto a=feedbackForFrame(m,b,1010);check(a[h]==0&&a[1-h]==m.amplitude[1-h]);}
 for(unsigned remaining=100;remaining>0;--remaining){auto b=m;b.amplitude[1]=.6f*remaining/100;auto a=feedbackForFrame(b,f,1010);check(a[1]==b.amplitude[1]);}
 std::cout<<"PASS "<<n<<" bounded motion feedback checks\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
