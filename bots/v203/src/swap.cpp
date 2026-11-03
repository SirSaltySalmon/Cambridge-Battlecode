// v135 (TANK_SWAP): a hurt champion farming with a sturdy ally beside the group steps behind it instead of leaving,
// and a sturdy ally steps in front of a camp that hits a hurt or fragile ally. Sturdy is a health gate (max health
// TANK_HEALTH+, health over the recall bar), not a champion type. Audit: 55% of hurt exits out of a farm had such an
// ally inside the same camp's reach; the exit pulled the ranged out too, and everyone walked back in.
#include "bot.h"
#include "seen.h"
#include "state.h"
#include "trace.h"
#include <array>

namespace {

bool sturdy(const bc::champion& a, double bar) {
    return a.max_health() >= TANK_HEALTH && a.health() >= a.max_health() * bar && !channelling(a);
}

}  // namespace

std::size_t Bot::awake_near(vec2 at, int margin, int s, std::array<AwakeCamp, 16>& out) const {
    // Awake camps (raw state read; a camp hidden in a bush fails it) whose reach comes within `margin` of `at`.
    std::size_t n = 0;
    for (const auto& c : seen::camps()) {
        if (n >= out.size()) break;
        const int reach = camp_kind_reach(c.kind());
        if (reach <= 0) continue;
        const auto spot = c.spawn_position();
        if (distance(at, spot) > reach + margin) continue;
        std::uint32_t state = 0;
        if (bc::detail::sdk_camp_state(static_cast<std::uint32_t>(static_cast<bc::handle_camp_t>(c)), &state) != 0 || state != 1)
            continue;
        out[n++] = {spot, reach, camp_kind_strength(c.kind()), slot_at(spot) == s};
    }
    return n;
}

bool Bot::swap_hurt(const bc::champion& me) {
    return swap_behind(me, true);
}

bool Bot::swap_behind(const bc::champion& me, bool hurt) {
    if (!TANK_SWAP || route.empty() || !near_enemies().empty() || (FIX_HIDDEN && hidden_active())) return false;
    const double top = me.max_health(), health = me.health();
    if (hurt && health < top * FINISH_HEALTH) return false;
    const int s = slot % static_cast<int>(route.size());
    const auto position = me.position();
    std::array<AwakeCamp, 16> camps{};
    const std::size_t n = awake_near(position, 2, s, camps);
    bool ours = false;
    for (std::size_t i = 0; i < n; ++i) ours = ours || camps[i].ours;
    if (!ours) return false;
    // Sturdy allies near those camps (us excluded). v148-normal (RANGED_COVER): not hurt, a ranged champion only steps
    // behind a melee or an ally with more health than us.
    std::array<vec2, 8> tanks{};
    std::array<bool, 8> first{};  // v148-normal (CAMP_TIE_ORDER): the tank acts before us, so it also takes a tie
    std::size_t t_n = 0;
    const auto& allies = seen::champions(Team::ally);
    const auto& tiles = seen::positions(Team::ally);
    for (std::size_t i = 0; i < allies.size() && i < tiles.size() && t_n < tanks.size(); ++i) {
        if (allies[i] == me || !sturdy(allies[i], RECALL_HEALTH)) continue;
        if (!hurt && allies[i].attack_range() > 1 && allies[i].health() <= health) continue;
        // SWAP_STEP: a cover is healthier than us and over TANK_TAKE_HEALTH (hurt allies passed four camps around
        // among themselves until all were under the bar).
        if (SWAP_STEP && (allies[i].health() <= health || !sturdy(allies[i], TANK_TAKE_HEALTH))) continue;
        bool near = false;
        for (std::size_t k = 0; k < n && !near; ++k) near = distance(tiles[i], camps[k].at) <= camps[k].reach + 2;
        if (near) {
            first[t_n] = acts_before_me(allies[i]);
            tanks[t_n++] = tiles[i];
        }
    }
    if (t_n == 0) return false;
    const auto group = camps_at(s);
    const int reach = me.attack_range();
    // Candidate tiles: here and every free step.
    bc::bitboard blocked{bc::uninitialized}, step{bc::uninitialized};
    blocked_tiles_into(blocked);
    blocked.set(position, false);
    blocked.reachable_tiles_into(position, me.can_move() ? 1 : 0, step);
    step.set(position, true);
    struct Pick {
        vec2 at;
        bool covered, hits_camp, here;
        int open;  // strength of the camps still hitting this tile
    };
    std::optional<Pick> best;
    auto better = [](const Pick& a, const Pick& b) {
        if (a.covered != b.covered) return a.covered;
        // v148-normal (SWAP_STEP): among covered tiles one we hit from first; uncovered, the least hit first.
        if (SWAP_STEP && a.covered && a.hits_camp != b.hits_camp) return a.hits_camp;
        if (a.open != b.open) return a.open < b.open;
        if (a.hits_camp != b.hits_camp) return a.hits_camp;
        return a.here && !b.here;
    };
    bool here_covered = true;
    for (const auto t : step.positions()) {
        Pick p{t, true, false, t == position, 0};
        for (std::size_t k = 0; k < n; ++k) {
            const int g = distance(t, camps[k].at);
            if (g > camps[k].reach) continue;
            bool shield = false;
            for (std::size_t a = 0; a < t_n && !shield; ++a) shield = distance(tanks[a], camps[k].at) < g + (first[a] ? 1 : 0);
            if (!shield) {
                p.covered = false;
                p.open += camps[k].strength;
            }
        }
        for (const auto& c : group)
            if (distance(t, c.spawn_position()) <= reach) p.hits_camp = true;
        if (p.here) here_covered = p.covered;
        if (!best || better(p, *best)) best = p;
    }
    if (!best) return false;
    if (!hurt) {
        // RANGED_COVER: only when a camp hits us here and a covered tile still reaches the group.
        if (here_covered || !best->covered || !best->hits_camp) return false;
    } else if (SWAP_STEP && best->covered && !best->hits_camp) {
        // Out one tile behind the tank, for a few rounds at most.
        if (swap_idle_round != turn_round - 1) swap_idle = 0;
        if (swap_idle >= SWAP_IDLE_ROUNDS) return false;
        ++swap_idle;
        swap_idle_round = turn_round;
    } else if (SWAP_HIT && !best->hits_camp) {
        return false;
    }
    if (hurt && !best->covered) {
        // Not covered yet: stay only while our health outlasts the hits until the nearest sturdy ally is in front.
        int rounds = 1 << 10;
        for (std::size_t a = 0; a < t_n; ++a)
            for (std::size_t k = 0; k < n; ++k) rounds = std::min(rounds, std::max(0, distance(tanks[a], camps[k].at) - 1));
        const double take = best->open * (rounds + 1) * camp_taken_mult(owned_of(me).passives);
        if (health - take < top * FINISH_HEALTH) return false;
    }
    bool moved = false;
    if (best->at != position) {
        bc::bitboard goal;
        goal.set(best->at, true);
        Go opt;
        opt.dash = DashUse::keep;
        moved = go(goal, opt);
    }
    bool hit = false;
    if (me.can_attack()) {
        std::vector<bc::camp> in_reach;
        for (const auto& c : camps_at(s))
            if (within(me, c, reach)) in_reach.push_back(c);
        if (const auto target = lowest(in_reach)) hit = attack_if_in_range(*target);
    }
    if (hit) swap_idle = 0;
    WHY(hurt ? "swap.behind" : "swap.cover",
        std::string(!hurt               ? "a camp hits us with a sturdy ally there: step where it takes the hits"
                    : best->covered     ? "hurt, a sturdy ally takes the camps: stay behind it"
                                        : "hurt, a sturdy ally is stepping in: hold on the least-hit tile")
            + (moved ? ", step" : "") + (hit ? ", hit" : ""));
    return true;
}

