#include "actions.h"
#include "board.h"
#include "crystal.h"
#include "seen.h"
#include "state.h"
#include "trace.h"
#include <algorithm>
#include <array>
#include <set>
#include <tuple>

const bc::bitboard& thin_holes() {
    // Hole tiles with standable terrain on both opposite sides (horizontally, vertically or diagonally): a dash of
    // two tiles crosses them. Built once from the static map.
    // v99: whole-board shifts (a few dozen engine calls) instead of a loop over every hole: on islands the loop took
    // the dashers' first move over four rounds of fuel. open shifted by +d and by -d marks the tiles whose -d and +d
    // neighbours are standable; tiles shifted off the map are dropped, so the map edge counts as unstandable.
    static const bc::bitboard board = [] {
        const auto open = ~(bc::map::walls() | bc::map::holes() | bc::map::altars());
        auto shifted = [&](int dx, int dy) {
            bc::bitboard out = dy == 0 ? open : open.shift_up(dy);
            return dx == 0 ? out : out.shift_right(dx);
        };
        bc::bitboard any;
        constexpr int dirs[4][2] = {{1, 0}, {0, 1}, {1, 1}, {1, -1}};
        for (const auto& d : dirs) {
            auto both = shifted(d[0], d[1]);
            both &= shifted(-d[0], -d[1]);
            any |= both;
        }
        any &= bc::map::holes();
        return any;
    }();
    return board;
}

