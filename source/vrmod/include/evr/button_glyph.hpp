#pragma once
// The VR button pictures that replace the game's (2026-10-21, the user: "LT RT LA LB LG RG ... Index and Quest told
// apart"; docs/analysis/vr_button_glyphs_20261021): a light rounded plate with a dark rim and a dark label, in the
// game's own form - R8G8B8A8 with premultiplied alpha (every colour value at most its alpha, as all 83 of the game's
// pictures), the size of the picture it replaces. Drawn with GDI at four times the size and averaged down.
#include <windows.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>
namespace evr::glyph {
// The label that is drawn as a swing (no letters).
inline const std::wstring swingLabel=L"<swing>";
// The label drawn as a smaller swing over "RG": a swing with the right grip held (a melee weapon's special ability).
inline const std::wstring gripSwingLabel=L"<RG+swing>";
// round: a circle instead of the rounded square (2026-10-21, the user: "the A / B kind round, so they are told apart from
// the square ones - the grips and the like").
struct Style {uint8_t plate=246,ink=34;float rim=.05f,margin=.06f,radius=.24f,text=.56f;bool round=false;};
// The face buttons (Index LA LB RA RB, Quest X Y A B) are drawn round.
inline bool faceLabel(const std::wstring& l){return l==L"RA"||l==L"RB"||l==L"LA"||l==L"LB"||l==L"A"||l==L"B"||l==L"X"||l==L"Y";}
// A stick (2026-10-26, user after the headset: "the letters of the icons are too small to read - at that size two letters, three at
// the most; work on the icon's shape and keep to two letters: the right stick up could be a circle with RS and a small upward
// triangle at its top right"): a round plate with the stick's two letters (RS, LS) and a small round badge at its top right holding
// the direction - a triangle up, down, left or right, two for up and down or left and right - or a dot for the press.
enum class StickMark : uint8_t {None,Up,Down,Left,Right,Vertical,Horizontal,Press};
inline std::wstring stickLabel(const std::wstring& stick,StickMark mark){
    static const wchar_t marks[]=L"-UDLRVHP";return L"<stick:"+stick+L":"+std::wstring(1,marks[size_t(mark)])+L">";}
inline bool stickOf(const std::wstring& l,std::wstring& stick,StickMark& mark){
    if(l.size()!=12||l.compare(0,7,L"<stick:")!=0||l[9]!=L':'||l[11]!=L'>')return false;
    const auto k=std::wstring(L"-UDLRVHP").find(l[10]);if(k==std::wstring::npos)return false;
    stick=l.substr(7,2);mark=StickMark(k);return true;}
// width x height x 4 bytes, rows top down.
inline std::vector<uint8_t> paint(uint32_t width,uint32_t height,const std::wstring& asked,const Style& askedStyle={}){
    if(width<8||height<8||width>512||height>512)throw std::invalid_argument("glyph size");
    std::wstring stick;StickMark mark=StickMark::None;const bool isStick=stickOf(asked,stick,mark);
    const std::wstring& label=isStick?stick:asked;Style style=askedStyle;if(isStick)style.round=true;
    constexpr int s=4;const int W=int(width)*s,H=int(height)*s;
    HDC screen=GetDC(nullptr);HDC dc=CreateCompatibleDC(screen);ReleaseDC(nullptr,screen);
    if(!dc)throw std::runtime_error("glyph dc");
    BITMAPINFO bi{};bi.bmiHeader.biSize=sizeof bi.bmiHeader;bi.bmiHeader.biWidth=W;bi.bmiHeader.biHeight=-H;bi.bmiHeader.biPlanes=1;bi.bmiHeader.biBitCount=32;bi.bmiHeader.biCompression=BI_RGB;
    void* bits=nullptr;HBITMAP bmp=CreateDIBSection(dc,&bi,DIB_RGB_COLORS,&bits,nullptr,0);
    if(!bmp||!bits){DeleteDC(dc);throw std::runtime_error("glyph bitmap");}
    const auto old=SelectObject(dc,bmp);
    const auto pixels=static_cast<const uint8_t*>(bits);
    const auto mask=[&](auto draw){
        RECT all{0,0,W,H};FillRect(dc,&all,static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));GdiFlush();draw();GdiFlush();
        std::vector<uint8_t> m(size_t(width)*height);
        for(uint32_t y=0;y<height;++y)for(uint32_t x=0;x<width;++x){unsigned sum=0;
            for(int j=0;j<s;++j)for(int i=0;i<s;++i)sum+=pixels[(size_t(y*s+j)*W+(x*s+i))*4+1];
            m[size_t(y)*width+x]=uint8_t(sum/(s*s));}
        return m;};
    const int margin=int(style.margin*std::min(W,H)+.5f),rim=std::max(s,int(style.rim*std::min(W,H)+.5f)),radius=int(style.radius*std::min(W,H)*2+.5f);
    // a round plate is a circle in the middle, as wide as the picture is high
    const int side=std::min(W,H)-2*margin,left=style.round?(W-side)/2:margin,right=style.round?left+side:W-margin;
    const auto plate=[&](int inset){
        SelectObject(dc,GetStockObject(WHITE_BRUSH));SelectObject(dc,GetStockObject(NULL_PEN));
        if(style.round)Ellipse(dc,left+inset,margin+inset,right-inset+1,H-margin-inset+1);
        else RoundRect(dc,margin+inset,margin+inset,W-margin-inset+1,H-margin-inset+1,std::max(0,radius-2*inset),std::max(0,radius-2*inset));};
    const auto outer=mask([&]{plate(0);});
    const auto inner=mask([&]{plate(rim);});
    // The label as large as fits inside the plate.
    // a stick's badge: a circle at the top right of the plate, over its rim (pr the plate's radius)
    const bool badge=isStick&&mark!=StickMark::None;const float pr=float(right-left)*.5f,pcx=float(left)+pr,pcy=float(margin)+pr;
    const float rb=pr*.5f,bx=pcx+pr*.56f,by=pcy-pr*.56f;
    const int room=badge?int((right-left-2*rim)*.7f):style.round?int((right-left-2*rim)*.78f):W-2*(margin+rim)-2*s;int textHeight=int((badge?.46f:style.text)*H);
    HFONT font=nullptr;
    for(;textHeight>4*s;textHeight-=s){
        if(font)DeleteObject(font);
        font=CreateFontW(-textHeight,0,0,0,FW_BOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_TT_PRECIS,CLIP_DEFAULT_PRECIS,NONANTIALIASED_QUALITY,DEFAULT_PITCH|FF_SWISS,L"Segoe UI");
        SelectObject(dc,font);SIZE size{};GetTextExtentPoint32W(dc,label.c_str(),int(label.size()),&size);
        if(size.cx<=room)break;}
    // swingLabel: no letters but a stroke - an arc over the top with its arrow head (2026-10-21, the user: "what is done by
    // swinging alone, the wand or a weapon: an icon that looks like swinging").
    // a swing: an arc of radius R about (cx, cy), clockwise from 160 to 28 degrees, with its arrow head (k: the size against H)
    const auto swing=[&](float cx,float cy,float R,float k){
        constexpr float d=3.14159265f/180;const float a0=160*d,a1=28*d;
        const auto at=[&](float a){return POINT{LONG(cx+R*std::cos(a)),LONG(cy-R*std::sin(a))};};
        HPEN pen=CreatePen(PS_SOLID,std::max(2,int(H*.12f*k)),RGB(255,255,255));const auto oldPen=SelectObject(dc,pen);
        SetArcDirection(dc,AD_CLOCKWISE);const auto p0=at(a0),p1=at(a1);
        Arc(dc,LONG(cx-R),LONG(cy-R),LONG(cx+R),LONG(cy+R),p0.x,p0.y,p1.x,p1.y);
        SelectObject(dc,oldPen);DeleteObject(pen);
        const float dx=std::sin(a1),dy=std::cos(a1),L=H*.22f*k,w=H*.15f*k;   // along the stroke at its end, and across it
        const POINT head[3]{{LONG(p1.x+dx*L),LONG(p1.y+dy*L)},{LONG(p1.x-dy*w),LONG(p1.y+dx*w)},{LONG(p1.x+dy*w),LONG(p1.y-dx*w)}};
        SelectObject(dc,GetStockObject(WHITE_BRUSH));SelectObject(dc,GetStockObject(NULL_PEN));Polygon(dc,head,3);};
    const auto ink=mask([&]{
        if(label==swingLabel){swing(W*.45f,H*.74f,std::min(W*.29f,H*.4f),1.f);return;}
        // gripSwingLabel (2026-10-23, the user: "is the way to cast it shown in the UI too?"): the swing in the upper half, "RG" below
        if(label==gripSwingLabel){
            swing(W*.47f,H*.5f,std::min(W*.22f,H*.27f),.72f);
            HFONT small=CreateFontW(-int(H*.34f),0,0,0,FW_BOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_TT_PRECIS,CLIP_DEFAULT_PRECIS,NONANTIALIASED_QUALITY,DEFAULT_PITCH|FF_SWISS,L"Segoe UI");
            const auto oldFont=SelectObject(dc,small);SetBkMode(dc,TRANSPARENT);SetTextColor(dc,RGB(255,255,255));RECT r{0,int(H*.5f),W,int(H*.9f)};
            DrawTextW(dc,L"RG",2,&r,DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_NOPREFIX);SelectObject(dc,oldFont);DeleteObject(small);
            return;}
        SetBkMode(dc,TRANSPARENT);SetTextColor(dc,RGB(255,255,255));RECT r{0,0,W,H};
        // with a badge the letters sit a little left and down, clear of it
        if(badge)r={LONG(-pr*.14f),LONG(pr*.07f),LONG(W-pr*.14f),LONG(H+pr*.07f)};
        DrawTextW(dc,label.c_str(),int(label.size()),&r,DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_NOPREFIX);});
    // The badge as a layer of its own over the plate: its circle (light, with the dark rim) and its mark (dark).
    std::vector<uint8_t> badgeOuter,badgeInner,badgeInk;
    if(badge){
        const int brim=std::max(s,int(rim*.8f));
        const auto disc=[&](float x,float y,float r){Ellipse(dc,LONG(x-r),LONG(y-r),LONG(x+r)+1,LONG(y+r)+1);};
        const auto fill=[&]{SelectObject(dc,GetStockObject(WHITE_BRUSH));SelectObject(dc,GetStockObject(NULL_PEN));};
        badgeOuter=mask([&]{fill();disc(bx,by,rb);});
        badgeInner=mask([&]{fill();disc(bx,by,rb-float(brim));});
        badgeInk=mask([&]{fill();
            // a triangle pointing (dx, dy), of half size k (against the badge's radius), its middle at (x, y)
            const auto tri=[&](float x,float y,float dx,float dy,float k){const float L=rb*k;
                const POINT p[3]{{LONG(x+dx*L),LONG(y+dy*L)},{LONG(x-dx*L*.7f-dy*L),LONG(y-dy*L*.7f+dx*L)},{LONG(x-dx*L*.7f+dy*L),LONG(y-dy*L*.7f-dx*L)}};
                Polygon(dc,p,3);};
            switch(mark){
            case StickMark::Up:tri(bx,by+rb*.12f,0,-1,.58f);break;
            case StickMark::Down:tri(bx,by-rb*.12f,0,1,.58f);break;
            case StickMark::Left:tri(bx+rb*.12f,by,-1,0,.58f);break;
            case StickMark::Right:tri(bx-rb*.12f,by,1,0,.58f);break;
            case StickMark::Vertical:tri(bx,by-rb*.36f,0,-1,.36f);tri(bx,by+rb*.36f,0,1,.36f);break;
            case StickMark::Horizontal:tri(bx-rb*.36f,by,-1,0,.36f);tri(bx+rb*.36f,by,1,0,.36f);break;
            case StickMark::Press:disc(bx,by,rb*.42f);break;
            default:break;}});
    }
    SelectObject(dc,old);if(font)DeleteObject(font);DeleteObject(bmp);DeleteDC(dc);
    std::vector<uint8_t> out(size_t(width)*height*4);
    for(size_t p=0;p<size_t(width)*height;++p){
        const int a=outer[p],i=std::min<int>(inner[p],a),t=std::min<int>(ink[p],i);
        int c=(style.ink*(a-i)+style.plate*(i-t)+style.ink*t)/255;   // premultiplied: rim, plate, label
        int alpha=a;
        if(badge){const int ba=badgeOuter[p],bn=std::min<int>(badgeInner[p],ba),bt=std::min<int>(badgeInk[p],bn);
            const int bc=(style.ink*(ba-bn)+style.plate*(bn-bt)+style.ink*bt)/255;   // the badge over the plate
            c=bc+c*(255-ba)/255;alpha=ba+a*(255-ba)/255;}
        out[p*4]=out[p*4+1]=out[p*4+2]=uint8_t(std::min(c,alpha));out[p*4+3]=uint8_t(alpha);}
    return out;
}
}