bool Bot::swap_tank_step(const bc::champion& me, int s) {
    if (!TANK_SWAP || route.empty() || bc::fuel_remaining() < V135_SPARE_FUEL) return false;
    if (me.max_health() < TANK_HEALTH || !near_enemies().empty() || !me.can_move()) return false;
    // camp_heat's scan (cached per vision list) also reports whether any awake camp is near at all.
    camp_heat(me);
    if (!heat_any || !sturdy(me, TANK_TAKE_HEALTH)) return false;
    const auto position = me.position();
    std::array<AwakeCamp, 16> camps{};
    const std::size_t n = awake_near(position, 3, s, camps);
    if (n == 0) return false;
    const auto& allies = seen::champions(Team::ally);
    const auto& tiles = seen::positions(Team::ally);
    // Camps of the group whose nearest champion (a tie counts) is an ally that should not tank: hurt or fragile.
    // v148-normal (CAMP_TIE_ORDER): a tie goes to whoever acts first, so the gap to beat is the weak ally's gap, or one
    // more when we act before every weak ally at it.
    std::array<std::pair<vec2, int>, 16> need{};  // camp tile, a gap under which we take the camp
    std::size_t need_n = 0;
    // Each ally's sturdiness read once (health and max health are engine reads), not once per camp. v137: only for allies
    // inside a camp's reach (0 unread, 1 sturdy, 2 not): most allies are elsewhere.
    std::array<char, 16> strong{};
    // v148-normal (TANK_TAKE_EARLY): an ally under BAND_HEALTH with less health share than ours, or a ranged ally while
    // we are melee, hands the camp over too.
    const double my_share = TANK_TAKE_EARLY ? me.health() / me.max_health() : 0;
    const bool melee = me.attack_range() <= 1;
    auto hands_over = [&](const bc::champion& a) {
        if (!sturdy(a, RECALL_HEALTH)) return true;
        if (!TANK_TAKE_EARLY) return false;
        if (melee && a.attack_range() > 1) return true;
        const double share = a.health() / a.max_health();
        return share < BAND_HEALTH && share < my_share;
    };
    for (std::size_t k = 0; k < n; ++k) {
        if (!camps[k].ours) continue;
        const int mine = distance(position, camps[k].at);
        int weak = 1 << 10;
        bool strong_nearer = false, weak_first = false;
        for (std::size_t i = 0; i < allies.size() && i < tiles.size() && i < strong.size(); ++i) {
            if (allies[i] == me) continue;
            const int g = distance(tiles[i], camps[k].at);
            if (g > camps[k].reach) continue;
            if (!strong[i]) strong[i] = hands_over(allies[i]) ? 2 : 1;
            if (strong[i] == 1) {
                if (g < mine || (g == mine && acts_before_me(allies[i]))) strong_nearer = true;
            } else if (g < weak) {
                weak = g;
                weak_first = acts_before_me(allies[i]);
            } else if (g == weak) {
                weak_first = weak_first || acts_before_me(allies[i]);
            }
        }
        const int bar = !CAMP_TIE_ORDER || weak_first ? weak : weak + 1;
        if (strong_nearer || weak > camps[k].reach || mine < bar) continue;
        need[need_n++] = {camps[k].at, bar};
    }
    if (need_n == 0) return false;
    bc::bitboard blocked{bc::uninitialized}, step{bc::uninitialized};
    blocked_tiles_into(blocked);
    blocked.reachable_tiles_into(position, 1, step);
    std::optional<vec2> best;
    int best_cover = 0, best_gap = 1 << 10;
    for (const auto t : step.positions()) {
        if (t == position) continue;
        int cover = 0, gap = 0;
        for (std::size_t k = 0; k < need_n; ++k) {
            const int g = distance(t, need[k].first);
            if (g < need[k].second) ++cover;
            gap += g;
        }
        if (cover > best_cover || (cover == best_cover && cover > 0 && gap < best_gap)) {
            best = t;
            best_cover = cover;
            best_gap = gap;
        }
    }
    if (!best || best_cover == 0) return false;
    if (!me.move_absolute(*best)) return false;
    trace::moved("move");
    // v202 (BUSY_JOBS, JOB_SHARED): the carrier bits hold the job holders then (the tank on duty was never read).
    if (!jobs_shared()) write_tank(cached_ident(me));
    NOTE(format("tank swap: steps in front of %d camp(s) hitting a hurt ally", best_cover));
    return true;
}

