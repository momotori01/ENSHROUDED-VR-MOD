#include <evr/attack_catalog.hpp>
#include <iostream>
using namespace evr::attacks;
// The 1.0 attack catalog (generated from the game data, docs/analysis/attacks_1_0_20261017): sorted for find(), and the
// actions the VR routes decide by have the class the game data gives them.
int main(){unsigned failed=0,passed=0;auto check=[&](bool ok,const char* label){std::cout<<(ok?"PASS ":"FAIL ")<<label<<'\n';ok?++passed:++failed;};
 bool sorted=true;for(size_t i=1;i<catalog.size();++i)sorted=sorted&&catalog[i-1].sequence<catalog[i].sequence;
 check(sorted,"sequences strictly ascending (binary search, no duplicate)");
 bool all=true;for(const auto& a:catalog)all=all&&find(a.sequence)==&a;
 check(all,"every row is found by its number");
 check(!find(0)&&!find(1808128177u),"an unknown number (the pickaxe's dig, not an attack) is not found");
 const auto sword=find(2496979158u);check(sword&&!sword->skill&&sword->weaponHits==3&&!sword->moving,"one-hand sword stroke 1: normal, three hits on the hand, no move");
 const auto axe3=find(2911403388u);check(axe3&&!axe3->skill&&axe3->moving,"great axe stroke 3: a normal attack with a push (withheld: normal attacks do not move)");
 const auto slash=find(2251110514u);check(slash&&slash->skill&&slash->weaponHits==1&&slash->moving,"slash up (sword): skill with one weapon hit and a jump");
 const auto crash=find(3198880426u);check(crash&&crash->skill&&!crash->weaponHits&&crash->otherHits==3,"crash down (sword): skill with three shock-wave hits and no weapon hit (left to the game)");
 const auto dodge=find(660765474u);check(dodge&&dodge->skill&&dodge->weaponHits==1&&dodge->lengthMs==1800,"dodge attack (sword): skill, one weapon hit, 1.8 s");
 unsigned skills=0;for(const auto& a:catalog)skills+=a.skill;check(catalog.size()==196&&skills==42,"196 player attacks, 42 skill attacks in the 1.0 data");
 std::cout<<passed<<" passed, "<<failed<<" failed\n";return failed?1:0;
}
