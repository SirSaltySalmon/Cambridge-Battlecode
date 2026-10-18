// Champion kits: when to cast, dash, kite, or hold range.
#include "bot.h"
#include "seen.h"
#include "state.h"
#include <tuple>

void Bot::engage(const bc::champion& me, const Target& target, const std::vector<bc::champion>& enemies, bool is_camp,
                 const bc::bitboard* avoid) {
    if (me.champion_name() == Champ::vesper_vale)
        engage_vesper(me, target, enemies, is_camp, avoid);
    else if (me.attack_range() > 1)
        engage_ranged(me, target, enemies, is_camp, avoid);
    else
        engage_melee(me, target, is_camp, avoid);
}

void Bot::engage_melee(const bc::champion& me, const Target& target, bool is_camp, const bc::bitboard* avoid) {
    const auto kind = me.champion_name();
    const int reach = me.attack_range();

    if (kind == Champ::varg && under_threat(me)) cast_self(ability_of(me, Kind::shield));

    // Ren's stun excludes walking, so it is cast before any step and only on champions.
    if (kind == Champ::ren && !is_camp) {
        const auto stun = ability_of(me, Kind::cc);
        if (stun && within(me, target, ability_range(*stun))) cast(stun, target);
        if (!alive(target)) return;
    }

    // Ryth's area stun excludes walking, so it is cast before the step. The reset is the other cast.
    const bool stunned = kind == Champ::ryth && ryth_stun(me, target, is_camp);
    if (stunned && !alive(target)) return;

    if (!is_camp && (kind == Champ::issac || kind == Champ::vendetta || kind == Champ::ren)
        && distance(me.position(), target.position()) > reach + 1) {
        const auto last_seen = target.position();
        const bool dashed = dash_towards(me, last_seen, reach);
        // The dash moved us, and the target may have dropped out of sight (around a wall, into a bush).
        // Stop here: reading it now would throw, which ended the turn at this same point anyway.
        if (dashed && !sighted(target)) {
            note("target left vision after dash");
            return;
        }
    }

    if (!within(me, target, reach) && !stunned) move_towards_closest(tiles_around(target.position(), reach), avoid);

    if (attack_if_in_range(target) && !stunned
        && (kind == Champ::issac || kind == Champ::vendetta || kind == Champ::varg || kind == Champ::ryth)) {
        if (alive(target) && cast_self(ability_of(me, Kind::attack_reset))) attack_if_in_range(target);
    }
}

void Bot::engage_ranged(const bc::champion& me, const Target& target, const std::vector<bc::champion>& enemies,
                        bool is_camp, const bc::bitboard* avoid) {
    const auto kind = me.champion_name();
    const int reach = me.attack_range();
    const int g = distance(me.position(), target.position());

    // Healing and the attack reset share Willow's one cast per turn.
    const bool resets = kind == Champ::willow && !heal_lowest_ally(me);
    if (kind == Champ::cassi) {
        if (!is_camp) cassi_stun(me, enemies);
        // The damage spell excludes walking, so it waits until we are already outside camp reach.
        if (!backing_off_camp(me, target, is_camp, g)) {
            const auto damage = ability_of(me, Kind::damage);
            if (damage && g <= ability_range(*damage)) cast(damage, target);
            if (!alive(target)) return;
        }
    }

    // Stellar's spells exclude walking. Step out of a camp before spending the cast.
    bool cast_spell = false;
    if (kind == Champ::stellar && !backing_off_camp(me, target, is_camp, g)) {
        cast_spell = stellar_cast(me, target, is_camp);
        if (cast_spell && !alive(target)) return;
    }

    // Hold outside camp reach until an allied melee is closer and taking the hits.
    if (is_camp && should_hold_back(me, target)) {
        hold_camp(me, target, avoid);
        return;
    }

    if (g <= reach) {
        // The attack is attempted before the reset check, so a spell that already spent the turn skips it.
        if (!cast_spell && attack_if_in_range(target) && (resets || kind == Champ::stellar) && alive(target)) {
            if (resets && cast_self(ability_of(me, Kind::attack_reset))) attack_if_in_range(target);
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
        reach = RANGED_CAMP_STANDOFF - 1;
    }
    return std::max(RANGED_CAMP_STANDOFF, reach + 1);
}

bool Bot::should_hold_back(const bc::champion& me, const Target& camp) {
    // Clusters have no strength, so there is nothing to absorb. Willow still takes those.
    if (camp.strength() <= 0) return false;
    for (const auto& ally : closer_melees(me, camp))
        if (coming_to_camp(ally, camp)) return false;
    // Other ranged may open a camp with no tank in sight. A melee that then appears,
    // still further out or clearly passing by, pulls them back. Willow never opens one.
    return me.champion_name() == Champ::willow || !visible_melees(me).empty();
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
        ident = champ_id(ally.champion_name()) + 1;
    } catch (const bc::error&) {
        return false;
    }
    if (carrier && ident == carrier) return false;
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
            reach = RANGED_CAMP_STANDOFF - 1;
        }
        if (distance(here, camp_pos) > reach) return false;
    }
    return true;
}

void Bot::remember_allies() {
    // Kept across the turn, then refreshed. Melees act first, so the new point is where they stepped.
    const int now = static_cast<int>(bc::round_number());
    for (auto& sighting : ally_at)
        if (sighting && now - sighting->first > 1) sighting.reset();
    std::optional<int> mine;
    try {
        mine = champ_id(bc::self().champion_name()) + 1;
    } catch (const bc::error&) {
    }
    for (const auto& ally : seen::champions(Team::ally)) {
        try {
            const int ident = champ_id(ally.champion_name()) + 1;
            if (mine && ident == *mine) continue;
            ally_at.at(ident) = std::make_pair(now, ally.position());
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
        note(std::string(willow ? "willow" : "ranged") + format(" standoff %d from camp, closer melee is going elsewhere", g));
    else if (willow)
        note(format("willow waits %d from camp, melee is not closer", g));
    else
        note(format("ranged standoff %d from camp, melee is not closer", g));
    const auto ring = tiles_around(camp.position(), g) & ~danger;
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
    std::string name;
    try {
        name = std::to_string(champ_id(anchor.champion_name()));
    } catch (const bc::error&) {
        name = "?";
    }
    note("willow follows c" + name + at(anchor.position()) + ", stays outside camp");
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
    if (use_area && cast(area, *center)) return true;
    return single_ok && cast(single, target);
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
    if (!target) target = lowest(near);
    return target && cast(stun, *target);
}

bool Bot::heal_lowest_ally(const bc::champion& me) {
    const auto heal = ability_of(me, Kind::heal);
    if (!heal || !heal->is_ready()) return false;
    std::optional<bc::champion> target;
    double best = 0;
    for (const auto& a : seen::champions(Team::ally))
        if (a != me && a.health() < a.max_health() && within(me, a, ability_range(*heal))) {
            const double ratio = static_cast<double>(a.health()) / a.max_health();
            if (!target || ratio < best) {
                target = a;
                best = ratio;
            }
        }
    return target && cast(heal, *target);
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
    // After a dash the target may be out of sight; reading it would throw. Vesper never walks after a dash.
    if (dashed && !sighted(target)) {
        note("target left vision after dash");
        return;
    }
    if (!within(me, target, reach)) {
        if (!dashed) move_towards_closest(tiles_around(target.position(), reach), avoid);
        return;
    }

    if (!attack_if_in_range(target) || dashed) return;
    if (alive(target) && cast_self(ability_of(me, Kind::attack_reset)))
        attack_if_in_range(target);
    else if (!is_camp)
        vesper_kite(me, enemies, target, reach);
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
