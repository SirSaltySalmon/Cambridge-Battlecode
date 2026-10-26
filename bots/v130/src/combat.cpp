// Fighting, retreating, and clearing the current camp group.
#include "bot.h"
#include "seen.h"
#include "state.h"
#include "trace.h"
#include <algorithm>
#include <cmath>

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
    bool any = false;
    for (const auto& e : near)
        if (steps_to_enemy_spawn(e.position()) <= HOME_CAMP_STEPS) any = true;
    if (!any) return near;
    home_buf.clear();
    for (const auto& e : near) {
        if (steps_to_enemy_spawn(e.position()) <= HOME_CAMP_STEPS)
            ++home_skipped;
        else
            home_buf.push_back(e);
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
    const vec2 here = me.position();
    for (const auto& ally : seen::champions(Team::ally))
        if (team_n < MAX_UNITS && (!FIGHT_LOCAL || distance(ally.position(), here) <= FIGHT_LOCAL_RANGE))
            team[team_n++] = ally.position();
    if (me.is_alive() && team_n < MAX_UNITS) team[team_n++] = here;
    for (const auto& enemy : enemies)
        if (spot_n < MAX_UNITS) spots[spot_n++] = enemy.position();
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
    if (CAMP_EXIT_REACT && kind != Champ::willow && camp_heat(me) > 0 && in_reach_channel.empty()) {
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
    // v119 (JOIN_FIRST): not alone into a fight we are not in yet.
    if (JOIN_FIRST && !within(me, *target, me.attack_range()) && !channelling(*target) && join_first(me, *target, enemies))
        return true;
    WHY("fight.hit", std::string(pick) + " at " + at(target->position()) + (detail.empty() ? "" : " (chase ring: " + detail + ")"));
    commit_ping(me);
    engage(me, *target, enemies, false, avoid);
    return true;
}

bool Bot::join_first(const bc::champion& me, const bc::champion& target, const std::vector<bc::champion>& enemies) {
    // A contact is the run of rounds we close on enemies; we wait for an ally at most JOIN_WAIT rounds of it. Willow
    // already keeps out of reach, and anyone an enemy can hit fights on (waiting there is a retreat).
    if (me.champion_name() == Champ::willow) return false;
    const int now = static_cast<int>(bc::round_number());
    if (join_last < now - 1) join_since = now;
    join_last = now;
    if (now - join_since >= JOIN_WAIT) return false;
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
    bool tank_in_sight = false, tank_ahead = false;
    for (const auto& a : seen::champions(Team::ally)) {
        if (a == me || channelling(a)) continue;
        const int d = distance(position, a.position());
        if (d < near_d) near_d = d, nearest = a.position();
        if (a.attack_range() <= 1 && a.max_health() > SQUISHY_HEALTH) {
            tank_in_sight = true;
            if (distance(a.position(), target.position()) < to_target) tank_ahead = true;
        }
    }
    const bool covered = squishy ? near_d <= JOIN_RANGE && (tank_ahead || !tank_in_sight) : near_d <= JOIN_RANGE + 3;
    if (covered) return false;
    if (!nearest && JOIN_ALONE == 1) return false;
    if (!nearest && JOIN_ALONE == 2) {
        // Nobody in sight to wait for: the enemy out of reach is not ours to chase; farm on.
        WHY("fight.join.farm", "no ally in sight to close with: farm instead");
        farm(me, nullptr, "join");
        return true;
    }
    if (nearest && near_d > 1) {
        WHY("fight.join", format("no ally within %d of us: walk to the nearest, %d away", JOIN_RANGE, near_d));
        if (!move_towards_closest(tiles_around(*nearest, 1))) NOTE("join: no step toward the ally");
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
int short_of(const bc::champion& chaser, const Ready& ours, const bc::champion& enemy, const Ready& theirs) {
    const int d = distance(chaser.position(), enemy.position());
    const int gap = std::max(0, d - static_cast<int>(chaser.attack_range()));
    if (gap == 0 || channelling(enemy) || (ours.stun > 0 && d <= ours.stun)) return 0;
    return std::max(0, gap - ours.dash + theirs.dash);
}
} // namespace

int Bot::catch_short(const bc::champion& chaser, const bc::champion& enemy) {
    return short_of(chaser, ready_moves(chaser), enemy, ready_moves(enemy));
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
    auto body = [&](const bc::champion& a) {
        for (const auto& e : enemies) {
            const int d = distance(a.position(), e.position());
            if (channelling(a)) {
                if (d <= TEAM_CHASE_GUARD) engaged = true;
                continue;
            }
            gap = std::min(gap, d);
            if (d <= static_cast<int>(a.attack_range()) + 1) engaged = true;
        }
    };
    body(me);
    for (const auto& a : seen::champions(Team::ally))
        if (a != me) body(a);
    for (const auto& e : enemies)
        if (channelling(e)) engaged = true;
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
        std::array<Ready, 8> theirs{};
        for (std::size_t i = 0; i < enemies.size() && i < theirs.size(); ++i) theirs[i] = ready_moves(enemies[i]);
        bool closable = false;
        auto check = [&](const bc::champion& a) {
            if (closable || channelling(a)) return;
            const auto ours = ready_moves(a);
            for (std::size_t i = 0; i < enemies.size() && i < theirs.size(); ++i)
                if (short_of(a, ours, enemies[i], theirs[i]) == 0) closable = true;
        };
        check(me);
        for (const auto& a : seen::champions(Team::ally))
            if (a != me) check(a);
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
    PySum total;
    for (const auto& a : allies) total.add(a.strength());
    const double damage = total.value();
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
                if (!(target.is_monolith() && monolith_delegated()) && attack_if_in_range(target)) {
                    swung = Target{target};
                    break;
                }
    }
    // v131 (RETREAT_SAFE): toward the nearest ally already out of reach, else simply out of reach.
    bool stepped = false;
    if (RETREAT_SAFE) {
        std::optional<vec2> safe_ally;
        for (const auto& a : seen::champions(Team::ally))
            if (a != me && !channelling(a) && !zone.is_set(a.position())
                && (!safe_ally || distance(me.position(), a.position()) < distance(me.position(), *safe_ally)))
                safe_ally = a.position();
        if (safe_ally) {
            const auto goal = tiles_around(*safe_ally, static_cast<float>(RETREAT_ALLY_TILES)) & ~zone;
            stepped = !goal.is_empty() && move_towards_closest(goal);
        }
    }
    if (!stepped && !move_towards_closest(~zone)) {
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
    const auto position = me.position();
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
        bool nearer = false;
        for (const auto& ally : seen::champions(Team::ally))
            if (ally != me && distance(ally.position(), spot) < gap) nearer = true;
        for (const auto& enemy : seen::champions(Team::enemy))
            if (distance(enemy.position(), spot) < gap) nearer = true;
        stray_hit = !nearer;
    }
    return !stray_reach.is_empty();
}

bool Bot::stray_exit(const bc::champion& me, int s, const std::vector<bc::camp>& camps, const bc::bitboard* avoid,
                     const std::string& stem) {
    // v120 (STRAY_EXIT): hit by another group's awake camps while farming s: one step to a tile that still reaches a camp
    // of s and is out of theirs, else one step out of theirs (they sleep after two rounds with nobody in reach), else
    // toward the nearest such tile. The farm turn goes on after the step (a melee still swings at what it reaches).
    if (!stray_heat(me, s) || !stray_hit || !me.can_move()) return false;
    const auto position = me.position();
    const int reach = me.attack_range();
    fast::not_into(stray_reach, stray_out);
    stray_goal.clear();
    for (const auto& c : camps) stray_goal.draw_chebyshev(c.position(), static_cast<float>(reach));
    stray_goal &= stray_out;
    auto& step = stray_step;
    fast::around_into(position, 1, step);
    step &= stray_goal;
    const char* how = "a tile in reach of our camp, out of the other group's";
    bool moved = !step.is_empty() && move_towards_closest(step, avoid);
    if (!moved) {
        fast::around_into(position, 1, step);
        step &= stray_out;
        how = "out of the other group's reach";
        moved = !step.is_empty() && move_towards_closest(step, avoid);
    }
    if (!moved) {
        fast::around_into(position, static_cast<float>(MONOLITH_REACH + 1), step);
        step &= stray_out;
        how = "toward the edge of the other group's reach";
        moved = !step.is_empty() && move_towards_closest(step, avoid);
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
    const auto position = me.position();
    const auto& allies = seen::champions(Team::ally);
    const auto& enemies = seen::champions(Team::enemy);
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
        bool nearer = false;
        for (const auto& ally : allies)
            if (ally != me && distance(ally.position(), spot) < gap) nearer = true;
        for (const auto& enemy : enemies)
            if (distance(enemy.position(), spot) < gap) nearer = true;
        if (!nearer) heat_hit += camp_kind_strength(camp.kind());
    }
    return heat_hit;
}

bool Bot::foreign_camp_hit(const bc::champion& me) {
    // v129 (CAMP_EXIT_ALWAYS): only camps in their own reach of us are read (a raw state read, as camp_heat).
    const int round_now = static_cast<int>(bc::round_number());
    const int farm_s = route.empty() ? -1 : slot % static_cast<int>(route.size());
    const int farmed_s = round_now - farmed_round <= FARMED_ROUNDS ? farmed_slot : -1;
    const auto position = me.position();
    const int own_reach = me.attack_range();
    bool hit = false;
    for (const auto& camp : seen::camps()) {
        const int reach = camp_kind_reach(camp.kind());
        if (reach <= 0) continue;
        const auto spot = camp.spawn_position();
        const int gap = distance(position, spot);
        if (gap > reach) continue;
        const int cs = slot_at(spot);
        if (cs >= 0 && (cs == farm_s || cs == farmed_s || cs == mono_job)) {
            // Farming one of ours: farm() steps out of the others while still reaching it.
            if (gap <= own_reach) return false;
            continue;
        }
        if (hit) continue;
        std::uint32_t state = 0;
        if (bc::detail::sdk_camp_state(static_cast<std::uint32_t>(static_cast<bc::handle_camp_t>(camp)), &state) != 0 || state != 1) continue;
        bool nearer = false;
        for (const auto& ally : seen::champions(Team::ally))
            if (ally != me && distance(ally.position(), spot) < gap) nearer = true;
        for (const auto& enemy : seen::champions(Team::enemy))
            if (distance(enemy.position(), spot) < gap) nearer = true;
        hit = !nearer;
    }
    return hit;
}

bool Bot::camp_exit(const bc::champion& me, const std::vector<bc::champion>& enemies, const std::string& stem) {
    // v105 (CAMP_EXIT_REACT): reacting to an enemy while awake camps hit us, leave the camps first. Every test is a
    // board, and the walking reach grows one step at a time with the engine's own search (one call a step), so no
    // guest loop runs over tiles. Tests: out = beyond every awake camp's reach; away = farther from every near enemy
    // than we stand now; fast = within one step of the quickest way out.
    if (!CAMP_EXIT_REACT || camp_heat(me) <= 0 || !me.can_move()) return false;
    const auto position = me.position();
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
    exit_blocked.reachable_tiles_into(position, static_cast<std::uint16_t>(quickest < 0 ? 1 : quickest + 1), exit_fast);
    exit_blocked.reachable_tiles_into(position, static_cast<std::uint16_t>(quickest < 0 ? 1 : quickest + 2), exit_reach);
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
    const bool hit_first = swing();
    if (!move_towards_closest(exit_goal)) {
        NOTE(std::string("camp exit (") + passed + "): no step found");
        return hit_first;
    }
    const bool hit_after = !hit_first && swing();
    WHY(stem + ".exit", std::string("awake camps hit us: leave them first (") + passed + ")"
                            + (hit_first || hit_after ? ", hit back on the way" : ""));
    return true;
}

bool Bot::escape(const bc::champion& me, const bc::bitboard& zone) {
    // v72: a hurt champion stepping out of reach used to only walk, one tile a turn, while its chasers kept hitting
    // it: over half of our ladder deaths with gems were on these turns. Now it dashes out when it can, stuns a
    // chaser when the stun allows walking (Cassi), walks out toward an ally, and a melee hits back on the way.
    const auto& enemies = seen::champions(Team::enemy);
    auto position = me.position();
    vec2 home = ally_spawn_list.front();
    for (const auto tile : ally_spawn_list)
        if (distance(position, tile) < distance(position, home)) home = tile;
    std::string how;

    const auto dash = ability_of(me, Kind::dash);
    if (dash && dash->is_ready() && zone.is_set(position)) {
        const auto free = tiles_around(position, static_cast<float>(static_cast<int>(ability_range(*dash)))) & ~blocked_tiles();
        std::optional<vec2> best;
        std::pair<bool, int> best_key{};
        for (const auto tile : free.positions()) {
            const std::pair<bool, int> key{!zone.is_set(tile), -distance(tile, home)};
            if (!best || key > best_key) {
                best = tile;
                best_key = key;
            }
        }
        if (best && !zone.is_set(*best) && cast(dash, *best)) {
            how += ", dash";
            position = me.position();
        }
    }
    if (how.empty() && me.champion_name() == Champ::cassi && cassi_stun(me, enemies)) how += ", stun a chaser";

    // Out of reach, next to the nearest visible ally if there is one, else anywhere out of reach.
    std::optional<vec2> ally;
    for (const auto& a : seen::champions(Team::ally))
        if (a != me && (!ally || distance(position, a.position()) < distance(position, *ally))) ally = a.position();
    bool moved = false;
    if (zone.is_set(position)) {
        if (ally) {
            auto near_ally = tiles_around(*ally, ESCAPE_ALLY_RANGE);
            near_ally &= ~zone;
            if (!near_ally.is_empty()) moved = move_towards_closest(near_ally);
        }
        if (!moved) moved = move_towards_closest(~zone);
    }

    // Melee may still swing at whatever is in reach after the step.
    if (me.attack_range() <= 1) {
        auto sorted = enemies;
        std::stable_sort(sorted.begin(), sorted.end(), [](const auto& a, const auto& b) { return a.health() < b.health(); });
        for (const auto& target : sorted)
            if (sighted(Target{target}) && attack_if_in_range(target)) {
                how += ", hit back";
                break;
            }
    }
    NOTE("escape: out of reach" + (ally ? std::string(", toward an ally") : "") + how + (moved ? "" : "; no step found"));
    return moved || !how.empty();
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
    const int by_recall = static_cast<int>(bc::constants::recall_time) + *from_home;
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
    if (FIX_WALK_TARGET) {
        const int other = walk_target(me, s);
        if (other >= 0) {
            s = other;
            camps = camps_at(s);
        } else if (pocket_recall(me, s, stem)) {
            return;
        }
    }
    // v81 (MAGE_MONOLITH): a melee never hits a monolith and keeps out of an awake one's reach; mages take it.
    bc::bitboard keep_out;
    if (!is_mage(me.champion_name()) && !monoliths.empty() && monolith_delegated()) {
        if (is_monolith(s)) {
            monolith_guard(me, s, avoid, stem);
            return;
        }
        keep_out = awake_monoliths();
        if (!keep_out.is_empty()) {
            if (avoid) keep_out |= *avoid;
            avoid = &keep_out;
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
            WHY(stem + ".wait", format("s%d ", s) + at(center) + tile_flags(center) + format(" respawns in %d: wait %d off", wait, WAIT_RANGE));
            bc::bitboard ring;
            for (const auto tile : spawn_lists[s]) ring.draw_chebyshev(tile, WAIT_RANGE);
            move_towards_closest(ring & ~tiles, avoid);
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
    farmed_slot = s;
    farmed_round = round_now;
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

bool Bot::dash_escape(const bc::champion& me, const bc::bitboard& zone) {
    // v130 (ESCAPE_DASH, from our v123): see constants.h.
    const auto dash = ability_of(me, Kind::dash);
    if (!dash || !dash->is_ready()) return false;
    if (me.champion_name() == Champ::heart_of_ruin) {
        if (!ESCAPE_DASH_HOR) return false;
        const auto shield = ability_of(me, Kind::shield);
        if (shield && shield->is_ready()) return false;  // the shield goes up first (one cast a turn)
    }
    const auto& enemies = seen::champions(Team::enemy);
    if (enemies.empty()) return false;
    const auto position = me.position();
    auto gap_at = [&](vec2 tile) {
        int g = 1 << 20;
        for (const auto& e : enemies) g = std::min(g, distance(tile, e.position()));
        return g;
    };
    const int now_gap = gap_at(position);
    if (now_gap > THREAT_RANGE + 2) return false;
    vec2 home = ally_spawn_list.empty() ? position : ally_spawn_list.front();
    for (const auto tile : ally_spawn_list)
        if (distance(position, tile) < distance(position, home)) home = tile;
    std::vector<std::pair<std::pair<int, int>, vec2>> options;
    const auto free = tiles_around(position, static_cast<float>(static_cast<int>(ability_range(*dash)))) & ~blocked_tiles();
    for (const auto tile : free.positions()) {
        const int g = gap_at(tile);
        if (g <= now_gap || (g - now_gap < ESCAPE_MIN_GAIN && zone.is_set(tile))) continue;
        options.push_back({{g + (zone.is_set(tile) ? 0 : 2), -distance(tile, home)}, tile});
    }
    std::sort(options.begin(), options.end(), [](const auto& a, const auto& b) { return a.first > b.first; });
    for (std::size_t i = 0; i < options.size() && i < 3; ++i) {
        bool ok = false;
        try {
            ok = cast(dash, options[i].second);
        } catch (const std::exception&) {
            ok = false;
        }
        if (ok) {
            NOTE(format("escape dash to %s: nearest enemy %d -> %d tiles", at(options[i].second).c_str(), now_gap, options[i].first.first));
            return true;
        }
    }
    return false;
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
    const auto position = me.position();
    bool attacked = gather;
    for (const auto& enemy : seen::champions(Team::enemy))
        if (distance(position, enemy.position()) <= PING_REACH) attacked = true;
    if (!attacked) return;
    // v78 (PING_FILTER): an ally beside us is already in this fight; the ping would add nothing.
    if (PING_FILTER)
        for (const auto& ally : seen::champions(Team::ally))
            if (ally != me && distance(position, ally.position()) <= PING_ALONE) return;
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
