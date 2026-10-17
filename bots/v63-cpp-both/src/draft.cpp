#include "draft.h"
#include "board.h"
#include <algorithm>
#include <map>
#include <tuple>

namespace draft {
namespace {

using Draft = bc::champion_draft;
using Camp = bc::camp_type;

// Picks in order, by the team size the map asks for (3 to 5).
constexpr Draft ROSTER_5[] = {Draft::cassi_and_issac, Draft::varg, Draft::ryth, Draft::willow, Draft::vesper_vale};
constexpr Draft ROSTER_4[] = {Draft::cassi_and_issac, Draft::varg, Draft::vesper_vale, Draft::willow};
constexpr Draft ROSTER_3[] = {Draft::cassi_and_issac, Draft::varg, Draft::vesper_vale};

std::span<const Draft> roster(std::uint32_t size) {
    if (size <= 3) return ROSTER_3;
    if (size == 4) return ROSTER_4;
    return ROSTER_5;
}

// Every camp kind can join the pocket (wolf, gem cluster, gold cluster, elk, monolith).
constexpr int DENSITY_RADIUS = 8;
// Groups this close to the pocket anchor are farmed while the pocket respawns.
constexpr int ROAM_RADIUS = 60;
// Gems lost per point of health the team takes while clearing.
constexpr double DAMAGE_COST = 0.01;
// Rough team basic-attack damage per round, used to estimate clear time.
constexpr int TEAM_DAMAGE = 200;
// Walking distance to the nearest altar at which a pocket is worth half.
constexpr int ALTAR_FALLOFF = 20;
constexpr int UNREACHABLE_GAP = 255;
// Aggressive route: distance to the map centre at which a pocket is worth half.
constexpr int CENTER_FALLOFF = 5;
// The aggressive route stays on the centre pocket and its neighbours.
constexpr int AGGRESSIVE_RADIUS = 12;

// Health and strength per kind, from the rules; camps outside vision hide them.
std::pair<int, int> camp_stats(Camp kind) {
    switch (kind) {
    case Camp::wolf: return {1350, 20};
    case Camp::elk: return {2400, 60};
    case Camp::gem_cluster: return {500, 0};
    case Camp::gold_cluster: return {500, 0};
    case Camp::monolith: return {20000, 200};
    }
    return {0, 0};
}

double group_income(const bc::map::camp_group& group, double gold_weight) {
    // Value per round when the group is farmed every time it returns.
    const double cycle = group.max_respawn_timer() + clear_rounds(group) + bc::constants::capture_time;
    return group_value(group, gold_weight) / std::max(1.0, cycle);
}

// Walking distance from each listed group to the nearest root tile, indexed by group.
std::vector<int> walking_gaps(const GroupData& data, const std::vector<int>& indexes, const bc::bitboard& roots) {
    bc::distance_field field;
    bc::bitboard reached;
    field.compute(bc::map::walls() | bc::map::holes(), roots, reached);
    std::vector<int> gaps(data.groups.size(), UNREACHABLE_GAP);
    for (const int index : indexes) {
        std::optional<int> best;
        for (const auto tile : data.spawn_lists[index])
            if (reached.is_set(tile)) {
                const int steps = field.get(tile);
                if (!best || steps < *best) best = steps;
            }
        gaps[index] = best ? *best : UNREACHABLE_GAP;
    }
    return gaps;
}

void encode_pocket(std::vector<std::uint8_t>& out, vec2 anchor, const std::vector<int>& members) {
    // Layout: anchor x, anchor y, count, then one little-endian u16 group index per member.
    out.push_back(static_cast<std::uint8_t>(anchor.x));
    out.push_back(static_cast<std::uint8_t>(anchor.y));
    out.push_back(static_cast<std::uint8_t>(members.size()));
    for (const int member : members) {
        out.push_back(static_cast<std::uint8_t>(member & 0xFF));
        out.push_back(static_cast<std::uint8_t>(member >> 8 & 0xFF));
    }
}

void encode_theirs(std::vector<std::uint8_t>& out, const std::vector<bool>& theirs) {
    // Layout: group count as a little-endian u16, then one bit per group, least significant first.
    out.push_back(static_cast<std::uint8_t>(theirs.size() & 0xFF));
    out.push_back(static_cast<std::uint8_t>(theirs.size() >> 8 & 0xFF));
    for (std::size_t i = 0; i < theirs.size(); i += 8) {
        std::uint8_t bits = 0;
        for (std::size_t b = 0; b < 8 && i + b < theirs.size(); ++b)
            if (theirs[i + b]) bits |= static_cast<std::uint8_t>(1u << b);
        out.push_back(bits);
    }
}

std::vector<std::uint8_t> encode_routes() {
    // The theirs mask, the normal route block, then the aggressive route block trimmed to fit draft memory.
    std::vector<std::uint8_t> memory;
    auto normal = compute_pocket();
    if (!normal.anchor) return memory;
    encode_theirs(memory, theirs_groups());
    encode_pocket(memory, *normal.anchor, normal.members);
    auto aggressive = compute_pocket(true);
    const int room = (static_cast<int>(bc::constants::draft_memory_bytes) - static_cast<int>(memory.size()) - 3) / 2;
    if (aggressive.anchor && room > 0) {
        if (static_cast<int>(aggressive.members.size()) > room) aggressive.members.resize(room);
        encode_pocket(memory, *aggressive.anchor, aggressive.members);
    }
    return memory;
}

} // namespace

const GroupData& group_data() {
    static const GroupData data = [] {
        GroupData d;
        d.groups = bc::map::camp_groups();
        d.has_camps.reserve(d.groups.size());
        d.centers.reserve(d.groups.size());
        d.spawn_lists.reserve(d.groups.size());
        for (const auto& group : d.groups) {
            d.has_camps.push_back(!group.camps().empty());
            // Every camp has its own spawn tile, so the list comes straight from the camps rather than
            // through a spawn_tiles() board. Its order differs from the board's tile order, but every
            // use (nearest tile, centre sums, drawing rings, the slot grid) ignores order.
            std::vector<vec2> tiles;
            tiles.reserve(group.camps().size());
            for (const auto& camp : group.camps()) tiles.push_back(camp.spawn_position());
            int sx = 0, sy = 0;
            for (const auto tile : tiles) {
                sx += tile.x;
                sy += tile.y;
            }
            const int n = std::max<int>(1, static_cast<int>(tiles.size()));
            d.centers.push_back({sx / n, sy / n});
            d.spawn_lists.push_back(std::move(tiles));
        }
        return d;
    }();
    return data;
}

vec2 group_center(const bc::map::camp_group& group) {
    const auto tiles = group.spawn_tiles().positions();
    int sx = 0, sy = 0;
    for (const auto tile : tiles) {
        sx += tile.x;
        sy += tile.y;
    }
    const int n = static_cast<int>(tiles.size());
    return {sx / n, sy / n};
}

double clear_rounds(const bc::map::camp_group& group) {
    int health = 0;
    for (const auto& camp : group.camps()) health += camp_stats(camp.kind()).first;
    return static_cast<double>(health) / TEAM_DAMAGE;
}

double group_value(const bc::map::camp_group& group, double gold_weight) {
    int strength = 0;
    for (const auto& camp : group.camps()) strength += camp_stats(camp.kind()).second;
    const double damage_taken = strength * clear_rounds(group);
    const double reward = group.gem_value() + gold_weight * group.gold_value();
    return std::max(0.0, reward - DAMAGE_COST * damage_taken);
}

Pocket compute_pocket(bool aggressive) {
    // The normal route favours pockets near an altar; the aggressive one favours the map centre.
    // Groups on their side count for less in both.
    const auto& data = group_data();
    const int count = static_cast<int>(data.groups.size());
    std::vector<int> ids;  // groups with camps, ascending
    for (int index = 0; index < count; ++index)
        if (data.has_camps[index]) ids.push_back(index);
    if (ids.empty()) return {};

    const double weight = aggressive ? GOLD_WEIGHT : FARM_GOLD_WEIGHT;
    const auto& theirs = theirs_groups();
    std::vector<double> incomes(count, 0.0);
    for (const int index : ids)
        incomes[index] = group_income(data.groups[index], weight) * (theirs[index] ? THEIRS_VALUE : 1.0);
    std::vector<int> gaps(count, 0);
    int falloff;
    if (aggressive) {
        const vec2 middle{bc::map::width() / 2, bc::map::height() / 2};
        for (const int index : ids) gaps[index] = distance(data.centers[index], middle);
        falloff = CENTER_FALLOFF;
    } else {
        gaps = walking_gaps(data, ids, bc::map::altars());
        falloff = ALTAR_FALLOFF;
    }

    // Find neighbours with native bitboard ops. Groups sharing a centre tile are chained in index
    // order on a flat per-tile grid, matching the per-tile lists the Python bot keeps.
    const int width = bc::map::width();
    bc::bitboard board;
    std::vector<int> head(static_cast<std::size_t>(width) * bc::map::height(), -1), tail(head.size(), -1), next(count, -1);
    for (const int index : ids) {
        const auto center = data.centers[index];
        const auto cell = static_cast<std::size_t>(center.y) * width + center.x;
        board.set(center);
        if (head[cell] < 0) head[cell] = index;
        else next[tail[cell]] = index;
        tail[cell] = index;
    }
    std::vector<int> members;
    bc::bitboard area;
    auto members_near = [&](vec2 center, int radius) {
        members.clear();
        area.clear();
        area.draw_chebyshev(center, static_cast<float>(radius));
        area &= board;
        for (const auto tile : area.positions())
            for (int m = head[static_cast<std::size_t>(tile.y) * width + tile.x]; m >= 0; m = next[m]) members.push_back(m);
        return members;
    };

    std::optional<std::pair<double, int>> best;
    for (const int index : ids) {
        members_near(data.centers[index], DENSITY_RADIUS);
        int gap_sum = 0;
        PySum income;
        for (const int m : members) {
            gap_sum += gaps[m];
            income.add(incomes[m]);
        }
        const double gap = static_cast<double>(gap_sum) / members.size();
        const double score = income.value() * falloff / (falloff + gap);
        if (!best || score > best->first) best = {score, index};
    }

    const auto anchor = data.centers[best->second];
    auto route = members_near(anchor, aggressive ? AGGRESSIVE_RADIUS : ROAM_RADIUS);
    std::stable_sort(route.begin(), route.end(), [&](int a, int b) {
        return std::make_pair(distance(anchor, data.centers[a]), a) < std::make_pair(distance(anchor, data.centers[b]), b);
    });
    if (route.size() > 255) route.resize(255);
    return {anchor, route};
}

const std::vector<bool>& theirs_groups() {
    // One walking field from each spawn; built once, in the draft where fuel is plentiful (setup only
    // needs it if draft memory is missing).
    static const std::vector<bool> theirs = [] {
        const auto& data = group_data();
        std::vector<int> ids;
        for (int index = 0; index < static_cast<int>(data.groups.size()); ++index)
            if (data.has_camps[index]) ids.push_back(index);
        const auto ours = walking_gaps(data, ids, bc::map::spawn_region(Team::ally));
        const auto enemy = walking_gaps(data, ids, bc::map::spawn_region(Team::enemy));
        std::vector<bool> result(data.groups.size(), false);
        for (const int index : ids) result[index] = HOME_RATIO * enemy[index] <= ours[index];
        return result;
    }();
    return theirs;
}

std::vector<bool> decode_theirs(const std::vector<std::uint8_t>& data, std::size_t& offset, std::size_t groups) {
    if (data.size() < offset + 2) return {};
    const std::size_t count = data[offset] | data[offset + 1] << 8;
    const std::size_t bytes = (count + 7) / 8;
    if (count != groups || data.size() < offset + 2 + bytes) return {};
    std::vector<bool> theirs(count);
    for (std::size_t i = 0; i < count; ++i) theirs[i] = data[offset + 2 + i / 8] >> (i % 8) & 1;
    offset += 2 + bytes;
    return theirs;
}

Pocket decode_pocket(const std::vector<std::uint8_t>& data, std::size_t& offset) {
    if (data.size() < offset + 3 || data.size() < offset + 3 + 2 * static_cast<std::size_t>(data[offset + 2])) return {};
    const vec2 anchor{data[offset], data[offset + 1]};
    const int count = data[offset + 2];
    std::vector<int> members;
    members.reserve(count);
    for (int i = 0; i < count; ++i)
        members.push_back(data[offset + 3 + 2 * i] | data[offset + 4 + 2 * i] << 8);
    offset += 3 + 2 * count;
    return {anchor, members};
}

void play() {
    // Submit before picking: the pick ends our input for the turn and later memory access is rejected.
    static const auto memory = encode_routes();
    std::vector<std::byte> bytes(memory.size());
    std::transform(memory.begin(), memory.end(), bytes.begin(), [](std::uint8_t b) { return std::byte(b); });
    bc::draft_memory = std::span<const std::byte>{bytes};

    const auto chosen = bc::draft::current_draft(Team::ally);
    const auto size = std::min<std::uint32_t>(std::max<std::uint32_t>(bc::draft::team_size(), 3), 5);
    for (const auto pick : roster(size))
        if (std::find(chosen.begin(), chosen.end(), pick) == chosen.end()) {
            bc::draft::pick(pick);
            break;
        }
}

} // namespace draft
