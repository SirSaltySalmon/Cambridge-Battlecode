// v202 (SPREAD): spread-by-default farming. Roles and territories from the roster and the map, the solo's own job, and
// the danger marks it avoids. See SPREAD in constants.h.
#include "bot.h"
#include "seen.h"
#include "state.h"
#include "trace.h"
#include <algorithm>
#include <cmath>

namespace {

int kind_health(bc::camp_type kind) {
    switch (kind) {
    case bc::camp_type::wolf: return 1350;
    case bc::camp_type::elk: return 2400;
    case bc::camp_type::monolith: return 20000;
    case bc::camp_type::gem_cluster: case bc::camp_type::gold_cluster: return 120;
    default: return 5000;
    }
}

vec2 centroid(const std::vector<vec2>& tiles) {
    if (tiles.empty()) return {0, 0};
    long x = 0, y = 0;
    for (const auto t : tiles) {
        x += t.x;
        y += t.y;
    }
    const long n = static_cast<long>(tiles.size());
    return {static_cast<int>(x / n), static_cast<int>(y / n)};
}

} // namespace

bool Bot::spread_on() const {
    return SPREAD && spread_ready && spread_units > 0;
}

void Bot::spread_setup() {
    // Once, after the zones and spawns are known. Every champion runs the same steps on the same facts.
    spread_ready = true;
    spread_unit_of.fill(-1);
    spread_units = 0;
    if (!SPREAD || !SPREAD_PLAY || prism_team() || !zones_known || route.empty() || roster_size() < SPREAD_MIN_TEAM) return;
    std::vector<Champ> kinds;
    for (const auto& ally : seen::roster(Team::ally)) {
        try {
            kinds.push_back(ally.champion_name());
        } catch (const bc::error&) {
        }
    }
    const auto has_kind = [&](Champ k) { return std::find(kinds.begin(), kinds.end(), k) != kinds.end(); };
    const int cap = SPREAD_MAX_SOLO[std::clamp(roster_size(), 0, 5)];
    for (const Champ k : SPREAD_SOLO_ORDER) {
        if (spread_units >= cap) break;
        if (k == Champ::cassi) {
            if (!has_kind(Champ::cassi) && !has_kind(Champ::issac)) continue;
            spread_unit_of[champ_id(Champ::cassi) + 1] = static_cast<std::int8_t>(spread_units);
            spread_unit_of[champ_id(Champ::issac) + 1] = static_cast<std::int8_t>(spread_units);
            spread_unit_kind[spread_units] = Champ::cassi;
            ++spread_units;
            continue;
        }
        if (!has_kind(k)) continue;
        spread_unit_kind[spread_units] = k;
        spread_unit_of[champ_id(k) + 1] = static_cast<std::int8_t>(spread_units++);
    }
    spread_unit = spread_unit_of[champ_id(bc::self().champion_name()) + 1];
    // Territories: our-half and middle groups (no golem, no monolith), by side offset from the spawn-to-spawn line, cut
    // into spread_units + 1 bands of equal value; the band nearest the line is the core's.
    const int n = static_cast<int>(route.size());
    spread_owner.assign(n, -1);
    if (spread_units == 0) return;
    const vec2 ours = centroid(ally_spawn_list), theirs = centroid(enemy_spawn_list);
    const double ax = theirs.x - ours.x, ay = theirs.y - ours.y;
    const double len = std::max(1.0, std::hypot(ax, ay));
    struct Item {
        double side, value;
        int s;
    };
    std::vector<Item> items;
    double total = 0;
    for (int s = 0; s < n; ++s) {
        if (zone(s) > ZONE_UNCERTAIN || is_monolith(s) || is_golem(s) || values[0][s] <= 0) continue;
        const double side = (ax * (centers[s].y - ours.y) - ay * (centers[s].x - ours.x)) / len;
        items.push_back({side, values[0][s], s});
        total += values[0][s];
    }
    std::sort(items.begin(), items.end(), [](const Item& a, const Item& b) { return a.side < b.side || (a.side == b.side && a.s < b.s); });
    const int bands = spread_units + 1;
    std::vector<int> band_of(items.size(), 0);
    std::vector<double> band_side(bands, 0), band_count(bands, 0);
    double run = 0;
    for (std::size_t i = 0; i < items.size(); ++i) {
        const int b = std::min(bands - 1, static_cast<int>(run / std::max(1e-9, total) * bands));
        band_of[i] = b;
        band_side[b] += items[i].side;
        band_count[b] += 1;
        run += items[i].value;
    }
    int core = 0;
    for (int b = 0; b < bands; ++b) {
        const double mean = band_count[b] > 0 ? std::abs(band_side[b] / band_count[b]) : 1e9;
        const double best = band_count[core] > 0 ? std::abs(band_side[core] / band_count[core]) : 1e9;
        if (mean < best) core = b;
    }
    std::vector<int> unit_of_band(bands, -1);
    for (int b = 0, u = 0; b < bands; ++b)
        if (b != core) unit_of_band[b] = u++;
    for (std::size_t i = 0; i < items.size(); ++i) spread_owner[items[i].s] = static_cast<std::int8_t>(unit_of_band[band_of[i]]);
    LOG({
        std::string text = format("setup: spread %d solo units, we are %d; bands", spread_units, spread_unit);
        for (std::size_t i = 0; i < items.size(); ++i) text += format(" s%d:%d", items[i].s, spread_owner[items[i].s]);
        say(text);
    });
}

