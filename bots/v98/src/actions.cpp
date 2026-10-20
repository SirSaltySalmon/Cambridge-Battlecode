#include "actions.h"
#include "board.h"
#include "seen.h"
#include "state.h"
#include "trace.h"
#include <algorithm>
#include <set>
#include <tuple>

const bc::bitboard& thin_holes() {
    // Hole tiles with standable terrain on both opposite sides (horizontally, vertically or diagonally): a dash of
    // two tiles crosses them. Built once from the static map.
    static const bc::bitboard board = [] {
        bc::bitboard thin;
        const auto& walls = bc::map::walls();
        const auto& holes = bc::map::holes();
        const auto& altars = bc::map::altars();
        const int w = bc::map::width(), h = bc::map::height();
        auto standable = [&](int x, int y) {
            if (x < 0 || y < 0 || x >= w || y >= h) return false;
            const vec2 p{x, y};
            return !walls.is_set(p) && !holes.is_set(p) && !altars.is_set(p);
        };
        constexpr int dirs[4][2] = {{1, 0}, {0, 1}, {1, 1}, {1, -1}};
        for (const auto p : holes.positions())
            for (const auto& d : dirs)
                if (standable(p.x + d[0], p.y + d[1]) && standable(p.x - d[0], p.y - d[1])) {
                    thin.set(p);
                    break;
                }
        return thin;
    }();
    return board;
}

namespace {

// v98 (FIX_HOP): a dasher's route through thin holes when it beats walking by HOP_SAVE steps (or walking cannot
// reach the goal). Dashes across when ready, else walks toward the edge or waits there. Returns whether the hop
// route was taken: the turn's movement is then decided, waiting included, so the caller's branch keeps the turn
// (a wait that returned false let the next branch walk us off the edge, and the dash then hopped us back).
bool hop_route(const bc::champion& me, vec2 origin, const bc::bitboard& walls, const bc::bitboard& goals,
               const bc::bitboard& blocked, const bc::distance_field& walk, const bc::bitboard& walk_reached,
               std::optional<bc::ability> dash, bool& acted) {
    const auto& thin = thin_holes();
    if (thin.is_empty() || bc::fuel_remaining() < HOP_FUEL) return false;
    // Board-level checks only: looping over the goal tiles (thousands on some boards) ran 10x more turns out of fuel.
    if (walk_reached.is_set(origin) && static_cast<int>(walk.get(origin)) < HOP_SAVE + 2) return false;
    auto near = tiles_around(origin, static_cast<float>(HOP_NEAR));
    near &= thin;
    if (near.is_empty()) return false;
    auto hop_walls = walls;
    hop_walls &= ~thin;
    bc::distance_field hop;
    bc::bitboard hop_reached;
    hop.compute_to(hop_walls, goals, tiles_around(origin, 0), hop_reached);
    if (!hop_reached.is_set(origin)) return false;
    const auto via_hop = hop.get(origin);
    // A recharging dash means waiting at the edge: its remaining cooldown counts against the hop (fracture: 217
    // edge-wait turns, and the hop variant lost a map v97 won 4 of 4).
    std::int64_t recharge = 0;
    if (const auto any = ability_of(me, Kind::dash)) recharge = std::max<std::int64_t>(0, any->cooldown());
    if (walk_reached.is_set(origin)
        && static_cast<std::int64_t>(via_hop) + recharge + HOP_SAVE > static_cast<std::int64_t>(walk.get(origin)))
        return false;
    if (dash) {
        // Land on the standable tile that gains the most ground on the hop route (holes are in blocked).
        auto tiles = (tiles_around(origin, static_cast<float>(static_cast<int>(ability_range(*dash)))) & hop_reached & ~blocked).positions();
        std::stable_sort(tiles.begin(), tiles.end(), [&](vec2 a, vec2 b) { return hop.get(a) < hop.get(b); });
        for (const auto tile : tiles) {
            // A dash that gains no more than a walking step would leave the next hole uncrossable for 8 turns.
            if (hop.get(tile) + 2 > via_hop) break;
            if (cast(*dash, tile)) {
                trace::moved("hop");
                acted = true;
                return true;
            }
        }
    }
    // Walk along the hop route; at the edge of the hole no walking step gains, so we wait there for the dash.
    const auto reachable = blocked.reachable_tiles(origin, static_cast<std::uint16_t>(me.base_stats().base_move_speed));
    auto step = origin;
    auto best = via_hop;
    for (const auto tile : (reachable & hop_reached).positions())
        if (hop.get(tile) < best) {
            step = tile;
            best = hop.get(tile);
        }
    if (step != origin && me.move_absolute(step)) {
        trace::moved("move");
        acted = true;
        return true;
    }
    // A ready dash that found no legal landing (our thin-hole test is looser than the engine's dash path) would
    // wait here forever (fracture: 55 turns on one tile), so the walking route takes over.
    if (dash) return false;
    LOG(trace::move_failed("hop route: wait at the hole edge for the dash"););
    return true;
}

}  // namespace

