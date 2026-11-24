// Champion kits: when to cast, dash, kite, or hold range.
#include "bot.h"
#include "seen.h"
#include "state.h"
#include "trace.h"
#include <algorithm>
#include <cmath>
#include <tuple>

void Bot::engage(const bc::champion& me, const Target& target, const std::vector<bc::champion>& enemies, bool is_camp,
                 const bc::bitboard* avoid) {
    const bool mage = is_mage(me.champion_name());
    const char* cheese_why = nullptr;
    if (me.champion_name() == Champ::vesper_vale)
        engage_vesper(me, target, enemies, is_camp, avoid);
    else if (me.champion_name() == Champ::heart_of_ruin)
        engage_hor(me, target, enemies, is_camp, avoid);
    else if (VARG_SPLASH && is_camp && me.champion_name() == Champ::varg)
        engage_melee(me, varg_splash_target(me, target), is_camp, avoid);
    else if (AOE_CLEAN && !is_camp && me.champion_name() == Champ::varg)
        engage_melee(me, varg_fight_target(me, target), is_camp, avoid);
    // v94 (MAGE_OUTRANGE): the standoff only where the spell outranges the group (wolves); otherwise hit like a ranged.
    else if (mage && is_camp && MAGE_STANDOFF && group_hits(target) && (!MAGE_OUTRANGE || outranged(target))
             && (!V86_MAGE_RULE || mage_cheese(me, target, &cheese_why))) {
        if (cheese_why) SUB("", cheese_why);
        mage_camp(me, target, avoid);
    } else if (mage && is_camp && cheese_why) {
        // The v86 mage rule said no standoff: the camp is hit like any other (the same fallthrough as before).
        SUB("", cheese_why);
        if (me.attack_range() > 1)
            engage_ranged(me, target, enemies, is_camp, avoid);
        else
            engage_melee(me, target, is_camp, avoid);
    } else if (mage && !is_camp && MAGE_KITE)
        mage_fight(me, target, enemies, avoid);
    else if (me.attack_range() > 1)
        engage_ranged(me, target, enemies, is_camp, avoid);
    else
        engage_melee(me, target, is_camp, avoid);
}

void Bot::engage_melee(const bc::champion& me, const Target& target, bool is_camp, const bc::bitboard* avoid) {
    const auto kind = me.champion_name();
    const int reach = me.attack_range();

    if (kind == Champ::varg && under_threat(me) && cast_self(ability_of(me, Kind::shield))) SUB("", "shield: under threat");
    // v94 (VARG_CAMP_SHIELD): the camps scan only runs with the shield ready.
    if (VARG_CAMP_SHIELD && kind == Champ::varg && is_camp) {
        const auto shield = ability_of(me, Kind::shield);
        if (shield && shield->is_ready() && camp_exit_cost(me) >= VARG_SHIELD_CAMP_HIT && cast_self(shield))
            SUB("", "shield: awake camps hit us");
    }

    // Ren's stun excludes walking, so it is cast before any step. In a fight (the fight score said so): the
    // lowest-health enemy in stun range. On a camp target: once Ren owns REN_CORE_EMBLEMS, an enemy champion in stun range
    // comes first; otherwise the camp is stunned, but only once we already stand next to it, since the
    // cast costs the walk.
    if (kind == Champ::ren) {
        const auto stun = ability_of(me, Kind::cc);
        if (stun && stun->is_ready()) {
            const double range = ability_range(*stun);
            if (!is_camp) {
                // A fight (the fight score said fight): stun the lowest-health enemy in sight within stun
                // range, not only the current target. (Dashing first to set up a stun next turn was tried
                // and lost 15-19: the dash pulled Ren away from the team's fight.)
                std::vector<bc::champion> near;
                for (const auto& enemy : seen::champions(Team::enemy))
                    if (within(me, enemy, range)) near.push_back(enemy);
                if (const auto enemy = lowest(near); enemy && cast(stun, *enemy)) SUB("", "stun the weakest enemy in reach");
            } else {
                bool cast_on_enemy = false;
                if (emblem_count(me) >= REN_CORE_EMBLEMS) {
                    std::vector<bc::champion> near;
                    for (const auto& enemy : seen::champions(Team::enemy))
                        if (within(me, enemy, range)) near.push_back(enemy);
                    if (const auto enemy = lowest(near)) cast_on_enemy = cast(stun, *enemy);
                    if (cast_on_enemy) SUB("", "core built: stun an enemy before the camp");
                }
                if (!cast_on_enemy && stun->can_target_camps() && within(me, target, reach) && cast(stun, target))
                    SUB("", "stun the camp from beside it");
            }
        }
        if (!alive(target)) return;
    }

    // Ryth's area stun excludes walking, so it is cast before the step. The reset is the other cast.
    const bool stunned = kind == Champ::ryth && ryth_stun(me, target, is_camp);
    if (stunned) SUB("", "area spell before the step");
    if (stunned && !alive(target)) return;
    // v110: Marcella's spell excludes walking too, so it also goes before the step.
    const bool spelled = kind == Champ::marcella && marcella_cast(me, target, is_camp);
    if (spelled) SUB("", "damage spell from where we stand");
    if (spelled && !alive(target)) return;

    if (!within(me, target, reach) && !stunned && !spelled) {
        // The dashers close on a champion with a dash that leaves at most one walk (then walk on).
        Go opt;
        opt.avoid = avoid;
        // v138 (CHASE_STEP): a fleeing champion is chased one step ahead, diagonally across its run.
        const auto lead = is_camp ? std::nullopt : chase_step(*target.champion(), reach);
        const vec2 aim = lead ? *lead : target.position();
        if (lead) SUB(".step", "one step ahead, across its run");
        if (lead) opt.align = aim, opt.free_dash = CHASE_DASH;
        if (!is_camp && (kind == Champ::issac || kind == Champ::vendetta || kind == Champ::ren))
            close_in(opt, me, aim, reach, 1, lead.has_value());
        if (!go(tiles_around(aim, reach), opt) && lead && me.can_move()) {
            Go plain;
            plain.avoid = avoid;
            go(tiles_around(target.position(), reach), plain);
        }
        // The move may have taken the target out of sight (around a wall, into a bush): reading it would throw.
        if (!sighted(target)) {
            NOTE("moved in; the target dropped out of sight");
            return;
        }
    }

    // v150 probe (debug only): after a Vendetta kill, does the turn's cast come back with her cooldowns?
    bool cast_before = false;
    LOG(cast_before = kind == Champ::vendetta && me.can_cast(););
    auto kill_probe = [&](const char* when) {
        LOG(if (kind == Champ::vendetta && !alive(target)) {
            const auto reset = ability_of(me, Kind::attack_reset);
            const auto dash = ability_of(me, Kind::dash);
            note(format("vendetta kill probe (%s, %s): can_cast before %d after %d can_attack %d can_move %d reset_cd %d dash_cd %d", when,
                        is_camp ? "camp" : "champion", cast_before ? 1 : 0, me.can_cast() ? 1 : 0, me.can_attack() ? 1 : 0, me.can_move() ? 1 : 0,
                        reset ? static_cast<int>(reset->cooldown()) : -1, dash ? static_cast<int>(dash->cooldown()) : -1));
        });
    };
    const bool hit = attack_if_in_range(target);
    if (hit) kill_probe("first hit");
    // v150 (VENDETTA_KILL_RESET): her kill refreshed the reset; with the turn's cast unused it buys a hit on the next
    // target in reach.
    if (VENDETTA_KILL_RESET && hit && kind == Champ::vendetta && !alive(target) && me.can_cast()) {
        std::optional<Target> next;
        auto champion_next = [&] {
            for (const auto& e : seen::champions(Team::enemy))
                if (within(me, e, reach) && (!next || e.health() < next->health())) next = Target{e};
        };
        auto camp_next = [&] {
            for (const auto& c : group_camps())
                if (alive(Target{c}) && within(me, c, reach)) {
                    next = Target{c};
                    break;
                }
        };
        if (is_camp) camp_next();
        if (!next) champion_next();
        if (!next && !is_camp) camp_next();
        if (next && cast_self(ability_of(me, Kind::attack_reset))) {
            SUB("", "kill refreshed the reset: second hit on the next target");
            attack_if_in_range(*next);
        }
    }
    if (hit && !stunned && !spelled
        && (kind == Champ::issac || kind == Champ::vendetta || kind == Champ::varg || kind == Champ::ryth
            || kind == Champ::marcella)) {
        if (alive(target) && cast_self(ability_of(me, Kind::attack_reset))) {
            SUB("", "reset for a second hit");
            if (attack_if_in_range(target)) kill_probe("hit after a reset cast");
        }
    }
}

