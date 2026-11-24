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
    if (JOBS) return jobs_ready;
    return SPREAD && spread_ready && spread_units > 0;
}

void Bot::spread_setup() {
    // Once, after the zones and spawns are known. Every champion runs the same steps on the same facts.
    spread_ready = true;
    spread_unit_of.fill(-1);
    spread_units = 0;
    if (JOBS) {
        // v221 (JOBS): no units or bands. job_base: a bare champion (JOB_BASE_DAMAGE a round, JOB_BASE_HEALTH) clears the
        // group alone, weakest camp first, for under JOB_TAKE of its health.
        const int groups = static_cast<int>(route.size());
        spread_owner.assign(groups, -1);
        job_base.assign(groups, 0);
        job_camps.assign(groups, {});
        job_skip_until.assign(groups, 0);
        int count = 0;
        for (int s = 0; s < groups; ++s) {
            auto& list = job_camps[s];
            for (const auto& camp : route[s]->camps())
                list.push_back({static_cast<float>(kind_health(camp.kind())), static_cast<float>(camp_kind_strength(camp.kind()))});
            std::sort(list.begin(), list.end());
            if (is_monolith(s) || static_cast<int>(list.size()) > JOB_MAX_CAMPS) continue;
            double rounds = 0;
            job_base[s] = job_take(s, JOB_BASE_DAMAGE, rounds) <= JOB_TAKE * JOB_BASE_HEALTH ? 1 : 0;
            count += job_base[s];
        }
        bool gated = false;
        for (const int g : gate_slots)
            if (g >= 0) gated = true;
        jobs_ready = groups > 0 && !gem_starved && !gated;
        claim_count.assign(groups, 0);
        state_seen.assign(groups, -1000);
        claim_seen.fill(JOB_CLAIM_NONE);
        claim_owner.fill(-1);
        job_mode = !jobs_ready ? JOB_MODE_OFF : draft::cluster_map() ? JOB_MODE_ALL : JOB_MODE_CARRY;
        const auto& roster = seen::roster(Team::ally);
        const bool vesper = bc::self().champion_name() == Champ::vesper_vale;
        if (job_mode == JOB_MODE_ALL) {
            job_taker = true;
            for (std::size_t i = 0; i < roster.size() && i < static_cast<std::size_t>(JOB_CLAIM_PLACES); ++i) {
                claim_owner[i] = static_cast<int>(i);
                if (roster[i] == bc::self()) claim_place = static_cast<int>(i);
            }
        } else if (job_mode == JOB_MODE_CARRY) {
            job_taker = vesper;
            for (std::size_t i = 0; i < roster.size(); ++i)
                if (roster[i].champion_name() == Champ::vesper_vale) claim_owner[JOB_CARRY_PLACE] = static_cast<int>(i);
            if (vesper) claim_place = JOB_CARRY_PLACE;
            if (claim_owner[JOB_CARRY_PLACE] < 0) job_mode = JOB_MODE_OFF;  // no carry drafted: the old team
        }
        if (job_mode == JOB_MODE_OFF) jobs_ready = false;
        if (JOB_CLAIMS) set_jobs_layout(job_mode);
        LOG(say(format("setup: jobs mode %d (%s), we %s, %d of %d groups are bare-solo", job_mode,
                       job_mode == JOB_MODE_ALL ? "everyone" : job_mode == JOB_MODE_CARRY ? "the carry" : "off",
                       job_taker ? "take jobs" : "farm the shared target", count, groups)););
        return;
    }
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
    claims_read(me);
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

int Bot::job_pick(const bc::champion& me) {
    // v221 (JOBS): the best job for us after the allies in sight took theirs. Candidates and our own rates first (path
    // steps, our damage and health); then a greedy deal over us and those allies, best rate first, lower id on a tie.
    const int n = static_cast<int>(route.size());
    const int now = static_cast<int>(bc::round_number());
    const auto position = me.position();
    const auto passives = owned_of(me).passives;
    const double damage = std::max(1.0, me.strength() * camp_dmg_mult(passives) + SAT_FOCUS_SHARE * me.focus());
    const double health = me.health();
    const double taken = camp_taken_mult(passives);
    const bool carry = me.champion_name() == Champ::vesper_vale;
    struct Cand {
        int s;
        double value, rounds;
        int ready;  // rounds from now until it is up (0: up or its reward lies there)
        int steps;  // our walking steps
        int crew;   // champions like us it takes
    };
    const int living = allies_alive();
    static std::vector<Cand> cands;
    static std::vector<int> cand_seated;
    cands.clear();
    cand_seated.clear();
    for (int u = 0; u < n; ++u) {
        if (is_monolith(u) || u == capture_slot) continue;
        if (static_cast<int>(job_camps[u].size()) > JOB_MAX_CAMPS) continue;
        const double worth = job_value(u, carry);
        if (worth <= 0) continue;
        const int air = distance(position, centers[u]);
        if (air > JOB_MAX_WALK) continue;
        const bool reward = rewards.count(u) > 0;
        const int ready = reward ? 0 : std::max(0, ready_at[u] - now);
        if (ready > JOB_MAX_WALK + JOB_WAIT) continue;
        if (job_skip_until[u] > now) continue;
        const int seated = claims_on() ? claim_count[u] : 0;
        double rounds = 0;
        int crew = 1;
        if (!reward && (u == spread_job ? spread_clear(u, damage, rounds) : job_take(u, damage, rounds)) * taken > JOB_TAKE * health) {
            crew = 0;
            for (int k = 2; k <= living && !crew && job_mode == JOB_MODE_ALL; ++k)
                if (job_take(u, k * damage, rounds) * taken <= JOB_TAKE * k * health) crew = k;
            if (!crew) continue;
        }
        if (seated >= crew) continue;  // v221 (JOB_CLAIMS): its crew is already on it
        if (u >= static_cast<int>(goal_fields.size()) || !goal_fields[u]) continue;
        const auto steps = goal_fields[u]->field.get(position);
        if (steps == bc::distance_field::unreachable || static_cast<int>(steps) > JOB_MAX_WALK) continue;
        const int walk = static_cast<int>(steps);
        if (ready > walk + JOB_WAIT) continue;
        if (reward && u != spread_job && region_allies(u, &me) > 0) continue;
        if (spread_danger(u, now)) continue;
        // A partly seated crew pulls: its value per member grows with the seats taken.
        cands.push_back({u, worth * (1 + JOB_CREW_PULL * seated / crew), rounds, ready, walk, crew});
        cand_seated.push_back(seated);
    }
    if (cands.empty()) return -1;
    struct Agent {
        vec2 at;
        int ident;
        bool dealt;
    };
    std::array<Agent, 8> agents{};
    int agent_n = 0;
    const int mine = cached_ident(me);
    agents[agent_n++] = {position, mine, false};
    {
        const auto& allies = seen::champions(Team::ally);
        const auto& tiles = seen::positions(Team::ally);
        const auto& roster = seen::roster(Team::ally);
        for (std::size_t i = 0; i < allies.size() && i < tiles.size() && agent_n < static_cast<int>(agents.size()); ++i) {
            if (job_mode != JOB_MODE_ALL) break;  // the carry alone takes jobs
            if (allies[i] == me || channelling(allies[i])) continue;
            // v221 (JOB_CLAIMS): one with a claim has its job (and its seat is counted).
            if (claims_on()) {
                bool claims = false;
                for (int r = 0; r < JOB_CLAIM_PLACES; ++r)
                    if (claim_owner[r] >= 0 && claim_owner[r] < static_cast<int>(roster.size()) && roster[claim_owner[r]] == allies[i])
                        claims = claim_seen[r] >= 1 && claim_seen[r] <= JOB_CLAIM_MAX_GROUP + 1;
                if (claims) continue;
            }
            agents[agent_n++] = {tiles[i], cached_ident(allies[i]), false};
        }
    }
    static std::vector<int> gone;  // seats left
    gone.resize(cands.size());
    for (std::size_t c = 0; c < cands.size(); ++c) gone[c] = cands[c].crew - cand_seated[c];
    const double capture = capture_rounds;
    for (int deal = 0; deal < agent_n; ++deal) {
        int best_a = -1, best_c = -1;
        double best_rate = 0;
        for (int a = 0; a < agent_n; ++a) {
            if (agents[a].dealt) continue;
            for (std::size_t c = 0; c < cands.size(); ++c) {
                if (gone[c] <= 0) continue;
                const Cand& k = cands[c];
                const int walk = distance(agents[a].at, centers[k.s]);
                double rate = k.value / k.crew / (std::max(walk, k.ready) + k.rounds + capture);
                if (walk <= JOB_NEAR) rate *= JOB_KEEP;
                else if (a == 0 && k.s == spread_job) rate *= JOB_KEEP;
                if (best_a < 0 || rate > best_rate || (rate == best_rate && agents[a].ident < agents[best_a].ident)) {
                    best_a = a;
                    best_c = static_cast<int>(c);
                    best_rate = rate;
                }
            }
        }
        if (best_a < 0) return -1;
        if (best_a == 0) {
            job_crew = cands[best_c].crew;
            return cands[best_c].s;
        }
        agents[best_a].dealt = true;
        --gone[best_c];
    }
    return -1;
}

double Bot::job_take(int s, double damage, double& rounds) const {
    // The damage a lone champion takes clearing a full group s, its camps dying weakest first, each hitting until it dies.
    double spent = 0, take = 0;
    for (const auto& [hp, strength] : job_camps[s]) {
        spent += hp;
        take += strength * std::ceil(spent / damage);
    }
    rounds = std::ceil(spent / damage);
    return take;
}

bool Bot::claims_on() const {
    return JOBS && JOB_CLAIMS && jobs_ready && jobs_layout() != JOB_MODE_OFF;
}

double Bot::job_value(int s, bool carry) const {
    return route[s]->gem_value() + (carry ? JOB_GOLD_CARRY : JOB_GOLD_OTHER) * route[s]->gold_value();
}

void Bot::claims_read(const bc::champion&) {
    // v221 (JOB_CLAIMS): once a turn, before any pick. A claim that moved off a group means its owner captured it.
    if (!claims_on()) return;
    const int now = static_cast<int>(bc::round_number());
    const int n = static_cast<int>(route.size());
    std::fill(claim_count.begin(), claim_count.end(), 0);
    const auto& roster = seen::roster(Team::ally);
    for (int i = 0; i < JOB_CLAIM_PLACES; ++i) {
        const int owner = claim_owner[i];
        if (owner < 0 || owner >= static_cast<int>(roster.size())) continue;
        const int value = claim_read(i);
        const int before = claim_seen[i];
        claim_seen[i] = value;
        if (i == claim_place) continue;
        bool alive = false;
        try {
            alive = roster[owner].is_alive();
        } catch (const bc::error&) {
        }
        if (before >= 1 && before <= JOB_CLAIM_MAX_GROUP + 1 && value != before && value != JOB_CLAIM_LEFT && alive) {
            const int s = before - 1;
            // Not a group we see the state of ourselves this turn, nor one we already know is down.
            if (s < n && state_seen[s] != now && ready_at[s] <= now) {
                ready_at[s] = now + route[s]->max_respawn_timer();
                rewards.erase(s);
                NOTE(format("claim news: s%d was captured, respawns in %d", s, route[s]->max_respawn_timer()));
            }
        }
        if (alive && value >= 1 && value <= JOB_CLAIM_MAX_GROUP + 1 && value - 1 < n) ++claim_count[value - 1];
    }
}

void Bot::claims_write() {
    // v221 (JOB_CLAIMS): the end of our turn. A job turn shows the job; any other turn shows none. A group left without
    // its capture shows 63 for one round first, so nobody starts its respawn clock.
    if (!claims_on() || claim_place < 0) return;
    int wanted = JOB_CLAIM_NONE;
    if (job_turn && spread_job >= 0 && spread_job <= JOB_CLAIM_MAX_GROUP) wanted = spread_job + 1;
    const bool held = claim_written >= 1 && claim_written <= JOB_CLAIM_MAX_GROUP + 1;
    if (held && wanted != claim_written && !claim_captured) wanted = JOB_CLAIM_LEFT;
    claim_captured = false;
    if (wanted == claim_written) return;
    claim_write(claim_place, wanted);
    claim_written = wanted;
}

bool Bot::job_capturer(const bc::champion& me, int s) {
    // v221 (JOB_CLAIMS): a cleared crew job: one of us in sight takes the reward. The old collect order first (1 before 2,
    // 0 last), then whoever is nearer its tiles, then the lower id.
    auto key_of = [&](const bc::champion& c, vec2 at) {
        const int rank = collect_rank(c, s);
        int near = 1000;
        for (const auto t : spawn_lists[s]) near = std::min(near, distance(at, t));
        return std::tuple<int, int, int>{rank == 0 ? 3 : rank, near, cached_ident(c)};
    };
    const auto mine = key_of(me, me.position());
    const auto& allies = seen::champions(Team::ally);
    const auto& tiles = seen::positions(Team::ally);
    for (std::size_t i = 0; i < allies.size() && i < tiles.size(); ++i) {
        if (allies[i] == me || channelling(allies[i])) continue;
        if (distance(tiles[i], centers[s]) > JOB_COMMIT_RANGE + 2) continue;
        if (key_of(allies[i], tiles[i]) < mine) return false;
    }
    return true;
}

int Bot::job_bag_bar(vec2 position) const {
    int steps = 1000;
    for (const auto altar : seen::altar_list()) steps = std::min(steps, distance(position, altar));
    if (steps >= 1000) return JOB_BAG_MAX;
    return std::clamp(JOB_BAG_MIN + static_cast<int>(JOB_BAG_STEP * steps), JOB_BAG_MIN, JOB_BAG_MAX);
}

bool Bot::job_committed(const bc::champion& me) {
    // v221 (JOBS): within JOB_COMMIT_RANGE of a started shared target that is no bare-solo group: the team's job.
    if (route.empty()) return false;
    const int s = slot % static_cast<int>(route.size());
    if (s < static_cast<int>(job_base.size()) && job_base[s]) return false;
    return distance(me.position(), centers[s]) <= JOB_COMMIT_RANGE && mid_clear(s);
}

int Bot::spread_pick(const bc::champion& me) {
    if (JOBS) return job_pick(me);
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
    // v221 (JOB_CLAIMS): a capture lets the claim move on at once (the others start its respawn clock).
    if (spread_job >= 0 && std::string_view(why) == "captured") claim_captured = true;
    spread_job = -1;
}

bool Bot::spread_step(const bc::champion& me, int now_mode) {
    // The solo's farm turn: its own group instead of the shared target. False hands the turn to farm() (the core's).
    if (!spread_on() || (!JOBS && spread_unit < 0) || route.empty()) return false;
    if (JOBS && !job_taker) return false;
    const int now = static_cast<int>(bc::round_number());
    if (now_mode == RETREAT) {
        spread_drop("retreat");
        return false;
    }
    if (!seen::champions(Team::enemy).empty()) {
        spread_drop("enemy in sight");
        spread_cool_until = now + (JOBS ? JOB_ENEMY_COOL : SPREAD_ENEMY_COOL);
        return false;
    }
    if (now < spread_cool_until) return false;
    const int s = slot % static_cast<int>(route.size());
    if (mode == FIGHT && distance(me.position(), centers[s]) <= (JOBS ? JOB_HELP : SPREAD_HELP)) {
        spread_drop("the team fights near us");
        return false;
    }
    // v221 (JOBS): no health bar: a job must cost under JOB_TAKE of the health we have, and clusters cost none.
    if (!JOBS && me.health() < me.max_health() * SPREAD_HEALTH) {
        spread_drop("hurt");
        return false;
    }
    if (!JOBS && static_cast<int>(me.gem_count()) >= SPREAD_BAG) {
        spread_drop("bag to bank");
        return false;
    }
    if (JOBS && spread_job < 0 && job_committed(me)) return false;
    if (JOBS && JOB_BREAKER && job_hist_n >= 4 && job_hist_at[0] == job_hist_at[2] && job_hist_at[1] == job_hist_at[3]
        && job_hist_at[0] != job_hist_at[1] && job_hist_job[0] != job_hist_job[1] && job_hist_job[0] == job_hist_job[2]
        && job_hist_job[1] == job_hist_job[3]) {
        spread_drop("our walk and another rule's undo each other: no job for a while");
        spread_cool_until = now + JOB_BREAK_ROUNDS;
        job_hist_n = 0;
        return false;
    }
    if (!JOBS && (me.champion_name() == Champ::cassi || me.champion_name() == Champ::issac)) {
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
    if (claims_on() && spread_job >= 0 && claim_count[spread_job] >= job_crew && !mid_clear(spread_job)
        && !spawn_board(spread_job).is_set(me.position()))
        spread_drop("claimed by others meanwhile");
    if (claims_on() && spread_job >= 0 && job_crew > 1 && rewards.count(spread_job) && !job_capturer(me, spread_job))
        spread_drop("captured");  // an ally in sight takes the reward: the group is as good as captured for us
    const bool crew_busy = JOBS && spread_job >= 0 && job_crew > 1 && mid_clear(spread_job);
    if (JOBS && spread_job >= 0 && job_crew > 1 && !crew_busy && stage_slot == spread_job && stage_last >= now - 1
        && now - stage_since >= JOB_STAGE_PATIENCE) {
        job_skip_until[spread_job] = now + JOB_STAGE_SKIP;
        spread_drop("staged too long: no crew came");
    }
    if (!crew_busy && (spread_job < 0 || now - spread_pick_round >= (JOBS ? JOB_REPICK : SPREAD_REPICK))) {
        const int pick = spread_pick(me);
        spread_pick_round = now;
        if (pick != spread_job) {
            if (pick >= 0) NOTE(format("spread: job s%d", pick));
            else spread_drop(JOBS ? "no job for us" : "nothing in our band");
        }
        spread_job = pick;
    }
    if (spread_job < 0) return false;
    slot = spread_job;
    // v221 (JOBS): a crew job keeps the stage gate (the crew opens it together); a job of our own needs none.
    spread_farming = !(JOBS && job_crew > 1);
    job_turn = true;
    farm(me, nullptr, JOBS ? (job_crew > 1 ? "crew" : "job") : "spread");
    spread_farming = false;
    return true;
}

// v204 (OUTCLASS): see constants.h.

int Bot::outclass_gap(const bc::champion& me, bool& near, bool& finish) {
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