bool move_towards_closest(const bc::bitboard& goals_in, const bc::bitboard* avoid) {
    auto& me = bc::self();
    auto origin = me.position();
    if (!me.can_move()) {
        LOG(trace::move_failed("cannot move"););
        return false;
    }
    if (goals_in.is_set(origin)) {
        LOG(trace::move_failed("already on goal"););
        return false;
    }

    // Only free goals or occupied camp tiles can seed the field. Never mutate the caller's board.
    auto blocked = blocked_tiles();
    if (avoid) {
        blocked |= *avoid;
        blocked.set(origin, false);
    }
    const auto& raw = goals_in;
    // goals_in & (~blocked | camp tiles), built in place to skip the temporary boards.
    auto goals = ~blocked;
    goals |= bc::vision::camp_tiles();
    goals &= goals_in;
    if (goals.is_empty()) {
        // v90-log: the reason text (goal_cover, no_step_reason read boards) is built only for the log.
        LOG(trace::move_failed("no free goal; " + goal_cover(raw, blocked)););
        return false;
    }
    auto dash = ability_of(me, Kind::dash);
    // v98 (FIX_HOP): a dasher plans through holes even while its dash recharges, so it skips the engine's walk, but
    // only with a thin hole within HOP_NEAR: skipping it everywhere lost broken-curve (no holes) 0 of 6 against v97.
    bool dasher = false;
    if (FIX_HOP && dash) {
        auto near = tiles_around(origin, static_cast<float>(HOP_NEAR));
        near &= thin_holes();
        dasher = !near.is_empty();
    }
    if (dash && !dash->is_ready()) dash.reset();
    try {
        if (!dash && !dasher && !avoid && goals.count() == 1 && !blocked.is_set(goals.positions()[0])) {
            if (me.move_towards(goals.positions()[0])) {
                trace::moved("move");
                return true;
            }
            // A rejected step may have revealed a hidden unit in a bush (move_failed does this too, but only in the log).
            seen::reset();
            LOG(trace::move_failed("engine rejected move_towards"););
            return false;
        }

        // Expand from the goals until our current tile is reached.
        bc::distance_field field;
        bc::bitboard reached;
        auto walls = ~goals;
        walls &= blocked;
        field.compute_to(walls, goals, tiles_around(origin, 0), reached);

        if (dasher && !avoid) {
            bool acted = false;
            if (hop_route(me, origin, walls, goals, blocked, field, reached, dash, acted)) return true;
        }

        // Dash whenever it is ready (it leaves the walk free), then walk on from where it lands.
        const bool dashed = dash && dash_along(*dash, origin, field, reached & ~blocked);
        if (dashed) {
            trace::moved("dash");
            origin = me.position();
            blocked = avoid ? (blocked_tiles() | *avoid) : blocked_tiles();
            blocked.set(origin, false);
            if (goals.is_set(origin) || !me.can_move()) return true;
        }

        // Choose a reachable step with a smaller distance to the goal.
        const auto reachable = blocked.reachable_tiles(origin, static_cast<std::uint16_t>(me.base_stats().base_move_speed));
        auto step = origin;
        auto best = field.get(origin);
        for (const auto tile : (reachable & reached).positions()) {
            const auto steps = field.get(tile);
            if (steps < best) {
                step = tile;
                best = steps;
            }
        }
        if (step != origin) {
            if (me.move_absolute(step)) {
                trace::moved("move");
                return true;
            }
            if (dashed) return true;
            seen::reset();
            LOG(trace::move_failed("engine rejected move_absolute; " + no_step_reason(origin, &reached)););
            return false;
        }
        if (dashed) return true;
        LOG(trace::move_failed(no_step_reason(origin, &reached)););
        return false;
    } catch (const bc::error& error) {
        LOG(trace::move_failed(std::string("move error ") + error_text(error)););
        return false;
    }
}

