#include "draft.h"
#include "board.h"
#include <algorithm>
#include <map>
#include <tuple>

std::vector<vec2> compute_ward_spots();

namespace draft {
namespace {

using Draft = bc::champion_draft;
using Camp = bc::camp_type;

// Picks in order, by the team size the map asks for (3 to 5).
// v70: Ren takes Varg's place and Stellar takes Vesper Vale's, on every team size.
// v94 (balance patch): Varg takes Cassi & Issac's place (splash on cluster-heavy maps), Vesper Vale takes Stellar's.
// v101: Heart of Ruin takes Stellar's place on five, and Varg's (HOR_SMALL_SWAP 0) or Ren's (1) on three and four.
// v110 (patch: Ren's stun 1.25 -> 0.75 x Focus): Marcella takes Varg's place and Ryth takes Ren's.
// v123 (PRISM_MIN_GEMS_5, PRISM_MIN_GEMS_4): Prism takes Marcella's place on five (Varg's too, PRISM_OVER_VARG) and
// Ryth's on four, only on maps whose consistent camp gems (prism_gems) reach the bar; three is unchanged.
constexpr Draft ROSTER_5[] = {Draft::ryth, Draft::heart_of_ruin, Draft::willow, Draft::vesper_vale, Draft::marcella};
constexpr Draft ROSTER_5_PRISM[] = {Draft::ryth, Draft::heart_of_ruin, Draft::willow, Draft::vesper_vale, Draft::prism};
constexpr Draft ROSTER_4[] = {Draft::heart_of_ruin, Draft::vesper_vale, Draft::willow, Draft::ryth};
constexpr Draft ROSTER_4_PRISM[] = {Draft::heart_of_ruin, Draft::vesper_vale, Draft::willow, Draft::prism};
constexpr Draft ROSTER_3[] = {Draft::heart_of_ruin, Draft::marcella, Draft::vesper_vale};
// v150 (VENDETTA_3): Vendetta in Marcella's place on three.
constexpr Draft ROSTER_3_VENDETTA[] = {Draft::heart_of_ruin, Draft::vendetta, Draft::vesper_vale};
// v188 (DUO_3): four bodies from three picks.
constexpr Draft ROSTER_3_DUO[] = {Draft::vesper_vale, Draft::marcella, Draft::cassi_and_issac};
// v191 (TOP_ROSTER): what Mite and Intercontinental Fish draft on four and five picks.
constexpr Draft ROSTER_4_TOP[] = {Draft::marcella, Draft::vendetta, Draft::heart_of_ruin, Draft::cassi_and_issac};
constexpr Draft ROSTER_5_TOP[] = {Draft::vesper_vale, Draft::vendetta, Draft::heart_of_ruin, Draft::marcella, Draft::cassi_and_issac};
// v113: five with Varg in Marcella's place, on maps packed with wolves and gem clusters.
constexpr Draft ROSTER_5_VARG[] = {Draft::ryth, Draft::heart_of_ruin, Draft::willow, Draft::vesper_vale, Draft::varg};
// v158: the Prism team on gem-cluster maps (prism_map); smaller teams take the first picks.
constexpr Draft ROSTER_PRISM_TEAM[] = {Draft::prism, Draft::heart_of_ruin, Draft::marcella, Draft::ryth,
                                       PRISM_WILLOW ? Draft::willow : Draft::vendetta};

// v169 (PRISM_WILLOW_4): four picks.
constexpr Draft ROSTER_PRISM_TEAM_4[] = {Draft::prism, Draft::heart_of_ruin, Draft::marcella, Draft::willow};

bool varg_map() {
    int wolves = 0, gems = 0;
    for (const auto& group : group_data().groups)
        for (const auto& camp : group.camps()) {
            if (camp.kind() == Camp::wolf) ++wolves;
            if (camp.kind() == Camp::gem_cluster) ++gems;
        }
    return wolves >= VARG_WOLVES && gems >= VARG_GEM_CLUSTERS;
}

int prism_gems() {
    // v123: the gems a map's steady camps give per clear: wolves 4, elks 15, gem clusters 7. Golems and monoliths are
    // left out (not consistent sources), as are gold clusters.
    int gems = 0;
    for (const auto& group : group_data().groups)
        for (const auto& camp : group.camps()) {
            if (camp.kind() == Camp::wolf) gems += 4;
            else if (camp.kind() == Camp::elk) gems += 15;
            else if (camp.kind() == Camp::gem_cluster) gems += 7;
        }
    return gems;
}

std::span<const Draft> roster(std::uint32_t size) {
    if (prism_map() && PRISM_WILLOW_4 && size == 4) return ROSTER_PRISM_TEAM_4;
    if (prism_map()) return std::span<const Draft>(ROSTER_PRISM_TEAM).first(size);
    if (size <= 3 && DUO_3) return ROSTER_3_DUO;
    if (size <= 3) return VENDETTA_3 ? std::span<const Draft>(ROSTER_3_VENDETTA) : ROSTER_3;
    if (TOP_ROSTER) return size == 4 ? std::span<const Draft>(ROSTER_4_TOP) : std::span<const Draft>(ROSTER_5_TOP);
    static const int gems = prism_gems();
    if (size == 4) return gems >= PRISM_MIN_GEMS_4 ? std::span<const Draft>(ROSTER_4_PRISM) : ROSTER_4;
    if (gems >= PRISM_MIN_GEMS_5 && PRISM_OVER_VARG) return ROSTER_5_PRISM;
    static const bool varg = varg_map();
    if (gems >= PRISM_MIN_GEMS_5 && !varg) return ROSTER_5_PRISM;
    return varg ? std::span<const Draft>(ROSTER_5_VARG) : ROSTER_5;
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
// Aggressive route: distance to the map centre at which a pocket is worth half, and the closest
// walking distance to the enemy spawn a group may sit at.
constexpr int CENTER_FALLOFF = 5;
constexpr int ENEMY_SPAWN_BUFFER = 12;
// The aggressive route stays on the centre pocket and its neighbours.
constexpr int AGGRESSIVE_RADIUS = 12;
// First byte of the setup block in draft memory.
constexpr std::uint8_t SETUP_MAGIC = 0xA7;
// v86: first byte of the zone block, right after the setup block.
constexpr std::uint8_t ZONE_MAGIC = 0xC6;

// Health and strength per kind, from the rules; camps outside vision hide them.
std::pair<int, int> camp_stats(Camp kind) {
    switch (kind) {
    case Camp::wolf: return {1350, 20};
    case Camp::elk: return {2400, 60};
    // v96: the balance patch cut cluster health from 500 to 120.
    case Camp::gem_cluster: return {120, 0};
    case Camp::gold_cluster: return {120, 0};
    case Camp::monolith: return {20000, 200};
    // v94: any kind this SDK does not name is the golem of the balance patch.
    default: return {5000, 120};
    }
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

std::vector<std::uint8_t> encode_setup() {
    // Layout: SETUP_MAGIC, spot count, (x, y) per ward spot, altar count, one solo flag byte per altar.
    std::vector<std::uint8_t> out;
    const auto spots = compute_ward_spots();
    const auto altars = bc::map::altars().positions();
    if (spots.size() > 255 || altars.size() > 255) return out;
    out.push_back(SETUP_MAGIC);
    out.push_back(static_cast<std::uint8_t>(spots.size()));
    for (const auto spot : spots) {
        out.push_back(static_cast<std::uint8_t>(spot.x));
        out.push_back(static_cast<std::uint8_t>(spot.y));
    }
    out.push_back(static_cast<std::uint8_t>(altars.size()));
    for (const auto altar : altars) out.push_back(solo_altar(altar) ? 1 : 0);
    return out;
}

std::vector<std::uint8_t> encode_zones() {
    // v86 (V86_ZONES). Layout: ZONE_MAGIC, group count, then per group (bc::map::camp_groups() order) a flag byte
    // (zone in bits 0-1, bit 2 = a pair can clear it alone), walking steps from our spawn and from theirs (255 for
    // unreachable). Two map-wide walking fields: draft fuel the champions' turns would miss.
    std::vector<std::uint8_t> out;
    if (!V86_ZONES) return out;
    const auto& data = group_data();
    const int count = static_cast<int>(data.groups.size());
    if (count > 255) return out;
    std::vector<int> ids;
    for (int index = 0; index < count; ++index) ids.push_back(index);
    const auto ours = walking_gaps(data, ids, bc::map::spawn_region(Team::ally));
    const auto theirs = walking_gaps(data, ids, bc::map::spawn_region(Team::enemy));
    out.push_back(ZONE_MAGIC);
    out.push_back(static_cast<std::uint8_t>(count));
    for (int index = 0; index < count; ++index) {
        int zone = ZONE_UNCERTAIN;
        // v168 (PRISM_OUR_SIDE): safe is every group we reach no later than they do.
        if (ours[index] < UNREACHABLE_GAP && ours[index] * (PRISM_OUR_SIDE && prism_map() ? 1.0 : ZONE_SIDE) <= theirs[index]) zone = ZONE_SAFE;
        else if (theirs[index] < UNREACHABLE_GAP && theirs[index] * ZONE_SIDE <= ours[index]) zone = ZONE_UNSAFE;
        int strength = 0, health = 0;
        for (const auto& camp : data.groups[index].camps()) {
            const auto [hp, hit] = camp_stats(camp.kind());
            health += hp;
            strength += hit;
        }
        const bool pair_ok = !data.groups[index].camps().empty()
            && static_cast<double>(strength) * health / PAIR_DAMAGE <= PAIR_TAKE;
        out.push_back(static_cast<std::uint8_t>(zone | (pair_ok ? 4 : 0)));
        out.push_back(static_cast<std::uint8_t>(std::min(ours[index], UNREACHABLE_GAP)));
        out.push_back(static_cast<std::uint8_t>(std::min(theirs[index], UNREACHABLE_GAP)));
    }
    return out;
}

std::vector<std::uint8_t> encode_routes() {
    // Normal route block, the aggressive route block (always written, maybe empty, so the setup block
    // after it is found) trimmed to fit draft memory, then the setup block and (v86) the zone block.
    std::vector<std::uint8_t> memory;
    auto normal = compute_pocket();
    if (!normal.anchor) return memory;
    encode_pocket(memory, *normal.anchor, normal.members);
    const auto setup = encode_setup();
    const auto zones = encode_zones();
    auto aggressive = compute_pocket(true);
    const int room = (static_cast<int>(bc::constants::draft_memory_bytes) - static_cast<int>(memory.size()) - 3
                      - static_cast<int>(setup.size()) - static_cast<int>(zones.size())) / 2;
    if (!aggressive.anchor || room <= 0) aggressive.members.clear();
    if (static_cast<int>(aggressive.members.size()) > room) aggressive.members.resize(std::max(0, room));
    encode_pocket(memory, aggressive.anchor.value_or(vec2{0, 0}), aggressive.members);
    if (memory.size() + setup.size() <= bc::constants::draft_memory_bytes) {
        memory.insert(memory.end(), setup.begin(), setup.end());
        if (!setup.empty() && memory.size() + zones.size() <= bc::constants::draft_memory_bytes)
            memory.insert(memory.end(), zones.begin(), zones.end());
    }
    return memory;
}

} // namespace

bool prism_map() {
    static const bool rule = [] {
        int all = 0, clusters = 0;
        for (const auto& group : group_data().groups)
            for (const auto& camp : group.camps()) {
                ++all;
                if (camp.kind() == Camp::gem_cluster) ++clusters;
            }
        return all > 0 && clusters > PRISM_TEAM_BAR * all;
    }();
    return rule;
}

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

GroupStats group_stats(const bc::map::camp_group& group) {
    GroupStats stats;
    for (const auto& camp : group.camps()) {
        const auto [hp, hit] = camp_stats(camp.kind());
        stats.health += hp;
        stats.strength += hit;
        if (camp.kind() == Camp::monolith) stats.monolith = true;
        if (hit >= STRONG_CAMP_STRENGTH) stats.strong += hit;
        if (camp.kind() == Camp::gem_cluster) ++stats.gem_clusters;
        if (camp.kind() == Camp::elk) ++stats.elks;
    }
    return stats;
}

double clear_rounds(const GroupStats& stats) { return static_cast<double>(stats.health) / TEAM_DAMAGE; }

double group_load(const GroupStats& stats) { return static_cast<double>(stats.strength) * stats.health; }

ValueParts value_parts(const bc::map::camp_group& group, double gold_weight, const GroupStats& stats) {
    ValueParts parts;
    const double rounds = clear_rounds(stats);
    parts.reward = group.gem_value() + gold_weight * group.gold_value();
    // v158: on the Prism team's map gem clusters come first and gold barely counts.
    if (prism_map())
        parts.reward = group.gem_value() + (PRISM_CLUSTER_VALUE - 1) * GEM_CLUSTER_GEMS * stats.gem_clusters
                       + std::min(gold_weight, GOLD_WEIGHT) * group.gold_value();
    parts.cost = DAMAGE_COST * (stats.strength * rounds);
    parts.strong_cost = DAMAGE_COST * (stats.strong * rounds);
    // v72 (FIX_GEM_FIRST): a gold cluster (no gems) is a filler for when nothing better is up.
    parts.scale = FIX_GEM_FIRST && group.gem_value() == 0 ? GEMLESS_VALUE : 1.0;
    // v177 (ECON_PHASES): the parts value_phased reweighs during the match.
    parts.elk_gems = ELK_GEMS * stats.elks;
    parts.cluster_gems = GEM_CLUSTER_GEMS * stats.gem_clusters;
    parts.gemless = FIX_GEM_FIRST && group.gem_value() == 0;
    return parts;
}

double group_value(const bc::map::camp_group& group, double gold_weight, const GroupStats& stats) {
    return value_of(value_parts(group, gold_weight, stats), 0);
}

double clear_rounds(const bc::map::camp_group& group) { return clear_rounds(group_stats(group)); }

double group_load(const bc::map::camp_group& group) { return group_load(group_stats(group)); }

double group_value(const bc::map::camp_group& group, double gold_weight) {
    return group_value(group, gold_weight, group_stats(group));
}

Pocket compute_pocket(bool aggressive) {
    // The normal route favours pockets near an altar; the aggressive one favours the map centre
    // and leaves out groups close to the enemy spawn.
    const auto& data = group_data();
    const int count = static_cast<int>(data.groups.size());
    std::vector<int> ids;  // groups with camps, ascending
    for (int index = 0; index < count; ++index)
        if (data.has_camps[index]) ids.push_back(index);
    if (aggressive) {
        const auto enemy_gaps = walking_gaps(data, ids, bc::map::spawn_region(Team::enemy));
        std::erase_if(ids, [&](int index) { return enemy_gaps[index] < ENEMY_SPAWN_BUFFER; });
    }
    if (ids.empty()) return {};

    const double weight = aggressive ? GOLD_WEIGHT : FARM_GOLD_WEIGHT;
    std::vector<double> incomes(count, 0.0);
    for (const int index : ids) incomes[index] = group_income(data.groups[index], weight);
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

    // v168 (PRISM_OUR_SIDE): the plan anchors on our side and counts only its income; the route still lists every group.
    const bool our_side = PRISM_OUR_SIDE && prism_map();
    std::vector<int> side_ours, side_theirs;
    if (our_side) {
        side_ours = walking_gaps(data, ids, bc::map::spawn_region(Team::ally));
        side_theirs = walking_gaps(data, ids, bc::map::spawn_region(Team::enemy));
        for (const int index : ids)
            if (side_ours[index] > side_theirs[index]) incomes[index] = 0;
    }
    std::optional<std::pair<double, int>> best;
    for (const int index : ids) {
        if (our_side && side_ours[index] > side_theirs[index]) continue;
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

    if (!best) return {};
    const auto anchor = data.centers[best->second];
    auto route = members_near(anchor, aggressive ? AGGRESSIVE_RADIUS : ROAM_RADIUS);
    std::stable_sort(route.begin(), route.end(), [&](int a, int b) {
        return std::make_pair(distance(anchor, data.centers[a]), a) < std::make_pair(distance(anchor, data.centers[b]), b);
    });
    if (route.size() > 255) route.resize(255);
    return {anchor, route};
}

bool solo_altar(vec2 altar) {
    const vec2 middle{bc::map::width() / 2, bc::map::height() / 2};
    static const auto ours = bc::map::spawn_region(Team::ally).positions();
    static const auto theirs = bc::map::spawn_region(Team::enemy).positions();
    auto nearest = [](vec2 spot, const std::vector<vec2>& tiles) {
        std::optional<int> best;
        for (const auto t : tiles)
            if (!best || distance(spot, t) < *best) best = distance(spot, t);
        return best.value_or(0);
    };
    return distance(altar, middle) > CENTRAL_ALTAR_RANGE && nearest(altar, ours) <= nearest(altar, theirs);
}

std::optional<SetupFacts> decode_setup(const std::vector<std::uint8_t>& data, std::size_t offset) {
    if (offset + 2 > data.size() || data[offset] != SETUP_MAGIC) return std::nullopt;
    SetupFacts facts;
    std::size_t at = offset + 1;
    const std::size_t spots = data[at++];
    if (at + 2 * spots + 1 > data.size()) return std::nullopt;
    for (std::size_t i = 0; i < spots; ++i, at += 2) facts.ward_spots.push_back({data[at], data[at + 1]});
    const std::size_t altars = data[at++];
    if (at + altars > data.size()) return std::nullopt;
    for (std::size_t i = 0; i < altars; ++i) facts.solo.push_back(data[at + i] != 0);
    return facts;
}

std::size_t setup_end(const std::vector<std::uint8_t>& data, std::size_t offset) {
    if (offset + 2 > data.size() || data[offset] != SETUP_MAGIC) return offset;
    std::size_t at = offset + 1;
    at += 1 + 2 * static_cast<std::size_t>(data[at]);
    if (at >= data.size()) return data.size();
    at += 1 + data[at];
    return std::min(at, data.size());
}

std::optional<std::size_t> zones_block(const std::vector<std::uint8_t>& data, std::size_t offset) {
    if (offset + 2 > data.size() || data[offset] != ZONE_MAGIC) return std::nullopt;
    const std::size_t count = data[offset + 1];
    if (count != group_data().groups.size() || offset + 2 + 3 * count > data.size()) return std::nullopt;
    return offset + 2;
}

std::optional<ZoneFacts> decode_zones(const std::vector<std::uint8_t>& data, std::size_t offset) {
    if (offset + 2 > data.size() || data[offset] != ZONE_MAGIC) return std::nullopt;
    const std::size_t count = data[offset + 1];
    if (count != group_data().groups.size() || offset + 2 + 3 * count > data.size()) return std::nullopt;
    ZoneFacts facts;
    for (std::size_t i = 0; i < count; ++i) {
        const std::size_t at = offset + 2 + 3 * i;
        facts.zone.push_back(data[at] & 3);
        facts.pair_ok.push_back((data[at] & 4) != 0);
        facts.ours.push_back(data[at + 1]);
        facts.theirs.push_back(data[at + 2]);
    }
    return facts;
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
