// Fighting, retreating, and clearing the current camp group.
#include "bot.h"
#include "seen.h"
#include "state.h"
#include "trace.h"
#include <algorithm>
#include <cmath>

namespace {
// v133t: whether a champion in sight other than us stands nearer `spot` than `gap` (an awake camp hits the nearest),
// from the vision lists' own tiles: no engine read per camp and champion.
// v148-normal (CAMP_TIE_ORDER): an ally level with us that acts before us counts too (enemy ties stay ours).
bool other_nearer(const bc::champion& me, vec2 spot, int gap) {
    const auto& allies = seen::champions(Team::ally);
    const auto& ally_tiles = seen::positions(Team::ally);
    for (std::size_t i = 0; i < allies.size(); ++i) {
        const int g = distance(ally_tiles[i], spot);
        if ((g < gap || (g == gap && acts_before_me(allies[i]))) && allies[i] != me) return true;
    }
    for (const auto tile : seen::positions(Team::enemy))
        if (distance(tile, spot) < gap) return true;
    return false;
}
} // namespace

const std::vector<bc::champion>& Bot::near_enemies() {
    // Worked out once per vision list: the list is only replaced after one of our own actions, and an
    // old one is kept alive until the turn ends, so its address names it within a round.
    const auto& enemies = seen::champions(Team::enemy);
    const auto round = bc::round_number();
    if (near_src == &enemies && near_round == round) return home_skipped ? home_buf : far_skipped ? near_buf : enemies;
    home_skipped = 0;
    const auto& near = near_by_steps();
    if (!FIX_HOME_CAMPERS || near.empty()) return near;
    // v76 (FIX_HOME_CAMPERS): an enemy on or beside its own spawn heals to full every turn, so it cannot be killed
    // there; it no longer counts as near, so it neither keeps the team in fight mode nor becomes a target. On
    // rushbait_v2 (spawns ~5 tiles apart) LigaSiur sat on its spawn hitting ours: 0 kills either way in 1,083
    // rounds, while our team hovered at home in fight mode and captured 6 camps.
    // v141 (HOME_STALEMATE): with us beside our own spawn, the cut-off widens to HOME_STALE_STEPS. A side has at most
    // six champions, so the flags sit in a fixed array (one position read each, not two).
    const int cut = HOME_STALEMATE && steps_to_ally_spawn(seen::self_position()) <= HOME_STALE_STEPS
                        ? std::max(HOME_STALE_STEPS, HOME_CAMP_STEPS) : HOME_CAMP_STEPS;
    std::array<bool, 8> home{};
    bool any = false;
    int stale = 0;
    for (std::size_t i = 0; i < near.size() && i < home.size(); ++i) {
        const int steps = steps_to_enemy_spawn(near[i].position());
        if (steps <= cut) home[i] = any = true;
        if (steps <= cut && steps > HOME_CAMP_STEPS) ++stale;
    }
    if (stale) NOTE(format("home stalemate: %d enemies beside their spawn while we are beside ours, not near", stale));
    if (!any) return near;
    home_buf.clear();
    for (std::size_t i = 0; i < near.size(); ++i) {
        if (i < home.size() && home[i])
            ++home_skipped;
        else
            home_buf.push_back(near[i]);
    }
    return home_buf;
}

const std::vector<bc::champion>& Bot::near_by_steps() {
    const auto& enemies = seen::champions(Team::enemy);
    const auto round = bc::round_number();
    near_src = &enemies;
    near_round = round;
    far_skipped = 0;
    if (enemies.empty()) return enemies;
    // Walking steps are never fewer than straight-line tiles, so the search only starts from team
    // members within FIGHT_STEPS tiles of some enemy, only runs when there is one, and stops at
    // FIGHT_STEPS. A side has at most six champions, so fixed arrays stand in for vectors (every
    // allocation costs fuel), and the search reuses one field and its boards.
    constexpr std::size_t MAX_UNITS = 8;
    const auto& me = bc::self();
    std::array<vec2, MAX_UNITS> team, spots;
    std::size_t team_n = 0, spot_n = 0;
    // v123 (FIGHT_LOCAL): only allies within FIGHT_LOCAL_RANGE of us root the search (one far off, seen through our
    // ward, has its own fight). The walking-step test (FIGHT_LOCAL_WALK) runs only for an ally that would root it.
    // v133t: tiles from the vision lists themselves (read with them).
    const vec2 here = seen::self_position();
    for (const auto tile : seen::positions(Team::ally))
        if (team_n < MAX_UNITS && (!FIGHT_LOCAL || distance(tile, here) <= FIGHT_LOCAL_RANGE)) team[team_n++] = tile;
    if (me.is_alive() && team_n < MAX_UNITS) team[team_n++] = here;
    for (const auto tile : seen::positions(Team::enemy))
        if (spot_n < MAX_UNITS) spots[spot_n++] = tile;
    std::array<bool, MAX_UNITS> root{}, near{};
    bool any_close = false;
    for (std::size_t t = 0; t < team_n; ++t) {
        for (std::size_t e = 0; e < spot_n && !root[t]; ++e)
            if (distance(team[t], spots[e]) <= FIGHT_STEPS) root[t] = true;
        if (root[t] && FIGHT_LOCAL && FIGHT_LOCAL_WALK && team[t] != here && !local_reach(here).is_set(team[t])) root[t] = false;
        any_close = any_close || root[t];
    }
    if (any_close) {
        if (!near_field) near_field.emplace();
        near_roots.clear();
        for (std::size_t t = 0; t < team_n; ++t)
            if (root[t]) near_roots.set(team[t]);
        near_reached.clear();
        near_field->compute(seen::terrain(), near_roots, near_reached, FIGHT_STEPS);
        for (std::size_t e = 0; e < spot_n; ++e) near[e] = near_reached.is_set(spots[e]);
    }
    std::size_t first = 0;
    while (first < spot_n && near[first]) ++first;
    if (first == enemies.size()) return enemies;
    near_buf.assign(enemies.begin(), enemies.begin() + first);
    for (std::size_t i = first; i < enemies.size(); ++i) {
        if (i < spot_n && near[i])
            near_buf.push_back(enemies[i]);
        else
            ++far_skipped;
    }
    return near_buf;
}

bool Bot::assists(vec2 ally, vec2 from) {
    return distance(ally, from) <= FIGHT_LOCAL_RANGE && (!FIGHT_LOCAL_WALK || local_reach(from).is_set(ally));
}

const bc::bitboard& Bot::local_reach(vec2 from) {
    const auto round = bc::round_number();
    for (const auto& slot : local_reach_slots)
        if (slot.valid && slot.round == round && slot.from == from) return slot.tiles;
    auto& slot = local_reach_slots[local_reach_next];
    local_reach_next ^= 1;
    if (!near_field) near_field.emplace();
    near_roots.clear();
    near_roots.set(from);
    slot.tiles.clear();
    near_field->compute(seen::terrain(), near_roots, slot.tiles, FIGHT_LOCAL_RANGE);
    slot.round = round;
    slot.from = from;
    slot.valid = true;
    return slot.tiles;
}

bool Bot::fight(const bc::champion& me) {
    auto enemies = near_enemies();
    if (enemies.empty() && FIX_PING && answer_ping(me)) return true;
    if (enemies.empty()) {
        if (far_skipped) NOTE(format("fight mode, but the %d enemies are over %d steps away", far_skipped, FIGHT_STEPS));
        return false;
    }
    // Near their spawn, only chase what dies before it gets home (anything already in reach still gets
    // hit); with nothing left, leave.
    std::string detail;
    if (spawn_zone.is_set(me.position())) {
        std::vector<bc::champion> kept;
        for (const auto& enemy : enemies) {
            if (within(me, enemy, me.attack_range())) {
                kept.push_back(enemy);
                LOG(detail += (detail.empty() ? "" : "; ") + at(enemy.position()) + " in reach";);
                continue;
            }
            const auto [keep, text] = chase_line(me, enemy);
            LOG(detail += (detail.empty() ? "" : "; ") + text;);
            if (keep) kept.push_back(enemy);
        }
        enemies = kept;
        if (enemies.empty()) {
            left_ring_round = static_cast<int>(bc::round_number());
            // v141 (RING_FARM): farm on from here; the retreat rules still keep us out of reach.
            if (RING_FARM) {
                NOTE("in their chase ring, nobody dies before reaching home (" + detail + "): farm instead");
                return false;
            }
            WHY("fight.leave", "in their chase ring, nobody dies before reaching home (" + detail + ")");
            move_towards_closest(~spawn_zone);
            return true;
        }
    } else {
        // v76 (FIX_STUCK_RING): an enemy standing in its own chase ring that we cannot catch before it gets home is not
        // a target from outside either; closing on it only walked us into the ring, where the line above sent us
        // straight back out (rushbait_v2: ~1,300 turns of fight.hit / fight.leave in one game).
        // Checked only on the turn right after we stepped out of the ring, which is the bounce itself.
        if (FIX_STUCK_RING && left_ring_round == static_cast<int>(bc::round_number()) - 1) {
            std::vector<bc::champion> kept;
            for (const auto& enemy : enemies) {
                if (!spawn_zone.is_set(enemy.position()) || within(me, enemy, me.attack_range()) || chase_line(me, enemy).first)
                    kept.push_back(enemy);
            }
            if (kept.empty()) {
                NOTE("every enemy is home in its chase ring, out of reach: farm instead");
                return false;
            }
            enemies = kept;
        }
    }
    // v116 (CHASE_LINE_ALL): outside the ring as well, only what dies before it gets home.
    if (CHASE_LINE_ALL && !spawn_zone.is_set(me.position())) {
        std::vector<bc::champion> kept;
        for (const auto& e : enemies)
            if (within(me, e, me.attack_range()) || channelling(e) || chase_line(me, e).first) kept.push_back(e);
        if (kept.empty()) {
            NOTE("nobody in sight dies before it gets home: farm instead");
            return false;
        }
        enemies = kept;
    }
    // v116 (CHASE_PATIENCE): enemies we have stopped gaining on are not chased.
    if (CHASE_PATIENCE) {
        std::vector<bc::champion> kept;
        for (const auto& e : enemies)
            if (!chase_dropped(me, e)) kept.push_back(e);
        if (kept.empty()) {
            NOTE("every enemy in sight outran our chase: farm instead");
            return false;
        }
        enemies = kept;
    }
    // A tributing enemy is stuck in place with its bag, which the kill steals.
    std::vector<bc::champion> in_reach, in_reach_channel, channel;
    for (const auto& e : enemies) {
        const bool reach = within(me, e, me.attack_range());
        if (reach) in_reach.push_back(e);
        if (reach && channelling(e)) in_reach_channel.push_back(e);
        if (channelling(e)) channel.push_back(e);
    }
    const char* pick = "channelling enemy in reach";
    auto target = lowest(in_reach_channel);
    if (!target) target = lowest(in_reach), pick = "weakest enemy in reach";
    if (!target) target = lowest(channel), pick = "channelling enemy, closing";
    if (!target) target = lowest(enemies), pick = "weakest enemy, closing";
    // v105 (CAMP_EXIT_REACT): awake camps hitting us come first (a melee swings at the target before stepping), unless
    // a channelling enemy is in reach: its bag is the kill's prize. Not hit, we close around awake camps' reach when
    // the target can be hit from outside it. Willow's backline already keeps her out of camp reach.
    const auto kind = me.champion_name();
    const bc::bitboard* avoid = nullptr;
    // v132f/g (CAMP_REACT_ANY): out of every awake camp reaching us, hitting us or an ally, even for a channelling enemy.
    const bool camp_in = camp_heat(me) > 0 || (CAMP_REACT_ANY && heat_any && heat_reach.is_set(me.position()));
    if (CAMP_EXIT_REACT && kind != Champ::willow && camp_in && (CAMP_REACT_ANY || in_reach_channel.empty())) {
        if (me.attack_range() <= 1) attack_if_in_range(*target);
        if (camp_exit(me, enemies, "fight")) return true;
        // The swing may have killed the target or the exit's reads moved on: engage reads it again.
        if (!sighted(*target) || !alive(*target)) {
            WHY("fight.hit", "hit the target before leaving the camps; no way out");
            return true;
        }
    } else if (CAMP_EXIT_REACT && kind != Champ::willow && heat_any && !heat_reach.is_set(me.position())
               && !within(me, *target, me.attack_range())) {
        fast::around_into(target->position(), static_cast<float>(me.attack_range()), exit_goal);
        fast::not_into(heat_reach, exit_out);
        exit_goal &= exit_out;
        if (!exit_goal.is_empty()) {
            avoid = &heat_reach;
            NOTE("closes on the enemy around awake camps' reach");
        }
    }
    // v132d/e (CAMP_SHARE): an awake camp hits an ally beside us; we would be next. Leave with the ally, toward the target.
    if (CAMP_SHARE && kind != Champ::willow && in_reach_channel.empty() && me.can_move() && camp_heat(me) == 0
        && !within(me, *target, me.attack_range()) && ally_tanked_camp(me)) {
        bool hit = false;
        for (const auto& e : enemies)
            if (!hit && attack_if_in_range(e)) hit = true;
        remember_hitting_camps(me);
        fast::not_into(heat_reach, exit_out);
        fast::around_into(me.position(), static_cast<float>(CAMP_EXIT_STEPS + 3), exit_reach);
        exit_out &= exit_reach;
        fast::around_into(target->position(), static_cast<float>(me.attack_range()), exit_goal);
        exit_goal &= exit_out;
        const char* how = "toward a tile reaching the target";
        bool moved = !exit_goal.is_empty() && leave_camps(exit_goal);
        if (!moved) how = "out", moved = leave_camps(exit_out);
        if (moved || hit) {
            WHY("fight.share", std::string("an awake camp hits an ally beside us, we are next: leave it ") + how
                                   + (hit ? ", hit what is in reach" : ""));
            return true;
        }
    }
    // v132 (CAMP_WAIT_FIGHT): not back into awake camps we stepped out of: around them, else wait outside until they sleep.
    if (CAMP_WAIT_FIGHT && kind != Champ::willow && !within(me, *target, me.attack_range()) && camp_wait_fight(me)) {
        fast::around_into(target->position(), static_cast<float>(me.attack_range()), exit_goal);
        fast::not_into(fight_wait, exit_out);
        exit_goal &= exit_out;
        // An enemy that can hit us where we stand is fought as before: holding there only takes its hits.
        if (exit_goal.is_empty() && !threat_zone(seen::champions(Team::enemy), false).is_set(me.position())) {
            bool hit = false;
            for (const auto& e : enemies)
                if (!hit && attack_if_in_range(e)) hit = true;
            WHY("fight.wait", "the target is only in reach from inside awake camps we left: wait outside until they sleep"
                                  + std::string(hit ? ", hit what is in reach" : ""));
            return true;
        }
        if (!exit_goal.is_empty()) {
            if (avoid) fight_wait |= *avoid;
            avoid = &fight_wait;
            NOTE("camp wait: closes on the enemy around the awake camps we left");
        }
    }
    // v159 (MONO_FIGHT_OUT): out of every awake monolith's reach first, then close on the target around it.
    if (MONO_FIGHT_OUT) {
        mono_zone = mono_fight_zone();
        if (!mono_zone.is_empty()) {
            if (mono_zone.is_set(me.position()) && camp_exit(me, enemies, "fight.mono")) return true;
            if (avoid) mono_zone |= *avoid;
            avoid = &mono_zone;
        }
    }
    // v119 (JOIN_FIRST): not alone into a fight we are not in yet.
    if (JOIN_FIRST && !within(me, *target, me.attack_range()) && !channelling(*target) && join_first(me, *target, enemies, avoid))
        return true;
    WHY("fight.hit", std::string(pick) + " at " + at(target->position()) + (detail.empty() ? "" : " (chase ring: " + detail + ")"));
    commit_ping(me);
    mark_ghost(*target);
    engage(me, *target, enemies, false, avoid);
    return true;
}

