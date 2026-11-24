// v216 (FIGHT_SIM, fight_coord_plan.md F3): a short fight simulation of the near contact, fought and fled, in gems.
// See the FIGHT_SIM block in constants.h. Every engine read happens once per unit before the loop; the result is
// cached per round and vision list.
// v218 (FIGHT_SIM 4): g4_eval (the g4 model alone, once a round) and veto_mode, a brake on v213's FIGHT.
#include "bot.h"
#include "seen.h"
#include "state.h"
#include <algorithm>
#include <array>
#include <cmath>

bool stays_out(const bc::champion& a);  // route.cpp

namespace {

constexpr int MAX_UNITS = 12;
constexpr int NO_ROUND = 1 << 20;

struct Slot {
    int at = NO_ROUND;          // first sim round it can be cast (its cooldown)
    float damage = 0, shield = 0, heal = 0;
    bool stun = false;
};

struct Unit {
    float hp = 0, hp0 = 0, max_hp = 1, strength = 0, w = 1, value = 0;
    float rd = 0, ehp = 0;  // F1's g4 features: round_damage x dealt, health / taken (no shield)
    int arrive = 0, dash = 0, stun_range = 0, roster = -1, emblems = 0, g4_reach = 1;
    bool ours = false, alive = true, hor = false, varg = false, in_reach = false;
    std::array<Slot, 2> slots{};
    int stunned_at = -1;
};

// A unit's ability slots: each damage, stun, shield, heal or reset from its live cooldown (cd < 0: unknown).
void read_slots(const bc::champion& c, Unit& u, double focus, bool enemy, int& ok, int& fail, int& busy) {
    int k = 0;
    for (const auto& slot : c.abilities()) {
        Slot& s = u.slots[static_cast<std::size_t>(k++ & 1)];
        const auto& info = slot.info();
        if (!info) continue;
        int cd = 0;
        if (enemy && SIM_ENEMY_READY == 1) cd = 0;
        else if (enemy && SIM_ENEMY_READY == 2) cd = NO_ROUND;
        else {
            try {
                cd = std::max(0, static_cast<int>(slot.cooldown()));
                if (enemy) ++ok, busy += cd > 0;
            } catch (const std::exception&) {
                cd = 0;  // unknown: taken as ready
                if (enemy) ++fail;
            }
        }
        s.at = cd;
        if (const auto* d = std::get_if<bc::ability_damage>(&*info)) s.damage = static_cast<float>(d->modifier * focus);
        else if (const auto* a = std::get_if<bc::ability_aoe_damage>(&*info)) s.damage = static_cast<float>(a->modifier * focus);
        else if (const auto* x = std::get_if<bc::ability_cc>(&*info)) {
            s.damage = static_cast<float>(x->modifier * focus);
            s.stun = true;
            if (cd == 0) u.stun_range = std::max(u.stun_range, static_cast<int>(x->range));
        } else if (const auto* h = std::get_if<bc::ability_heal>(&*info)) s.heal = static_cast<float>(h->modifier * focus);
        else if (const auto* sh = std::get_if<bc::ability_shield>(&*info)) s.shield = static_cast<float>(sh->modifier * focus);
        else if (const auto* r = std::get_if<bc::ability_attack_reset>(&*info)) {
            (void)r;
            s.damage = -1;  // a second basic attack: filled in once strength is known
        } else if (const auto* ds = std::get_if<bc::ability_dash>(&*info)) {
            if (cd == 0) u.dash = std::max(u.dash, static_cast<int>(ds->range));
            s.damage = static_cast<float>(ds->modifier * focus);
            s.stun = ds->duration > 0;
        }
        if (!SIM_BURST && s.damage > 0) s.damage = 0;
    }
}

double death_value(double bag, int round) {
    const double left = std::clamp(2000 - round, 0, 60);
    return SIM_BAG_W * bag + SIM_GOLD_W * 100.0 + SIM_RESPAWN_W * left * sim_farm_rate(round);
}

// Losses of one side in gems: deaths in full, damage short of death SIM_HURT_W by share of health lost.
double losses(const std::array<Unit, MAX_UNITS>& units, int n, bool ours, double& dead) {
    double out = 0;
    dead = 0;
    for (int i = 0; i < n; ++i) {
        const Unit& u = units[static_cast<std::size_t>(i)];
        if (u.ours != ours) continue;
        if (!u.alive) {
            out += u.value * u.w;
            dead += u.w;
        } else if (u.hp < u.hp0) {
            out += SIM_HURT_W * u.value * u.w * (u.hp0 - u.hp) / std::max(1.0f, u.hp0);
        }
    }
    return out;
}

// F1 (results/fight-model.txt): reach 1 melee / 3 ranged, +2 for a dash kit, at least 4 for a damage-spell kit.
int g4_reach(Champ kind, int range) {
    int r = range;
    if (kind == Champ::vesper_vale || kind == Champ::vendetta || kind == Champ::issac || kind == Champ::ren
        || kind == Champ::heart_of_ruin)
        r += 2;
    if (kit_spell_mult(kind) > 0) r = std::max(r, 4);
    return r;
}

float hit_mult(const Unit& u, int foes) {
    float m = 1.0f;
    if (u.hor) m *= 1.0f + 0.5f * std::clamp(1.0f - u.hp / std::max(1.0f, u.max_hp), 0.0f, 1.0f);
    if (u.varg && foes >= 2) m *= 1.5f;
    return m;
}

// Fight it out: returns the rounds until a side is gone (SIM_HORIZON if neither).
int run_fight(std::array<Unit, MAX_UNITS>& units, int n, int focus_roster) {
    std::array<float, MAX_UNITS> dmg{}, heal{};
    for (int t = 0; t < SIM_HORIZON; ++t) {
        int alive_ours = 0, alive_theirs = 0;
        for (int i = 0; i < n; ++i)
            if (units[static_cast<std::size_t>(i)].alive) (units[static_cast<std::size_t>(i)].ours ? alive_ours : alive_theirs)++;
        if (!alive_ours || !alive_theirs) return t;
        dmg.fill(0);
        heal.fill(0);
        for (int i = 0; i < n; ++i) {
            Unit& u = units[static_cast<std::size_t>(i)];
            if (!u.alive || u.arrive > t || u.stunned_at == t) continue;
            // Targets: ours the focus, else the best damage per health; theirs our lowest health. Stun: their top damage.
            int target = -1, stun_target = -1;
            double best = -1, best_stun = -1;
            for (int j = 0; j < n; ++j) {
                const Unit& v = units[static_cast<std::size_t>(j)];
                if (!v.alive || v.ours == u.ours) continue;
                double key;
                if (u.ours) key = (focus_roster >= 0 && v.roster == focus_roster) ? 1e9 : v.strength / std::max(1.0f, v.hp);
                else key = 1e6 - v.hp;
                if (key > best) best = key, target = j;
                if (v.strength > best_stun) best_stun = v.strength, stun_target = j;
            }
            if (target < 0) continue;
            float d = u.strength;
            for (auto& s : u.slots) {
                if (s.at > t) continue;
                s.at = NO_ROUND;  // one cast per cooldown in the horizon
                d += s.damage < 0 ? u.strength : s.damage;
                if (s.shield > 0) u.hp += s.shield;
                if (s.stun && stun_target >= 0) units[static_cast<std::size_t>(stun_target)].stunned_at = t + 1;
                if (s.heal > 0) {
                    int low = -1;
                    for (int j = 0; j < n; ++j) {
                        const Unit& v = units[static_cast<std::size_t>(j)];
                        if (j != i && v.alive && v.ours == u.ours && (low < 0 || v.hp < units[static_cast<std::size_t>(low)].hp)) low = j;
                    }
                    if (low >= 0) heal[static_cast<std::size_t>(low)] += s.heal;
                }
            }
            dmg[static_cast<std::size_t>(target)] += d * hit_mult(u, u.ours ? alive_theirs : alive_ours);
        }
        for (int i = 0; i < n; ++i) {
            Unit& u = units[static_cast<std::size_t>(i)];
            if (!u.alive) continue;
            u.hp = std::min(u.hp + heal[static_cast<std::size_t>(i)], std::max(u.hp, u.max_hp)) - dmg[static_cast<std::size_t>(i)];
            if (u.hp <= 0) u.alive = false;
        }
    }
    return SIM_HORIZON;
}

// We walk away from round 0: an enemy in reach hits once, one that can catch us SIM_CATCH_ROUNDS more, on our lowest.
void run_flee(std::array<Unit, MAX_UNITS>& units, int n) {
    for (int i = 0; i < n; ++i) {
        Unit& e = units[static_cast<std::size_t>(i)];
        if (e.ours || !e.alive) continue;
        const bool catches = e.dash > 0 || e.stun_range > 0;
        const int hits = (e.in_reach ? 1 : 0) + (catches && e.arrive <= 2 ? SIM_CATCH_ROUNDS : 0);
        for (int h = 0; h < hits; ++h) {
            int low = -1;
            for (int j = 0; j < n; ++j) {
                const Unit& v = units[static_cast<std::size_t>(j)];
                if (v.ours && v.alive && (low < 0 || v.hp < units[static_cast<std::size_t>(low)].hp)) low = j;
            }
            if (low < 0) return;
            float d = e.strength;
            if (h == 0)
                for (auto& s : e.slots)
                    if (s.at <= 0 && s.damage != 0) d += s.damage < 0 ? e.strength : s.damage, s.at = NO_ROUND;
            Unit& v = units[static_cast<std::size_t>(low)];
            v.hp -= d * hit_mult(e, 1);
            if (v.hp <= 0) v.alive = false;
        }
    }
}

}  // namespace