void Bot::engage_ranged(const bc::champion& me, const Target& target, const std::vector<bc::champion>& enemies,
                        bool is_camp, const bc::bitboard* avoid) {
    const auto kind = me.champion_name();
    const int reach = me.attack_range();
    const int g = distance(me.position(), target.position());

    // v101 (WILLOW_BACKLINE): against champions Willow heals from out of reach instead of walking in to attack.
    if (WILLOW_BACKLINE && kind == Champ::willow && !is_camp) {
        willow_backline(me, avoid);
        return;
    }
    // Healing and the attack reset share Willow's one cast per turn.
    const bool resets = kind == Champ::willow && !heal_lowest_ally(me);
    if (kind == Champ::cassi) {
        if (!is_camp) cassi_stun(me, enemies);
        // The damage spell excludes walking, so it waits until we are already outside camp reach.
        if (!backing_off_camp(me, target, is_camp, g)) {
            const auto damage = ability_of(me, Kind::damage);
            if (damage && g <= ability_range(*damage) && cast(damage, target)) SUB("", "damage spell from outside the camp's reach");
            if (!alive(target)) return;
        }
    }

    // Stellar's spells exclude walking. Step out of a camp before spending the cast.
    // v123: Prism's too (her stun-hit is the single spell); she still basic attacks after a cast, as a ranger.
    bool cast_spell = false;
    const bool prism = kind == Champ::prism;
    if ((kind == Champ::stellar || prism) && !backing_off_camp(me, target, is_camp, g)) {
        cast_spell = stellar_cast(me, target, is_camp);
        if (cast_spell && !alive(target)) return;
    }

    // v101 (WILLOW_CAMP_SAFETY): every awake camp that reaches Willow needs a sturdy ally closer to it than her, not
    // only the target (a group's other camp turned on her when the tank stepped away: 01boss-battle).
    if (WILLOW_CAMP_SAFETY && is_camp && kind == Champ::willow && willow_exposed(me)) {
        hold_camp(me, target, avoid);
        return;
    }
    // Hold outside camp reach until an allied melee is closer and taking the hits.
    if (is_camp && should_hold_back(me, target)) {
        hold_camp(me, target, avoid);
        return;
    }

    if (g <= reach) {
        // The attack is attempted before the reset check, so a spell that already spent the turn skips it.
        if ((!cast_spell || prism) && attack_if_in_range(target) && (resets || kind == Champ::stellar) && alive(target)) {
            if (resets && cast_self(ability_of(me, Kind::attack_reset))) {
                SUB("", "no ally to heal: reset for a second hit");
                attack_if_in_range(target);
            }
        }
    } else if (!cast_spell) {
        // v138 (CHASE_STEP): a fleeing champion is chased one step ahead, diagonally across its run.
        const auto lead = is_camp ? std::nullopt : chase_step(*target.champion(), reach);
        bool moved = false;
        if (lead) {
            SUB(".step", "one step ahead, across its run");
            Go opt;
            opt.avoid = avoid;
            opt.align = *lead;
            opt.free_dash = CHASE_DASH;
            moved = go(tiles_around(*lead, reach), opt);
        }
        if (!moved) move_towards_closest(tiles_around(target.position(), reach), avoid);
    }
}

bool Bot::backing_off_camp(const bc::champion& me, const Target& target, bool is_camp, int g) {
    return is_camp && should_hold_back(me, target) && g < camp_standoff(target);
}

int Bot::camp_standoff(const Target& camp) const {
    // Just outside this camp's own reach, and never closer than the usual gap.
    int reach;
    try {
        reach = camp.attack_range();
    } catch (const std::exception&) {
        const auto* c = camp.camp();
        reach = c ? camp_kind_reach(c->kind()) : RANGED_CAMP_STANDOFF - 1;
    }
    return std::max(RANGED_CAMP_STANDOFF, reach + 1);
}

bool Bot::should_hold_back(const bc::champion& me, const Target& camp) {
    // Clusters have no strength, so there is nothing to absorb. Willow still takes those.
    if (camp.strength() <= 0) return false;
    if (FIX_ABSORB && camp_tanked(me, camp)) return false;
    // v148-normal (FINISH_RANGED): no tank, but we finish the started group above FINISH_HEALTH: stay and finish it.
    if (FINISH_RANGED && me.champion_name() != Champ::willow && static_cast<int>(me.gem_count()) < CARRY_GEMS
        && seen::champions(Team::enemy).empty() && !hidden_active()) {
        if (const auto s = camp_slot(camp); s && mid_clear(*s)) {
            double rounds = 0;
            const double take = finish_take(me, *s, rounds);
            if (rounds > 0 && me.health() - take >= me.max_health() * FINISH_HEALTH) {
                SUB(".finish", format("no tank, but we finish it in ~%.0f rounds for ~%.0f health", rounds, take));
                return false;
            }
        }
    }
    // v135 (TANK_SWAP): a sturdy ally at the camp takes over from a tank stepping back; we keep hitting meanwhile.
    if (TANK_SWAP) {
        vec2 spot;
        int reach;
        try {
            spot = camp.position();
            reach = camp.attack_range();
        } catch (const std::exception&) {
            return true;
        }
        const auto& allies = seen::champions(Team::ally);
        const auto& tiles = seen::positions(Team::ally);
        for (std::size_t i = 0; i < allies.size() && i < tiles.size(); ++i)
            if (allies[i] != me && distance(tiles[i], spot) <= reach + 2 && allies[i].max_health() >= TANK_HEALTH
                && allies[i].health() >= allies[i].max_health() * RECALL_HEALTH && !channelling(allies[i]))
                return false;
    }
    for (const auto& ally : closer_melees(me, camp))
        if (coming_to_camp(ally, camp)) return false;
    // Other ranged may open a camp with no tank in sight. A melee that then appears,
    // still further out or clearly passing by, pulls them back. Willow never opens one.
    bool hold = me.champion_name() == Champ::willow || !visible_melees(me).empty();
    // v145 (RANGED_HOLD): with no melee in sight a ranged champion still waits, unless it wins the group alone or no
    // melee ally is alive.
    if (RANGED_HOLD && !hold) {
        const auto s = camp_slot(camp);
        bool melee_alive = false;
        for (const auto& a : seen::roster(Team::ally)) {
            try {
                if (a != me && a.is_alive() && a.attack_range() <= 1) melee_alive = true;
            } catch (const bc::error&) {
            }
        }
        hold = melee_alive && !(s && solo_win(*s));
        if (hold) SUB(".nomelee", "no melee in sight: wait for one rather than open it alone");
    }
    // v76 (FIX_STUCK_HOLD): nobody may come (Willow waited ~430 rounds beside one camp on rushbait_v2), so after
    // HOLD_LIMIT rounds at the same camp we open it ourselves.
    if (FIX_STUCK_HOLD && hold) {
        vec2 spot;
        try {
            spot = camp.position();
        } catch (const std::exception&) {
            return hold;
        }
        const int now = static_cast<int>(bc::round_number());
        if (!hold_spot || *hold_spot != spot || now - hold_last > 2) {
            hold_spot = spot;
            hold_from = now;
        }
        hold_last = now;
        if (now - hold_from >= HOLD_LIMIT) {
            // v143 (WILLOW_REGROUP): Willow never opens it herself: she goes to find the team.
            if (!(WILLOW_REGROUP && me.champion_name() == Champ::willow)) return false;
            if (regroup_since < 0) {
                regroup_since = now;
                NOTE(format("held %d rounds at ", now - hold_from) + at(spot) + " with no tank: regroup with the team");
            }
        }
    }
    return hold;
}