bool Bot::join_first(const bc::champion& me, const bc::champion& target, const std::vector<bc::champion>& enemies,
                     const bc::bitboard* avoid) {
    // A contact is the run of rounds we close on enemies; we wait for an ally at most JOIN_WAIT rounds of it. Willow
    // already keeps out of reach, and anyone an enemy can hit fights on (waiting there is a retreat).
    if (me.champion_name() == Champ::willow) return false;
    const int now = static_cast<int>(bc::round_number());
    if (join_last < now - 1) join_since = now;
    join_last = now;
    const bool leash = CHASE_LEASH > 0 && static_cast<int>(seen::roster(Team::ally).size()) <= LEASH_TEAM;
    if (now - join_since >= (leash ? LEASH_WAIT : JOIN_WAIT)) return false;
    const auto position = me.position();
    for (const auto& e : enemies)
        if (within(me, e, me.attack_range())) return false;
    if (threat_zone(seen::champions(Team::enemy), false).is_set(position)) return false;
    bool living = false;
    for (const auto& a : seen::roster(Team::ally)) {
        try {
            if (a != me && a.is_alive()) living = true;
        } catch (const bc::error&) {
        }
    }
    if (!living) return false;
    const bool squishy = me.attack_range() > 1 || me.max_health() <= SQUISHY_HEALTH;
    const int to_target = distance(position, target.position());
    std::optional<vec2> nearest;
    int near_d = 1 << 20;
    bool tank_in_sight = false, tank_ahead = false, leashed = !leash;
    for (const auto& a : seen::champions(Team::ally)) {
        if (a == me || channelling(a)) continue;
        const int d = distance(position, a.position());
        if (d < near_d) near_d = d, nearest = a.position();
        if (distance(a.position(), target.position()) <= to_target + CHASE_LEASH) leashed = true;
        if (a.attack_range() <= 1 && a.max_health() > SQUISHY_HEALTH) {
            tank_in_sight = true;
            if (distance(a.position(), target.position()) < to_target) tank_ahead = true;
        }
    }
    const bool covered = squishy ? near_d <= JOIN_RANGE && (tank_ahead || !tank_in_sight) : near_d <= JOIN_RANGE + 3;
    if (covered && leashed) return false;
    LOG(if (covered) note("leash: no ally keeps up with us toward the target"););
    if (!nearest && JOIN_ALONE == 1) return false;
    if (!nearest && JOIN_ALONE == 2) {
        // Nobody in sight to wait for: the enemy out of reach is not ours to chase; farm on.
        WHY("fight.join.farm", "no ally in sight to close with: farm instead");
        farm(me, CAMP_WAIT_FIGHT ? avoid : nullptr, "join");
        return true;
    }
    if (nearest && near_d > 1) {
        WHY("fight.join", format("no ally within %d of us: walk to the nearest, %d away", JOIN_RANGE, near_d));
        if (!move_towards_closest(tiles_around(*nearest, 1), CAMP_WAIT_FIGHT ? avoid : nullptr)) NOTE("join: no step toward the ally");
    } else {
        WHY("fight.join", nearest ? std::string("wait for a sturdy ally to lead") : std::string("no ally in sight: hold"));
    }
    return true;
}

namespace {
// v119 (CATCH_MODEL): the ranges of a champion's ready dash and ready stun (0 for none). An enemy's read can fail.
struct Ready {
    int dash = 0, stun = 0;
};
Ready ready_moves(const bc::champion& c) {
    Ready out;
    try {
        for (const auto& slot : c.abilities()) {
            if (!slot.is_ready()) continue;
            const int range = static_cast<int>(ability_range(slot));
            if (slot.is(Kind::dash)) out.dash = std::max(out.dash, range);
            else if (slot.is(Kind::cc)) out.stun = std::max(out.stun, range);
        }
    } catch (const std::exception&) {
    }
    return out;
}
// The tiles of gap left after the chaser's dash and stun, against an enemy walking away (its dash opens it again).
// v133t: from values read once by the caller (each read is an engine call).
// v138 (CATCH_ACROSS): an enemy whose last step kept its gap to the chaser (across it) is closed on foot.
int short_of(vec2 chaser_at, int chaser_reach, const Ready& ours, vec2 enemy_at, bool enemy_channel, const Ready& theirs,
             std::optional<vec2> step = std::nullopt) {
    const int d = distance(chaser_at, enemy_at);
    const int gap = std::max(0, d - chaser_reach);
    if (gap == 0 || enemy_channel || (ours.stun > 0 && d <= ours.stun)) return 0;
    if (step && distance(chaser_at, enemy_at + *step) <= d) return 0;
    return std::max(0, gap - ours.dash + theirs.dash);
}
int short_of(const bc::champion& chaser, const Ready& ours, const bc::champion& enemy, const Ready& theirs,
             std::optional<vec2> step = std::nullopt) {
    return short_of(chaser.position(), static_cast<int>(chaser.attack_range()), ours, enemy.position(), channelling(enemy), theirs,
                    step);
}
} // namespace

int Bot::catch_short(const bc::champion& chaser, const bc::champion& enemy) {
    return short_of(chaser, ready_moves(chaser), enemy, ready_moves(enemy), CATCH_ACROSS ? enemy_step(enemy) : std::nullopt);
}

void Bot::update_team_chase(const bc::champion& me, int current_mode) {
    // Once a round, from our own sight: us and the allies in sight (not channelling) against the near enemies.
    if (!TEAM_CHASE) return;
    const int now = static_cast<int>(bc::round_number());
    if (team_gap_seen == now) return;
    const auto& enemies = near_enemies();
    if (enemies.empty() || !me.is_alive()) {
        team_gap_seen = -10;
        return;
    }
    int gap = 1 << 20;
    bool engaged = false;
    // v133t: every unit's tile, range and channel read once, not once per pair (each read is an engine call).
    constexpr std::size_t MAX_UNITS = 8;
    std::array<vec2, MAX_UNITS> spots;
    std::array<bool, MAX_UNITS> spot_channel{};
    std::size_t spot_n = 0;
    for (const auto& e : enemies) {
        const bool channel = channelling(e);
        if (channel) engaged = true;
        if (spot_n < MAX_UNITS) {
            spot_channel[spot_n] = channel;
            spots[spot_n++] = e.position();
        }
    }
    auto body = [&](vec2 at, const bc::champion& a) {
        if (spot_n == 0) return;
        const bool channel = channelling(a);
        const int reach = channel ? 0 : static_cast<int>(a.attack_range()) + 1;
        for (std::size_t i = 0; i < spot_n; ++i) {
            const int d = distance(at, spots[i]);
            if (channel) {
                if (d <= TEAM_CHASE_GUARD) engaged = true;
                continue;
            }
            gap = std::min(gap, d);
            if (d <= reach) engaged = true;
        }
    };
    body(seen::self_position(), me);
    const auto& allies = seen::champions(Team::ally);
    const auto& ally_tiles = seen::positions(Team::ally);
    for (std::size_t i = 0; i < allies.size(); ++i)
        if (allies[i] != me) body(ally_tiles[i], allies[i]);
    if (engaged || gap == (1 << 20)) {
        team_gap_best = gap;
        team_gap_since = now;
        team_gap_seen = now;
        if (team_chase_until > now) NOTE("team chase: engaged again, fights back on");
        team_chase_until = -1;
        catch_miss = 0;
        catch_gap_last = gap;
        return;
    }
    const bool continued = team_gap_seen == now - 1;
    // v119 (CATCH_MODEL): can anyone in sight close on any near enemy with what is ready now?
    if (CATCH_MODEL) {
        std::array<Ready, MAX_UNITS> theirs{};
        std::array<std::optional<vec2>, MAX_UNITS> steps{};
        for (std::size_t i = 0; i < spot_n; ++i) {
            theirs[i] = ready_moves(enemies[i]);
            if (CATCH_ACROSS) steps[i] = enemy_step(enemies[i]);
        }
        bool closable = false;
        auto check = [&](vec2 at, const bc::champion& a) {
            if (closable || channelling(a)) return;
            const auto ours = ready_moves(a);
            const int reach = static_cast<int>(a.attack_range());
            for (std::size_t i = 0; i < spot_n && !closable; ++i)
                if (short_of(at, reach, ours, spots[i], spot_channel[i], theirs[i], steps[i]) == 0) closable = true;
        };
        check(seen::self_position(), me);
        for (std::size_t i = 0; i < allies.size(); ++i)
            if (allies[i] != me) check(ally_tiles[i], allies[i]);
        const bool shrank = continued && gap < catch_gap_last;
        catch_miss = closable || shrank ? 0 : (continued ? catch_miss : 0) + 1;
        catch_gap_last = gap;
    }
    if (!continued || gap < team_gap_best) {
        team_gap_best = gap;
        team_gap_since = now;
    }
    team_gap_seen = now;
    if (current_mode == FIGHT && team_chase_until <= now && now - team_gap_since >= TEAM_CHASE_ROUNDS) {
        team_chase_until = now + TEAM_CHASE_GIVEUP;
        NOTE(format("team chase dropped: no closer than %d tiles in %d rounds", team_gap_best, TEAM_CHASE_ROUNDS));
    } else if (CATCH_MODEL && current_mode == FIGHT && team_chase_until <= now && catch_miss >= CATCH_DROP_ROUNDS) {
        team_chase_until = now + TEAM_CHASE_GIVEUP;
        NOTE(format("team chase dropped: nobody can close the %d tiles (%d rounds)", gap, CATCH_DROP_ROUNDS));
    }
}

bool Bot::chase_dropped(const bc::champion& me, const bc::champion& enemy) {
    // In reach or channelling it is always a target. Out of reach, our best gap to it is kept; one not improved for
    // CHASE_PATIENCE_ROUNDS rounds drops the chase for CHASE_GIVEUP rounds. A chase not looked at last round starts over.
    const int now = static_cast<int>(bc::round_number());
    const auto k = static_cast<std::size_t>(champ_id(enemy.champion_name())) % chase_best.size();
    if (within(me, enemy, me.attack_range()) || channelling(enemy)) {
        chase_seen[k] = 0;
        return false;
    }
    if (chase_drop[k] > now) return true;
    const int gap = distance(me.position(), enemy.position());
    if (chase_seen[k] == now) return false;
    if (chase_seen[k] != now - 1 || gap < chase_best[k]) {
        chase_best[k] = gap;
        chase_since[k] = now;
    }
    chase_seen[k] = now;
    if (now - chase_since[k] < CHASE_PATIENCE_ROUNDS) return false;
    chase_drop[k] = now + CHASE_GIVEUP;
    NOTE(format("chase dropped: no closer to %s in %d rounds", at(enemy.position()).c_str(), CHASE_PATIENCE_ROUNDS));
    return true;
}

std::pair<bool, std::string> Bot::chase_line(const bc::champion& me, const bc::champion& enemy) {
    // Rounds for us to close in and land enough basic attacks (allies in reach help), against its walk home.
    const auto position = enemy.position();
    std::vector<bc::champion> allies;
    for (const auto& a : seen::champions(Team::ally))
        if (distance(a.position(), position) <= a.attack_range() + 1) allies.push_back(a);
    if (std::find(allies.begin(), allies.end(), me) == allies.end()) allies.push_back(me);
    // v134 (ITEM_DAMAGE): Skirmish and Collectors on our side, Durability on theirs.
    PySum total;
    for (const auto& a : allies) total.add(a.strength() * champ_dmg_mult(owned_of(a).passives));
    const double damage = total.value() * champ_taken_mult(owned_of(enemy).passives);
    const int home = steps_to_enemy_spawn(position);
    if (damage <= 0) {
        std::string text;
        LOG(text = at(position) + " drop: we deal no damage";);
        return {false, text};
    }
    int approach = std::max(0, distance(me.position(), position) - me.attack_range());
    // v119 (CATCH_MODEL): walking never closes on someone walking home; a gap our dash and stun leave is never closed.
    if (CATCH_MODEL && approach > 0 && catch_short(me, enemy) > 0) {
        std::string text;
        LOG(text = at(position) + format(" drop: home in %d, out of our dash and stun", home););
        return {false, text};
    }
    if (CATCH_MODEL) approach = std::min(approach, 1);
    const double rounds = approach + std::ceil(enemy.health() / damage);
    const bool keep = rounds <= home;
    std::string text;
    LOG(text = at(position) + (keep ? format(" chase: dies in %.0f, home in %d", rounds, home)
                                    : format(" drop: home in %d, dies in %.0f", home, rounds)););
    return {keep, text};
}

std::optional<vec2> Bot::enemy_step(const bc::champion& enemy) {
    // From track_enemies: its step between last round's sighting and this one's, one tile per axis at most.
    int ident;
    try {
        ident = cached_ident(enemy) - 1;
    } catch (const bc::error&) {
        return std::nullopt;
    }
    const auto k = static_cast<std::size_t>(ident) % enemy_at.size();
    const int now = static_cast<int>(bc::round_number());
    if (enemy_at_round[k] != now || enemy_was_round[k] != now - 1) return std::nullopt;
    const vec2 d = enemy_at[k] - enemy_was[k];
    if (d.x == 0 && d.y == 0) return std::nullopt;
    return vec2{(d.x > 0) - (d.x < 0), (d.y > 0) - (d.y < 0)};
}

std::size_t Bot::lead_path(vec2 from, vec2 step, int n, vec2* out) {
    // A step that went down its walk field home keeps going home (down the field, the old heading first on ties);
    // any other keeps its heading, sliding along walls and holes, and stops where neither axis is free.
    const auto& terrain = seen::terrain();
    const int home_here = steps_to_enemy_spawn(from);
    const bool home = home_here < steps_to_enemy_spawn(from - step);
    auto open = [&](vec2 t) { return bc::map::is_in_bounds(t) && !terrain.is_set(t); };
    static constexpr std::array<vec2, 8> dirs{vec2{0, -1}, vec2{1, -1}, vec2{1, 0}, vec2{1, 1},
                                              vec2{0, 1},  vec2{-1, 1}, vec2{-1, 0}, vec2{-1, -1}};
    vec2 at = from, d = step;
    int here = home_here;
    std::size_t m = 0;
    for (int i = 0; i < n; ++i) {
        vec2 next = at;
        if (home) {
            int best = here;
            std::int64_t best_dot = -10;
            for (const auto dir : dirs) {
                const vec2 t = at + dir;
                if (!bc::map::is_in_bounds(t)) continue;
                const int v = steps_to_enemy_spawn(t);  // walls and holes are unreachable in the field
                const auto dot = dir.dot(d);
                if (v < best || (v == best && v < here && dot > best_dot)) best = v, best_dot = dot, next = t;
            }
            here = best;
        } else if (open(at + d)) {
            next = at + d;
        } else if (d.x != 0 && d.y != 0 && open(at + vec2{d.x, 0})) {
            next = at + vec2{d.x, 0};
        } else if (d.x != 0 && d.y != 0 && open(at + vec2{0, d.y})) {
            next = at + vec2{0, d.y};
        }
        if (next == at) break;
        d = next - at;
        at = next;
        out[m++] = at;
    }
    return m;
}

