#pragma once
#include <atomic>
namespace evr::game {
// An already-counted native iteration stays outstanding through its final
// component read, ledger update and diagnostic/error handling. This is a MOD
// drain receipt, not a pin on native memory. Declare outside the try/catch.
class QueryReturnCompletion {
public:
 QueryReturnCompletion(std::atomic<unsigned>& outstanding,std::atomic<unsigned>& closed) noexcept
  :outstanding_(outstanding),closed_(closed){}
 QueryReturnCompletion(const QueryReturnCompletion&)=delete;
 QueryReturnCompletion& operator=(const QueryReturnCompletion&)=delete;
 void markReturned() noexcept {returned_=true;}
 bool returned()const noexcept{return returned_;}
 ~QueryReturnCompletion(){
  // A wrong stack query must remain outstanding; never claim restoration.
  if(returned_){closed_.fetch_add(1,std::memory_order_relaxed);outstanding_.fetch_sub(1,std::memory_order_release);}
 }
private:
 std::atomic<unsigned>& outstanding_;std::atomic<unsigned>& closed_;bool returned_=false;
};
}
