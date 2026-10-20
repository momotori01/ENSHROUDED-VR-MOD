#pragma once
// The graphics card for the recommended graphics (include/evr/recommended_graphics.hpp): DXGI's list of adapters, the one
// with the most dedicated video memory (a laptop's discrete card over its integrated one), Microsoft's software adapter
// left out. Read only; nothing is created on the card. Defined in hooks/gpu_info.cpp, a translation unit of its own:
// dxgi.h brings rpcndr.h, whose macros (small, interface) break names in the larger ones.
#include <evr/recommended_graphics.hpp>
namespace evr::recommend {
Machine detectMachine();
}