std::optional<vec2> Bot::chase_step(const bc::champion& target, int reach) {
    if (!CHASE_STEP) return std::nullopt;
    const auto step = enemy_step(target);
    if (!step) return std::nullopt;
    const vec2 from = target.position(), here = seen::self_position(), next = from + *step;
    const int gap = distance(here, from);
    if (gap <= reach || distance(here, next) < gap) return std::nullopt;
    if (!bc::map::is_in_bounds(next) || seen::terrain().is_set(next)) return std::nullopt;
    LOG(note(format("chase step: %s heading %d,%d, aim at %s", at(from).c_str(), step->x, step->y, at(next).c_str())););
    return next;
}

bool Bot::retreat(const bc::champion& me) {
    // v105 (CAMP_EXIT_REACT): awake camps hitting us while we fall back: out of them first, away from the enemy.
    if (CAMP_EXIT_REACT && me.champion_name() != Champ::willow && !near_enemies().empty()
        && camp_exit(me, near_enemies(), "retreat"))
        return true;
    const auto& enemies = seen::champions(Team::enemy);
    last_retreat_round = static_cast<int>(bc::round_number());
    // v131 (RETREAT_SAFE): the enemies' dash and spell reach, not the flat THREAT_RANGE.
    const auto zone = RETREAT_SAFE ? recall_zone(enemies, false) : threat_zone(enemies, false);
    if (!zone.is_set(me.position())) {
        farm(me, &zone, "retreat");
        LOG(branch_why = "out of enemy reach, farm around it; " + branch_why;);
        return true;
    }

    // Melee may swing at something already in range before stepping out; nobody dashes in.
    std::optional<Target> swung;
    if (me.attack_range() <= 1) {
        auto sorted = enemies;
        std::stable_sort(sorted.begin(), sorted.end(), [](const auto& a, const auto& b) { return a.health() < b.health(); });
        for (const auto& target : sorted)
            if (attack_if_in_range(target)) {
                swung = Target{target};
                break;
            }
        if (!swung)
            for (const auto& target : group_camps())
                if (attack_if_in_range(target)) {
                    swung = Target{target};
                    break;
                }
    }
    // v141 (SPAWN_RETREAT): within SPAWN_RETREAT_STEPS walking steps of our spawn, onto it (it heals us to full at turn
    // end); already on it, hold there. Either way hit the weakest enemy in reach if the attack is still ours.
    if (SPAWN_RETREAT && steps_to_ally_spawn(seen::self_position()) <= SPAWN_RETREAT_STEPS) {
        const auto& home = seen::ally_spawn();
        const bool on = home.is_set(seen::self_position());
        if (on || get_away(home)) {
            if (!swung) {
                std::vector<bc::champion> near;
                for (const auto& e : enemies)
                    if (within(me, e, me.attack_range())) near.push_back(e);
                const auto target = lowest(near);
                if (target && attack_if_in_range(*target)) swung = Target{*target};
            }
            WHY("retreat.spawn", std::string(on ? "inside enemy reach on our spawn: hold, it heals us"
                                                : "inside enemy reach near our spawn: onto it")
                                     + (swung ? ", hit what was in reach" : ""));
            return true;
        }
    }
    // v138 (RETREAT_LEAD): the goal also keeps out of reach of where the enemies coming at us (or across) will stand.
    std::optional<bc::bitboard> led;
    if (RETREAT_LEAD) {
        const vec2 here = seen::self_position();
        const auto& tiles = seen::positions(Team::enemy);
        for (std::size_t i = 0; i < enemies.size() && i < tiles.size(); ++i) {
            const auto step = enemy_step(enemies[i]);
            if (!step || distance(here, tiles[i] + *step) > distance(here, tiles[i])) continue;
            std::array<vec2, RETREAT_LEAD_STEPS> path;
            const auto n = lead_path(tiles[i], *step, RETREAT_LEAD_STEPS, path.data());
            if (n == 0) continue;
            int reach = THREAT_RANGE;
            if (RETREAT_SAFE && RECALL_SPELL_CAUTION) reach = std::max(reach, recall_reach(enemies[i]) + RECALL_SPELL_STEP);
            if (!led) led = zone;
            for (std::size_t j = 0; j < n; ++j) led->draw_chebyshev(path[j], static_cast<float>(reach));
        }
        if (led) NOTE("retreat lead: keep out of where the enemies coming at us will be");
    }
    const auto& ahead = led ? *led : zone;
    // v131 (RETREAT_SAFE): toward the nearest ally already out of reach, else simply out of reach.
    bool stepped = false, kited = false;
    if (RETREAT_SAFE) {
        std::optional<vec2> safe_ally;
        for (const auto& a : seen::champions(Team::ally))
            if (a != me && !channelling(a) && !zone.is_set(a.position())
                && (!safe_ally || distance(me.position(), a.position()) < distance(me.position(), *safe_ally)))
                safe_ally = a.position();
        if (safe_ally) {
            const auto around = tiles_around(*safe_ally, static_cast<float>(RETREAT_ALLY_TILES));
            auto goal = around & ~ahead;
            if (goal.is_empty() && led) goal = around & ~zone;
            kited = !goal.is_empty() && vesper_retreat(me, goal);
            stepped = kited || (!goal.is_empty() && get_away(goal));
        }
    }
    auto out = ~ahead;
    if (led && out.is_empty()) out = ~zone;
    if (!stepped) kited = vesper_retreat(me, out);
    if (kited) return true;
    if (!stepped && !get_away(out) && !(led && get_away(~zone))) {
        std::vector<bc::champion> near;
        for (const auto& e : enemies)
            if (within(me, e, me.attack_range())) near.push_back(e);
        const auto target = lowest(near);
        if (target && attack_if_in_range(*target)) swung = Target{*target};
    }
    WHY("retreat.out", std::string("inside enemy reach: step out") + (swung ? ", hit what was in reach first" : ""));
    return true;
}

bool Bot::stray_heat(const bc::champion& me, int s) {
    // v120 (STRAY_EXIT): awake camps of groups other than slot s near us (a stray area hit or a passing fight woke them),
    // their reach, and whether one hits us (nearest champion in its reach, a tie counting as us). Same raw state read as
    // camp_heat; worked out once per vision list and slot. Returns whether any is near.
    const auto round = bc::round_number();
    if (stray_round == round && stray_gen == seen::generation() && stray_slot == s) return !stray_reach.is_empty();
    stray_round = round;
    stray_gen = seen::generation();
    stray_slot = s;
    stray_hit = false;
    stray_reach.clear();
    const auto position = seen::self_position();
    for (const auto& camp : seen::camps()) {
        const int reach = camp_kind_reach(camp.kind());
        if (reach <= 0) continue;
        const auto spot = camp.spawn_position();
        const int gap = distance(position, spot);
        if (gap > reach + 2 || slot_at(spot) == s) continue;
        std::uint32_t state = 0;
        if (bc::detail::sdk_camp_state(static_cast<std::uint32_t>(static_cast<bc::handle_camp_t>(camp)), &state) != 0 || state != 1) continue;
        stray_reach.draw_chebyshev(spot, static_cast<float>(reach));
        if (gap > reach || stray_hit) continue;
        stray_hit = !other_nearer(me, spot, gap);
    }
    return !stray_reach.is_empty();
}

bool Bot::stray_exit(const bc::champion& me, int s, const std::vector<bc::camp>& camps, const bc::bitboard* avoid,
                     const std::string& stem) {
    // v120 (STRAY_EXIT): hit by another group's awake camps while farming s: one step to a tile that still reaches a camp
    // of s and is out of theirs, else one step out of theirs (they sleep after two rounds with nobody in reach), else
    // toward the nearest such tile. The farm turn goes on after the step (a melee still swings at what it reaches).
    if (!stray_heat(me, s) || !stray_hit || exit_turn_steps(me) <= 0) return false;
    const auto position = me.position();
    const int reach = me.attack_range();
    fast::not_into(stray_reach, stray_out);
    stray_goal.clear();
    for (const auto& c : camps) stray_goal.draw_chebyshev(c.position(), static_cast<float>(reach));
    stray_goal &= stray_out;
    auto& step = stray_step;
    const char* how = "a tile in reach of our camp, out of the other group's";
    // v133 (GO_EXIT_DASH): the first two tests take what this turn reaches, a dash included.
    const float near = static_cast<float>(std::max(1, exit_turn_steps(me)));
    fast::around_into(position, near, step);
    step &= stray_goal;
    bool moved = !step.is_empty() && leave_camps(step, avoid);
    if (!moved) {
        fast::around_into(position, near, step);
        step &= stray_out;
        how = "out of the other group's reach";
        moved = !step.is_empty() && leave_camps(step, avoid);
    }
    if (!moved) {
        fast::around_into(position, static_cast<float>(MONOLITH_REACH + 1), step);
        step &= stray_out;
        how = "toward the edge of the other group's reach";
        moved = !step.is_empty() && leave_camps(step, avoid);
    }
    if (moved) NOTE(stem + ": another group's awake camps hit us: step " + how);
    return moved;
}

int Bot::camp_heat(const bc::champion& me) {
    // v105 (CAMP_EXIT_REACT): awake camps within reach of where an exit could take us. A camp's kind and spawn tile
    // are held client-side and camps never move, so the only engine read is each near camp's state (a raw read: a
    // camp hidden in a bush fails it instead of throwing). Worked out once per vision list.
    const auto round = bc::round_number();
    if (heat_round == round && heat_gen == seen::generation()) return heat_hit;
    heat_round = round;
    heat_gen = seen::generation();
    heat_hit = 0;
    heat_any = false;
    heat_reach.clear();
    const auto position = seen::self_position();
    for (const auto& camp : seen::camps()) {
        const int reach = camp_kind_reach(camp.kind());
        if (reach <= 0) continue;
        const auto spot = camp.spawn_position();
        const int gap = distance(position, spot);
        if (gap > reach + CAMP_EXIT_STEPS + 2) continue;
        std::uint32_t state = 0;
        if (bc::detail::sdk_camp_state(static_cast<std::uint32_t>(static_cast<bc::handle_camp_t>(camp)), &state) != 0 || state != 1) continue;
        heat_any = true;
        heat_reach.draw_chebyshev(spot, static_cast<float>(reach));
        if (gap > reach) continue;
        // It hits the nearest champion in its reach, ours or theirs (a tie counts as us).
        if (!other_nearer(me, spot, gap)) heat_hit += camp_kind_strength(camp.kind());
    }
    return heat_hit;
}

void Bot::remember_hitting_camps(const bc::champion& me) {
    // v131 (CAMP_WAIT): the awake camps hitting us now (nearest champion in their reach), before we step out of them.
    const int now = static_cast<int>(bc::round_number());
    const auto position = me.position();
    for (const auto& camp : seen::camps()) {
        const int reach = camp_kind_reach(camp.kind());
        if (reach <= 0) continue;
        const auto spot = camp.spawn_position();
        const int gap = distance(position, spot);
        if (gap > reach) continue;
        std::uint32_t state = 0;
        if (bc::detail::sdk_camp_state(static_cast<std::uint32_t>(static_cast<bc::handle_camp_t>(camp)), &state) != 0 || state != 1) continue;
        bool known = false;
        for (auto& w : wait_camps)
            if (w.spot == spot) { w.round = now; known = true; }
        if (!known) wait_camps.push_back({spot, reach, now});
    }
}

const bc::bitboard* Bot::camp_wait_avoid(const bc::champion& me, const bc::bitboard* avoid) {
    // v131 (CAMP_WAIT): the reach of each remembered camp still awake and still ours to be hit by, added to `avoid`.
    if (wait_camps.empty()) return avoid;
    const int now = static_cast<int>(bc::round_number());
    const int farm_s = route.empty() ? -1 : slot % static_cast<int>(route.size());
    const int farmed_s = now - farmed_round <= FARMED_ROUNDS ? farmed_slot : -1;
    std::erase_if(wait_camps, [&](const WaitCamp& w) { return now - w.round > CAMP_WAIT_ROUNDS; });
    wait_zone.clear();
    bool any = false;
    const auto& enemies = seen::champions(Team::enemy);
    for (auto it = wait_camps.begin(); it != wait_camps.end();) {
        // The group we farm is ours to engage - unless an enemy champion is near: we left it because of that enemy and the
        // camp together, and walking back in only repeats the exit (v131 debug: 576 fight.exit and 494 retreat.exit loops).
        const int cs = slot_at(it->spot);
        if (cs >= 0 && (cs == farm_s || cs == farmed_s) && (!CAMP_WAIT_TARGET_FIGHT || near_enemies().empty())) { ++it; continue; }
        bool seen_camp = false, awake = true;
        for (const auto& camp : seen::camps()) {
            if (camp.spawn_position() != it->spot) continue;
            seen_camp = true;
            std::uint32_t state = 0;
            awake = bc::detail::sdk_camp_state(static_cast<std::uint32_t>(static_cast<bc::handle_camp_t>(camp)), &state) == 0 && state == 1;
            break;
        }
        if (seen_camp && !awake) { it = wait_camps.erase(it); continue; }  // asleep again: free to go back
        bool on_enemy = false;
        for (const auto& e : enemies)
            if (distance(e.position(), it->spot) < it->reach) on_enemy = true;  // it hits them, not us at its edge
        if (!on_enemy) {
            wait_zone.draw_chebyshev(it->spot, static_cast<float>(it->reach));
            any = true;
        }
        ++it;
    }
    if (!any || wait_zone.is_set(me.position())) return avoid;
    wait_avoid = wait_zone;
    if (avoid) wait_avoid |= *avoid;
    NOTE(format("camp wait: keeps out of %d awake camps we left until they sleep or turn on an enemy", static_cast<int>(wait_camps.size())));
    return &wait_avoid;
}

