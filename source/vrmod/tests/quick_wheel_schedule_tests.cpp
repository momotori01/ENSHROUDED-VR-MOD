#include <evr/quick_wheel_schedule.hpp>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
using namespace evr::motion;
using Verdict=QuickWheelProgress::Verdict;
struct Run {Verdict verdict=Verdict::Continue;std::vector<std::pair<uint64_t,unsigned>> tags;std::string error;size_t closed=0;};
// Replays frames first..last; overlay follows `open(frame)`, release state follows `released(frame)`.
template<class Open,class Released>
Run replay(const QuickWheelSchedule& s,uint64_t first,uint64_t last,Open open,Released released,unsigned repeats=1){
 QuickWheelProgress p(s);Run r;
 for(uint64_t f=first;f<=last;f++)for(unsigned k=0;k<repeats;k++){
  const auto step=p.update(f,open(f),released(f));
  if(step.captureTag)r.tags.push_back({f,step.captureTag});
  if(step.verdict!=Verdict::Continue){r.verdict=step.verdict;r.error=step.error?step.error:"";r.closed=p.closedCycles();return r;}
 }
 r.closed=p.closedCycles();return r;
}
int main(){unsigned n=0;try{
 auto check=[&](bool b){++n;if(!b)throw std::runtime_error("quick wheel schedule invariant "+std::to_string(n));};
 auto always=[](uint64_t){return true;};
 // Single schedule: native overlay 452..721 like game150737 (grip 450, release 720, closed at 722).
 auto singleOpen=[](uint64_t f){return f>=452&&f<722;};
 {auto r=replay(quickWheelSingle,364,800,singleOpen,[](uint64_t f){return f<450||f>=740;});
  check(r.verdict==Verdict::Complete&&r.closed==1);
  check(r.tags.size()==3&&r.tags[0]==std::make_pair(uint64_t(520),1u)&&r.tags[1]==std::make_pair(uint64_t(620),3u)&&r.tags[2]==std::make_pair(uint64_t(780),2u));}
 {auto r=replay(quickWheelSingle,364,800,singleOpen,[](uint64_t f){return f<450||f>=740;},3);check(r.verdict==Verdict::Complete&&r.tags.size()==3);}
 // Overlay drops inside the required window.
 {auto r=replay(quickWheelSingle,364,800,[](uint64_t f){return f>=452&&f<722&&f!=600;},always);check(r.verdict==Verdict::Fail&&r.error=="native quick wheel did not stay open");}
 // Never opened -> fails at the first required frame.
 {auto r=replay(quickWheelSingle,364,800,[](uint64_t){return false;},always);check(r.verdict==Verdict::Fail&&r.error=="native quick wheel did not stay open");}
 // Still open at the end, or released state missing at the end.
 {auto r=replay(quickWheelSingle,364,800,[](uint64_t f){return f>=452;},always);check(r.verdict==Verdict::Fail);}
 {auto r=replay(quickWheelSingle,364,800,singleOpen,[](uint64_t){return false;});check(r.verdict==Verdict::Fail&&r.error=="wheel did not close/release");}
 // Reopened after the final close.
 {auto r=replay(quickWheelSingle,364,800,[](uint64_t f){return (f>=452&&f<722)||f==760;},always);check(r.verdict==Verdict::Fail&&r.error=="wheel reopened after the final cycle");}
 // Frames running backward are rejected; equal frames are accepted.
 {QuickWheelProgress p(quickWheelSingle);check(p.update(400,false,true).verdict==Verdict::Continue&&p.update(400,false,true).verdict==Verdict::Continue&&p.update(399,false,true).verdict==Verdict::Fail);}
 // No use after a verdict.
 {QuickWheelProgress p(quickWheelSingle);check(p.update(510,false,true).verdict==Verdict::Fail&&p.update(511,true,true).verdict==Verdict::Fail);}
 // Rebind schedule: grips 450/580/710 (+2 frames to open), releases 530/660/790 (+2 to close).
 auto rebindOpen=[](uint64_t f){return (f>=452&&f<532)||(f>=582&&f<662)||(f>=712&&f<792);};
 auto rebindReleased=[](uint64_t f){return !((f>=450&&f<540)||(f>=580&&f<670)||(f>=710&&f<800));};
 {auto r=replay(quickWheelRebind,333,860,rebindOpen,rebindReleased);
  check(r.verdict==Verdict::Complete&&r.closed==3);
  check(r.tags.size()==3&&r.tags[0]==std::make_pair(uint64_t(515),1u)&&r.tags[1]==std::make_pair(uint64_t(775),3u)&&r.tags[2]==std::make_pair(uint64_t(850),2u));}
 {auto r=replay(quickWheelRebind,333,860,rebindOpen,rebindReleased,2);check(r.verdict==Verdict::Complete&&r.closed==3);}
 // Middle cycle never opens.
 {auto r=replay(quickWheelRebind,333,860,[](uint64_t f){return (f>=452&&f<532)||(f>=712&&f<792);},rebindReleased);check(r.verdict==Verdict::Fail&&r.error=="native quick wheel did not stay open"&&r.closed==1);}
 // Middle cycle does not close within the grace window.
 {auto r=replay(quickWheelRebind,333,860,[](uint64_t f){return (f>=452&&f<532)||(f>=582&&f<700)||(f>=712&&f<792);},rebindReleased);check(r.verdict==Verdict::Fail&&r.error=="wheel did not close/release"&&r.closed==1);}
 // A cycle that never closes before the next one is not counted twice.
 {auto r=replay(quickWheelRebind,333,860,[](uint64_t f){return f>=452&&f<792;},rebindReleased);check(r.verdict==Verdict::Fail&&r.closed==0);}
 // Final close missing.
 {auto r=replay(quickWheelRebind,333,860,[](uint64_t f){return (f>=452&&f<532)||(f>=582&&f<662)||f>=712;},rebindReleased);check(r.verdict==Verdict::Fail);}
 // Consumable observation: grips 450/580 (+2 to open), releases 530/660 (+2 to close).
 {auto open=[](uint64_t f){return (f>=452&&f<532)||(f>=582&&f<662);};
  auto released=[](uint64_t f){return !((f>=450&&f<540)||(f>=580&&f<670));};
  auto r=replay(quickWheelConsumable,333,730,open,released);
  check(r.verdict==Verdict::Complete&&r.closed==2);
  check(r.tags.size()==3&&r.tags[0]==std::make_pair(uint64_t(515),1u)&&r.tags[1]==std::make_pair(uint64_t(645),3u)&&r.tags[2]==std::make_pair(uint64_t(720),2u));
  auto bad=replay(quickWheelConsumable,333,730,[](uint64_t f){return f>=452&&f<532;},released);check(bad.verdict==Verdict::Fail&&bad.closed==1);}
 check(quickWheelSchedule("EVR_QUICK_WHEEL_CONSUMABLE_1")==&quickWheelConsumable&&quickWheelSchedule("EVR_QUICK_WHEEL_CONSUMABLE_2")==&quickWheelConsumableRight);
 // Survey: grips 450,530,610,690,770 (+2 to open), releases 510,590,670,750,830 (+2 to close).
 {auto open=[](uint64_t f){for(uint64_t g:{450,530,610,690,770})if(f>=g+2&&f<g+62)return true;return false;};
  auto released=[](uint64_t f){for(uint64_t g:{450,530,610,690,770})if(f>=g&&f<g+70)return false;return true;};
  auto r=replay(quickWheelSurvey,333,880,open,released);check(r.verdict==Verdict::Complete&&r.closed==5);
  check(r.tags.size()==3&&r.tags[0]==std::make_pair(uint64_t(500),1u)&&r.tags[1]==std::make_pair(uint64_t(740),3u)&&r.tags[2]==std::make_pair(uint64_t(870),2u));
  auto bad=replay(quickWheelSurvey,333,880,[&](uint64_t f){return open(f)&&!(f>=612&&f<672);},released);check(bad.verdict==Verdict::Fail&&bad.closed==2);
  check(quickWheelSchedule("EVR_QUICK_WHEEL_SURVEY_1")==&quickWheelSurvey);}
 // Router: RB from the router at 477 and 757 (long press 300 ms), releases at 550/820.
 {auto open=[](uint64_t f){return (f>=479&&f<552)||(f>=759&&f<822);};
  auto released=[](uint64_t f){return !((f>=477&&f<560)||(f>=757&&f<830));};
  auto r=replay(quickWheelRouter,333,880,open,released);check(r.verdict==Verdict::Complete&&r.closed==2);
  check(r.tags.size()==3&&r.tags[0]==std::make_pair(uint64_t(520),1u)&&r.tags[1]==std::make_pair(uint64_t(800),3u)&&r.tags[2]==std::make_pair(uint64_t(860),2u));
  check(quickWheelSchedule("EVR_GRIP_ROUTER_WHEEL_1")==&quickWheelRouter);}
 // Equip: WheelSelect opens 462/552/652 and releases after ~330 ms (492/582/682), closes +2.
 {auto open=[](uint64_t f){return (f>=462&&f<494)||(f>=552&&f<584)||(f>=652&&f<684);};
  auto released=[](uint64_t f){return !((f>=460&&f<494)||(f>=550&&f<584)||(f>=650&&f<684));};
  auto r=replay(quickWheelEquip,333,760,open,released);check(r.verdict==Verdict::Complete&&r.closed==3);
  check(r.tags.size()==3&&r.tags[0]==std::make_pair(uint64_t(480),1u)&&r.tags[1]==std::make_pair(uint64_t(670),3u)&&r.tags[2]==std::make_pair(uint64_t(740),2u));
  check(quickWheelSchedule("EVR_GRIP_ROUTER_EQUIP_1")==&quickWheelEquip);
  auto o=replay(quickWheelEquipOverlap,333,760,open,released);check(o.verdict==Verdict::Complete&&o.closed==3&&o.tags==r.tags);
  check(quickWheelSchedule("EVR_GRIP_ROUTER_EQUIP_2")==&quickWheelEquipOverlap);}
 // Lookup by exact token only.
 check(quickWheelSchedule("EVR_QUICK_WHEEL_1")==&quickWheelSingle&&quickWheelSchedule("EVR_QUICK_WHEEL_REBIND_1")==&quickWheelRebind);
 check(!quickWheelSchedule("EVR_QUICK_WHEEL")&&!quickWheelSchedule("EVR_QUICK_WHEEL_REBIND_10")&&!quickWheelSchedule(""));
 // Schedule sanity: cycles ordered, non-overlapping, captures within the intended phases.
 for(const auto* s:{&quickWheelSingle,&quickWheelRebind,&quickWheelConsumable,&quickWheelConsumableRight,&quickWheelSurvey,&quickWheelRouter,&quickWheelEquip,&quickWheelEquipOverlap}){
  for(size_t i=0;i<s->count;i++){check(s->cycles[i].open<s->cycles[i].close);if(i)check(s->cycles[i-1].close+s->closeGrace<s->cycles[i].open);}
  check(s->captures[0]>=s->cycles[0].open&&s->captures[2]>=s->cycles[s->count-1].close&&s->captures[2]<s->end);
 }
 std::cout<<"PASS "<<n<<" quick wheel schedule checks\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<"\n";return 1;}}
