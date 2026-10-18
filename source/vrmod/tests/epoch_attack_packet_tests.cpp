#include <evr/epoch_attack_packet.hpp>
#include <thread>
#include <iostream>
#include <stdexcept>
using evr::motion::EpochAttackPacket;
int main(){unsigned checks=0;try{auto ck=[&](bool v){++checks;if(!v)throw std::runtime_error("epoch attack packet invariant");};
 for(unsigned n=0;n<1000;++n){EpochAttackPacket packet;ck(packet.load()==256);auto held=packet.publish(255);ck(uint8_t(held)==255&&(held>>8)==2);ck(packet.publish(255)==held);auto neutral=packet.stop();ck(uint8_t(neutral)==0&&(neutral>>8)==3);ck(packet.publish(255)==neutral&&packet.stop()==neutral);}
 for(unsigned n=0;n<100;++n){EpochAttackPacket packet;std::atomic<bool> stopped{false},bad{false};std::thread writer([&]{for(unsigned i=0;i<1000;++i){const bool after=stopped.load();const auto value=packet.publish((i&1)?255:0);if(after&&uint8_t(value))bad=true;}});std::thread closer([&]{packet.stop();stopped=true;});writer.join();closer.join();ck(!bad&&uint8_t(packet.load())==0);ck(packet.publish(255)==packet.load()&&uint8_t(packet.load())==0);}
 std::cout<<"PASS "<<checks<<" latched packet checks, 100000 concurrent publications\n";return 0;}catch(const std::exception& e){std::cerr<<"FAIL "<<checks<<' '<<e.what()<<'\n';return 1;}}
