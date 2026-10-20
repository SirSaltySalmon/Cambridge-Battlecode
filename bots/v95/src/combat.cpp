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
    WHY("fight.hit", std::string(pick) + " at " + at(target->position()) + (detail.empty() ? "" : " (chase ring: " + detail + ")"));
    engage(me, *target, enemies, false);
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
    const int approach = std::max(0, distance(me.position(), position) - me.attack_range());
    const double rounds = approach + std::ceil(enemy.health() / damage);
    const bool keep = rounds <= home;
    std::string text;
    LOG(text = at(position) + (keep ? format(" chase: dies in %.0f, home in %d", rounds, home)
                                    : format(" drop: home in %d, dies in %.0f", home, rounds)););
    return {keep, text};
}

bool Bot::retreat(const bc::champion& me) {
    const auto& enemies = seen::champions(Team::enemy);
    const auto zone = threat_zone(enemies, false);
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
    if (!move_towards_closest(~zone)) {
        std::vector<bc::champion> near;
        for (const auto& e : enemies)
            if (within(me, e, me.attack_range())) near.push_back(e);
        const auto target = lowest(near);
        if (target && attack_if_in_range(*target)) swung = Target{*target};
    }
    WHY("retreat.out", std::string("inside enemy reach: step out") + (swung ? ", hit what was in reach first" : ""));
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

void Bot::farm(const bc::champion& me, const bc::bitboard* avoid, const std::string& stem) {
    if (route.empty()) {
        WHY(stem + ".none", "no route");
        return;
    }
    int s = slot % static_cast<int>(route.size());
    auto camps = camps_at(s);
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
    // v81 (MAGE_MONOLITH): a melee never hits a monolith and keeps out of an awake one's reach; mages take it.
    bc::bitboard keep_out;
    if (!is_mage(me.champion_name()) && !monoliths.empty() && monolith_delegated()) {
        if (monoliths.count(s)) {
            monolith_guard(me, s, avoid, stem);
            return;
        }
        keep_out = awake_monoliths();
        if (!keep_out.is_empty()) {
            if (avoid) keep_out |= *avoid;
            avoid = &keep_out;
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
        } else {
            WHY(stem + ".walk", format("s%d ", s) + at(center) + tile_flags(center) + " is up but out of sight: walk to it");
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
    WHY(stem + ".camp", format("s%d camp ", s) + at(target->position()) + tile_flags(target->position())
                            + (in_reach.empty() ? ": nearest, closing" : ": weakest in reach"));
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
    if ((ping_slot && !own_fresh) || !me.is_alive() || last_health < 0 || last_health_round != now - 1) return;
    if (PING_FILTER && mode == FIGHT) return;
    const int lost = last_health - static_cast<int>(me.health());
    if (lost < PING_MIN_LOSS) return;
    const auto position = me.position();
    bool attacked = false;
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
    NOTE(format("ping s%d: an enemy champion is hurting us, nobody beside us", best));
}

bool Bot::answer_ping(const bc::champion& me) {
    // A fight-mode champion with no enemy in sight walks to the pinged slot instead of farming on.
    if (!ping_slot) return false;
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
    if (distance(champion.position(), centers[*ping_slot]) > PING_ARRIVE) return false;
    bool built_ren = false;
    try {
        built_ren = champion.champion_name() == Champ::ren && emblem_count(champion) >= REN_CORE_EMBLEMS;
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