bool Bot::camp_tanked(const bc::champion& me, const Target& camp) {
    // A camp hits the nearest champion in its reach. A sturdy ally already in that reach and closer than we
    // are takes the hits whatever it is doing (a collector on the next reward, say), so there is nothing to
    // wait for. One about to recall away is not counted.
    int reach;
    vec2 spot;
    try {
        reach = camp.attack_range();
        spot = camp.position();
    } catch (const std::exception&) {
        return false;
    }
    // Compared with where we would shoot from, so stepping into range does not undo the answer.
    const int my_gap = std::min(distance(me.position(), spot), me.attack_range());
    for (const auto& ally : seen::champions(Team::ally)) {
        if (ally == me) continue;
        const int gap = distance(ally.position(), spot);
        // v148-normal (CAMP_TIE_ORDER): level with us, it takes the camp when it acts first.
        if (gap > my_gap || (gap == my_gap && !acts_before_me(ally)) || gap > reach) continue;
        if (ally.attack_range() > 1 && ally.max_health() <= SQUISHY_HEALTH) continue;
        if (ally.is_recalling()) continue;
        // v91 (V91_TANK_HURT): one under the recall bar is stepping away to recall, not tanking (a mage kept hitting
        // from inside the camp's reach while Issac dodged out to recall).
        // v101 (WILLOW_CAMP_SAFETY): Willow wants a tank that will stay, not one about to leave.
        const double bar = WILLOW_CAMP_SAFETY && me.champion_name() == Champ::willow ? WILLOW_TANK_HEALTH : RECALL_HEALTH;
        if (V91_TANK_HURT && ally.health() < ally.max_health() * bar) {
            // v148-normal (STAY_TANKED): it steps behind another sturdy ally in the camp's reach, who takes over.
            if (!STAY_TANKED || me.champion_name() == Champ::willow || ally.health() < ally.max_health() * FINISH_HEALTH)
                continue;
            bool backup = false;
            for (const auto& other : seen::champions(Team::ally))
                if (other != me && other != ally && distance(other.position(), spot) <= reach + 1
                    && other.max_health() >= TANK_HEALTH && other.health() >= other.max_health() * TANK_TAKE_HEALTH
                    && !channelling(other)) {
                    backup = true;
                    break;
                }
            if (!backup) continue;
        }
        return true;
    }
    return false;
}

std::vector<bc::champion> Bot::visible_melees(const bc::champion& me) {
    std::vector<bc::champion> melees;
    for (const auto& a : seen::champions(Team::ally))
        if (a != me && a.attack_range() <= 1) melees.push_back(a);
    return melees;
}

std::vector<bc::champion> Bot::closer_melees(const bc::champion& me, const Target& camp) {
    const int my_gap = distance(me.position(), camp.position());
    std::vector<bc::champion> closer;
    for (const auto& a : visible_melees(me))
        if (distance(a.position(), camp.position()) < my_gap) closer.push_back(a);
    return closer;
}

std::optional<int> Bot::camp_slot(const Target& camp) const {
    const auto* c = camp.camp();
    if (!c) return std::nullopt;
    const int s = slot_at(c->spawn_position());
    if (s < 0) return std::nullopt;
    return s;
}

bool Bot::coming_to_camp(const bc::champion& ally, const Target& camp) {
    // Closer is not enough: a carrier, a collector, or a recall is going somewhere else,
    // and a last step that increased the gap is leaving. The first sighting has no step yet.
    if (channelling(ally)) return false;
    int ident;
    try {
        if (ally.is_teleporting()) return false;
        ident = cached_ident(ally);
    } catch (const bc::error&) {
        return false;
    }
    if (carrier && ident == carrier) return false;
    // v91 (V91_TANK_HURT): under the recall bar it is about to step away and recall, even before its first step out.
    try {
        if (V91_TANK_HURT && ally.health() < ally.max_health() * RECALL_HEALTH) return false;
    } catch (const bc::error&) {
    }
    const auto s = camp_slot(camp);
    if (capture_slot != NO_CAPTURE && ident == collector && s != std::optional<int>{capture_slot}) return false;
    const auto& prev = ally_at.at(ident);
    const int now = static_cast<int>(bc::round_number());
    vec2 here, camp_pos;
    try {
        here = ally.position();
        camp_pos = camp.position();
    } catch (const bc::error&) {
        return false;
    }
    if (prev && now - prev->first <= 1) {
        const auto then = prev->second;
        if (distance(here, camp_pos) > distance(then, camp_pos)) return false;
        bool hurt;
        try {
            hurt = ally.health() < ally.max_health() * RECALL_HEALTH;
        } catch (const bc::error&) {
            hurt = false;
        }
        if (hurt && !ally_spawn_list.empty()) {
            int spawn_now = distance(here, ally_spawn_list.front()), spawn_then = distance(then, ally_spawn_list.front());
            for (const auto tile : ally_spawn_list) {
                spawn_now = std::min(spawn_now, distance(here, tile));
                spawn_then = std::min(spawn_then, distance(then, tile));
            }
            if (spawn_now < spawn_then) return false;
        }
    } else if (s && !route.empty() && *s != slot % static_cast<int>(route.size())) {
        // A side camp is not the shared target, so a first glance does not make them its tank.
        int reach;
        try {
            reach = camp.attack_range();
        } catch (const std::exception&) {
            const auto* c = camp.camp();
            reach = c ? camp_kind_reach(c->kind()) : RANGED_CAMP_STANDOFF - 1;
        }
        if (distance(here, camp_pos) > reach) return false;
    }
    return true;
}

