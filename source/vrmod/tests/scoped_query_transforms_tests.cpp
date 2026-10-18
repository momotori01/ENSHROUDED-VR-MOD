#include <string>
#include <evr/scoped_query_transforms.hpp>
#include <iostream>
using evr::game::ScopedQueryTransforms;
int main(){unsigned n=0;try{
 auto check=[&](bool v){++n;if(!v)throw std::runtime_error("scoped query invariant "+std::to_string(n));};
 ScopedQueryTransforms a;ScopedQueryTransforms::Current current{};ScopedQueryTransforms::Previous last{};
 for(unsigned i=0;i<current.size();++i)current[i]=uint8_t(i*3);for(unsigned i=0;i<last.size();++i)last[i]=uint8_t(i*7);
 for(unsigned iteration=0;iteration<1000;++iteration){
  ScopedQueryTransforms::Query q{{1,reinterpret_cast<uintptr_t>(current.data()),3,iteration%2?reinterpret_cast<uintptr_t>(last.data()):0,5,6,7,8,9,10}};auto original=q;
  a.open(q,current,last);check(a.active());check(q[1]!=original[1]);check(a.current()==current);check(a.previous()==last);check((q[3]==0)==(original[3]==0));
  for(unsigned i=0;i<q.size();++i)if(i!=1&&i!=3)check(q[i]==original[i]);
  bool rejected=false;try{a.open(q,current,last);}catch(const std::logic_error&){rejected=true;}check(rejected);
  auto other=q;rejected=false;try{a.close(other);}catch(const std::logic_error&){rejected=true;}check(rejected&&a.active());
  check(a.close(q));check(!a.active()&&q==original);check(current[10]==30&&last[10]==70);
 }
 ScopedQueryTransforms::Query q{{1,2,3,4,5,6,7,8,9,10}},original=q;a.open(q,current,last);q[4]=100;check(!a.close(q));check(q[1]==original[1]&&q[3]==original[3]&&q[4]==100);
 q[1]=0;bool rejected=false;try{a.open(q,current,last);}catch(const std::logic_error&){rejected=true;}check(rejected&&!a.active());
 std::cout<<"PASS "<<n<<" scoped query ownership checks\n";return 0;
 }catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
