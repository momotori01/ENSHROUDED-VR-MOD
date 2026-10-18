#include <evr/motion_event_retime.hpp>
#include <iostream>
#include <stdexcept>
using namespace evr::motion;
int main(){unsigned checks=0;auto check=[&](bool v){++checks;if(!v)throw std::runtime_error("event ownership/deduplication invariant");};using D=EventRetime::Decision;
 EventActionKey key{123,1,27,99,456,2,0};EventIdentity event{999,{1,2,3}};
 NativeAttackTicket ticket{1,99,123,100,1,true};check(ticket.permits(key,250));check(ticket.permits(key,1100));check(!ticket.permits(key,1101));check(!ticket.permits(key,99));auto old=key;old.trigger=123;check(!ticket.permits(old,250));old=key;old.item=23;check(!ticket.permits(old,250));ticket.valid=false;check(!ticket.permits(key,250));
 EventRetime ledger;check(ledger.consider(key,event,true,false,2,16)==D::Ignore);check(ledger.consider(key,event,true,true,16,16)==D::Ignore);check(ledger.consider(key,event,false,true,0,16)==D::Ignore);check(ledger.consider(key,event,true,true,2,16)==D::Append);check(ledger.commit(key,event));check(!ledger.commit(key,event));
 for(unsigned i=0;i<1000;++i){check(ledger.consider(key,event,false,false,1,16)==D::Suppress);check(ledger.consider(key,event,true,true,0,16)==D::Ignore);}
 for(unsigned i=0;i<7;++i){auto foreign=key;if(i==0)++foreign.world;if(i==1)++foreign.entity;if(i==2)++foreign.sequence;if(i==3)++foreign.item;if(i==4)++foreign.trigger;if(i==5)++foreign.call;if(i==6)++foreign.sub;check(ledger.consider(foreign,event,false,true,1,16)==D::Ignore);check(!ledger.owns(foreign));}
 auto other=event;other.guid[15]^=1;check(ledger.consider(key,other,false,true,1,16)==D::Ignore);other=event;other.variant+=12;check(ledger.consider(key,other,false,true,1,16)==D::Ignore);
 EventRetime fresh;auto zero=key;zero.world=0;check(!fresh.commit(zero,event));check(fresh.consider(zero,event,true,true,0,16)==D::Ignore);
 ticket.valid=true;ticket.revoked=true;check(ticket.identifies(key,250));check(!ticket.permits(key,250));
 EventRetime blocked;check(blocked.block(key,event));check(blocked.blocked());check(!blocked.commit(key,event));
 for(unsigned i=0;i<1000;++i){check(blocked.consider(key,event,false,false,1,16)==D::Suppress);check(blocked.consider(key,event,true,true,0,16)==D::Ignore);}
 EventAuthority authority{key,event.guid,true};std::array<uint8_t,24> nativeEvent{};std::memcpy(nativeEvent.data(),&key.sequence,4);nativeEvent[4]=key.sub;std::memcpy(nativeEvent.data()+8,event.guid.data(),16);
 check(authority.matches(key.world,key.entity,key.item,key.trigger,nativeEvent));
 check(!authority.matches(key.world+1,key.entity,key.item,key.trigger,nativeEvent));check(!authority.matches(key.world,key.entity+1,key.item,key.trigger,nativeEvent));check(!authority.matches(key.world,key.entity,key.item+1,key.trigger,nativeEvent));check(!authority.matches(key.world,key.entity,key.item,key.trigger+1,nativeEvent));
 for(auto offset:{0,4,8,23}){auto changed=nativeEvent;changed[offset]^=1;check(!authority.matches(key.world,key.entity,key.item,key.trigger,changed));}
 authority.motion=false;check(!authority.matches(key.world,key.entity,key.item,key.trigger,nativeEvent));
 std::cout<<"PASS "<<checks<<" event retime ownership checks\n";
}
