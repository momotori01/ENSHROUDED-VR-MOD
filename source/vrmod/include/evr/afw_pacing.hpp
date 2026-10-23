#pragma once
#include <atomic>
#include <cstdint>
#include <cstdlib>
// The pacing of the alternate eyes (2026-10-23; user after the headset's frames.csv: "the AFW shakes while walking" - at
// 75 Hz with eyes of 2002 x 2148 a frame of one eye took about 19 ms, and the runtime went between showing each frame for
// one refresh and for two, 589 and 777 times in 30 s, so the world moved on in uneven steps; both eyes drawn took about
// 27 ms and were shown for two refreshes every time). The user's three ways, chosen in the settings exe ([Picture]
// AfwPacing, the loader's EVR_AFW_PACING) and switched in the game's VR settings:
//   0  every refresh a frame can make (the runtime shows the last frame again for the misses);
//   1  a steady two refreshes a frame: a frame that would be shown one refresh after the last is begun again for the
//      refresh after (include/evr/openxr_client.hpp beginFrameContent; the runtime drops the frame begun first) - half the
//      headset's rate and no switching, the room left over for a larger picture (AfwRenderScale, AfwDlss);
//   2  the mod's own frame for every refresh the game misses: the last pictures of both eyes warped by their depth to
//      the head's pose of that refresh, the character's movement carried on (layer/native_xr.hpp asyncWarp).
namespace evr::afw {
inline int pacingFromEnvironment(){const char* v=std::getenv("EVR_AFW_PACING");return v&&(v[0]=='1'||v[0]=='2')&&!v[1]?v[0]-'0':0;}
inline std::atomic<int> pacing{pacingFromEnvironment()};
inline std::atomic<uint64_t> steadyBegunAgain{0};   // frames begun again for the next refresh (pacing 1)
} // namespace evr::afw
