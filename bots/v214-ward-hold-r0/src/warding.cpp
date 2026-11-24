// Ward spots, placement, and the walk over to an uncovered spot.
#include "bot.h"
#include "seen.h"
#include "state.h"
#include "trace.h"
#include "probe.h"
#include <algorithm>
#include <array>
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
            NOTE(format("ward on open spot %d ", index) + at(tile));
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
    WHY("ward.detour", format("quiet, light bag, ward unused: walk to the open spot %d away ", distance(position, *spot)) + at(*spot));
    const bool ok = move_towards_closest(tiles_around(*spot, bc::constants::ward_placement_radius));
    if (!ok) NOTE("ward detour failed: " + trace::stall);
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
            NOTE(std::string("ward into a key bush (") + KINDS[kind] + ") " + at(tile));
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
        NOTE(format("bush ward moves near the new target s%d ", target) + at(tile));
        return true;
    }
    return false;
}

bool Bot::scout_ward(const bc::champion& me) {
    // v72: the ward follows us. Vision is not shared, so a ward far off only helps its owner's
    // view of somewhere else; one beside us shows what is about to hit us. It goes into a reachable bush we
    // cannot see into (hidden enemies and camps), else up to two tiles ahead toward our target.
    if (SCOUT_KEEP_POSTS && (ward_slot || bush_warded)) return false;
    // v214-ward-hold (WARD_HOLD): note an enemy champion within WARD_HOLD_RANGE every turn (cached vision list, no
    // engine read), so the WARD_HOLD_ROUNDS window also covers rounds the charge was recharging.
    bool hold_near = false;
    if constexpr (WARD_HOLD) {
        const auto here = seen::self_position();
        for (const auto p : seen::positions(Team::enemy))
            if (distance(p, here) <= WARD_HOLD_RANGE) {
                hold_near = true;
                last_near_enemy_round = turn_round;
                break;
            }
    }
    try {
        if (me.ward_cooldown() > 0) return false;
    } catch (const bc::error&) {
        return false;
    }
    // v214-ward-hold: a fight is likely, so the charge is kept for ghost_ward and answer_hidden.
    if (WARD_HOLD && (mode == FIGHT || hold_near || turn_round - last_near_enemy_round < WARD_HOLD_ROUNDS)) {
        LOG(probe::ev(format("held fight=%d near=%d ago=%d", mode == FIGHT ? 1 : 0, hold_near ? 1 : 0,
                             turn_round - last_near_enemy_round)););
        return false;
    }
    const auto position = me.position();
    const auto own = me.ward_positions();
    const int reach = static_cast<int>(bc::constants::ward_placement_radius);
    // A ward already watching a bush beside us stays: moving it would hide that bush again.
    for (const auto ward : own)
        if (bc::map::bushes().is_set(ward) && distance(position, ward) <= reach + 1) return false;
    // Moving costs fuel (every vision list is read again), so the ward only moves to look into a bush beside
    // us, or to catch up once it has fallen SCOUT_REFRESH tiles behind. Cheap checks first.
    const bool behind = own.empty() || distance(position, own.front()) > SCOUT_REFRESH;
    // v133t: reused boards and *_into reads (each `~`, `|`, new board and positions() allocated: ~8 a call).
    static bc::bitboard near_bush{bc::uninitialized}, tiles{bc::uninitialized}, part{bc::uninitialized},
        hidden{bc::uninitialized};
    fast::around_into(position, static_cast<float>(reach), near_bush);
    near_bush &= bc::map::bushes();
    if (!behind && near_bush.is_empty()) return false;

    me.ward_placement_tiles_into(tiles);
    bc::vision::ward_tiles_into(Team::ally, part);
    part |= seen::terrain();
    fast::not_into(part, part);
    tiles &= part;
    if (tiles.is_empty()) return false;
    fast::and_into(tiles, near_bush, hidden);
    if (!hidden.is_empty()) {
        bc::vision::visible_tiles_into(part);
        fast::not_into(part, part);
        hidden &= part;
    }
    if (!behind && hidden.is_empty()) return false;
    std::optional<vec2> goal;
    if (!route.empty()) goal = centers[slot % static_cast<int>(route.size())];

    std::optional<vec2> best;
    int best_score = 0;
    std::array<vec2, 64> spots;
    const auto spot_n = std::min<std::size_t>(tiles.positions_into(spots), spots.size());
    for (std::size_t i = 0; i < spot_n; ++i) {
        const auto tile = spots[i];
        int score = (hidden.is_set(tile) ? SCOUT_BUSH_SCORE : 0) + 2 * distance(position, tile);
        if (goal) score -= 3 * distance(tile, *goal);
        if (!best || score > best_score) {
            best = tile;
            best_score = score;
        }
    }
    if (!best || std::find(own.begin(), own.end(), *best) != own.end()) return false;
    int wp_before = 0, wp_gain = 0;
    int wp_near = 0;
    LOG(wp_before = probe::enemies_in_sight(); wp_gain = probe::gain(*best); wp_near = probe::near_now(););
    try {
        if (!me.place_ward(*best)) {
            LOG(probe::ev(format("scout refused %d %d", best->x, best->y)););
            // Something hidden stands there, and is now revealed.
            seen::reset();
            return false;
        }
    } catch (const bc::error&) {
        return false;
    }
    trace::act("ward");
    LOG(probe::ev(format("place scout-%s gain=%d dist=%d rev=%d near=%d fight=%d ago=%d", hidden.is_set(*best) ? "bush" : behind ? "catch" : "other", wp_gain,
                         distance(position, *best), probe::enemies_in_sight() - wp_before, wp_near, mode == FIGHT ? 1 : 0,
                         static_cast<int>(bc::round_number()) - probe::last_near)););
    // v90-log: the ward follows us every round (the "ward" act shows it); only a look into a hidden bush is worth a line.
    if (hidden.is_set(*best)) NOTE("scout ward into a hidden bush " + at(*best));
    return true;
}