void Bot::build_walk_guard(const bc::champion& me) {
    // v132f/g (WALK_GUARD): read once a turn after vision. Reacting to an enemy, every awake camp in sight; otherwise
    // those no ally stands in, leaving out the groups we farm. A raw state read (a camp hidden in a bush fails it).
    guard_zone.clear();
    const bool react = !near_enemies().empty();
    const int now = static_cast<int>(bc::round_number());
    const int farm_s = route.empty() ? -1 : slot % static_cast<int>(route.size());
    const int farmed_s = now - farmed_round <= FARMED_ROUNDS ? farmed_slot : -1;
    const auto& allies = seen::champions(Team::ally);
    const auto& ally_tiles = seen::positions(Team::ally);
    const auto position = seen::self_position();
    for (const auto& camp : seen::camps()) {
        const auto spot = camp.spawn_position();
        // v132h (GUARD_NEAR): camps never move; the spawn tile is held client-side, so far camps cost no engine read.
        if (distance(position, spot) > MONOLITH_REACH + GUARD_NEAR) continue;
        const int reach = camp_kind_reach(camp.kind());
        if (reach <= 0 || distance(position, spot) > reach + GUARD_NEAR) continue;
        if (!react) {
            const int cs = slot_at(spot);
            if (cs >= 0 && (cs == farm_s || cs == farmed_s)) continue;
            bool tanked = false;
            for (std::size_t i = 0; i < allies.size() && !tanked; ++i)
                tanked = distance(ally_tiles[i], spot) <= reach && allies[i] != me;
            if (tanked) continue;
        }
        std::uint32_t state = 0;
        if (bc::detail::sdk_camp_state(static_cast<std::uint32_t>(static_cast<bc::handle_camp_t>(camp)), &state) != 0 || state != 1) continue;
        guard_zone.draw_chebyshev(spot, static_cast<float>(reach));
        guard_any = true;
    }
    walk_guard = guard_any ? &guard_zone : nullptr;
}

bool Bot::area_ok(int s, int farm_s, bool engaged) {
    // v135: the one rule for what an area hit (HoR's landing, an area spell) may touch. Clusters cannot hit back.
    if (clusters_only[s]) return true;
    // The farm target (or a group we already hit) passes on the team's rules: gate and monolith rules.
    if ((s == farm_s || mid_clear(s)) && allowed(s)) return true;
    // v135 (SOLO_LAND, user): any other group only if we beat it alone, and never while awake camps already hit us:
    // a second group's damage would stack on the first.
    return SOLO_LAND && !engaged && solo_win(s);
}

bool Bot::solo_win(int s) {
    const auto& me = bc::self();
    // Every live camp of the group must be in sight (an unseen one is unknown: no landing beside it).
    static bc::bitboard sight{bc::uninitialized};
    bc::vision::visible_tiles_into(sight);
    for (const auto t : spawn_lists[s])
        if (!sight.is_set(t)) return false;
    double left = 0, strength = 0;
    for (const auto& c : seen::camps()) {
        if (slot_at(c.spawn_position()) != s) continue;
        left += c.health();
        strength += c.strength();
    }
    if (strength <= 0) return true;
    const auto mine = owned_of(me).passives;
    const double hit = me.strength() * camp_dmg_mult(mine);
    if (hit <= 0) return false;
    double landing = 0;
    if (const auto dash = ability_of(me, Kind::dash)) landing = spell_damage(dash) * camp_dmg_mult(mine);
    const double rounds = std::ceil(std::max(0.0, left - landing) / hit);
    const double take = strength * rounds * camp_taken_mult(mine);
    double shield = 0;
    try {
        const auto buffs = me.buffs();
        if (buffs.has_shield()) shield = buffs.get<bc::buff_shield>().value;
    } catch (const std::exception&) {
    }
    return me.health() + shield - take >= me.max_health() * FINISH_HEALTH;
}

void Bot::build_land_bar(const bc::champion& me) {
    // Only with the dash ready, and only groups whose tiles a landing can touch: the far ones cost no read.
    const auto dash = ability_of(me, Kind::dash);
    if (!dash || !dash->is_ready()) return;
    const int radius = static_cast<int>(ability_radius(*dash));
    if (radius <= 0 || route.empty()) return;
    const int span = static_cast<int>(ability_range(*dash)) + radius;
    const auto position = seen::self_position();
    const int now = static_cast<int>(bc::round_number());
    const int farm_s = slot % static_cast<int>(route.size());
    const bool engaged = camp_heat(me) > 0;
    static bc::bitboard sight{bc::uninitialized}, held{bc::uninitialized};
    bool read = false, any = false;
    land_bar_board.clear();
    if (group_radius.size() != route.size()) {
        group_radius.assign(route.size(), 0);
        for (int s = 0; s < static_cast<int>(route.size()); ++s)
            for (const auto t : spawn_lists[s]) group_radius[s] = std::max(group_radius[s], distance(centers[s], t));
    }
    for (int s = 0; s < static_cast<int>(route.size()); ++s) {
        if (distance(position, centers[s]) > span + group_radius[s]) continue;
        if (clusters_only[s] || ready_at[s] > now || rewards.count(s) || s == capture_slot) continue;
        bool near = false;
        for (const auto t : spawn_lists[s])
            if (distance(position, t) <= span) { near = true; break; }
        if (!near || area_ok(s, farm_s, engaged)) continue;
        if (!read) {
            bc::vision::visible_tiles_into(sight);
            bc::vision::camp_tiles_into(held);
            read = true;
        }
        // A tile in sight with no camp on it is a dead camp: nothing there to wake.
        for (const auto t : spawn_lists[s])
            if (!sight.is_set(t) || held.is_set(t)) {
                land_bar_board.draw_chebyshev(t, static_cast<float>(radius));
                any = true;
            }
    }
    land_bar = any ? &land_bar_board : nullptr;
    if (any) NOTE("safe landing: the dash keeps its area off groups we are not farming");
}

bool Bot::ally_tanked_camp(const bc::champion& me) {
    // v132d/e (CAMP_SHARE): read only for camps reaching us (a raw state read, as camp_heat).
    const auto position = me.position();
    for (const auto& camp : seen::camps()) {
        const int reach = camp_kind_reach(camp.kind());
        if (reach <= 0) continue;
        const auto spot = camp.spawn_position();
        const int gap = distance(position, spot);
        if (gap > reach) continue;
        int ally_gap = 1 << 20, enemy_gap = 1 << 20;
        bool ally_first = false;  // v148-normal (CAMP_TIE_ORDER): an ally level with us acting first takes it
        for (const auto& a : seen::champions(Team::ally))
            if (a != me) {
                const int g = distance(a.position(), spot);
                ally_gap = std::min(ally_gap, g);
                if (g == gap && acts_before_me(a)) ally_first = true;
            }
        for (const auto& e : seen::champions(Team::enemy)) enemy_gap = std::min(enemy_gap, distance(e.position(), spot));
        if (ally_gap > gap || (ally_gap == gap && !ally_first) || enemy_gap <= ally_gap) continue;  // it hits us, or an enemy
        std::uint32_t state = 0;
        if (bc::detail::sdk_camp_state(static_cast<std::uint32_t>(static_cast<bc::handle_camp_t>(camp)), &state) != 0 || state != 1) continue;
        return true;
    }
    return false;
}

bool Bot::camp_wait_fight(const bc::champion& me) {
    // v132 (CAMP_WAIT_FIGHT): unlike camp_wait_avoid, an ally or enemy inside a camp's reach does not free it: the camp
    // hits whoever is nearest, and that is us again the moment they step out. Only sleep (or the round limit) frees it.
    if (wait_camps.empty() && !(WALK_GUARD && guard_any)) return false;
    const int now = static_cast<int>(bc::round_number());
    std::erase_if(wait_camps, [&](const WaitCamp& w) { return now - w.round > CAMP_WAIT_ROUNDS; });
    fight_wait.clear();
    bool any = false;
    for (auto it = wait_camps.begin(); it != wait_camps.end();) {
        bool asleep = false;
        for (const auto& camp : seen::camps()) {
            if (camp.spawn_position() != it->spot) continue;
            std::uint32_t state = 0;
            asleep = bc::detail::sdk_camp_state(static_cast<std::uint32_t>(static_cast<bc::handle_camp_t>(camp)), &state) == 0 && state != 1;
            break;
        }
        if (asleep) { it = wait_camps.erase(it); continue; }
        if (now - it->round > CAMP_WAIT_FIGHT_ROUNDS) { ++it; continue; }  // v132b: farm() may still keep out of it
        fight_wait.draw_chebyshev(it->spot, static_cast<float>(it->reach));
        any = true;
        ++it;
    }
    // v132f/g (WALK_GUARD): and the awake camps of this turn's guard.
    if (WALK_GUARD && guard_any) {
        fight_wait |= guard_zone;
        any = true;
    }
    return any && !fight_wait.is_set(me.position());
}

bool Bot::foreign_camp_hit(const bc::champion& me) {
    // v129 (CAMP_EXIT_ALWAYS): only camps in their own reach of us are read (a raw state read, as camp_heat).
    const int round_now = static_cast<int>(bc::round_number());
    const int farm_s = route.empty() ? -1 : slot % static_cast<int>(route.size());
    const int farmed_s = round_now - farmed_round <= FARMED_ROUNDS ? farmed_slot : -1;
    const auto position = seen::self_position();
    const int own_reach = me.attack_range();
    bool hit = false;
    for (const auto& camp : seen::camps()) {
        const int reach = camp_kind_reach(camp.kind());
        if (reach <= 0) continue;
        const auto spot = camp.spawn_position();
        const int gap = distance(position, spot);
        if (gap > reach) continue;
        const int cs = slot_at(spot);
        if (cs >= 0 && (cs == farm_s || cs == farmed_s)) {
            // Farming one of ours: farm() steps out of the others while still reaching it.
            if (gap <= own_reach) return false;
            continue;
        }
        if (hit) continue;
        std::uint32_t state = 0;
        if (bc::detail::sdk_camp_state(static_cast<std::uint32_t>(static_cast<bc::handle_camp_t>(camp)), &state) != 0 || state != 1) continue;
        hit = !other_nearer(me, spot, gap);
    }
    return hit;
}

bool Bot::camp_exit(const bc::champion& me, const std::vector<bc::champion>& enemies, const std::string& stem) {
    // v105 (CAMP_EXIT_REACT): reacting to an enemy while awake camps hit us, leave the camps first. Every test is a
    // board, and the walking reach grows one step at a time with the engine's own search (one call a step), so no
    // guest loop runs over tiles. Tests: out = beyond every awake camp's reach; away = farther from every near enemy
    // than we stand now; fast = within one step of the quickest way out. v133 (GO_EXIT_DASH): a ready dash counts.
    const int turn = exit_turn_steps(me);
    if (!CAMP_EXIT_REACT || turn <= 0) return false;
    const auto position = me.position();
    // v132f/g (CAMP_REACT_ANY): any awake camp reaching us, not only one hitting us.
    if (camp_heat(me) <= 0 && !(CAMP_REACT_ANY && heat_any && heat_reach.is_set(position))) return false;
    blocked_tiles_into(exit_blocked);
    fast::not_into(heat_reach, exit_out);
    int quickest = -1;
    for (int k = 1; k <= CAMP_EXIT_STEPS; ++k) {
        exit_blocked.reachable_tiles_into(position, static_cast<std::uint16_t>(k), exit_reach);
        fast::and_into(exit_reach, exit_out, exit_goal);
        if (!exit_goal.is_empty()) {
            quickest = k;
            break;
        }
    }
    // Fast: one step more than the quickest exit, or a single step when none is in reach. The window for a slower
    // exit that also moves away is one step further still.
    // v133 (GO_EXIT_DASH): with a dash every exit this turn reaches counts as fast.
    const int fast_steps = std::max(quickest < 0 ? 1 : quickest + 1, turn);
    const int window = std::max(quickest < 0 ? 1 : quickest + 2, quickest < 0 ? turn : turn + 1);
    exit_blocked.reachable_tiles_into(position, static_cast<std::uint16_t>(fast_steps), exit_fast);
    exit_blocked.reachable_tiles_into(position, static_cast<std::uint16_t>(window), exit_reach);
    exit_out &= exit_reach;
    exit_away.clear();
    for (const auto& enemy : enemies) {
        const auto spot = enemy.position();
        exit_away.draw_chebyshev(spot, static_cast<float>(distance(position, spot)));
    }
    fast::not_into(exit_away, exit_away);
    // The nearest tile passing all three tests, else out and away, else out fast, else away fast.
    const char* passed = "out, away and fast";
    fast::and_into(exit_out, exit_away, exit_goal);
    exit_goal &= exit_fast;
    if (exit_goal.is_empty()) {
        passed = "out and away";
        fast::and_into(exit_out, exit_away, exit_goal);
    }
    if (exit_goal.is_empty()) {
        passed = "out fast";
        fast::and_into(exit_out, exit_fast, exit_goal);
    }
    if (exit_goal.is_empty()) {
        passed = "away fast, no way out in reach";
        fast::and_into(exit_away, exit_fast, exit_goal);
    }
    exit_goal.set(position, false);
    if (exit_goal.is_empty()) return false;
    // A melee hits back on the way: what is in reach now, and again from the new tile if the swing is still free.
    const bool melee = me.attack_range() <= 1;
    auto swing = [&] {
        if (!melee || !me.can_attack()) return false;
        for (const auto& enemy : seen::champions(Team::enemy))
            if (attack_if_in_range(enemy)) return true;
        return false;
    };
    if (CAMP_WAIT) remember_hitting_camps(me);
    const bool hit_first = swing();
    if (!(GO_EXIT_DASH ? leave_camps(exit_goal) : get_away(exit_goal))) {
        NOTE(std::string("camp exit (") + passed + "): no step found");
        return hit_first;
    }
    const bool hit_after = !hit_first && swing();
    WHY(stem + ".exit", std::string("awake camps hit us: leave them first (") + passed + ")"
                            + (hit_first || hit_after ? ", hit back on the way" : ""));
    return true;
}

std::vector<bc::camp> Bot::group_camps() {
    if (route.empty()) return {};
    return camps_at(slot % static_cast<int>(route.size()));
}

std::vector<bc::camp> Bot::camps_at(int s) {
    // A camp belongs to the group whose spawn tile it started on; the slot grid answers that
    // without an engine read per camp.
    std::vector<bc::camp> result;
    for (const auto& camp : seen::camps())
        if (slot_at(camp.spawn_position()) == s) result.push_back(camp);
    return result;
}

int Bot::walk_target(const bc::champion& me, int s) {
    const auto position = me.position();
    if (walkable(s, position)) return -1;
    const bool dasher = ability_of(me, Kind::dash).has_value();
    if (dasher) {
        int nearest = distance(position, spawn_lists[s].front());
        for (const auto t : spawn_lists[s]) nearest = std::min(nearest, distance(position, t));
        if (nearest <= WALK_CLAIM_DASH) return -1;
    }
    const int now = static_cast<int>(bc::round_number());
    // v103 (FIX_POCKET): in a hole pocket only a group that is up now; a wait there keeps us from the team.
    const bool pocket = FIX_POCKET && dasher && !home_island().is_set(position);
    int best = -1;
    double best_rate = 0;
    for (int o = 0; o < static_cast<int>(route.size()); ++o) {
        // Only groups whose walking field is built (build_fields() fills them from spare fuel).
        if (o == s || is_monolith(o) || loads[o] > WALK_TARGET_LOAD || !zone_open(o)) continue;
        if (pocket && ready_at[o] > now) continue;
        if (o >= static_cast<int>(goal_fields.size()) || !goal_fields[o]) continue;
        const auto steps = goal_fields[o]->field.get(position);
        if (steps == bc::distance_field::unreachable) continue;
        const double rate = route_rate_steps(o, static_cast<int>(steps));
        if (best < 0 || rate > best_rate) {
            best = o;
            best_rate = rate;
        }
    }
    if (best >= 0) NOTE(format("no walking path to s%d: farm s%d instead", s, best));
    else if (pocket) NOTE(format("in a hole pocket, nothing up here: back out to s%d", s));
    return best;
}

