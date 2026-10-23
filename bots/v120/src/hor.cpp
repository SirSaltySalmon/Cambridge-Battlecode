// v101: Heart of Ruin's kit: the dash-stun dive, the held shield, and staying in a fight under the recall bar.
// Self-contained: the rest of the bot calls engage_hor (from engage), hor_holds (the recall step), hor_shield (the
// dodge) and hor_shield_hp (the fight score). See the HOR_* switches in constants.h.
#include "bot.h"
#include "seen.h"
#include "state.h"
#include "trace.h"
#include <algorithm>

namespace {

// Shield absorption still up on a champion (0 without one).
double shield_up(const bc::champion& champion) {
    try {
        const auto buffs = champion.buffs();
        return buffs.has_shield() ? static_cast<double>(buffs.get<bc::buff_shield>().value) : 0.0;
    } catch (const std::exception&) {
        return 0.0;
    }
}

bool stunned(const bc::champion& champion) {
    try {
        return champion.buffs().has_stun();
    } catch (const std::exception&) {
        return false;
    }
}

} // namespace

void Bot::hor_scan(const bc::champion& me) {
    // What every enemy in sight could deal next turn and every hitting camp near us, read once a round: enemies and
    // camps do not move during our turn, and the dive asks for up to 24 landing tiles.
    const int now = static_cast<int>(bc::round_number());
    if (hor_scan_round == now) return;
    hor_scan_round = now;
    hor_threat_n = 0;
    for (const auto& e : seen::champions(Team::enemy)) {
        if (hor_threat_n >= hor_threats.size()) break;
        try {
            HorThreat t{};
            t.at = e.position();
            const int basic = e.attack_range();
            // A melee walks one and hits; a ranged one either walks or shoots.
            t.hit_reach = basic + (basic <= 1 ? 1 : 0);
            t.basic = e.strength();
            const double focus = e.focus();
            for (const auto& slot : e.abilities()) {
                const auto& info = slot.info();
                if (!info || slot.cooldown() > RECALL_READY_TICKS) continue;
                if (std::holds_alternative<bc::ability_attack_reset>(*info)) {
                    t.reset = t.basic;
                } else if (const auto* d = std::get_if<bc::ability_dash>(&*info)) {
                    t.hit_reach = std::max(t.hit_reach, basic + static_cast<int>(d->range));
                    if (d->modifier > 0 && d->modifier * focus > t.spell) {
                        t.spell = d->modifier * focus;
                        t.spell_reach = static_cast<int>(d->range + d->radius);
                    }
                } else if (const auto* h = std::get_if<bc::ability_damage>(&*info)) {
                    if (h->modifier * focus > t.spell) t.spell = h->modifier * focus, t.spell_reach = static_cast<int>(h->range);
                } else if (const auto* c = std::get_if<bc::ability_cc>(&*info)) {
                    if (c->modifier * focus > t.spell) t.spell = c->modifier * focus, t.spell_reach = static_cast<int>(c->range);
                } else if (const auto* o = std::get_if<bc::ability_aoe_damage>(&*info)) {
                    if (o->modifier * focus > t.spell)
                        t.spell = o->modifier * focus, t.spell_reach = static_cast<int>(o->range + o->radius);
                }
            }
            hor_threats[hor_threat_n++] = t;
        } catch (const std::exception&) {
            continue;
        }
    }
    hor_camp_n = 0;
    const auto position = me.position();
    for (const auto& camp : seen::camps()) {
        if (hor_camp_n >= hor_camps.size()) break;
        // Camps never leave their spawn tile; the dive reaches 2 tiles, plus the camp's reach.
        // v120 (AOE_CLEAN): a cluster counts too when its group has camps that hit back (the landing wakes them all).
        if (distance(position, camp.spawn_position()) > MONOLITH_REACH + 3) continue;
        if (camp.strength() <= 0) {
            const int cs = slot_at(camp.spawn_position());
            if (!AOE_CLEAN || cs < 0 || clusters_only[cs]) continue;
        }
        HorCamp c{};
        c.at = camp.position();
        c.reach = camp.attack_range();
        c.strength = camp.strength();
        try {
            c.awake = camp.health() < camp.max_health() || camp.is_awake();
        } catch (const bc::error&) {
            c.awake = true;
        }
        hor_camps[hor_camp_n++] = c;
    }
}

