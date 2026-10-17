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
// A group on their side (HOME_RATIO times as far from our spawn as from theirs) is worth this share of
// its value in every route rating: the enemy reaches it first and gets back to heal sooner.
inline constexpr double THEIRS_VALUE = 0.5;

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

// By group index: whether the group is on their side. Each camp is labelled ours (twice as close to
// our spawn), theirs (twice as close to theirs) or contested; only "theirs" changes a rating.
const std::vector<bool>& theirs_groups();
// The theirs mask at the start of draft memory; empty when it is missing or for another group count.
std::vector<bool> decode_theirs(const std::vector<std::uint8_t>& data, std::size_t& offset, std::size_t groups);

void play();

} // namespace draft