int Bot::cached_ident(const bc::champion& champion) const {
    // champ_id(type) + 1, read once per champion: a type never changes, and the read is an engine call each time.
    const auto handle = static_cast<std::uint32_t>(static_cast<bc::handle_champion_t>(champion));
    for (std::size_t i = 0; i < ident_count; ++i)
        if (ident_cache[i].first == handle) return ident_cache[i].second;
    const int ident = champ_id(champion.champion_name()) + 1;
    if (ident_count < ident_cache.size()) ident_cache[ident_count++] = {handle, ident};
    return ident;
}

void Bot::remember_allies() {
    // Kept across the turn, then refreshed. Melees act first, so the new point is where they stepped.
    const int now = static_cast<int>(bc::round_number());
    for (auto& sighting : ally_at)
        if (sighting && now - sighting->first > 1) sighting.reset();
    std::optional<int> mine;
    try {
        mine = cached_ident(bc::self());
    } catch (const bc::error&) {
    }
    // v133t: tiles from the vision list (read with it).
    const auto& allies = seen::champions(Team::ally);
    const auto& tiles = seen::positions(Team::ally);
    for (std::size_t i = 0; i < allies.size(); ++i) {
        const auto& ally = allies[i];
        try {
            const int ident = cached_ident(ally);
            if (mine && ident == *mine) continue;
            ally_at.at(ident) = std::make_pair(now, tiles[i]);
            // v86: kept for ALLY_GRACE rounds (an ally beside a bush may be hidden in it); a recall is not hidden.
            if (!channelling(ally)) ally_seen.at(ident) = ally_at.at(ident);
        } catch (const bc::error&) {
            continue;
        }
    }
}

void Bot::hold_camp(const bc::champion& me, const Target& camp, const bc::bitboard* avoid) {
    const int g = camp_standoff(camp);
    const auto danger = tiles_around(camp.position(), g - 1);
    const bool willow = me.champion_name() == Champ::willow;
    if (willow && willow_follow(me, camp, danger, avoid)) return;
    const auto closer = closer_melees(me, camp);
    bool any_coming = false;
    for (const auto& ally : closer)
        if (coming_to_camp(ally, camp)) any_coming = true;
    if (!closer.empty() && !any_coming)
        SUB(".hold", format("waits %d off the camp: the closer melee is going elsewhere", g));
    else
        SUB(".hold", format("waits %d off the camp until a melee is closer and tanks", g));
    auto ring = tiles_around(camp.position(), g) & ~danger;
    // v101 (WILLOW_CAMP_SAFETY): out of every visible camp's reach, when such a tile is on the ring.
    if (WILLOW_CAMP_SAFETY && willow) {
        const auto safe = ring & ~threat_zone({}, true);
        if (!safe.is_empty()) ring = safe;
    }
    move_towards_closest(ring, avoid);
}

bool Bot::willow_regroup(const bc::champion& me) {
    // v143 (WILLOW_REGROUP): the freshest ally sighting away from us, else our spawn; over with an ally beside us.
    const int now = static_cast<int>(bc::round_number());
    if (ally_near(me) || now - regroup_since > REGROUP_MAX) {
        NOTE(ally_near(me) ? "regroup over: an ally is with us" : "regroup over: timed out");
        regroup_since = -1;
        return false;
    }
    const auto position = me.position();
    std::optional<vec2> goal;
    int freshest = now - REGROUP_SEEN - 1;
    for (const auto& sighting : ally_seen)
        if (sighting && sighting->first > freshest && distance(sighting->second, position) > TEAM_RANGE) {
            freshest = sighting->first;
            goal = sighting->second;
        }
    if (goal) {
        WHY("willow.regroup", "no tank came: to the ally last seen at " + at(*goal) + format(" (r%d)", freshest));
        move_towards_closest(tiles_around(*goal, 1));
        return true;
    }
    if (seen::ally_spawn().is_set(position)) {
        WHY("willow.regroup", "no tank came: waits on spawn for the team");
        return true;
    }
    WHY("willow.regroup", "no tank came and no ally seen lately: back to spawn");
    return recall_to_shop(me) || move_towards_closest(seen::ally_spawn());
}

bool Bot::willow_follow(const bc::champion& me, const Target& camp, const bc::bitboard& danger, const bc::bitboard* avoid) {
    // A visible ally still behind us is the team, so meet them. Someone already as close
    // as we are is arriving at this camp, and the ring is the regroup point.
    std::vector<bc::champion> allies, melees;
    for (const auto& a : seen::champions(Team::ally))
        if (a != me) {
            allies.push_back(a);
            if (a.attack_range() <= 1) melees.push_back(a);
        }
    if (allies.empty()) return false;
    const auto& pool = melees.empty() ? allies : melees;
    auto anchor = pool.front();
    for (const auto& a : pool)
        if (distance(me.position(), a.position()) < distance(me.position(), anchor.position())) anchor = a;
    if (distance(anchor.position(), camp.position()) <= distance(me.position(), camp.position())) return false;
    const auto near = tiles_around(anchor.position(), 1) & ~danger;
    if (near.is_empty()) return false;
    SUB(".follow", "the team is still behind: meet it at " + at(anchor.position()) + ", outside the camp");
    move_towards_closest(near, avoid);
    return true;
}

void Bot::aoe_scan(const Target& target, bool is_camp) {
    // v120 (AOE_CLEAN): a hit wakes the camp's whole group, which then hits the nearest champion. On a camp the area
    // counts only the target's group; every camp of another group that hits back (not clusters only) is kept clear.
    aoe_places.clear();
    aoe_clear.clear();
    const auto s = is_camp ? camp_slot(target) : std::nullopt;
    // v135 (SAFE_LAND): the same rule as HoR's landing (area_ok): what is not ours to wake stays out of the area.
    const int farm_s = route.empty() ? -1 : slot % static_cast<int>(route.size());
    // Short of fuel: v134's rule (every other group kept clear), without the per-group checks.
    const bool spare = SAFE_LAND && bc::fuel_remaining() >= V135_SPARE_FUEL;
    const bool engaged = spare && camp_heat(bc::self()) > 0;
    // area_ok once per group (it reads camps and may run the solo math): 0 unknown, 1 ok, 2 not.
    aoe_ok_memo.assign(route.size(), 0);
    auto ok = [&](int g) {
        if (!aoe_ok_memo[g]) aoe_ok_memo[g] = area_ok(g, farm_s, engaged) ? 1 : 2;
        return aoe_ok_memo[g] == 1;
    };
    for (const auto& c : seen::camps()) {
        const int cs = slot_at(c.spawn_position());
        if (s && cs == *s) {
            aoe_places.push_back(c.position());
            continue;
        }
        if (cs >= 0 && clusters_only[cs]) continue;
        if (spare && cs >= 0 && ok(cs)) continue;
        aoe_clear.push_back(c.position());
    }
    // v135 (SAFE_LAND): live groups' tiles out of sight (a bush, behind a wall) are kept clear too; spells reach 4 and
    // areas 1, so only tiles within 6 of us matter.
    if (spare && !route.empty()) {
        const auto position = seen::self_position();
        const int now = static_cast<int>(bc::round_number());
        static bc::bitboard sight{bc::uninitialized};
        bool read = false;
        if (group_radius.size() != route.size()) {
            group_radius.assign(route.size(), 0);
            for (int g = 0; g < static_cast<int>(route.size()); ++g)
                for (const auto t : spawn_lists[g]) group_radius[g] = std::max(group_radius[g], distance(centers[g], t));
        }
        for (int g = 0; g < static_cast<int>(route.size()); ++g) {
            if (distance(position, centers[g]) > 6 + group_radius[g]) continue;
            if ((s && g == *s) || clusters_only[g] || ready_at[g] > now || rewards.count(g)) continue;
            if (ok(g)) continue;
            for (const auto t : spawn_lists[g]) {
                if (distance(position, t) > 6) continue;
                if (!read) {
                    bc::vision::visible_tiles_into(sight);
                    read = true;
                }
                if (!sight.is_set(t)) aoe_clear.push_back(t);
            }
        }
    }
    if (!is_camp)
        for (const auto& e : seen::champions(Team::enemy))
            if (alive(e)) aoe_places.push_back(e.position());
    if (const auto focus = target.position(); std::find(aoe_places.begin(), aoe_places.end(), focus) == aoe_places.end())
        aoe_places.push_back(focus);
}