void Bot::spread_mark(vec2 tile, int round) {
    // One mark per tile and few rounds; the ring overwrites the oldest.
    for (const auto& [r, t] : spread_marks)
        if (t == tile && round - r < 5 && r > 0) return;
    spread_marks[spread_mark_next] = {round, tile};
    spread_mark_next = (spread_mark_next + 1) % static_cast<int>(spread_marks.size());
}

bool Bot::spread_danger(int s, int now) const {
    for (const auto& [r, t] : spread_marks)
        if (r > 0 && now - r <= SPREAD_DANGER_ROUNDS && distance(t, centers[s]) <= SPREAD_DANGER_RANGE) return true;
    return false;
}

void Bot::spread_update(const bc::champion& me) {
    // Every turn before the shared decision: units alive (for the leader's filter) and danger marks (enemy sightings,
    // ally deaths at their last seen tile).
    if (!spread_on()) return;
    const int now = static_cast<int>(bc::round_number());
    spread_alive.fill(false);
    for (const auto& ally : seen::roster(Team::ally)) {
        const int ident = cached_ident(ally);
        if (ident < 0 || ident >= 16) continue;
        bool alive = false;
        try {
            alive = ally.is_alive();
        } catch (const bc::error&) {
        }
        if (alive && spread_unit_of[ident] >= 0) spread_alive[spread_unit_of[ident]] = true;
        if (!alive && spread_ally_was_alive[ident] && ally_seen[ident] && now - ally_seen[ident]->first <= SPREAD_DANGER_ROUNDS)
            spread_mark(ally_seen[ident]->second, now);
        spread_ally_was_alive[ident] = alive;
    }
    if (me.is_alive())
        for (const auto tile : seen::positions(Team::enemy)) spread_mark(tile, now);
    if (now - spread_feasible_round >= SPREAD_ESTIMATE_EVERY) spread_estimate();
}

