#include <evr/recommended_graphics.hpp>
#include <iostream>
#include <stdexcept>
#include <string>
using namespace evr::recommend;
namespace {
// The shape of the game 1.0's local_settings.json (its graphics part, 2026-10-20), tabs and CRLF as the game writes it.
const std::string game=
 "{\r\n\t\"graphics\": {\r\n\t\t\"windowMode\": \"Fullscreen\",\r\n\t\t\"renderScale\": \"1\",\r\n\t\t\"automaticRenderScale\": true,\r\n\t\t\"vsync\": true,\r\n"
 "\t\t\"hdrSettings\": {\r\n\t\t\t\"enableHdr\": false\r\n\t\t},\r\n\t\t\"nvidiaReflex\": \"On\",\r\n\t\t\"render\": {\r\n\t\t\t\"preset\": \"Custom\",\r\n"
 "\t\t\t\"antiAliasing\": \"Dlss\",\r\n\t\t\t\"fsr2\": \"Balance\",\r\n\t\t\t\"fsr3\": \"Balance\",\r\n\t\t\t\"dlss\": \"Balanced\",\r\n\t\t\t\"xess\": \"Balanced\",\r\n"
 "\t\t\t\"shadows\": \"Low\",\r\n\t\t\t\"contactShadows\": \"Disabled\",\r\n\t\t\t\"dynamicGi\": \"High\",\r\n\t\t\t\"reflections\": \"Balance\",\r\n\t\t\t\"water\": \"Balance\",\r\n"
 "\t\t\t\"fog\": \"Medium\",\r\n\t\t\t\"ssao\": \"Disabled\",\r\n\t\t\t\"distantObjects\": \"Medium\",\r\n\t\t\t\"decoObjects\": \"Medium\",\r\n\t\t\t\"detailScattering\": \"Medium\",\r\n"
 "\t\t\t\"voxelMeshQuality\": \"Low\",\r\n\t\t\t\"tessellation\": \"Medium\",\r\n\t\t\t\"pointLightShadows\": \"Low\",\r\n\t\t\t\"textureAnisotropy\": \"Low\",\r\n\t\t\t\"models\": \"Balance\"\r\n"
 "\t\t}\r\n\t},\r\n\t\"sound\": {\r\n\t\t\"muteInBackground\": true\r\n\t}\r\n}\r\n";
Machine card(double gb){Machine m;m.known=true;m.gpu="test";m.vendor=nvidia;m.videoMemory=uint64_t(gb*1024*1024*1024);return m;}
std::string value(const std::string& text,const std::string& name){size_t b=0,e=0;return evr::autostart::field(text,name,b,e)?text.substr(b,e-b):"?";}
}
int main(){try{unsigned n=0;auto ck=[&](bool b,int line=__builtin_LINE()){++n;if(!b)throw std::runtime_error(std::to_string(n)+" at line "+std::to_string(line));};
 const auto find=[](const Recommendation& r,const std::string& name){for(const auto& [k,v]:r.render)if(k==name)return v;return std::string("?");};
 // 16 GB (the user's RTX 4080): the usual picture, GI High, filtering High; detail scattering Medium in every class
 {const auto r=recommend(card(15.9));ck(r.renderScale=="0.8"&&r.giQuality=="High"&&r.dlss=="Quality"&&!r.vsync&&!r.hdr&&r.tier=="16 GB");
  ck(find(r,"detailScattering")=="Medium"&&find(r,"textureAnisotropy")=="High"&&find(r,"fog")=="Medium"&&find(r,"contactShadows")=="Disabled"&&find(r,"ssao")=="Disabled");
  // what is seen stays as the user set it on this PC (18:55): distance, deco, tessellation Medium, voxels Low
  ck(find(r,"distantObjects")=="Medium"&&find(r,"decoObjects")=="Medium"&&find(r,"tessellation")=="Medium"&&find(r,"voxelMeshQuality")=="Low");
  ck(find(r,"antiAliasing")=="Dlss"&&find(r,"dlss")=="?"&&find(r,"models")=="Balance");   // DLSS here since 2026-10-25 (the mode below); the unknown "models": not touched
  // a card that is not NVIDIA's: FSR 3 (2026-10-25); one not known: DLSS
  {Machine radeon=card(16);radeon.vendor=amd;ck(find(recommend(radeon),"antiAliasing")=="Fsr3"&&find(recommend(Machine{}),"antiAliasing")=="Dlss");
   const auto asked=pendingMachine(setPending("",true,radeon));ck(asked.known&&asked.vendor==amd&&find(recommend(asked),"antiAliasing")=="Fsr3");
   ck(pendingMachine(setPending("",true,card(16))).vendor==nvidia);}
  // the record of an earlier VR start loses what the button wrote, keeps the rest
  {evr::autostart::RenderRecord rec{{{"antiAliasing","Fsr3","Dlss"},{"dynamicGi","Balance","High"},{"enableHdr","true","false"},{"someOther","A","B"}}};
   ck(dropFromRecord(rec,r)&&rec.changes.size()==1&&rec.changes[0].name=="someOther"&&!dropFromRecord(rec,r));}
  ck(find(r,"dynamicGi")=="High");}   // GI into the game's settings (2026-10-25: the VR start keeps the player's own)
 // 10 GB: a smaller picture, GI Performance, filtering Medium
 {const auto r=recommend(card(9.9));ck(r.renderScale=="0.7"&&r.giQuality=="Performance"&&find(r,"dynamicGi")=="Performance"&&find(r,"textureAnisotropy")=="Medium"&&find(r,"fog")=="Medium"&&find(r,"detailScattering")=="Medium");}
 // 6 GB: fog Low, filtering Low
 {const auto r=recommend(card(6));ck(r.renderScale=="0.7"&&find(r,"textureAnisotropy")=="Low"&&find(r,"fog")=="Medium"&&find(r,"distantObjects")=="Medium"&&find(r,"detailScattering")=="Medium");}   // looks never lowered
 // unknown card: as the usual class
 {const auto r=recommend(Machine{});ck(r.renderScale=="0.8"&&r.giQuality=="High"&&r.tier=="unknown");}
 // into the game's settings: only the recommended values change, every other byte stays
 {Applied a;const auto out=applyGame(game,recommend(card(16)),a);
  ck(value(out,"distantObjects")=="Medium"&&value(out,"decoObjects")=="Medium"&&value(out,"tessellation")=="Medium"&&value(out,"textureAnisotropy")=="High"&&value(out,"detailScattering")=="Medium");
  ck(value(out,"antiAliasing")=="Dlss"&&value(out,"dynamicGi")=="High"&&value(out,"models")=="Balance"&&value(out,"windowMode")=="Fullscreen");
  ck(out.find("\"vsync\": false,")!=std::string::npos&&out.find("\"enableHdr\": false")!=std::string::npos&&out.find("\"automaticRenderScale\": true")!=std::string::npos);
  // textureAnisotropy, vsync, and the DLSS mode with FSR 3's (the VR start keeps the game's own mode since 2026-10-23)
  ck(a.missing.empty()&&a.changed.size()==4&&value(out,"dlss")=="Quality"&&value(out,"fsr3")=="Quality");
  ck(out.size()==game.size()+1);   // Low->High (+1), true->false (+1), Balanced->Quality (-1), Balance->Quality (0)
  Applied again;ck(applyGame(out,recommend(card(16)),again)==out&&again.changed.empty());}   // pressed twice: nothing more
 // a file without one of the fields: the rest is done, that one named
 {std::string older=game;const auto at=older.find("\t\t\t\"textureAnisotropy\": \"Low\",\r\n");older.erase(at,std::string("\t\t\t\"textureAnisotropy\": \"Low\",\r\n").size());
  Applied a;const auto out=applyGame(older,recommend(card(16)),a);ck(a.missing.size()==1&&a.missing[0]=="textureAnisotropy"&&out.find("\"vsync\": false")!=std::string::npos);}
 // the mod's settings: the VR picture and GI (the DLSS mode goes into the game's settings); the request for the next start and the card's memory
 {const std::string ini="[Mode]\r\nVR=1\r\n\r\n[Picture]\r\nRenderScale=1.0\r\nDlss=Balanced\r\nSharpness=0.2\r\n";
  std::vector<std::string> changed;auto out=applyIni(ini,recommend(card(9.9)),&changed);
  ck(evr::settings::get(out,"Picture","RenderScale")=="0.7"&&evr::settings::get(out,"Picture","Dlss")=="Balanced"&&evr::settings::get(out,"Picture","GiQuality").empty()&&evr::settings::get(out,"Picture","Sharpness")=="0.2"&&changed.size()==1);
  ck(!pending(out));out=setPending(out,true,card(9.9));ck(pending(out));
  const auto m=pendingMachine(out);ck(m.known&&gigabytes(m.videoMemory)>9.8&&gigabytes(m.videoMemory)<10.0&&recommend(m).renderScale=="0.7");
  out=setPending(out,false);ck(!pending(out));
  ck(!pendingMachine("[Graphics]\r\nVideoMemoryMB=abc\r\n").known&&!pendingMachine("").known);}
 std::cout<<"PASS recommended_graphics "<<n<<" checks\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