bool Bot::clear_of(vec2 center, int radius) const {
    return std::none_of(aoe_clear.begin(), aoe_clear.end(), [&](vec2 c) { return distance(center, c) <= radius; });
}

bool Bot::ryth_stun(const bc::champion& me, const Target& target, bool is_camp) {
    // Ground stun in radius one, range 4. One body is enough; a pack is taken when the centre covers it.
    const auto focus = target.position();
    const auto area = ability_of(me, Kind::aoe_damage);
    if (AOE_CLEAN) {
        const auto stun = ability_of(me, Kind::cc);
        if (!(area && area->is_ready()) && !(stun && stun->is_ready())) return false;
        aoe_scan(target, is_camp);
        if (cast_aoe(area, aoe_places, focus, 1, &aoe_clear)) return true;
        if (!stun || !within(me, target, ability_range(*stun))) return false;
        if (is_camp && !stun->can_target_camps()) return false;
        if (stun->can_target_ground()) return clear_of(focus, static_cast<int>(ability_radius(*stun))) && cast(stun, focus);
        return cast(stun, target);
    }
    const auto places = spell_places(target, is_camp);
    if (cast_aoe(area, places, focus, 1)) return true;
    const auto stun = ability_of(me, Kind::cc);
    if (!stun || !within(me, target, ability_range(*stun))) return false;
    if (is_camp && !stun->can_target_camps()) return false;
    if (stun->can_target_ground()) return cast(stun, focus);
    return cast(stun, target);
}

bool Bot::marcella_cast(const bc::champion& me, const Target& target, bool is_camp) {
    // 2 x Focus at range 4. Beside the target it costs nothing, but in a fight the enhanced reset (its second hit
    // stuns) comes first while ready (MARCELLA_STUN_RESET). Out of reach a camp is walked to; an enemy champion one
    // step away is too, and one further off takes the spell instead of the walk.
    const auto spell = ability_of(me, Kind::damage);
    if (!spell || !spell->is_ready() || (is_camp && !spell->can_target_camps())) return false;
    const int g = distance(me.position(), target.position());
    const int reach = me.attack_range();
    if (g > ability_range(*spell)) return false;
    if (g <= reach) {
        if (!is_camp && MARCELLA_STUN_RESET) {
            const auto reset = ability_of(me, Kind::attack_reset);
            if (reset && reset->is_ready()) return false;
        }
        return cast(spell, target);
    }
    if (is_camp || g <= reach + 1) return false;
    return cast(spell, target);
}

bool Bot::stellar_cast(const bc::champion& me, const Target& target, bool is_camp) {
    // Single hit is 2x Focus. The area is 1.4x, so it wins once two bodies are inside it.
    // v123: Prism's single hit is her stun (1.5x Focus), her area 1x: the area wins once two bodies are inside it.
    auto single = ability_of(me, Kind::damage);
    if (!single && me.champion_name() == Champ::prism) single = ability_of(me, Kind::cc);
    const auto area = ability_of(me, Kind::aoe_damage);
    if (AOE_CLEAN) aoe_scan(target, is_camp);
    const auto places = AOE_CLEAN ? aoe_places : spell_places(target, is_camp);
    const auto focus = target.position();
    bool single_ok = false;
    if (single && single->is_ready() && (!is_camp || single->can_target_camps())) {
        try {
            single_ok = within(me, target, ability_range(*single));
        } catch (const std::exception&) {
            single_ok = false;
        }
    }
    const double single_hit = single_ok ? spell_damage(single) : 0.0;
    std::optional<vec2> center;
    int hits = 0;
    double area_hit = 0.0;
    if (area && area->is_ready()) {
        try {
            const auto aim = aoe_aim(me.position(), places, ability_range(*area), ability_radius(*area), focus,
                                     AOE_CLEAN ? &aoe_clear : nullptr);
            center = aim.first;
            hits = aim.second;
            area_hit = spell_damage(area);
        } catch (const std::exception&) {
            center.reset();
        }
    }
    bool use_area = false;
    if (center && hits > 0) {
        if (single_ok && single_hit > 0 && area_hit > 0)
            use_area = hits * area_hit > single_hit;
        else if (single_ok)
            use_area = hits >= 2;
        else
            use_area = true;
    }
    if (use_area && cast(area, *center)) {
        SUB("", format("area spell: %d in it", hits));
        return true;
    }
    if (single_ok && cast(single, target)) {
        SUB("", "single spell");
        return true;
    }
    return false;
}

bool Bot::cassi_stun(const bc::champion& me, const std::vector<bc::champion>& enemies) {
    const auto stun = ability_of(me, Kind::cc);
    if (!stun || !stun->is_ready()) return false;
    std::vector<bc::champion> near, near_channel;
    for (const auto& e : enemies)
        if (within(me, e, ability_range(*stun))) {
            near.push_back(e);
            if (channelling(e)) near_channel.push_back(e);
        }
    auto target = lowest(near_channel);
    const bool channel = target.has_value();
    if (!target) target = lowest(near);
    if (!target || !cast(stun, *target)) return false;
    SUB("", channel ? "stun a channelling enemy" : "stun the weakest enemy in reach");
    return true;
}