void Bot::spread_estimate() {
    // Base stats from the rules (damage = strength + SAT_FOCUS_SHARE x focus); Cassi & Issac add both damages and
    // tank on Issac's health x SPREAD_PAIR_HEALTH, as spread_pick counts them.
    spread_feasible_round = static_cast<int>(bc::round_number());
    const int n = static_cast<int>(route.size());
    spread_feasible.assign(n, 0);
    for (int unit = 0; unit < spread_units; ++unit) {
        const Champ k = spread_unit_kind[unit];
        double str = 45, foc = 100, hp = 1000;
        switch (k) {
        case Champ::cassi: str = 75; foc = 200; hp = 900 * SPREAD_PAIR_HEALTH; break;
        case Champ::vendetta: str = 70; foc = 100; hp = 1000; break;
        case Champ::marcella: str = 45; foc = 60; hp = 1200; break;
        case Champ::ryth: str = 45; foc = 100; hp = 1200; break;
        case Champ::ren: str = 35; foc = 100; hp = 900; break;
        case Champ::tavi: str = 45; foc = 110; hp = 900; break;
        default: break;
        }
        int owned = 0;
        const auto e = emblems.find(champ_id(k));
        if (e != emblems.end()) owned = e->second;
        if (k == Champ::cassi) {
            const auto i = emblems.find(champ_id(Champ::issac));
            if (i != emblems.end()) owned = std::max(owned, i->second);
        }
        const double damage = (str + SAT_FOCUS_SHARE * foc) * (1 + SPREAD_EMBLEM_DAMAGE * owned);
        const double health = hp * (1 + SPREAD_EMBLEM_HEALTH * owned);
        for (int s = 0; s < n; ++s) {
            if (spread_owner[s] != unit) continue;
            double spent = 0, take = 0;
            std::array<double, 24> hps{};
            std::array<double, 24> strs{};
            std::size_t c = 0;
            for (const auto& camp : route[s]->camps())
                if (c < hps.size()) {
                    hps[c] = kind_health(camp.kind());
                    strs[c++] = camp_kind_strength(camp.kind());
                }
            std::array<std::size_t, 24> order{};
            for (std::size_t i = 0; i < c; ++i) order[i] = i;
            std::sort(order.begin(), order.begin() + c, [&](std::size_t a, std::size_t b) { return hps[a] < hps[b]; });
            for (std::size_t i = 0; i < c; ++i) {
                spent += hps[order[i]];
                take += strs[order[i]] * std::ceil(spent / damage);
            }
            spread_feasible[s] = take <= SPREAD_TAKE * health ? 1 : 0;
        }
    }
}

double Bot::spread_clear(int s, double damage, double& rounds) {
    // Damage we take clearing group s alone: the camps die weakest first and each hits us until it dies. Visible camps
    // count their real health; the rest full.
    struct Camp {
        double health, strength;
    };
    std::array<Camp, 24> camps{};
    std::size_t k = 0;
    const auto live = camps_at(s);
    if (!live.empty()) {
        for (const auto& c : live)
            if (k < camps.size()) camps[k++] = {static_cast<double>(c.health()), static_cast<double>(c.strength())};
    } else {
        for (const auto& c : route[s]->camps())
            if (k < camps.size()) camps[k++] = {static_cast<double>(kind_health(c.kind())), static_cast<double>(camp_kind_strength(c.kind()))};
    }
    std::sort(camps.begin(), camps.begin() + k, [](const Camp& a, const Camp& b) { return a.health < b.health; });
    double spent = 0, take = 0;
    for (std::size_t i = 0; i < k; ++i) {
        spent += camps[i].health;
        take += camps[i].strength * std::ceil(spent / damage);
    }
    rounds = std::ceil(spent / damage);
    return take;
}

