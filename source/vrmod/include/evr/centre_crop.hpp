#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
namespace evr {
// The middle of a w x h picture in the shape of a W x H one (x0, y0 inclusive, x1, y1 exclusive): what the desktop
// shows of the right eye's square picture (layer/desktop_mirror.hpp has the same sum for its own blit).
struct CropRect {int32_t x0=0,y0=0,x1=0,y1=0;};
inline CropRect centreCrop(uint32_t w,uint32_t h,uint32_t W,uint32_t H){
    CropRect r{0,0,int32_t(w),int32_t(h)};if(!w||!h||!W||!H)return r;
    if(double(w)/h<double(W)/H){const auto ch=std::min(int32_t(h),std::max(1,int32_t(std::lround(double(w)*H/W))));r.y0=(int32_t(h)-ch)/2;r.y1=r.y0+ch;}
    else{const auto cw=std::min(int32_t(w),std::max(1,int32_t(std::lround(double(h)*W/H))));r.x0=(int32_t(w)-cw)/2;r.x1=r.x0+cw;}
    return r;
}
}
