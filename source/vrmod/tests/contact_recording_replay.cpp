#include <evr/observation_budget.hpp>
#include <evr/motion_event_retime.hpp>
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>
using namespace evr;
int main(int argc,char** argv){try{
 if(argc<3||argc>4)throw std::runtime_error("input.tsv log_limit [simulate-old-cutoff]");
 std::ifstream input(argv[1]);if(!input)throw std::runtime_error("missing input");
 diagnostic::ObservationBudget budget(std::stoull(argv[2]));const bool old=argc==4;motion::EventRetimeLedger<8> ledger;
 uint64_t count=0,mismatches=0,appended=0,suppressed=0,logged=0;std::string line;
 while(std::getline(input,line)){
  std::istringstream s(line);motion::EventActionKey k;unsigned call=0,sub=0,early=0,qualified=0;size_t capacity=0,n=0;motion::EventIdentity impact;std::string guid;
  s>>k.world>>k.entity>>k.sequence>>k.item>>k.trigger>>call>>sub>>impact.variant>>guid>>early>>qualified>>capacity>>n;k.call=uint8_t(call);k.sub=uint8_t(sub);
  if(guid.size()!=32||n>256)throw std::runtime_error("event row bounds");
  for(unsigned i=0;i<16;i++)impact.guid[i]=uint8_t(std::stoul(guid.substr(i*2,2),nullptr,16));
  std::vector<uintptr_t> original(n);for(auto& p:original)s>>p;s>>n;if(n>256)throw std::runtime_error("expected bounds");std::vector<uintptr_t> expected(n);for(auto& p:expected)s>>p;
  if(!s)throw std::runtime_error("truncated row");std::string trailing;if(s>>trailing)throw std::runtime_error("trailing row");
  const auto record=budget.observe();++count;logged+=record.record;auto out=original;
  if(!old||record.record){
   out.clear();for(auto variant:original){auto event=impact;event.variant=variant;
    if(ledger.consider(k,event,false,false,original.size(),capacity)==motion::EventRetime::Decision::Suppress)++suppressed;else out.push_back(variant);}
   if(early&&std::find(out.begin(),out.end(),impact.variant)==out.end()&&ledger.consider(k,impact,true,qualified!=0,out.size(),capacity)==motion::EventRetime::Decision::Append){
    out.push_back(impact.variant);if(!ledger.commit(k,impact))throw std::runtime_error("duplicate commit");++appended;}
  }
  if(out!=expected)++mismatches;
 }
 std::cout<<"{\"rows\":"<<count<<",\"recorded\":"<<logged<<",\"mismatches\":"<<mismatches<<",\"appended\":"<<appended<<",\"suppressed\":"<<suppressed<<",\"simulated_old_cutoff\":"<<(old?"true":"false")<<"}\n";return mismatches?2:0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
