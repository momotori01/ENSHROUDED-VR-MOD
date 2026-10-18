#include <evr/capture_burst.hpp>
#include <iostream>
#include <thread>
using evr::diagnostic::CaptureBurst;
int main(){unsigned passed=0,failed=0;auto check=[&](bool b,const char* s){std::cout<<(b?"PASS ":"FAIL ")<<s<<'\n';b?++passed:++failed;};
 CaptureBurst b;check(!b.take(),"empty request does not capture");check(b.request(0)==2&&b.request(33)==2&&b.request(0xffffffff)==2,"invalid or unbounded request rejected");
 check(b.request(32)==0&&b.request(1)==1,"one bounded request accepted");bool ordered=true;for(unsigned n=1;n<=32;++n){auto f=b.take();ordered&=f.index==n&&f.total==32;}check(ordered,"exactly32 ordered paired readbacks");
 check(!b.take()&&b.request(1)==1,"exhaustion does not wrap or allow unbounded rearming");check(b.reset()==0&&b.request(3)==0,"new session can request again");b.take();check(b.reset()==2&&!b.take(),"shutdown cancels only pending captures");
 bool one=true;for(unsigned n=0;n<128;++n){b.reset();int a=-1,c=-1;std::thread t([&]{a=b.request(17);});std::thread u([&]{c=b.request(31);});t.join();u.join();one&=(a==0&&c==1)||(a==1&&c==0);const unsigned expected=a==0?17:31;unsigned count=0;while(auto f=b.take()){++count;one&=f.total==expected&&f.index==count;}one&=count==expected;}check(one,"concurrent requests cannot mix total and remaining counts");
 b.reset();check(b.request(8,0)==2&&b.request(8,5)==2,"manual budget must be between one and four");
 bool bounded=true;for(unsigned request=0;request<4;++request){bounded&=b.request(32,4)==0&&b.request(1,4)==1;for(unsigned n=1;n<=32;++n){auto f=b.take();bounded&=f.index==n&&f.total==32;}bounded&=!b.take();}
 check(bounded&&b.request(1,4)==1&&b.request(1)==1,"four explicit bursts stop at128 pairs without changing default rearm policy");
 b.reset();bool concurrent=true;for(unsigned request=0;request<4;++request){int a=-1,c=-1;std::thread t([&]{a=b.request(1,4);});std::thread u([&]{c=b.request(2,4);});t.join();u.join();concurrent&=(a==0&&c==1)||(a==1&&c==0);while(b.take()){};}
 check(concurrent&&b.request(1,4)==1,"concurrent manual requests cannot exceed completed request budget");
 std::cout<<passed<<" PASS / "<<failed<<" FAIL\n";return failed?1:0;
}
