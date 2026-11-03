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
// v158: over PRISM_TEAM_BAR of the map's camps are gem clusters: the Prism team's map.
bool prism_map();

vec2 group_center(const bc::map::camp_group& group);
double clear_rounds(const bc::map::camp_group& group);
// v99: a group's summed camp health and strength (and whether it holds a monolith), read once so setup's per-group
// numbers below need one pass over its camps instead of five.
struct GroupStats {
    int strength = 0, health = 0;
    bool monolith = false;
    // v105 (STRONG_EARLY): summed strength of the camps hitting STRONG_CAMP_STRENGTH or more.
    int strong = 0;
    // v158: gem cluster camps in the group.
    int gem_clusters = 0;
    // v177 (ECON_PHASES): elk camps in the group.
    int elks = 0;
};
GroupStats group_stats(const bc::map::camp_group& group);
double clear_rounds(const GroupStats& stats);
double group_load(const GroupStats& stats);
double group_value(const bc::map::camp_group& group, double gold_weight, const GroupStats& stats);
// v105 (STRONG_EARLY): a group's value in parts (reward, the health it costs, the strong camps' share of that, the
// gemless scale), so the strong camps' share can be weighted during the match: value_of(parts, 0) is group_value.
// v177 (ECON_PHASES): the gems of its elks and gem clusters, and whether it is gold-only (scale is GEMLESS_VALUE then).
struct ValueParts {
    double reward = 0, cost = 0, strong_cost = 0, scale = 1;
    double elk_gems = 0, cluster_gems = 0;
    bool gemless = false;
};
ValueParts value_parts(const bc::map::camp_group& group, double gold_weight, const GroupStats& stats);
inline double value_of(const ValueParts& parts, double strong_extra) {
    const double value = parts.reward - parts.cost - parts.strong_cost * strong_extra;
    return (value > 0 ? value : 0.0) * parts.scale;
}
// v177 (ECON_PHASES): before the team is built gold-only groups count EARLY_GOLD_SCALE; after it, elk and gem-cluster
// gems count ELK_GEM_MULT and CLUSTER_GEM_MULT.
inline double value_phased(const ValueParts& parts, double strong_extra, bool built, double elk_mult = ELK_GEM_MULT) {
    if (!built) {
        const double value = parts.reward + (ELK_STEPS ? (elk_mult - 1) * parts.elk_gems : 0.0) - parts.cost - parts.strong_cost * strong_extra;
        return (value > 0 ? value : 0.0) * (parts.gemless ? EARLY_GOLD_SCALE : parts.scale);
    }
    const double value = parts.reward + (elk_mult - 1) * parts.elk_gems + (CLUSTER_GEM_MULT - 1) * parts.cluster_gems
                         - parts.cost - parts.strong_cost * strong_extra;
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

void play();

} // namespace draft
