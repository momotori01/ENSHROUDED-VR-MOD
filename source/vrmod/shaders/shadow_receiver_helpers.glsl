// Receiver-plane correction for the reviewed sun visibility compute shader.
// Reconstruct the geometric receiver normal from depth, avoiding normal-map noise.
vec3 evrReceiverNormal;
vec3 evrPoint(ivec2 pixel) {
    pixel=clamp(pixel,ivec2(0),ivec2(evrUniform.parameters._m2)-ivec2(1));
    vec4 s=vec4(vec2(pixel)+vec2(0.5),texelFetch(_5,pixel,0).r,1.0);
    vec4 h=vec4(dot(evrUniform.parameters._m0._m0._m0,s),dot(evrUniform.parameters._m0._m0._m1,s),
                dot(evrUniform.parameters._m0._m0._m2,s),dot(evrUniform.parameters._m0._m0._m3,s));
    return h.xyz/h.w;
}
vec3 evrNormal(vec3 center,vec3 fallback) {
    ivec2 p=ivec2(gl_GlobalInvocationID.xy);
    vec3 r=evrPoint(p+ivec2(1,0))-center,l=center-evrPoint(p-ivec2(1,0));
    vec3 d=evrPoint(p+ivec2(0,1))-center,u=center-evrPoint(p-ivec2(0,1));
    vec3 n=cross(dot(r,r)<dot(l,l)?r:l,dot(d,d)<dot(u,u)?d:u);
    return dot(n,n)>1e-18?normalize(n):fallback;
}
vec2 evrSlope(uint cascade) {
#if EVR_PLANE
    vec3 n=vec3(dot(evrUniform.parameters._m1._m1._m0.xyz,evrReceiverNormal),
                dot(evrUniform.parameters._m1._m1._m1.xyz,evrReceiverNormal),
                dot(evrUniform.parameters._m1._m1._m2.xyz,evrReceiverNormal));
    vec3 scale=evrUniform.parameters._m1._m4[cascade]._m0;
    if(abs(n.z)<0.05) return vec2(0.0);
    return clamp(-n.xy/n.z*scale.z/scale.xy,vec2(-0.25),vec2(0.25));
#else
    return vec2(0.0);
#endif
}
vec4 evrGather(vec2 tap,vec3 center,uint cascade) {
    vec2 size=evrUniform.parameters._m1._m2.xy;
    tap=(floor(tap*size-vec2(0.5))+vec2(1.0))/size;
    vec4 depths=textureGatherOffset(sampler2DArray(_13,_15),vec3(tap,float(cascade)),ivec2(0));
    vec2 base=floor(tap*size-vec2(0.5))+vec2(0.5);
    vec4 dx=(vec4(base.x)+vec4(0,1,1,0))/size.x-vec4(center.x);
    vec4 dy=(vec4(base.y)+vec4(1,1,0,0))/size.y-vec4(center.y);
    vec2 slope=evrSlope(cascade);
    return depths-slope.x*dx-slope.y*dy;
}
float evrPcf(vec2 tap,vec3 center,uint cascade) {
    // Snap only the gather footprint to its exact texel midpoint. Interpolation
    // below retains the requested tap, avoiding hardware coordinate rounding
    // crossing a texel boundary while software fractions remain on its other side.
    vec2 size=evrUniform.parameters._m1._m2.xy;
    vec2 gatherTap=(floor(tap*size-vec2(0.5))+vec2(1.0))/size;
    vec4 d=evrGather(gatherTap,center,cascade);
    // Captured Vulkan sampler uses strict reference > depth, linear filtering.
    vec4 lit=vec4(greaterThan(vec4(center.z),d));
    vec2 f=fract(tap*evrUniform.parameters._m1._m2.xy-vec2(0.5));
    return mix(mix(lit.w,lit.z,f.x),mix(lit.x,lit.y,f.x),f.y);
}
