// Fighting, retreating, and clearing the current camp group.
#include "bot.h"
#include "seen.h"
#include "state.h"
#include "trace.h"
#include <algorithm>
#include <cmath>

namespace {

std::string champ_tag(const bc::champion& champion) {
    try {
        return format("c%d", champ_id(champion.champion_name()));
    } catch (const bc::error&) {
        return "c?";
    }
}

} // namespace

const std::vector<bc::champion>& Bot::near_enemies() {
    // Worked out once per vision list: the list is only replaced after one of our own actions, and an
    // old one is kept alive until the turn ends, so its address names it within a round.
    const auto& enemies = seen::champions(Team::enemy);
    const auto round = bc::round_number();
    if (near_src == &enemies && near_round == round) return far_skipped ? near_buf : enemies;
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
    for (const auto& ally : seen::champions(Team::ally))
        if (team_n < MAX_UNITS) team[team_n++] = ally.position();
    if (me.is_alive() && team_n < MAX_UNITS) team[team_n++] = me.position();
    for (const auto& enemy : enemies)
        if (spot_n < MAX_UNITS) spots[spot_n++] = enemy.position();
    std::array<bool, MAX_UNITS> root{}, near{};
    bool any_close = false;
    for (std::size_t t = 0; t < team_n; ++t)
        for (std::size_t e = 0; e < spot_n; ++e)
            if (distance(team[t], spots[e]) <= FIGHT_STEPS) root[t] = any_close = true;
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

bool Bot::fight(const bc::champion& me) {
    auto enemies = near_enemies();
    if (enemies.empty()) {
        if (far_skipped) note(format("fight skips %d enemies over %d steps away", far_skipped, FIGHT_STEPS));
        return false;
    }
    // Near their spawn, only chase what dies before it gets home (anything already in reach still gets
    // hit); with nothing left, leave.
    std::string detail;
    if (spawn_zone.is_set(me.position())) {
        std::vector<bc::champion> kept;
        std::string lines;
        for (const auto& enemy : enemies) {
            if (!lines.empty()) lines += "; ";
            if (within(me, enemy, me.attack_range())) {
                kept.push_back(enemy);
                lines += champ_tag(enemy) + " keep in-range " + at(enemy.position());
                continue;
            }
            const auto [keep, text] = chase_line(me, enemy);
            lines += text;
            if (keep) kept.push_back(enemy);
        }
        detail = lines;
        enemies = kept;
        if (enemies.empty()) {
            branch("fight.leave", "in chase ring, nothing catchable (" + detail + ")");
            move_towards_closest(~spawn_zone);
            return true;
        }
    } else {
        detail = "outside chase ring";
    }
    // A tributing enemy is stuck in place with its bag, which the kill steals.
    std::vector<bc::champion> in_reach, in_reach_channel, channel;
    for (const auto& e : enemies) {
        const bool reach = within(me, e, me.attack_range());
        if (reach) in_reach.push_back(e);
        if (reach && channelling(e)) in_reach_channel.push_back(e);
        if (channelling(e)) channel.push_back(e);
    }
    auto target = lowest(in_reach_channel);
    if (!target) target = lowest(in_reach);
    if (!target) target = lowest(channel);
    if (!target) target = lowest(enemies);
    const std::string closing = within(me, *target, me.attack_range()) ? "in-range" : "closing";
    const std::string chan = channelling(*target) ? " channelling" : "";
    branch("fight.hit", closing + " " + champ_tag(*target) + at(target->position()) + chan + " (" + detail + ")");
    engage(me, *target, enemies, false);
    return true;
}

std::pair<bool, std::string> Bot::chase_line(const bc::champion& me, const bc::champion& enemy) {
    // Rounds for us to close in and land enough basic attacks (allies in reach help), against its walk home.
    const auto position = enemy.position();
    const auto name = champ_tag(enemy);
    const auto spot = at(position);
    std::vector<bc::champion> allies;
    for (const auto& a : seen::champions(Team::ally))
        if (distance(a.position(), position) <= a.attack_range() + 1) allies.push_back(a);
    if (std::find(allies.begin(), allies.end(), me) == allies.end()) allies.push_back(me);
    PySum total;
    for (const auto& a : allies) total.add(a.strength());
    const double damage = total.value();
    const int home = steps_to_enemy_spawn(position);
    if (damage <= 0) return {false, name + format(" drop no-damage home %dr ", home) + spot};
    const int approach = std::max(0, distance(me.position(), position) - me.attack_range());
    const double rounds = approach + std::ceil(enemy.health() / damage);
    const bool keep = rounds <= home;
    return {keep, name + (keep ? " keep" : " drop") + format(" kill %.1fr home %dr dmg %.0f ", rounds, home, damage) + spot};
}

bool Bot::retreat(const bc::champion& me) {
    const auto& enemies = seen::champions(Team::enemy);
    const auto zone = threat_zone(enemies, false);
    if (!zone.is_set(me.position())) {
        farm(me, &zone, "retreat");
        branch_why = format("outside threat, %zu enemies; ", enemies.size()) + branch_why;
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
    if (!move_towards_closest(~zone)) {
        std::vector<bc::champion> near;
        for (const auto& e : enemies)
            if (within(me, e, me.attack_range())) near.push_back(e);
        const auto target = lowest(near);
        if (target && attack_if_in_range(*target)) swung = Target{*target};
    }
    std::string swing;
    if (swung) {
        if (const auto* champion = swung->champion()) {
            try {
                swing = ", swung " + champ_tag(*champion) + at(champion->position());
            } catch (const bc::error&) {
                swing = ", swung";
            }
        } else {
            // Camps have no champion name.
            swing = ", swung";
        }
    }
    branch("retreat.out", format("inside threat of %zu, step out", enemies.size()) + swing);
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

void Bot::farm(const bc::champion& me, const bc::bitboard* avoid, const std::string& stem) {
    if (route.empty()) {
        branch(stem + ".none", "no route");
        return;
    }
    int s = slot % static_cast<int>(route.size());
    auto camps = camps_at(s);
    // v71: a cleared target whose every reward tile already has an ally on it is being captured; one more
    // body adds nothing, so we hit the nearest visible group we may take instead of waiting beside it.
    // Only visible camps are looked at: a fresh pick_target() cost ~20k fuel here.
    if (FIX_TAKEN && camps.empty() && ready_at[s] <= static_cast<int>(bc::round_number())) {
        const auto& tiles = spawn_board(s);
        const auto position = me.position();
        if (!tiles.is_set(position)) {
            auto open = tiles;
            open &= ~bc::vision::champion_tiles(Team::ally);
            if (open.is_empty()) {
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
                    note(format("slot %d is being captured by allies, farm slot %d meanwhile", s, other));
                    s = other;
                    camps = camps_at(s);
                }
            }
        }
    }
    const int reach = me.attack_range();
    const auto center = centers[s];
    // Clusters have no strength, so a passing hit is free and should not be walked past.
    std::vector<bc::camp> passing;
    for (const auto& c : seen::camps())
        if (c.strength() <= 0 && within(me, c, reach) && can_take_camp(c)) passing.push_back(c);
    bool slot_in_reach = false;
    for (const auto& c : camps)
        if (within(me, c, reach)) slot_in_reach = true;
    if (!passing.empty() && !slot_in_reach) {
        const auto target = *lowest(passing);
        branch(stem + ".pass", "free cluster " + at(target.position()) + " in range, slot camps are not");
        engage(me, target, {}, true, avoid);
        return;
    }
    if (camps.empty()) {
        const auto& tiles = spawn_board(s);
        const auto flags = tile_flags(center);
        const int now = static_cast<int>(bc::round_number());
        if (ready_at[s] > now) {
            const int wait = ready_at[s] - now;
            branch(stem + ".wait", format("slot %d", s) + at(center) + " " + flags
                                       + format(" respawns in %d, stand %d off", wait, WAIT_RANGE));
            bc::bitboard ring;
            for (const auto tile : spawn_lists[s]) ring.draw_chebyshev(tile, WAIT_RANGE);
            move_towards_closest(ring & ~tiles, avoid);
        } else {
            branch(stem + ".walk", format("slot %d", s) + at(center) + " " + flags + " is up, no camp visible, walk onto it");
            move_towards_closest(tiles, avoid);
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
    branch(stem + ".camp", format("slot %d camp ", s) + at(target->position()) + " " + tile_flags(target->position())
                               + (in_reach.empty() ? ", closing" : ", in range"));
    engage(me, *target, {}, true, avoid);
}

// v71: enemies hidden in bushes

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
                    camp_spots->push_back({camp.spawn_position(), camp_kind_strength(kind),
                                           kind == bc::camp_type::monolith ? MONOLITH_REACH : CAMP_REACH});
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
    note(format("lost %d with no enemy in sight (camps in reach deal %.0f): enemy hidden in a bush", lost, camp_damage));
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
            note(std::string(placed ? "ward into bush " : "ward refused at ") + at(tile) + " to find the hidden attacker");
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
            note("hidden attacker revealed");
            hidden_until = -1;
            return false;
        }
    }
    // 2. Nothing to hit back at: step out of reach of every unseen bush it may be in.
    if (!hidden_zone.is_set(position)) return false;
    if (is_collector(me)) release_claim("hit by a hidden attacker");
    branch("hidden.dodge", format("lost %d with no enemy in sight; out of reach of the unseen bushes nearby", hidden_lost));
    move_towards_closest(~hidden_zone);
    return true;
}