int Bot::spread_pick(const bc::champion& me) {
    const int n = static_cast<int>(route.size());
    const int now = static_cast<int>(bc::round_number());
    const auto position = me.position();
    const auto passives = owned_of(me).passives;
    const bool pair = me.champion_name() == Champ::cassi || me.champion_name() == Champ::issac;
    double damage = std::max(1.0, me.strength() * camp_dmg_mult(passives) + SAT_FOCUS_SHARE * me.focus());
    double health = me.health();
    if (pair) {
        damage *= SPREAD_PAIR_DAMAGE;
        health *= SPREAD_PAIR_HEALTH;
    }
    const double taken = camp_taken_mult(passives);
    int best = -1;
    double best_rate = 0;
    for (int u = 0; u < n; ++u) {
        if (spread_owner[u] != spread_unit || u == capture_slot) continue;
        if (rewards.count(u) && u != spread_job) continue;
        if (u >= static_cast<int>(goal_fields.size()) || !goal_fields[u]) continue;
        const auto steps = goal_fields[u]->field.get(position);
        if (steps == bc::distance_field::unreachable || static_cast<int>(steps) > SPREAD_MAX_WALK) continue;
        const int walk = static_cast<int>(steps);
        if (ready_at[u] > now + walk) continue;
        if (spread_danger(u, now)) continue;
        double rounds = 0;
        if (spread_clear(u, damage, rounds) * taken > SPREAD_TAKE * health) continue;
        double rate = values[aggressive][u] / (walk + rounds + capture_rounds);
        if (u == spread_job) rate *= SPREAD_KEEP;
        if (best < 0 || rate > best_rate) {
            best = u;
            best_rate = rate;
        }
    }
    if (best >= 0 && SPREAD_EDGE > 0) {
        const int main = slot % n;
        const double share = route_rate_steps(main, distance(position, centers[main])) / std::max(1, allies_alive());
        if (best_rate < SPREAD_EDGE * share) return -1;
    }
    return best;
}

void Bot::spread_drop(const char* why) {
    if (spread_job >= 0) NOTE(format("spread: drops s%d: ", spread_job) + why);
    spread_job = -1;
}

bool Bot::spread_step(const bc::champion& me, int now_mode) {
    // The solo's farm turn: its own group instead of the shared target. False hands the turn to farm() (the core's).
    if (!spread_on() || spread_unit < 0 || route.empty()) return false;
    const int now = static_cast<int>(bc::round_number());
    if (now_mode == RETREAT) {
        spread_drop("retreat");
        return false;
    }
    if (!seen::champions(Team::enemy).empty()) {
        spread_drop("enemy in sight");
        spread_cool_until = now + SPREAD_ENEMY_COOL;
        return false;
    }
    if (now < spread_cool_until) return false;
    const int s = slot % static_cast<int>(route.size());
    if (mode == FIGHT && distance(me.position(), centers[s]) <= SPREAD_HELP) {
        spread_drop("the team fights near us");
        return false;
    }
    if (me.health() < me.max_health() * SPREAD_HEALTH) {
        spread_drop("hurt");
        return false;
    }
    if (static_cast<int>(me.gem_count()) >= SPREAD_BAG) {
        spread_drop("bag to bank");
        return false;
    }
    if (me.champion_name() == Champ::cassi || me.champion_name() == Champ::issac) {
        bool partner = false;
        for (const auto& a : seen::champions(Team::ally))
            if (a != me && (a.champion_name() == Champ::cassi || a.champion_name() == Champ::issac)
                && distance(me.position(), a.position()) <= SPREAD_PAIR_RANGE)
                partner = true;
        if (!partner) {
            spread_drop("partner not beside us");
            return false;
        }
    }
    // A job whose group is down with nothing to capture is over.
    if (spread_job >= 0 && ready_at[spread_job] > now && !rewards.count(spread_job) && camps_at(spread_job).empty()) spread_drop("captured");
    if (spread_job < 0 || now - spread_pick_round >= SPREAD_REPICK) {
        const int pick = spread_pick(me);
        spread_pick_round = now;
        if (pick != spread_job) {
            if (pick >= 0) NOTE(format("spread: job s%d", pick));
            else spread_drop("nothing in our band");
        }
        spread_job = pick;
    }
    if (spread_job < 0) return false;
    slot = spread_job;
    spread_farming = true;
    farm(me, nullptr, "spread");
    spread_farming = false;
    return true;
}