Bot::FightEval Bot::fight_sim() {
    const auto round = bc::round_number();
    if (sim_round == round && sim_gen == seen::generation()) return sim_cache;
    sim_round = round;
    sim_gen = seen::generation();
    FightEval out;
    const auto& enemies = near_enemies();
    if (enemies.empty()) return sim_cache = out;
    const auto& self = bc::self();
    const vec2 here = seen::self_position();
    std::array<Unit, MAX_UNITS> units{};
    double own_bag = 0, own_hpf = 1;
    std::array<vec2, MAX_UNITS> at{};
    std::array<int, MAX_UNITS> reach{};
    int n = 0;
    const auto& roster = seen::roster(Team::enemy);
    auto fill = [&](const bc::champion& c, bool ours, bool enemy) {
        if (n >= MAX_UNITS) return false;
        Unit u;
        u.ours = ours;
        const Champ kind = c.champion_name();
        u.hor = kind == Champ::heart_of_ruin;
        u.varg = kind == Champ::varg;
        const Passives pass = owned_of(c).passives;
        const double health = c.health(), top = c.max_health();
        const double focus = c.focus(), strength = c.strength();
        u.hp = u.hp0 = static_cast<float>((health + hor_shield_hp(c)) / champ_taken_mult(pass));
        u.max_hp = static_cast<float>(top);
        u.strength = static_cast<float>(strength * champ_dmg_mult(pass));
        try {
            read_slots(c, u, focus * champ_dmg_mult(pass), enemy, sim_cd_ok, sim_cd_fail, sim_cd_busy);
        } catch (const std::exception&) {
            if (enemy) ++sim_cd_fail;  // no ability list: no spells in the sim
        }
        const double bag = static_cast<double>(c.gem_count());
        u.value = static_cast<float>(death_value(bag, static_cast<int>(round)));
        const vec2 p = c.position();
        at[static_cast<std::size_t>(n)] = p;
        reach[static_cast<std::size_t>(n)] = static_cast<int>(c.attack_range());
        u.rd = static_cast<float>(round_damage(kind, strength, focus) * champ_dmg_mult(pass));
        u.ehp = static_cast<float>(health / champ_taken_mult(pass));
        u.emblems = owned_of(c).emblems;
        u.g4_reach = g4_reach(kind, reach[static_cast<std::size_t>(n)]);
        if (n == 0) own_bag = bag, own_hpf = health / std::max(1.0, top);
        if (enemy) {
            for (std::size_t r = 0; r < roster.size(); ++r)
                if (roster[r] == c) u.roster = static_cast<int>(r);
            const int k = (cached_ident(c) - 1) & 15;
            sim_seen[static_cast<std::size_t>(k)] = {static_cast<float>(health), static_cast<float>(top), u.strength,
                                                    static_cast<float>(focus), reach[static_cast<std::size_t>(n)], true,
                                                    u.rd, u.ehp, u.emblems, u.g4_reach};
        }
        units[static_cast<std::size_t>(n++)] = u;
        return true;
    };
    vec2 front = enemies[0].position();
    for (const auto& e : enemies)
        if (distance(e.position(), here) < distance(front, here)) front = e.position();
    out.nearest = distance(front, here);
    try {
        fill(self, true, false);
    } catch (const std::exception&) {
        return sim_cache = out;
    }
    for (const auto& a : seen::champions(Team::ally)) {
        if (a == self) continue;
        try {
            if (channelling(a) || a.is_teleporting() || stays_out(a)) continue;
            const vec2 p = a.position();
            if (distance(p, here) > SIM_ALLY_RANGE && distance(p, front) > SIM_ALLY_RANGE) continue;
            fill(a, true, false);
        } catch (const std::exception&) {
        }
    }
    out.ours = n;
    for (const auto& e : enemies) {
        try {
            fill(e, false, true);
        } catch (const std::exception&) {
        }
    }
    const int seen_n = n;
    if (SIM_UNSEEN) {
        // Living enemies out of sight, last seen lately, walked one tile a round toward us.
        const int now = static_cast<int>(round);
        for (const auto& e : roster) {
            if (n >= MAX_UNITS) break;
            bool visible = false;
            for (const auto& v : enemies)
                if (v == e) visible = true;
            if (visible) continue;
            try {
                if (!e.is_alive()) continue;
                const int k = (cached_ident(e) - 1) & 15;
                const auto& last = sim_seen[static_cast<std::size_t>(k)];
                const int age = now - enemy_at_round[static_cast<std::size_t>(k)];
                if (!last.known || enemy_at_round[static_cast<std::size_t>(k)] <= 0 || age > SIM_UNSEEN_AGE) continue;
                const vec2 p = enemy_at[static_cast<std::size_t>(k)];
                const int d = std::max(0, std::min(distance(p, here), distance(p, front)) - age);
                if (d > SIM_ALLY_RANGE) continue;
                Unit u;
                u.ours = false;
                u.w = static_cast<float>(SIM_UNSEEN_W);
                u.hp = u.hp0 = last.hp * u.w;
                u.max_hp = last.max_hp * u.w;
                u.strength = last.strength * u.w;
                u.rd = last.rd;
                u.ehp = last.ehp;
                u.emblems = last.emblems;
                u.g4_reach = last.g4_reach;
                u.value = static_cast<float>(death_value(0, static_cast<int>(round)));
                for (std::size_t r = 0; r < roster.size(); ++r)
                    if (roster[r] == e) u.roster = static_cast<int>(r);
                // Out of sight it walks on: its tile is unknown, so it arrives after its walk back.
                at[static_cast<std::size_t>(n)] = p;
                reach[static_cast<std::size_t>(n)] = last.range;
                u.arrive = std::max(0, d - last.range);
                units[static_cast<std::size_t>(n++)] = u;
                ++out.unseen;
            } catch (const std::exception&) {
            }
        }
    }
    out.theirs = n - out.ours;
    if (out.ours == 0 || out.theirs == 0) return sim_cache = out;
    // Arrival: gap to the nearest foe beyond the unit's reach, walked a tile a round, a ready dash saving 2.
    for (int i = 0; i < seen_n; ++i) {
        Unit& u = units[static_cast<std::size_t>(i)];
        int d = NO_ROUND;
        for (int j = 0; j < n; ++j)
            if (units[static_cast<std::size_t>(j)].ours != u.ours) d = std::min(d, distance(at[static_cast<std::size_t>(i)], at[static_cast<std::size_t>(j)]));
        const int gap = std::max(0, d - reach[static_cast<std::size_t>(i)]);
        u.in_reach = gap == 0;
        u.arrive = std::max(0, gap - (u.dash > 0 ? 2 : 0));
    }
    // F1's g4 (results/fight-model.txt section 4, US fits): arrival-weighted Lanchester + bodies + own health + bag.
    {
        double eh[2] = {0, 0}, dm[2] = {0, 0}, nb[2] = {0, 0}, em[2] = {0, 0};
        for (int i = 0; i < n; ++i) {
            const Unit& u = units[static_cast<std::size_t>(i)];
            int d = NO_ROUND;
            for (int j = 0; j < n; ++j)
                if (units[static_cast<std::size_t>(j)].ours != u.ours)
                    d = std::min(d, distance(at[static_cast<std::size_t>(i)], at[static_cast<std::size_t>(j)]));
            const int steps = i < seen_n ? std::max(0, d - u.g4_reach) : std::max(u.arrive, 0);
            const double w = std::max(0.0, 1.0 - steps / FM_ARRIVE_H) * u.w;
            const int side = u.ours ? 0 : 1;
            eh[side] += w * u.ehp;
            dm[side] += w * u.rd;
            nb[side] += w;
            if (distance(at[static_cast<std::size_t>(i)], here) <= 8) em[side] += u.emblems * u.w;
        }
        const double logh = std::log((eh[0] + 1) / (eh[1] + 1)), logd = std::log((dm[0] + 1) / (dm[1] + 1));
        const double ndiff = nb[0] - nb[1], emb = em[0] - em[1], bag = std::log(1 + own_bag);
        auto p = [&](double b0, double h, double dd, double nd, double e, double hf, double bg) {
            return 1.0 / (1.0 + std::exp(-(b0 + h * logh + dd * logd + nd * ndiff + e * emb + hf * own_hpf + bg * bag)));
        };
        out.p_lost = p(FM_LOST_B0, FM_LOST_LOGH, FM_LOST_LOGD, FM_LOST_NDIFF, FM_LOST_EMB, FM_LOST_HPF, FM_LOST_BAG);
        out.p_won = p(FM_WON_B0, FM_WON_LOGH, FM_WON_LOGD, FM_WON_NDIFF, FM_WON_EMB, FM_WON_HPF, FM_WON_BAG);
        out.p_die = p(FM_DIE_B0, FM_DIE_LOGH, FM_DIE_LOGD, FM_DIE_NDIFF, FM_DIE_EMB, FM_DIE_HPF, FM_DIE_BAG);
    }
    const int focus = read_focus();
    std::array<Unit, MAX_UNITS> flee = units;
    out.rounds = run_fight(units, n, focus);
    double their_dead = 0, our_dead = 0;
    const double theirs_lost = losses(units, n, false, their_dead);
    const double ours_lost = losses(units, n, true, our_dead);
    out.ev_fight = theirs_lost - ours_lost;
    out.our_dead = our_dead;
    out.their_dead = their_dead;
    if (SIM_RETREAT) {
        run_flee(flee, n);
        double dead_r = 0;
        const double lost_r = losses(flee, n, true, dead_r);
        out.our_dead_r = dead_r;
        out.ev_retreat = -lost_r - SIM_LEAVE_ROUNDS * sim_farm_rate(static_cast<int>(round)) * out.ours;
    }
    out.valid = true;
    return sim_cache = out;
}

