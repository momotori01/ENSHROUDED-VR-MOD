#pragma once
#include <evr/head_spirv_patch.hpp>

namespace evr::vfx {
inline constexpr const char* writebackSha="6f2733ba62e68ccf501b03049c39c353514c51a1362918c02f713dd0d1ed2131";
inline constexpr uint32_t payloadMarker=0x45565254;
// Extended binding-0 payload: native destination counter byte offset, marker,
// source counter byte offset, output capacity, and three float translation bits.
// The native four-byte payload retains native behavior. Extended mode appends
// records and rebases position only; it does not tick or mutate source particles.
inline std::vector<uint32_t> patchWriteback(const uint32_t* source,size_t bytes){
    if(bytes!=2720||head::sha256(source,bytes)!=writebackSha)throw std::runtime_error("unreviewed VFX writeback shader");
    std::vector<uint32_t> w(source,source+bytes/4);
    if(w[0]!=0x07230203||w[1]!=0x10500||w[3]!=79||w[550]!=0x50036||w[551]!=49||w[552]!=1)
        throw std::runtime_error("VFX writeback SPIR-V layout");
    uint32_t next=w[3];std::vector<uint32_t> declarations,body;
    auto emit=[](std::vector<uint32_t>& to,uint32_t opcode,std::initializer_list<uint32_t> operands){
        to.push_back((uint32_t(operands.size()+1)<<16)|opcode);to.insert(to.end(),operands.begin(),operands.end());};
    auto constant=[&](uint32_t value){const auto id=next++;emit(declarations,43,{35,id,value});return id;};
    auto unary=[&](uint32_t op,uint32_t type,uint32_t a){const auto id=next++;emit(body,op,{type,id,a});return id;};
    auto binary=[&](uint32_t op,uint32_t type,uint32_t a,uint32_t b){const auto id=next++;emit(body,op,{type,id,a,b});return id;};
    const auto one=constant(1),three=constant(3),four=constant(4),five=constant(5),six=constant(6),seven=constant(7);
    const auto marker=constant(payloadMarker),ten=constant(10);
    const auto falseId=next++;emit(declarations,42,{53,falseId});
    const auto zeroFloat=next++;emit(declarations,43,{32,zeroFloat,0});
    const auto zeroVector=next++;emit(declarations,44,{41,zeroVector,zeroFloat,zeroFloat,zeroFloat});
    auto label=[&](uint32_t id){emit(body,248,{id});};
    auto arraySize=[&](uint32_t variable){const auto id=next++;emit(body,68,{35,id,variable,0});return id;};
    auto accessWord=[&](uint32_t variable,uint32_t index,bool wrapped){const auto id=next++;
        if(wrapped)emit(body,65,{52,id,variable,34,index,34});else emit(body,65,{52,id,variable,34,index});return id;};
    auto payload=[&](uint32_t index){return unary(61,35,accessWord(3,index,true));};
    auto abortIf=[&](uint32_t bad){const auto rejected=next++,continued=next++;
        emit(body,247,{continued,0});emit(body,250,{bad,rejected,continued});label(rejected);emit(body,253,{});label(continued);};
    auto loadRecord=[&](uint32_t invocation,bool checked){
        const auto sortPtr=next++;emit(body,65,{54,sortPtr,4,34,invocation});
        const auto sort=unary(61,19,sortPtr);const auto sourceIndex=next++;emit(body,81,{35,sourceIndex,sort,2});
        if(checked)abortIf(binary(174,53,sourceIndex,arraySize(5)));
        const auto ptr=next++;emit(body,65,{55,ptr,5,34,sourceIndex});return unary(61,21,ptr);
    };
    auto storeRecord=[&](uint32_t index,uint32_t record){
        const auto ptr=next++;emit(body,65,{55,ptr,6,34,index});emit(body,62,{ptr,record});
        const auto field=next++;emit(body,65,{52,field,6,34,index,38});emit(body,62,{field,index});
    };
    emit(body,54,{49,1,0,50});
    const auto entry=next++,header=next++,headerEnd=next++;label(entry);
    const auto thread=unary(61,47,2),invocation=next++;emit(body,81,{35,invocation,thread,0});
    const auto destOffset=payload(36),destIndex=binary(194,35,destOffset,37);
    const auto fullHeader=binary(170,53,arraySize(3),seven);
    emit(body,247,{headerEnd,0});emit(body,250,{fullHeader,header,headerEnd});label(header);
    const auto tag=payload(one),sourceOffset=payload(37),capacity=payload(three);
    const auto dx=unary(124,32,payload(four)),dy=unary(124,32,payload(five)),dz=unary(124,32,payload(six));
    const auto delta=next++;emit(body,80,{41,delta,dx,dy,dz});
    auto valid=binary(167,53,binary(170,53,tag,marker),binary(171,53,sourceOffset,destOffset));
    valid=binary(167,53,valid,binary(170,53,binary(199,35,sourceOffset,three),36));
    emit(body,249,{headerEnd});label(headerEnd);
    const auto enabled=next++,selectedSource=next++,selectedCapacity=next++,selectedDelta=next++;
    emit(body,245,{53,enabled,falseId,entry,valid,header});
    emit(body,245,{35,selectedSource,destOffset,entry,sourceOffset,header});
    emit(body,245,{35,selectedCapacity,36,entry,capacity,header});
    emit(body,245,{41,selectedDelta,zeroVector,entry,delta,header});
    const auto native=next++,extended=next++,finished=next++;
    emit(body,247,{finished,0});emit(body,250,{enabled,extended,native});label(native);
    const auto nativeCount=unary(61,35,accessWord(7,destIndex,false));
    abortIf(binary(174,53,invocation,nativeCount));
    storeRecord(invocation,loadRecord(invocation,false));emit(body,249,{finished});
    label(extended);
    const auto sourceCounterIndex=binary(194,35,selectedSource,37),counterLength=arraySize(7);
    abortIf(binary(166,53,binary(174,53,sourceCounterIndex,counterLength),binary(174,53,destIndex,counterLength)));
    const auto sourceCount=unary(61,35,accessWord(7,sourceCounterIndex,false));
    abortIf(binary(166,53,binary(174,53,invocation,sourceCount),binary(174,53,invocation,arraySize(4))));
    const auto record=loadRecord(invocation,true);
    const auto counter=accessWord(7,destIndex,false),slot=next++;
    emit(body,234,{35,slot,counter,one,36,one}); // Device scope, relaxed atomic add.
    const auto outputLength=arraySize(6),limit=next++;
    emit(body,169,{35,limit,binary(176,53,selectedCapacity,outputLength),selectedCapacity,outputLength});
    const auto overflow=next++,inside=next++;
    const auto outOfSpace=binary(174,53,slot,limit);
    emit(body,247,{inside,0});emit(body,250,{outOfSpace,overflow,inside});label(overflow);
    const auto unused=next++;emit(body,237,{35,unused,counter,one,36,limit});emit(body,253,{});label(inside);
    const auto position=next++;emit(body,81,{41,position,record,10});
    const auto shifted=binary(129,41,position,selectedDelta),rebased=next++;
    emit(body,82,{21,rebased,shifted,record,10});
    storeRecord(slot,rebased);emit(body,249,{finished});label(finished);emit(body,253,{});emit(body,56,{});
    w.resize(550);w.insert(w.end(),declarations.begin(),declarations.end());w.insert(w.end(),body.begin(),body.end());w[3]=next;
    // Extended mode writes only the destination counter. The native descriptor
    // remains VkStorageBuffer; integration must declare this graph access writable.
    std::vector<uint32_t> result(w.begin(),w.begin()+5);unsigned removed=0;
    for(size_t at=5;at<w.size();){const auto count=w[at]>>16;
        if(count==4&&(w[at]&65535)==72&&w[at+1]==25&&w[at+2]==0&&w[at+3]==24)++removed;
        else result.insert(result.end(),w.begin()+at,w.begin()+at+count);
        at+=count;
    }
    if(removed!=1)throw std::runtime_error("VFX counter access decoration changed");
    return result;
}
}