void Bot::update_abandon(const bc::champion& me, int s) {
    if (abandon_until.size() != route.size()) abandon_until.assign(route.size(), 0);
    const int now = static_cast<int>(bc::round_number());
    turn_round = now;
    // Out of sight range of the group: nothing of it to read.
    if (distance(seen::self_position(), centers[s]) > 8 + 4) return;
    // Its camps in sight: one awake keeps the question open; all asleep frees an abandoned group again.
    bool seen_any = false;
    std::array<AwakeCamp, 16> awake{};
    std::size_t n = 0;
    for (const auto& c : seen::camps()) {
        const auto spot = c.spawn_position();
        if (slot_at(spot) != s) continue;
        seen_any = true;
        const int reach = camp_kind_reach(c.kind());
        if (reach <= 0 || n >= awake.size()) continue;
        std::uint32_t state = 0;
        if (bc::detail::sdk_camp_state(static_cast<std::uint32_t>(static_cast<bc::handle_camp_t>(c)), &state) != 0 || state != 1)
            continue;
        awake[n++] = {spot, reach, camp_kind_strength(c.kind()), true};
    }
    if (seen_any && n == 0) {
        if (abandon_until[s] > now) NOTE(format("s%d is asleep again: back on the table", s));
        abandon_until[s] = 0;
        return;
    }
    if (n == 0) return;
    const auto& allies = seen::champions(Team::ally);
    const auto& tiles = seen::positions(Team::ally);
    const auto& enemies = seen::positions(Team::enemy);
    bool hurt_hit = false;
    for (std::size_t k = 0; k < n; ++k) {
        // A sturdy ally near any awake camp of it keeps the group (TANK_SWAP takes it from there).
        int best = 1 << 10;
        std::size_t who = allies.size();
        for (std::size_t i = 0; i < allies.size() && i < tiles.size(); ++i) {
            const int g = distance(tiles[i], awake[k].at);
            const auto& a = allies[i];
            if (channelling(a)) continue;
            if (g <= awake[k].reach + 2 && a.max_health() >= TANK_HEALTH && a.health() >= a.max_health() * RECALL_HEALTH)
                return;
            if (g <= awake[k].reach && g < best) best = g, who = i;
        }
        for (const auto e : enemies)
            if (distance(e, awake[k].at) < best) who = allies.size();
        if (who < allies.size()) {
            const auto& a = allies[who];
            if (a.health() < a.max_health() * BAND_HEALTH) hurt_hit = true;
        }
    }
    if (!hurt_hit) return;
    if (abandon_until[s] <= now) NOTE(format("s%d abandoned: its awake camps hit a hurt ally and no sturdy ally is near", s));
    abandon_until[s] = now + ABANDON_ROUNDS;
}