bool dash_along(const bc::ability& dash, vec2 origin, const bc::distance_field& field, const bc::bitboard& landings) {
    // Land on the tile that gains the most ground towards the goal; any gain is worth the cooldown.
    const auto here = field.get(origin);
    auto tiles = (tiles_around(origin, static_cast<float>(static_cast<int>(ability_range(dash)))) & landings).positions();
    std::stable_sort(tiles.begin(), tiles.end(), [&](vec2 a, vec2 b) { return field.get(a) < field.get(b); });
    for (const auto tile : tiles) {
        if (field.get(tile) >= here) break;
        if (cast(dash, tile)) return true;
    }
    return false;
}

bool attack_if_in_range(const bc::unit& target) {
    auto& me = bc::self();
    try {
        if (!me.can_attack()) {
            LOG(trace::attack_failed("attack on cooldown"););
            return false;
        }
        if (!target.is_visible() || target.health() <= 0) {
            LOG(trace::attack_failed("target gone"););
            return false;
        }
        if (!me.attack_tiles().is_set(target.position())) {
            LOG(trace::attack_failed("out of attack tiles"););
            return false;
        }
        me.attack(target);
        trace::act("attack");
        return true;
    } catch (const bc::error& error) {
        LOG(trace::attack_failed(std::string("attack error ") + error_text(error)););
        return false;
    }
}

bool alive(const bc::unit& unit) {
    try {
        return unit.health() > 0;
    } catch (const bc::error&) {
        return false;
    }
}

bool within(const bc::champion& me, const bc::unit& unit, double radius) {
    return distance(me.position(), unit.position()) <= radius;
}

bool sighted(const Target& target) {
    if (const auto* champion = target.champion()) {
        bc::detail::champion_info info{};
        const auto handle = static_cast<std::uint32_t>(static_cast<bc::handle_champion_t>(*champion));
        return bc::detail::sdk_champion(handle, &info) == 0 && info.x >= 0 && info.y >= 0;
    }
    bc::detail::camp_state state{};
    const auto handle = static_cast<std::uint32_t>(static_cast<bc::handle_camp_t>(*target.camp()));
    return bc::detail::sdk_camp(handle, &state) == 0;
}

std::optional<bc::ability> ability_of(const bc::champion& me, Kind kind) {
    for (const auto& ability : me.abilities())
        if (ability.is(kind)) return ability;
    return std::nullopt;
}

double ability_range(const bc::ability& ability) {
    const auto& info = ability.info();
    if (!info) return 0;
    return std::visit([](const auto& payload) -> double {
        if constexpr (requires { payload.range; }) return payload.range;
        else return 0;
    }, *info);
}

double ability_radius(const bc::ability& ability) {
    const auto& info = ability.info();
    if (!info) return 0;
    return std::visit([](const auto& payload) -> double {
        if constexpr (requires { payload.radius; }) return payload.radius;
        else return 0;
    }, *info);
}

namespace {

template <class Use>
bool cast_with(const std::optional<bc::ability>& ability, Use use) {
    if (!ability) return false;
    bool used;
    try {
        if (!ability->is_ready()) return false;
        used = use(*ability);
    } catch (const bc::error&) {
        return false;
    }
    if (used)
        trace::act("cast");
    else
        // A rejected dash may have revealed a hidden unit in a bush.
        seen::reset();
    return used;
}

} // namespace

