#pragma once
// Diagnostic only (2026-10-26, the staff's spell list drawn by the mod itself - user: "the game shows the spells' names, so
// they are in memory; find where it reads them"): at one XR frame, the game's whole readable memory is searched for words
// (UTF-8 and UTF-16LE) and then for 8-byte pointers to what was found (or up to 64 bytes before it). Read only, on the game's
// thread inside the frontend's update (input_ui_observer.hpp gate) - the game stands still while it runs.
// EVR_STRING_SCAN_FRAME=<XR frame>, EVR_STRING_SCAN_HEX=<UTF-8 bytes in hex>/<...>; the results go to the log
// (string_scan_hit, string_scan_pointer, string_scan_done). Without both nothing happens.
// (<algorithm>, <functional>, <unordered_set>: view_resource_probe.cpp, at the global scope)
namespace stringScan {
inline uint64_t frame=0;inline std::vector<std::string> words;inline bool armed=false,done=false;
inline void install(){
    char f[32]{},h[1024]{};
    const auto n=GetEnvironmentVariableA("EVR_STRING_SCAN_FRAME",f,sizeof f),m=GetEnvironmentVariableA("EVR_STRING_SCAN_HEX",h,sizeof h);
    if(!n||n>=sizeof f||!m||m>=sizeof h)return;
    frame=std::strtoull(f,nullptr,10);std::string text(h);size_t at=0;
    while(at<text.size()){auto end=text.find('/',at);if(end==std::string::npos)end=text.size();const auto hex=text.substr(at,end-at);std::string w;
        for(size_t i=0;i+1<hex.size();i+=2)w.push_back(char(std::strtoul(hex.substr(i,2).c_str(),nullptr,16)));if(w.size()>=3)words.push_back(w);at=end+1;}
    armed=frame&&!words.empty();
    logFile->event("string_scan_armed","\"frame\":"+std::to_string(frame)+",\"words\":"+std::to_string(words.size()));
}
struct Hit {uintptr_t address=0;size_t word=0;size_t encoding=0;};   // 0 UTF-8, 1 UTF-16LE, 2 UTF-32LE
inline std::string utf16(const std::string& s){   // UTF-8 to UTF-16LE bytes (the BMP is enough here)
    std::string out;for(size_t i=0;i<s.size();){uint32_t c=uint8_t(s[i]);size_t k=c<0x80?1:c<0xe0?2:c<0xf0?3:4;
        if(k==2)c=((c&0x1f)<<6)|(uint8_t(s[i+1])&0x3f);else if(k==3)c=((c&0x0f)<<12)|((uint8_t(s[i+1])&0x3f)<<6)|(uint8_t(s[i+2])&0x3f);
        out.push_back(char(c&0xff));out.push_back(char(c>>8));i+=k;}
    return out;
}
inline void forEachRegion(const std::function<void(uintptr_t,const std::vector<char>&)>& f){
    uintptr_t at=0x10000;std::vector<char> buffer;
    while(at<0x7ffffffeffffull){
        MEMORY_BASIC_INFORMATION q{};if(!VirtualQuery(reinterpret_cast<void*>(at),&q,sizeof q))break;
        const auto base=reinterpret_cast<uintptr_t>(q.BaseAddress);const auto size=size_t(q.RegionSize);
        const bool readable=q.State==MEM_COMMIT&&!(q.Protect&(PAGE_GUARD|PAGE_NOACCESS))&&
            (q.Protect&(PAGE_READONLY|PAGE_READWRITE|PAGE_WRITECOPY|PAGE_EXECUTE_READ|PAGE_EXECUTE_READWRITE|PAGE_EXECUTE_WRITECOPY));
        if(readable&&size<=(1ull<<30)){
            for(size_t off=0;off<size;off+=(64ull<<20)){const size_t len=std::min<size_t>(64ull<<20,size-off);buffer.resize(len);SIZE_T got=0;
                if(ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(base+off),buffer.data(),len,&got)&&got==len)f(base+off,buffer);}
        }
        at=base+size;if(!size)break;
    }
}
inline void maybe(){
    if(!armed||done||liveFrame.frame<frame)return;
    done=true;
    try{
        const auto started=GetTickCount64();std::vector<Hit> hits;
        std::vector<std::pair<std::string,size_t>> keys;
        // UTF-8, UTF-16LE and UTF-32LE (id 3*i, 3*i+1, 3*i+2)
        for(size_t i=0;i<words.size();++i){keys.push_back({words[i],i*3});const auto w16=utf16(words[i]);keys.push_back({w16,i*3+1});
            std::string w32;for(size_t k=0;k+1<w16.size();k+=2){w32.push_back(w16[k]);w32.push_back(w16[k+1]);w32.push_back(0);w32.push_back(0);}keys.push_back({w32,i*3+2});}
        uint64_t scanned=0;std::vector<unsigned> perKey(keys.size(),0);
        forEachRegion([&](uintptr_t base,const std::vector<char>& b){scanned+=b.size();
            for(size_t k=0;k<keys.size();++k){const auto& key=keys[k].first;const auto id=keys[k].second;auto it=b.begin();
                while(perKey[k]<40){it=std::search(it,b.end(),std::boyer_moore_horspool_searcher(key.begin(),key.end()));if(it==b.end())break;
                    hits.push_back({base+uintptr_t(it-b.begin()),id/3,id%3});++perKey[k];++it;}}});
        for(const auto& h:hits){
            std::string text;SIZE_T got=0;std::vector<char> around(160);
            if(ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(h.address-32),around.data(),around.size(),&got)){std::ostringstream o;for(size_t k=0;k<got;++k)o<<std::hex<<std::setw(2)<<std::setfill('0')<<unsigned(uint8_t(around[k]));text=o.str();}
            MEMORY_BASIC_INFORMATION q{};VirtualQuery(reinterpret_cast<void*>(h.address),&q,sizeof q);
            logFile->event("string_scan_hit","\"word\":"+std::to_string(h.word)+",\"encoding\":"+std::to_string(h.encoding)+",\"address\":"+hex(h.address)+
                ",\"allocation\":"+hex(reinterpret_cast<uintptr_t>(q.AllocationBase))+",\"type\":"+std::to_string(q.Type)+",\"exe_rva\":"+hex(h.address>=baseAddress&&h.address<baseAddress+0x4000000?h.address-baseAddress:0)+
                ",\"bytes_from_minus_32\":\""+text+"\"");
        }
        // the pointers to them (or up to 64 bytes before them)
        std::unordered_set<uint64_t> targets;for(const auto& h:hits)for(uint64_t d=0;d<=64;d+=8)targets.insert(uint64_t(h.address-d));
        unsigned pointers=0;
        if(!targets.empty())forEachRegion([&](uintptr_t base,const std::vector<char>& b){
            const auto* q=reinterpret_cast<const uint64_t*>(b.data());const size_t n=b.size()/8;
            for(size_t i=0;i<n&&pointers<400;++i)if(targets.count(q[i])){++pointers;
                logFile->event("string_scan_pointer","\"at\":"+hex(base+i*8)+",\"to\":"+hex(uintptr_t(q[i]))+",\"exe_rva\":"+hex(base+i*8>=baseAddress&&base+i*8<baseAddress+0x4000000?base+i*8-baseAddress:0));}});
        logFile->event("string_scan_done","\"hits\":"+std::to_string(hits.size())+",\"pointers\":"+std::to_string(pointers)+",\"scanned_mb\":"+std::to_string(scanned>>20)+",\"ms\":"+std::to_string(GetTickCount64()-started));
    }catch(const std::exception& e){logFile->event("string_scan_failure","\"reason\":"+jsonString(e.what()));}
}
}
