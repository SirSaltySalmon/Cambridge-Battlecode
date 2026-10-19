// Draft picks and the pocket route written to draft memory.
#pragma once
#include "constants.h"
#include <cstddef>
#include <optional>
#include <vector>

namespace draft {

// Group value, in gems. Early farming rates gold above gems; the aggressive route keeps the lower weight.
inline constexpr double FARM_GOLD_WEIGHT = 1.5;
inline constexpr double GOLD_WEIGHT = 0.4;

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

void play();

} // namespace draft
