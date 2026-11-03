// v202 (BUSY_JOBS): side jobs. A champion the team target does not need, or one that would only stand beside a
// respawning target, farms a group of its own: up by its arrival, on our side or the middle, cleared alone for at most
// JOB_RISK of its health. It captures it, then rejoins the team. Fights, recalls, banking, captures and pings keep their
// places in play(): a job only replaces farm()'s walking and waiting. See BUSY_JOBS in constants.h.
#include "bot.h"
#include "seen.h"
#include "state.h"
#include <algorithm>
#include <array>
#include <cmath>

namespace {

// Camp health by kind (the rules' table): camps out of sight hide theirs, and a group comes back at full health.
double kind_health(bc::camp_type kind) {
    switch (kind) {
    case bc::camp_type::wolf: return 1350;
    case bc::camp_type::elk: return 2400;
    case bc::camp_type::monolith: return 20000;
    case bc::camp_type::gem_cluster: case bc::camp_type::gold_cluster: return 120;
    default: return 5000;
    }
}

}  // namespace

bool Bot::jobs_on() {
    // Read once after setup: the zones, the Prism team and the roster never change.
    if (!ready) return false;
    if (!jobs_on_cache) {
        bool pair = false;
        for (const auto& ally : seen::roster(Team::ally)) {
            try {
                if (split_member(ally.champion_name())) pair = true;
            } catch (const bc::error&) {
            }
        }
        jobs_on_cache = BUSY_JOBS && zones_known && !route.empty() && !prism_team() && !pair;
    }
    return *jobs_on_cache;
}

int Bot::job_reg() const {
    // The latest job in the pair's target bits; zeroed memory (no leader has written yet) holds none.
    if (shared_byte(STAMP_BYTE) == 0) return NO_TARGET2;
    return shared_byte(PHASE_BYTE) & TARGET2_MASK;
}

int Bot::job_bit() {
    // Our bit among the job holders (the mode byte's carrier bits, by roster index modulo four; read once).
    if (job_bit_cache < 0) {
        const auto& team = seen::roster(Team::ally);
        const auto& me = bc::self();
        int index = 0;
        for (std::size_t i = 0; i < team.size(); ++i)
            if (team[i] == me) index = static_cast<int>(i);
        job_bit_cache = 1 << (CARRIER_SHIFT + index % 4);
    }
    return job_bit_cache;
}

int Bot::job_holders() const {
    if (!JOB_SHARED || shared_byte(STAMP_BYTE) == 0) return 0;
    return __builtin_popcount(static_cast<unsigned>(shared_byte(MODE_BYTE) & CARRIER_MASK));
}

bool Bot::job_enemy_near(int s, int since) const {
    // An enemy champion last seen (by us) within JOB_ENEMY_RANGE of group s at round `since` or later.
    for (const auto& [round, tile] : job_enemy_seen)
        if (round >= since && round > 0 && distance(tile, centers[s]) <= JOB_ENEMY_RANGE) return true;
    return false;
}

void Bot::job_alive_counts() {
    // Roster life reads, once a round.
    if (job_alive_round == turn_round) return;
    job_alive_round = turn_round;
    const int ours = allies_alive(), theirs = enemies_alive();
    job_down = ours < theirs;
    job_up = ours > theirs;
}

bool Bot::job_outnumbered() {
    // JOB_EVEN: fewer of us alive than of them.
    if (!JOB_EVEN) return false;
    job_alive_counts();
    return job_down;
}

bool Bot::job_ahead() {
    job_alive_counts();
    return job_up;
}

bool Bot::ally_on_job(const bc::champion& ally) {
    // Whether an ally's bit is up among the job holders (roster index modulo four, so a fifth champion shares the first's).
    if (!jobs_shared() || job_holders() == 0) return false;
    const auto& team = seen::roster(Team::ally);
    for (std::size_t i = 0; i < team.size(); ++i)
        if (team[i] == ally) return (shared_byte(MODE_BYTE) & (1 << (CARRIER_SHIFT + static_cast<int>(i) % 4))) != 0;
    return false;
}