int Bot::sim_mode(std::optional<vec2> origin, int current) {
    const auto& enemies = near_enemies();
    if (enemies.empty()) {
        LOG(score_why = far_skipped ? format("farm: the %d enemies in sight are over %d steps away", far_skipped, FIGHT_STEPS)
                                    : std::string("farm: no enemy in sight"););
        return FARM;
    }
    const FightEval ev = fight_sim();
    if (!ev.valid) {
        LOG(score_why = "retreat: no sim (no unit read)";);
        return RETREAT;
    }
    const bool g4 = FIGHT_SIM == 3;
    double score = g4 ? G4_SCALE * (ev.p_won - ev.p_lost) : SIM_SCALE * (ev.ev_fight - ev.ev_retreat);
    double dist = 0;
    if (origin) {
        int nearest = distance(enemies[0].position(), *origin);
        for (const auto& e : enemies) nearest = std::min(nearest, distance(e.position(), *origin));
        dist = DISTANCE_PENALTY * nearest;
        score -= dist;
    }
    if (ping_bonus) score += PING_SCORE_BONUS;
    const double bar = g4 ? (current == FIGHT ? G4_BAR - G4_STAY : G4_BAR) : current == FIGHT ? SIM_BAR - SIM_STAY : SIM_BAR;
    const int choice = score >= bar ? FIGHT : RETREAT;
    LOG(score_why = format("%s %.1f %s %.1f (g4 lost %.2f won %.2f die %.2f, sim fight %+.1f flee %+.1f, dead %.1f v %.1f in %d, %dv%d %d unseen, distance -%.1f%s)",
                           MODE_NAMES[choice], score, choice == FIGHT ? ">=" : "<", bar, ev.p_lost, ev.p_won, ev.p_die,
                           ev.ev_fight, ev.ev_retreat,
                           ev.our_dead, ev.their_dead, ev.rounds, ev.ours, ev.theirs, ev.unseen, dist,
                           ping_bonus ? ", ping" : ""););
    return choice;
}

