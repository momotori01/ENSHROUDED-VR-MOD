#include <evr/bow_input_trial.hpp>
#include <iostream>
using namespace evr;using namespace evr::motion;
int main(){try{unsigned n=0;auto ck=[&](bool v){++n;if(!v)throw std::runtime_error(std::to_string(n));};
 ck(bowTrialHud(true,true,1,0,false));ck(bowTrialHud(true,false,1,3,false));
 ck(!bowTrialHud(true,false,2,3,false));ck(!bowTrialHud(true,false,1,1,false));ck(!bowTrialHud(true,false,1,3,true));ck(!bowTrialHud(false,false,1,3,false));
 HandFrame f;f.focused=true;f.sessionGeneration=f.originGeneration=1;for(auto& h:f.hands){h.gripActive=h.triggerActive=true;h.gripFlags=15;h.profile=1;}
 LivePadState raw;raw.rightTrigger=255;raw.leftX=234;BowInputTrial t;
 auto run=[&](float tr,uint64_t b=77,bool a=true){++f.frame;f.predictedTime+=11111111;f.hands[1].trigger=tr;return t.update(raw,f,b,a);};
 ck(run(1).buttons==0&&t.phase()==BowInputTrial::Waiting);ck(!run(0).buttons&&t.phase()==BowInputTrial::Armed);
 auto v=run(1);ck(v.buttons==LivePadMapper::X&&v.rightTrigger==0&&v.leftX==234);ck(run(.5f).buttons==LivePadMapper::X);
 ck(!run(0).buttons&&t.phase()==BowInputTrial::Released);ck(!run(1).buttons);ck(!run(0).buttons);ck(!run(1).buttons);
 for(unsigned kind=0;kind<6;kind++){t=BowInputTrial{};f.focused=true;f.sessionGeneration=f.originGeneration=1;f.hands[0].gripFlags=f.hands[1].gripFlags=15;run(0);run(1);
  if(kind==0)f.focused=false;if(kind==1)f.hands[0].gripFlags=0;if(kind==2)++f.originGeneration;if(kind==3)++f.sessionGeneration;if(kind==4)f.predictedTime+=10000000001ll;
  auto r=run(1,kind==5?88:77);ck(!r.buttons&&t.phase()==BowInputTrial::Aborted);ck(!run(0).buttons);ck(!run(1).buttons);
 }
 t=BowInputTrial(true);f.focused=true;f.hands[0].gripFlags=15;
 ck(run(0).leftTrigger==255&&t.phase()==BowInputTrial::Armed);
 auto aimed=run(1);ck(aimed.leftTrigger==255&&aimed.rightTrigger==255&&!aimed.buttons);
 aimed=run(0);ck(aimed.leftTrigger==255&&!aimed.rightTrigger&&t.phase()==BowInputTrial::Released);
 ck(!run(1).rightTrigger);f.predictedTime+=500000000ll;ck(!run(1).leftTrigger);
 t=BowInputTrial(true);run(0);run(1);f.focused=false;aimed=run(1);
 ck(!aimed.leftTrigger&&!aimed.rightTrigger&&t.phase()==BowInputTrial::Aborted);
 t=BowInputTrial(true,true);f.focused=true;f.hands[1].trigger=0;
 ck(!t.update(raw,f,77,true,100).leftTrigger);
 f.hands[1].trigger=1;aimed=t.update(raw,f,77,true,120);ck(aimed.leftTrigger==255&&!aimed.rightTrigger);
 aimed=t.update(raw,f,77,true,1800);ck(aimed.leftTrigger==255&&!aimed.rightTrigger);
 f.hands[1].trigger=0;aimed=t.update(raw,f,77,true,1900);ck(aimed.leftTrigger==255&&aimed.rightTrigger==255);
 aimed=t.update(raw,f,77,true,1959);ck(aimed.rightTrigger==255);
 aimed=t.update(raw,f,77,true,1960);ck(aimed.leftTrigger==255&&!aimed.rightTrigger);
 aimed=t.update(raw,f,77,true,2400);ck(!aimed.leftTrigger&&!aimed.rightTrigger);
 f.hands[1].trigger=1;ck(!t.update(raw,f,77,true,2500).rightTrigger);
 t=BowInputTrial(true,true);f.hands[1].trigger=0;t.update(raw,f,77,true,3000);f.hands[1].trigger=1;t.update(raw,f,77,true,3020);
 aimed=t.update(raw,f,77,false,3100);ck(!aimed.leftTrigger&&!aimed.rightTrigger&&t.phase()==BowInputTrial::Aborted);
 // The cancellation comparison cannot create either a shoot tap or X.
 // It must also stay disarmed after focus, tracking, identity or time loss.
 for(unsigned kind=0;kind<8;kind++){
  t=BowInputTrial(false,false,true);f.focused=true;f.sessionGeneration=f.originGeneration=1;
  f.hands[0].gripFlags=f.hands[1].gripFlags=15;f.hands[1].triggerActive=true;f.hands[0].profile=f.hands[1].profile=1;
  ck(t.cancellationTrial());auto neutral=run(0);ck(!neutral.leftTrigger&&!neutral.rightTrigger);
  auto hold=run(1);ck(hold.leftTrigger==255&&!hold.rightTrigger&&!hold.buttons);
  if(kind==1)f.focused=false;if(kind==2)f.hands[0].gripFlags=0;if(kind==3)++f.originGeneration;
  if(kind==4)++f.sessionGeneration;if(kind==5)f.predictedTime+=10000000001ll;if(kind==6)f.hands[1].triggerActive=false;
  const auto cancel=run(kind?1.f:0.f,kind==7?88:77);
  ck(!cancel.leftTrigger&&!cancel.rightTrigger&&!cancel.buttons);ck(t.phase()==(kind?BowInputTrial::Aborted:BowInputTrial::Released));
  auto again=run(1);ck(!again.leftTrigger&&!again.rightTrigger&&!again.buttons);
 }
 std::cout<<n<<" PASS\n";return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
