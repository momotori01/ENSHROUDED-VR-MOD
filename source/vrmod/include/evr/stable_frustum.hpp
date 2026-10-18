#pragma once
#include <evr/game_stereo.hpp>
#include <limits>

namespace evr::game {
using FrustumPlanes=std::array<std::array<float,4>,6>;

// Build outward planes without inverting a float32 world*projection matrix.
// Matrix43 is row-vector affine: ignore its padding column. This routine accepts
// only the validated, unrotated tag-3 descriptor; it never changes camera poses.
inline FrustumPlanes stableFrustum(const MatrixRows& matrix,const ProjectionDescriptor& descriptor){
    uint32_t tag=0,screenRotation=0;
    std::memcpy(&tag,descriptor.bytes.data(),4);
    std::memcpy(&screenRotation,descriptor.bytes.data()+0x44,4);
    std::array<float,6> bounds{};
    std::memcpy(bounds.data(),descriptor.bytes.data()+4,24);
    for(float value:bounds)if(!std::isfinite(value))throw std::invalid_argument("nonfinite frustum bounds");
    const double l=bounds[0],r=bounds[1],b=bounds[2],t=bounds[3],n=bounds[4],f=bounds[5];
    if(tag!=3||screenRotation||descriptor.bytes[0x48]!=1||l>=r||b>=t||n<=0||f<=n)
        throw std::invalid_argument("unsupported stable frustum descriptor");
    const auto& m=matrix.values;
    for(unsigned row=0;row<4;row++)for(unsigned col=0;col<3;col++)
        if(!std::isfinite(m[row*4+col]))throw std::invalid_argument("nonfinite affine view");
    // Invert only the well-conditioned 3x3 camera basis in double precision.
    double a[3][6]{};
    for(unsigned row=0;row<3;row++)for(unsigned col=0;col<3;col++){
        a[row][col]=m[row*4+col];a[row][col+3]=row==col?1:0;
        double dot=0;for(unsigned k=0;k<3;k++)dot+=double(m[row*4+k])*m[col*4+k];
        if(std::abs(dot-(row==col?1:0))>1e-4)throw std::invalid_argument("view basis is not rigid");
    }
    for(unsigned col=0;col<3;col++){
        unsigned pivot=col;
        for(unsigned row=col+1;row<3;row++)if(std::abs(a[row][col])>std::abs(a[pivot][col]))pivot=row;
        if(std::abs(a[pivot][col])<1e-8)throw std::invalid_argument("singular view basis");
        for(unsigned k=0;k<6;k++)std::swap(a[col][k],a[pivot][k]);
        const double divisor=a[col][col];for(double& value:a[col])value/=divisor;
        for(unsigned row=0;row<3;row++)if(row!=col){
            const double factor=a[row][col];for(unsigned k=0;k<6;k++)a[row][k]-=factor*a[col][k];
        }
    }
    std::array<std::array<double,3>,8> corners{};unsigned next=0;
    for(double depth:{n,f})for(double x:{l/n,r/n})for(double y:{b/n,t/n}){
        const double v[]{x*depth-m[12],y*depth-m[13],depth-m[14]};
        for(unsigned col=0;col<3;col++)for(unsigned k=0;k<3;k++)corners[next][col]+=v[k]*a[k][col+3];
        ++next;
    }
    const double local[6][4]{{-1,0,l/n,0},{1,0,-r/n,0},{0,-1,b/n,0},
                             {0,1,-t/n,0},{0,0,-1,n},{0,0,1,-f}};
    FrustumPlanes result{};
    // Seven rounded float operations bound a non-FMA dot4. Include conversion
    // of world corners to float; fused operations have no larger error bound.
    constexpr double unit=std::numeric_limits<float>::epsilon()/2;
    constexpr double gamma=7*unit/(1-7*unit);
    for(unsigned plane=0;plane<6;plane++){
        double p[4]{};
        for(unsigned row=0;row<4;row++)for(unsigned col=0;col<3;col++)p[row]+=m[row*4+col]*local[plane][col];
        p[3]+=local[plane][3];
        const double norm=std::sqrt(p[0]*p[0]+p[1]*p[1]+p[2]*p[2]);
        for(unsigned k=0;k<4;k++)p[k]/=norm;
        for(unsigned k=0;k<3;k++)result[plane][k]=float(p[k]);
        // Keep all eight descriptor corners inside after normal quantization,
        // world-position conversion, and GPU dot rounding. No fixed metre or
        // angular padding; the allowance follows the actual coordinates.
        double offset=p[3];
        for(const auto& corner:corners){
            double dot=0,magnitude=std::abs(p[3]);
            for(unsigned k=0;k<3;k++){
                dot+=double(result[plane][k])*corner[k];
                magnitude+=std::abs(double(result[plane][k])*corner[k]);
            }
            offset=std::min(offset,-dot-(gamma+unit)*magnitude);
        }
        result[plane][3]=std::nextafter(float(offset),-std::numeric_limits<float>::infinity());
        for(float value:result[plane])if(!std::isfinite(value))throw std::invalid_argument("stable frustum overflow");
    }
    return result;
}
}