void Bot::mark_job(bool on) {
    if (!jobs_shared()) return;
    const int bit = job_bit();
    const int byte = shared_byte(MODE_BYTE);
    const int want = on ? (byte | bit) : (byte & ~bit & 0xFF);
    if (want != byte) set_shared_byte(MODE_BYTE, want);
}

double Bot::job_dps(const bc::champion& me) {
    // Our camp damage a round: Strength with Monster (and Collectors); an attack reset adds a hit once a cooldown.
    double dps = me.strength() * camp_dmg_mult(owned_of(me).passives);
    if (ability_of(me, Kind::attack_reset)) dps *= 1.0 + JOB_RESET_SHARE;
    return dps;
}

std::size_t Bot::job_camps(int s, std::array<JobCamp, 16>& out) {
    // Live camps of group s, weakest first: those in sight at their health, the rest at full health (a group comes back
    // whole); a member whose tile we see with no camp on it is dead. A cleared group waiting for its capture has none.
    if (s < 0 || s >= static_cast<int>(route.size()) || rewards.count(s)) return 0;
    const bool back = ready_at[s] > turn_round;
    std::size_t n = 0;
    bool sight = false;
    for (const auto& member : route[s]->camps()) {
        if (n >= out.size()) break;
        const auto spot = member.spawn_position();
        const double strength = camp_kind_strength(member.kind());
        if (!back) {
            bool found = false;
            for (const auto& c : seen::camps())
                if (c.spawn_position() == spot) {
                    out[n++] = {static_cast<double>(c.health()), strength};
                    found = true;
                    break;
                }
            if (found) continue;
            if (!sight) {
                if (job_sight_gen != seen::generation()) {
                    bc::vision::visible_tiles_into(job_sight);
                    job_sight_gen = seen::generation();
                }
                sight = true;
            }
            if (job_sight.is_set(spot)) continue;
        }
        out[n++] = {kind_health(member.kind()), strength};
    }
    std::sort(out.begin(), out.begin() + n, [](const JobCamp& a, const JobCamp& b) { return a.health < b.health; });
    return n;
}

double Bot::job_take(const bc::champion& me, int s, double dps, double& rounds) {
    // Alone, every camp of the group hits us until it dies (FINISH_ORDER's model with nobody nearer), weakest first,
    // Jungle and Durability cutting it.
    rounds = 0;
    std::array<JobCamp, 16> camps{};
    const std::size_t n = job_camps(s, camps);
    if (n == 0) return 0;
    if (dps <= 0) return 1e9;
    double spent = 0, take = 0;
    for (std::size_t k = 0; k < n; ++k) {
        spent += camps[k].health;
        take += camps[k].strength * std::ceil(spent / dps);
    }
    rounds = std::ceil(spent / dps);
    return take * camp_taken_mult(owned_of(me).passives);
}