namespace {

// v99: boards and fields reused across calls. A distance_field costs ~1,550 fuel to construct (an allocation the size
// of the map, zeroed) and a bitboard ~210-300; compute_to() and the *_into helpers overwrite every entry (checked:
// unreached tiles come back as unreachable whatever the field held), so reuse reads exactly what fresh ones would.
// Movement never calls itself, so one set of each is enough.
struct MoveScratch {
    bc::distance_field field, hop, few;
    bc::bitboard blocked{bc::uninitialized}, goals{bc::uninitialized}, walls{bc::uninitialized}, root{bc::uninitialized},
        reached{bc::uninitialized}, near{bc::uninitialized}, reachable{bc::uninitialized}, landing{bc::uninitialized},
        part{bc::uninitialized}, hop_walls{bc::uninitialized}, hop_reached{bc::uninitialized}, few_reached{bc::uninitialized},
        walk2{bc::uninitialized};
};
MoveScratch& scratch() {
    static MoveScratch s;
    return s;
}

// This champion's walking range a turn: base stats never change, and base_stats() is ~220 fuel.
std::uint16_t move_speed(const bc::champion& me) {
    static const auto speed = static_cast<std::uint16_t>(me.base_stats().base_move_speed);
    return speed;
}

// The tiles of a small board in engine order, into a stack buffer (positions() allocates a vector).
using Tiles = std::array<vec2, 64>;
std::size_t tiles_of(const bc::bitboard& board, Tiles& out) {
    return std::min<std::size_t>(board.positions_into(out), out.size());
}

// Tiles within reach of origin that are also in allowed, best field value first (ties in engine order, as the stable
// sort over positions() gave).
std::size_t landings_by_field(vec2 origin, int reach, const bc::bitboard& allowed, const bc::distance_field& field,
                              Tiles& out) {
    auto& s = scratch();
    fast::around_into(origin, static_cast<float>(reach), s.landing);
    s.landing &= allowed;
    if (land_bar) s.landing &= ~*land_bar;
    const auto n = tiles_of(s.landing, out);
    std::stable_sort(out.begin(), out.begin() + static_cast<std::ptrdiff_t>(n),
                     [&](vec2 a, vec2 b) { return field.get(a) < field.get(b); });
    return n;
}

// The 8-neighbour dilation of in, into out (tmp and h are scratch).
void dilate_into(const bc::bitboard& in, bc::bitboard& tmp, bc::bitboard& h, bc::bitboard& out) {
    fast::copy_into(in, h);
    in.shift_right_into(1, tmp);
    h |= tmp;
    in.shift_right_into(-1, tmp);
    h |= tmp;
    fast::copy_into(h, out);
    h.shift_up_into(1, tmp);
    out |= tmp;
    h.shift_up_into(-1, tmp);
    out |= tmp;
}

// v103 (FIX_HOP_FEW): the thin holes on fewest-hop routes to the goals. Layer 0 is what walks to the goals; layer k
// adds the ground one more hop away (across a thin hole touching layer k-1, then walking on), and only the holes that
// step down a layer are kept. Static terrain only, so it is kept per goal board (HOP_CACHE of them) and rebuilt only
// for new goals, from HOP_LAYER_FUEL spare fuel; without it every thin hole counts, as before.
struct HopLayers {
    bc::bitboard goals, holes;
};
const bc::bitboard& few_hop_holes(const bc::bitboard& goals) {
    static std::vector<HopLayers> cache;
    for (const auto& c : cache)
        if (c.goals == goals) return c.holes;
    if (bc::fuel_remaining() < HOP_FUEL + HOP_LAYER_FUEL) return thin_holes();
    static bc::distance_field flood;
    static bc::bitboard reach{bc::uninitialized}, part{bc::uninitialized}, edge{bc::uninitialized},
        land{bc::uninitialized}, tmp{bc::uninitialized}, h{bc::uninitialized};
    static const bc::bitboard open = ~seen::fixed_blocks();
    const auto& walls = seen::fixed_blocks();
    HopLayers entry{goals, {}};
    fast::and_into(goals, open, part);
    flood.compute(walls, part, reach);
    for (int k = 0; k < HOP_LAYERS; ++k) {
        dilate_into(reach, tmp, h, edge);
        edge &= thin_holes();
        dilate_into(edge, tmp, h, land);
        land &= open;
        fast::not_into(reach, tmp);
        land &= tmp;
        if (land.is_empty()) break;
        // Only holes beside the new ground: one with both sides already in the layer would let a route hop twice.
        dilate_into(land, tmp, h, part);
        edge &= part;
        entry.holes |= edge;
        flood.compute(walls, land, part);
        reach |= part;
    }
    if (cache.size() >= HOP_CACHE) cache.erase(cache.begin());
    cache.push_back(std::move(entry));
    return cache.back().holes;
}

// v137 (HOP_STICKY): the last round a hop route was taken. The planner only wakes with a thin hole within HOP_NEAR,
// so a route through a farther hole was taken on one tile and dropped on the next (02tangle: 400 bounces).
int hop_round = -10;
bool hop_sticky() { return HOP_STICKY && hop_round >= static_cast<int>(bc::round_number()) - 1; }

// v98 (FIX_HOP): a dasher's route through thin holes when it beats walking by HOP_SAVE steps (or walking cannot
// reach the goal). Dashes across when ready, else walks toward the edge or waits there. Returns whether the hop
// route was taken: the turn's movement is then decided, waiting included, so the caller's branch keeps the turn
// (a wait that returned false let the next branch walk us off the edge, and the dash then hopped us back).
bool hop_route(const bc::champion& me, vec2 origin, const bc::bitboard& walls, const bc::bitboard& goals,
               const bc::bitboard& raw_goals, const bc::bitboard& blocked, const bc::distance_field& walk, const bc::bitboard& walk_reached,
               std::optional<bc::ability> dash, bool& acted) {
    auto& s = scratch();
    const auto& thin = thin_holes();
    if (thin.is_empty() || bc::fuel_remaining() < HOP_FUEL) return false;
    // Board-level checks only: looping over the goal tiles (thousands on some boards) ran 10x more turns out of fuel.
    if (walk_reached.is_set(origin) && static_cast<int>(walk.get(origin)) < GO_HOP_SAVE + 2) return false;
    fast::around_into(origin, static_cast<float>(HOP_NEAR), s.near);
    s.near &= thin;
    // v135 (HOP_WHEN_CUT): with no walking path at all, the crossing may be anywhere on the route.
    if (s.near.is_empty() && !(HOP_WHEN_CUT && !walk_reached.is_set(origin)) && !hop_sticky()) return false;
    static const bc::bitboard not_thin = ~thin_holes();
    fast::and_into(walls, not_thin, s.hop_walls);
    s.hop.compute_to(s.hop_walls, goals, s.root, s.hop_reached);
    if (!s.hop_reached.is_set(origin)) return false;
    const bc::distance_field* hop = &s.hop;
    const bc::bitboard* hop_reached = &s.hop_reached;
    const bc::bitboard* holes = &thin;
    auto via_hop = s.hop.get(origin);
    // v103 (FIX_HOP_FEW): each hop after the first waits out the dash's cooldown, so the fewest-hop route is taken
    // unless the any-hop one saves more than HOP_EXTRA steps (fracture/islands: 2-3 hops where 1 would do, then 6-8
    // turns idle at the second hole's edge).
    if (FIX_HOP_FEW) {
        const auto& few = few_hop_holes(raw_goals);
        if (!(few == thin)) {
            fast::not_into(few, s.part);
            fast::and_into(walls, s.part, s.hop_walls);
            s.few.compute_to(s.hop_walls, goals, s.root, s.few_reached);
            if (s.few_reached.is_set(origin) && s.few.get(origin) <= via_hop + HOP_EXTRA) {
                hop = &s.few;
                hop_reached = &s.few_reached;
                holes = &few;
                via_hop = s.few.get(origin);
            }
        }
    }
    // A recharging dash means waiting at the edge: its remaining cooldown counts against the hop (fracture: 217
    // edge-wait turns, and the hop variant lost a map v97 won 4 of 4).
    std::int64_t recharge = 0;
    if (const auto any = ability_of(me, Kind::dash)) recharge = std::max<std::int64_t>(0, any->cooldown());
    if (walk_reached.is_set(origin)
        && static_cast<std::int64_t>(via_hop) + recharge + GO_HOP_SAVE > static_cast<std::int64_t>(walk.get(origin)))
        return false;
    hop_round = static_cast<int>(bc::round_number());
    if (dash) {
        // Land on the standable tile that gains the most ground on the hop route (holes are in blocked).
        fast::not_into(blocked, s.part);
        s.part &= (*hop_reached);
        Tiles tiles;
        const auto n = landings_by_field(origin, static_cast<int>(ability_range(*dash)), s.part, *hop, tiles);
        if (FIX_HOP_FEW) blocked.reachable_tiles_into(origin, 2, s.walk2);
        for (std::size_t i = 0; i < n; ++i) {
            const auto tile = tiles[i];
            // A dash that gains no more than a walking step would leave the next hole uncrossable for 8 turns.
            if (hop->get(tile) + 2 > via_hop) break;
            // v103 (FIX_HOP_FEW): nor does one over plain ground (two walking steps reach it) with a route hole within
            // HOP_KEEP of the landing: the cooldown would still run at that hole's edge (islands: dash, then 6-8 idle).
            if (FIX_HOP_FEW && s.walk2.is_set(tile)) {
                fast::around_into(tile, static_cast<float>(HOP_KEEP), s.landing);
                s.landing &= *holes;
                if (!s.landing.is_empty()) continue;
            }
            // v101: Heart of Ruin's landing hits radius one, which would wake a camp beside the tile.
            if (me.champion_name() == Champ::heart_of_ruin) {
                bool beside = false;
                for (const auto& c : seen::camps())
                    if (distance(tile, c.position()) <= 1) beside = true;
                if (beside) continue;
            }
            if (cast(*dash, tile)) {
                trace::moved("hop");
                acted = true;
                return true;
            }
        }
    }
    // Walk along the hop route; at the edge of the hole no walking step gains, so we wait there for the dash.
    blocked.reachable_tiles_into(origin, move_speed(me), s.reachable);
    s.reachable &= (*hop_reached);
    Tiles tiles;
    const auto n = tiles_of(s.reachable, tiles);
    auto step = origin;
    auto best = via_hop;
    for (std::size_t i = 0; i < n; ++i)
        if (hop->get(tiles[i]) < best) {
            step = tiles[i];
            best = hop->get(tiles[i]);
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

const bc::bitboard& home_island() {
    // Static terrain, so one flood from a spawn tile serves the whole game.
    static const bc::bitboard board = [] {
        bc::bitboard out{bc::uninitialized};
        seen::fixed_blocks().reachable_tiles_into(bc::map::spawn_region(Team::ally).positions().front(), 0xFFFF, out);
        return out;
    }();
    return board;
}

std::optional<int> hop_steps(vec2 origin, const bc::bitboard& goals) {
    // Reuses the hop scratch: movement never runs inside this call.
    auto& s = scratch();
    static const bc::bitboard hop_walls = seen::fixed_blocks() & ~thin_holes();
    fast::around_into(origin, 0, s.root);
    s.hop.compute_to(hop_walls, goals, s.root, s.hop_reached);
    const auto steps = s.hop.get(origin);
    if (steps == bc::distance_field::unreachable) return std::nullopt;
    return static_cast<int>(steps);
}

const bc::bitboard* walk_guard = nullptr;
const bc::bitboard* land_bar = nullptr;

namespace {

// Land a ready dash on the free tile in its range that gains the most ground on `field` (any gain is worth the
// cooldown), and none farther than `most` from the goals; `landings` are the tiles allowed.
bool dash_along(const bc::ability& dash, vec2 origin, const bc::distance_field& field, const bc::bitboard& landings,
                int most = 1 << 20, std::optional<vec2> aim = std::nullopt, std::optional<vec2> align = std::nullopt) {
    const auto here = field.get(origin);
    Tiles tiles;
    const auto n = landings_by_field(origin, static_cast<int>(ability_range(dash)), landings, field, tiles);
    if (aim)
        std::stable_sort(tiles.begin(), tiles.begin() + static_cast<std::ptrdiff_t>(n), [&](vec2 a, vec2 b) {
            const auto fa = field.get(a), fb = field.get(b);
            return fa != fb ? fa < fb : distance(a, *aim) < distance(b, *aim);
        });
    // v138 (CHASE_STEP): equal landings go to the one nearest `align`, Chebyshev first, then the other axis.
    if (align) {
        auto off = [&](vec2 t) {
            const long dx = t.x - align->x, dy = t.y - align->y;
            return distance(t, *align) * 4096L + dx * dx + dy * dy;
        };
        std::stable_sort(tiles.begin(), tiles.begin() + static_cast<std::ptrdiff_t>(n), [&](vec2 a, vec2 b) {
            const auto fa = field.get(a), fb = field.get(b);
            return fa != fb ? fa < fb : off(a) < off(b);
        });
    }
    for (std::size_t i = 0; i < n; ++i) {
        if (field.get(tiles[i]) >= here || static_cast<int>(field.get(tiles[i])) > most) break;
        if (cast(dash, tiles[i])) return true;
    }
    return false;
}

// The caller's ranking: the free landings in the dash's range, best first (a hidden unit refuses one; the next goes).
bool dash_ranked(const bc::ability& dash, vec2 origin, const Go& opt) {
    auto& s = scratch();
    fast::around_into(origin, static_cast<float>(static_cast<int>(ability_range(dash))), s.landing);
    blocked_tiles_into(s.part);
    if (opt.avoid) s.part |= *opt.avoid;
    if (opt.guard_dash && walk_guard && !walk_guard->is_set(origin)) s.part |= *walk_guard;
    s.part.set(origin, true);
    fast::not_into(s.part, s.walk2);
    s.landing &= s.walk2;
    if (land_bar) s.landing &= ~*land_bar;
    Tiles tiles;
    const auto n = tiles_of(s.landing, tiles);
    std::array<std::pair<long, vec2>, 64> ranked;
    std::size_t m = 0;
    for (std::size_t i = 0; i < n; ++i)
        if (const auto r = opt.rank(tiles[i])) ranked[m++] = {*r, tiles[i]};
    std::stable_sort(ranked.begin(), ranked.begin() + static_cast<std::ptrdiff_t>(m),
                     [](const auto& a, const auto& b) { return a.first > b.first; });
    for (std::size_t i = 0; i < m; ++i)
        if (cast(dash, ranked[i].second)) return true;
    return false;
}

}  // namespace

int exit_turn_steps(const bc::champion& me) {
    int steps = me.can_move() ? 1 : 0;
    if (GO_EXIT_DASH)
        if (const auto dash = ability_of(me, Kind::dash); dash && dash->is_ready()) steps += static_cast<int>(ability_range(*dash));
    return steps;
}

void close_in(Go& opt, const bc::champion& me, vec2 aim, int reach, int extra, bool align) {
    if (align) opt.align = aim;
    const int most = reach + extra;
    const int gap = distance(me.position(), aim);
    if (gap <= most) return;
    opt.dash = DashUse::finish;
    opt.finish = extra;
    opt.aim = aim;
    if (GO_FINISH_PATH) return;
    opt.rank = [aim, gap, most, align](vec2 t) -> std::optional<long> {
        const int g = distance(t, aim);
        if (g > most || g >= gap) return std::nullopt;
        if (!align) return -g;
        const long dx = t.x - aim.x, dy = t.y - aim.y;
        return -(g * 4096L + dx * dx + dy * dy);
    };
}

bool go(const bc::bitboard& goals_in, const Go& opt) {
    auto& me = bc::self();
    auto origin = seen::self_position();
    if (opt.walk && !me.can_move()) {
        LOG(trace::move_failed("cannot move"););
        return false;
    }
    if (goals_in.is_set(origin)) {
        LOG(trace::move_failed("already on goal"););
        return false;
    }
    auto dash = ability_of(me, Kind::dash);
    if (opt.dash == DashUse::keep) dash.reset();
    const bool owns_dash = dash.has_value();

    // The caller's own landing ranking first; the rest of the move then runs from where it lands.
    if (opt.rank && dash && dash->is_ready()) {
        bool dashed = false;
        try {
            dashed = dash_ranked(*dash, origin, opt);
        } catch (const bc::error& error) {
            LOG(trace::move_failed(std::string("dash error ") + error_text(error)););
        }
        if (dashed) {
            trace::moved("dash");
            if (!opt.walk || !opt.walk_after_dash || goals_in.is_set(me.position())) return true;
            Go rest = opt;
            rest.rank = nullptr;
            go(goals_in, rest);
            return true;
        }
    }
    // A dash-only move with a ranking of its own takes no other landing.
    if (opt.rank && !opt.walk) return false;

    // Only free goals or occupied camp tiles can seed the field. Never mutate the caller's board.
    auto& s = scratch();
    auto& blocked = s.blocked;
    blocked_tiles_into(blocked);
    const auto* avoid = opt.avoid;
    if (avoid) {
        blocked |= *avoid;
        blocked.set(origin, false);
    }
    // v132f/g (WALK_GUARD): out of awake camps' reach, unless we already stand in it (then this is the walk out).
    const bool guarded = walk_guard && !walk_guard->is_set(origin);
    if (guarded) blocked |= *walk_guard;
    const auto& raw = goals_in;
    // goals_in & (~blocked | camp tiles), built in place to skip the temporary boards.
    auto& goals = s.goals;
    fast::not_into(blocked, goals);
    bc::vision::camp_tiles_into(s.part);
    goals |= s.part;
    goals &= goals_in;
    if (goals.is_empty()) {
        // v90-log: the reason text (goal_cover, no_step_reason read boards) is built only for the log.
        LOG(trace::move_failed("no free goal; " + goal_cover(raw, blocked)););
        return false;
    }
    // v98 (FIX_HOP): a dasher plans through holes even while its dash recharges, so it skips the engine's walk, but
    // only with a thin hole within HOP_NEAR: skipping it everywhere lost broken-curve (no holes) 0 of 6 against v97.
    bool dasher = false;
    if (FIX_HOP && dash) {
        fast::around_into(origin, static_cast<float>(HOP_NEAR), s.near);
        s.near &= thin_holes();
        dasher = !s.near.is_empty() || hop_sticky();
    }
    if (dash && !dash->is_ready()) dash.reset();
    // v133 (GO_FINISH_PATH): the closing dash goes before the keep rules: it only goes when it ends the approach.
    const bool finish = opt.dash == DashUse::finish && GO_FINISH_PATH && dash;
    // v115 (VESPER_KEEP_DASH): with an enemy champion in sight Vesper walks; her dash is kept for escapes and the kite.
    // v119 (JOIN_KEEP_DASH): everyone's walks do. v133: never an urgent move's.
    bool kept = false;
    if ((JOIN_KEEP_DASH || (VESPER_KEEP_DASH && me.champion_name() == Champ::vesper_vale))
        && !seen::champions(Team::enemy).empty() && opt.dash != DashUse::urgent && !opt.free_dash) {
        kept = true;
        dasher = false;
        if (!finish) dash.reset();
    }
    try {
        // v215 (TP): a champion holding a crystal plans on the field below (the crystal may beat this step).
        const bool tp_armed = TP && crystal::armed() && opt.walk && opt.dash != DashUse::urgent && !opt.rank;
        if (!dash && !dasher && !avoid && !guarded && opt.walk && goals.count() == 1 && !tp_armed) {
            std::array<vec2, 1> only{};
            if (goals.positions_into(only) == 1 && !blocked.is_set(only[0])) {
                if (me.move_towards(only[0])) {
                    trace::moved("move");
                    return true;
                }
                // A rejected step may have revealed a hidden unit in a bush (move_failed does this too, but only in the log).
                seen::reset();
                LOG(trace::move_failed("engine rejected move_towards"););
                return false;
            }
        }

        // Expand from the goals until our current tile is reached.
        auto& field = s.field;
        auto& reached = s.reached;
        auto& walls = s.walls;
        fast::not_into(goals, walls);
        walls &= blocked;
        fast::around_into(origin, 0, s.root);
        field.compute_to(walls, goals, s.root, reached);

        // v215 (TP): with a crystal, launch now, or walk to a stored launch tile instead of the goals.
        if (tp_armed) {
            vec2 pad{};
            const int tp = crystal::travel(me, origin, field, reached, blocked, goals, owns_dash, pad);
            if (tp == 1) return true;
            if (tp == 2) {
                goals.clear();
                goals.set(pad);
                fast::not_into(goals, walls);
                walls &= blocked;
                field.compute_to(walls, goals, s.root, reached);
            }
        }

        bool dashed = false;
        if (finish) {
            fast::not_into(blocked, s.part);
            s.part &= reached;
            dashed = dash_along(*dash, origin, field, s.part, opt.finish, opt.aim, opt.align);
            if (!dashed && kept) dash.reset();
        }
        const bool closed = dashed;

        // v135 (HOP_WHEN_CUT): no walking path at all: a dasher plans the hop route wherever the crossing is.
        if (HOP_WHEN_CUT && FIX_HOP && owns_dash && !kept && !reached.is_set(origin)) dasher = true;
        if (!dashed && dasher && opt.walk && (!avoid || GO_HOP_AVOID)) {
            bool acted = false;
            if (hop_route(me, origin, walls, goals, raw, blocked, field, reached, dash, acted)) return true;
        }

        // Dash whenever it is ready (it leaves the walk free), then walk on from where it lands. v133 (GO_SHORT_DASH):
        // not on a route walk this turn's steps already finish.
        if (!dashed && dash && !(GO_SHORT_DASH && opt.dash == DashUse::route && opt.walk && reached.is_set(origin)
                                 && static_cast<int>(field.get(origin)) <= static_cast<int>(move_speed(me)))) {
            fast::not_into(blocked, s.part);
            s.part &= reached;
            dashed = dash_along(*dash, origin, field, s.part, 1 << 20, std::nullopt, opt.align);
        }
        if (dashed) {
            trace::moved("dash");
            if (!opt.walk || (closed && !opt.walk_after_dash)) return true;
            origin = seen::self_position();
            blocked_tiles_into(blocked);
            if (avoid) blocked |= *avoid;
            if (walk_guard && !walk_guard->is_set(origin)) blocked |= *walk_guard;
            blocked.set(origin, false);
            if (goals.is_set(origin) || !me.can_move()) return true;
        }
        if (!opt.walk) return dashed;

        // Choose a reachable step with a smaller distance to the goal.
        blocked.reachable_tiles_into(origin, move_speed(me), s.reachable);
        s.reachable &= reached;
        Tiles tiles;
        const auto n = tiles_of(s.reachable, tiles);
        auto step = origin;
        auto best = field.get(origin);
        // v138 (CHASE_STEP): ties go to the step nearest opt.align, Chebyshev first, then the other axis.
        auto off = [&](vec2 t) {
            const long dx = t.x - opt.align->x, dy = t.y - opt.align->y;
            return distance(t, *opt.align) * 4096L + dx * dx + dy * dy;
        };
        long best_off = 0;
        for (std::size_t i = 0; i < n; ++i) {
            const auto steps = field.get(tiles[i]);
            if (steps < best) {
                step = tiles[i];
                best = steps;
                if (opt.align) best_off = off(step);
            } else if (opt.align && steps == best && step != origin) {
                if (const long o = off(tiles[i]); o < best_off) step = tiles[i], best_off = o;
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
    // v133t: our own tile from the per-vision cache (callers pass bc::self() as `me`).
    static const auto own_handle = static_cast<bc::handle_champion_t>(bc::self());
    const vec2 at = static_cast<bc::handle_champion_t>(me) == own_handle ? seen::self_position() : me.position();
    return distance(at, unit.position()) <= radius;
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
    // v133t: our own two slots never change (an ability is a handle; is_ready() and cooldown() still read the engine),
    // so their kinds are matched once; abilities() copies both slots on every call.
    static const auto own = bc::self().abilities();
    static const auto own_handle = static_cast<bc::handle_champion_t>(bc::self());
    if (static_cast<bc::handle_champion_t>(me) == own_handle) {
        for (const auto& ability : own)
            if (ability.is(kind)) return ability;
        return std::nullopt;
    }
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

std::vector<vec2>& aoe_centers(vec2 origin, const std::vector<vec2>& places, int reach, int radius) {
    // A body inside the cast can be the centre. One just past it needs a tile in between.
    // v120: the square around such a body is walked directly (radius 1: nine tiles) into one pooled list; three new
    // boards, a positions() list and a std::set node per tile cost more than the handful of tiles involved.
    static std::vector<vec2> centers;
    centers.clear();
    auto add = [&](vec2 tile) {
        if (std::find(centers.begin(), centers.end(), tile) == centers.end()) centers.push_back(tile);
    };
    const int width = bc::map::width(), height = bc::map::height();
    for (const auto place : places) {
        const int gap = distance(origin, place);
        if (gap <= reach) {
            add(place);
        } else if (gap <= reach + radius) {
            for (int dy = -radius; dy <= radius; ++dy)
                for (int dx = -radius; dx <= radius; ++dx) {
                    const vec2 tile{place.x + dx, place.y + dy};
                    if (tile.x < 0 || tile.y < 0 || tile.x >= width || tile.y >= height) continue;
                    if (distance(origin, tile) <= reach) add(tile);
                }
        }
    }
    return centers;
}

} // namespace

std::pair<std::optional<vec2>, int> aoe_aim(vec2 origin, const std::vector<vec2>& places, double reach,
                                            double radius, std::optional<vec2> focus, const std::vector<vec2>* clear) {
    if (places.empty()) return {std::nullopt, 0};
    std::optional<vec2> best;
    std::tuple<int, bool, int> best_key{};
    for (const auto center : aoe_centers(origin, places, static_cast<int>(reach), static_cast<int>(radius))) {
        if (clear && std::any_of(clear->begin(), clear->end(), [&](vec2 c) { return distance(center, c) <= radius; }))
            continue;
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
              std::optional<vec2> focus, int require, const std::vector<vec2>* clear) {
    if (!ability || !ability->is_ready()) return false;
    std::pair<std::optional<vec2>, int> aim;
    try {
        if (!ability->is_aoe_damage()) return false;
        const auto info = ability->get<bc::ability_aoe_damage>();
        aim = aoe_aim(bc::self().position(), places, info.range, info.radius, focus, clear);
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
