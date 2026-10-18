// Fuel cache for engine reads. Each vision list is read once per turn and read again only after one of
// our own actions (a move, attack, cast or ward changes what we see).
#pragma once
#include "constants.h"
#include <vector>

namespace seen {

// Called after every action: the next read fetches fresh lists.
void reset();
// Goes up whenever the lists are dropped, so a cached answer can tell it is still current.
std::uint64_t generation();
// Called at turn start: also frees the lists retired during the last turn.
void new_turn();
// Every champion of a team, in or out of vision; the team never changes during a match.
const std::vector<bc::champion>& roster(Team team);
// Living visible champions of a team. The reference stays valid until the next turn starts;
// copy it to change it.
const std::vector<bc::champion>& champions(Team team);
// Living visible camps, valid the same way.
const std::vector<bc::camp>& camps();
// Walls and holes.
const bc::bitboard& terrain();
// Everything that blocks a step and never moves: terrain, altars and the enemy spawn.
const bc::bitboard& fixed_blocks();
const bc::bitboard& ally_spawn();
// Altar tiles.
const std::vector<vec2>& altar_list();

} // namespace seen