bool Bot::job_surplus(const bc::champion& me, int s) {
    // The team target's squad: the allies in sight nearer it than us (a tie: the one acting first), nearest first, as
    // few as clear it for at most JOB_NEED_RISK of their summed health (basic attacks only). We are spare when they make
    // that squad without us. One about to recall, or channelling, is not counted; nor, on camps that hit back, Willow,
    // who never opens one (she waited out HOLD_LIMIT beside one while its melee was on a job).
    std::array<JobCamp, 16> camps{};
    const std::size_t n = job_camps(s, camps);
    bool hits = false;
    for (std::size_t c = 0; c < n; ++c) hits = hits || camps[c].strength > 0;
    const auto& allies = seen::champions(Team::ally);
    const auto& tiles = seen::positions(Team::ally);
    const auto center = centers[s];
    const int mine = distance(me.position(), center);
    struct Body {
        int gap;
        bool first;
        double health, dps;
    };
    std::array<Body, 8> nearer{};
    std::size_t k = 0;
    for (std::size_t i = 0; i < allies.size() && i < tiles.size() && k < nearer.size(); ++i) {
        const auto& a = allies[i];
        if (a == me || channelling(a)) continue;
        const int gap = distance(tiles[i], center);
        if (gap > mine) continue;
        const bool first = acts_before_me(a);
        if (gap == mine && !first) continue;
        const double health = a.health();
        if (health < a.max_health() * RECALL_HEALTH) continue;
        if (hits && cached_ident(a) == champ_id(Champ::willow) + 1) continue;
        nearer[k++] = {gap, first, health, a.strength() * camp_dmg_mult(owned_of(a).passives)};
    }
    if (k == 0) return false;
    // Nothing that hits back (clusters, or a reward waiting): one of them is enough.
    if (!hits) return true;
    std::sort(nearer.begin(), nearer.begin() + k,
              [](const Body& a, const Body& b) { return a.gap != b.gap ? a.gap < b.gap : a.first > b.first; });
    double dps = 0, health = 0;
    for (std::size_t j = 0; j < k; ++j) {
        dps += nearer[j].dps;
        health += nearer[j].health;
        if (dps <= 0) continue;
        double spent = 0, take = 0;
        for (std::size_t c = 0; c < n; ++c) {
            spent += camps[c].health;
            take += camps[c].strength * std::ceil(spent / dps);
        }
        if (take <= JOB_NEED_RISK * health) return true;
    }
    return false;
}

int Bot::job_want(const bc::champion& me, int s) {
    // -1: no job now; 0: any job (the team target does not need us); n > 0: a job done, and walked back from, within the
    // n rounds the team target still respawns. job_why (debug log only) says why not.
    // Without JOB_SURPLUS only an idle team target (respawning, or another's capture) gives jobs: that test goes first,
    // as most farm turns have the target up (no engine read).
    const bool respawning = ready_at[s] > turn_round;
    const bool others = capture_slot == s && collector != cached_ident(me);
    if (!respawning && !others && (!JOB_SURPLUS || (JOB_SURPLUS_AHEAD && !job_ahead()))) {
        LOG(job_why = "the team target is up";);
        return -1;
    }
    if (mode != FARM || win_tribute || !near_enemies().empty() || turn_round - last_enemy_round < JOB_QUIET) {
        LOG(job_why = "not a quiet farm turn (team mode, an enemy champion near or seen lately)";);
        return -1;
    }
    if (me.health() < me.max_health() * JOB_HEALTH || static_cast<int>(me.gem_count()) >= CARRY_GEMS) {
        LOG(job_why = "too hurt or carrying a heavy bag";);
        return -1;
    }
    if (job_outnumbered()) {
        LOG(job_why = "fewer of us alive than of them";);
        return -1;
    }
    // The core stays together: at most roster - JOB_CORE (at least one) of us on jobs at once.
    if (jobs_shared() && job_holders() >= std::max(1, roster_size() - JOB_CORE)) {
        LOG(job_why = format("%d allies already on jobs", job_holders()););
        return -1;
    }
    // On the team target already (its camps within a step of our reach): we finish it with the team. Straight off the
    // vision list (camps never leave their spawn tile, so no position read, and no list built).
    const auto position = me.position();
    const int reach = me.attack_range();
    bool live = false;
    for (const auto& c : seen::camps()) {
        if (slot_at(c.spawn_position()) != s) continue;
        live = true;
        if (distance(position, c.spawn_position()) <= reach + 1) {
            LOG(job_why = "on the team target";);
            return -1;
        }
    }
    // JOB_SURPLUS_AHEAD: spare champions leave only while more of us are alive than of them.
    const bool spare_ok = JOB_SURPLUS && (!JOB_SURPLUS_AHEAD || job_ahead());
    if (!live && respawning) {
        const int left = ready_at[s] - turn_round;
        if (left <= JOB_WAIT_SHORT) {
            LOG(job_why = "it respawns soon";);
            return -1;
        }
        return spare_ok && job_surplus(me, s) ? 0 : left;
    }
    // Someone else collects it: the team moves on, nothing for us there.
    if (!live && others) return 0;
    if (spare_ok && job_surplus(me, s)) return 0;
    LOG(job_why = "the team target needs us";);
    return -1;
}

