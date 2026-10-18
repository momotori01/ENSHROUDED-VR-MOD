#include <evr/tiny_text.hpp>
#include <iostream>
#include <stdexcept>
#include <string>
using namespace evr::text;
int main(){try{unsigned n=0;auto ck=[&](bool b){++n;if(!b)throw std::runtime_error(std::to_string(n));};
 const std::array<uint8_t,4> ink{250,250,250,255},paper{10,10,10,255};
 const auto px=[](const Picture& p,uint32_t x,uint32_t y){const auto* q=p.rgba.data()+(size_t(y)*p.width+x)*4;return std::array<uint8_t,4>{q[0],q[1],q[2],q[3]};};
 // Size: six columns a letter less the last blank one, eight rows, the band all round.
 const auto p=render("VR Now Loading...",2,3,ink,paper);
 ck(p.width==17*6*2-2+6&&p.height==8*2+6&&p.rgba.size()==size_t(p.width)*p.height*4);
 ck(px(p,0,0)==paper&&px(p,3,3)==ink&&px(p,4,4)==ink);            // V: its top left pixel is set, two by two
 ck(px(p,3+2*1,3)==paper);                                         // V: the second column of the top row is not
 ck(px(p,3+6*2,3)==ink);                                           // R begins one blank column after V
 // A letter that the font does not have is blank; nothing for an empty line or a scale of 0.
 const auto q=render("Z",3,0,ink,paper);ck(q.width==15&&q.height==24);bool blank=true;for(uint32_t y=0;y<q.height;y++)for(uint32_t x=0;x<q.width;x++)blank=blank&&px(q,x,y)==paper;ck(blank);
 ck(render("",2,1,ink,paper).rgba.empty()&&render("V",0,1,ink,paper).rgba.empty());
 // Every letter of the line is in the font.
 for(char c:std::string("VR Now Loading..."))ck(glyph(c)!=nullptr);
 std::cout<<"tiny_text "<<n<<" PASS\n";return 0;}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
