#include <evr/mod_update.hpp>
#include <iostream>
#include <stdexcept>
#include <string>
using namespace evr::modupdate;
int main(){try{unsigned n=0;auto ck=[&](bool b){++n;if(!b)throw std::runtime_error(std::to_string(n));};
 // Versions: three parts of digits; nothing else.
 ck(parseVersion("0.9.9")==Version{0,9,9}&&parseVersion("1.10.0")==Version{1,10,0}&&parseVersion("12.0.345")==Version{12,0,345});
 for(const char* bad:{"","0.9","0.9.9.1","v0.9.9","0.9.x","0..9","0.9.9 ","1.2.3-beta","9999999.0.0"})ck(!parseVersion(bad));
 ck(newer({0,9,10},{0,9,9})&&newer({1,0,0},{0,9,99})&&!newer({0,9,9},{0,9,9})&&!newer({0,9,2},{0,9,9}));
 ck(text({0,9,12})=="0.9.12");
 // The release's tag as the updater accepts it.
 ck(tagVersion("EnshroudedVR-0.9.12")==Version{0,9,12}&&!tagVersion("v0.9.12")&&!tagVersion("EnshroudedVR-0.9")&&!tagVersion("EnshroudedVR-"));
 // Fields of the manifest (tools/pack_release.ps1 writes it with ConvertTo-Json: "key":  "value", two spaces).
 const std::string manifest="{\r\n  \"name\":  \"Enshrouded VR MOD\",\r\n  \"version\":  \"0.9.9\",\r\n  \"packed\":  \"20261019-164709\",\r\n  \"repository\":  \"momotori01/ENSHROUDED-VR-MOD\",\r\n  \"files\": [{\"path\": \"dinput8.dll\"}]\r\n}";
 const auto i=installed(manifest);ck(i.version==Version{0,9,9}&&i.repository=="momotori01/ENSHROUDED-VR-MOD");
 ck(!installed("{}").version&&installed("{}").repository==defaultRepository);
 ck(installed("{\"version\":\"1.0.0\",\"repository\":\"evil/../x y\"}").repository==defaultRepository);   // an odd repository is not used
 ck(repositoryValid("a/b")&&!repositoryValid("a")&&!repositoryValid("/b")&&!repositoryValid("a/")&&!repositoryValid("a/b/c")&&!repositoryValid("a b/c"));
 // A key that appears as a value first is skipped to the real one.
 ck(stringField("{\"note\":\"version\",\"version\":\"2.0.0\"}","version")=="2.0.0");
 ck(!stringField("{\"version\":1}","version")&&!stringField("{\"version\":\"a\\\"b\"}","version"));
 // GitHub's answer against the installed version.
 const std::string answer="{\"url\":\"https://api.github.com/x\",\"tag_name\": \"EnshroudedVR-0.9.10\",\"name\":\"0.9.10\",\"body\":\"tag_name EnshroudedVR-9.9.9\"}";
 ck(compare(i,answer)==State::Available);
 ck(compare(installed("{\"version\":\"0.9.10\"}"),answer)==State::UpToDate&&compare(installed("{\"version\":\"0.9.11\"}"),answer)==State::UpToDate);
 ck(compare(installed("{}"),answer)==State::Unknown&&compare(i,"{\"message\":\"Not Found\"}")==State::Unknown&&compare(i,"{\"tag_name\":\"release-1\"}")==State::Unknown);
 // The slow blink: 1.2 s shown, 0.8 s not.
 ck(blinkShown(0)&&blinkShown(1199)&&!blinkShown(1200)&&!blinkShown(1999)&&blinkShown(2000)&&blinkShown(123456789ull*2000+5));
 ck(noticeShown(2,0)&&!noticeShown(2,1500)&&noticeShown(3,1500)&&!noticeShown(1,0)&&!noticeShown(0,0));
 std::cout<<"mod_update "<<n<<" PASS\n";return 0;}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
