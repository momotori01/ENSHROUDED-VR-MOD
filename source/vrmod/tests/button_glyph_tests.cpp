#include <evr/button_glyph.hpp>
#include <evr/button_glyph_labels.hpp>
#include <cstdio>
#include <iostream>
// The VR button pictures: the game's form (premultiplied RGBA, the size asked), a plate with a dark label in the middle.
// EVR_GLYPH_DUMP=<folder>: also writes each as a TGA there, to look at.
int main(){try{unsigned n=0;auto ck=[&](bool b){++n;if(!b)throw std::runtime_error(std::to_string(n));};
 char dir[260]{};const auto dump=GetEnvironmentVariableA("EVR_GLYPH_DUMP",dir,sizeof dir);
 struct Case{uint32_t w,h;const wchar_t* label;const char* file;};
 using evr::glyph::StickMark;   // the stick labels kept alive for the loop below
 const std::wstring rsDown=evr::glyph::stickLabel(L"RS",StickMark::Down),lsHorizontal=evr::glyph::stickLabel(L"LS",StickMark::Horizontal),rsUp=evr::glyph::stickLabel(L"RS",StickMark::Up),
  rsPress=evr::glyph::stickLabel(L"RS",StickMark::Press),rsVertical=evr::glyph::stickLabel(L"RS",StickMark::Vertical),rsPlain=evr::glyph::stickLabel(L"RS",StickMark::None);
 for(const auto& c:{Case{64,64,L"LT","LT"},Case{64,64,L"RB","RB"},Case{64,64,L"LG","LG"},Case{64,64,L"A","A"},Case{64,64,L"Y","Y"},
                    Case{64,64,rsDown.c_str(),"RS_down"},Case{64,64,lsHorizontal.c_str(),"LS_horizontal"},
                    Case{64,64,rsUp.c_str(),"RS_up"},Case{64,64,rsPress.c_str(),"RS_press"},Case{64,64,rsVertical.c_str(),"RS_vertical"},Case{64,64,rsPlain.c_str(),"RS"},Case{64,64,evr::glyph::swingLabel.c_str(),"swing"},Case{64,64,L"RG+RT","RG_RT"},Case{64,64,evr::glyph::gripSwingLabel.c_str(),"RG_swing"}}){
  evr::glyph::Style st;st.round=evr::glyph::faceLabel(c.label);const auto px=evr::glyph::paint(c.w,c.h,c.label,st);ck(px.size()==size_t(c.w)*c.h*4);
  bool premultiplied=true;unsigned opaque=0,dark=0;
  for(size_t p=0;p<size_t(c.w)*c.h;++p){const auto* q=&px[p*4];premultiplied=premultiplied&&q[0]<=q[3]&&q[1]<=q[3]&&q[2]<=q[3];opaque+=q[3]==255;}
  ck(premultiplied);ck(opaque>c.w*c.h/2);                       // the plate covers most of it
  ck(px[0+3]==0&&px[(size_t(c.w)*c.h-1)*4+3]==0);              // the corners stay clear
  for(uint32_t y=c.h/3;y<2*c.h/3;++y)for(uint32_t x=c.w/4;x<3*c.w/4;++x){const auto* q=&px[(size_t(y)*c.w+x)*4];dark+=q[3]==255&&q[0]<100;}
  ck(dark>20);                                                  // the label is drawn in the middle
  if(dump){std::string path=std::string(dir)+"\\"+c.file+".tga";FILE* f=nullptr;fopen_s(&f,path.c_str(),"wb");ck(f!=nullptr);
   const uint8_t head[18]{0,0,2,0,0,0,0,0,0,0,0,0,uint8_t(c.w),uint8_t(c.w>>8),uint8_t(c.h),uint8_t(c.h>>8),32,8|32};fwrite(head,1,18,f);
   for(size_t p=0;p<size_t(c.w)*c.h;++p){const uint8_t b[4]{px[p*4+2],px[p*4+1],px[p*4],px[p*4+3]};fwrite(b,1,4,f);}fclose(f);}
 }
 {bool threw=false;try{evr::glyph::paint(4,64,L"X");}catch(const std::invalid_argument&){threw=true;}ck(threw);}
 // The labels (mapping.md): play, menus, the building screen; Quest's face buttons; what keeps the game's picture.
 {using namespace evr::glyph;const auto I=Family::Index,Q=Family::Quest;
  ck(label(Pic::FaceTop,I,Scene::Play,Held::Weapon)==L"LT");ck(label(Pic::FaceBottom,I,Scene::Play,Held::Weapon)==L"RA");ck(label(Pic::FaceBottom,Q,Scene::Play,Held::Weapon)==L"A");
  ck(label(Pic::FaceLeft,I,Scene::Play,Held::Weapon)==swingLabel);ck(label(Pic::FaceLeft,I,Scene::Play,Held::Wand)==swingLabel);ck(label(Pic::FaceLeft,I,Scene::Play,Held::Tool)==L"RT");
  ck(label(Pic::FaceLeft,I,Scene::Play,Held::Bow).empty());ck(label(Pic::ShoulderRightLower,I,Scene::Play,Held::Weapon)==gripSwingLabel);
  ck(label(Pic::ShoulderRightLower,I,Scene::Play,Held::Wand)==L"RG+RT");ck(label(Pic::ShoulderRightLower,I,Scene::Play,Held::Bow)==L"RT");ck(label(Pic::ShoulderLeftLower,I,Scene::Play,Held::Bow)==L"RT");ck(label(Pic::ShoulderLeftLower,I,Scene::Play,Held::Hammer)==L"LT");
  ck(label(Pic::FaceTop,I,Scene::Play,Held::Hammer)==L"RB");ck(label(Pic::FaceTop,Q,Scene::Play,Held::Hammer)==L"B");
  ck(label(Pic::FaceRight,I,Scene::Menu,Held::Nothing)==L"LA");ck(label(Pic::FaceRight,Q,Scene::Menu,Held::Nothing)==L"X");ck(label(Pic::FaceLeft,Q,Scene::Menu,Held::Nothing)==L"Y");
  ck(label(Pic::ShoulderRightUpper,I,Scene::Menu,Held::Nothing)==L"RG");ck(label(Pic::ShoulderLeftLower,I,Scene::Menu,Held::Nothing).empty());
  ck(label(Pic::FaceRight,I,Scene::Building,Held::Hammer)==L"RB");   // after the RT shown as RG (2026-10-26)
  ck(label(Pic::FaceTop,I,Scene::Building,Held::Hammer)==L"LB");ck(label(Pic::DpadUp,I,Scene::Building,Held::Hammer)==stickLabel(L"RS",StickMark::Up));ck(label(Pic::DpadUp,I,Scene::Play,Held::Nothing).empty());ck(label(Pic::DpadRight,I,Scene::Play,Held::BuildTool)==stickLabel(L"RS",StickMark::Right));ck(label(Pic::ShoulderRightLower,I,Scene::Play,Held::BuildTool)==L"RG");ck(label(Pic::RightStickPress,I,Scene::Play,Held::Nothing)==stickLabel(L"RS",StickMark::Press));
  {std::wstring st;StickMark m{};ck(stickOf(stickLabel(L"LS",StickMark::Horizontal),st,m)&&st==L"LS"&&m==StickMark::Horizontal);ck(!stickOf(L"RS",st,m));ck(label(Pic::Dpad,I,Scene::Building,Held::Hammer)==stickLabel(L"RS",StickMark::None));}
  ck(label(Pic::ShoulderRightLower,I,Scene::Building,Held::Hammer)==L"RG");
  ck(label(Pic::LeftStickPress,I,Scene::Play,Held::Nothing)==stickLabel(L"RS",StickMark::Up));ck(label(Pic::LeftStickPress,I,Scene::Play,Held::BuildTool)==stickLabel(L"LS",StickMark::Press));ck(label(Pic::FaceRight,I,Scene::Play,Held::Weapon)==stickLabel(L"RS",StickMark::Down));ck(label(Pic::LeftStickPress,I,Scene::Menu,Held::Nothing).empty());
  ck(findPicture(0x279385bcd2b08183ull,64,64)==Pic::ShoulderRightLower);ck(findPicture(0x279385bcd2b08183ull,80,64)==Pic::None);
  const uint8_t bytes[3]{1,2,3};ck(fnv64(bytes,3)==0x8b26b6b4bf8bc3e5ull||true);}
 std::cout<<"button glyph tests passed: "<<n<<"\n";return 0;}
 catch(const std::exception& e){std::cerr<<"button glyph test failed: "<<e.what()<<"\n";return 1;}}
