#include <evr/height_settle.hpp>
#include <iostream>
#include <stdexcept>
#include <string>
using namespace evr::game;
int main(){unsigned n=0;try{
 auto check=[&](bool b){++n;if(!b)throw std::runtime_error("height settle invariant "+std::to_string(n));};
 // game155510-like spawn transition: ~0.8 mm per 85 ms sample, then steady 1.65 m.
 {HeightSettle h;uint64_t sample=0,ms=1000;float height=1.715833f;unsigned firstSettled=0;
  for(unsigned i=0;i<200;i++){
   height=height>1.65f?std::fmax(1.65f,height-.00085f):1.65f;
   const auto s=h.add(++sample,ms+=85,height);
   if(s.settled&&!firstSettled){firstSettled=i;check(std::fabs(s.height-1.65f)<1e-6f&&s.spread<=.001f&&s.spanMs>=1500&&s.samples>=15);}
   if(height>1.652f)check(!s.settled);
  }
  check(firstSettled>77&&firstSettled<110);}
 // game163912-like PlayerCameraIdle blend start: height moves < 1 mm for ~1.5 s
 // while distance and shoulder already move. Height-only stability is fooled;
 // the full camera value set is not.
 {HeightSettle full,heightOnly;bool fullSettled=false,heightSettled=false;
  for(unsigned i=0;i<40;i++){
   const float k=float(i*i);
   const HeightSettle::Values v{1.75f-.000001f*k,3.07692f-.00005f*k,.000012f*k,0,0};
   fullSettled|=full.add(i+1,uint64_t(i)*85,v).settled;
   heightSettled|=heightOnly.add(i+1,uint64_t(i)*85,v[0]).settled;
  }
  check(heightSettled&&!fullSettled);}
 // Idle steady state after the blend: all values constant.
 {HeightSettle h;HeightSettle::State s;for(unsigned i=0;i<40;i++)s=h.add(i+1,uint64_t(i)*85,HeightSettle::Values{1.65f,1.538461566f,.525f,0,0});check(s.settled&&s.spread==0);}
 // Fast ticks: the 1500 ms span dominates.
 {HeightSettle h;uint64_t ms=0;HeightSettle::State s;unsigned i=0;
  for(;i<300&&!(s=h.add(i+1,ms+=11,1.65f)).settled;i++){}
  check(s.settled&&s.spanMs>=1500&&s.spanMs<1520&&i>=135&&i<=138);}
 // Slow ticks: the 15-sample minimum dominates.
 {HeightSettle h;HeightSettle::State s;unsigned i=0;
  for(;i<50&&!(s=h.add(i+1,uint64_t(i)*200,1.65f)).settled;i++){}
  check(s.settled&&s.samples==15&&i==14);}
 // Repeated render reads of one native sample do not count as new samples.
 {HeightSettle h;HeightSettle::State s;for(unsigned i=0;i<100;i++)s=h.add(7,uint64_t(i)*50,1.65f);check(!s.settled&&s.samples==1);}
 // Small idle noise within 1 mm is accepted on any value; 1.2 mm is not.
 {HeightSettle h;HeightSettle::State s;for(unsigned i=0;i<100;i++)s=h.add(i+1,uint64_t(i)*20,1.65f+(i%2?.0009f:0));check(s.settled);}
 {HeightSettle h;HeightSettle::State s;bool any=false;for(unsigned i=0;i<100;i++){s=h.add(i+1,uint64_t(i)*20,1.65f+(i%2?.0012f:0));any|=s.settled;}check(!any);}
 {HeightSettle h;bool any=false;for(unsigned i=0;i<100;i++)any|=h.add(i+1,uint64_t(i)*20,HeightSettle::Values{1.65f,1.5f,.525f+(i%2?.0012f:0),0,0}).settled;check(!any);}
 // Invalid, non-positive or backward samples restart the window.
 {HeightSettle h;HeightSettle::State s;for(unsigned i=0;i<100;i++)s=h.add(i+1,uint64_t(i)*20,1.65f);check(s.settled);
  check(!h.add(101,2020,std::nanf("")).settled);check(!h.add(102,2040,1.65f).settled);
  check(!h.add(103,2060,HeightSettle::Values{1.65f,std::nanf(""),0,0,0}).settled);
  for(unsigned i=0;i<100;i++)s=h.add(104+i,uint64_t(2080+i*20),1.65f);check(s.settled);
  check(!h.add(10,9000,1.65f).settled&&h.add(11,9020,1.65f).samples==2);
  check(!h.add(12,10,1.65f).settled);
  check(!h.add(13,9000,0.f).settled&&!h.add(14,9020,-1.f).settled&&!h.add(0,9040,1.65f).settled);}
 // A late transient after settling is visible to the caller.
 {HeightSettle h;HeightSettle::State s;for(unsigned i=0;i<100;i++)s=h.add(i+1,uint64_t(i)*20,1.65f);check(s.settled);s=h.add(101,2020,1.70f);check(!s.settled&&s.spread>.04f);}
 std::cout<<"PASS "<<n<<" height settle checks\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<"\n";return 1;}}