int Bot::pick_job(const bc::champion& me, int s, int fit, double& cost) {
    // The best group for us alone, by value / (steps + wait + clear + capture). With fit > 0 the job, plus the walk back
    // to the team target s, ends within fit rounds.
    const int n = static_cast<int>(route.size());
    const int now = turn_round;
    const auto position = me.position();
    const double health = me.health();
    const double dps = job_dps(me);
    const bool willow = me.champion_name() == Champ::willow;
    const int reg = jobs_shared() ? job_reg() : NO_TARGET2;
    const auto& allies = seen::champions(Team::ally);
    const auto& tiles = seen::positions(Team::ally);
    const bool s_field = s < static_cast<int>(goal_fields.size()) && goal_fields[s];
    int best = -1;
    double best_score = 0;
    cost = 0;
    // Debug log only: candidates turned away, by reason (side, enemy, far, field, steps, taken, gate, value, risk, fit).
    [[maybe_unused]] std::array<int, 10> no{};
    for (int u = 0; u < n; ++u) {
        // JOB_NEAR_TEAM first (the most selective, and arithmetic only): with the team, within TEAM_RANGE of its target,
        // so the fight score sees us and we join a fight in a few steps.
        if (JOB_NEAR_TEAM && distance(centers[u], centers[s]) > TEAM_RANGE) {
            LOG(++no[2];);
            continue;
        }
        if (u == s || u == capture_slot || u == reg || is_monolith(u)) continue;
        if (u == job_drop_slot && now - job_drop_round < JOB_DROP_COOL) continue;
        if (zone(u) == ZONE_UNSAFE || !zone_open(u)) {
            LOG(++no[0];);
            continue;
        }
        if (u < static_cast<int>(home_steps.size())) {
            const int ours = home_steps[u], theirs = enemy_steps[u];
            if (ours >= 255 || ours + theirs <= 0 || ours > JOB_SIDE_SHARE * (ours + theirs)) {
                LOG(++no[0];);
                continue;
            }
        }
        if (job_enemy_near(u, now - JOB_ENEMY_ROUNDS)) {
            LOG(++no[1];);
            continue;
        }
        // Cheap bounds first: walking steps are never under the tile distance to the nearest spawn tile.
        const int far = distance(position, centers[u]);
        if (far > JOB_MAX_STEPS + 3 || ready_at[u] > now + JOB_MAX_STEPS) {
            LOG(++no[2];);
            continue;
        }
        if (willow && !clusters_only[u] && !rewards.count(u)) continue;
        if (u >= static_cast<int>(goal_fields.size()) || !goal_fields[u]) {
            LOG(++no[3];);
            continue;
        }
        const int steps = goal_fields[u]->field.get(position);
        if (steps == bc::distance_field::unreachable || steps > JOB_MAX_STEPS || ready_at[u] > now + steps) {
            LOG(++no[4];);
            continue;
        }
        // An ally in sight nearer the group, and close to it, has it.
        bool taken = false;
        for (std::size_t i = 0; i < allies.size() && i < tiles.size() && !taken; ++i) {
            if (allies[i] == me || channelling(allies[i])) continue;
            const int g = distance(tiles[i], centers[u]);
            taken = g <= JOB_ALLY_RANGE && (g < far || (g == far && acts_before_me(allies[i])));
        }
        if (taken) {
            LOG(++no[5];);
            continue;
        }
        if (!allowed(u)) {
            LOG(++no[6];);
            continue;
        }
        const double value = values[aggressive][u];
        if (value <= 0) {
            LOG(++no[7];);
            continue;
        }
        double rounds = 0;
        const double take = job_take(me, u, dps, rounds);
        if (take > JOB_RISK * health) {
            LOG(++no[8];);
            continue;
        }
        const int wait = std::max(0, ready_at[u] - now - steps);
        const double busy = steps + wait + rounds + capture_rounds;
        if (fit > 0) {
            // Walking steps back to the team target: its own field when built, else the tile distance.
            const auto back = s_field ? gap(u, s) : std::optional<int>{distance(centers[u], centers[s])};
            if (!back || busy + *back > fit) {
                LOG(++no[9];);
                continue;
            }
        }
        const double score = value / std::max(1.0, busy);
        if (score > best_score) {
            best = u;
            best_score = score;
            cost = busy;
        }
    }
    LOG(if (best < 0) job_why = format("no group fits%s: side %d, enemy %d, far %d, field %d, steps %d, taken %d, gate %d, "
                                       "value %d, risk %d, round trip %d", fit > 0 ? format(" in %d", fit).c_str() : "",
                                       no[0], no[1], no[2], no[3], no[4], no[5], no[6], no[7], no[8], no[9]););
    return best;
}

