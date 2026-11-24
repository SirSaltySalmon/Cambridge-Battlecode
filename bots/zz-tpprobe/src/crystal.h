// Teleport crystal planning: walking steps with one crystal, on row-major arrays read in one engine call each.
#pragma once
#include "constants.h"
#include <cstdint>
#include <vector>

namespace crystal {

inline constexpr int REACH = 3;    // landing within this Chebyshev distance of the launch tile
inline constexpr int CHANNEL = 3;  // turn ends of channel: the trip costs as many turns as three walking steps
inline constexpr std::uint16_t INF = 0xFFFF;

// The static map, row-major: 1 where nothing may stand or land (walls, holes, altars, the enemy spawn).
struct Grid {
    int w = 0, h = 0;
    std::vector<std::uint8_t> block;
};
const Grid& grid();

// Walking steps to `goals` (f), the best walking steps left after a crystal from each tile (land: min f over the legal
// landings within REACH, INF if none), and the steps with one crystal anywhere on the way (one = min(f, multi-source
// BFS seeded with land + CHANNEL)). All row-major, INF unreachable.
struct Fields {
    std::vector<std::uint16_t> f, land, one;
};
void compute(const bc::bitboard& goals, Fields& out);

}  // namespace crystal
