// Movement, attacks, casts, channels, and purchases.
#pragma once
#include "constants.h"
#include <functional>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

// A champion or a camp: the target of an attack or a unit-targeted cast.
struct Target {
    std::variant<bc::champion, bc::camp> value;
    Target(const bc::champion& champion) : value{champion} {}
    Target(const bc::camp& camp) : value{camp} {}
    const bc::unit& unit() const {
        return std::visit([](const auto& u) -> const bc::unit& { return u; }, value);
    }
    const bc::champion* champion() const { return std::get_if<bc::champion>(&value); }
    const bc::camp* camp() const { return std::get_if<bc::camp>(&value); }
    vec2 position() const { return unit().position(); }
    float health() const { return unit().health(); }
    float max_health() const { return unit().max_health(); }
    float strength() const { return unit().strength(); }
    int attack_range() const { return unit().attack_range(); }
};

// v133a (GO): the one mover. Every walk, dash and hop goes through go(); callers only pick the goals and how the dash
// may be spent. See GO_* in constants.h.
//   keep:   never dash.
//   route:  dash (or hop through thin holes) whenever it gains ground, except with an enemy in sight under the keep
//           rules (VESPER_KEEP_DASH, JOIN_KEEP_DASH).
//   finish: first a dash that lands within `finish` steps of the goals (or the caller's `rank`); else as route.
//   urgent: as route, but the keep rules never hold it back (getting away beats keeping it).
enum class DashUse { keep, route, finish, urgent };
struct Go {
    const bc::bitboard* avoid = nullptr;  // hard keep-out on top of blocked tiles and the walk guard
    DashUse dash = DashUse::route;
    int finish = 1;
    bool walk = true;                     // false: dash only (Vesper keeps the walk's attack allowance)
    bool walk_after_dash = true;          // false: a finish or ranked dash ends the move (Vesper fires instead)
    bool guard_dash = true;               // the walk guard also bars dash landings
    std::optional<vec2> aim;              // finish: among equal path steps, land nearest this tile
    // A ranking of the caller's own for the dash landing, tried before the path one: higher first, nullopt = never.
    std::function<std::optional<long>(vec2)> rank;
};
bool go(const bc::bitboard& goals, const Go& opt = {});
inline bool move_towards_closest(const bc::bitboard& goals, const bc::bitboard* avoid = nullptr) {
    Go opt;
    opt.avoid = avoid;
    return go(goals, opt);
}
// The dash use of a getaway (retreat, camp exits, the hurt dodge): urgent with GO_URGENT_DASH, else route.
inline DashUse getaway() { return GO_URGENT_DASH ? DashUse::urgent : DashUse::route; }
// The closing dash on `aim`: a landing within `reach + extra` of it (a straight line; GO_FINISH_PATH: within `extra`
// path steps of the goals), when walking alone does not get there this turn.
void close_in(Go& opt, const bc::champion& me, vec2 aim, int reach, int extra);
inline bool get_away(const bc::bitboard& goals, const bc::bitboard* avoid = nullptr) {
    Go opt;
    opt.avoid = avoid;
    opt.dash = getaway();
    return go(goals, opt);
}
// v133 (GO_EXIT_DASH): out of awake camps' reach as fast as we can: a getaway that dashes even with the walk spent.
// Without GO_EXIT_DASH, the walk it always was.
inline bool leave_camps(const bc::bitboard& goals, const bc::bitboard* avoid = nullptr) {
    if (!GO_EXIT_DASH) return move_towards_closest(goals, avoid);
    Go opt;
    opt.avoid = avoid;
    opt.dash = getaway();
    opt.walk = bc::self().can_move();
    return go(goals, opt);
}
// v133 (GO_EXIT_DASH): steps a camp exit reaches this turn: the walk, plus a ready dash's range.
int exit_turn_steps(const bc::champion& me);
// v132f/g (WALK_GUARD): awake camp reach every walk and dash keeps out of this turn (set by Bot::build_walk_guard).
extern const bc::bitboard* walk_guard;
// v134-move (HOR_DASH_SAFE): tiles a movement dash never lands on this turn (Heart of Ruin: beside camps), or nullptr.
extern const bc::bitboard* dash_keep_off;
// Make one basic attack if the target is already in attack range.
bool attack_if_in_range(const bc::unit& target);
inline bool attack_if_in_range(const Target& target) { return attack_if_in_range(target.unit()); }