// v218 (FIGHT_SIM 4): fight_sim's g4 features alone, with the same units (us, free allies within SIM_ALLY_RANGE of us
// or the front enemy, near enemies, unseen enemies seen lately) and the same weights. No ability or cooldown reads:
// health, focus, strength and range a unit (type and emblems are cached, enemy tiles read once), once a round per
// near-enemy count. The free allies are score_mode_old's assist list (fight_allies), built this round just before.
static_assert(FIGHT_LOCAL && !FIX_FIGHT_BODIES && !FIGHT_LOCAL_WALK && FIGHT_SKIP_LEAVERS && !FIGHT_ALLY_WEIGH
                  && !LEAVERS_KEEP_HOR && FIGHT_LOCAL_RANGE == SIM_ALLY_RANGE,
              "g4_eval reuses fight_allies as fight_sim's ally filter");
Bot::FightEval Bot::g4_eval() {
    const auto round = bc::round_number();
    const auto& enemies = near_enemies();
    if (g4_round == round && g4_near == enemies.size()) return g4_cache;
    g4_round = round;
    g4_near = enemies.size();
    FightEval out;
    if (enemies.empty()) return g4_cache = out;
    struct G4Unit {
        vec2 at{};
        float rd = 0, ehp = 0, w = 1;
        int emblems = 0, reach = 1, arrive = 0;
        bool ours = false, unseen = false;
    };
    std::array<G4Unit, MAX_UNITS> units{};
    int n = 0;
    double own_bag = 0, own_hpf = 1;
    const auto& self = bc::self();
    const vec2 here = seen::self_position();
    auto fill = [&](const bc::champion& c, bool ours, bool me, vec2 at) {
        if (n >= MAX_UNITS) return;
        G4Unit u;
        u.ours = ours;
        const int ident = cached_ident(c);
        const Champ kind = static_cast<Champ>(ident - 1);
        const Owned owned = owned_of(c);
        const double health = c.health(), focus = c.focus(), strength = c.strength();
        const int range = static_cast<int>(c.attack_range());
        u.rd = static_cast<float>(round_damage(kind, strength, focus) * champ_dmg_mult(owned.passives));
        u.ehp = static_cast<float>(health / champ_taken_mult(owned.passives));
        u.emblems = owned.emblems;
        u.reach = g4_reach(kind, range);
        u.at = at;
        if (me) own_bag = static_cast<double>(c.gem_count()), own_hpf = health / std::max(1.0, static_cast<double>(c.max_health()));
        if (!ours) g4_seen[static_cast<std::size_t>((ident - 1) & 15)] = {u.rd, u.ehp, u.emblems, u.reach, range, true};
        units[static_cast<std::size_t>(n++)] = u;
    };
    std::array<vec2, MAX_UNITS> enemy_at_now{};
    const std::size_t ne = std::min(enemies.size(), enemy_at_now.size());
    for (std::size_t i = 0; i < ne; ++i) enemy_at_now[i] = enemies[i].position();
    vec2 front = enemy_at_now[0];
    int nearest = distance(front, here);
    for (std::size_t i = 1; i < ne; ++i)
        if (distance(enemy_at_now[i], here) < nearest) front = enemy_at_now[i], nearest = distance(front, here);
    out.nearest = nearest;
    try {
        fill(self, true, true, here);
    } catch (const std::exception&) {
        return g4_cache = out;
    }
    for (const auto& a : fight_allies) {
        if (a == self) continue;
        try {
            fill(a, true, false, a.position());
        } catch (const std::exception&) {
        }
    }
    out.ours = n;
    for (std::size_t i = 0; i < ne; ++i) {
        try {
            fill(enemies[i], false, false, enemy_at_now[i]);
        } catch (const std::exception&) {
        }
    }
    if (SIM_UNSEEN) {
        const int now = static_cast<int>(round);
        for (const auto& e : seen::roster(Team::enemy)) {
            if (n >= MAX_UNITS) break;
            bool visible = false;
            for (const auto& v : enemies)
                if (v == e) visible = true;
            if (visible) continue;
            try {
                // The cached checks first: the alive read only for an enemy seen lately and close enough.
                const auto k = static_cast<std::size_t>((cached_ident(e) - 1) & 15);
                const auto& last = g4_seen[k];
                const int age = now - enemy_at_round[k];
                if (!last.known || enemy_at_round[k] <= 0 || age > SIM_UNSEEN_AGE) continue;
                const vec2 p = enemy_at[k];
                const int d = std::max(0, std::min(distance(p, here), distance(p, front)) - age);
                if (d > SIM_ALLY_RANGE || !e.is_alive()) continue;
                G4Unit u;
                u.w = static_cast<float>(SIM_UNSEEN_W);
                u.rd = last.rd;
                u.ehp = last.ehp;
                u.emblems = last.emblems;
                u.reach = last.g4_reach;
                u.at = p;
                u.arrive = std::max(0, d - last.range);
                u.unseen = true;
                units[static_cast<std::size_t>(n++)] = u;
                ++out.unseen;
            } catch (const std::exception&) {
            }
        }
    }
    out.theirs = n - out.ours;
    if (out.ours == 0 || out.theirs == 0) return g4_cache = out;
    double eh[2] = {0, 0}, dm[2] = {0, 0}, nb[2] = {0, 0}, em[2] = {0, 0};
    for (int i = 0; i < n; ++i) {
        const G4Unit& u = units[static_cast<std::size_t>(i)];
        int d = NO_ROUND;
        for (int j = 0; j < n; ++j)
            if (units[static_cast<std::size_t>(j)].ours != u.ours) d = std::min(d, distance(u.at, units[static_cast<std::size_t>(j)].at));
        const int steps = u.unseen ? u.arrive : std::max(0, d - u.reach);
        const double w = std::max(0.0, 1.0 - steps / FM_ARRIVE_H) * u.w;
        const int side = u.ours ? 0 : 1;
        eh[side] += w * u.ehp;
        dm[side] += w * u.rd;
        nb[side] += w;
        if (distance(u.at, here) <= 8) em[side] += u.emblems * u.w;
    }
    const double logh = std::log((eh[0] + 1) / (eh[1] + 1)), logd = std::log((dm[0] + 1) / (dm[1] + 1));
    const double ndiff = nb[0] - nb[1], emb = em[0] - em[1], bag = std::log(1 + own_bag);
    auto p = [&](double b0, double h, double dd, double nd, double e, double hf, double bg) {
        return 1.0 / (1.0 + std::exp(-(b0 + h * logh + dd * logd + nd * ndiff + e * emb + hf * own_hpf + bg * bag)));
    };
    out.p_lost = p(FM_LOST_B0, FM_LOST_LOGH, FM_LOST_LOGD, FM_LOST_NDIFF, FM_LOST_EMB, FM_LOST_HPF, FM_LOST_BAG);
    out.p_won = p(FM_WON_B0, FM_WON_LOGH, FM_WON_LOGD, FM_WON_NDIFF, FM_WON_EMB, FM_WON_HPF, FM_WON_BAG);
    out.p_die = p(FM_DIE_B0, FM_DIE_LOGH, FM_DIE_LOGD, FM_DIE_NDIFF, FM_DIE_EMB, FM_DIE_HPF, FM_DIE_BAG);
    out.valid = true;
    return g4_cache = out;
}

