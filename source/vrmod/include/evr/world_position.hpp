#pragma once
#include <cstdint>
#include <evr/math.hpp>
#include <limits>

namespace evr
{
// Observed WorldPosition encoding for the documented Enshrouded executable.
// This value type owns its data. It never exposes writable game/player memory.
struct WorldPositionQ32
{
    int64_t x = 0, y = 0, z = 0;
};
static_assert(sizeof(WorldPositionQ32) == 24, "WorldPosition observed layout");
inline int64_t addQ32(int64_t anchor, double gameUnits)
{
    constexpr double scale = 4294967296.0;
    double raw = std::round(gameUnits * scale);
    // Use an exact power-of-two upper boundary: double(INT64_MAX) rounds to 2^63.
    if (!std::isfinite(raw) || raw >= 9223372036854775808.0 || raw < -9223372036854775808.0)
        throw std::invalid_argument("Q32 offset outside representable range");
    int64_t delta = static_cast<int64_t>(raw);
    if ((delta > 0 && anchor > std::numeric_limits<int64_t>::max() - delta) ||
        (delta < 0 && anchor < std::numeric_limits<int64_t>::min() - delta))
        throw std::invalid_argument("Q32 world coordinate overflow");
    return anchor + delta;
}
inline double relativeQ32(int64_t value, int64_t origin)
{
    // Unsigned subtraction preserves tiny deltas at large absolute coordinates.
    if (value >= origin)
        return double(uint64_t(value) - uint64_t(origin)) / 4294967296.0;
    return -double(uint64_t(origin) - uint64_t(value)) / 4294967296.0;
}
inline WorldPositionQ32 cameraPosition(WorldPositionQ32 root, Vec3 offsetMeters, double gameUnitsPerMeter)
{
    if (!finite(offsetMeters) || !std::isfinite(gameUnitsPerMeter) || gameUnitsPerMeter <= 0)
        throw std::invalid_argument("invalid camera world scale/offset");
    return {addQ32(root.x, offsetMeters.x * gameUnitsPerMeter),
            addQ32(root.y, offsetMeters.y * gameUnitsPerMeter),
            addQ32(root.z, offsetMeters.z * gameUnitsPerMeter)};
}
} // namespace evr