double Bot::hor_burst(vec2 at, double* camp_part) const {
    // An upper bound on next turn's damage at `at`: each enemy's basic attack where it reaches, plus the best of its
    // ready reset or spell (one cast a turn); every awake camp in reach hits too.
    double total = 0;
    for (std::size_t i = 0; i < hor_threat_n; ++i) {
        const auto& t = hor_threats[i];
        const int gap = distance(at, t.at);
        double extra = 0;
        if (gap <= t.hit_reach) {
            total += t.basic;
            extra = t.reset;
        }
        if (gap <= t.spell_reach) extra = std::max(extra, t.spell);
        total += extra;
    }
    double camps = 0;
    for (std::size_t i = 0; i < hor_camp_n; ++i)
        if (hor_camps[i].awake && distance(at, hor_camps[i].at) <= hor_camps[i].reach) camps += hor_camps[i].strength;
    if (camp_part) *camp_part = camps;
    return total + camps;
}

bool Bot::hor_shield(const bc::champion& me) {
    // Held while health is high (his damage grows as it falls, and shields do not count as health), raised once the
    // enemies here could take him to HOR_BUFFER in one turn, or while awake camps hit hard and he is under
    // HOR_CAMP_SHIELD_HEALTH.
    if (!HOR_SHIELD) return false;
    const auto shield = ability_of(me, Kind::shield);
    if (!shield || !shield->is_ready() || shield_up(me) > 0) return false;
    hor_scan(me);
    double camps = 0;
    const double burst = hor_burst(me.position(), &camps);
    const double health = me.health();
    const char* why = nullptr;
    if (burst > camps && health <= burst + HOR_BUFFER)
        why = "shield: one turn of enemy damage would take us under the buffer";
    else if (camps >= HOR_SHIELD_CAMP_HIT && health < me.max_health() * HOR_CAMP_SHIELD_HEALTH)
        why = "shield: awake camps hit us hard";
    if (!why || !cast_self(shield)) return false;
    SUB("", why);
    return true;
}

bool Bot::hor_dive(const bc::champion& me) {
    // The dash lands on the tile within 2 whose radius-1 stun hits the most enemy value (a channelling enemy counts
    // HOR_CHANNEL_WEIGHT times, one already stunned only for the damage), only where health and the shield up
    // outlast that tile's burst by HOR_BUFFER, and never beside a sleeping camp it would wake.
    if (!HOR_DIVE) return false;
    const auto dash = ability_of(me, Kind::dash);
    if (!dash || !dash->is_ready()) return false;
    const auto origin = me.position();
    const int range = static_cast<int>(ability_range(*dash));
    struct Hit {
        vec2 at;
        int value;
    };
    std::array<Hit, 8> hits{};
    std::size_t hit_n = 0;
    for (const auto& e : seen::champions(Team::enemy)) {
        if (hit_n >= hits.size()) break;
        const auto at = e.position();
        if (distance(origin, at) > range + 1) continue;
        const int value = stunned(e) ? 1 : channelling(e) ? 2 * HOR_CHANNEL_WEIGHT : 2;
        hits[hit_n++] = {at, value};
    }
    if (hit_n == 0) return false;
    hor_scan(me);
    const double health = me.health() + shield_up(me);
    struct Spot {
        vec2 at;
        int value;
        double burst;
    };
    std::array<Spot, 32> spots{};
    std::size_t spot_n = 0;
    const auto free = tiles_around(origin, static_cast<float>(range)) & ~blocked_tiles();
    for (const auto tile : free.positions()) {
        if (spot_n >= spots.size()) break;
        if (tile == origin) continue;
        int value = 0;
        for (std::size_t i = 0; i < hit_n; ++i)
            if (distance(tile, hits[i].at) <= 1) value += hits[i].value;
        if (value == 0) continue;
        bool wakes = false;
        for (std::size_t i = 0; i < hor_camp_n; ++i)
            if (!hor_camps[i].awake && distance(tile, hor_camps[i].at) <= 1) wakes = true;
        if (wakes) continue;
        const double burst = hor_burst(tile);
        if (health - burst < HOR_BUFFER) continue;
        spots[spot_n++] = {tile, value, burst};
    }
    if (spot_n == 0) {
        NOTE("dive: no landing that stuns an enemy and stays over the buffer");
        return false;
    }
    std::stable_sort(spots.begin(), spots.begin() + static_cast<std::ptrdiff_t>(spot_n), [](const Spot& a, const Spot& b) {
        return a.value != b.value ? a.value > b.value : a.burst < b.burst;
    });
    // A hidden unit on the tile refuses the dash without spending it; the next best is tried.
    for (std::size_t i = 0; i < spot_n && i < 3; ++i)
        if (cast(dash, spots[i].at)) {
            trace::moved("dive");
            SUB(".dive", format("dash-stun at %s: value %d, burst there %.0f", at(spots[i].at).c_str(), spots[i].value,
                                spots[i].burst));
            return true;
        }
    return false;
}

