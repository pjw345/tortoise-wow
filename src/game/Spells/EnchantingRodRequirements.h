#pragma once

#include <array>
#include <cstdint>

namespace Enchanting
{
// Vanilla/Turtle runed rods, verified against the world item definitions.
// Only tool-presence checks use this hierarchy; consumed reagents never do.
constexpr std::array<std::uint32_t, 5> RunedRods = {{6218, 6339, 11130, 11145, 16207}};

template<class HasCarriedItem>
bool HasRequiredTool(std::uint32_t required, HasCarriedItem const& hasCarriedItem)
{
    if (hasCarriedItem(required))
        return true;

    bool higherTier = false;
    for (auto rod : RunedRods)
    {
        if (higherTier && hasCarriedItem(rod))
            return true;
        if (rod == required)
            higherTier = true;
    }
    return false;
}
}