bool Bot::clear_collector_path(const bc::champion& me, int cs, bc::bitboard& goal) {
    // The collector's walking path to the reward: down the group's walk field (terrain steps to its spawn tiles) from
    // the collector's tile. Those tiles leave the waiting ring, so our bodies never stand between it and the reward.
    if (cs >= static_cast<int>(goal_fields.size()) || !goal_fields[cs]) return false;
    const auto& field = goal_fields[cs]->field;
    const auto& allies = seen::champions(Team::ally);
    const auto& tiles = seen::positions(Team::ally);
    std::optional<vec2> from;
    for (std::size_t i = 0; i < allies.size() && i < tiles.size(); ++i)
        if (allies[i] != me && cached_ident(allies[i]) == collector) from = tiles[i];
    if (!from) return false;
    auto at = *from;
    auto steps = field.get(at);
    if (steps == bc::distance_field::unreachable) return false;
    const int w = static_cast<int>(bc::map::width()), h = static_cast<int>(bc::map::height());
    bool cut = false;
    for (int guard = 0; guard < 40 && steps > 0; ++guard) {
        std::optional<vec2> next;
        for (int dx = -1; dx <= 1 && !next; ++dx)
            for (int dy = -1; dy <= 1 && !next; ++dy) {
                if (!dx && !dy) continue;
                const vec2 t{at.x + dx, at.y + dy};
                if (t.x < 0 || t.y < 0 || t.x >= w || t.y >= h) continue;
                if (field.get(t) == steps - 1) next = t;
            }
        if (!next) break;
        at = *next;
        steps = field.get(at);
        if (goal.is_set(at)) {
            goal.set(at, false);
            cut = true;
        }
    }
    return cut;
}

bool Bot::stage_capture(const bc::champion& me, int s, bool no_camps, const bc::bitboard* avoid, const std::string& stem) {
    const int cs = capture_slot;
    if (cs == NO_CAPTURE || cs >= static_cast<int>(route.size()) || is_collector(me)) return false;
    const auto position = me.position();
    if (CAPTURE_MOVE_ON) {
        // Only when the team target is still the captured group (nothing else up): keep off its tiles, stay put.
        if (s != cs || !no_camps) return false;
        const auto& tiles = spawn_board(cs);
        if (tiles.is_set(position)) {
            bc::bitboard off;
            off.draw_chebyshev(position, 1);
            off &= ~tiles;
            WHY(stem + ".moveon", format("s%d is the collector's: step off its tiles", cs));
            if (!move_towards_closest(off, avoid)) move_towards_closest(off);
        } else if (PRISM_MOVE_WITH && prism_team() && distance(position, centers[cs]) > PRISM_WITH_RANGE) {
            // v160: stay put only near her.
            WHY(stem + ".with", format("s%d is hers: walk over to her", cs));
            bc::bitboard ring;
            for (const auto tile : spawn_lists[cs]) ring.draw_chebyshev(tile, WAIT_RANGE);
            move_towards_closest(ring & ~tiles, avoid);
        } else {
            WHY(stem + ".moveon", format("s%d is the collector's and nothing else is up: stay off it", cs));
        }
        return true;
    }
    // A reward the collector cannot walk to is not being captured: nothing to wait for (02the-pond: an island cluster).
    if (STAGE_PATH && cs < static_cast<int>(goal_fields.size()) && goal_fields[cs]) {
        const auto& allies = seen::champions(Team::ally);
        const auto& tiles = seen::positions(Team::ally);
        for (std::size_t i = 0; i < allies.size() && i < tiles.size(); ++i)
            if (cached_ident(allies[i]) == collector && goal_fields[cs]->field.get(tiles[i]) == bc::distance_field::unreachable
                && !(DASH_REACH && can_reach(cs, tiles[i], dash_kind(static_cast<Champ>(collector - 1)))))
                return false;
    }
    bc::bitboard goal;
    const char* why = nullptr;
    if (s == cs && no_camps) {
        // Off its tiles, within WAIT_RANGE of them: the reward is captured from there by the collector alone.
        for (const auto tile : spawn_lists[cs]) goal.draw_chebyshev(tile, WAIT_RANGE);
        goal &= ~spawn_board(cs);
        why = "is being captured by an ally: wait off its tiles";
        if (STAGE_PATH && clear_collector_path(me, cs, goal)) why = "is being captured by an ally: wait off its tiles and its path";
    } else if (s != cs && zones_known && zone(cs) != ZONE_SAFE && distance(position, centers[cs]) <= CAPTURE_NEAR + 2) {
        // The next group is beyond CAPTURE_NEAR: wait on the reward's edge nearest it, so the walk on is shortest.
        const int apart = distance(centers[cs], centers[s]);
        if (apart <= CAPTURE_NEAR) return false;
        goal.draw_chebyshev(centers[cs], static_cast<float>(CAPTURE_NEAR));
        bc::bitboard toward;
        toward.draw_chebyshev(centers[s], static_cast<float>(apart - CAPTURE_NEAR));
        goal &= toward;
        goal &= ~spawn_board(cs);
        why = "is being captured outside the safe zone: wait at its edge toward the next group";
    } else {
        return false;
    }
    if (goal.is_empty()) return false;
    if (goal.is_set(position)) {
        WHY(stem + ".stage", format("s%d ", cs) + why + " (here)");
        return true;
    }
    WHY(stem + ".stage", format("s%d ", cs) + why);
    if (!move_towards_closest(goal, avoid)) move_towards_closest(goal);
    return true;
}

bool Bot::stage_group(const bc::champion& me, int s, const std::vector<bc::camp>& camps, const bc::bitboard* avoid,
                      const std::string& stem) {
    if (!HARD_GATE || s >= static_cast<int>(hard_need.size()) || (hard_need[s] <= 0 && !(GOLEM_GATE && is_golem(s)))
        || mid_clear(s))
        return false;
    const auto position = me.position();
    if (distance(position, centers[s]) > PRESENT_STAGE_RANGE) return false;
    const auto p = present_near(centers[s], PRESENT_STAGE_RANGE);
    const int n = std::max(1, p.n);
    // v142 (GOLEM_GATE): golems ask by who is here, and never pass on the fallback floor.
    const int left = std::max(0, hard_need[s] - (HARD_FREE + n)) + golem_left(s, n);
    const bool pass = left == 0 || left <= std::max(p.emblems, GOLEM_GATE && is_golem(s) ? -1 : hard_floor);
    const int now = static_cast<int>(bc::round_number());
    if (stage_slot != s || now - stage_last > 2) {
        stage_slot = s;
        stage_since = now;
    }
    stage_last = now;
    int living = 0;
    for (const auto& a : seen::roster(Team::ally))
        if (a.is_alive()) ++living;
    const bool gathered = n >= std::min(STAGE_MIN, std::max(1, living));
    if (pass && (gathered || now - stage_since >= STAGE_ROUNDS)) return false;
    // Just outside every camp's reach: the camps stay asleep until we open them together.
    bc::bitboard danger, ring;
    for (const auto& c : camps) {
        const int reach = camp_kind_reach(c.kind());
        if (reach <= 0) continue;
        danger.draw_chebyshev(c.spawn_position(), static_cast<float>(reach));
        ring.draw_chebyshev(c.spawn_position(), static_cast<float>(reach + 1));
    }
    ring &= ~danger;
    const std::string why = pass ? format("s%d: waits for the team before opening it (%d here)", s, n)
                                 : format("s%d: the %d of us here fall short of its emblem gate: wait outside it", s, n);
    if (ring.is_empty()) return false;
    if (ring.is_set(position)) {
        WHY(stem + ".stage", why + " (here)");
        return true;
    }
    WHY(stem + ".stage", why);
    if (!move_towards_closest(ring, avoid)) move_towards_closest(ring);
    return true;
}

bool Bot::pocketed(const bc::champion& me) {
    if (!FIX_POCKET || home_island().is_set(me.position())) return false;
    // Types and life read out of sight.
    for (const auto& ally : seen::roster(Team::ally)) {
        const auto kind = ally.champion_name();
        const bool dashes = kind == Champ::vesper_vale || kind == Champ::ren || kind == Champ::issac
                            || kind == Champ::vendetta || kind == Champ::heart_of_ruin;
        if (!dashes && ally.is_alive()) return true;
    }
    return false;
}

bool Bot::pocket_recall(const bc::champion& me, int s, const std::string& stem) {
    const auto position = me.position();
    if (!FIX_POCKET || walkable(s, position) || home_island().is_set(position) || !ability_of(me, Kind::dash)) return false;
    if (bc::fuel_remaining() < HOP_FUEL) return false;
    const auto from_home = steps_to(group_field(s), ally_spawn_list);
    if (!from_home) return false;
    const int by_recall = recall_ticks(owned_of(me).passives) + *from_home;
    const auto hop = hop_steps(position, spawn_board(s));
    if (hop && *hop <= by_recall + POCKET_RECALL_SLACK) return false;
    if (channel_cut || recall_zone(seen::champions(Team::enemy), true).is_set(position)) return false;
    try {
        me.recall();
        trace::act("recall");
        WHY(stem + ".pocket.recall", hop ? format("in a hole pocket: s%d is %d steps by dash, %d by recall", s, *hop, by_recall)
                                         : format("in a hole pocket: no dash route to s%d, %d by recall", s, by_recall));
        return true;
    } catch (const bc::error& error) {
        NOTE("pocket recall refused: " + error_text(error));
        return false;
    }
}

void Bot::farm(const bc::champion& me, const bc::bitboard* avoid, const std::string& stem) {
    if (route.empty()) {
        WHY(stem + ".none", "no route");
        return;
    }
    // v131 (RETREAT_COOL): just after a retreat, farming keeps out of the visible enemies' reach.
    if (RETREAT_COOL && !avoid && stem == "farm" && static_cast<int>(bc::round_number()) - last_retreat_round <= RETREAT_COOL_ROUNDS) {
        const auto& enemies = RETREAT_COOL_NEAR ? near_enemies() : seen::champions(Team::enemy);
        if (!enemies.empty()) {
            cool_zone = recall_zone(enemies, false);
            if (!cool_zone.is_set(me.position())) {
                avoid = &cool_zone;
                NOTE(format("retreat cool-off: farm outside enemy reach (%d rounds since the retreat)",
                            static_cast<int>(bc::round_number()) - last_retreat_round));
            }
        }
    }
    // v131 (CAMP_WAIT): keep out of awake camps we just stepped out of until they sleep or turn on an enemy.
    if (CAMP_WAIT && CAMP_WAIT_FARM) avoid = camp_wait_avoid(me, avoid);
    int s = slot % static_cast<int>(route.size());
    auto camps = camps_at(s);
    // v122 (FARM_KEEP): our planned group is out of sight, but the camps of the group we were just farming dropped out
    // of sight only rounds ago: that group stays the target, so a camp at the edge of sight does not flip us between
    // two groups every step.
    const int round_now = static_cast<int>(bc::round_number());
    if (FARM_KEEP && camps.empty() && keep_slot >= 0 && keep_slot != s && keep_slot < static_cast<int>(route.size())
        && round_now - keep_round <= FARM_KEEP_ROUNDS && ready_at[keep_slot] <= round_now && allowed(keep_slot)) {
        NOTE(format("s%d's camps left sight %d rounds ago: keep it over s%d", keep_slot, round_now - keep_round, s));
        s = keep_slot;
        camps = camps_at(s);
    }
    // v71: a cleared target whose every reward tile already has an ally on it is being captured; one more
    // body adds nothing, so we hit the nearest visible group we may take instead of waiting beside it.
    // Only visible camps are looked at: a fresh pick_target() cost ~20k fuel here.
    if (FIX_TAKEN && camps.empty() && ready_at[s] <= static_cast<int>(bc::round_number())) {
        const auto position = me.position();
        const auto& tiles = spawn_board(s);
        auto open = tiles;
        open &= ~bc::vision::champion_tiles(Team::ally);
        if (!tiles.is_set(position) && open.is_empty()) {
            int other = -1, best = 0;
            for (const auto& c : seen::camps()) {
                const int cs = slot_at(c.spawn_position());
                if (cs < 0 || cs == s || !can_take_camp(c)) continue;
                const int d = distance(position, c.position());
                if (other < 0 || d < best) {
                    other = cs;
                    best = d;
                }
            }
            if (other >= 0) {
                NOTE(format("allies hold every reward tile of s%d: farm s%d meanwhile", s, other));
                s = other;
                camps = camps_at(s);
            }
        }
    }
    // v135 (CAPTURE_STAGE): a reward someone else collects is not ours to walk onto (a body on its tiles adds nothing
    // and the walk/step-off loop cost 23.8k turns in 65 games); outside the safe zone the team waits at its edge.
    if (CAPTURE_STAGE && stage_capture(me, s, camps.empty(), avoid, stem)) return;
    if (FIX_WALK_TARGET) {
        const int other = walk_target(me, s);
        if (other >= 0) {
            s = other;
            camps = camps_at(s);
        } else if (pocket_recall(me, s, stem)) {
            return;
        }
    }
    // v120 (STRAY_EXIT): out of another group's awake camps first; then the walk keeps out of their reach (unless
    // already inside it: the path out would be blocked).
    if (STRAY_EXIT && !camps.empty()) {
        // The step may take the group out of sight: its camps are read again (an old handle throws).
        if (stray_exit(me, s, camps, avoid, stem)) {
            camps = camps_at(s);
            if (camps.empty()) {
                WHY(stem + ".stray", format("s%d ", s) + "out of sight after leaving another group's awake camps");
                return;
            }
        }
        if (stray_heat(me, s) && !stray_reach.is_set(me.position())) {
            fast::copy_into(stray_reach, stray_avoid);
            if (avoid) stray_avoid |= *avoid;
            avoid = &stray_avoid;
        }
    }
    const int reach = me.attack_range();
    const auto center = centers[s];
    // Clusters have no strength, so a passing hit is free and should not be walked past.
    std::vector<bc::camp> passing;
    for (const auto& c : seen::camps())
        if (c.strength() <= 0 && within(me, c, reach) && can_take_camp(c)) {
            const int cs = slot_at(c.spawn_position());
            if (PASS_FIX && cs >= 0 && cs != s && !clusters_only[cs]) continue;
            if (PASS_REACH && cs >= 0 && !(DASH_REACH ? can_reach(cs, me.position(), dash_kind(me.champion_name())) : walkable(cs, me.position()))) continue;
            passing.push_back(c);
        }
    bool slot_in_reach = false;
    for (const auto& c : camps)
        if (within(me, c, reach)) slot_in_reach = true;
    if (!passing.empty() && !slot_in_reach) {
        const auto target = *lowest(passing);
        WHY(stem + ".pass", "a cluster in reach on the way " + at(target.position()) + ": free hit");
        engage(me, target, {}, true, avoid);
        return;
    }
    if (camps.empty()) {
        const auto& tiles = spawn_board(s);
        const int now = static_cast<int>(bc::round_number());
        if (ready_at[s] > now) {
            const int wait = ready_at[s] - now;
            // v145 (WAIT_OUTSIDE): ranged wait just outside the group's reach (a hidden camp may be woken by someone).
            int off = WAIT_RANGE;
            if (WAIT_OUTSIDE && me.attack_range() > 1)
                for (const auto& c : route[s]->camps()) off = std::max(off, camp_kind_reach(c.kind()) + 1);
            WHY(stem + ".wait", format("s%d ", s) + at(center) + tile_flags(center) + format(" respawns in %d: wait %d off", wait, off));
            bc::bitboard ring, inside;
            for (const auto tile : spawn_lists[s]) {
                ring.draw_chebyshev(tile, static_cast<float>(off));
                if (off > WAIT_RANGE) inside.draw_chebyshev(tile, static_cast<float>(off - 1));
            }
            if (off > WAIT_RANGE) ring &= ~inside;
            move_towards_closest(ring & ~tiles, avoid);
        } else if (prism_team() && (me.champion_name() != Champ::prism || picker_reward(s)) && !gemless[s] && prism_wants(s)
                   && rewards.count(s) && capture_slot != s) {
            // v158: not ours to stand on: Prism's claim comes with the leader's next turn.
            WHY(stem + ".off", format("s%d ", s) + at(center) + " is cleared and hers: wait off its tiles");
            bc::bitboard ring;
            for (const auto tile : spawn_lists[s]) ring.draw_chebyshev(tile, WAIT_RANGE);
            move_towards_closest(ring & ~tiles, avoid);
        } else if (CAPTURE_STAND && rewards.count(s) && capture_slot != s) {
            // v137: cleared with nobody's claim on it: standing on a tile captures it, so stay on (no FIX_POCKET step-off).
            WHY(stem + ".capture", format("s%d ", s) + at(center) + " is cleared and unclaimed: stand on its reward");
            if (!tiles.is_set(me.position())) move_towards_closest(tiles, avoid);
        } else if (FIX_POCKET && tiles.is_set(me.position())) {
            // v103: due but not here: our own body on a spawn tile holds the group down (islands: two dashers stood on
            // a pocket pair's tiles for 296 rounds, each "walking to it"). Stepping off lets it respawn.
            WHY(stem + ".off", format("s%d ", s) + at(center) + " is due but we stand on its spawn: step off");
            bc::bitboard ring;
            for (const auto tile : spawn_lists[s]) ring.draw_chebyshev(tile, WAIT_RANGE);
            move_towards_closest(ring & ~tiles, avoid);
        } else {
            WHY(stem + ".walk", format("s%d ", s) + at(center) + tile_flags(center) + " is up but out of sight: walk to it");
            // v122 (SPAWN_RING): every spawn tile in sight and empty means the group has not respawned (standing on a
            // tile would hold it down): wait on the ring around them. A tile out of sight (a bush, or still far) may
            // hide the camps, so we walk onto the tiles as before (the ring everywhere idled 32k turns beside bushes).
            bool tiles_seen = false;
            if (SPAWN_RING) {
                bc::vision::visible_tiles_into(ring_out);
                tiles_seen = std::all_of(spawn_lists[s].begin(), spawn_lists[s].end(), [&](vec2 t) { return ring_out.is_set(t); });
            }
            if (tiles_seen) {
                ring_board.clear();
                for (const auto tile : spawn_lists[s]) ring_board.draw_chebyshev(tile, WAIT_RANGE);
                fast::not_into(tiles, ring_out);
                ring_board &= ring_out;
                move_towards_closest(ring_board, avoid);
            } else {
                move_towards_closest(tiles, avoid);
            }
        }
        return;
    }
    std::vector<bc::camp> in_reach;
    for (const auto& c : camps)
        if (within(me, c, reach)) in_reach.push_back(c);
    auto target = lowest(in_reach);
    if (!target) {
        target = camps.front();
        for (const auto& c : camps)
            if (distance(me.position(), c.position()) < distance(me.position(), target->position())) target = c;
    }
    if (FARM_KEEP) {
        keep_slot = s;
        keep_round = round_now;
    }
    // v135 (PRESENT_GATE): an unstarted elk/golem group waits for enough of us (gate counted over who is there).
    if (PRESENT_GATE && stage_group(me, s, camps, avoid, stem)) return;
    if (TANK_SWAP && swap_tank_step(me, s)) SUB(".tank", "steps in front of a camp hitting a hurt ally");
    farmed_slot = s;
    farmed_round = round_now;
    // v148-normal (RANGED_COVER): a camp of the group hits us with a sturdy ally there: one step behind it, then hit on.
    if (RANGED_COVER && me.attack_range() > 1 && me.champion_name() != Champ::willow && s == slot % static_cast<int>(route.size())
        && bc::fuel_remaining() >= V135_SPARE_FUEL && camp_heat(me) > 0 && swap_behind(me, false))
        return;
    WHY(stem + ".camp", format("s%d camp ", s) + at(target->position()) + tile_flags(target->position())
                            + (in_reach.empty() ? ": nearest, closing" : ": weakest in reach"));
    engage(me, *target, {}, true, avoid);
}

