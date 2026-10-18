#include <evr/bug_report.hpp>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <vector>
using namespace evr;
int main(){try{unsigned n=0;auto ck=[&](bool b){++n;if(!b)throw std::runtime_error(std::to_string(n));};
 // eulerDegrees inverts math.hpp's euler (within the ranges that are unique: pitch in (-90, 90)).
 for(float yaw:{-170.f,-90.f,0.f,35.f,179.f})for(float pitch:{-80.f,-30.f,0.f,45.f,85.f})for(float roll:{-120.f,0.f,10.f,170.f}){
  const float r=3.14159265358979f/180.f;const auto q=euler(yaw*r,pitch*r,roll*r);const auto e=bugreport::eulerDegrees(q);
  const auto back=euler(e[0]*r,e[1]*r,e[2]*r);
  const float dot=std::abs(q.x*back.x+q.y*back.y+q.z*back.z+q.w*back.w);ck(dot>0.99999f);
  ck(std::abs(e[1]-pitch)<1e-2f);}
 // A hand row has the Mock script's 39 values and its ranges.
 {motion::HandFrame f;f.focused=true;for(auto& h:f.hands){h.gripActive=true;h.gripFlags=15;h.grip.position={.3f,1.2f,-.4f};h.trigger=1.5f;h.grab=true;h.stick={2,-.5f};h.stickClick=true;h.primaryPressed=true;}
  const auto row=bugreport::mockHandRow(7,f);
  std::vector<std::string> v;std::stringstream ss(row);std::string c;while(std::getline(ss,c,','))v.push_back(c);
  ck(v.size()==39);ck(v[0]=="7");ck(v[10]=="1");ck(v[11]=="1");ck(v[12]=="1");ck(v[13]=="1");ck(v[27]=="1");ck(v[29]=="1");ck(v[30]=="-0.5");ck(v[31]=="1");ck(v[35]=="1");ck(v[36]=="0");}
 // A head row has the 12 values of the pose script.
 {const auto row=bugreport::mockHeadRow(1.5,{euler(.3f,0,0),{0,1.7f,0}},true,1963,1963);int commas=0;for(char ch:row)commas+=ch==',';ck(commas==11);ck(row.rfind("1.5000,",0)==0);}
 // With the headset's eye fovs: the Mock's 8 optional columns; a fov that is not one is left out.
 {const std::array<float,8> fov{-.98f,.86f,.93f,-.91f,-.86f,.98f,.93f,-.91f};
  const auto row=bugreport::mockHeadRow(1.5,{euler(.3f,0,0),{0,1.7f,0}},true,2804,2804,&fov);int commas=0;for(char ch:row)commas+=ch==',';ck(commas==19);
  ck(row.find(",2804,2804,-0.98")!=std::string::npos);
  const std::array<float,8> zero{};const auto bare=bugreport::mockHeadRow(1.5,{euler(.3f,0,0),{0,1.7f,0}},true,2804,2804,&zero);commas=0;for(char ch:bare)commas+=ch==',';ck(commas==11);}
 // Texts: every language has all of its parts; an unknown language is English.
 for(const char* l:{"japanese","english","german","french","spanish","latam","italian","polish","brazilian","russian","ukrainian","schinese","tchinese","koreana","turkish","klingon"}){
  const auto t=bugreport::texts(l);ck(t.title&&t.description&&t.armed&&t.recording&&t.making&&t.saved&&t.font);
  ck(std::wstring(t.recording).find(L"%d")!=std::wstring::npos);ck(std::wstring(t.saved).find(L"%ls")!=std::wstring::npos);ck(std::wstring(t.description).find(L".zip")!=std::wstring::npos);ck(std::wstring(t.description).find(L"<user>")!=std::wstring::npos);}
 ck(std::wstring(bugreport::texts("klingon").title)==L"! VR BUG REPORT !");ck(std::wstring(bugreport::texts("japanese").title)==L"！VRバグ報告ボタン！");
 // The capture plan stays inside the window, in order.
 {uint64_t last=0;for(const auto& s:bugreport::captureSteps){ck(s.atMs>=last&&s.atMs<bugreport::windowMs&&s.pairs>=1&&s.pairs<=32);last=s.atMs;}ck(bugreport::plannedPairs()==7);unsigned full=0;for(const auto& s:bugreport::captureSteps)full+=s.full?s.pairs:0;ck(full==1);}
 std::cout<<"bug report tests: "<<n<<" checks passed\n";return 0;}
 catch(const std::exception& e){std::cerr<<"bug report test failed at check "<<e.what()<<"\n";return 1;}}
