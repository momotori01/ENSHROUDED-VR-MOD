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
// width x height x 4 bytes, rows top down.
inline std::vector<uint8_t> paint(uint32_t width,uint32_t height,const std::wstring& label,const Style& style={}){
    if(width<8||height<8||width>512||height>512)throw std::invalid_argument("glyph size");
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
    const int room=style.round?int((right-left-2*rim)*.78f):W-2*(margin+rim)-2*s;int textHeight=int(style.text*H);
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
        DrawTextW(dc,label.c_str(),int(label.size()),&r,DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_NOPREFIX);});
    SelectObject(dc,old);if(font)DeleteObject(font);DeleteObject(bmp);DeleteDC(dc);
    std::vector<uint8_t> out(size_t(width)*height*4);
    for(size_t p=0;p<size_t(width)*height;++p){
        const int a=outer[p],i=std::min<int>(inner[p],a),t=std::min<int>(ink[p],i);
        const int c=(style.ink*(a-i)+style.plate*(i-t)+style.ink*t)/255;   // premultiplied: rim, plate, label
        out[p*4]=out[p*4+1]=out[p*4+2]=uint8_t(std::min(c,a));out[p*4+3]=uint8_t(a);}
    return out;
}
}