// v71: enemies hidden in bushes

bool Bot::escape_tools(const bc::champion& me) {
    // v132: see ESCAPE_VARG_SHIELD, ESCAPE_MARCELLA_STUN, ESCAPE_STUN in constants.h. Returns whether the turn's walk is spent.
    const auto kind = me.champion_name();
    const auto& enemies = seen::champions(Team::enemy);
    if (enemies.empty()) return false;
    const auto position = me.position();
    std::optional<bc::champion> near;
    for (const auto& e : enemies)
        if (!near || distance(position, e.position()) < distance(position, near->position())) near = e;
    const int gap = distance(position, near->position());
    if (ESCAPE_VARG_SHIELD && kind == Champ::varg) {
        if (gap <= THREAT_RANGE + 1 && cast_self(ability_of(me, Kind::shield))) NOTE("escape: shield up on the way out");
        return false;
    }
    if (ESCAPE_MARCELLA_STUN && kind == Champ::marcella) {
        const auto reset = ability_of(me, Kind::attack_reset);
        // The reset (her fight stun) is only spent once the first hit has landed and the chaser still stands.
        if (gap <= me.attack_range() && reset && reset->is_ready() && attack_if_in_range(*near) && sighted(Target{*near})
            && alive(Target{*near}) && cast_self(reset) && attack_if_in_range(*near))
            NOTE("escape: stun the chaser with the enhanced hit, then step away");
        return false;
    }
    if (ESCAPE_STUN && (kind == Champ::ryth || kind == Champ::prism)) {
        int their_reach = 1;
        try {
            their_reach = near->attack_range();
        } catch (const std::exception&) {
        }
        const bool caught = their_reach <= 1 ? gap <= 1 : gap <= ESCAPE_STUN_RANGED;
        if (!caught) return false;
        if (ESCAPE_STUN_BACKED) {
            bool backed = false;
            for (const auto& a : seen::champions(Team::ally))
                if (a != me && !channelling(a) && a.health() >= a.max_health() * 0.5 && distance(position, a.position()) <= ESCAPE_STUN_ALLY)
                    backed = true;
            int pack = 0;
            for (const auto& e : enemies)
                if (distance(e.position(), near->position()) <= 1) ++pack;
            if (!backed && pack < 2) return false;
        }
        bool stunned = false;
        if (kind == Champ::ryth) stunned = ryth_stun(me, Target{*near}, false);
        else {
            const auto stun = ability_of(me, Kind::cc);
            stunned = stun && stun->is_ready() && within(me, *near, ability_range(*stun)) && cast(stun, *near);
        }
        if (stunned) NOTE("escape: stun the chaser instead of walking (it would keep up)");
        return stunned;
    }
    return false;
}

std::function<std::optional<long>(vec2)> Bot::escape_rank(const bc::champion& me, const bc::bitboard& zone) {
    // v130 (ESCAPE_DASH, from our v123): see constants.h. v133 (GO_ESCAPE_PATH): the shortest path out of reach instead.
    if (!ESCAPE_DASH || me.champion_name() == Champ::willow) return {};
    const auto dash = ability_of(me, Kind::dash);
    if (!dash || !dash->is_ready()) return {};
    if (me.champion_name() == Champ::heart_of_ruin) {
        if (!ESCAPE_DASH_HOR) return {};
        const auto shield = ability_of(me, Kind::shield);
        if (shield && shield->is_ready()) return {};  // the shield goes up first (one cast a turn)
    }
    const auto& enemies = seen::champions(Team::enemy);
    if (enemies.empty()) return {};
    std::vector<vec2> spots;
    for (const auto& e : enemies) spots.push_back(e.position());
    auto gap_at = [spots](vec2 tile) {
        int g = 1 << 20;
        for (const auto spot : spots) g = std::min(g, distance(tile, spot));
        return g;
    };
    const auto position = me.position();
    const int now_gap = gap_at(position);
    if (now_gap > THREAT_RANGE + 2) return {};
    vec2 home = ally_spawn_list.empty() ? position : ally_spawn_list.front();
    for (const auto tile : ally_spawn_list)
        if (distance(position, tile) < distance(position, home)) home = tile;
    if (GO_ESCAPE_PATH) {
        // Steps to the nearest free tile out of reach (holes crossed by the dash itself count as the landing's own).
        blocked_tiles_into(escape_walls);
        fast::not_into(escape_walls, escape_seen);
        fast::not_into(zone, escape_goals);
        escape_goals &= escape_seen;
        escape_field.compute(escape_walls, escape_goals, escape_seen, static_cast<std::uint16_t>(ESCAPE_PATH_STEPS));
        auto steps = [this](vec2 t) {
            return escape_seen.is_set(t) ? static_cast<int>(escape_field.get(t)) : ESCAPE_PATH_STEPS + 1;
        };
        const int here = steps(position);
        return [steps, gap_at, now_gap, here, home](vec2 t) -> std::optional<long> {
            const int st = steps(t), g = gap_at(t);
            if (st > here || (st == here && g <= now_gap)) return std::nullopt;
            return -st * 100'000L + g * 200L - distance(t, home);
        };
    }
    return [&zone, gap_at, now_gap, home](vec2 t) -> std::optional<long> {
        const int g = gap_at(t);
        if (g <= now_gap || (g - now_gap < ESCAPE_MIN_GAIN && zone.is_set(t))) return std::nullopt;
        return (g + (zone.is_set(t) ? 0 : 2)) * 1'000L - distance(t, home);
    };
}

bool Bot::vesper_retreat(const bc::champion& me, const bc::bitboard& goal) {
    // v133 (GO_KITE): against enemies with no dash ready (they walk one tile a turn), a dash out gains ground and leaves
    // the attack: fire at the weakest enemy still in reach instead of walking on. Any enemy dash ready: retreat as usual.
    if (!GO_KITE || me.champion_name() != Champ::vesper_vale || !me.can_attack()) return false;
    const auto dash = ability_of(me, Kind::dash);
    if (!dash || !dash->is_ready()) return false;
    const auto& enemies = seen::champions(Team::enemy);
    if (enemies.empty()) return false;
    for (const auto& e : enemies)
        if (ready_moves(e).dash > 0) return false;
    Go opt;
    opt.dash = DashUse::urgent;
    opt.walk = false;
    // The vision list is read before the dash: an enemy it left out of sight would throw on any read.
    const auto before = enemies;
    if (!go(goal, opt)) return false;
    std::vector<bc::champion> sorted;
    for (const auto& e : before)
        if (sighted(Target{e})) sorted.push_back(e);
    std::stable_sort(sorted.begin(), sorted.end(), [](const auto& a, const auto& b) { return a.health() < b.health(); });
    for (const auto& target : sorted)
        if (attack_if_in_range(target)) {
            WHY("retreat.kite", "no enemy dash ready: dash out, then fire at " + at(target.position()));
            return true;
        }
    get_away(goal);
    WHY("retreat.kite", "no enemy dash ready: dash out; nothing in reach, walk on");
    return true;
}

