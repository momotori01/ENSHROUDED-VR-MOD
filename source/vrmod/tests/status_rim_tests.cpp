#include <evr/status_rim.hpp>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
using namespace evr::game;
int main(){try{unsigned n=0;auto ck=[&](bool b){++n;if(!b)throw std::runtime_error(std::to_string(n));};
 // What the game holds while nothing is wrong (every run's first render_post_params_seen): black, alpha 1.
 ck(lowHealthRim({0,0,0,1})==0);
 // The constructor's value, and nothing at all.
 ck(lowHealthRim({1,1,1,1})==0&&lowHealthRim({0,0,0,0})==0);
 // Low health in the headset run game-20261013-105615: the style's red, the alpha pulsing 0.8 .. 1.
 ck(lowHealthRim({.64314f,.14902f,.14902f,.83137f})==.83137f&&lowHealthRim({.64314f,.14902f,.14902f,1})==1&&lowHealthRim({.64314f,.14902f,.14902f,.8f})==.8f);
 // Fading out: the colour stays, the alpha goes.
 ck(lowHealthRim({.64314f,.14902f,.14902f,.05f})==.05f&&lowHealthRim({.64314f,.14902f,.14902f,0})==0);
 // Another style's colour counts as well; values out of range are held.
 ck(lowHealthRim({.2f,.4f,.9f,.5f})==.5f&&lowHealthRim({.5f,.1f,.1f,3})==1&&lowHealthRim({.5f,.1f,.1f,-1})==0);
 ck(lowHealthRim({.5f,.1f,.1f,std::numeric_limits<float>::quiet_NaN()})==0&&lowHealthRim({std::numeric_limits<float>::infinity(),.1f,.1f,1})==0);
 std::cout<<"status_rim "<<n<<" PASS\n";return 0;}
 catch(const std::exception& e){std::cerr<<"status_rim FAIL "<<e.what()<<"\n";return 1;}}
