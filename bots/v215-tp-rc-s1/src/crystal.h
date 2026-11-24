// v215 (TP): teleport crystal planning (draft) and use (travel moves). See TP in constants.h.
#pragma once
#include "constants.h"
#include <cstdint>
#include <vector>

namespace crystal {

inline constexpr int REACH = 3;    // landing within this Chebyshev distance of the launch tile
inline constexpr int CHANNEL = 3;  // channel turn ends: the trip costs the turns of three walking steps
inline constexpr std::uint8_t MAGIC = 0xB5;

// A stored crystal jump; usable both ways (both ends are standable, landing rules are the same).
struct Jump {
    vec2 a, b;
};

// Draft: the tail block of draft memory (3 bytes a jump, then count, value, MAGIC), at most `room` bytes. Empty when
// TP is off, nothing pays or the first draft turn's fuel runs low.
std::vector<std::uint8_t> encode(int room);
// Setup: read the tail block.
void decode(const std::vector<std::uint8_t>& memory);
const std::vector<Jump>& jumps();
// Gem-weighted turns a crystal saves per trip (group to altar plus spawn to group), from the draft.
int value();
// Crystals are bought on this map.
bool map_on();

// We hold a crystal (kept by the shop and by launches; the inventory is read on shop visits).
extern bool carrying;
// This turn's travel moves may launch (set each turn by Bot::play).
extern bool gate;
inline bool armed() { return gate && carrying; }

// go(), once the walking field to the goals is built: 0 nothing; 1 the channel started (the move is done); 2 walk to
// `pad` (a stored launch tile) instead, and launch from there.
int travel(const bc::champion& me, vec2 origin, const bc::distance_field& field, const bc::bitboard& reached,
           const bc::bitboard& blocked, const bc::bitboard& goals, bool owns_dash, vec2& pad);

// TP_RECALL: steps from origin to `target` on foot exceed a recall (recall_time) plus our spawn to `target` with a
// crystal by `margin`.
bool recall_pays(vec2 origin, const bc::bitboard& target, int recall_time, int margin);

}  // namespace crystal