int Bot::veto_block() {
    int block = win_tribute ? 1 : 0;
    const auto& self = bc::self();
    try {
        if (channelling(self) || self.is_teleporting()) block |= 2;
    } catch (const std::exception&) {
    }
    const vec2 here = seen::self_position();
    for (const auto& a : seen::champions(Team::ally)) {
        if (a == self) continue;
        try {
            if (distance(a.position(), here) <= SIM_ALLY_RANGE && a.is_submitting_gems()) block |= 4;
        } catch (const std::exception&) {
        }
    }
    for (const auto& e : near_enemies()) {
        try {
            if (distance(e.position(), here) <= VETO_PRIZE_RANGE && channelling(e)) block |= 8;
        } catch (const std::exception&) {
        }
    }
    return block;
}

// v218 (FIGHT_SIM 4): v213 said FIGHT. Only a clearly lost contact (g4 p_lost - p_won over the bar, stricter to leave
// a fight we are in) turns it into RETREAT, and never with a bag at stake (veto_block, read only once the bar is
// crossed). Never RETREAT into FIGHT: this only runs on v213's FIGHT.
int Bot::veto_mode(int current) {
    // A fight v213 likes by VETO_MARGIN or more is never in the lost tail (logged: 2 of 73 would-be vetoes), so g4 is
    // not even read then.
    if (veto_margin >= VETO_MARGIN) {
        LOG(note(format("g4v: old fight cur %d | skip margin %.1f", current, veto_margin)););
        return FIGHT;
    }
    const FightEval ev = g4_eval();
    const double edge = ev.p_lost - ev.p_won;
    const double bar = current == FIGHT ? VETO_STAY : VETO_BAR;
    const bool over = ev.valid && edge > bar;
    const int block = over ? veto_block() : 0;
    const bool veto = over && block == 0;
    LOG(note(format("g4v: old fight cur %d | g4 %.3f %.3f %.3f edge %.3f bar %.2f | %dv%d %d unseen near %d | veto %d block %d",
                    current, ev.p_lost, ev.p_won, ev.p_die, edge, bar, ev.ours, ev.theirs, ev.unseen, ev.nearest,
                    veto ? 1 : 0, block)););
    if (!veto) return FIGHT;
    LOG(score_why = format("retreat: g4 veto, lost %.2f - won %.2f = %.2f > %.2f, over ", ev.p_lost, ev.p_won, edge, bar) + score_why;);
    return RETREAT;
}

