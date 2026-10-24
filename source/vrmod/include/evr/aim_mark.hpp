#pragma once
#include <evr/math.hpp>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

// The mark of where a bow, a staff or a wand aims.
// The game draws its reticle - a dot - in the middle of the HUD when a ranged weapon is drawn or aimed: where the
// camera looks. In the headset the shot leaves along the hands (bow_aim.hpp, ranged_aim.hpp), so that dot tells
// nothing about it (user after the headset run of 2026-10-13: "a dot comes up in the middle that has nothing to do
// with where the arrow flies - make it follow the right hand and be where I aim").
// VHVR (docs/analysis/vhvr_spec_20261010/ui_hud.md section 4, staff_throw.md 2.4): the game's own mark is switched
// off, and while a weapon aims, a mark stands in the world 128 m along the weapon's line, turned to the camera and
// scaled with the distance so that it always looks the same size (LocalWeaponWield.cs:27, 431-445). VHVR shows the
// bow's path as a line instead; the user decided against that line on 2026-10-11, so the bow gets the same mark here.
// Value logic only: the lines in the tracking space, the marked point, and where a point shows in an eye's image.
namespace evr::aim {
constexpr float markDistance=128.f;   // metres along the line
// What the mark's radius looks like from the eye, in radians. The game's dot is about 4 of the HUD's 1920 pixels,
// which are 2 m wide 1.5 m ahead: 0.28 degrees.
// (That was 0.005 - the radius taken for the dot's width, three and a half times the game's dot. User after the
// headset run game-20261013-141010: "the reticle is too large and stands out, make it about a third".)
constexpr float markRadius=.0017f;
struct Line {bool valid=false;Vec3 origin{},direction{};};
inline bool unit(Quat q) {const float n=q.x*q.x+q.y*q.y+q.z*q.z+q.w*q.w;return std::isfinite(n)&&std::abs(n-1)<.02f;}
// The bow while a hand holds its string: from that hand through the arrow rest, which sits 1.55 mm above the grip of
// the bow's model (bow_aim.hpp knownBowWorldAim takes the same two points into the game's world when the shot goes).
// model: the bow as shown, in the tracking space; scale: its model scale.
inline Line bowLine(const Pose& model,Vec3 scale,Vec3 stringHand) {
    Line l;
    if(!finite(model.position)||!unit(model.orientation)||!finite(scale)||!finite(stringHand)||scale.y<.5f||scale.y>1.5f)return l;
    const Vec3 rest=model.position+rotate(model.orientation,{0,.00155006f*scale.y,0});
    const Vec3 delta=rest-stringHand;const float span=length(delta);
    if(!std::isfinite(span)||span<.10f||span>1.5f)return l;
    l.valid=true;l.origin=rest;l.direction=delta*(1/span);return l;
}
// A staff or a wand (ranged_aim.hpp): the shot leaves the emitter on the weapon's long axis (model +Y, emitterY metres
// up at scale 1). Held in one hand it flies along the hand's pointer ray, held with both along the weapon.
inline Line rangedLine(const Pose& model,Vec3 scale,float emitterY,bool twoHanded,const Pose& pointer) {
    Line l;
    if(!finite(model.position)||!unit(model.orientation)||!finite(scale)||scale.y<.5f||scale.y>1.5f||!std::isfinite(emitterY)||emitterY<0||emitterY>3)return l;
    if(!twoHanded&&!unit(pointer.orientation))return l;
    const Vec3 axis=rotate(model.orientation,{0,1,0});
    const Vec3 direction=twoHanded?axis:rotate(pointer.orientation,{0,0,-1});const float size=length(direction);
    if(!finite(direction)||size<.5f||size>1.5f)return l;
    l.valid=true;l.origin=model.position+axis*(emitterY*scale.y);l.direction=direction*(1/size);return l;
}
// How far along its line the mark sits. The shot leaves the weapon along that line, but the eye is not on it: a
// mark 128 m out stands, seen from the eye, beside what the shot hits nearby - by the eye's distance from the line,
// 0.3 .. 0.5 m, which is 5 degrees and more for a hit 3 m away (headset run game-20261013-105615: "the hit is not
// where the dot is"; 15 of the 34 shots of that run went 27 degrees down, to the ground 3 m ahead). So the mark
// goes where the line is known to end: at the nearest creature it passes through (a body: an upright cylinder from
// its feet), else on the level ground of the player's own feet, else 128 m out. Level ground is a guess where the
// ground slopes, and walls are not known here.
// All in metres of one frame; groundY: the height of the player's feet (NaN: not known). by: 0 far, 1 ground, 2 body.
struct MarkBody {Vec3 foot{};float radius=.6f,height=2.2f;};
constexpr float markNearest=1.f;
inline float markReach(const Line& l,float groundY,const MarkBody* bodies,size_t count,int* by=nullptr) {
    if(by)*by=0;
    if(!l.valid||!finite(l.origin)||!finite(l.direction))return markDistance;
    const Vec3 o=l.origin,d=l.direction;float best=markDistance;int kind=0;
    const float level=d.x*d.x+d.z*d.z;
    for(size_t i=0;bodies&&i<count;++i){
        const auto& b=bodies[i];
        if(!finite(b.foot)||!(b.radius>0)||!(b.height>0))continue;
        const float bottom=b.foot.y,top=b.foot.y+b.height;float hit=-1;
        // The side: the line in the level plane against the circle.
        if(level>1e-8f){
            const float px=o.x-b.foot.x,pz=o.z-b.foot.z;const float half=(px*d.x+pz*d.z)/level,rest=(px*px+pz*pz-b.radius*b.radius)/level;
            const float under=half*half-rest;
            if(under>=0){const float t=-half-std::sqrt(under);const float y=o.y+d.y*t;if(t>0&&y>=bottom&&y<=top)hit=t;}
        }
        // The top, for a line that comes down onto it (and the bottom for one from below).
        if(hit<0&&std::abs(d.y)>1e-4f)for(const float plane:{top,bottom}){
            const float t=(plane-o.y)/d.y;if(!(t>0))continue;
            const float x=o.x+d.x*t-b.foot.x,z=o.z+d.z*t-b.foot.z;
            if(x*x+z*z<=b.radius*b.radius&&(hit<0||t<hit))hit=t;
        }
        if(hit>0&&(kind!=2||hit<best)){best=hit;kind=2;}
    }
    if(kind!=2&&std::isfinite(groundY)&&d.y<-1e-3f){const float t=(groundY-o.y)/d.y;if(t>0&&t<best){best=t;kind=1;}}
    best=std::clamp(best,markNearest,markDistance);
    if(by)*by=best>=markDistance?0:kind;
    return best;
}
inline Vec3 markPoint(const Line& l,float reach=markDistance) {return l.origin+l.direction*reach;}
// What else sits at the middle of the HUD goes with the mark (user, 2026-10-13: "let the stamina and charge gauges
// move with the reticle too - anything in the middle of the screen pulls the aim towards it"). While a ranged
// weapon aims the game draws there: its dot, the charge and stamina gauges around it, and to its right the count and
// name of the ammunition or spell (seen in the UI images of Mock run game-20261013-021932: up to 400 pixels right
// of the middle). That part of the UI image - the cluster - is taken out of the plate ahead of the head and shown at
// the aimed point instead, facing the eye and as large as it looked on the plate: the plate is submitted as the four
// parts around the cluster, the cluster as a quad of its own (OpenXR composition layers of the same UI image; the
// runtime gives it the depth of the aimed point, so it does not double against what is aimed at).
struct PixelRect {int32_t x0=0,y0=0,x1=0,y1=0;};
// The cluster in an image of this size: 200 pixels left of the middle to 600 right, 170 up and down, of a 1080 rows
// high image. (420 right and 120 up and down until 2026-10-24 - user after the headset: "the staff's spell switching is cut off
// and cannot be read; the arrows' too, probably". Bug reports 20261024-152947: the spell list, centred on the chosen spell's row,
// reached 544 pixels right and 93 up / 96 down, the rows 30 pixels apart, so a list of 6 spells reaches 150 up or down.)
inline PixelRect clusterRect(uint32_t width,uint32_t height) {
    const float s=float(height)/1080.f,cx=float(width)*.5f,cy=float(height)*.5f;
    PixelRect r{int32_t(cx-200*s),int32_t(cy-170*s),int32_t(cx+600*s),int32_t(cy+170*s)};
    r.x0=std::clamp(r.x0,1,int32_t(width)-2);r.x1=std::clamp(r.x1,r.x0+1,int32_t(width)-1);
    r.y0=std::clamp(r.y0,1,int32_t(height)-2);r.y1=std::clamp(r.y1,r.y0+1,int32_t(height)-1);
    return r;
}
// A part of the UI image as a quad: its pixels, the pose of its centre (x to the right, y up, seen from +z) and its
// size in metres.
struct QuadPart {PixelRect rect{};Pose pose=identityPose();float width=0,height=0;};
// The plate without the cluster: above it, below it, left and right of it. The poses are relative to the plate's
// centre; plateWidth metres hold imageWidth pixels.
inline std::array<QuadPart,4> plateAround(PixelRect cut,uint32_t imageWidth,uint32_t imageHeight,float plateWidth) {
    const float s=plateWidth/float(imageWidth),w=float(imageWidth),h=float(imageHeight);
    const auto part=[&](int32_t x0,int32_t y0,int32_t x1,int32_t y1){
        QuadPart p;p.rect={x0,y0,x1,y1};p.width=float(x1-x0)*s;p.height=float(y1-y0)*s;
        p.pose.position={(float(x0+x1)*.5f-w*.5f)*s,(h*.5f-float(y0+y1)*.5f)*s,0};return p;};
    return {part(0,0,int32_t(imageWidth),cut.y0),part(0,cut.y1,int32_t(imageWidth),int32_t(imageHeight)),
            part(0,cut.y0,cut.x0,cut.y1),part(cut.x1,cut.y0,int32_t(imageWidth),cut.y1)};
}
// The plate without one or two parts taken out (the aim cluster and the locked enemy's health bar, 2026-10-19): the image in
// rows cut at the parts' top and bottom edges, each row without the parts that cross it. The parts must not overlap (false);
// out holds up to 10 pieces (two parts at different heights: at most 5 rows of at most 3 pieces).
inline bool plateWithout(const PixelRect* cuts,size_t count,uint32_t imageWidth,uint32_t imageHeight,float plateWidth,std::array<QuadPart,10>& out,size_t& pieces) {
    pieces=0;if(!cuts||count==0||count>2||!imageWidth||!imageHeight)return false;
    const int32_t w=int32_t(imageWidth),h=int32_t(imageHeight);
    for(size_t i=0;i<count;++i){const auto& c=cuts[i];if(c.x0<0||c.y0<0||c.x1>w||c.y1>h||c.x1<=c.x0||c.y1<=c.y0)return false;}
    if(count==2){const auto& a=cuts[0];const auto& b=cuts[1];if(a.x0<b.x1&&b.x0<a.x1&&a.y0<b.y1&&b.y0<a.y1)return false;}
    std::array<int32_t,6> ys{0,h,0,0,0,0};size_t n=2;
    for(size_t i=0;i<count;++i){ys[n++]=cuts[i].y0;ys[n++]=cuts[i].y1;}
    std::sort(ys.begin(),ys.begin()+n);n=size_t(std::unique(ys.begin(),ys.begin()+n)-ys.begin());
    const float s=plateWidth/float(imageWidth);
    for(size_t r=0;r+1<n;++r){
        const int32_t y0=ys[r],y1=ys[r+1];if(y1<=y0)continue;
        std::array<std::pair<int32_t,int32_t>,2> taken{};size_t t=0;
        for(size_t i=0;i<count;++i)if(cuts[i].y0<=y0&&cuts[i].y1>=y1)taken[t++]={cuts[i].x0,cuts[i].x1};
        if(t==2&&taken[1].first<taken[0].first)std::swap(taken[0],taken[1]);
        int32_t x=0;
        const auto piece=[&](int32_t x0,int32_t x1){
            if(x1<=x0)return;auto& p=out[pieces++];p.rect={x0,y0,x1,y1};p.width=float(x1-x0)*s;p.height=float(y1-y0)*s;
            p.pose=identityPose();p.pose.position={(float(x0+x1)*.5f-float(w)*.5f)*s,(float(h)*.5f-float(y0+y1)*.5f)*s,0};};
        for(size_t i=0;i<t;++i){piece(x,taken[i].first);x=taken[i].second;}
        piece(x,w);
    }
    return pieces>0;
}
// A rotation from its three axes (unit, perpendicular, right-handed).
inline Quat fromAxes(Vec3 x,Vec3 y,Vec3 z) {
    const float trace=x.x+y.y+z.z;Quat q;
    if(trace>0){const float s=std::sqrt(trace+1)*2;q={(y.z-z.y)/s,(z.x-x.z)/s,(x.y-y.x)/s,.25f*s};}
    else if(x.x>y.y&&x.x>z.z){const float s=std::sqrt(1+x.x-y.y-z.z)*2;q={.25f*s,(y.x+x.y)/s,(z.x+x.z)/s,(y.z-z.y)/s};}
    else if(y.y>z.z){const float s=std::sqrt(1+y.y-x.x-z.z)*2;q={(y.x+x.y)/s,.25f*s,(z.y+y.z)/s,(z.x-x.z)/s};}
    else{const float s=std::sqrt(1+z.z-x.x-y.y)*2;q={(z.x+x.z)/s,(z.y+y.z)/s,.25f*s,(x.y-y.x)/s};}
    return normalized(q);
}
// The cluster towards the aimed point, in the tracking space: it faces the eye, upright; the image's middle - the
// reticle - lies on the line from the eye to the point; a pixel is as large as it looked on the plate (plateWidth
// metres for imageWidth pixels, plateDistance ahead), scaled with the distance. width 0: no place (the point is at
// the eye, or not finite).
// It stands at the point's own distance, but no further than clusterFarthest: a quad 128 m out and 55 m wide was
// submitted in the headset run game-20261013-105615 and was not seen (SteamVR; "the ammunition count and the charge
// gauge are not displayed any more"). At 20 m it is 0.2 degrees nearer to the eyes than the far mark.
constexpr float clusterFarthest=20.f;
// The same for any part of the image whose pixel (anchorX, anchorY) belongs on the line from the eye to the point: the game's
// spatial HUD (FbUiHUDSpatial - the locked enemy's health bar and lock-on arrow) drawn into the HUD image where the game's own
// third-person camera sees its anchor, taken out and set at the anchor's place in the world (2026-10-19, hooks/target_mark.hpp).
inline QuadPart partAt(PixelRect cut,float anchorX,float anchorY,uint32_t imageWidth,uint32_t imageHeight,float plateWidth,float plateDistance,Vec3 eye,Vec3 point) {
    QuadPart p;p.rect=cut;
    if(!finite(eye)||!finite(point)||!std::isfinite(anchorX)||!std::isfinite(anchorY)||!imageWidth||!imageHeight||!(plateWidth>0)||!(plateDistance>0))return p;
    const Vec3 back=eye-point;const float away=length(back);
    if(!std::isfinite(away)||away<.5f)return p;
    const Vec3 z=back*(1/away);Vec3 x=cross({0,1,0},z);
    if(length(x)<.05f)x={1,0,0};
    x=x*(1/length(x));const Vec3 y=cross(z,x);
    const float distance=std::min(away,clusterFarthest);const Vec3 place=eye-z*distance;
    const float s=plateWidth/float(imageWidth)*(distance/plateDistance);
    const float dx=(float(cut.x0+cut.x1)*.5f-anchorX)*s,dy=(anchorY-float(cut.y0+cut.y1)*.5f)*s;
    p.pose={fromAxes(x,y,z),place+x*dx+y*dy};p.width=float(cut.x1-cut.x0)*s;p.height=float(cut.y1-cut.y0)*s;
    return p;
}
inline QuadPart clusterAt(PixelRect cut,uint32_t imageWidth,uint32_t imageHeight,float plateWidth,float plateDistance,Vec3 eye,Vec3 point) {
    QuadPart p;p.rect=cut;
    if(!finite(eye)||!finite(point)||!imageWidth||!imageHeight||!(plateWidth>0)||!(plateDistance>0))return p;
    const Vec3 back=eye-point;const float away=length(back);
    if(!std::isfinite(away)||away<.5f)return p;
    const Vec3 z=back*(1/away);Vec3 x=cross({0,1,0},z);
    if(length(x)<.05f)x={1,0,0};                              // straight above or below: any level axis
    x=x*(1/length(x));const Vec3 y=cross(z,x);
    const float distance=std::min(away,clusterFarthest);const Vec3 place=eye-z*distance;
    const float s=plateWidth/float(imageWidth)*(distance/plateDistance);
    const float dx=(float(cut.x0+cut.x1)*.5f-float(imageWidth)*.5f)*s,dy=(float(imageHeight)*.5f-float(cut.y0+cut.y1)*.5f)*s;
    p.pose={fromAxes(x,y,z),place+x*dx+y*dy};p.width=float(cut.x1-cut.x0)*s;p.height=float(cut.y1-cut.y0)*s;
    return p;
}
// Where a point of the tracking space shows in one eye's image, and how many pixels the mark's radius takes there.
// eye: that eye's pose in the tracking space; the four angles: its field of view as OpenXR gives it (left and down
// negative); width, height: the image that field of view fills, rows running downwards.
struct EyePlace {bool on=false;float x=0,y=0,radius=0;};
inline EyePlace eyePlace(const Pose& eye,float angleLeft,float angleRight,float angleUp,float angleDown,Vec3 point,uint32_t width,uint32_t height,float radians=markRadius) {
    EyePlace e;
    if(!finite(eye.position)||!unit(eye.orientation)||!finite(point)||!width||!height||!(radians>0))return e;
    const float l=std::tan(angleLeft),r=std::tan(angleRight),u=std::tan(angleUp),d=std::tan(angleDown);
    if(!std::isfinite(l)||!std::isfinite(r)||!std::isfinite(u)||!std::isfinite(d)||!(r-l>.01f)||!(u-d>.01f))return e;
    const Pose back=inverse(eye);const Vec3 p=rotate(back.orientation,point)+back.position;
    if(!(p.z<-.1f))return e;                                   // behind the eye, or in it
    const float tx=p.x/-p.z,ty=p.y/-p.z;
    e.x=(tx-l)/(r-l)*float(width);e.y=(u-ty)/(u-d)*float(height);
    e.radius=std::max(2.f,radians*float(width)/(r-l));
    if(!std::isfinite(e.x)||!std::isfinite(e.y))return {};
    // Shown while any of it is inside the image.
    const float reach=e.radius+3;
    e.on=e.x>-reach&&e.y>-reach&&e.x<float(width)+reach&&e.y<float(height)+reach;
    return e;
}
}
