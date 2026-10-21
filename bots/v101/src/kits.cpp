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
    else if (VARG_SPLASH && is_camp && me.champion_name() == Champ::varg)
        engage_melee(me, varg_splash_target(me, target), is_camp, avoid);
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

    if (!is_camp && (kind == Champ::issac || kind == Champ::vendetta || kind == Champ::ren)
        && distance(me.position(), target.position()) > reach + 1) {
        const auto last_seen = target.position();
        const bool dashed = dash_towards(me, last_seen, reach);
        if (dashed) SUB("", "dash to close in");
        // The dash moved us, and the target may have dropped out of sight (around a wall, into a bush).
        // Stop here: reading it now would throw, which ended the turn at this same point anyway.
        if (dashed && !sighted(target)) {
            NOTE("dashed in; the target dropped out of sight");
            return;
        }
    }

    if (!within(me, target, reach) && !stunned) move_towards_closest(tiles_around(target.position(), reach), avoid);

    if (attack_if_in_range(target) && !stunned
        && (kind == Champ::issac || kind == Champ::vendetta || kind == Champ::varg || kind == Champ::ryth)) {
        if (alive(target) && cast_self(ability_of(me, Kind::attack_reset))) {
            SUB("", "reset for a second hit");
            attack_if_in_range(target);
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
    bool cast_spell = false;
    if (kind == Champ::stellar && !backing_off_camp(me, target, is_camp, g)) {
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
        if (!cast_spell && attack_if_in_range(target) && (resets || kind == Champ::stellar) && alive(target)) {
            if (resets && cast_self(ability_of(me, Kind::attack_reset))) {
                SUB("", "no ally to heal: reset for a second hit");
                attack_if_in_range(target);
            }
        }
    } else if (!cast_spell) {
        move_towards_closest(tiles_around(target.position(), reach), avoid);
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
    for (const auto& ally : closer_melees(me, camp))
        if (coming_to_camp(ally, camp)) return false;
    // Other ranged may open a camp with no tank in sight. A melee that then appears,
    // still further out or clearly passing by, pulls them back. Willow never opens one.
    const bool hold = me.champion_name() == Champ::willow || !visible_melees(me).empty();
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
        if (now - hold_from >= HOLD_LIMIT) return false;
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
        if (gap >= my_gap || gap > reach) continue;
        if (ally.attack_range() > 1 && ally.max_health() <= SQUISHY_HEALTH) continue;
        if (ally.is_recalling()) continue;
        // v91 (V91_TANK_HURT): one under the recall bar is stepping away to recall, not tanking (a mage kept hitting
        // from inside the camp's reach while Issac dodged out to recall).
        // v101 (WILLOW_CAMP_SAFETY): Willow wants a tank that will stay, not one about to leave.
        const double bar = WILLOW_CAMP_SAFETY && me.champion_name() == Champ::willow ? WILLOW_TANK_HEALTH : RECALL_HEALTH;
        if (V91_TANK_HURT && ally.health() < ally.max_health() * bar) continue;
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
    for (const auto& ally : seen::champions(Team::ally)) {
        try {
            const int ident = cached_ident(ally);
            if (mine && ident == *mine) continue;
            ally_at.at(ident) = std::make_pair(now, ally.position());
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

bool Bot::ryth_stun(const bc::champion& me, const Target& target, bool is_camp) {
    // Ground stun in radius one, range 4. One body is enough; a pack is taken when the centre covers it.
    const auto places = spell_places(target, is_camp);
    const auto focus = target.position();
    if (cast_aoe(ability_of(me, Kind::aoe_damage), places, focus, 1)) return true;
    const auto stun = ability_of(me, Kind::cc);
    if (!stun || !within(me, target, ability_range(*stun))) return false;
    if (is_camp && !stun->can_target_camps()) return false;
    if (stun->can_target_ground()) return cast(stun, focus);
    return cast(stun, target);
}

bool Bot::stellar_cast(const bc::champion& me, const Target& target, bool is_camp) {
    // Single hit is 2x Focus. The area is 1.4x, so it wins once two bodies are inside it.
    const auto single = ability_of(me, Kind::damage);
    const auto area = ability_of(me, Kind::aoe_damage);
    const auto places = spell_places(target, is_camp);
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
            const auto aim = aoe_aim(me.position(), places, ability_range(*area), ability_radius(*area), focus);
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

    // Ranged walking uses up the attack, so dash straight into range when it is close enough.
    const bool dashed = !within(me, target, reach) && dash_towards(me, target.position(), reach - 1);
    if (dashed) SUB("", "dash into range");
    // After a dash the target may be out of sight; reading it would throw. Vesper never walks after a dash.
    if (dashed && !sighted(target)) {
        NOTE("dashed in; the target dropped out of sight");
        return;
    }
    if (!within(me, target, reach)) {
        if (!dashed) move_towards_closest(tiles_around(target.position(), reach), avoid);
        return;
    }

    if (!attack_if_in_range(target) || dashed) return;
    if (alive(target) && cast_self(ability_of(me, Kind::attack_reset))) {
        SUB("", "reset for a second hit");
        attack_if_in_range(target);
    } else if (!is_camp && vesper_kite(me, enemies, target, reach)) {
        SUB("", "dash out of enemy reach after firing");
    }
}

bool Bot::vesper_kite(const bc::champion& me, const std::vector<bc::champion>& enemies, const Target& target, int reach) {
    // After firing, dash to the tile furthest outside enemy reach, preferring ones still in range.
    const auto dash = ability_of(me, Kind::dash);
    std::vector<std::pair<vec2, int>> threats;
    for (const auto& e : alive(enemies)) threats.emplace_back(e.position(), e.attack_range());
    if (!dash || !dash->is_ready() || threats.empty()) return false;
    const auto aim = alive(target) ? std::optional<vec2>{target.position()} : std::nullopt;

    auto margin = [&](vec2 tile) {
        int best = distance(tile, threats.front().first) - threats.front().second;
        for (const auto& [spot, reach_of] : threats) best = std::min(best, distance(tile, spot) - reach_of);
        return best;
    };

    const auto position = me.position();
    if (margin(position) > 0) return false;
    const auto free = tiles_around(position, static_cast<float>(static_cast<int>(ability_range(*dash)))) & ~blocked_tiles();
    std::optional<vec2> best;
    std::tuple<bool, bool, int> best_key{};
    for (const auto t : free.positions()) {
        const std::tuple<bool, bool, int> key{margin(t) > 0, aim && distance(t, *aim) <= reach, margin(t)};
        if (!best || key > best_key) {
            best = t;
            best_key = key;
        }
    }
    return best && margin(*best) > margin(position) && cast(dash, *best);
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
    const auto s = camp_slot(target);
    if (!s) return target;
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
    const auto* current = target.camp();
    const int reach = me.attack_range();
    std::optional<bc::camp> best;
    int best_count = current ? splash(*current) : 0;
    for (const auto& c : group) {
        if (current && c == *current) continue;
        if (distance(me.position(), c.position()) > reach) continue;
        const int count = splash(c);
        if (count > best_count || (best && count == best_count && c.health() < best->health())) {
            best = c;
            best_count = count;
        }
    }
    if (!best) return target;
    SUB("", "hit the camp whose splash covers the most");
    return *best;
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
    double damage = me.strength();
    for (const auto& ally : seen::champions(Team::ally))
        if (ally != me && distance(ally.position(), spot) <= ally.attack_range()) damage += ally.strength();
    if (damage <= 0) return {false, 0};
    // Every camp of the group is awake while we fight it (a cluster beside a monolith is no free hit).
    double strength = camp.strength();
    if (const auto s = camp_slot(camp)) {
        strength = 0;
        for (const auto& c : seen::camps())
            if (slot_at(c.spawn_position()) == *s && c.health() > 0) strength += c.strength();
    }
    const int cost = static_cast<int>(std::ceil(camp.health() / damage) * strength);
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

bool Bot::mages_drafted() {
    if (mage_team) return *mage_team;
    bool any = false, known = true;
    for (const auto& ally : seen::roster(Team::ally)) {
        try {
            if (is_mage(ally.champion_name())) any = true;
        } catch (const bc::error&) {
            known = false;
        }
    }
    if (any || known) mage_team = any;
    return any;
}

bool Bot::monolith_delegated() {
    // v94: the monolith's reach (5) passed the spells' (4), so nobody can take it from outside its reach.
    return MAGE_MONOLITH && MONOLITH_REACH < MAGE_CAST_RANGE && mages_drafted() && !(MONOLITH_TEAM_STARVED && gem_starved);
}

bc::bitboard Bot::awake_monoliths() {
    bc::bitboard zone;
    for (const auto& c : seen::camps())
        if (c.is_monolith() && c.health() < c.max_health()) zone.draw_chebyshev(c.position(), static_cast<float>(c.attack_range()));
    return zone;
}

bool Bot::mage_monolith(const bc::champion& me) {
    // Monoliths are off the shared route; a mage takes one on whenever it is near the team's target, or near us while
    // that target respawns. On a gem-starved map the route's gates make it the team's target, and the mages take it
    // then. Fights, pings, banking and shopping come first.
    if (monoliths.empty() || route.empty()) return false;
    const int now = static_cast<int>(bc::round_number());
    const auto position = me.position();
    if (mono_job >= 0 && ready_at[mono_job] > now) NOTE(format("monolith s%d is down: mage job over", mono_job));
    // Picked afresh each turn: the job holds while its reason does (or we have been hitting it).
    const int was = mono_job;
    mono_job = -1;
    int pick = -1;
    {
        const int target = slot % static_cast<int>(route.size());
        const bool idle = wait_at(target, position) > 0;
        std::pair<bool, int> pick_key{};
        for (const int m : monoliths) {
            if (ready_at[m] > now) continue;
            // Near the team's target, or near us while that target respawns; one we have been hitting is finished.
            const bool near = distance(centers[m], centers[target]) <= MONOLITH_JOB_RANGE
                || (idle && distance(position, centers[m]) <= MONOLITH_JOB_RANGE);
            const bool started = m == mono_last && now - mono_hit <= MONOLITH_RESUME;
            const bool hit = started || mid_clear(m);
            // On a gem-starved map the monolith is the team's target once the team is ready for it (the route gates).
            if (gem_starved ? !hit && m != target : !near && !hit) continue;
            // One already hit first, then the one nearest the team's target, so both mages pick the same one.
            const std::pair<bool, int> key{!hit, distance(centers[m], centers[target])};
            if (pick < 0 || key < pick_key) {
                pick = m;
                pick_key = key;
            }
        }
        if (pick < 0) return false;
        if (pick != was)
            NOTE(format("mage takes monolith s%d: ", pick)
                 + (gem_starved ? "gem-starved map, it is the team's target" : idle ? "team target respawning" : "near the team target"));
    }
    mono_job = pick;
    auto camps = camps_at(pick);
    // A monolith in a bush is hidden from outside it, and spells need sight: the ward goes in first.
    if (camps.empty() && distance(position, centers[pick]) <= MAGE_CAST_RANGE + 4 && mono_ward(me, pick)) camps = camps_at(pick);
    if (camps.empty()) {
        bc::bitboard danger, ring, tiles;
        for (const auto tile : spawn_lists[pick]) {
            danger.draw_chebyshev(tile, MONOLITH_REACH);
            ring.draw_chebyshev(tile, MAGE_CAST_RANGE);
            tiles.set(tile);
        }
        ring &= ~danger;
        // Only ring tiles that see a monolith tile (sight is symmetric, so cast from the tiles; walls and bushes
        // block it), when there are any.
        auto sight = mono_sight(pick);
        sight &= ring;
        if (!sight.is_empty()) ring = sight;
        auto seen_tiles = tiles;
        seen_tiles &= bc::vision::visible_tiles();
        auto bush_tiles = tiles;
        bush_tiles &= bc::map::bushes();
        if (!seen_tiles.is_empty()) {
            NOTE(format("monolith s%d's tiles are in sight and empty: mage job over", pick));
            mono_job = -1;
            return false;
        }
        WHY("mono.walk", format("monolith s%d out of sight (%d of %d tiles bush): walk to its cast ring", pick,
                                static_cast<int>(bush_tiles.count()), static_cast<int>(tiles.count())));
        if (ring.is_set(position)) {
            // On the ring and still blind: step to the ring tile nearest its tiles.
            auto inner = ring;
            inner &= tiles_around(centers[pick], MAGE_CAST_RANGE);
            if (!move_towards_closest(inner, &danger)) NOTE("on the cast ring and still blind to the monolith");
            return true;
        }
        move_towards_closest(ring, &danger);
        return true;
    }
    const auto target = *lowest(camps);
    mono_last = pick;
    mono_hit = now;
    WHY("mono.camp", format("mage job: monolith s%d ", pick) + at(target.position()));
    mage_camp(me, target, nullptr);
    return true;
}

bc::bitboard Bot::mono_sight(int s) {
    if (mono_sights.size() != route.size()) mono_sights.resize(route.size());
    auto& board = mono_sights[s];
    if (!board) {
        board.emplace();
        const auto opaque = bc::map::walls() | bc::map::bushes();
        for (const auto tile : spawn_lists[s]) *board |= opaque.shadow_cast(tile, static_cast<std::uint16_t>(MAGE_CAST_RANGE));
        *board &= ~opaque;
    }
    return *board;
}

bool Bot::mono_ward(const bc::champion& me, int s) {
    // The ward goes on the free tile in reach nearest the monolith's tiles, a bush one first: from inside the bush
    // it sees the whole bush, monolith included.
    try {
        if (me.ward_cooldown() > 0) return false;
    } catch (const bc::error&) {
        return false;
    }
    auto tiles = me.ward_placement_tiles();
    tiles &= ~(seen::terrain() | bc::vision::ward_tiles(Team::ally));
    if (tiles.is_empty()) return false;
    const auto& bushes = bc::map::bushes();
    auto gap = [&](vec2 tile) {
        int best = 1000;
        for (const auto spot : spawn_lists[s]) best = std::min(best, distance(tile, spot));
        return best;
    };
    std::optional<vec2> best;
    std::pair<bool, int> best_key{};
    for (const auto tile : tiles.positions()) {
        const std::pair<bool, int> key{bushes.is_set(tile), -gap(tile)};
        if (!best || key > best_key) {
            best = tile;
            best_key = key;
        }
    }
    const auto own = me.ward_positions();
    if (!best || std::find(own.begin(), own.end(), *best) != own.end()) return false;
    bool placed = false;
    try {
        placed = me.place_ward(*best);
    } catch (const bc::error&) {
        return false;
    }
    seen::reset();
    if (placed) trace::act("ward");
    NOTE(format("monolith s%d hidden: ward ", s) + (placed ? "placed at " : "refused at ") + at(*best)
         + (bushes.is_set(*best) ? " (bush)" : ""));
    return placed;
}

bool Bot::monolith_guard(const bc::champion& me, int s, const bc::bitboard* avoid, const std::string& stem) {
    // A melee never hits a monolith: it holds a ring outside its reach, near enough to meet an enemy.
    bc::bitboard danger, ring;
    for (const auto tile : spawn_lists[s]) {
        danger.draw_chebyshev(tile, MONOLITH_REACH);
        ring.draw_chebyshev(tile, MONOLITH_GUARD_RANGE);
    }
    ring &= ~danger;
    WHY(stem + ".guard", format("s%d is a monolith: mages take it, hold outside its reach", s));
    if (avoid) danger |= *avoid;
    if (!ring.is_set(me.position())) move_towards_closest(ring, &danger);
    return true;
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
        if (camp.strength() <= 0 || camp.health() >= camp.max_health()) continue;  // harmless or asleep
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