bool Bot::vesper_escape(const bc::champion& me, const bc::bitboard& zone) {
    // v137 (VESPER_ESCAPE, from v135-golem): a tile is as safe as the enemies' walking steps to it (around walls and
    // holes, so a dash across a hole counts what it gains) and its margin outside their reach. The dash goes first; then
    // a step that leaves every enemy's reach, else a shot at the weakest in reach (being hit while walking gains nothing).
    // Fuel: one capped field (~2-3k); skipped when the turn is short of fuel (the old dodge runs).
    if (!VESPER_ESCAPE || me.champion_name() != Champ::vesper_vale || bc::fuel_remaining() < VESPER_ESCAPE_FUEL) return false;
    const auto& enemies = seen::champions(Team::enemy);
    const auto& enemy_tiles = seen::positions(Team::enemy);
    std::array<std::pair<vec2, int>, 8> threats{};
    std::size_t t_n = 0;
    escape_goals.clear();
    for (std::size_t i = 0; i < enemies.size() && i < enemy_tiles.size() && t_n < threats.size(); ++i) {
        try {
            threats[t_n++] = {enemy_tiles[i], static_cast<int>(enemies[i].attack_range())};
            escape_goals.set(enemy_tiles[i]);
        } catch (const bc::error&) {
        }
    }
    if (t_n == 0) return false;
    escape_field.compute(seen::terrain(), escape_goals, escape_seen, static_cast<std::uint16_t>(VESPER_ESCAPE_STEPS));
    auto steps = [this](vec2 t) {
        return escape_seen.is_set(t) ? std::min(static_cast<int>(escape_field.get(t)), VESPER_ESCAPE_STEPS + 1) : VESPER_ESCAPE_STEPS + 1;
    };
    auto margin = [&threats, t_n](vec2 t) {
        int best = 1 << 20;
        for (std::size_t i = 0; i < t_n; ++i) best = std::min(best, distance(t, threats[i].first) - threats[i].second);
        return best;
    };
    const auto start = seen::self_position();
    vec2 home = ally_spawn_list.empty() ? start : ally_spawn_list.front();
    for (const auto tile : ally_spawn_list)
        if (distance(start, tile) < distance(start, home)) home = tile;
    auto score = [&](vec2 t) {
        const int m = margin(t);
        return (m > 0 ? 1'000'000L : 0L) + (zone.is_set(t) ? 0L : 100'000L) + steps(t) * 1'000L + m * 10L - distance(t, home);
    };
    const long at_start = score(start);
    Go out;
    out.walk = false;
    out.dash = DashUse::urgent;
    out.guard_dash = GO_GUARD_DASH;
    out.rank = [&score, at_start](vec2 t) -> std::optional<long> {
        const long s = score(t);
        if (s <= at_start) return std::nullopt;
        return s;
    };
    const bool dashed = go(~zone, out);
    if (dashed) SUB("", "dash out by the enemies' steps");
    // The walk: the free neighbour that scores best (the walk guard holds unless we already stand in it).
    const auto position = seen::self_position();
    if (me.can_move()) {
        static bc::bitboard blocked{bc::uninitialized};
        blocked_tiles_into(blocked);
        if (walk_guard && !walk_guard->is_set(position)) blocked |= *walk_guard;
        std::optional<vec2> best;
        long best_score = score(position);
        const int w = bc::map::width(), h = bc::map::height();
        for (int dy = -1; dy <= 1; ++dy)
            for (int dx = -1; dx <= 1; ++dx) {
                const vec2 t{position.x + dx, position.y + dy};
                if ((!dx && !dy) || t.x < 0 || t.y < 0 || t.x >= w || t.y >= h || blocked.is_set(t)) continue;
                const long s = score(t);
                if (s > best_score) {
                    best = t;
                    best_score = s;
                }
            }
        // Out of every enemy's reach with this step: walk. Otherwise fire, if anything is in reach.
        const bool walk_out = best && margin(*best) > 0;
        if (!walk_out && me.can_attack()) {
            std::optional<bc::champion> weakest;
            double weakest_hp = 0;
            const int reach = me.attack_range();
            const auto& now_enemies = seen::champions(Team::enemy);
            const auto& now_tiles = seen::positions(Team::enemy);
            for (std::size_t i = 0; i < now_enemies.size() && i < now_tiles.size(); ++i)
                try {
                    if (distance(position, now_tiles[i]) > reach) continue;
                    const double hp = now_enemies[i].health();
                    if (!weakest || hp < weakest_hp) {
                        weakest = now_enemies[i];
                        weakest_hp = hp;
                    }
                } catch (const bc::error&) {
                }
            if (weakest && attack_if_in_range(*weakest)) {
                if (alive(Target{*weakest}) && cast_self(ability_of(me, Kind::attack_reset))) attack_if_in_range(*weakest);
                SUB("", "no step leaves their reach: fire at the weakest " + at(weakest->position()));
                return true;
            }
        }
        if (best) {
            try {
                if (me.move_absolute(*best)) {
                    trace::moved("move");
                    SUB("", walk_out ? "step out of reach" : "step to the safest tile");
                    return true;
                }
            } catch (const bc::error& error) {
                NOTE("escape step refused: " + error_text(error));
            }
            seen::reset();
        }
    }
    return dashed;
}

bool Bot::fled(const bc::champion& me, const bc::champion& enemy) const {
    // It walked away from where we stand since last round.
    const auto k = static_cast<std::size_t>(champ_id(enemy.champion_name())) % enemy_at.size();
    const int now = static_cast<int>(bc::round_number());
    if (enemy_at_round[k] != now || enemy_was_round[k] != now - 1) return false;
    const auto here = me.position();
    return distance(enemy_at[k], here) > distance(enemy_was[k], here);
}

void Bot::track_enemies() {
    // v138: types from the ident cache and tiles from the vision list (no engine reads of their own).
    const int now = static_cast<int>(bc::round_number());
    const auto& enemies = seen::champions(Team::enemy);
    const auto& tiles = seen::positions(Team::enemy);
    for (std::size_t i = 0; i < enemies.size() && i < tiles.size(); ++i) {
        int ident;
        try {
            ident = cached_ident(enemies[i]) - 1;
        } catch (const bc::error&) {
            continue;
        }
        const auto k = static_cast<std::size_t>(ident) % enemy_at.size();
        if (enemy_at_round[k] == now) continue;
        enemy_was[k] = enemy_at[k];
        enemy_was_round[k] = enemy_at_round[k];
        enemy_at[k] = tiles[i];
        enemy_at_round[k] = now;
    }
}

void Bot::mark_ghost(const bc::champion& target) {
    if (!GHOST) return;
    // Willow holds the backline; a target past GHOST_NEAR leaves by walking out of sight, not by hiding.
    if (cached_ident(bc::self()) == champ_id(Champ::willow) + 1
        || distance(seen::self_position(), target.position()) > GHOST_NEAR) {
        ghost_k = -1;
        return;
    }
    try {
        ghost_k = (cached_ident(target) - 1) % static_cast<int>(enemy_at.size());
    } catch (const bc::error&) {
        return;
    }
    ghost_round = static_cast<int>(bc::round_number());
}

bool Bot::ghost_where(const bc::champion& me, vec2& aim) {
    if (!GHOST || ghost_k < 0) return false;
    const int now = static_cast<int>(bc::round_number());
    const auto k = static_cast<std::size_t>(ghost_k);
    const int last = enemy_at_round[k];
    // In sight: the fight logic has it. Its sighting must be the one we fought (or the round after), and recent.
    if (last == now) return false;
    const int gone = now - last;
    auto drop = [&](const char* why) {
        LOG(note(std::string("ghost dropped: ") + why););
        ghost_k = -1;
        return false;
    };
    if (gone > GHOST_ROUNDS || ghost_round < last - 1) return drop("too long unseen");
    for (const auto& e : seen::roster(Team::enemy)) {
        int ident;
        try {
            ident = cached_ident(e) - 1;
        } catch (const bc::error&) {
            continue;
        }
        if (ident % static_cast<int>(enemy_at.size()) != ghost_k) continue;
        bool living = true;
        try {
            living = e.is_alive();
        } catch (const bc::error&) {
        }
        if (!living) return drop("dead");
        break;
    }
    const vec2 from = enemy_at[k];
    aim = from;
    if (enemy_was_round[k] == last - 1) {
        const vec2 d = from - enemy_was[k];
        if (d.x != 0 || d.y != 0) {
            std::array<vec2, GHOST_ROUNDS + 1> path;
            const auto n = lead_path(from, vec2{(d.x > 0) - (d.x < 0), (d.y > 0) - (d.y < 0)}, gone, path.data());
            if (n > 0) aim = path[n - 1];
        }
    }
    if (spawn_zone.is_set(aim)) return drop("heading into its home ring");
    // Anywhere unseen it can stand within its walk since the sighting (a dash adds GHOST_SLACK).
    fast::around_into(from, static_cast<float>(gone + GHOST_SLACK), ghost_hide);
    fast::not_into(seen::fixed_blocks(), ghost_tmp);
    ghost_hide &= ghost_tmp;
    bc::vision::visible_tiles_into(ghost_tmp);
    fast::not_into(ghost_tmp, ghost_tmp);
    ghost_hide &= ghost_tmp;
    if (ghost_hide.is_empty()) return drop("every tile it could be on is in sight");
    return true;
}

bool Bot::ghost_ward(const bc::champion& me) {
    if (!GHOST || !GHOST_WARD || ghost_k < 0) return false;
    try {
        if (me.ward_cooldown() > 0) return false;
    } catch (const bc::error&) {
        return false;
    }
    vec2 aim;
    if (!ghost_where(me, aim)) return false;
    me.ward_placement_tiles_into(ghost_tmp);
    ghost_tmp &= ghost_hide;
    std::array<vec2, 32> spots;
    const auto n = std::min<std::size_t>(ghost_tmp.positions_into(spots), spots.size());
    if (n == 0) return false;
    std::size_t best = 0;
    for (std::size_t i = 1; i < n; ++i)
        if (distance(spots[i], aim) < distance(spots[best], aim)) best = i;
    bool placed;
    try {
        placed = me.place_ward(spots[best]);
    } catch (const bc::error&) {
        return false;
    }
    // Refused: something hidden stands there, now revealed. Either way the vision lists are stale.
    seen::reset();
    NOTE(std::string(placed ? "ghost ward at " : "ghost ward refused at ") + at(spots[best]) + ", its guess " + at(aim));
    if (!placed) return false;
    trace::act("ward");
    if (ward_slot) {
        set_shared_byte(WARD_MASK_BYTE, shared_byte(WARD_MASK_BYTE) & ~(1 << *ward_slot));
        ward_slot.reset();
    }
    ward_for_slot = -1;
    return true;
}

bool Bot::chase_ghost(const bc::champion& me) {
    if (!GHOST || ghost_k < 0 || !me.can_move() || team_chase_off()) return false;
    if (me.health() < me.max_health() * GHOST_HEALTH) return false;
    if (GHOST_JOIN) {
        const vec2 here = seen::self_position();
        const auto& allies = seen::champions(Team::ally);
        const auto& tiles = seen::positions(Team::ally);
        bool backed = false;
        for (std::size_t i = 0; i < allies.size() && !backed; ++i)
            if (allies[i] != me && distance(tiles[i], here) <= FIGHT_LOCAL_RANGE) backed = true;
        if (!backed) return false;
    }
    vec2 aim;
    if (!ghost_where(me, aim)) return false;
    // The unseen tiles beside the guess, else any it could be on.
    fast::around_into(aim, 1.0f, ghost_tmp);
    ghost_tmp &= ghost_hide;
    const auto& goal = ghost_tmp.is_empty() ? ghost_hide : ghost_tmp;
    Go opt;
    opt.align = aim;
    opt.dash = GHOST_DASH ? DashUse::route : DashUse::keep;
    if (!go(goal, opt)) return false;
    WHY("ghost.chase", "lost sight of " + at(enemy_at[static_cast<std::size_t>(ghost_k)])
                           + format(" %d rounds ago: after it, its last step leads to ", static_cast<int>(bc::round_number()) - enemy_at_round[static_cast<std::size_t>(ghost_k)])
                           + at(aim));
    // The step may have shown it: a melee hits it at once.
    for (const auto& e : seen::champions(Team::enemy)) {
        int ident;
        try {
            ident = cached_ident(e) - 1;
        } catch (const bc::error&) {
            continue;
        }
        if (ident % static_cast<int>(enemy_at.size()) == ghost_k) {
            if (attack_if_in_range(e)) SUB(".hit", "found it in reach");
            break;
        }
    }
    return true;
}

void Bot::remember_health(const bc::champion& me) {
    // Read at the end of every turn, so the next one can tell how much it lost in between.
    if (!me.is_alive()) {
        last_health = -1;
        return;
    }
    last_health = static_cast<int>(me.health());
    last_health_round = static_cast<int>(bc::round_number());
}

void Bot::check_hidden(const bc::champion& me) {
    // Between our turns only camps and enemy champions deal damage. With no enemy in sight and more lost
    // than every visible camp in reach could deal, someone we cannot see hit us: an enemy needs sight of us,
    // and sight is symmetric except into bushes, so they stand in an unseen bush within spell reach.
    const int now = static_cast<int>(bc::round_number());
    if (last_health_round != now - 1 || last_health < 0) return;
    const int health = static_cast<int>(me.health());
    if (health >= last_health || !seen::champions(Team::enemy).empty()) return;
    const int lost = last_health - health;
    const auto position = me.position();
    double camp_damage = 0;
    for (const auto& camp : seen::camps())
        if (camp.strength() > 0 && distance(position, camp.position()) <= camp.attack_range()) camp_damage += camp.strength();
    if (lost <= camp_damage + 1) return;
    // A camp inside a bush sees out and hits us unseen: every camp spawn tile in reach we cannot see counts.
    const auto& bushes = bc::map::bushes();
    if (bushes.is_empty()) return;
    const auto visible = bc::vision::visible_tiles();
    if (!camp_spots) {
        camp_spots.emplace();
        for (const auto* group : route)
            for (const auto& camp : group->camps()) {
                const auto kind = camp.kind();
                if (camp_kind_strength(kind) > 0)
                    camp_spots->push_back({camp.spawn_position(), camp_kind_strength(kind), camp_kind_reach(kind)});
            }
    }
    for (const auto& spot : *camp_spots)
        if (distance(position, spot.at) <= spot.reach && !visible.is_set(spot.at)) camp_damage += spot.strength;
    if (lost <= camp_damage + 1) return;
    auto spots = bushes;
    spots &= tiles_around(position, HIDDEN_REACH);
    spots &= ~visible;
    if (spots.is_empty()) return;
    hidden_spots = spots;
    hidden_zone.clear();
    for (const auto tile : spots.positions()) hidden_zone.draw_chebyshev(tile, HIDDEN_REACH);
    hidden_until = now + HIDDEN_ROUNDS - 1;
    hidden_lost = lost;
    NOTE("hit harder than the camps in reach can, nobody in sight: an enemy hides in a bush");
}

bool Bot::answer_hidden(const bc::champion& me) {
    // With an enemy in sight the fight logic answers; on spawn we heal and are safe.
    if (!seen::champions(Team::enemy).empty()) return false;
    const auto position = me.position();
    if (seen::ally_spawn().is_set(position)) return false;
    // 1. The ward costs no action: put it into an unseen bush the attacker can stand in, nearest first.
    bool cooled = false;
    try {
        cooled = me.ward_cooldown() == 0;
    } catch (const bc::error&) {
    }
    if (cooled) {
        auto options = me.ward_placement_tiles();
        options &= hidden_spots;
        options &= ~bc::vision::visible_tiles();
        auto order = options.positions();
        std::stable_sort(order.begin(), order.end(), [&](vec2 a, vec2 b) { return distance(a, position) < distance(b, position); });
        for (const auto tile : order) {
            bool placed;
            try {
                placed = me.place_ward(tile);
            } catch (const bc::error&) {
                continue;
            }
            // Refused means something hidden stands on that tile (a camp, or the attacker), now revealed; a
            // refusal keeps the ward, so the next tile is tried unless the attacker has shown up.
            seen::reset();
            NOTE(std::string(placed ? "ward into bush " : "ward refused at ") + at(tile) + " to find the hidden attacker");
            if (!placed) {
                if (!seen::champions(Team::enemy).empty()) break;
                continue;
            }
            trace::act("ward");
            // The ward leaves its spot (open again for others) and from now on follows the team.
            if (ward_slot) {
                set_shared_byte(WARD_MASK_BYTE, shared_byte(WARD_MASK_BYTE) & ~(1 << *ward_slot));
                ward_slot.reset();
            }
            bush_warded = true;
            ward_for_slot = -1;
            break;
        }
        if (!seen::champions(Team::enemy).empty()) {
            NOTE("hidden attacker revealed");
            hidden_until = -1;
            return false;
        }
    }
    // 2. Nothing to hit back at: step out of reach of every unseen bush it may be in.
    if (!hidden_zone.is_set(position)) return false;
    if (is_collector(me)) release_claim("hit by a hidden attacker");
    WHY("hidden.dodge", "hit by someone unseen: step out of reach of the unseen bushes nearby");
    move_towards_closest(~hidden_zone);
    return true;
}

void Bot::update_ping(const bc::champion& me) {
    ping_slot.reset();
    if (route.empty()) return;
    const int now = static_cast<int>(bc::round_number());
    const int value = shared_byte(PING_BYTE);
    if (value & 63) {
        const int age = (now - (value >> 6)) & 3;
        if (age > PING_FRESH)
            set_shared_byte(PING_BYTE, 0);
        else if ((value & 63) - 1 < static_cast<int>(route.size()))
            ping_slot = (value & 63) - 1;
    }
    auto dead_enemies = [] {
        int n = 0;
        for (const auto& e : seen::roster(Team::enemy))
            if (!e.is_alive()) ++n;
        return n;
    };
    // v78 (PING_LOG): PING_REPORT rounds after our ping, record what came of it.
    if (PING_LOG && ping_mine_round >= 0 && now - ping_mine_round == PING_REPORT) {
        if (!me.is_alive())
            NOTE(format("our ping at s%d: we died", ping_mine_slot));
        else
            NOTE(format("our ping at s%d: survived, %d enemies died", ping_mine_slot, dead_enemies() - ping_mine_dead));
        ping_mine_round = -10;
    }
    // Send one when an enemy champion is hurting us and nobody has a fresh ping up.
    const bool own_fresh = PING_REFRESH && ping_mine_round >= now - PING_FRESH;
    // v119 (PING_GATHER): a near enemy champion in sight after a few rounds without one is a new contact.
    bool gather = false;
    if (PING_GATHER && me.is_alive() && !near_enemies().empty()) {
        gather = near_seen_round < now - 3;
        near_seen_round = now;
    }
    if ((ping_slot && !own_fresh) || !me.is_alive()) return;
    if (!gather && (last_health < 0 || last_health_round != now - 1)) return;
    if (PING_FILTER && mode == FIGHT) return;
    const int lost = gather ? 0 : last_health - static_cast<int>(me.health());
    if (!gather && lost < PING_MIN_LOSS) return;
    const auto position = seen::self_position();
    bool attacked = gather;
    for (const auto tile : seen::positions(Team::enemy))
        if (distance(position, tile) <= PING_REACH) attacked = true;
    if (!attacked) return;
    // v78 (PING_FILTER): an ally beside us is already in this fight; the ping would add nothing.
    if (PING_FILTER) {
        const auto& allies = seen::champions(Team::ally);
        const auto& ally_tiles = seen::positions(Team::ally);
        for (std::size_t i = 0; i < allies.size(); ++i)
            if (distance(position, ally_tiles[i]) <= PING_ALONE && allies[i] != me) return;
    }
    int best = -1;
    for (int s = 0; s < static_cast<int>(centers.size()); ++s)
        if (best < 0 || distance(position, centers[s]) < distance(position, centers[best])) best = s;
    if (best < 0 || best >= 63) return;
    set_shared_byte(PING_BYTE, (best + 1) | ((now & 3) << 6));
    ping_slot = best;
    if (own_fresh) {
        // v79 (PING_REFRESH): the same fight goes on; keep the outcome log anchored to its first ping.
        ping_mine_slot = best;
        return;
    }
    ping_mine_round = now;
    ping_mine_slot = best;
    ping_mine_health = static_cast<int>(me.health());
    if (PING_LOG) ping_mine_dead = dead_enemies();
    NOTE(format(gather ? "ping s%d: enemy champion in sight, nobody beside us" : "ping s%d: an enemy champion is hurting us, nobody beside us", best));
}

void Bot::commit_ping(const bc::champion& me) {
    // v124 (FIGHT_COMMIT_PING): we fight counting an ally that cannot see this fight; call it in. Someone else's fresh
    // ping stands; our own is refreshed while the fight goes on.
    if (!FIGHT_COMMIT_PING || route.empty() || !me.is_alive()) return;
    const int now = static_cast<int>(bc::round_number());
    const bool own_fresh = ping_mine_round >= now - PING_FRESH;
    if (ping_slot && !own_fresh) return;
    const auto position = me.position();
    const auto& enemies = near_enemies();
    if (enemies.empty()) return;
    vec2 front = enemies[0].position();
    for (const auto& e : enemies)
        if (distance(e.position(), position) < distance(front, position)) front = e.position();
    // The allies the fight score counts (FIGHT_LOCAL), free and healthy, beyond our sight of them.
    int far = 0;
    for (const auto& a : seen::champions(Team::ally)) {
        if (a == me || channelling(a) || a.is_teleporting() || a.health() < a.max_health() * RECALL_HEALTH) continue;
        const vec2 p = a.position();
        if (distance(p, position) > COMMIT_FAR && (assists(p, position) || assists(p, front))) ++far;
    }
    if (far == 0) return;
    int best = 0;
    for (int s = 1; s < static_cast<int>(centers.size()); ++s)
        if (distance(position, centers[s]) < distance(position, centers[best])) best = s;
    if (best >= 63) return;
    set_shared_byte(PING_BYTE, (best + 1) | ((now & 3) << 6));
    ping_slot = best;
    ping_mine_slot = best;
    if (own_fresh) return;
    ping_mine_round = now;
    ping_mine_health = static_cast<int>(me.health());
    if (PING_LOG) {
        int dead = 0;
        for (const auto& e : seen::roster(Team::enemy))
            if (!e.is_alive()) ++dead;
        ping_mine_dead = dead;
    }
    NOTE(format("ping s%d: committed to a fight counting %d allies out of sight", best, far));
}

bool Bot::ping_busy(const bc::champion& me) {
    // v128 (PING_IDLE_ONLY): busy farming: the reward's collector, or a camp we fight in reach (+1).
    if (!PING_IDLE_ONLY) return false;
    if (is_collector(me)) return true;
    const auto position = me.position();
    const int reach = static_cast<int>(me.attack_range()) + 1;
    for (const auto& camp : seen::camps())
        if ((camp.health() < camp.max_health() || camp.is_awake()) && distance(position, camp.position()) <= reach) return true;
    return false;
}

bool Bot::answer_ping(const bc::champion& me) {
    // A fight-mode champion with no enemy in sight walks to the pinged slot instead of farming on.
    if (!ping_slot || ping_busy(me)) return false;
    const auto spot = centers[*ping_slot];
    const int gap = distance(me.position(), spot);
    const int now = static_cast<int>(bc::round_number());
    if (PING_ANSWER_LIMIT) {
        // v78: only someone who can arrive while the fight is on, has nothing to lose and did not send it.
        if (ping_mine_round >= now - PING_FRESH || !can_answer(me, now)) return false;
    }
    if (gap <= 2 || gap > 2 * PING_RANGE) return false;
    if (PING_LOG && ping_answered < now - PING_FRESH) {
        ping_answered = now;
    }
    WHY("fight.answer", format("answer the ping at s%d, %d away: healthy, light bag, can arrive in time", *ping_slot, gap));
    return move_towards_closest(tiles_around(spot, 1));
}

bool Bot::can_answer(const bc::champion& champion, int now) {
    // Healthy enough to stay in the fight after a hit or two, close enough to arrive while it is on, and not
    // walking a big bag into it (a built Ren is the team's carry, so his limit is higher).
    if (!ping_slot || !champion.is_alive() || channelling(champion)) return false;
    if (champion.health() < champion.max_health() * PING_ANSWER_HEALTH) return false;
    if (distance(champion.position(), centers[*ping_slot]) > (FIGHT_COMMIT_PING ? PING_ARRIVE_COMMIT : PING_ARRIVE)) return false;
    bool built_ren = false;
    try {
        built_ren = champion.champion_name() == Champ::ren && emblem_count(champion) >= REN_CORE_EMBLEMS;
        // v123 (PRISM_CARRY_FIGHT): Prism answers whatever she carries.
        if (PRISM_CARRY_FIGHT && champion.champion_name() == Champ::prism) return true;
    } catch (const bc::error&) {
    }
    (void)now;
    return static_cast<int>(champion.gem_count()) < (built_ren ? REN_ANSWER_GEMS : CARRY_GEMS);
}

bool Bot::retreat_to_allies(const bc::champion& me) {
    const int now = static_cast<int>(bc::round_number());
    if (ping_mine_round < now - PING_FRESH || me.health() >= me.max_health() * PING_RETREAT_HEALTH) return false;
    const auto position = me.position();
    std::vector<bc::champion> near_enemies_here;
    for (const auto& e : seen::champions(Team::enemy))
        if (distance(position, e.position()) <= PING_REACH + 1) near_enemies_here.push_back(e);
    if (near_enemies_here.empty()) return false;
    int with_us = 0;
    std::optional<vec2> ally;
    for (const auto& a : seen::champions(Team::ally)) {
        const int d = distance(position, a.position());
        if (d <= PING_ALONE) ++with_us;
        if (a != me && d > PING_ALONE && (!ally || d < distance(position, *ally))) ally = a.position();
    }
    // Only when actually outnumbered: retreating from an even fight lost to this (37-31 vs 42-26 against v80).
    if (with_us >= static_cast<int>(near_enemies_here.size())) return false;
    // v105 (CAMP_EXIT_REACT): out of awake camps hitting us before walking to help.
    if (CAMP_EXIT_REACT && me.champion_name() != Champ::willow && camp_exit(me, near_enemies_here, "ping.retreat")) return true;
    // A melee may still swing at whatever is in reach before stepping away.
    if (me.attack_range() <= 1) {
        auto sorted = near_enemies_here;
        std::stable_sort(sorted.begin(), sorted.end(), [](const auto& a, const auto& b) { return a.health() < b.health(); });
        for (const auto& target : sorted)
            if (attack_if_in_range(target)) break;
    }
    const char* where = "our spawn";
    bool moved = false;
    if (ally) {
        where = "the nearest ally";
        moved = move_towards_closest(tiles_around(*ally, 1));
    }
    if (!moved && !route.empty()) {
        const auto spot = centers[slot % static_cast<int>(route.size())];
        if (distance(position, spot) > 2) {
            where = "the team's target";
            moved = move_towards_closest(tiles_around(spot, 2));
        }
    }
    if (!moved) {
        where = "our spawn";
        moved = move_towards_closest(seen::ally_spawn());
    }
    WHY("ping.retreat", format("outnumbered %d v %zu after our ping, under %.0f%%: fall back to ", with_us, near_enemies_here.size(),
                               PING_RETREAT_HEALTH * 100) + where);
    return true;
}

// v159 (MONO_COVER, MONO_LEAVE, MONO_FIGHT_OUT): an awake monolith hits only the nearest champion in its reach (5).

namespace {
// Awake monoliths whose reach covers `at`: spot and strength, in sight only.
struct MonoNear {
    vec2 spot;
    int reach;
    double strength;
};
std::size_t monoliths_reaching(vec2 at, std::array<MonoNear, 4>& out) {
    std::size_t n = 0;
    for (const auto& c : seen::camps()) {
        if (n >= out.size()) break;
        if (!c.is_monolith()) continue;
        const auto spot = c.position();
        const int reach = c.attack_range();
        if (distance(at, spot) > reach) continue;
        bool awake;
        try {
            awake = c.is_awake();
        } catch (const bc::error&) {
            awake = c.health() < c.max_health();
        }
        if (awake) out[n++] = {spot, reach, static_cast<double>(c.strength())};
    }
    return n;
}
}  // namespace

bool Bot::mono_cover(const bc::champion& me) {
    // MONO_COVER: a fragile champion (under TANK_HEALTH max health) stays strictly farther from an awake monolith than
    // the nearest sturdy ally there, so the sturdy one takes its hits; with no sturdy ally there it may hit on, and
    // MONO_LEAVE takes it out like anyone else.
    if (!MONO_COVER || me.max_health() >= TANK_HEALTH || !me.can_move()) return false;
    const auto position = me.position();
    std::array<MonoNear, 4> monos{};
    const std::size_t n = monoliths_reaching(position, monos);
    if (n == 0) return false;
    for (std::size_t i = 0; i < n; ++i) {
        const auto spot = monos[i].spot;
        int tank_gap = 1 << 10;
        for (const auto& a : seen::champions(Team::ally))
            if (a != me && a.max_health() >= TANK_HEALTH && !channelling(a) && a.health() > a.max_health() * RECALL_HEALTH)
                tank_gap = std::min(tank_gap, distance(a.position(), spot));
        if (tank_gap > monos[i].reach) continue;  // no sturdy ally in its reach: hit on (MONO_LEAVE decides)
        const int gap = distance(position, spot);
        if (gap > tank_gap) {
            // Covered. Closing to our reach would put us level with or ahead of the tank: hold here instead.
            if (gap > static_cast<int>(me.attack_range()) && static_cast<int>(me.attack_range()) <= tank_gap) {
                WHY("mono.cover.hold", format("the monolith at %s is out of reach without passing the tank (gap %d)",
                                              at(spot).c_str(), tank_gap));
                return true;
            }
            continue;
        }
        // Step back behind the tank: a tile farther than the tank, as near as our reach allows.
        const int band = std::max(tank_gap + 1, static_cast<int>(me.attack_range()));
        bc::bitboard goal = tiles_around(spot, static_cast<float>(band));
        goal &= ~tiles_around(spot, static_cast<float>(tank_gap));
        if (goal.is_empty()) continue;
        WHY("mono.cover", format("fragile and level with or ahead of the tank (gap %d vs %d) at the monolith %s: step behind",
                                 gap, tank_gap, at(spot).c_str()));
        if (move_towards_closest(goal)) return true;
    }
    return false;
}

int Bot::mono_leave_bar(const bc::champion& me) {
    // MONO_LEAVE: the hits we take before an ally is the nearer (it holds its tile while we step back), or before we are
    // out of reach when nobody is behind us, plus MONO_LEAVE_MARGIN hits; 0 while no awake monolith hits us.
    const auto position = me.position();
    std::array<MonoNear, 4> monos{};
    const std::size_t n = monoliths_reaching(position, monos);
    double hits = 0;
    int steps = 0;
    for (std::size_t i = 0; i < n; ++i) {
        const auto spot = monos[i].spot;
        const int gap = distance(position, spot);
        if (other_nearer(me, spot, gap)) continue;
        int next = 1 << 10;
        for (const auto& a : seen::champions(Team::ally))
            if (a != me && !channelling(a)) next = std::min(next, distance(a.position(), spot));
        steps = std::max(steps, next <= monos[i].reach ? next - gap + 1 : monos[i].reach - gap + 1);
        hits += monos[i].strength;
    }
    if (hits <= 0) return 0;
    return static_cast<int>(hits * (steps + MONO_LEAVE_MARGIN) * camp_taken_mult(owned_of(me).passives));
}

bool Bot::mono_leave(const bc::champion& me) {
    // MONO_LEAVE: the champion an awake monolith hits leaves once its health would not outlast the walk until an ally is
    // the nearer (mono_leave_bar); the next one in line hits on and leaves on the same rule. Out of reach, it recalls
    // when nothing can break the recall, and keeps out of the monolith (mono_rest) until healed.
    if (!MONO_LEAVE) return false;
    if (mono_rest && (seen::ally_spawn().is_set(me.position()) || me.health() >= me.max_health() * MONO_BACK_HEALTH))
        mono_rest = false;
    const int bar = mono_leave_bar(me);
    if (bar > 0 && me.health() <= bar) {
        mono_rest = true;
        if (camp_exit(me, near_enemies(), "mono.leave")) {
            LOG(branch_why = format("the monolith hits us and %d health would not outlast the walk out (%d); ",
                                    static_cast<int>(me.health()), bar) + branch_why;);
            return true;
        }
    }
    if (!mono_rest) return false;
    std::array<MonoNear, 4> monos{};
    if (monoliths_reaching(me.position(), monos) > 0) {
        if (camp_exit(me, near_enemies(), "mono.rest")) return true;
        return false;
    }
    // Out of reach: recall to heal when nothing breaks it; otherwise the usual rules (which stay off the monolith).
    if (!channel_cut && !me.is_recalling() && near_enemies().empty() && camp_heat(me) == 0) {
        try {
            me.recall();
            trace::act("recall");
            WHY("mono.recall", "left the monolith hurt: recall to heal before the next session");
            return true;
        } catch (const bc::error& error) {
            NOTE("recall refused: " + error_text(error));
        }
    }
    return false;
}

bc::bitboard Bot::mono_fight_zone() {
    // MONO_FIGHT_OUT: every awake monolith's reach, kept out of while fighting.
    bc::bitboard zone;
    if (!MONO_FIGHT_OUT) return zone;
    for (const auto& c : seen::camps()) {
        if (!c.is_monolith()) continue;
        bool awake;
        try {
            awake = c.is_awake();
        } catch (const bc::error&) {
            awake = c.health() < c.max_health();
        }
        if (awake) zone.draw_chebyshev(c.position(), static_cast<float>(c.attack_range()));
    }
    return zone;
}
