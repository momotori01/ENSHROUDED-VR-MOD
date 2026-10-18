// Used only when native absolute pruning discards EVERY otherwise valid probe.
// Preserve native invalid-probe and occlusion tests. Relative weights remain
// proportional to distance visibility, trilinear location and surface direction.
// Rescale only this empty set before half packing to avoid weight underflow.
        if (_727 == 0u && evrRejectedCount > 0u && evrRejectedMax > 0.0) {
            uint evrKept=0u;
            float evrSum=0.0;
            for (uint evrI=0u; evrI<evrRejectedCount; ++evrI) {
                uint evrPacked=packHalf2x16(vec2(evrRejectedWeights[evrI]/evrRejectedMax,0.0));
                float evrWeight=unpackHalf2x16(evrPacked).x;
                if (evrWeight > 0.0) {
                    _26[gl_LocalInvocationIndex]._m0[evrKept++]=evrRejectedIds[evrI] | (evrPacked << 16u);
                    evrSum+=evrWeight;
                }
            }
            _26[gl_LocalInvocationIndex]._m1=evrKept | (packHalf2x16(vec2(evrSum,0.0)) << 16u);
        }