int Bot::score_mode(std::optional<vec2> origin, int current) {
    if constexpr (FIGHT_SIM == 2 || FIGHT_SIM == 3) return sim_mode(origin, current);
    const int choice = score_mode_old(origin, current);
    if constexpr (FIGHT_SIM == 4) {
        if (choice == FIGHT) return veto_mode(current);
        LOG(if (!near_enemies().empty()) note(format("g4v: old %s cur %d", MODE_NAMES[choice], current)););
        return choice;
    }
    if constexpr (FIGHT_SIM == 1)
        LOG({
            if (!near_enemies().empty()) {
                const FightEval ev = fight_sim();
                if (ev.valid) {
                    const FightEval v4 = g4_eval();
                    note(format("sim: f %.1f r %.1f od %.2f td %.2f odr %.2f t %d n %d %d %d near %d | old %s %.1f %d cur %d | g4 %.3f %.3f %.3f | cd %d %d %d | v4 %.3f %.3f %.3f x %d",
                                ev.ev_fight, ev.ev_retreat, ev.our_dead, ev.their_dead, ev.our_dead_r, ev.rounds, ev.ours,
                                ev.theirs, ev.unseen, ev.nearest, MODE_NAMES[choice], old_score, old_bar, current, ev.p_lost, ev.p_won, ev.p_die, sim_cd_ok,
                                sim_cd_fail, sim_cd_busy, v4.p_lost, v4.p_won, v4.p_die, veto_block()));
                }
            }
        });
    return choice;
}