bool Bot::hor_camp_dash(const bc::champion& me, const Target& target) {
    // Farming with no enemy seen for HOR_CAMP_QUIET rounds: the dash goes into the target's group where its damage
    // (clusters die to it) and its stun (a skipped camp hit) are worth HOR_CAMP_DASH_VALUE, never beside another group.
    if (!HOR_CAMP_DASH || static_cast<int>(bc::round_number()) - last_enemy_round < HOR_CAMP_QUIET) return false;
    const auto dash = ability_of(me, Kind::dash);
    if (!dash || !dash->is_ready()) return false;
    const auto s = camp_slot(target);
    if (!s) return false;
    const auto origin = me.position();
    const int range = static_cast<int>(ability_range(*dash));
    const double damage = spell_damage(dash);
    struct Body {
        vec2 at;
        double value;
        bool ours;
    };
    std::array<Body, 16> bodies{};
    std::size_t n = 0;
    for (const auto& c : seen::camps()) {
        if (n >= bodies.size()) break;
        const auto at = c.position();
        if (distance(origin, at) > range + 1) continue;
        const bool ours = slot_at(c.spawn_position()) == *s;
        bodies[n++] = {at, ours ? std::min(static_cast<double>(c.health()), damage) + c.strength() : 0.0, ours};
    }
    std::optional<vec2> best;
    double best_value = HOR_CAMP_DASH_VALUE - 1;
    const auto free = tiles_around(origin, static_cast<float>(range)) & ~blocked_tiles();
    for (const auto tile : free.positions()) {
        if (tile == origin) continue;
        double value = 0;
        bool other = false;
        for (std::size_t i = 0; i < n; ++i)
            if (distance(tile, bodies[i].at) <= 1) {
                if (!bodies[i].ours) other = true;
                value += bodies[i].value;
            }
        if (!other && value > best_value) {
            best = tile;
            best_value = value;
        }
    }
    if (!best || !cast(dash, *best)) return false;
    trace::moved("dive");
    SUB(".campdash", format("dash-stun into the group: worth %.0f", best_value));
    return true;
}

void Bot::engage_hor(const bc::champion& me, const Target& target, const std::vector<bc::champion>& enemies, bool is_camp,
                     const bc::bitboard* avoid) {
    (void)enemies;
    // One cast a turn: the shield when the burst calls for it, else the dash.
    const bool shielded = hor_shield(me);
    const bool dashed = !shielded && (is_camp ? hor_camp_dash(me, target) : hor_dive(me));
    if (dashed && !alive(target)) {
        // The landing killed it, or it dropped out of sight: the basic attack goes to whatever is in reach.
        if (is_camp) {
            auto camps = group_camps();
            if (const auto next = lowest(camps)) attack_if_in_range(*next);
        } else if (const auto next = lowest(alive(seen::champions(Team::enemy)))) {
            attack_if_in_range(*next);
        }
        return;
    }
    if (dashed && !sighted(target)) return;
    engage_melee(me, target, is_camp, avoid);
}

bool Bot::hor_holds(const bc::champion& me) {
    // Under the recall bar in a fight the score likes, with a light bag: stay while health, the shield up and a ready
    // shield outlast a turn of the enemies' damage here by HOR_BUFFER. His damage is highest now.
    if (!HOR_HOLD || me.champion_name() != Champ::heart_of_ruin) return false;
    if (static_cast<int>(me.gem_count()) >= CARRY_GEMS || near_enemies().empty()) return false;
    if (mode != FIGHT && score_mode(me.position(), local_prev) != FIGHT) return false;
    hor_scan(me);
    double ready = 0;
    if (const auto shield = ability_of(me, Kind::shield); shield && shield->is_ready()) {
        try {
            ready = shield->shield_amount();
        } catch (const std::exception&) {
        }
    }
    const double health = me.health(), up = shield_up(me);
    const double burst = hor_burst(me.position());
    if (health + up + ready - burst < HOR_BUFFER) return false;
    NOTE(format("hor holds the fight under the bar: health %.0f + shield %.0f + ready %.0f vs burst %.0f", health, up, ready, burst));
    return true;
}

double Bot::hor_shield_hp(const bc::champion& champion) {
    // A Heart of Ruin's shield, for the fight score's health (shields are not health to the engine).
    try {
        if (cached_ident(champion) != champ_id(Champ::heart_of_ruin) + 1) return 0.0;
    } catch (const bc::error&) {
        return 0.0;
    }
    return shield_up(champion);
}
