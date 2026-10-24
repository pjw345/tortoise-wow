#include "WmoTileCoverage.h"
#include <cassert>
#include <limits>
#include <algorithm>

int main()
{
    using VMAP::WmoTileCoverage;
    auto tiles = WmoTileCoverage(24986.884765625, 15603.357421875, 26474.984375, 17091.708984375);
    assert(tiles.size() == 16);
    assert(tiles.front() == std::make_pair(29u, 46u));
    assert(tiles.back() == std::make_pair(32u, 49u));
    assert(std::find(tiles.begin(), tiles.end(), std::make_pair(30u, 47u)) != tiles.end());
    assert(WmoTileCoverage(600, 100, 700, 200).front() == std::make_pair(0u, 1u));
    assert(WmoTileCoverage(-10, -10, 10, 10).size() == 1);
    assert(WmoTileCoverage(34000, 34000, 35000, 35000).front() == std::make_pair(63u, 63u));
    assert(WmoTileCoverage(-100, -100, -1, -1).empty());
    assert(WmoTileCoverage(35000, 10, 36000, 20).empty());
    assert(WmoTileCoverage(10, 10, 9, 20).empty());
    assert(WmoTileCoverage(0, 0, std::numeric_limits<double>::infinity(), 20).empty());
    assert(WmoTileCoverage(std::numeric_limits<double>::quiet_NaN(), 0, 10, 20).empty());
    // Exact shared edges are included; native input uses float bounds.
    assert(WmoTileCoverage(0, 0, 533.33333333, 533.33333333).size() == 4);
}
