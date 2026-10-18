#include <evr/world_lifetime.hpp>
#include <iostream>
#include <stdexcept>
using namespace evr::motion;
int main(){unsigned checks=0;try{
 auto check=[&](bool good,const char* why){++checks;if(!good)throw std::runtime_error(why);};
 const WorldIdentity server{100,101,102,0},client{200,201,202,2},prediction{300,201,202,1};
 WorldLifetimes<3> book;WorldToken oldServer,oldClient,oldPrediction;
 for(unsigned n=0;n<20000;++n){
  auto s=n?book.created(server):book.adopt(server);auto c=book.created(client);auto p=book.created(prediction);
  check(s&&c&&p&&book.size()==3,"three distinct Worlds with shared client Registry");
  check(!(s==c)&&!(c==p)&&!(s==p),"World, not Registry, is token identity");
  check(book.adopt(server)==s&&book.adopt(prediction)==p,"adopt does not invent an extra epoch");
  check(book.snapshot(s).provenance==(n?WorldProvenance::NativeCreation:WorldProvenance::Adopted),"native creation and attach adoption distinguished");
  check(!book.finishClose(s)&&!book.reclaim(s),"alive object cannot complete or reclaim");
  check(book.pin(c)&&book.pin(c)&&book.pin(p)&&book.pin(s),"late callbacks hold MOD slot pins");
  if(oldServer){
   check(!book.pin(oldServer)&&!book.unpin(oldClient)&&!book.beginClose(oldPrediction)&&!book.finishClose(oldClient)&&!book.reclaim(oldServer),"callbacks from previous epoch cannot affect matching native addresses");
   check(s.serial>oldServer.serial&&c.serial>oldClient.serial&&p.serial>oldPrediction.serial,"monotonic epochs despite all address/Registry reuse");
  }
  check(book.alive(s)&&book.alive(c)&&book.alive(p)&&book.snapshot(c).pins==2,"stale callback rejection left current Worlds and pins intact");
  book.sealAdoption();
  auto batch=book.beginRootClose(client.root);
  check(batch.count==2&&!book.alive(c)&&!book.alive(p)&&book.alive(s),"client root invalidates both main and prediction only");
  check(!book.pin(c)&&!book.pin(p)&&!book.adopt(client)&&!book.faulted(),"closing cannot rearm on resample");
  check(!book.reclaim(c)&&!book.reclaim(p),"closing state cannot release storage");
  for(size_t i=0;i<batch.count;++i)check(book.finishClose(batch.tokens[i]),"exact saved root batch completes");
  check(!book.reclaim(c)&&!book.reclaim(p),"native end alone cannot release pinned MOD state");
  check(book.unpin(c)&&!book.reclaim(c)&&book.unpin(c)&&book.reclaim(c),"all nested callbacks must release");
  check(book.unpin(p)&&book.reclaim(p),"prediction releases its own slot");
  check(book.beginClose(s)&&!book.alive(s)&&book.finishClose(s),"server independent native close");
  check(book.unpin(s)&&book.reclaim(s)&&book.size()==0,"no World storage leak");
  check(!book.unpin(s)&&!book.finishClose(c)&&!book.reclaim(p),"duplicate completions rejected");
  check(!book.adopt(server)&&!book.adopt(client)&&!book.adopt(prediction),"late snapshots cannot manufacture a World after native end");
  oldServer=s;oldClient=c;oldPrediction=p;
 }
 // Reused address may coexist with an ended, still-pinned MOD value entry.
 WorldLifetimes<2> overlap;auto a=overlap.created(server);check(overlap.pin(a)&&overlap.beginClose(a)&&overlap.finishClose(a),"old native epoch ended while MOD callback remains");
 auto b=overlap.created(server);check(b&&!(b==a)&&overlap.alive(b)&&!overlap.alive(a),"same native address gets separate slot while old pin survives");
 check(!overlap.finishClose(a)&&!overlap.reclaim(a)&&overlap.alive(b),"stale native completion cannot close replacement");
 check(overlap.unpin(a)&&overlap.reclaim(a)&&overlap.alive(b),"old pin release cannot remove new World");
 WorldLifetimes<1> full;auto f=full.created(server);check(!full.created(client)&&full.alive(f)&&!full.faulted(),"storage full cannot evict live World");
 check(full.beginClose(f)&&full.finishClose(f)&&full.reclaim(f),"bounded table drains");
 check(!full.created({0,1,2,0})&&!full.created({1,0,2,0})&&!full.created({1,2,0,0})&&!full.created({1,2,3,3}),"invalid/unbound Worlds rejected");
 // Missing teardown before a new creation is a hard coverage failure.
 WorldLifetimes<3> missing;auto m=missing.adopt(server);auto other=missing.created(client);check(!missing.created(server)&&missing.faulted()&&!missing.alive(m)&&!missing.alive(other),"missed lifetime boundary fails closed for all publications");
 check(!missing.adopt(server)&&!missing.created(prediction)&&!missing.pin(m),"fault cannot silently resume by raw address");
 WorldLifetimes<2> conflict;auto k=conflict.created(client);auto changed=prediction;changed.registry++;
 check(!conflict.created(changed)&&conflict.faulted()&&!conflict.alive(k),"same root conflicting Registry fails closed");
 WorldLifetimes<2> changedOwner;auto v=changedOwner.adopt(server);auto identity=server;identity.kind=2;
 check(!changedOwner.adopt(identity)&&changedOwner.faulted()&&!changedOwner.alive(v),"adopt cannot silently change kind");
 WorldLifetimes<3> lateChild;auto lc=lateChild.adopt(client);check(lateChild.beginClose(lc)&&!lateChild.adopt(prediction)&&!lateChild.faulted(),"closing root rejects a previously unseen child");
 check(!lateChild.created(prediction)&&lateChild.faulted(),"new native child inside closing root indicates missing boundary");
 std::cout<<"PASS "<<checks<<" World epoch/late callback checks\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL after "<<checks<<": "<<e.what()<<'\n';return 1;}}
