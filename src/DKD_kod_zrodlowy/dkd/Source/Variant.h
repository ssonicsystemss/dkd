#pragma once
// One code base, three plugins. DKD_VARIANT is set by each CMake project:
//   0 = dkd       (kick + reverse bass, green phosphor screens)
//   1 = dkdalpha  (kick only, blue "gem" LCD)
//   2 = dkdomega  (reverse bass only, red "gem" LCD)
#ifndef DKD_VARIANT
 #define DKD_VARIANT 0
#endif

namespace Variant
{
    constexpr int id = DKD_VARIANT;
    constexpr bool alpha = id == 1, omega = id == 2, gem = id != 0;
    constexpr bool hasKick = id != 2, hasBass = id != 1;
}
