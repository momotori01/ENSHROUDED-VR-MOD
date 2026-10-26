// ammo_panel_preview <game folder> <language index> <out.rgba> [chosen [chosen count / name, other count / name variants]]: the staff's spell list as hooks/spell_panel.hpp paints it
// (include/evr/ammo_panel_paint.hpp, the game's own font files and icons read from the game's files), for comparing it with the
// game's own list without starting the game (2026-10-26). Rows: Fire Ball I 20, Eternal Ice Bolt I, Chain Lightning I 76 - the
// list of the Mock save. Writes the RGBA bytes (straight alpha) and prints width, height, anchor and whether the fonts loaded.
#include <evr/ammo_panel_paint.hpp>
#include <cstdio>
#include <fstream>
int wmain(int argc,wchar_t** argv){
    if(argc<4){std::fprintf(stderr,"usage: ammo_panel_preview <game folder> <language index> <out.rgba> [chosen]\n");return 2;}
    evr::ammopanel::Assets assets{std::filesystem::path(argv[1])};
    const unsigned lang=unsigned(_wtoi(argv[2]));const size_t chosen=argc>4?size_t(_wtoi(argv[4])):0;
    const std::vector<evr::ammopanel::Row> rows{{3918049792u,20,false},{1390932160u,3,true},{1048548069u,76,false}};
    evr::ammopanel::Style style;if(argc>8){style.chosenCount=unsigned(_wtoi(argv[5]));style.chosenName=unsigned(_wtoi(argv[6]));style.otherCount=unsigned(_wtoi(argv[7]));style.otherName=unsigned(_wtoi(argv[8]));}
    if(argc>11){style.chosenCountEmbolden=float(_wtof(argv[9]));style.chosenNameEmbolden=float(_wtof(argv[10]));style.otherEmbolden=float(_wtof(argv[11]));}
    const auto p=evr::ammopanel::paint(assets,rows,chosen,lang,style);
    std::ofstream f(argv[3],std::ios::binary);f.write(reinterpret_cast<const char*>(p.rgba.data()),std::streamsize(p.rgba.size()));
    std::printf("{\"width\":%u,\"height\":%u,\"anchor\":%.4f,\"fonts\":%s,\"failure\":\"%s\"}\n",p.width,p.height,p.anchor,p.fonts?"true":"false",assets.failure().c_str());
    return p.rgba.empty()?1:0;
}
