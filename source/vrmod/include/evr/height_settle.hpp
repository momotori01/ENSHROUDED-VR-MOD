#pragma once
#include <array>
#include <cmath>
#include <cstdint>
#include <deque>
#include <utility>
namespace evr::game {
// First-person eye-height calibration must not freeze a native camera transition.
// game155510: target height fell 1.716 -> 1.650 m over ~9 s after spawn.
// game163158/163912: PlayerCameraDefault held 1.75 m, then PlayerCameraIdle began
// a blend that kept the height within 1 mm for ~1 s while distance/shoulder moved.
// All camera values (height, distance, shoulder xyz) must stay within `tolerance`
// for distinct native samples spanning `spanMs`. Value-only; reads no game memory.
class HeightSettle {
public:
 static constexpr size_t valueCount=5;
 using Values=std::array<float,valueCount>; // height, distance, shoulder x/y/z
 struct Config {float tolerance=.001f;uint64_t spanMs=1500;unsigned minSamples=15;};
 struct State {bool settled=false;unsigned samples=0;uint64_t spanMs=0;float spread=0,height=0;};
 HeightSettle()=default;
 explicit HeightSettle(Config c):config(c){}
 // `sample` is the native observation number; repeated render reads of the same
 // observation are ignored so a slow game tick cannot fake a long stable span.
 State add(uint64_t sample,uint64_t milliseconds,const Values& values){
  bool valid=sample&&values[0]>0;for(auto v:values)valid=valid&&std::isfinite(v);
  if(!valid){reset();return {};}
  if(!window.empty()&&sample==lastSample)return state();
  if(!window.empty()&&(sample<lastSample||milliseconds<window.back().first))reset();
  lastSample=sample;window.emplace_back(milliseconds,values);
  // Trim only while both the time span and the sample count stay satisfied, so
  // slow diagnostic ticks extend the window instead of never reaching minSamples.
  while(window.size()>config.minSamples&&window[1].first+config.spanMs<=window.back().first)window.pop_front();
  return state();
 }
 State add(uint64_t sample,uint64_t milliseconds,float height){return add(sample,milliseconds,Values{height,0,0,0,0});}
 void reset(){window.clear();lastSample=0;}
private:
 State state()const{
  State s;s.samples=unsigned(window.size());
  if(window.empty())return s;
  s.height=window.back().second[0];
  for(size_t i=0;i<valueCount;i++){
   float lo=window.front().second[i],hi=lo;
   for(const auto& w:window){lo=std::fmin(lo,w.second[i]);hi=std::fmax(hi,w.second[i]);}
   s.spread=std::fmax(s.spread,hi-lo);
  }
  s.spanMs=window.back().first-window.front().first;
  s.settled=s.spanMs>=config.spanMs&&s.samples>=config.minSamples&&s.spread<=config.tolerance;
  return s;
 }
 Config config;
 std::deque<std::pair<uint64_t,Values>> window;
 uint64_t lastSample=0;
};
}
