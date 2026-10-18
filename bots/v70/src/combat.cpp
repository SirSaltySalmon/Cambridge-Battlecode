// Fighting, retreating, and clearing the current camp group.
#include "bot.h"
#include "seen.h"
#include "state.h"
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
    const int s = slot % static_cast<int>(route.size());
    const auto camps = camps_at(s);
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
