#include <string>
#include <evr/query_return_completion.hpp>
#include <evr/scoped_query_transforms.hpp>
#include <array>
#include <chrono>
#include <future>
#include <iostream>
#include <stdexcept>
#include <thread>
using namespace evr::game;
using namespace std::chrono_literals;
namespace {
unsigned checks=0;
void check(bool ok){++checks;if(!ok)throw std::runtime_error("query return ordering check "+std::to_string(checks));}
// The drain reader runs on another thread while return processing is deliberately
// paused at the last native read, the ledger update, logging, and error handling.
void scenario(bool cancellation,unsigned failAt){
 std::atomic<unsigned> outstanding{0},closed{0},failures{0};
 std::array<std::promise<void>,4> entered,release;
 std::array<std::future<void>,4> seen,proceed;
 for(unsigned i=0;i<4;++i){seen[i]=entered[i].get_future();proceed[i]=release[i].get_future();}
 bool restored=false,ledgerReturned=false,reported=false;
 std::thread worker([&]{
  ScopedQueryTransforms borrow;ScopedQueryTransforms::Current current{};ScopedQueryTransforms::Previous previous{};
  ScopedQueryTransforms::Query q{{1,reinterpret_cast<uintptr_t>(current.data()),3,reinterpret_cast<uintptr_t>(previous.data()),5,6,7,8,9,10}};
  const auto original=q;if(!cancellation)borrow.open(q,current,previous);
  ++outstanding;
  QueryReturnCompletion completion(outstanding,closed);
  auto pause=[&](unsigned n){entered[n-1].set_value();proceed[n-1].wait();};
  try{
   restored=borrow.active()?borrow.close(q):q==original;completion.markReturned();
   pause(1);if(failAt==1)throw std::runtime_error("last component read");
   // Real source storage is still valid here. It must not have been retired.
   if(*reinterpret_cast<const uint8_t*>(q[1])!=0)throw std::runtime_error("source changed");
   pause(2);if(failAt==2)throw std::runtime_error("ledger update");ledgerReturned=true;
   pause(3);if(failAt==3)throw std::runtime_error("diagnostic sink");reported=true;
  }catch(...){pause(4);++failures;}
 });
 bool ordered=true;
 const unsigned phases=failAt?failAt:3;
 for(unsigned n=1;n<=phases;++n){
  ordered&=seen[n-1].wait_for(5s)==std::future_status::ready;
  ordered&=outstanding.load(std::memory_order_acquire)==1&&closed.load()==0;
  release[n-1].set_value();
 }
 if(failAt){ordered&=seen[3].wait_for(5s)==std::future_status::ready;ordered&=outstanding.load()==1&&closed.load()==0&&failures.load()==0;release[3].set_value();}
 worker.join();
 check(ordered);check(restored);check(outstanding.load(std::memory_order_acquire)==0&&closed.load()==1);
 check(failures.load()==unsigned(failAt!=0));check(ledgerReturned==(failAt==0||failAt==3));check(reported==(failAt==0));
}
void concurrent(){
 std::atomic<unsigned> outstanding{2},closed{0};std::array<std::promise<void>,2> ready,release;
 auto a=ready[0].get_future(),b=ready[1].get_future();auto p=release[0].get_future(),q=release[1].get_future();
 auto finish=[&](unsigned i,std::future<void>& go){QueryReturnCompletion completion(outstanding,closed);completion.markReturned();ready[i].set_value();go.wait();};
 std::thread one(finish,0,std::ref(p)),two(finish,1,std::ref(q));a.wait();b.wait();
 const bool before=outstanding.load()==2&&closed.load()==0;
 release[0].set_value();one.join();const bool partial=outstanding.load()==1&&closed.load()==1;
 release[1].set_value();two.join();check(before);check(partial);check(outstanding.load()==0&&closed.load()==2);
}
}
int main(){try{
 for(bool cancel:{false,true})for(unsigned failAt=0;failAt<=3;++failAt)scenario(cancel,failAt);
 concurrent();
 // Wrong-query exceptions leave the actual borrow and drain receipt alive.
 std::atomic<unsigned> outstanding{1},closed{0};ScopedQueryTransforms borrow;
 ScopedQueryTransforms::Current current{};ScopedQueryTransforms::Previous previous{};
 ScopedQueryTransforms::Query q{{1,2,3,4,5,6,7,8,9,10}},wrong=q;borrow.open(q,current,previous);
 bool rejected=false;
 {QueryReturnCompletion completion(outstanding,closed);try{borrow.close(wrong);completion.markReturned();}catch(const std::logic_error&){rejected=true;}}
 check(rejected&&borrow.active()&&outstanding.load()==1&&closed.load()==0);
 {QueryReturnCompletion completion(outstanding,closed);check(borrow.close(q));completion.markReturned();completion.markReturned();}
 check(!borrow.active()&&outstanding.load()==0&&closed.load()==1);
 std::cout<<"PASS "<<checks<<" query return concurrency/error checks\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
