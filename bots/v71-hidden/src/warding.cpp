// Ward spots, placement, and the walk over to an uncovered spot.
#include "bot.h"
#include "seen.h"
#include "state.h"
#include "trace.h"
#include <algorithm>
#include <cstdlib>
#include <algorithm>
#include <tuple>

std::vector<vec2> compute_ward_spots() {
    // Deterministic, so every champion agrees on the bit for each spot.
    const auto free = ~(bc::map::walls() | bc::map::holes() | bc::map::altars() | bc::map::spawn_region(Team::ally)
                        | bc::map::spawn_region(Team::enemy));
    const vec2 middle{bc::map::width() / 2, bc::map::height() / 2};
    std::vector<vec2> spots;

    // A tile is taken once it is within the anti-ward radius of a chosen spot. With at most a handful
    // of spots, checking them directly is far cheaper than reading a board tile by tile.
    auto taken = [&](vec2 tile) {
        for (const auto spot : spots)
            if (distance(tile, spot) <= ANTI_WARD_RADIUS - 1) return true;
        return false;
    };
    // order holds tiles nearest the middle first, then by x and y.
    auto pick = [&](const std::vector<vec2>& order, int count) {
        for (const auto tile : order) {
            if (count == 0) break;
            if (!taken(tile)) {
                spots.push_back(tile);
                --count;
            }
        }
        return count;
    };
    // Sort by a packed (ring, x, y) key so the comparisons are plain integers.
    auto sorted_by = [](const std::vector<vec2>& tiles, auto ring_of) {
        std::vector<std::pair<std::int64_t, vec2>> keyed;
        keyed.reserve(tiles.size());
        for (const auto t : tiles)
            keyed.push_back({(static_cast<std::int64_t>(ring_of(t)) << 32) | (static_cast<std::int64_t>(t.x) << 16) | t.y, t});
        std::sort(keyed.begin(), keyed.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
        std::vector<vec2> order;
        order.reserve(keyed.size());
        for (const auto& entry : keyed) order.push_back(entry.second);
        return order;
    };
    auto ring_from_middle = [&](vec2 t) { return std::max(std::abs(t.x - middle.x), std::abs(t.y - middle.y)); };

    // Central spots ring by ring from the middle, out to the anti-ward radius, each ring in (x, y)
    // order. The ring's tiles are generated in that order directly (sorting ~300 tiles cost ~80k fuel),
    // and the board is only read for tiles no chosen spot has already ruled out.
    int count = CENTRAL_WARDS;
    const int width = bc::map::width(), height = bc::map::height();
    for (int r = 0; r <= ANTI_WARD_RADIUS && count > 0; ++r) {
        for (int x = middle.x - r; x <= middle.x + r && count > 0; ++x) {
            const bool edge = x == middle.x - r || x == middle.x + r;
            for (int y = middle.y - r; y <= middle.y + r && count > 0; y += (edge || r == 0) ? 1 : 2 * r) {
                const vec2 tile{x, y};
                if (x < 0 || y < 0 || x >= width || y >= height || taken(tile) || !free.is_set(tile)) continue;
                spots.push_back(tile);
                --count;
            }
        }
    }

    bc::bitboard near, inner;
    for (const auto tile : bc::map::spawn_region(Team::enemy).positions()) {
        near.draw_chebyshev(tile, ENEMY_WARD_GAP);
        inner.draw_chebyshev(tile, ENEMY_WARD_GAP - 1);
    }
    // The ring side facing the middle comes first, since that is where the enemy walks out.
    near &= ~inner;
    near &= free;
    pick(sorted_by(near.positions(), ring_from_middle), ENEMY_SPAWN_WARDS);
    if (spots.size() > 8) spots.resize(8);
    return spots;
}

std::vector<int> Bot::open_ward_spots(const bc::champion& me) {
    // Uncovered spot indexes; any spot a visible allied ward already covers is marked for everyone.
    // Each spot's zone is fixed, so the zones and their union are built once.
    if (!ward_zones) {
        ward_zones.emplace();
        ward_union.clear();
        for (const auto spot : ward_spots) {
            ward_zones->push_back(tiles_around(spot, ANTI_WARD_RADIUS - 1));
            ward_union |= ward_zones->back();
        }
    }
    int mask = shared_byte(WARD_MASK_BYTE);
    std::optional<bc::bitboard> allied;
    bool covered = false;
    std::vector<int> open_spots;
    for (int index = 0; index < static_cast<int>(ward_spots.size()); ++index) {
        if (mask >> index & 1) continue;
        if (!allied) {
            allied = bc::vision::ward_tiles(Team::ally);
            for (const auto own : me.ward_positions()) allied->set(own, false);
            *allied &= ward_union;
            covered = !allied->is_empty();
        }
        if (covered && !(*allied & (*ward_zones)[index]).is_empty()) {
            mask |= 1 << index;
            set_shared_byte(WARD_MASK_BYTE, mask);
            continue;
        }
        open_spots.push_back(index);
    }
    return open_spots;
}

bool Bot::place_ward(const bc::champion& me) {
    // A ward is placed once: one already in a bush stays there.
    if (bush_warded || ward_spots.empty()) return false;
    if (ward_slot) {
        if (!me.ward_positions().empty()) return false;
        // Our ward is gone, so its spot is open again.
        set_shared_byte(WARD_MASK_BYTE, shared_byte(WARD_MASK_BYTE) & ~(1 << *ward_slot));
        ward_slot.reset();
    }
    try {
        if (me.ward_cooldown() > 0) return false;
    } catch (const bc::error&) {
        return false;
    }

    const auto position = me.position();
    const double reach = bc::constants::ward_placement_radius;
    for (const int index : open_ward_spots(me)) {
        const auto spot = ward_spots[index];
        if (distance(position, spot) > reach + WARD_SLACK) continue;
        auto tiles = me.ward_placement_tiles() & tiles_around(spot, WARD_SLACK);
        tiles &= ~(seen::terrain() | bc::vision::ward_tiles());
        auto order = tiles.positions();
        std::stable_sort(order.begin(), order.end(), [&](vec2 a, vec2 b) { return distance(a, spot) < distance(b, spot); });
        for (const auto tile : order) {
            try {
                if (!me.place_ward(tile)) continue;
            } catch (const bc::error&) {
                continue;
            }
            ward_slot = index;
            set_shared_byte(WARD_MASK_BYTE, shared_byte(WARD_MASK_BYTE) | 1 << index);
            trace::act("ward");
            note(format("placed ward %d at ", index) + at(tile));
            say(format("round %u: champion %d wards spot %d", bc::round_number(), champ_id(me.champion_name()), index));
            return true;
        }
    }
    return false;
}

bool Bot::ward_detour(const bc::champion& me, int gems) {
    // Only a champion whose ward is still free, carrying little and not mid-clear, walks to a spot.
    if (ward_slot || bush_warded || ward_spots.empty() || gems >= IDLE_BANK || !group_camps().empty() || under_threat(me))
        return false;
    const auto position = me.position();
    const int limit = aggressive ? AGGRESSIVE_WARD_DETOUR_RANGE : WARD_DETOUR_RANGE;
    std::optional<vec2> spot;
    for (const int i : open_ward_spots(me)) {
        const auto candidate = ward_spots[i];
        if (distance(position, candidate) > limit) continue;
        if (!spot || distance(position, candidate) < distance(position, *spot)) spot = candidate;
    }
    if (!spot) return false;
    branch("ward.detour", "uncovered ward " + at(*spot) + format(" within %d, bag light", limit));
    const bool ok = move_towards_closest(tiles_around(*spot, bc::constants::ward_placement_radius));
    if (!ok) note("ward detour to " + at(*spot) + " failed: " + (trace::stall.empty() ? "None" : trace::stall));
    return ok;
}

void Bot::build_bush_boards() {
    // Key bushes by priority, each class without the tiles of the ones before it. Built once.
    const auto& bush = bc::map::bushes();
    std::array<bc::bitboard, 3> boards;
    if (!bush.is_empty()) {
        bc::bitboard area;
        for (const auto altar : seen::altar_list()) area.draw_chebyshev(altar, BUSH_ALTAR_RANGE);
        boards[0] = bush;
        boards[0] &= area;
        area.clear();
        area.draw_chebyshev({bc::map::width() / 2, bc::map::height() / 2}, BUSH_CENTER_RANGE);
        boards[1] = bush;
        boards[1] &= area;
        boards[1] &= ~boards[0];
        area.clear();
        for (const auto center : centers) area.draw_chebyshev(center, BUSH_CAMP_RANGE);
        boards[2] = bush;
        boards[2] &= area;
        boards[2] &= ~boards[0];
        boards[2] &= ~boards[1];
    }
    key_bush_reach = boards[0];
    key_bush_reach |= boards[1];
    key_bush_reach |= boards[2];
    // Grown one Chebyshev ring at a time by the placement range.
    for (int ring = 0; ring < static_cast<int>(bc::constants::ward_placement_radius); ++ring) {
        key_bush_reach |= key_bush_reach.shift_right(1) | key_bush_reach.shift_right(-1);
        key_bush_reach |= key_bush_reach.shift_up(1) | key_bush_reach.shift_up(-1);
    }
    key_bushes = std::move(boards);
}

bool Bot::bush_ward(const bc::champion& me) {
    // A ward still unused goes once into a key bush we cannot see into. Never on a fight turn or with
    // an enemy in sight, and never next to a bush a visible allied ward already watches.
    if (ward_slot || !key_bushes || mode == FIGHT || route.empty()) return false;
    const int target = slot % static_cast<int>(route.size());
    if (bush_warded) return follow_ward(me, target);
    const auto position = me.position();
    if (!key_bush_reach.is_set(position) || bc::map::bushes().is_set(position)) return false;
    if (!seen::champions(Team::enemy).empty()) return false;
    try {
        if (me.ward_cooldown() > 0 || !me.ward_positions().empty()) return false;
    } catch (const bc::error&) {
        return false;
    }
    auto tiles = me.ward_placement_tiles();
    tiles &= ~bc::vision::visible_tiles();
    if (tiles.is_empty()) return false;
    const auto allied = bc::vision::ward_tiles(Team::ally).positions();
    static const char* const KINDS[] = {"altar", "centre", "camp"};
    for (int kind = 0; kind < 3; ++kind) {
        auto options = tiles;
        options &= (*key_bushes)[kind];
        if (options.is_empty()) continue;
        for (const auto tile : options.positions()) {
            bool watched = false;
            for (const auto ward : allied)
                if (distance(ward, tile) <= BUSH_WARD_GAP) watched = true;
            if (watched) continue;
            try {
                if (!me.place_ward(tile)) {
                    // Something hidden stands there, and is now revealed.
                    seen::reset();
                    continue;
                }
            } catch (const bc::error&) {
                continue;
            }
            bush_warded = true;
            ward_for_slot = target;
            trace::act("ward");
            note(std::string("bush ward (") + KINDS[kind] + ") at " + at(tile));
            say(format("round %u: champion %d wards %s bush (%d,%d)", bc::round_number(), champ_id(me.champion_name()),
                       KINDS[kind], tile.x, tile.y));
            return true;
        }
    }
    return false;
}

bool Bot::follow_ward(const bc::champion& me, int target) {
    // The bush ward was placed for another target: move it to a hidden bush near the current one once we
    // are close enough to reach one. Checked only after the target has moved, so most turns stop here.
    if (ward_for_slot == target) return false;
    const auto position = me.position();
    const auto center = centers[target];
    const int reach = static_cast<int>(bc::constants::ward_placement_radius);
    if (distance(position, center) > FOLLOW_BUSH_RANGE + reach || bc::map::bushes().is_set(position)) return false;
    if (!seen::champions(Team::enemy).empty()) return false;
    try {
        if (me.ward_cooldown() > 0) return false;
    } catch (const bc::error&) {
        return false;
    }
    auto tiles = me.ward_placement_tiles();
    tiles &= bc::map::bushes();
    // Most spots have no bush in placement reach: stop before building the target's area board.
    if (tiles.is_empty()) return false;
    tiles &= tiles_around(center, FOLLOW_BUSH_RANGE);
    if (tiles.is_empty()) return false;
    tiles &= ~bc::vision::visible_tiles();
    if (tiles.is_empty()) return false;
    const auto allied = bc::vision::ward_tiles(Team::ally).positions();
    const auto own = me.ward_positions();
    for (const auto tile : tiles.positions()) {
        bool watched = false;
        for (const auto ward : allied)
            if (distance(ward, tile) <= BUSH_WARD_GAP && std::find(own.begin(), own.end(), ward) == own.end())
                watched = true;
        if (watched) continue;
        try {
            if (!me.place_ward(tile)) {
                seen::reset();
                continue;
            }
        } catch (const bc::error&) {
            continue;
        }
        ward_for_slot = target;
        trace::act("ward");
        note("bush ward follows to " + at(tile) + format(" for slot %d", target));
        say(format("round %u: champion %d moves ward to bush (%d,%d) for slot %d", bc::round_number(),
                   champ_id(me.champion_name()), tile.x, tile.y, target));
        return true;
    }
    return false;
}