bool Bot::job_step(const bc::champion& me) {
    // farm()'s step: keep the job upkeep validated this turn, or take one when the team target leaves us idle.
    LOG(job_why.clear(););
    if (!jobs_on()) return false;
    if (job_slot >= 0) return true;
    const int s = slot % static_cast<int>(route.size());
    const int now = turn_round;
    if (job_search_target == s && now - job_search_round < JOB_RETRY) {
        LOG(job_why = "searched lately";);
        return false;
    }
    const int fit = job_want(me, s);
    if (fit < 0) return false;
    double cost = 0;
    const int u = pick_job(me, s, fit, cost);
    if (u < 0) {
        job_search_round = now;
        job_search_target = s;
        return false;
    }
    job_slot = u;
    job_since = now;
    job_ready = ready_at[u];
    job_health = me.health();
    job_until = now + static_cast<int>(std::ceil(cost)) + JOB_SLACK;
    slot = u;
    keep_slot = -1;
    if (jobs_shared() && u < NO_TARGET2) write_target2(u);
    mark_job(true);
    NOTE(format("job: takes s%d alone (~%.0f rounds): ", u, cost)
         + (fit > 0 ? format("s%d respawns in %d", s, fit)
            : capture_slot == s ? format("an ally collects s%d", s) : std::string("the team's target needs nobody more")));
    return true;
}

void Bot::drop_job(const char* why, bool cool) {
    if (job_slot < 0) return;
    NOTE(format("job s%d ends: ", job_slot) + why);
    if (jobs_shared() && job_reg() == job_slot) write_target2(NO_TARGET2);
    mark_job(false);
    if (cool) {
        job_drop_slot = job_slot;
        job_drop_round = turn_round;
    }
    job_slot = -1;
    keep_slot = -1;
}