bool cast(const std::optional<bc::ability>& ability) {
    return cast_with(ability, [](const bc::ability& a) { return a.use(); });
}

bool cast(const std::optional<bc::ability>& ability, vec2 target) {
    return cast_with(ability, [&](const bc::ability& a) { return a.use(target); });
}

bool cast(const std::optional<bc::ability>& ability, const bc::champion& target) {
    return cast_with(ability, [&](const bc::ability& a) { return a.use(target); });
}

bool cast(const std::optional<bc::ability>& ability, const bc::camp& target) {
    return cast_with(ability, [&](const bc::ability& a) { return a.use(target); });
}

bool cast(const std::optional<bc::ability>& ability, const Target& target) {
    if (const auto* champion = target.champion()) return cast(ability, *champion);
    return cast(ability, *target.camp());
}

bool cast_self(const std::optional<bc::ability>& ability) {
    if (!ability || !ability->is_ready()) return false;
    for (int attempt = 0; attempt < 2; ++attempt) {
        bool used;
        try {
            used = attempt == 0 ? ability->use() : ability->use(bc::self());
        } catch (const bc::error&) {
            continue;
        }
        if (used) trace::act("cast");
        return used;
    }
    return false;
}

double spell_damage(const std::optional<bc::ability>& ability) {
    if (!ability) return 0.0;
    try {
        return ability->damage_amount();
    } catch (const std::exception&) {
        return 0.0;
    }
}

std::vector<vec2> spell_places(const Target& target, bool is_camp) {
    std::vector<vec2> places;
    std::set<vec2> taken;
    auto add = [&](const bc::unit& unit) {
        try {
            const auto place = unit.position();
            if (taken.insert(place).second) places.push_back(place);
        } catch (const bc::error&) {
        }
    };
    if (is_camp)
        for (const auto& camp : seen::camps()) add(camp);
    else
        for (const auto& enemy : seen::champions(Team::enemy)) add(enemy);
    try {
        const auto place = target.position();
        if (!taken.count(place)) places.push_back(place);
    } catch (const bc::error&) {
    }
    return places;
}

namespace {

std::vector<vec2> aoe_centers(vec2 origin, const std::vector<vec2>& places, int reach, int radius) {
    // A body inside the cast can be the centre. One just past it needs a tile in between.
    std::vector<vec2> centers;
    std::set<vec2> taken;
    for (const auto place : places) {
        const int gap = distance(origin, place);
        std::vector<vec2> tiles;
        if (gap <= reach)
            tiles = {place};
        else if (gap <= reach + radius)
            tiles = (tiles_around(place, static_cast<float>(radius)) & tiles_around(origin, static_cast<float>(reach))).positions();
        else
            continue;
        for (const auto tile : tiles)
            if (taken.insert(tile).second) centers.push_back(tile);
    }
    return centers;
}

} // namespace

std::pair<std::optional<vec2>, int> aoe_aim(vec2 origin, const std::vector<vec2>& places, double reach,
                                            double radius, std::optional<vec2> focus) {
    if (places.empty()) return {std::nullopt, 0};
    std::optional<vec2> best;
    std::tuple<int, bool, int> best_key{};
    for (const auto center : aoe_centers(origin, places, static_cast<int>(reach), static_cast<int>(radius))) {
        int hits = 0;
        for (const auto place : places)
            if (distance(center, place) <= radius) ++hits;
        if (hits <= 0) continue;
        const bool covers = focus && distance(center, *focus) <= radius;
        const std::tuple<int, bool, int> key{hits, covers, -distance(origin, center)};
        if (!best || key > best_key) {
            best = center;
            best_key = key;
        }
    }
    return {best, best ? std::get<0>(best_key) : 0};
}

bool cast_aoe(const std::optional<bc::ability>& ability, const std::vector<vec2>& places,
              std::optional<vec2> focus, int require) {
    if (!ability || !ability->is_ready()) return false;
    std::pair<std::optional<vec2>, int> aim;
    try {
        if (!ability->is_aoe_damage()) return false;
        const auto info = ability->get<bc::ability_aoe_damage>();
        aim = aoe_aim(bc::self().position(), places, info.range, info.radius, focus);
    } catch (const std::exception&) {
        return false;
    }
    if (!aim.first || aim.second < require) return false;
    return cast(ability, *aim.first);
}

