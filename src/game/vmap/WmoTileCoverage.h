#ifndef VMAP_WMO_TILE_COVERAGE_H
#define VMAP_WMO_TILE_COVERAGE_H

#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>

namespace VMAP
{
// VMAP placement bounds use internal map coordinates. Tile filenames reverse
// the horizontal axes: filename X is internal Y, filename Y is internal X.
inline std::vector<std::pair<unsigned, unsigned>> WmoTileCoverage(
    double lowX, double lowY, double highX, double highY)
{
    constexpr double tileSize = 533.33333333;
    constexpr double mapSize = 64 * tileSize;
    std::vector<std::pair<unsigned, unsigned>> result;
    if (!std::isfinite(lowX) || !std::isfinite(lowY) ||
        !std::isfinite(highX) || !std::isfinite(highY) ||
        lowX > highX || lowY > highY || highX < 0 || highY < 0 ||
        lowX >= mapSize || lowY >= mapSize)
        return result;

    auto index = [tileSize](double value) {
        return static_cast<unsigned>(std::min(63.0, std::max(0.0, std::floor(value / tileSize))));
    };
    for (unsigned x = index(lowY); x <= index(highY); ++x)
        for (unsigned y = index(lowX); y <= index(highX); ++y)
            result.emplace_back(x, y);
    return result;
}
}
#endif
