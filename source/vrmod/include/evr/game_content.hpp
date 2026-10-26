#pragma once
// The game's own content - font files and icon pixels - read from the game's own files at run time (2026-10-26, the staff's spell
// list beside the aim mark; user: "use the game's own font and icons"). Nothing of the game's is copied into the mod: the mod
// carries only the keys (include/evr/ammo_names.hpp, generated from the game data).
// enshrouded.kfc (KFC3/KFC4, < 32 MB): at 16 + 8 i a table of {u32 offset from that field, u32 count} for the arrays version,
// containers, unused0, unused1, resource_locations, resource_indices, content_buckets, content_keys (7: 16 bytes each - size and
// three hash words, as ContentKey), content_values (8: 16 bytes each - u32 offset, u16 flags, u16 container). A content with
// flags 0 is `size` plain bytes at `offset` of enshrouded_<container, 3 digits>.dat (as docs/analysis/weapon_mount_20261010/
// kfc_readonly.py read_content, layout after Brabb3l/kfc-parser). Read only; the files are opened shared.
#include <windows.h>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <mutex>
#include <string>
#include <vector>
#include <evr/ammo_names.hpp>
namespace evr::content {
class Index {
    std::filesystem::path root;std::vector<uint8_t> blob;uint32_t keysAt=0,valuesAt=0,count=0,containers=0;bool tried=false,ok=false;std::string why;
    std::mutex lock;
    static bool readFile(const std::filesystem::path& p,uint64_t offset,uint32_t size,std::vector<uint8_t>& out){
        HANDLE f=CreateFileW(p.c_str(),GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
        if(f==INVALID_HANDLE_VALUE)return false;
        LARGE_INTEGER at;at.QuadPart=LONGLONG(offset);LARGE_INTEGER end{};
        bool good=GetFileSizeEx(f,&end)&&uint64_t(end.QuadPart)>=offset+size&&SetFilePointerEx(f,at,nullptr,FILE_BEGIN);
        if(good){out.resize(size);DWORD got=0;good=ReadFile(f,out.data(),size,&got,nullptr)&&got==size;}
        CloseHandle(f);return good;
    }
    bool load(){
        if(tried)return ok;tried=true;
        const auto path=root/"enshrouded.kfc";std::error_code ec;const auto size=std::filesystem::file_size(path,ec);
        if(ec||size<144||size>(32ull<<20)){why="enshrouded.kfc missing or of an unexpected size";return false;}
        if(!readFile(path,0,uint32_t(size),blob)){why="enshrouded.kfc unreadable";return false;}
        if(std::memcmp(blob.data(),"KFC4",4)&&std::memcmp(blob.data(),"KFC3",4)){why="not a KFC3/KFC4 index";return false;}
        const auto field=[&](unsigned i,uint32_t& at,uint32_t& n){const uint32_t f=16+8*i;uint32_t rel=0;std::memcpy(&rel,blob.data()+f,4);std::memcpy(&n,blob.data()+f+4,4);at=f+rel;};
        uint32_t c=0,n=0,v=0,nv=0;field(1,c,containers);field(7,keysAt,n);field(8,valuesAt,nv);
        if(n!=nv||!n||n>2000000||uint64_t(keysAt)+uint64_t(n)*16>blob.size()||uint64_t(valuesAt)+uint64_t(n)*16>blob.size()){why="content table out of bounds";return false;}
        count=n;ok=true;return true;
    }
  public:
    explicit Index(std::filesystem::path gameRoot):root(std::move(gameRoot)){}
    // The game's folder: where the running executable is.
    static std::filesystem::path gameFolder(){wchar_t p[MAX_PATH]{};const auto n=GetModuleFileNameW(nullptr,p,MAX_PATH);return n&&n<MAX_PATH?std::filesystem::path(p).parent_path():std::filesystem::path();}
    const std::string& failure() const {return why;}
    // The content's bytes; false when the index or the content is not there (a game update may move or change it).
    bool read(const evr::ammo::ContentKey& key,std::vector<uint8_t>& out){
        std::lock_guard<std::mutex> l(lock);
        if(!load()||!key.size||key.size>(32u<<20))return false;
        uint8_t want[16];std::memcpy(want,&key.size,4);std::memcpy(want+4,&key.hash0,4);std::memcpy(want+8,&key.hash1,4);std::memcpy(want+12,&key.hash2,4);
        for(uint32_t i=0;i<count;++i){
            if(std::memcmp(blob.data()+keysAt+16*size_t(i),want,16))continue;
            uint32_t offset=0;uint16_t flags=0,container=0;const auto* v=blob.data()+valuesAt+16*size_t(i);
            std::memcpy(&offset,v,4);std::memcpy(&flags,v+4,2);std::memcpy(&container,v+6,2);
            if(flags||container>=containers){why="unsupported content flags or container";return false;}
            wchar_t name[32];swprintf(name,32,L"enshrouded_%03u.dat",unsigned(container));
            if(!readFile(root/name,offset,key.size,out)){why="content file unreadable";return false;}
            return true;
        }
        why="content key not found";return false;
    }
};
}
