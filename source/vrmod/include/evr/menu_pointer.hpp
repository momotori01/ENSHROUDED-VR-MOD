#pragma once
#include <evr/math.hpp>
#include <algorithm>
#include <cmath>
#include <cstdint>

// VHVR's pointer on the 2D GUI (docs/analysis/vhvr_spec_20261010/ui_hud.md, section 2). One hand's ray, used only while
// the game shows its cursor. The cursor's place is the ray cut with the plane of the GUI plate - not a physics hit, so
// it stays exact while the hand moves fast. The trigger is the left mouse button, B the right one. A click has some
// slack, so that pulling the trigger does not move the cursor off the button or start a drag. A press that the game was
// told about is always followed by its release, also when the ray has gone in between.
// Value logic only: what moves the game's cursor and tells it about the buttons is in hooks.
namespace evr::menu {
// The GUI plate as it is submitted (openxr_client.hpp, XrCompositionLayerQuad): the pose of its centre - x to the
// right, y up, seen from its +z side - in the space the ray is given in, its size in metres and that of its image.
struct Plate {
    Pose pose=identityPose();
    float width=2.f,height=1.125f;
    uint32_t pixelsX=1920,pixelsY=1080;
};
struct Spot {
    bool on=false;         // the ray comes from the front and meets the plane inside the plate
    float x=0,y=0;         // on the plate, metres from its centre (right, up), kept inside the plate
    int pixelX=0,pixelY=0; // in the image, from its top left corner, kept inside the image
};
// VRGUI.cs:751-764 and 1303-1308: the ray taken into the plate's own frame and cut with its plane z = 0; then
// (x + 0.5, y + 0.5) kept in 0..1, times the resolution. The image's rows run downwards.
inline Spot spot(const Plate& plate,Vec3 origin,Vec3 direction) {
    Spot s;
    if(!finite(origin)||!finite(direction)||!finite(plate.pose.position)||!(plate.width>0)||!(plate.height>0)||!plate.pixelsX||!plate.pixelsY)return s;
    const Pose back=inverse(plate.pose);
    const Vec3 o=rotate(back.orientation,origin)+back.position,d=rotate(back.orientation,direction);
    if(!(o.z>0)||!(d.z<-1e-6f))return s; // from behind, or along the plate
    const float t=-o.z/d.z,x=o.x+d.x*t,y=o.y+d.y*t;
    if(!std::isfinite(x)||!std::isfinite(y))return s;
    s.on=std::abs(x)<=plate.width*.5f&&std::abs(y)<=plate.height*.5f;
    s.x=std::clamp(x,-plate.width*.5f,plate.width*.5f);s.y=std::clamp(y,-plate.height*.5f,plate.height*.5f);
    const float u=s.x/plate.width+.5f,v=s.y/plate.height+.5f;
    s.pixelX=std::clamp(int(u*float(plate.pixelsX)),0,int(plate.pixelsX)-1);
    s.pixelY=std::clamp(int((1.f-v)*float(plate.pixelsY)),0,int(plate.pixelsY)-1);
    return s;
}
// VHVR LockGuiPositionWhenMenuOpen (ui_hud.md 1.3, row 3; on by default): while a shop, the inventory, a menu or the
// map is open the plate keeps its place against the rig - walking and turning by the stick take it along, turning
// the head does not. VHVR's plate stands upright at all times (it turns about the vertical only). The user after the
// headset run of 2026-10-13: "once the menu is out it should be fixed in space, not follow the head".
// Here the plate follows the head during play (the HUD). So when a menu opens, the place it gets is the one it had
// that moment, made upright: 1.5 m ahead of the head's place, in the level direction the head looks. A head that
// looks straight up or down has no level direction; then the top of the head tells it (it points ahead when the face
// points down).
// The result is the frozen origin for the plate (openxr_client.hpp UiQuadPlacement::origin, LOCAL space): the plate's
// centre is compose(origin, {identity, (0, 0, -distance)}).
inline Pose plateOrigin(const Pose& head) {
    if(!finite(head.position))return identityPose();
    const Vec3 ahead=rotate(head.orientation,{0,0,-1}),top=rotate(head.orientation,{0,1,0});
    Vec3 level{ahead.x,0,ahead.z};
    if(std::sqrt(level.x*level.x+level.z*level.z)<.2f)level=ahead.y<0?Vec3{top.x,0,top.z}:Vec3{-top.x,0,-top.z};
    const float size=std::sqrt(level.x*level.x+level.z*level.z);
    if(!std::isfinite(size)||size<1e-4f)return {{0,0,0,1},head.position};
    const float yaw=std::atan2(-level.x,-level.z);
    return {{0,std::sin(yaw*.5f),0,std::cos(yaw*.5f)},head.position};
}
// The plate of a frozen origin, for spot(): the ray is then given in the LOCAL space.
inline Plate fixedPlate(const Pose& origin,float distance,float width,float height,uint32_t pixelsX,uint32_t pixelsY) {
    Plate p;p.pose=compose(origin,Pose{{0,0,0,1},{0,0,-distance}});p.width=width;p.height=height;p.pixelsX=pixelsX;p.pixelsY=pixelsY;return p;
}
// VRGUI.cs:152-161, 766-785: from the press of the left button on, the cursor stays where it was until the ray has
// moved on the plate by more than 5 mm and by more than 16 pixels - both. The metres keep a small plate in the hand
// from being too touchy, the pixels keep a large one from leaving the slack before the game's own drag threshold.
struct ClickSlack {
    float metres=.005f;
    int pixels=16;
    bool held=false,loose=true;
    Spot anchor{};
    Spot filter(const Spot& now,bool leftDown) {
        if(!leftDown){held=false;loose=true;return now;}
        if(!held){held=true;loose=false;anchor=now;return now;}
        if(loose)return now;
        const float dx=now.x-anchor.x,dy=now.y-anchor.y;
        const float px=float(now.pixelX-anchor.pixelX),py=float(now.pixelY-anchor.pixelY);
        if(std::sqrt(dx*dx+dy*dy)>metres&&std::sqrt(px*px+py*py)>float(pixels)){loose=true;return now;}
        return anchor;
    }
};
// What the game is told about the mouse buttons. SteamVR_LaserPointer.cs:131-156: a button that is down when the ray
// comes on does not count until both are up. LaserPointerChords.cs:154-185: a press that was handed over gets its
// release when the button comes up, with the ray or without.
struct Buttons {
    struct Change {
        bool leftDown=false,leftUp=false,rightDown=false,rightUp=false;
        bool any() const {return leftDown||leftUp||rightDown||rightUp;}
    };
    bool ray=false,wait=false;
    bool left=false,right=false; // what the game has been told
    Change update(bool rayNow,bool trigger,bool secondary) {
        Change c;
        if(rayNow&&!ray)wait=trigger||secondary;
        ray=rayNow;
        if(wait&&!trigger&&!secondary)wait=false;
        const bool use=ray&&!wait;
        if(use&&trigger&&!left){left=true;c.leftDown=true;}
        else if(left&&!trigger){left=false;c.leftUp=true;}
        if(use&&secondary&&!right){right=true;c.rightDown=true;}
        else if(right&&!secondary){right=false;c.rightUp=true;}
        return c;
    }
    // VRControls.cs:468-479: the trigger of the hand with the ray is not the game's "use" while the ray is out, nor
    // while a click of it is still down.
    bool takesTrigger() const {return ray||left;}
    bool takesSecondary() const {return ray||right;}
};
}
