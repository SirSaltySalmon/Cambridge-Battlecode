// Draft picks and the pocket route written to draft memory.
#pragma once
#include "constants.h"
#include <cstddef>
#include <optional>
#include <vector>

namespace draft {

// Group value, in gems. v96: gems win, so gold weighs less (v95: 1.5 early, 0.4 aggressive; bench-v96 sweep).
inline constexpr double FARM_GOLD_WEIGHT = 0.6;
inline constexpr double GOLD_WEIGHT = 0.1;

// What the pocket search and setup read about each camp group, gathered once per bot instance:
// the engine call and the spawn tile lists are the expensive part.
struct GroupData {
    std::vector<bc::map::camp_group> groups;
    std::vector<bool> has_camps;
    std::vector<vec2> centers;  // same as group_center()
    std::vector<std::vector<vec2>> spawn_lists;
};
const GroupData& group_data();

vec2 group_center(const bc::map::camp_group& group);
double clear_rounds(const bc::map::camp_group& group);
// v99: a group's summed camp health and strength (and whether it holds a monolith), read once so setup's per-group
// numbers below need one pass over its camps instead of five.
struct GroupStats {
    int strength = 0, health = 0;
    bool monolith = false;
    // v105 (STRONG_EARLY): summed strength of the camps hitting STRONG_CAMP_STRENGTH or more.
    int strong = 0;
};
GroupStats group_stats(const bc::map::camp_group& group);
double clear_rounds(const GroupStats& stats);
double group_load(const GroupStats& stats);
double group_value(const bc::map::camp_group& group, double gold_weight, const GroupStats& stats);
// v105 (STRONG_EARLY): a group's value in parts (reward, the health it costs, the strong camps' share of that, the
// gemless scale), so the strong camps' share can be weighted during the match: value_of(parts, 0) is group_value.
struct ValueParts {
    double reward = 0, cost = 0, strong_cost = 0, scale = 1;
};
ValueParts value_parts(const bc::map::camp_group& group, double gold_weight, const GroupStats& stats);
inline double value_of(const ValueParts& parts, double strong_extra) {
    const double value = parts.reward - parts.cost - parts.strong_cost * strong_extra;
    return (value > 0 ? value : 0.0) * parts.scale;
}
// v97: summed strength x summed health, what a few champions trade to clear the group (FIX_WALK_TARGET).
double group_load(const bc::map::camp_group& group);
// Reward minus the health the team trades for it; never negative.
double group_value(const bc::map::camp_group& group, double gold_weight = GOLD_WEIGHT);

struct Pocket {
    std::optional<vec2> anchor;
    std::vector<int> members;
};
// Returns the anchor tile and route group indexes, nearest to the anchor first.
Pocket compute_pocket(bool aggressive = false);
// Returns the anchor, members and the offset just past this block.
Pocket decode_pocket(const std::vector<std::uint8_t>& data, std::size_t& offset);

// Map facts every champion needs at setup. The draft has fuel to spare (10x on its first turn), so it works
// them out once and hands them over in draft memory after the two route blocks; each champion's first
// turn then only decodes them.
struct SetupFacts {
    std::vector<vec2> ward_spots;
    std::vector<bool> solo;  // per altar, in bc::map::altars() tile order
};
// Solo altars sit away from the centre and no closer to the enemy spawn than to ours.
bool solo_altar(vec2 altar);
std::optional<SetupFacts> decode_setup(const std::vector<std::uint8_t>& data, std::size_t offset);
// Offset just past the setup block (offset itself when there is none).
std::size_t setup_end(const std::vector<std::uint8_t>& data, std::size_t offset);

// v86 (V86_ZONES): per camp group, in bc::map::camp_groups() order: zone (ZONE_SAFE...), whether a pair can clear it
// alone, and walking steps from our spawn and from theirs (255 for unreachable).
struct ZoneFacts {
    std::vector<int> zone;
    std::vector<bool> pair_ok;
    std::vector<int> ours, theirs;
};
std::optional<ZoneFacts> decode_zones(const std::vector<std::uint8_t>& data, std::size_t offset);
// v99: where the zone block's per-group entries start (3 bytes each, as decode_zones reads them), if it is valid.
std::optional<std::size_t> zones_block(const std::vector<std::uint8_t>& data, std::size_t offset);

// v148 (VARG_PLAN): a group's fighters (clusters left out) in Varg's order: VARG_GOLEMS double golem, 4 double elk,
// 3 elk and wolf, 2 double wolf, 1 single elk, 0 clusters only, -1 anything else.
inline constexpr int VARG_GOLEMS = 5;
int varg_class(const bc::map::camp_group& group);
// Our half of the mirror split between the spawns: a spawn tile no further (squared, from the spawn regions' centres)
// from ours than from theirs; a group on the line counts as ours.
bool safe_half(const bc::map::camp_group& group);
// The plan's draft rule: VARG_DOUBLE_ELK_MIN double-elk groups on our half (team size 3 needs VARG_PLAN_3).
bool varg_plan_map(std::uint32_t size);

void play();

} // namespace draft
