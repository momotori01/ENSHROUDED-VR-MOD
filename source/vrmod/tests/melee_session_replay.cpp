#include <evr/melee_session.hpp>
#include <fstream>
#include <sstream>
#include <iostream>
using namespace evr;using namespace evr::motion;
int main(int argc,char** argv){try{
 if(argc!=4)throw std::runtime_error("profile input-tsv output-jsonl");
 std::ifstream profile(argv[1]);std::string magic;uint32_t item,type,slot,dummy;profile>>magic>>item>>type>>slot;for(int i=0;i<8;i++)profile>>dummy;
 Weapon w;w.twoHanded=true;w.offHandTolerance=.035f;Vec3 scale;auto& p=w.gripToModel.position;auto& q=w.gripToModel.orientation;auto& off=w.offHandGripInModel;
 profile>>p.x>>p.y>>p.z>>q.x>>q.y>>q.z>>q.w>>scale.x>>scale.y>>scale.z>>off.x>>off.y>>off.z;
 if(!profile||magic!="EVR_GRIP_PREVIEW_1")throw std::runtime_error("measured profile missing");
 MeleeSession driver{{2,.3f,.6f,2,100000000,12,30},{120,1000,200}};std::ifstream input(argv[2]);std::ofstream output(argv[3]);if(!input||!output)throw std::runtime_error("replay I/O");
 unsigned issued=0,accepted=0,retired=0,revoked=0;std::string line;
 while(std::getline(input,line)){
  std::stringstream in(line);uint64_t now,receipt;HandFrame f;MeleeNative n;MeleePermission permission{true,true,false};
  in>>now>>receipt>>f.frame>>f.predictedTime>>f.sessionGeneration>>f.originGeneration>>f.focused;
  for(auto& h:f.hands){in>>h.grip.position.x>>h.grip.position.y>>h.grip.position.z>>h.grip.orientation.x>>h.grip.orientation.y>>h.grip.orientation.z>>h.grip.orientation.w>>h.gripFlags>>h.profile>>h.gripActive>>h.grabActive>>h.grab;}
  in>>n.world>>n.entity>>n.item>>n.binding>>n.milliseconds>>n.known>>n.local>>n.alive>>n.gameplay>>n.canPrimary>>n.active>>n.usedItem>>n.sequence>>n.trigger>>n.triggerTime>>n.playedTime>>n.currentTime>>n.length>>permission.reserved;
  if(!in||n.item!=item)throw std::runtime_error("bad native replay row");w.binding=n.binding;
  const auto o=driver.update(now,f,receipt,w,n,permission);issued+=bool(o.issued);accepted+=bool(o.accepted);retired+=bool(o.retired);revoked+=o.revoked;
  output<<"{\"frame\":"<<f.frame<<",\"issued\":"<<o.issued<<",\"accepted\":"<<o.accepted<<",\"retired\":"<<o.retired<<",\"current\":"<<o.current<<",\"level\":"<<unsigned(o.level)<<",\"revoked\":"<<(o.revoked?"true":"false")<<",\"suspended\":"<<(o.suspended?"true":"false")<<",\"reason\":"<<unsigned(o.reason)<<",\"blocked\":"<<unsigned(o.intent.blocked)<<",\"speed\":"<<o.intent.speed<<"}\n";
 }
 std::cout<<"issued="<<issued<<" accepted="<<accepted<<" retired="<<retired<<" revoked="<<revoked<<" completed="<<driver.completed()<<'\n';return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