bool Bot::heal_lowest_ally(const bc::champion& me) {
    const auto heal = ability_of(me, Kind::heal);
    if (!heal || !heal->is_ready()) return false;
    std::optional<bc::champion> target;
    double best = 0;
    // v85 (WILLOW_HEAL_PICK): the ally who gains most. Effective heal is the missing health up to the heal's size;
    // an ally low on health or with an enemy champion near counts double. Any hurt ally can still be picked.
    const double amount = 3.0 * me.focus();
    const auto& enemies = seen::champions(Team::enemy);
    for (const auto& a : seen::champions(Team::ally))
        if (a != me && a.health() < a.max_health() && within(me, a, ability_range(*heal))) {
            // v101 (WILLOW_HEAL_SKIP_RECALL): a recalling ally, or one on our spawn, is healed to full anyway
            // (5% of her heals went to recalling allies: ~9% of her useful healing).
            if (WILLOW_HEAL_SKIP_RECALL && (a.is_recalling() || seen::ally_spawn().is_set(a.position()))) continue;
            // v101 (WILLOW_HEAL_THRESHOLD): only an ally missing at least HEAL_MIN_SHARE of the heal.
            if (WILLOW_HEAL_THRESHOLD && a.max_health() - a.health() < HEAL_MIN_SHARE * amount) continue;
            double score;
            if (WILLOW_HEAL_PICK) {
                score = std::min(amount, static_cast<double>(a.max_health() - a.health()));
                bool danger = a.health() < a.max_health() * HEAL_LOW;
                for (const auto& e : enemies)
                    if (!danger && distance(a.position(), e.position()) <= HEAL_DANGER) danger = true;
                if (danger) score *= 2;
                if (PRISM_WILLOW && prism_team() && a.champion_name() == Champ::prism) score *= PRISM_HEAL_WEIGHT;
            } else {
                score = 1.0 - static_cast<double>(a.health()) / a.max_health();
            }
            if (!target || score > best) {
                target = a;
                best = score;
            }
        }
    if (!target || !cast(heal, *target)) return false;
    NOTE("heals the ally in reach who gains most");
    return true;
}

void Bot::engage_vesper(const bc::champion& me, const Target& target, const std::vector<bc::champion>& enemies,
                        bool is_camp, const bc::bitboard* avoid) {
    // One ability per turn: attack reset doubles a hit; otherwise the dash closes in or kites out.
    const int reach = me.attack_range();
    if (is_camp && should_hold_back(me, target)) {
        hold_camp(me, target, avoid);
        return;
    }
    // v218 (MICRO_VESPER): standing here would drop us under the recall bar within MICRO_PROJECT enemy turns (or costs
    // MICRO_DMG_BAR of our health this turn): the micro fires here, or dashes and fires, or steps out.
    // MICRO_APPROACH: otherwise the approach keeps out of uncovered enemy melee reach (and recent unsafe tiles).
    if ((MICRO_VESPER || MICRO_APPROACH) && !is_camp) {
        static bc::bitboard micro_unsafe{bc::uninitialized};
        bool has_unsafe = false;
        if (micro_vesper(me, target, avoid, micro_unsafe, has_unsafe)) return;
        if (has_unsafe) {
            if (avoid) micro_unsafe |= *avoid;
            avoid = &micro_unsafe;
        }
    }

    // Ranged walking uses up the attack, so dash straight into range when it is close enough (and fire), else walk.
    if (!within(me, target, reach)) {
        const auto before = me.position();
        Go opt;
        opt.avoid = avoid;
        opt.walk_after_dash = false;
        // v138 (CHASE_STEP): a fleeing champion is chased one step ahead, diagonally across its run.
        const auto lead = is_camp ? std::nullopt : chase_step(*target.champion(), reach);
        const vec2 aim = lead ? *lead : target.position();
        if (lead) SUB(".step", "one step ahead, across its run");
        if (lead) opt.align = aim, opt.free_dash = CHASE_DASH && CHASE_DASH_VESPER;
        close_in(opt, me, aim, reach, 0, lead.has_value());
        if (!go(tiles_around(aim, reach), opt) && lead && me.position() == before) {
            Go plain;
            plain.avoid = avoid;
            plain.walk_after_dash = false;
            close_in(plain, me, target.position(), reach, 0);
            go(tiles_around(target.position(), reach), plain);
        }
        // A dash leaves the walk (and with it the attack); a walk spent both.
        if (me.position() == before || !me.can_move()) return;
        SUB("", "dash into range");
        if (!sighted(target)) {
            NOTE("dashed in; the target dropped out of sight");
            return;
        }
        if (within(me, target, reach)) attack_if_in_range(target);
        return;
    }

    if (!attack_if_in_range(target)) return;
    if (alive(target) && cast_self(ability_of(me, Kind::attack_reset))) {
        SUB("", "reset for a second hit");
        attack_if_in_range(target);
    } else if (!is_camp && GO_KITE && sighted(target) && alive(target) && vesper_follow(me, enemies, target, reach)) {
        SUB("", "dash after the fleeing target after firing");
    } else if (!is_camp && vesper_kite(me, enemies, target, reach)) {
        SUB("", "dash out of enemy reach after firing");
    }
}

