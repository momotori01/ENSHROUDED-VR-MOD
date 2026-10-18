#pragma once
// The controllers' buttons as the game's menu keys, while only the flat picture is shown in the headset
// (layer/screen_xr.hpp; user, 2026-10-14: "it must be possible to start from the motion controllers by button input",
// "from the start, consistently operable in VR is a must").
// The game's menus take the arrow keys, SPACE as "confirm" and ESC as "back" (its own hints: "SPACE 確定 / ESC 戻る").
//   a stick pushed beyond 0.6 = that arrow, once; held: again after 400 ms, then every 150 ms; let go below 0.4
//   A or X = SPACE, B or Y = ESC: down with the button, up with it
// An arrow is a tap: down, and up 80 ms later (a down and an up handed over together can be missed).
// Value logic only: what tells the game is in the layer.
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>
namespace evr::screen {
enum class Key : uint8_t {Up, Down, Left, Right, Confirm, Back};
struct KeyEvent {Key key;bool down;};
inline bool operator==(const KeyEvent& a,const KeyEvent& b){return a.key==b.key&&a.down==b.down;}
// What the two hands give together: the stick that is pushed further, a face button of either hand.
struct Pad {float x=0,y=0;bool confirm=false,back=false;};
inline Pad pad(float leftX,float leftY,bool leftOn,float rightX,float rightY,bool rightOn,bool confirm,bool back){
    Pad p;p.confirm=confirm;p.back=back;
    const auto finite=[](float a,float b){return std::isfinite(a)&&std::isfinite(b);};
    const float l=leftOn&&finite(leftX,leftY)?leftX*leftX+leftY*leftY:-1,r=rightOn&&finite(rightX,rightY)?rightX*rightX+rightY*rightY:-1;
    if(l<0&&r<0)return p;
    if(l>=r){p.x=leftX;p.y=leftY;}else{p.x=rightX;p.y=rightY;}
    return p;
}
class Keys {
    struct Arrow {bool held=false,pressed=false;uint64_t again=0,up=0;};
    std::array<Arrow,4> arrows{};
    bool confirm=false,back=false;
  public:
    static constexpr float push=.6f,rest=.4f;
    static constexpr uint64_t firstRepeat=400,repeat=150,tap=80;
    std::vector<KeyEvent> update(const Pad& p,uint64_t ms){
        std::vector<KeyEvent> out;
        // Only the stronger axis counts, so a stick pushed up and a little sideways is "up" alone.
        const bool vertical=std::abs(p.y)>=std::abs(p.x);
        const float value[4]={vertical?p.y:0,vertical?-p.y:0,vertical?0:-p.x,vertical?0:p.x};
        for(unsigned i=0;i<4;i++){
            auto& a=arrows[i];const auto key=Key(i);
            if(a.pressed&&ms>=a.up){a.pressed=false;out.push_back({key,false});}
            const bool now=a.held?value[i]>rest:value[i]>push;
            if(now&&!a.held){a.held=true;a.again=ms+firstRepeat;if(!a.pressed){a.pressed=true;a.up=ms+tap;out.push_back({key,true});}}
            else if(now&&ms>=a.again&&!a.pressed){a.again=ms+repeat;a.pressed=true;a.up=ms+tap;out.push_back({key,true});}
            else if(!now)a.held=false;
        }
        if(p.confirm!=confirm){confirm=p.confirm;out.push_back({Key::Confirm,confirm});}
        if(p.back!=back){back=p.back;out.push_back({Key::Back,back});}
        return out;
    }
    // Every key that is still down gets its release (the plate goes: the world's eyes take over, or the game closes).
    std::vector<KeyEvent> release(){
        std::vector<KeyEvent> out;
        for(unsigned i=0;i<4;i++){if(arrows[i].pressed)out.push_back({Key(i),false});arrows[i]={};}
        if(confirm){confirm=false;out.push_back({Key::Confirm,false});}
        if(back){back=false;out.push_back({Key::Back,false});}
        return out;
    }
    bool anyDown() const {for(const auto& a:arrows)if(a.pressed)return true;return confirm||back;}
};
// The plate goes back ahead of the player when they have turned away from it for a while (they stood facing
// elsewhere when the game started, or turned round): away = more than 75 degrees between where the head looks, level,
// and the direction to the plate's middle, for 1.5 s without a break.
class Recentre {
    uint64_t since=0;
  public:
    static constexpr float cosine=.2588f;   // cos 75 degrees
    static constexpr uint64_t hold=1500;
    bool update(float towardPlateX,float towardPlateZ,float aheadX,float aheadZ,uint64_t ms){
        const float a=std::sqrt(towardPlateX*towardPlateX+towardPlateZ*towardPlateZ),b=std::sqrt(aheadX*aheadX+aheadZ*aheadZ);
        if(!std::isfinite(a)||!std::isfinite(b)||a<1e-4f||b<.2f){since=0;return false;}   // looking straight up or down: no level direction
        const bool away=(towardPlateX*aheadX+towardPlateZ*aheadZ)/(a*b)<cosine;
        if(!away){since=0;return false;}
        if(!since){since=ms?ms:1;return false;}
        if(ms-since<hold)return false;
        since=0;return true;
    }
};
// The plate also goes back level with the eyes (2026-10-19, a friend's headset run with Virtual Desktop: "the start
// menu is shown far down"; the plate was placed 1.2 s after the start at the head of that moment - 57 degrees down,
// the headset not yet on - and turning was the only thing that moved it again):
//  - the head is more than 25 cm above or below the plate's middle for 1 s without a break (the headset was put on,
//    the player stood up or sat down);
//  - the plate was placed while the head looked more than 40 degrees up or down (no one reads a plate that way: the
//    headset lay on a desk or hung at the neck), and the head looks within 30 degrees of level for 0.5 s.
class Level {
    uint64_t since=0;bool steepPlaced=false;
  public:
    static constexpr float metres=.25f;
    static constexpr uint64_t hold=1000,holdLevel=500;
    static constexpr float steepSine=.6428f,levelSine=.5f;   // sin 40, sin 30 degrees
    // aheadY: the vertical part of where the head looks (unit vector); call placed() whenever the plate is placed.
    void placed(float aheadY){steepPlaced=std::isfinite(aheadY)&&std::abs(aheadY)>steepSine;since=0;}
    bool update(float headAbovePlate,float aheadY,uint64_t ms){
        if(!std::isfinite(headAbovePlate)||!std::isfinite(aheadY)){since=0;return false;}
        const bool off=std::abs(headAbovePlate)>metres,looksLevel=std::abs(aheadY)<levelSine;
        const bool wanted=off||(steepPlaced&&looksLevel);
        if(!wanted){since=0;return false;}
        if(!since){since=ms?ms:1;return false;}
        if(ms-since<(off?hold:holdLevel))return false;
        since=0;return true;
    }
};
} // namespace evr::screen
