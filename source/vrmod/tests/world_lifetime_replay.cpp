#include <evr/world_lifetime.hpp>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>
using namespace evr::motion;
int main(int argc,char** argv){unsigned checks=0;try{
 if(argc!=2)throw std::runtime_error("one evidence CSV required");std::ifstream input(argv[1]);if(!input)throw std::runtime_error("evidence file missing");
 auto check=[&](bool good,const char* why){++checks;if(!good)throw std::runtime_error(why);};
 WorldLifetimes<8> book;std::map<uintptr_t,WorldToken> worlds;std::map<uint64_t,WorldLifetimes<8>::Batch> roots;std::string line;
 auto number=[](const std::string& s){size_t used=0;auto n=std::stoull(s,&used,0);if(used!=s.size())throw std::runtime_error("invalid integer");return n;};
 while(std::getline(input,line)){
  std::istringstream row(line);std::string op,a,b,c,d;row>>op>>a>>b>>c>>d;
  if(op=="adopt"||op=="create"){
   const auto kind=number(d);if(kind>2)throw std::runtime_error("kind out of range");WorldIdentity id{number(a),number(b),number(c),uint8_t(kind)};
   auto token=op=="adopt"?book.adopt(id):book.created(id);check(bool(token)&&book.alive(token)&&book.snapshot(token).identity==id,"observed identity published");check(worlds.emplace(id.world,token).second,"unique World identity");check(book.pin(token),"synthetic late callback pins MOD value");
  }else if(op=="seal"){book.sealAdoption();check(book.adoptionSealed(),"initial observed inventory sealed");}
  else if(op=="root_begin"){
   const auto call=number(a),root=number(b);auto batch=book.beginRootClose(root);check(batch.count>0,"observed native root matched known Worlds");check(roots.emplace(call,batch).second,"native call id unique");for(size_t i=0;i<batch.count;++i)check(!book.alive(batch.tokens[i])&&!book.pin(batch.tokens[i]),"root begin invalidates every member before native cleanup");
  }else if(op=="world_closing"){
   auto token=worlds.at(number(a));auto state=book.snapshot(token);check(state.phase==WorldPhase::Closing&&state.identity.registry==number(b)&&state.identity.kind==number(c),"native child cleanup/storage consumes same closing epoch");check(!book.reclaim(token),"native destruction in flight prevents reclaim");
  }else if(op=="root_end"){
   auto batch=roots.at(number(a));for(size_t i=0;i<batch.count;++i){auto t=batch.tokens[i];check(book.finishClose(t)&&!book.alive(t),"saved tokens complete after native root return");check(!book.reclaim(t),"synthetic late MOD callback still prevents reuse");check(book.unpin(t)&&book.reclaim(t),"last MOD callback permits value-slot retirement");check(!book.pin(t)&&!book.finishClose(t),"late duplicate completion rejected");}roots.erase(number(a));
  }else if(op=="expect_size"){check(book.size()==number(a)&&!book.faulted(),"native trace final population matches");}
  else throw std::runtime_error("unknown evidence operation");
 }
 check(roots.empty(),"no native root call left open");std::cout<<"PASS "<<checks<<" native World trace / synthetic callback checks; retained="<<book.size()<<'\n';return 0;
}catch(const std::exception& e){std::cerr<<"FAIL after "<<checks<<": "<<e.what()<<'\n';return 1;}}
