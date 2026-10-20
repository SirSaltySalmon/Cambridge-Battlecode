#include "actions.h"
#include "board.h"
#include "seen.h"
#include "state.h"
#include "trace.h"
#include <algorithm>
#include <set>
#include <tuple>

// v83 (FIX_HIDDEN_STEP): the last tile a step to was refused by an unseen unit, and when; it is avoided for a few rounds.
vec2 hidden_block_tile{-1, -1};
int hidden_block_round = -100;

bool tp_carried = false;

bool move_towards_closest(const bc::bitboard& goals_in, const bc::bitboard* avoid) {
    auto& me = bc::self();
    auto origin = me.position();
    if (!me.can_move()) {
        trace::move_failed("cannot move");
        return false;
    }
    if (goals_in.is_set(origin)) {
        trace::move_failed("already on goal");
        return false;
    }

    // Only free goals or occupied camp tiles can seed the field. Never mutate the caller's board.
    auto blocked = blocked_tiles();
    if (avoid) {
        blocked |= *avoid;
        blocked.set(origin, false);
    }
    if (FIX_HIDDEN_STEP && static_cast<int>(bc::round_number()) - hidden_block_round <= HIDDEN_STEP_MEMORY
        && hidden_block_tile != origin && distance(origin, hidden_block_tile) <= 2)
        blocked.set(hidden_block_tile, true);
    const auto& raw = goals_in;
    // goals_in & (~blocked | camp tiles), built in place to skip the temporary boards.
    auto goals = ~blocked;
    goals |= bc::vision::camp_tiles();
    goals &= goals_in;
    if (goals.is_empty()) {
        trace::move_failed(LOG_TEXT ? "no free goal; " + goal_cover(raw, blocked) : std::string("no free goal"));
        return false;
    }
    auto dash = ability_of(me, Kind::dash);
    if (dash && !dash->is_ready()) dash.reset();
    try {
        if (!dash && !avoid && goals.count() == 1 && !blocked.is_set(goals.positions()[0])) {
            if (me.move_towards(goals.positions()[0])) {
                trace::moved("move");
                return true;
            }
            trace::move_failed("engine rejected move_towards");
            return false;
        }

        // Expand from the goals until our current tile is reached.
        bc::distance_field field;
        bc::bitboard reached;
        auto walls = ~goals;
        walls &= blocked;
        field.compute_to(walls, goals, tiles_around(origin, 0), reached);

        // v83 (TP_CRYSTAL): a crystal jump over terrain when it saves at least TP_MIN_SAVE steps past its channel.
        if (TP_CRYSTAL && tp_carried && reached.is_set(origin) && seen::champions(Team::enemy).empty()) {
            const auto here = field.get(origin);
            if (here > TP_MIN_SAVE + TP_CHANNEL) {
                auto landings = tiles_around(origin, static_cast<float>(TP_RANGE));
                landings &= reached;
                landings &= ~blocked;
                std::optional<vec2> jump;
                auto jump_steps = here;
                for (const auto tile : landings.positions()) {
                    const auto steps = field.get(tile);
                    if (steps < jump_steps) {
                        jump = tile;
                        jump_steps = steps;
                    }
                }
                // Walking there would take at least its Chebyshev distance; the jump must beat walking by TP_MIN_SAVE.
                if (jump && static_cast<int>(here) - static_cast<int>(jump_steps) >= TP_MIN_SAVE + TP_CHANNEL) {
                    try {
                        if (me.consumables()[bc::consumable_type::tp_crystal].use(*jump)) {
                            tp_carried = false;
                            trace::act("teleport");
                            say(format("round %u: champion %d teleports (%d,%d) -> (%d,%d), %u -> %u steps",
                                       bc::round_number(), static_cast<int>(me.champion_name()), origin.x, origin.y,
                                       jump->x, jump->y, here, jump_steps));
                            return true;
                        }
                    } catch (const bc::error&) {
                        tp_carried = false;
                    }
                }
            }
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
            // v83 (FIX_HIDDEN_STEP): an unseen unit (an enemy in a bush) stands there; the refusal reveals it and keeps
            // our move, so try the next best step instead of standing still (v82 lost up to 10 turns in a row this way).
            if (FIX_HIDDEN_STEP) {
                hidden_block_tile = step;
                hidden_block_round = static_cast<int>(bc::round_number());
                for (int attempt = 0; attempt < 2; ++attempt) {
                    auto tried = step;
                    step = origin;
                    best = field.get(origin);
                    for (const auto tile : (reachable & reached).positions()) {
                        if (tile == tried || tile == hidden_block_tile) continue;
                        const auto steps = field.get(tile);
                        if (steps < best || (steps == best && step == origin && steps <= field.get(origin))) {
                            step = tile;
                            best = steps;
                        }
                    }
                    if (step == origin) break;
                    if (me.move_absolute(step)) {
                        trace::moved("move");
                        return true;
                    }
                    hidden_block_tile = step;
                }
            }
            if (dashed) return true;
            trace::move_failed(std::string("engine rejected move_absolute; ") + (LOG_TEXT ? no_step_reason(origin, &reached) : std::string()));
            return false;
        }
        if (dashed) return true;
        trace::move_failed(LOG_TEXT ? no_step_reason(origin, &reached) : std::string("no step"));
        return false;
    } catch (const bc::error& error) {
        trace::move_failed(std::string("move error ") + error_text(error));
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
            trace::attack_failed("attack on cooldown");
            return false;
        }
        if (!target.is_visible() || target.health() <= 0) {
            trace::attack_failed("target gone");
            return false;
        }
        if (!me.attack_tiles().is_set(target.position())) {
            trace::attack_failed("out of attack tiles");
            return false;
        }
        me.attack(target);
        trace::act("attack");
        return true;
    } catch (const bc::error& error) {
        trace::attack_failed(std::string("attack error ") + error_text(error));
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

namespace {

std::string enemy_label(const bc::champion& enemy) {
    try {
        return format("c%d", champ_id(enemy.champion_name())) + at(enemy.position());
    } catch (const bc::error&) {
        try {
            return at(enemy.position());
        } catch (const bc::error&) {
            return "enemy";
        }
    }
}

} // namespace

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
    const std::string left_text = left ? format("left %d", *left) : "left unread";
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
            const auto who = enemy_label(enemy);
            if (gap <= 0)
                return {true, format("%s already in range, gap %d attack %d, %s", who.c_str(), gap, reach, left_text.c_str())};
            return {true, format("%s one step out, gap %d attack %d, %s", who.c_str(), gap, reach, left_text.c_str())};
        }
    }
    if (!nearest) return {false, "no visible enemy, " + left_text};
    const auto who = enemy_label(nearest->enemy);
    if (nearest->gap <= 1 && finishes_next_turn)
        return {false, format("%s gap %d attack %d arrives after finish, %s", who.c_str(), nearest->gap,
                              nearest->reach, left_text.c_str())};
    return {false, format("nearest %s gap %d attack %d needs more than a step, %s", who.c_str(), nearest->gap,
                          nearest->reach, left_text.c_str())};
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
