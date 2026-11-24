// Ward probe: log-only measurements (inside LOG, so free in debug builds and absent in release).
#pragma once
#include "board.h"
#include "seen.h"
#include "constants.h"
#include <cambc.h>
namespace probe {
inline bool ward_only(vec2 tile) {
    bc::bitboard own{bc::uninitialized};
    bc::self().visible_tiles_from_into(bc::self().position(), own);
    return !own.is_set(tile);
}
inline bool is_enemy_tile(vec2 tile) {
    for (const auto p : seen::positions(Team::enemy))
        if (p == tile) return true;
    return false;
}
// v214-ward-hold: last round an enemy champion was in sight within WARD_HOLD_RANGE (set at turn start in turn.cpp).
inline int last_near = -1000;
inline int near_now() {
    const auto here = bc::self().position();
    for (const auto p : seen::positions(Team::enemy))
        if (distance(p, here) <= WARD_HOLD_RANGE) return 1;
    return 0;
}
inline int enemies_in_sight() { return static_cast<int>(seen::champions(Team::enemy).size()); }
inline int gain(vec2 ward) {
    bc::bitboard own{bc::uninitialized};
    bc::self().visible_tiles_from_into(bc::self().position(), own);
    auto w = bc::ward::visible_tiles_from(ward);
    w &= ~own;
    return static_cast<int>(w.count());
}
inline void ev(const std::string& text) {
    say(format("WP r%u c%d ", bc::round_number(), static_cast<int>(bc::self().champion_name())) + text);
}
}  // namespace probe