bool channelling(const bc::champion& champion) {
    try {
        return champion.is_recalling() || champion.is_submitting_gems();
    } catch (const bc::error&) {
        return false;
    }
}

std::string channel_left(const bc::champion& me) {
    try {
        return format("left %d", me.remaining_channel_time());
    } catch (const bc::error&) {
        return "left unread";
    }
}

ChannelThreat channel_threat(const bc::champion& me) {
    // A basic move is one tile. An enemy already in range is hitting now.
    // One step out only matters when the stand outlasts next turn: a finish then
    // is gone by the time they arrive. The text is the reason for staying or aborting.
    std::optional<int> left;
    bool finishes_next_turn = false;
    try {
        left = me.remaining_channel_time();
        finishes_next_turn = *left <= 1;
    } catch (const bc::error&) {
    }
    const auto origin = me.position();
    struct Nearest {
        int gap, reach;
        bc::champion enemy;
    };
    std::optional<Nearest> nearest;
    // v82 (RECALL_SPELL_CAUTION): any damage breaks a recall, so a caster counts with its spell reach; a tribute
    // only breaks on a stun or death, and keeps the basic attack range.
    const bool recalling = RECALL_SPELL_CAUTION && me.is_recalling();
    for (const auto& enemy : seen::champions(Team::enemy)) {
        int reach, gap;
        try {
            reach = enemy.attack_range();
            if (recalling) reach = std::max(reach, recall_reach(enemy));
            gap = distance(origin, enemy.position()) - reach;
        } catch (const bc::error&) {
            continue;
        }
        if (!nearest || gap < nearest->gap) nearest = Nearest{gap, reach, enemy};
        if (gap <= 0 || (gap <= 1 && !finishes_next_turn)) {
            ChannelThreat result{true, {}};
            LOG(result.why = format(gap <= 0 ? "enemy at %s can hit us now" : "enemy at %s one step from hitting us",
                                    at(enemy.position()).c_str()););
            return result;
        }
    }
    ChannelThreat result{false, {}};
    LOG({
        const std::string ends = left ? format(", ends in %d", *left) : "";
        if (!nearest)
            result.why = "no enemy in sight" + ends;
        else if (nearest->gap <= 1 && finishes_next_turn)
            result.why = "enemy at " + at(nearest->enemy.position()) + " arrives after it ends";
        else
            result.why = format("nearest enemy needs %d steps", nearest->gap) + ends;
    });
    return result;
}

bool stop_channel(const bc::champion& me) {
    // Recall and tribute cancel. A teleport runs to the end; the engine rejects a cancel.
    try {
        if (me.is_recalling())
            me.cancel_recall();
        else if (me.is_submitting_gems())
            me.cancel_submit_gems();
        else
            return false;
    } catch (const bc::error&) {
        return false;
    }
    return true;
}

bool buy(const bc::champion& me, Item item, int count) {
    if (count <= 0) return false;
    try {
        if (me.can_buy(item, static_cast<std::uint32_t>(count))) {
            me.buy(item, static_cast<std::uint32_t>(count));
            trace::act("buy");
            return true;
        }
    } catch (const bc::error&) {
    }
    return false;
}

bool dash_towards(const bc::champion& me, vec2 target_position, int reach) {
    const auto dash = ability_of(me, Kind::dash);
    if (!dash || !dash->is_ready()) return false;
    const auto origin = me.position();
    const int gap = distance(origin, target_position);
    auto free = (tiles_around(origin, static_cast<float>(static_cast<int>(ability_range(*dash)))) & ~blocked_tiles()).positions();
    std::stable_sort(free.begin(), free.end(), [&](vec2 a, vec2 b) {
        return distance(a, target_position) < distance(b, target_position);
    });
    for (const auto tile : free) {
        const int landing_gap = distance(tile, target_position);
        if (landing_gap > reach + 1 || landing_gap >= gap) break;
        if (cast(dash, tile)) return true;
    }
    return false;
}
