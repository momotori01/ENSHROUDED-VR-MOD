#pragma once
// Snap turn (2026-11-01, user: "can snap turn be added? changeable in the game too, the angle set freely with a slider, 30
// degrees a step by default"). After VHVR's SnapTurnEnabled (docs/analysis/vhvr_spec_20261010/locomotion_input.md 4; off by
// default there, as here), in its instant form (SmoothSnapTurn false): the right stick's left / right no longer turns the view
// smoothly; pushed to the side it turns once by the angle, and again only after the stick has come back towards the middle -
// held over, it does not repeat. A push more up or down than to the side (sprint, dodge) does not turn.
// The turn itself is the camera hook's (hooks/camera_control_observer.hpp snapRequest): the angle is added to the game's own
// turn input of one camera update and to the turn the VR view follows, so the character and the view turn together.
#include <cmath>
namespace evr::motion {
struct SnapTurnSettings {float fire=.7f,rearm=.35f;};
class SnapTurn {
    bool armed_=false;   // a stick already pushed when the snap turn begins waits for its return
public:
    // x, y: the right stick (x + to the right, y + up). allowed: the stick turns the view now (play, no menu). Returns -1 for a
    // turn to the left, +1 to the right, 0 for none.
    int update(float x,float y,bool allowed,const SnapTurnSettings& s={}){
        if(!std::isfinite(x)||!std::isfinite(y)){armed_=false;return 0;}
        const float a=std::abs(x);
        if(a<s.rearm)armed_=true;
        if(!allowed){if(a>=s.rearm)armed_=false;return 0;}
        if(!armed_||a<s.fire||a<=std::abs(y))return 0;
        armed_=false;return x>0?1:-1;
    }
    bool armed()const{return armed_;}
};
// The angle a step: 10 .. 90 degrees in steps of 5 (VHVR 0 .. 90, 45 by default; here 30, the user's).
constexpr int snapAngleLow=10,snapAngleHigh=90,snapAngleStep=5,snapAngleDefault=30;
inline int clampSnapAngle(int degrees){
    const int stepped=int(std::lround(double(degrees)/snapAngleStep))*snapAngleStep;
    return stepped<snapAngleLow?snapAngleLow:stepped>snapAngleHigh?snapAngleHigh:stepped;
}
// The game's turn input of a camera update for a turn to the right of `degrees`: the right stick to the right gives a
// negative weighted yaw (headset logs 2026-10-22..26: right_x +30726 -> -0.086 a camera update, -31990 -> +0.074), in radians
// (the heading realignment adds its correction there in radians, include/evr/camera_mode.hpp headingCorrection).
inline float snapTurnInput(int direction,int degrees){
    return direction==0?0.f:float(-direction)*float(clampSnapAngle(degrees))*3.14159265f/180.f;
}
}
