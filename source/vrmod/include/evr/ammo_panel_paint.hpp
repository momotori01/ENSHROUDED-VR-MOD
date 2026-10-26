#pragma once
// The staff's spell list as the game draws it, painted by the mod with the game's own font files and icons (2026-10-26; user after
// the first try in a font of Windows: "the icons and the letters shape the mood - use the game's own font"). Measured on the game's
// own list in the HUD (Mock 20261026-110253, UI picture of frame 1671, 1080 rows; scratchpad spells/game-list-zoom.png):
//   the chosen spell's row centred on the crosshair's row; rows 30 apart (29 from the chosen one);
//   the count right-aligned 170 right of the crosshair, the icon on a dark 30 x 30 square ending 205 right (black at 35 % for the
//   chosen one, 18 % for the others), the name from 209;
//   the chosen row white, letters about 29 high (the count bold); the others grey 80 % at 63 %, about 24 high;
//   a shadow 2 right and 2 down, black at 50 % (namedStyles.hud.ammoListEntry dropShadowOffset / Color).
// Fonts: the HUD font resource's language groups (ammo_names.hpp glyphSets): Japanese draws every character with Noto Serif JP
// (regular: Light, bold: SemiBold), the default group Latin with Harmonique Display, and so on.
// The picture: RGBA8, display values, straight alpha, rows from the top, in pixels of a 1080 rows high HUD; its left edge 100 right
// of the crosshair.
#include <windows.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <map>
#include <string>
#include <vector>
#include <evr/ammo_names.hpp>
#include <evr/game_content.hpp>
namespace evr::ammopanel {
struct Row {uint32_t item=0,count=0;bool eternal=false;};
struct Picture {std::vector<uint8_t> rgba;uint32_t width=0,height=0;float anchor=.5f;float left=100;bool fonts=false;};
constexpr float leftOfPicture=100;   // HUD pixels right of the crosshair
class Assets {
    content::Index index;std::map<unsigned,bool> fontLoaded;std::map<std::pair<uint32_t,int>,std::vector<float>> icons;   // premultiplied RGBA floats
  public:
    explicit Assets(std::filesystem::path root):index(std::move(root)){}
    const std::string& failure() const {return index.failure();}
    // The font file of the HUD font resource, added to this process (AddFontMemResourceEx: private to it).
    bool font(unsigned i){
        if(i>=std::size(ammo::fonts))return false;
        if(const auto f=fontLoaded.find(i);f!=fontLoaded.end())return f->second;
        std::vector<uint8_t> data;bool good=false;
        if(index.read(ammo::fonts[i].key,data)){DWORD n=0;good=AddFontMemResourceEx(data.data(),DWORD(data.size()),nullptr,&n)!=nullptr&&n>0;}
        fontLoaded[i]=good;return good;
    }
    // The item's icon at size x size (box filter over its pixels), premultiplied floats 0..1; empty when there is none.
    const std::vector<float>& icon(uint32_t item,int size){
        static const std::vector<float> none;
        const auto key=std::make_pair(item,size);if(const auto f=icons.find(key);f!=icons.end())return f->second;
        auto& out=icons[key];const auto* n=ammo::find(item);std::vector<uint8_t> px;
        if(!n||!n->iconSize||size<=0||!index.read(n->icon,px)||px.size()!=size_t(n->iconSize)*n->iconSize*4)return out;
        const int src=n->iconSize;out.assign(size_t(size)*size*4,0.f);
        for(int y=0;y<size;++y)for(int x=0;x<size;++x){
            const float x0=float(x)*src/size,x1=float(x+1)*src/size,y0=float(y)*src/size,y1=float(y+1)*src/size;float sum[4]{},area=0;
            for(int sy=int(y0);sy<int(std::ceil(y1))&&sy<src;++sy)for(int sx=int(x0);sx<int(std::ceil(x1))&&sx<src;++sx){
                const float wx=std::min(x1,float(sx+1))-std::max(x0,float(sx)),wy=std::min(y1,float(sy+1))-std::max(y0,float(sy));const float w=wx*wy;if(w<=0)continue;
                for(int c=0;c<4;++c)sum[c]+=w*px[(size_t(sy)*src+sx)*4+c]/255.f;area+=w;}
            for(int c=0;c<4;++c)out[(size_t(y)*size+x)*4+c]=area>0?sum[c]/area:0;}
        return out;
    }
};
// The group of the game's language (its keen::LanguageId bit), else the fallback group.
inline unsigned groupFor(unsigned languageIndex){
    const int id=ammo::languageIds[languageIndex<std::size(ammo::languageIds)?languageIndex:0];unsigned fallback=0;
    for(unsigned g=0;g<std::size(ammo::languageGroups);++g){if(ammo::languageGroups[g].fallback)fallback=g;
        else if(id>0&&id<64&&(ammo::languageGroups[g].languages>>id&1))return g;}
    return fallback;
}
inline int fontFor(unsigned group,unsigned variant,uint32_t c){
    for(int pass=0;pass<2;++pass){
        for(const auto& s:ammo::glyphSets)if(s.group==group&&s.variant==variant&&c>=s.first&&c<=s.last)return s.font;
        for(const auto& g:ammo::languageGroups)if(g.fallback){group=unsigned(&g-ammo::languageGroups);break;}   // then the fallback group's
    }
    return -1;
}
// A canvas of premultiplied floats.
struct Canvas {
    int w=0,h=0;std::vector<float> p;
    Canvas(int width,int height):w(width),h(height),p(size_t(width)*height*4,0.f){}
    void over(int x,int y,float r,float g,float b,float a){   // premultiplied colour over the pixel
        if(x<0||y<0||x>=w||y>=h||a<=0)return;auto* q=&p[(size_t(y)*w+x)*4];const float k=1-a;q[0]=r+q[0]*k;q[1]=g+q[1]*k;q[2]=b+q[2]*k;q[3]=a+q[3]*k;}
};
// Text drawn by GDI at s times the size, the game's font files chosen per character; returns the coverage at 1x (cw x ch).
class TextPainter {
    static constexpr int s=3;
  public:
    // Draws `text` with its baseline at (x, baseline) - left aligned, or right aligned at x - into a coverage mask over the canvas.
    // embolden: the strokes widened by this many pixels each side (a max filter at s times the size) - the game's chosen row is
    // heavier than the boldest file of its font draws it.
    static float draw(Assets& assets,Canvas& canvas,const std::wstring& text,float x,float baseline,float pixels,unsigned group,unsigned variant,bool right,
                      std::array<float,3> colour,float alpha,bool shadow,float embolden=0){
        if(text.empty())return 0;
        // runs of one font
        struct Run {std::wstring text;int font;};std::vector<Run> runs;
        for(size_t i=0;i<text.size();++i){uint32_t c=text[i];if(c>=0xd800&&c<0xdc00&&i+1<text.size()){c=0x10000+((c-0xd800)<<10)+(text[i+1]-0xdc00);}
            const int f=fontFor(group,variant,c);std::wstring piece(1,text[i]);if(c>=0x10000){piece.push_back(text[++i]);}
            if(!runs.empty()&&runs.back().font==f)runs.back().text+=piece;else runs.push_back({piece,f});}
        HDC dc=CreateCompatibleDC(nullptr);if(!dc)return 0;
        std::vector<HFONT> made;
        const auto fontOf=[&](int f)->HFONT{
            const wchar_t* face=L"Segoe UI";std::wstring family;int weight=FW_NORMAL;BOOL italic=FALSE;
            if(f>=0&&assets.font(unsigned(f))){const auto& ff=ammo::fonts[f];family.assign(ff.family,ff.family+std::strlen(ff.family));
                const std::string sub=ff.subfamily;if(sub.find("Bold")!=std::string::npos)weight=FW_BOLD;if(sub.find("Italic")!=std::string::npos)italic=TRUE;face=family.c_str();}
            LOGFONTW lf{};lf.lfHeight=-LONG(std::lround(pixels*s));lf.lfWeight=weight;lf.lfItalic=BYTE(italic);lf.lfCharSet=DEFAULT_CHARSET;lf.lfOutPrecision=OUT_TT_PRECIS;
            lf.lfQuality=ANTIALIASED_QUALITY;wcsncpy_s(lf.lfFaceName,face,LF_FACESIZE-1);const HFONT h=CreateFontIndirectW(&lf);made.push_back(h);return h;};
        // measure
        std::vector<HFONT> fonts;int total=0;std::vector<int> widths;
        for(const auto& r:runs){const HFONT h=fontOf(r.font);fonts.push_back(h);const auto old=SelectObject(dc,h);SIZE sz{};GetTextExtentPoint32W(dc,r.text.c_str(),int(r.text.size()),&sz);SelectObject(dc,old);widths.push_back(sz.cx);total+=sz.cx;}
        const int W=int(std::ceil(total/float(s)))+int(pixels)+8,H=int(std::ceil(pixels*1.6f))+8;const float originX=right?x-float(total)/s:x;
        const int left=int(std::floor(originX))-int(pixels*.25f)-2,top=int(std::floor(baseline-pixels*1.15f))-2;
        BITMAPINFO bi{};bi.bmiHeader.biSize=sizeof bi.bmiHeader;bi.bmiHeader.biWidth=W*s;bi.bmiHeader.biHeight=-H*s;bi.bmiHeader.biPlanes=1;bi.bmiHeader.biBitCount=32;bi.bmiHeader.biCompression=BI_RGB;
        void* bits=nullptr;HBITMAP bmp=CreateDIBSection(dc,&bi,DIB_RGB_COLORS,&bits,nullptr,0);
        if(bmp&&bits){
            const auto oldBmp=SelectObject(dc,bmp);SetBkMode(dc,TRANSPARENT);SetTextColor(dc,RGB(255,255,255));SetTextAlign(dc,TA_LEFT|TA_BASELINE);
            std::memset(bits,0,size_t(W)*s*H*s*4);int pen=int(std::lround((originX-left)*s));const int base=int(std::lround((baseline-top)*s));
            for(size_t i=0;i<runs.size();++i){const auto old=SelectObject(dc,fonts[i]);TextOutW(dc,pen,base,runs[i].text.c_str(),int(runs[i].text.size()));SelectObject(dc,old);pen+=widths[i];}
            GdiFlush();
            const int SW=W*s,SH=H*s;std::vector<uint8_t> g(size_t(SW)*SH);const auto* px=static_cast<const uint8_t*>(bits);
            for(size_t i=0;i<g.size();++i)g[i]=px[i*4+1];
            if(const int R=int(std::lround(embolden*s));R>0){   // a square max filter, horizontally then vertically
                std::vector<uint8_t> t(g.size());
                for(int y=0;y<SH;++y)for(int xx=0;xx<SW;++xx){uint8_t m=0;for(int k=std::max(0,xx-R);k<=std::min(SW-1,xx+R);++k)m=std::max(m,g[size_t(y)*SW+k]);t[size_t(y)*SW+xx]=m;}
                for(int y=0;y<SH;++y)for(int xx=0;xx<SW;++xx){uint8_t m=0;for(int k=std::max(0,y-R);k<=std::min(SH-1,y+R);++k)m=std::max(m,t[size_t(k)*SW+xx]);g[size_t(y)*SW+xx]=m;}
            }
            std::vector<float> cover(size_t(W)*H,0.f);
            for(int y=0;y<H;++y)for(int xx=0;xx<W;++xx){unsigned sum=0;for(int j=0;j<s;++j)for(int i=0;i<s;++i)sum+=g[size_t(y*s+j)*SW+size_t(xx*s+i)];cover[size_t(y)*W+xx]=sum/(255.f*s*s);}
            if(shadow)for(int y=0;y<H;++y)for(int xx=0;xx<W;++xx){const float c=cover[size_t(y)*W+xx];if(c>0)canvas.over(left+xx+2,top+y+2,0,0,0,c*.5f*alpha);}
            for(int y=0;y<H;++y)for(int xx=0;xx<W;++xx){const float c=cover[size_t(y)*W+xx]*alpha;if(c>0)canvas.over(left+xx,top+y,colour[0]*c,colour[1]*c,colour[2]*c,c);}
            SelectObject(dc,oldBmp);DeleteObject(bmp);
        }
        for(const auto h:made)if(h)DeleteObject(h);DeleteDC(dc);
        return float(total)/s;
    }
};
inline std::wstring wide(const char* s){if(!s||!*s)return {};const int n=MultiByteToWideChar(CP_UTF8,0,s,-1,nullptr,0);if(n<=1)return {};std::wstring w(size_t(n-1),L'\0');MultiByteToWideChar(CP_UTF8,0,s,-1,w.data(),n);return w;}
// The font variants (bit 0 bold, bit 1 italic - the HUD font's glyph sets) of the chosen row's count and name and of the others',
// and how much their strokes are widened. Matched by eye to the game's own list (scratchpad spells/variants2.png, 2026-10-26):
// the chosen count Bold + 0.6, its name SemiBold + 0.4, the others Light + 0.15.
struct Style {unsigned chosenCount=3,chosenName=1,otherCount=0,otherName=0;float chosenCountEmbolden=.6f,chosenNameEmbolden=.4f,otherEmbolden=.15f;};
// The list: rows in the game's order, `chosen` the spell in use.
inline Picture paint(Assets& assets,const std::vector<Row>& rows,size_t chosen,unsigned languageIndex,Style style={}){
    Picture out;if(rows.empty())return out;if(chosen>=rows.size())chosen=0;
    const unsigned group=groupFor(languageIndex);
    // row centres from the chosen one's
    std::vector<float> centre(rows.size());
    for(size_t i=0;i<rows.size();++i){const float d=float(int(i)-int(chosen));centre[i]=d==0?0:d>0?29.f+(d-1)*30.f:-29.f+(d+1)*30.f;}
    const float topCentre=centre.front()-20,bottomCentre=centre.back()+20;const int height=int(std::ceil(bottomCentre-topCentre))+4,width=600;
    Canvas canvas(width,height);const float y0=-topCentre+2;   // picture y of the crosshair's row
    const float countRight=170-leftOfPicture,squareRight=205-leftOfPicture,nameLeft=209-leftOfPicture;
    float widest=0;bool fontsOk=true;
    for(size_t i=0;i<rows.size();++i){
        const bool on=i==chosen;const float cy=y0+centre[i];const auto& r=rows[i];
        // the icon's square and the icon
        const int side=30,sw=30;const int sx1=int(std::lround(squareRight)),sx0=sx1-sw,sy0=int(std::lround(cy-side*.5f));
        for(int y=sy0;y<sy0+side;++y)for(int x=sx0;x<sx1;++x)canvas.over(x,y,0,0,0,on?.35f:.18f);
        const int is=side-2;const auto& icon=assets.icon(r.item,is);const float dim=on?1.f:.55f;
        if(!icon.empty())for(int y=0;y<is;++y)for(int x=0;x<is;++x){const auto* q=&icon[(size_t(y)*is+x)*4];canvas.over(sx1-sw+(sw-is)/2+x,sy0+1+y,q[0]*dim,q[1]*dim,q[2]*dim,q[3]*dim);}
        // the count and the name
        const float px=on?29.f:24.f;const std::array<float,3> ink=on?std::array<float,3>{.95f,.985f,1.f}:std::array<float,3>{.8f,.8f,.8f};const float alpha=on?1.f:.63f;
        const float baseline=cy+px*.37f;
        const std::wstring count=r.eternal?std::wstring(L"\x221e"):std::to_wstring(r.count);
        TextPainter::draw(assets,canvas,count,countRight,baseline,px,group,on?style.chosenCount:style.otherCount,true,ink,alpha,true,on?style.chosenCountEmbolden:style.otherEmbolden);
        const auto* n=ammo::find(r.item);const std::wstring name=n?wide(n->names[languageIndex<std::size(n->names)?languageIndex:0]):std::to_wstring(r.item);
        widest=std::max(widest,TextPainter::draw(assets,canvas,name,nameLeft,baseline,px,group,on?style.chosenName:style.otherName,false,ink,alpha,true,on?style.chosenNameEmbolden:style.otherEmbolden));
        if(fontFor(group,0,name.empty()?L'A':name[0])<0||!assets.font(unsigned(std::max(0,fontFor(group,0,name.empty()?L'A':name[0])))))fontsOk=false;
    }
    // cut to what is drawn, straight alpha
    const int used=std::min(width,int(std::ceil(nameLeft+widest))+6);
    out.width=uint32_t(used);out.height=uint32_t(height);out.anchor=y0/float(height);out.left=leftOfPicture;out.fonts=fontsOk;out.rgba.assign(size_t(used)*height*4,0);
    for(int y=0;y<height;++y)for(int x=0;x<used;++x){const auto* q=&canvas.p[(size_t(y)*width+x)*4];auto* o=&out.rgba[(size_t(y)*used+x)*4];const float a=std::clamp(q[3],0.f,1.f);
        if(a<=0)continue;for(int c=0;c<3;++c)o[c]=uint8_t(std::clamp(q[c]/a,0.f,1.f)*255.f+.5f);o[3]=uint8_t(a*255.f+.5f);}
    return out;
}
}