const char* Bot::job_keep(const bc::champion& me, bool& cool) {
    // Why the job ends now, or nullptr to keep it; cool: its group is left alone for JOB_DROP_COOL rounds.
    const int u = job_slot;
    const int now = turn_round;
    cool = false;
    if (!me.is_alive()) return "dead";
    if (u >= static_cast<int>(route.size())) return "no such group";
    // A respawn we planned for (up by our arrival) is not a capture: only a new timer is.
    if (ready_at[u] > now && ready_at[u] != job_ready) return "its group respawns (captured)";
    if (u == main_slot) return "it is the team's target now";
    if (win_tribute) return "the team tributes to win";
    if (me.is_recalling()) return "recalling";
    // The team fights: back to it (Fish regroups for fights). Down bodies: back to it too, before their raid finds us.
    if (mode == FIGHT) return "the team fights";
    if (job_outnumbered()) return "fewer of us alive than of them";
    cool = true;
    if (!near_enemies().empty()) return "an enemy champion is near";
    if (job_enemy_near(u, job_since + 1)) return "an enemy champion was seen near it";
    if (now > job_until) return "taking too long";
    const auto position = me.position();
    const auto& tiles = spawn_board(u);
    const bool on = tiles.is_set(position);
    if (capture_slot == u && collector != cached_ident(me) && !on) return "an ally collects it";
    // Started by us: a camp of it within our reach.
    const int reach = me.attack_range();
    bool engaged = false;
    for (const auto& c : seen::camps())
        if (slot_at(c.spawn_position()) == u && distance(position, c.spawn_position()) <= reach) {
            engaged = true;
            break;
        }
    // JOB_NEAR_TEAM: the team's target moved off (past FIGHT_LOCAL_RANGE, looser than the pick's TEAM_RANGE): back to it,
    // unless we are on the job already.
    if (JOB_NEAR_TEAM && !engaged && !on && main_slot >= 0 && main_slot < static_cast<int>(route.size())
        && distance(centers[u], centers[main_slot]) > FIGHT_LOCAL_RANGE)
        return "the team moved off";
    if (!engaged && !on) {
        const int far = distance(position, centers[u]);
        const auto& allies = seen::champions(Team::ally);
        const auto& ally_tiles = seen::positions(Team::ally);
        for (std::size_t i = 0; i < allies.size() && i < ally_tiles.size(); ++i) {
            if (allies[i] == me || channelling(allies[i])) continue;
            const int g = distance(ally_tiles[i], centers[u]);
            if (g <= JOB_ON_RANGE && (g < far || (g == far && acts_before_me(allies[i])))) return "an ally nearer it works it";
        }
    }
    // Health: the camps only lose health, so while ours has not dropped since the pick the pick's risk still holds.
    const double health = me.health();
    if (health >= job_health) return nullptr;
    double rounds = 0;
    const double top = me.max_health();
    const double take = job_take(me, u, job_dps(me), rounds);
    if (mid_clear(u) && engaged) {
        if (health - take < top * JOB_FINISH_HEALTH) return "too hurt to finish it";
    } else if (health < top * JOB_HEALTH || take > JOB_KEEP_RISK * health) {
        return "too hurt to take it on";
    }
    return nullptr;
}

void Bot::job_upkeep(const bc::champion& me) {
    // Every turn after the shared decision (dead or alive): enemy sightings, the shared job byte, and our job.
    if (!jobs_on()) return;
    const int now = turn_round;
    // Each enemy champion's last sighting (tile and round, by roster index): tiles come with the vision list (no engine
    // read); groups are checked against them only when a job is picked or kept.
    if (me.is_alive()) {
        const auto& visible = seen::champions(Team::enemy);
        const auto& spots = seen::positions(Team::enemy);
        const auto& roster = seen::roster(Team::enemy);
        for (std::size_t i = 0; i < visible.size() && i < spots.size(); ++i)
            for (std::size_t k = 0; k < roster.size() && k < job_enemy_seen.size(); ++k)
                if (roster[k] == visible[i]) {
                    job_enemy_seen[k] = {now, spots[i]};
                    break;
                }
    }
    // A job group seen respawning is done: free the byte (its holder may have lost track of it). A dead holder clears its
    // own bit and byte on its dead turn (job_keep).
    if (jobs_shared()) {
        const int reg = job_reg();
        if (reg != NO_TARGET2 && reg != job_slot && reg < static_cast<int>(route.size()) && ready_at[reg] > now)
            write_target2(NO_TARGET2);
    }
    if (job_slot < 0) return;
    bool cool = false;
    if (const char* why = job_keep(me, cool)) {
        drop_job(why, cool);
        return;
    }
    // Our farm target for the turn (as farm_alone and the split pair do): the walk guard, landings, the finish and the
    // band rules all read it.
    slot = job_slot;
    mark_job(true);
    if (jobs_shared() && job_slot < NO_TARGET2 && job_reg() == NO_TARGET2) write_target2(job_slot);
}