// Whether a unit is visible and still has health.
bool alive(const bc::unit& unit);
inline bool alive(const Target& target) { return alive(target.unit()); }

template <class Unit>
std::vector<Unit> alive(const std::vector<Unit>& units) {
    std::vector<Unit> living;
    for (const auto& unit : units)
        if (alive(unit)) living.push_back(unit);
    return living;
}

// Lowest health, first one on ties.
template <class Unit>
std::optional<Unit> lowest(const std::vector<Unit>& units) {
    std::optional<Unit> best;
    float best_health = 0;
    for (const auto& unit : units) {
        const float health = unit.health();
        if (!best || health < best_health) {
            best = unit;
            best_health = health;
        }
    }
    return best;
}

bool within(const bc::champion& me, const bc::unit& unit, double radius);
// Whether a target is still in vision, read without an exception (one raw engine read, ~20-65 fuel).
// After our own move or dash a target may have dropped out of sight, and position() would throw.
bool sighted(const Target& target);
inline bool within(const bc::champion& me, const Target& target, double radius) { return within(me, target.unit(), radius); }

std::optional<bc::ability> ability_of(const bc::champion& me, Kind kind);
// v98 (FIX_HOP): hole tiles a two-tile dash crosses (standable ground on both opposite sides).
const bc::bitboard& thin_holes();
// v103 (FIX_POCKET): the tiles our spawn reaches on foot (the main island). Off it, a champion is in a hole pocket.
const bc::bitboard& home_island();
// v103 (FIX_POCKET): steps from origin to the goals with thin holes crossable (the hop route), ignoring units and the
// dash cooldown; nullopt when even hopping cannot get there.
std::optional<int> hop_steps(vec2 origin, const bc::bitboard& goals);
// Cast range and area radius of an ability's payload (0 when the payload has none).
double ability_range(const bc::ability& ability);
double ability_radius(const bc::ability& ability);

bool cast(const std::optional<bc::ability>& ability);
bool cast(const std::optional<bc::ability>& ability, vec2 target);
bool cast(const std::optional<bc::ability>& ability, const bc::champion& target);
bool cast(const std::optional<bc::ability>& ability, const bc::camp& target);
bool cast(const std::optional<bc::ability>& ability, const Target& target);
// Self-cast is untargeted in the SDK; fall back to targeting ourselves.
bool cast_self(const std::optional<bc::ability>& ability);
double spell_damage(const std::optional<bc::ability>& ability);

// Tiles an area spell can cover: the camp group, or the enemy champions, plus the focus.
std::vector<vec2> spell_places(const Target& target, bool is_camp);
// More bodies first, then a centre that still covers the chosen target. v120: no centre within radius of `clear`.
std::pair<std::optional<vec2>, int> aoe_aim(vec2 origin, const std::vector<vec2>& places, double reach,
                                            double radius, std::optional<vec2> focus,
                                            const std::vector<vec2>* clear = nullptr);
bool cast_aoe(const std::optional<bc::ability>& ability, const std::vector<vec2>& places,
              std::optional<vec2> focus, int require, const std::vector<vec2>* clear = nullptr);

bool channelling(const bc::champion& champion);
std::string channel_left(const bc::champion& me);
struct ChannelThreat {
    bool threat;
    std::string why;
};
ChannelThreat channel_threat(const bc::champion& me);
bool stop_channel(const bc::champion& me);
bool buy(const bc::champion& me, Item item, int count);

