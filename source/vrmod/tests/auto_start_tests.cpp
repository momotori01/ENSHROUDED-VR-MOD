#include <algorithm>
#include <evr/auto_start.hpp>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
using namespace evr::autostart;
// With <log> <scene> <view>: how the loader reads a proof log that a real run wrote.
int main(int argc,char** argv){try{unsigned n=0;
 if(argc==4){std::ifstream f(argv[1],std::ios::binary);std::ostringstream s;s<<f.rdbuf();std::string reason;
  const bool ok=f&&proofAccepted(s.str(),argv[2],argv[3],reason);std::cout<<(ok?"accepted":"refused: "+reason)<<"\n";return ok?0:1;}auto ck=[&](bool b,int line=__builtin_LINE()){++n;if(!b)throw std::runtime_error(std::to_string(n)+" at line "+std::to_string(line));};
 // The game's settings file: a field is found only where it stands exactly once, as "name": "value".
 const std::string json="{\r\n  \"graphics\": {\r\n    \"vsync\": false,\r\n    \"render\": {\r\n      \"antiAliasing\": \"Fsr3\",\r\n      \"dlss\": \"Balanced\",\r\n      \"fsr3\": \"Quality\",\r\n      \"shadows\": \"High\"\r\n    }\r\n  }\r\n}\r\n";
 {size_t b=0,e=0;ck(field(json,"antiAliasing",b,e)&&json.substr(b,e-b)=="Fsr3");ck(field(json,"dlss",b,e)&&json.substr(b,e-b)=="Balanced");
  ck(!field(json,"missing",b,e)&&!field(json+json,"dlss",b,e)&&!field(json,"vsync",b,e)&&!field(json,"render",b,e));}
 // A VR start: antiAliasing Dlss and the chosen mode; everything else stays byte for byte; the record holds what stood there.
 {std::string out,reason;RenderRecord r;ck(applyVr(json,"Quality",nullptr,out,r,reason));
  ck(out.size()==json.size()-1&&out.find("\"antiAliasing\": \"Dlss\"")!=std::string::npos&&out.find("\"dlss\": \"Quality\"")!=std::string::npos&&out.find("\"fsr3\": \"Quality\"")!=std::string::npos);
  ck(r.changes.size()==3&&r.changes[0].name=="antiAliasing"&&r.changes[0].original=="Fsr3"&&r.changes[0].applied=="Dlss"&&r.changes[1].original=="Balanced"&&r.changes[1].applied=="Quality");
  // FSR 3 (a graphics card without DLSS) at the same scale.
  ck(r.changes[2].name=="fsr3"&&r.changes[2].original=="Quality"&&r.changes[2].applied=="Quality");
  // The record as a file and back.
  RenderRecord parsed;ck(parseRecord(recordText(r),parsed)&&parsed.changes.size()==3&&parsed.changes[1].name=="dlss"&&parsed.changes[1].original=="Balanced");
  ck(!parseRecord("",parsed)&&!parseRecord("EVR_RENDER_SETTINGS_2\r\n",parsed)&&!parseRecord("EVR_RENDER_SETTINGS_1\r\ndlss only\r\n",parsed)&&!parseRecord("EVR_RENDER_SETTINGS_1\r\nd\"x a b\r\n",parsed));
  // Put back: the file is the one from before, byte for byte.
  std::vector<std::string> names;ck(restore(out,r,&names)==json&&names.size()==2);
  // The player chose another upscaling in the game while in VR: that stays, only what still is the VR value goes back.
  auto changed=out;const auto at=changed.find("\"dlss\": \"Quality\"");changed.replace(at,17,"\"dlss\": \"Performance\"");names.clear();
  const auto back=restore(changed,r,&names);ck(names==std::vector<std::string>{"antiAliasing"}&&back.find("\"antiAliasing\": \"Fsr3\"")!=std::string::npos&&back.find("\"dlss\": \"Performance\"")!=std::string::npos);
  // A second VR start before anything was put back keeps the first start's originals; another mode is written.
  std::string again;RenderRecord second;ck(applyVr(out,"Performance",&r,again,second,reason)&&second.changes[0].original=="Fsr3"&&second.changes[0].applied=="Dlss"&&second.changes[1].original=="Balanced"&&second.changes[1].applied=="Performance"&&
   second.changes[2].original=="Quality"&&second.changes[2].applied=="Performance"&&again.find("\"fsr3\": \"Performance\"")!=std::string::npos);
  ck(restore(again,second)==json);
  // The same start again changes nothing.
  std::string same;RenderRecord third;ck(applyVr(out,"Quality",&r,same,third,reason)&&same==out&&third.changes[1].original=="Balanced");}
 // DLAA is an antiAliasing value; the player's upscaling mode is left.
 {std::string out,reason;RenderRecord r;ck(applyVr(json,"Dlaa",nullptr,out,r,reason)&&out.find("\"antiAliasing\": \"Dlaa\"")!=std::string::npos&&out.find("\"dlss\": \"Balanced\"")!=std::string::npos&&out.find("\"fsr3\": \"Native\"")!=std::string::npos&&restore(out,r)==json);
  // DLSS Balanced is FSR 3 Balance; a settings file without fsr3 is changed as before.
  {std::string o,why;RenderRecord rec;ck(applyVr(json,"Balanced",nullptr,o,rec,why)&&o.find("\"fsr3\": \"Balance\"")!=std::string::npos&&restore(o,rec)==json);
   const std::string noFsr=json.substr(0,json.find("      \"fsr3\""))+json.substr(json.find("      \"shadows\""));
   ck(applyVr(noFsr,"Performance",nullptr,o,rec,why)&&rec.changes.size()==2&&restore(o,rec)==noFsr);}
  // no mode (2026-10-23, the setting exe's DLSS row gone): antiAliasing Dlss, the game's own dlss and fsr3 kept; a DLAA kept
  {std::string o,why;RenderRecord rec;ck(applyVr(json,"",nullptr,o,rec,why)&&o.find("\"antiAliasing\": \"Dlss\"")!=std::string::npos&&o.find("\"dlss\": \"Balanced\"")!=std::string::npos&&restore(o,rec)==json);
   std::string dlaa=json;const auto at=dlaa.find("\"antiAliasing\": \"Fsr3\"");ck(at!=std::string::npos);dlaa.replace(at,std::string("\"antiAliasing\": \"Fsr3\"").size(),"\"antiAliasing\": \"Dlaa\"");
   std::string d;RenderRecord drec;ck(applyVr(dlaa,"",nullptr,d,drec,why)&&d.find("\"antiAliasing\": \"Dlaa\"")!=std::string::npos&&d.find("\"dlss\": \"Balanced\"")!=std::string::npos);}
  ck(!applyVr("{}","Quality",nullptr,out,r,reason)&&!reason.empty()&&out=="{}");
  // HDR (2026-10-23): on in the game -> off for VR, recorded, put back; off is left; vsync (another true/false) untouched.
 {const std::string hdr=json.substr(0,json.find("    \"render\""))+"    \"hdrSettings\": {\r\n      \"enableHdr\": true\r\n    },\r\n"+json.substr(json.find("    \"render\""));
  size_t b=0,e=0;ck(flag(hdr,"enableHdr",b,e)&&hdr.substr(b,e-b)=="true"&&flag(hdr,"vsync",b,e)&&!field(hdr,"enableHdr",b,e));
  std::string o,why;RenderRecord rec;ck(applyVr(hdr,"Quality",nullptr,o,rec,why)&&o.find("\"enableHdr\": false")!=std::string::npos&&o.find("\"vsync\": false")!=std::string::npos);
  ck(rec.changes.size()==4&&rec.changes[3].name=="enableHdr"&&rec.changes[3].original=="true"&&rec.changes[3].applied=="false");
  RenderRecord parsed;ck(parseRecord(recordText(rec),parsed)&&parsed.changes.size()==4);
  std::vector<std::string> names;ck(restore(o,parsed,&names)==hdr&&std::find(names.begin(),names.end(),"enableHdr")!=names.end());
  // a second VR start before anything was put back: no change to the file, the original "true" kept for the way back
  std::string again;RenderRecord second;ck(applyVr(o,"Quality",&rec,again,second,why)&&again==o&&second.changes.size()==4&&second.changes[3].original=="true"&&restore(again,second)==hdr);
  const auto at=hdr.find("true\r\n    },");const std::string off=hdr.substr(0,at)+"false"+hdr.substr(at+4);
  ck(applyVr(off,"Quality",nullptr,o,rec,why)&&rec.changes.size()==3&&o.find("\"enableHdr\": false")!=std::string::npos);}
 // The GI quality of a VR start (2026-10-15): put with the others, recorded, Keep leaves it, a missing field refuses.
  const std::string gi=json.substr(0,json.find("\"shadows\""))+"\"dynamicGi\": \"Performance\",\r\n      "+json.substr(json.find("\"shadows\""));
  {std::string o,why;RenderRecord rec;ck(applyVr(gi,"Quality",nullptr,o,rec,why,"High")&&o.find("\"dynamicGi\": \"High\"")!=std::string::npos&&rec.changes.size()==4&&rec.changes[3].name=="dynamicGi"&&rec.changes[3].original=="Performance");
   const auto back=restore(o,rec);ck(back==gi);
   std::string k,whyK;RenderRecord keep;ck(applyVr(gi,"Quality",nullptr,k,keep,whyK,"Keep")&&keep.changes.size()==3&&k.find("\"dynamicGi\": \"Performance\"")!=std::string::npos);
   std::string m,whyM;RenderRecord missing;ck(!applyVr(json,"Quality",nullptr,m,missing,whyM,"High")&&!whyM.empty());}
  ck(!applyVr("{\"antiAliasing\": \"a b\",\"dlss\": \"Quality\"}","Quality",nullptr,out,r,reason));}
 // The environment of a VR start: the layer and the loader come from the package, the logs go to this start's folder.
 {const Paths p{L"G:\\game",L"G:\\game\\EnshroudedVR",L"G:\\game\\EnshroudedVR\\bin",L"G:\\game\\EnshroudedVR\\data",L"G:\\game\\EnshroudedVR\\logs\\20261014-120000"};
  const auto value=[](const Environment& e,const wchar_t* name)->std::wstring{for(const auto& v:e)if(v.first==name)return v.second;return L"?";};
  const auto e=vrEnvironment(p,{L"aa",L"bb"},true,1.);
  ck(value(e,L"EVR_KNOWN_HEIGHT_FILE")==L"G:\\game\\EnshroudedVR\\run\\known-height.txt");
  ck(value(e,L"EVR_SCREEN_XR")==L"1"&&value(e,L"EVR_SCREEN_XR_SCALE")==L"1.0"&&value(vrEnvironment(p,{},false),L"EVR_SCREEN_XR_SCALE")==L"0.8");
  ck(value(e,L"EVR_LAYER_OUTPUT")==p.log&&value(e,L"VK_LAYER_PATH")==p.bin&&value(e,L"VK_INSTANCE_LAYERS")==L"VK_LAYER_EVR_diagnostic"&&value(e,L"EVR_PREPARE_LOADER")==L"G:\\game\\EnshroudedVR\\bin\\openxr_loader.dll");
  ck(value(e,L"EVR_LIVE_NATIVE_XR")==L"1"&&value(e,L"EVR_PREPARE_OPENXR")==L"1"&&value(e,L"EVR_UI_QUAD")==L"1"&&value(e,L"EVR_ONLINE_SYNC")==L"1"&&value(e,L"EVR_PLAY_LOG")==L"1"&&value(e,L"EVR_MOCK_HAND_SEQUENCE").empty());
  ck(value(e,L"EVR_SHADOW_RECEIVER_SHADER")==L"G:\\game\\EnshroudedVR\\data\\shadow_receiver.spv"&&value(e,L"EVR_SHADOW_RECEIVER_SHADER_SHA256")==L"aa"&&value(e,L"EVR_GI_PROBE_SHADER_SHA256")==L"bb");
  ck(value(e,L"XR_RUNTIME_JSON")==L"?");   // the PC's own OpenXR runtime unless the settings file names one
  const auto bare=vrEnvironment(p,{},false);ck(value(bare,L"EVR_SHADOW_RECEIVER_SHADER").empty()&&value(bare,L"EVR_GI_PROBE_SHADER_SHA256").empty()&&value(bare,L"EVR_ONLINE_SYNC").empty());
  ck(value(bare,L"EVR_ALTERNATE_EYES").empty()&&value(vrEnvironment(p,{},false,.8,true),L"EVR_ALTERNATE_EYES")==L"1");   // one eye a frame only when switched on
  ck(value(vrEnvironment(p,{},false,.8,true,1),L"EVR_EYE_WARP")==L"1"&&value(vrEnvironment(p,{},false,.8,true,2),L"EVR_EYE_WARP")==L"2"&&value(vrEnvironment(p,{},false,.8,false,2),L"EVR_EYE_WARP").empty()&&value(vrEnvironment(p,{},false,.8,true),L"EVR_EYE_WARP").empty());   // the warp only with one eye a frame
  for(const auto& v:e){size_t same=0;for(const auto& w:e)same+=v.first==w.first;ck(same==1);}
  ck(expanded(L"%LOG%\\mock.jsonl",p)==p.log+L"\\mock.jsonl"&&expanded(L"%GAME%;%MOD%;%GAME%",p)==L"G:\\game;G:\\game\\EnshroudedVR;G:\\game"&&expanded(L"plain",p)==L"plain");
  ck(overridable("EVR_GRIP_TRIM")&&overridable("XR_RUNTIME_JSON")&&!overridable("PATH")&&!overridable("VK_INSTANCE_LAYERS"));}
 // The files of a session that follow the settings.
 {evr::settings::Values v;v.renderScale=1.;v.sharpness=.5;const auto f=sessionFiles(v,"0x29c8a495600","0x29c8cb94060");
  ck(f.size()==4&&f.at("world-resource-proof.txt")=="EVR_WORLD_RESOURCE_1 0x29c8a495600 0x29c8cb94060\n"&&f.at("live-xr-config.txt")=="EVR_LIVE_XR_1 3000 1.0\n"&&f.at("world-vr-sharpness.txt")=="EVR_VR_SHARPNESS_1 0.5\n"&&f.at("world-session.txt")=="EVR_WORLD_SESSION_1 0\n");}
 // The world the waiting found.
 {std::string s,v;ck(worldFound("EVR_WORLD_FOUND_1 0x29c8a495600 0x29c8cb94060\n",s,v)&&s=="0x29c8a495600"&&v=="0x29c8cb94060");
  ck(!worldFound("",s,v)&&!worldFound("EVR_WORLD_FOUND_2 0x1 0x2",s,v)&&!worldFound("EVR_WORLD_FOUND_1 0x29C8 0x2000",s,v)&&!worldFound("EVR_WORLD_FOUND_1 29c8a495600 0x29c8cb94060",s,v)&&!worldFound("EVR_WORLD_FOUND_1 0x29c8a495600",s,v));}
 // The unused proof's log, as the development tool reads it.
 {const std::string scene="0x29c8a495600",view="0x29c8cb94060";
  const std::string good=
   "{\"us\":1,\"event\":\"unused_world_resource_diagnostic\",\"stage\":2,\"original_features\":1,\"drawn\":false}\n"
   "{\"us\":2,\"event\":\"native_view_created\",\"slot\":0,\"view\":\"0x29ca149fd10\",\"original_view\":\"0x29c8cb94060\",\"scene\":\"0x29c8a495600\",\"rendered\":false,\"features\":0}\n"
   "{\"us\":3,\"event\":\"native_view_created\",\"slot\":1,\"view\":\"0x29ca50d6120\",\"original_view\":\"0x29c8cb94060\",\"scene\":\"0x29c8a495600\",\"rendered\":false,\"features\":0}\n"
   "{\"us\":4,\"event\":\"culling_history_created\",\"index\":0}\n{\"us\":4,\"event\":\"culling_history_created\",\"index\":1}\n"
   "{\"us\":5,\"event\":\"water_feedback_pool_created\",\"pool\":\"0x1\"}\n{\"us\":5,\"event\":\"water_feedback_pool_created\",\"pool\":\"0x2\"}\n"
   "{\"us\":6,\"event\":\"water_feedback_pool_released\",\"pool\":\"0x1\"}\n{\"us\":6,\"event\":\"water_feedback_pool_released\",\"pool\":\"0x2\"}\n"
   "{\"us\":7,\"event\":\"culling_history_released\",\"index\":0}\n{\"us\":7,\"event\":\"culling_history_released\",\"index\":1}\n"
   "{\"us\":8,\"event\":\"native_view_probe_complete\",\"two_views_destroyed\":true,\"original_view_unchanged\":true,\"scene_time_unchanged\":true,\"back_pointer_restored\":true}\n"
   "{\"us\":9,\"event\":\"view_resource_hook_complete\",\"original_bytes_restored\":true,\"trampoline_retained\":true,\"result\":0}\n";
  std::string reason;ck(proofAccepted(good,scene,view,reason)&&reason.empty());
  const auto without=[&](const std::string& part,const std::string& with=""){auto s=good;const auto at=s.find(part);if(at==std::string::npos)throw std::runtime_error("fixture "+part);s.replace(at,part.size(),with);return s;};
  ck(!proofAccepted(good,scene,"0x29c8cb94061",reason)&&reason.find("world that was found")!=std::string::npos);
  ck(!proofAccepted(without("\"back_pointer_restored\":true","\"back_pointer_restored\":false"),scene,view,reason));
  ck(!proofAccepted(without("\"event\":\"culling_history_released\",\"index\":1"),scene,view,reason)&&reason.find("histories")!=std::string::npos);
  ck(!proofAccepted(without("\"event\":\"water_feedback_pool_released\",\"pool\":\"0x2\""),scene,view,reason)&&reason.find("water")!=std::string::npos);
  ck(!proofAccepted(good+"{\"us\":9,\"event\":\"water_feedback_failure\"}\n",scene,view,reason));
  ck(!proofAccepted(without("\"result\":0","\"result\":1"),scene,view,reason)&&reason.find("hook")!=std::string::npos);
  ck(!proofAccepted(without("\"stage\":2","\"stage\":1"),scene,view,reason)&&!proofAccepted("",scene,view,reason));}
 // The layer's log: OpenXR prepared, refused (with the layer's reason), or not known yet.
 {std::string reason;
  ck(xrPrepared("{\"us\":1,\"event\":\"preflight_runtime\",\"name\":\"SteamVR/OpenXR\"}\n{\"us\":2,\"event\":\"preflight_gpu_validation_deferred\",\"instance\":\"0x1\"}\n",reason)==1&&reason.empty());
  ck(xrPrepared("{\"us\":1,\"event\":\"layer_loaded\"}\n",reason)==-1&&xrPrepared("",reason)==-1&&xrPrepared("{\"us\":1,\"event\":\"preflight_runtime\",\"name\":\"x\"}\n",reason)==-1);
  ck(xrPrepared("{\"us\":1,\"event\":\"preflight_fallback\",\"phase\":\"instance_plan\",\"reason\":\"preflight xrCreateInstance XrResult=-51\"}\n{\"us\":2,\"event\":\"x\"}\n",reason)==0&&
     reason=="OpenXR could not be prepared (phase:instance_plan,reason:preflight xrCreateInstance XrResult=-51)");
  ck(xrPrepared("{\"us\":1,\"event\":\"preflight_runtime\",\"name\":\"x\"}\n{\"us\":2,\"event\":\"preflight_fallback\",\"phase\":\"device_create\",\"result\":-3}",reason)==0&&reason.find("device_create,result:-3")!=std::string::npos);
  ck(xrPrepared("{\"us\":1,\"event\":\"preflight_disabled\",\"reason\":\"unsupported_loaded_game_build\"}\n",reason)==0);}
 std::cout<<"auto_start "<<n<<" PASS\n";return 0;}
 catch(const std::exception& e){std::cerr<<"auto_start FAIL "<<e.what()<<"\n";return 1;}}