// v204 (OUTCLASS): see constants.h.

int Bot::outclass_gap(const bc::champion& me, bool& near, bool& finish) {
    // v218: the contract's settler and this turn's outclass_step ask the same question; nothing changed between them
    // while no action reset the vision lists (seen::generation).
    if (!contract_on()) return outclass_gap_read(me, near, finish);
    const int round = static_cast<int>(bc::round_number());
    if (oc_round == round && oc_gen == seen::generation()) {
        near = oc_near;
        finish = oc_finish;
        return oc_gap;
    }
    oc_gap = outclass_gap_read(me, near, finish);
    oc_round = round;
    oc_gen = seen::generation();
    oc_near = near;
    oc_finish = finish;
    return oc_gap;
}

int Bot::outclass_gap_read(const bc::champion& me, bool& near, bool& finish) {
    const auto position = me.position();
    const auto& foes = seen::roster(Team::enemy);
    near = false;
    finish = false;
    int theirs = 0;
    for (const auto& e : seen::champions(Team::enemy)) {
        if (distance(position, e.position()) > OUTCLASS_RANGE) continue;
        near = true;
        if (e.health() <= e.max_health() * OUTCLASS_FINISH) finish = true;
        if (enemy_emblems.size() == foes.size())
            for (std::size_t i = 0; i < foes.size(); ++i)
                if (foes[i] == e) {
                    theirs += enemy_emblems[i];
                    break;
                }
    }
    int ours = emblem_count(me);
    for (const auto& a : seen::champions(Team::ally)) {
        if (a == me || channelling(a) || distance(position, a.position()) > OUTCLASS_RANGE) continue;
        const auto it = emblems.find(cached_ident(a) - 1);
        if (it != emblems.end()) ours += it->second;
    }
    return ours - theirs;
}

bool Bot::outclass_step(const bc::champion& me) {
    const int now = static_cast<int>(bc::round_number());
    if (seen::ally_spawn().is_set(me.position())) return false;
    bool near = false, finish = false;
    const int gap = outclass_gap(me, near, finish);
    const auto& enemies = seen::champions(Team::enemy);
    if (near && !finish && gap <= OUTCLASS_GAP) {
        if (now > outclass_until) NOTE(format("outclass: %d emblems behind here: leave", -gap));
        outclass_until = now + OUTCLASS_HOLD;
        outclass_spots.clear();
        for (const auto& e : enemies) outclass_spots.push_back(e.position());
    }
    if (now > outclass_until || outclass_spots.empty()) return false;
    if (!enemies.empty()) {
        outclass_spots.clear();
        for (const auto& e : enemies) outclass_spots.push_back(e.position());
    }
    bc::bitboard danger;
    for (const auto spot : outclass_spots) danger.draw_chebyshev(spot, static_cast<float>(OUTCLASS_CLEAR));
    if (danger.is_set(me.position())) {
        WHY("outclass.leave", format("out-emblemed: out to %d tiles from them", OUTCLASS_CLEAR));
        Go out;
        out.dash = getaway();
        out.rank = escape_rank(me, danger);
        if (!go(~danger, out)) {
            const auto zone = recall_zone(enemies, false);
            if (!go(~zone, out)) return false;
        }
        if (me.champion_name() == Champ::heart_of_ruin) hor_shield(me);
        return true;
    }
    if (OUTCLASS_AVOID && !route.empty()) {
        const int s = slot % static_cast<int>(route.size());
        bool inside = false;
        for (const auto spot : outclass_spots)
            if (distance(centers[s], spot) <= OUTCLASS_CLEAR) inside = true;
        if (inside) {
            const int other = pick_target(s, me.position(), capture_slot);
            if (other >= 0 && other != s) {
                NOTE(format("outclass: s%d sits where they are: farm s%d instead", s, other));
                slot = other;
            }
        }
    }
    farm(me, &danger, "outclass");
    return true;
}
