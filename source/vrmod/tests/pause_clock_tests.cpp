#include <evr/pause_clock.hpp>
#include <iostream>
#include <stdexcept>
#include <string>
using namespace evr::game;
int main(){try{unsigned n=0;auto ck=[&](bool b){++n;if(!b)throw std::runtime_error(std::to_string(n));};
 PauseClock c;
 // Never paused: the plain age.
 ck(c.fresh(1000,1000,500)&&c.fresh(1000,1500,500)&&!c.fresh(1000,1501,500));
 ck(!c.fresh(0,1000,500)&&!c.fresh(2000,1000,500));                       // never taken; taken in the future
 ck(!c.still(1000,500));
 // The pause begins at 10000: what was fresh then stays fresh, however long the pause lasts.
 c.update(false,9000);c.update(true,10000);ck(c.paused&&c.pausedAt==10000&&c.still(10000,500)&&c.still(999999,500));
 ck(c.fresh(9900,10400,500)&&c.fresh(9900,60000,500)&&c.fresh(9500,60000,500));
 ck(!c.fresh(9499,60000,500));                                            // already too old when the pause began
 ck(c.fresh(9499,60000,1000));                                            // each consumer keeps its own limit
 ck(c.fresh(10030,60000,500));                                            // an observer that ran once more just after the pause began
 c.update(true,20000);ck(c.pausedAt==10000);                              // staying paused does not move the moment
 // The pause ends at 70000: the old samples hold for the grace, new ones by their plain age.
 c.update(false,70000);ck(!c.paused&&c.resumedAt==70000&&c.still(70000,500)&&c.still(70500,500)&&!c.still(70501,500));
 ck(c.fresh(9900,70300,500)&&!c.fresh(9900,70501,500));
 ck(c.fresh(70100,70400,500)&&c.fresh(70100,70600,500)&&!c.fresh(70100,70601,500));
 ck(c.fresh(9900,70300,500,1000)&&c.fresh(9900,71000,500,1000)&&!c.fresh(9900,71001,500,1000));
 // A second pause: the moment is the new one.
 c.update(true,80000);ck(c.pausedAt==80000&&!c.fresh(9900,90000,500)&&c.fresh(79800,90000,500));
 std::cout<<n<<" PASS\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