bool Bot::vesper_kite(const bc::champion& me, const std::vector<bc::champion>& enemies, const Target& target, int reach) {
    // After firing, dash to the tile furthest outside enemy reach, preferring ones still in range.
    std::vector<std::pair<vec2, int>> threats;
    for (const auto& e : alive(enemies)) threats.emplace_back(e.position(), e.attack_range());
    if (threats.empty()) return false;
    const auto aim = alive(target) ? std::optional<vec2>{target.position()} : std::nullopt;
    auto margin = [threats](vec2 tile) {
        int best = distance(tile, threats.front().first) - threats.front().second;
        for (const auto& [spot, reach_of] : threats) best = std::min(best, distance(tile, spot) - reach_of);
        return best;
    };
    const auto position = me.position();
    const int here = margin(position);
    if (here > 0) return false;
    bc::bitboard out;
    for (const auto& [spot, reach_of] : threats) out.draw_chebyshev(spot, static_cast<float>(reach_of));
    out = ~out;
    Go opt;
    opt.walk = false;
    opt.guard_dash = GO_GUARD_DASH;
    opt.rank = [margin, here, aim, reach](vec2 t) -> std::optional<long> {
        const int m = margin(t);
        if (m <= here) return std::nullopt;
        return (m > 0 ? 1'000'000L : 0L) + (aim && distance(t, *aim) <= reach ? 1'000L : 0L) + m;
    };
    return go(out, opt);
}

bool Bot::vesper_follow(const bc::champion& me, const std::vector<bc::champion>& enemies, const Target& target, int reach) {
    // v133 (GO_KITE): the target walked away from us since last round: after firing, dash toward it so it stays in reach
    // next turn, onto a tile no other enemy reaches.
    const auto* enemy = target.champion();
    if (!enemy || !fled(me, *enemy)) return false;
    const auto aim = enemy->position();
    const int gap = distance(me.position(), aim);
    std::vector<std::pair<vec2, int>> others;
    for (const auto& e : alive(enemies))
        if (e != *enemy) others.emplace_back(e.position(), e.attack_range());
    Go opt;
    opt.walk = false;
    opt.dash = DashUse::urgent;
    opt.guard_dash = GO_GUARD_DASH;
    opt.rank = [others, aim, gap, reach](vec2 t) -> std::optional<long> {
        const int g = distance(t, aim);
        if (g >= gap || g > reach) return std::nullopt;
        for (const auto& [spot, reach_of] : others)
            if (distance(t, spot) <= reach_of) return std::nullopt;
        return -g;
    };
    bc::bitboard ring;
    ring.draw_chebyshev(aim, static_cast<float>(reach - 1));
    return go(ring, opt);
}

// v81 mages (Cassi, Stellar): spells from outside reach, basic attacks only where they cost little.

bool Bot::mage_spell_ready(const bc::champion& me) {
    for (const auto kind : {Kind::damage, Kind::aoe_damage}) {
        const auto spell = ability_of(me, kind);
        if (spell && spell->is_ready()) return true;
    }
    return false;
}

bool Bot::mage_cast(const bc::champion& me, const Target& target, bool is_camp) {
    if (me.champion_name() == Champ::stellar) return stellar_cast(me, target, is_camp);
    const auto damage = ability_of(me, Kind::damage);
    if (!damage || !damage->is_ready() || (is_camp && !damage->can_target_camps())) return false;
    return within(me, target, ability_range(*damage)) && cast(damage, target);
}

bc::bitboard Bot::camp_danger(const Target& camp) {
    // The target's whole group wakes when it is hit; any other camp already hurt is awake too.
    bc::bitboard zone;
    const auto s = camp_slot(camp);
    for (const auto& c : seen::camps()) {
        if (c.strength() <= 0) continue;
        const bool group = s && slot_at(c.spawn_position()) == *s;
        if (group || c.health() < c.max_health()) zone.draw_chebyshev(c.position(), static_cast<float>(c.attack_range()));
    }
    zone.draw_chebyshev(camp.position(), static_cast<float>(camp.attack_range()));
    return zone;
}

bool Bot::outranged(const Target& camp) {
    // Every hitting camp of the group in sight reaches less than a spell (v94: only wolves).
    if (camp.strength() > 0 && camp.attack_range() >= MAGE_CAST_RANGE) return false;
    const auto s = camp_slot(camp);
    if (!s) return true;
    for (const auto& c : seen::camps())
        if (c.strength() > 0 && slot_at(c.spawn_position()) == *s && c.attack_range() >= MAGE_CAST_RANGE) return false;
    return true;
}

Target Bot::varg_splash_target(const bc::champion& me, const Target& target) {
    // Varg's basic attack splashes Strength onto every other camp within radius one of what it hits. In reach, he picks
    // the camp of the target's group whose splash covers the most other living camps (then the weakest); the target
    // stays unless another covers strictly more. Only the group's camps count, so no other group wakes.
    // v120 (AOE_CLEAN): a camp whose splash reaches another group that hits back is the last choice: a clean camp of the
    // group in reach comes first, then the nearest clean one out of reach (walked to); with none, the best as before.
    const auto s = camp_slot(target);
    if (!s) return target;
    if (AOE_CLEAN) aoe_scan(target, true);
    const auto& camps = seen::camps();
    std::vector<bc::camp> group;
    for (const auto& c : camps)
        if (slot_at(c.spawn_position()) == *s && alive(c)) group.push_back(c);
    auto splash = [&](const bc::camp& hit) {
        int count = 0;
        for (const auto& c : group)
            if (!(c == hit) && distance(c.position(), hit.position()) <= 1) ++count;
        return count;
    };
    auto clean = [&](const bc::camp& hit) { return !AOE_CLEAN || clear_of(hit.position(), 1); };
    const auto* current = target.camp();
    const int reach = me.attack_range();
    const auto position = me.position();
    std::optional<bc::camp> best;
    bool best_clean = !current || clean(*current);
    int best_count = current ? splash(*current) : 0;
    for (const auto& c : group) {
        if (current && c == *current) continue;
        if (distance(position, c.position()) > reach) continue;
        const bool ok = clean(c);
        const int count = splash(c);
        if (std::tie(ok, count) > std::tie(best_clean, best_count)
            || (best && ok == best_clean && count == best_count && c.health() < best->health())) {
            best = c;
            best_clean = ok;
            best_count = count;
        }
    }
    if (!best_clean) {
        std::optional<bc::camp> near;
        for (const auto& c : group)
            if (clean(c) && (!near || distance(position, c.position()) < distance(position, near->position()))) near = c;
        if (near) {
            SUB("", "splash would wake another group: walk to a clean camp");
            return *near;
        }
    }
    if (!best) return target;
    SUB("", "hit the camp whose splash covers the most");
    return *best;
}

Target Bot::varg_fight_target(const bc::champion& me, const Target& target) {
    // v120 (AOE_CLEAN): in a fight, an enemy whose splash would wake a camp gives way to another in reach that does not.
    if (within(me, target, me.attack_range())) {
        aoe_scan(target, false);
        if (clear_of(target.position(), 1)) return target;
        for (const auto& e : seen::champions(Team::enemy))
            if (within(me, e, me.attack_range()) && alive(e) && clear_of(e.position(), 1)) {
                SUB("", "splash would wake a camp: hit another enemy");
                return Target{e};
            }
    }
    return target;
}

bool Bot::group_hits(const Target& camp) {
    // The camp or any camp of its group in sight hits back (a cluster beside wolves is no free hit).
    if (camp.strength() > 0) return true;
    const auto s = camp_slot(camp);
    if (!s) return false;
    for (const auto& c : seen::camps())
        if (c.strength() > 0 && slot_at(c.spawn_position()) == *s) return true;
    return false;
}

std::pair<bool, int> Bot::finish_cost(const bc::champion& me, const Target& camp) {
    // Rounds to finish the camp with basic attacks (ours and those of allies already in reach of it), times its
    // strength: what the camp would hit us for meanwhile if we were the one in its reach.
    const auto spot = camp.position();
    // v134 (ITEM_DAMAGE): Monster and Collectors add to camp damage; Jungle and Durability cut what the camp hits us for.
    double damage = me.strength() * camp_dmg_mult(owned_of(me).passives);
    for (const auto& ally : seen::champions(Team::ally))
        if (ally != me && distance(ally.position(), spot) <= ally.attack_range())
            damage += ally.strength() * camp_dmg_mult(owned_of(ally).passives);
    if (damage <= 0) return {false, 0};
    // Every camp of the group is awake while we fight it (a cluster beside a monolith is no free hit).
    double strength = camp.strength();
    if (const auto s = camp_slot(camp)) {
        strength = 0;
        for (const auto& c : seen::camps())
            if (slot_at(c.spawn_position()) == *s && c.health() > 0) strength += c.strength();
    }
    const int cost = static_cast<int>(std::ceil(camp.health() / damage) * strength * camp_taken_mult(owned_of(me).passives));
    const bool cheap = cost <= MAGE_FINISH_SHARE * me.max_health() && me.health() - cost > me.max_health() * RECALL_HEALTH;
    return {cheap, cost};
}

void Bot::mage_camp(const bc::champion& me, const Target& target, const bc::bitboard* avoid) {
    const int reach = me.attack_range();
    const auto position = me.position();
    const int g = distance(position, target.position());
    const auto [finish, cost] = finish_cost(me, target);
    const bool tanked = camp_tanked(me, target);
    if (tanked || finish) {
        // Someone sturdier takes the hits, or the few left cost little: spells and basic attacks both.
        if (tanked)
            SUB(".tanked", "an ally tanks the camp: spells and basic attacks");
        else
            SUB(".finish", format("finishing costs under %.0f%% of our health: spells and basic attacks", MAGE_FINISH_SHARE * 100));
        const bool cast_spell = mage_cast(me, target, true);
        if (cast_spell && !alive(target)) return;
        if (g <= reach)
            attack_if_in_range(target);
        else if (!cast_spell)
            move_towards_closest(tiles_around(target.position(), reach), avoid);
        return;
    }
    // Untanked with too much health left: never stand in its reach.
    const auto danger = camp_danger(target);
    const auto ring = tiles_around(target.position(), MAGE_CAST_RANGE) & ~danger;
    if (danger.is_set(position) && MAGE_LEAVE) {
        SUB(".leave", "in the camp's reach, nobody tanks, too costly to finish: step out to the cast ring");
        if (!move_towards_closest(ring, avoid)) move_towards_closest(~danger, avoid);
        return;
    }
    if (g <= MAGE_CAST_RANGE) {
        if (mage_cast(me, target, true)) {
            // v90: the spell may have killed the camp; reading it now would throw (as fight.hit.cast did).
            SUB(".standoff", format("casts from %d, outside the camp's reach", g) + (alive(target) ? "" : "; killed it"));
        } else {
            SUB(".cooldown", format("waits %d off, outside the camp's reach: spells cooling", g));
        }
        return;
    }
    SUB(".closing", format("walks to the cast ring %d off the camp", MAGE_CAST_RANGE));
    bc::bitboard keep_out = danger;
    if (avoid) keep_out |= *avoid;
    if (!move_towards_closest(ring, &keep_out)) move_towards_closest(ring, avoid);
}

bc::bitboard Bot::enemy_reach(const std::vector<bc::champion>& enemies) {
    // Tiles an enemy can hit next turn with a basic attack: a melee walks one and hits one, a ranged one stands and
    // shoots three. Their spells reach 4 whether we stand at 3 or 4, so they do not change where we stand.
    bc::bitboard zone;
    for (const auto& e : enemies)
        if (alive(e)) zone.draw_chebyshev(e.position(), static_cast<float>(e.attack_range() <= 1 ? MELEE_THREAT : RANGED_THREAT));
    return zone;
}

void Bot::mage_fight(const bc::champion& me, const Target& target, const std::vector<bc::champion>& enemies,
                     const bc::bitboard* avoid) {
    // A ready spell is cast from wherever we stand; we only walk toward the fight outside enemy basic-attack reach,
    // step back out of it while the spells cool, and basic attack from outside it. One threat board, one path search.
    const auto zone = enemy_reach(seen::champions(Team::enemy));
    const auto position = me.position();
    const bool safe = !zone.is_set(position);
    // Cassi's stun allows walking, so it is spent first, as before.
    if (me.champion_name() == Champ::cassi) cassi_stun(me, enemies);
    // Targets by health, the fight's pick first.
    auto order = alive(enemies);
    if (order.empty()) return;
    std::stable_sort(order.begin(), order.end(), [](const auto& a, const auto& b) { return a.health() < b.health(); });
    if (const auto* pick = target.champion()) {
        auto it = std::find(order.begin(), order.end(), *pick);
        if (it != order.end()) std::rotate(order.begin(), it, it + 1);
    }
    const auto main = order.front();
    bc::bitboard keep_out = zone;
    if (avoid) keep_out |= *avoid;

    if (mage_spell_ready(me)) {
        for (const auto& e : order) {
            const int gap = distance(position, e.position());
            if (gap <= MAGE_CAST_RANGE && mage_cast(me, e, false)) {
                // v90: the spell may have killed e, and reading a dead unit throws "Unit is not in vision" (~3 a game,
                // the rest of the turn lost), so nothing reads it after the cast.
                const bool killed = !alive(e);
                SUB(".cast", format("spell from %d off", gap) + (killed ? ", killed it" : "")
                                 + (safe ? "" : "; inside enemy reach, no basic attack"));
                if (!safe) return;
                if (!killed) {
                    attack_if_in_range(e);
                } else if (MAGE_KILL_FOLLOWUP) {
                    // v90: the basic attack is still unspent; the next living enemy in reach takes it.
                    for (const auto& o : order)
                        if (o != e && alive(o) && attack_if_in_range(o)) {
                            SUB(".next", "basic attack on the next enemy in reach");
                            break;
                        }
                }
                return;
            }
        }
        SUB(".step", "spell ready, nobody in cast range: step toward " + at(main.position()) + " outside enemy reach");
        if (!move_towards_closest(tiles_around(main.position(), MAGE_CAST_RANGE) & ~zone, &keep_out) && !safe)
            move_towards_closest(~zone, avoid);
        return;
    }
    if (!safe) {
        SUB(".back", "spells cooling: back out of enemy reach");
        move_towards_closest(~zone, avoid);
        return;
    }
    for (const auto& e : order)
        if (attack_if_in_range(e)) {
            SUB(".basic", "spells cooling: basic attack from outside enemy reach");
            return;
        }
    const auto spots = tiles_around(main.position(), me.attack_range()) & ~zone;
    if (!spots.is_empty() && move_towards_closest(spots, &keep_out)) {
        SUB(".step", "spells cooling: step in to basic attack, outside enemy reach");
        return;
    }
    SUB(".hold", "spells cooling, no safe basic attack: hold");
}

void Bot::willow_backline(const bc::champion& me, const bc::bitboard* avoid) {
    heal_lowest_ally(me);
    const auto& foes = seen::champions(Team::enemy);
    const auto zone = threat_zone(foes, true);
    const auto position = me.position();
    // The ally nearest an enemy is the front: the one taking the hits she heals.
    std::optional<vec2> front;
    int front_gap = 0;
    for (const auto& a : seen::champions(Team::ally)) {
        if (a == me || channelling(a)) continue;
        int gap = 1 << 20;
        for (const auto& e : foes) gap = std::min(gap, distance(a.position(), e.position()));
        if (!front || gap < front_gap) {
            front = a.position();
            front_gap = gap;
        }
    }
    bc::bitboard goal;
    const char* where = "out of reach";
    if (front) {
        goal = tiles_around(*front, static_cast<float>(HEAL_REACH)) & ~zone;
        where = "out of reach, in heal range of the front";
        if (goal.is_empty()) {
            goal = tiles_around(*front, WILLOW_BACK_SLACK) & ~zone;
            where = "out of reach, near the front";
        }
    }
    if (goal.is_empty()) goal = ~zone;
    if (goal.is_set(position)) {
        SUB(".back", std::string("holds ") + where);
        return;
    }
    SUB(".back", std::string("steps back: ") + where);
    move_towards_closest(goal, avoid);
}

bool Bot::willow_exposed(const bc::champion& me) {
    const auto position = me.position();
    const auto& allies = seen::champions(Team::ally);
    for (const auto& camp : seen::camps()) {
        if (camp.strength() <= 0) continue;  // harmless
        // v145 (AWAKE_READ): damage wakes the whole group, so an unhurt camp may be awake too.
        if (camp.health() >= camp.max_health()) {
            bool awake = false;
            if (AWAKE_READ) {
                try {
                    awake = camp.is_awake();
                } catch (const bc::error&) {
                }
            }
            if (!awake) continue;
        }
        const auto spot = camp.position();
        const int my_gap = distance(position, spot);
        if (my_gap > camp.attack_range()) continue;
        bool covered = false;
        for (const auto& a : allies) {
            if (a == me || a.is_recalling() || distance(a.position(), spot) >= my_gap) continue;
            if (a.attack_range() > 1 && a.max_health() <= SQUISHY_HEALTH) continue;
            if (a.health() < a.max_health() * WILLOW_TANK_HEALTH) continue;
            covered = true;
            break;
        }
        if (!covered) return true;
    }
    return false;
}
