#include <evr/mock_navigation.hpp>
#include <iostream>
int main(){
 using namespace evr::mock; unsigned fail=0;
 auto check=[&](bool ok,const char* name){std::cout<<(ok?"PASS ":"FAIL ")<<name<<'\n';fail+=!ok;};
 NavigationState s;
 const std::string move="EVR_MOCK_NAV_1 1 10000 2000 30 -20 0 1 0 1";
 check(s.update(move,10100,500)&&s.active(500)&&s.active(2399)&&!s.active(2400),"delayed command expires at original deadline");
 check(!s.update(move,10200,1000)&&!s.active(2400),"same revision never restarts movement");
 check(!s.update("broken",10200,1001)&&!s.active(1001)&&s.command.yaw==30,"malformed input releases movement and retains head");
 check(!s.update(move,10200,1002)&&!s.active(1002),"old file cannot reactivate after invalid input");
 check(!s.update("EVR_MOCK_NAV_1 2 9000 2000 0 0 0 1 0 0",12000,3000)&&!s.active(3000),"stale command rejected");
 check(!s.update("EVR_MOCK_NAV_1 3 13000 2000 0 0 0 1 0 0",12000,3001),"future command rejected");
 check(s.update("EVR_MOCK_NAV_1 4 12000 0 90 -45 0 0 0 0",12000,3002)&&!s.active(3002)&&s.command.yaw==90,"head-only command has no button pulse");
 NavigationCommand c;bool invalid=true;
 for(const auto* line:{"EVR_MOCK_NAV_1 -1 10000 100 0 0 0 0 0 0","EVR_MOCK_NAV_1 5 10000 2001 0 0 0 0 0 0","EVR_MOCK_NAV_1 5 10000 100 nan 0 0 0 0 0","EVR_MOCK_NAV_1 5 10000 100 0 86 0 0 0 0","EVR_MOCK_NAV_1 5 10000 100 0 0 2 0 0 0","EVR_MOCK_NAV_1 5 10000 100 0 0 0 0 0 2","EVR_MOCK_NAV_1 5 10000 100 0 0 0 0 0 0 extra"})invalid&=!parseNavigation(line,c);
 check(invalid,"bad ranges, nonfinite data, negative sequence and extra fields rejected");
 check(s.update("EVR_MOCK_NAV_1 5 12000 2000 0 0 1 0 0 0",12000,4000)&&!s.active(3999)&&!s.active(6000),"clock boundary cannot extend hold");
 return fail?1:0;
}
