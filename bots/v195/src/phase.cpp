// v86: the leader's phase (group, centre, split, raid), the split pair, and the per-champion farming steps.
#include "bot.h"
#include "seen.h"
#include "state.h"
#include "trace.h"
#include <algorithm>
#include <array>
#include <cmath>

namespace {

int ident_of(const bc::champion& champion) {
    try {
        const int ident = champ_id(champion.champion_name()) + 1;
        return ident >= 0 && ident < 16 ? ident : -1;
    } catch (const bc::error&) {
        return -1;
    }
}

} // namespace

bool Bot::in_pair() {
    if (!in_pair_cache) in_pair_cache = split_member(bc::self().champion_name());
    return *in_pair_cache;
}

std::optional<Champ> Bot::sat_kind() const {
    // v159 (SATELLITE): the first of SAT_KINDS on a team of SAT_MIN_TEAM+ without Cassi & Issac; kinds never change.
    if (!sat_kind_cache) {
        std::optional<Champ> kind;
        if (SATELLITE && !as_v170() && roster_size() >= SAT_MIN_TEAM) {
            std::vector<Champ> kinds;
            for (const auto& ally : seen::roster(Team::ally)) {
                try {
                    kinds.push_back(ally.champion_name());
                } catch (const bc::error&) {
                }
            }
            const bool pair = std::find(kinds.begin(), kinds.end(), Champ::cassi) != kinds.end()
                || std::find(kinds.begin(), kinds.end(), Champ::issac) != kinds.end();
            if (!pair)
                for (const Champ k : SAT_KINDS)
                    if (!kind && std::find(kinds.begin(), kinds.end(), k) != kinds.end()) kind = k;
        }
        sat_kind_cache = kind;
    }
    return *sat_kind_cache;
}

double Bot::mf_bias(int u) const {
    if (!MULTIFARM) return 1.0;
    double bias = MF_NEAR / (MF_NEAR + distance(centers[u], ally_spawn_list.front()));
    if (mf_single[u]) bias *= MF_SINGLE;
    if (sat_elk[u]) bias *= MF_ELK;
    return bias;
}

bool Bot::sat_ready() const {
    // v159 (SATELLITE): the leader's view of the satellite: alive, not collecting or carrying, a small bag, and (when in
    // sight) healthy. Life and bags read out of sight; health only in sight.
    const auto kind = sat_kind();
    if (!kind) return false;
    for (const auto& ally : seen::roster(Team::ally)) {
        try {
            if (ally.champion_name() != *kind) continue;
            if (!ally.is_alive() || (!SAT_BANK_OWN && gems_carried(ally) >= SAT_BAG)) return false;
            // v192 (MULTIFARM): Vesper leaves the team only with her core built.
            if (MULTIFARM && *kind == Champ::vesper_vale) {
                const auto owned = emblems.find(champ_id(*kind));
                if (owned == emblems.end() || owned->second < VESPER_CORE_EMBLEMS) return false;
            }
            const int id = cached_ident(ally);
            if ((capture_slot != NO_CAPTURE && collector == id) || carrier == id) return false;
        } catch (const bc::error&) {
            return false;
        }
        for (const auto& seen_ally : seen::champions(Team::ally))
            if (seen_ally == ally && seen_ally.health() < seen_ally.max_health() * SAT_HEALTH) return false;
        return true;
    }
    return false;
}

const std::vector<vec2>& Bot::sat_bank_altars(vec2 position) {
    // v159 stage 3 (SAT_BANK_OWN): an altar's side by walking steps from its nearest free neighbour to each spawn (the
    // spawn fields are built once, on first use).
    if (!sat_altar_list) {
        sat_altar_list.emplace();
        for (const auto altar : bc::map::altars().positions()) {
            int ours = 1 << 20, theirs = 1 << 20;
            for (int dx = -1; dx <= 1; ++dx)
                for (int dy = -1; dy <= 1; ++dy) {
                    const vec2 tile{altar.x + dx, altar.y + dy};
                    if ((dx == 0 && dy == 0) || tile.x < 0 || tile.y < 0 || tile.x >= static_cast<int>(bc::map::width())
                        || tile.y >= static_cast<int>(bc::map::height()))
                        continue;
                    ours = std::min(ours, steps_to_ally_spawn(tile));
                    theirs = std::min(theirs, steps_to_enemy_spawn(tile));
                }
            if (ours < (1 << 20) && theirs + SAT_BANK_SIDE > ours) sat_altar_list->push_back(altar);
        }
    }
    sat_altars_near.clear();
    for (const auto altar : *sat_altar_list)
        if (distance(position, altar) <= SAT_BANK_RANGE) sat_altars_near.push_back(altar);
    return sat_altars_near;
}

int Bot::sat_pick(const bc::champion& me, int main, int exclude) {
    // v159 (SATELLITE): the best solo group by value a round from here (travel() with the pair filter: sat_ok, zone,
    // SAT_APART from the main target), within SAT_RANGE of the main target, up on arrival, and safe for us alone.
    const int n = static_cast<int>(route.size());
    if (main < 0 || main >= n) return -1;
    zone_cap = split_zone();
    pair_filter = 2;
    main_slot = main;
    const auto& steps = travel(me.position(), exclude);
    const int now = static_cast<int>(bc::round_number());
    const double damage = std::max(1.0, me.strength() + SAT_FOCUS_SHARE * me.focus());
    const double health = me.health();
    const bool elk_built = SAT_ELK && emblem_count(me) >= SAT_ELK_EMBLEMS;
    const double camp_cut = has(owned_of(me).passives, bc::item_family::jungle) ? 0.7 : 1.0;
    int best = -1;
    double best_rate = 0;
    for (int u = 0; u < n; ++u) {
        if (u == exclude || u == main || u == capture_slot || !steps.has(u) || is_monolith(u)) continue;
        if (sat_elk[u] && !elk_built) continue;
        if (rewards.count(u) && u != target2) continue;
        if (distance(centers[u], centers[main]) > SAT_RANGE) continue;
        const int walk = steps.at(u);
        if (ready_at[u] > now + walk) continue;
        const double rounds = sat_hp[u] / damage;
        if (sat_str[u] * camp_cut * rounds > (sat_elk[u] ? SAT_ELK_TAKE : SAT_TAKE) * health) continue;
        double rate = values[aggressive][u] / (walk + rounds + capture_rounds) * mf_bias(u);
        if (u == target2) rate *= SAT_KEEP;
        if (best < 0 || rate > best_rate) {
            best = u;
            best_rate = rate;
        }
    }
    // v159 (SAT_EDGE): not worth our share of the main target: rejoin.
    if (best >= 0 && best_rate < SAT_EDGE * route_rate_steps(main, distance(me.position(), centers[main])) / std::max(1, allies_alive())) {
        NOTE(format("satellite: s%d pays less than our share of s%d", best, main));
        return -1;
    }
    return best;
}

bool Bot::split_member(Champ kind) const {
    if (satellite_on()) return kind == *sat_kind();
    if (kind == Champ::cassi || kind == Champ::issac) return true;
    if (!SPLIT_SMALL) return false;
    const int team = roster_size();
    if (team <= 3) return kind == Champ::heart_of_ruin;
    if (team == 4)
        return kind == Champ::willow
            || (SPLIT_PAIR_4 == 0 ? kind == Champ::ryth || kind == Champ::prism : kind == Champ::heart_of_ruin);
    return false;
}

bool Bot::split_lead(Champ kind) const {
    // Issac's part: picks and writes the pair target. Willow (and Cassi) capture.
    if (satellite_on()) return split_member(kind);
    return kind == Champ::issac || (SPLIT_SMALL && split_member(kind) && kind != Champ::willow && kind != Champ::cassi);
}

bool Bot::split_equipped() const {
    // v150 (SPLIT_SMALL): every ally alive and owning SPLIT_MIN_EMBLEMS by our emblem record (items read in vision).
    if (!SPLIT_SMALL) return true;
    if (allies_alive() < roster_size()) return false;
    int counted = 0;
    for (const auto& [key, count] : emblems) {
        if (count < SPLIT_MIN_EMBLEMS) return false;
        ++counted;
    }
    return counted >= roster_size();
}

// Kills and enemy emblems

void Bot::update_enemies() {
    // Life reads for the whole roster in or out of sight; emblems only for enemies in sight, and only every
    // ENEMY_EMBLEM_EVERY rounds per enemy (they are never lost, so the most seen stands).
    const auto& roster = seen::roster(Team::enemy);
    const int now = static_cast<int>(bc::round_number());
    if (enemy_dead_since.size() != roster.size()) {
        enemy_dead_since.assign(roster.size(), -1);
        enemy_emblems.assign(roster.size(), 0);
        enemy_emblem_round.assign(roster.size(), -1000);
    }
    for (std::size_t i = 0; i < roster.size(); ++i) {
        bool living = true;
        try {
            living = roster[i].is_alive();
        } catch (const bc::error&) {
        }
        if (!living) {
            if (enemy_dead_since[i] < 0) {
                enemy_dead_since[i] = now;
                ++kills_seen;
            }
        } else {
            enemy_dead_since[i] = -1;
        }
    }
    // The dead see nothing: building the vision list would only cost fuel.
    if (!bc::self().is_alive()) return;
    const auto& visible = seen::champions(Team::enemy);
    if (visible.empty()) return;
    last_enemy_round = now;
    // v104 (INVADE): how many enemies stand near each enemy-side group, kept only while the aggressive route is on
    // (the only time it is read).
    if (invade_on() && (shared_byte(MODE_BYTE) & AGGRESSIVE_FLAG) && !route.empty()) {
        if (risk_seen.size() != route.size()) {
            risk_seen.assign(route.size(), 0);
            risk_round.assign(route.size(), -1000);
        }
        for (int u = 0; u < static_cast<int>(route.size()); ++u) {
            if (!enemy_side(u)) continue;
            int near = 0;
            for (const auto& enemy : visible)
                if (distance(enemy.position(), centers[u]) <= INVADE_RISK_RANGE) ++near;
            if (near == 0) continue;
            risk_seen[u] = risk_round[u] == now ? std::max(risk_seen[u], near) : near;
            risk_round[u] = now;
        }
    }
    for (const auto& enemy : visible)
        for (std::size_t i = 0; i < roster.size(); ++i) {
            if (roster[i] != enemy) continue;
            if (now - enemy_emblem_round[i] >= ENEMY_EMBLEM_EVERY) {
                enemy_emblems[i] = std::max(enemy_emblems[i], emblem_count(enemy));
                enemy_emblem_round[i] = now;
            }
            break;
        }
}

void Bot::update_invasion() {
    // v104 (INVADE): our deaths from roster life, like the enemy's; then the revert rule. Every input is global (life,
    // banked gems, the shared aggressive bit), so every champion reaches the same answer without shared memory.
    const auto& allies = seen::roster(Team::ally);
    const int now = static_cast<int>(bc::round_number());
    if (ally_dead_since.size() != allies.size()) ally_dead_since.assign(allies.size(), -1);
    for (std::size_t i = 0; i < allies.size(); ++i) {
        bool living = true;
        try {
            living = allies[i].is_alive();
        } catch (const bc::error&) {
        }
        if (!living) {
            if (ally_dead_since[i] < 0) {
                ally_dead_since[i] = now;
                ++deaths_seen;
            }
        } else {
            ally_dead_since[i] = -1;
        }
    }
    if (now < invade_off_until) return;
    const int ours = static_cast<int>(bc::global_submitted_gems(Team::ally));
    const int theirs = static_cast<int>(bc::global_submitted_gems(Team::enemy));
    if (invade_base_round < 0) {
        if (!(shared_byte(MODE_BYTE) & AGGRESSIVE_FLAG)) return;
        invade_base_round = now;
        base_kills = kills_seen;
        base_deaths = deaths_seen;
        base_ours = ours;
        base_theirs = theirs;
        return;
    }
    const int trade = (deaths_seen - base_deaths) - (kills_seen - base_kills);
    const int gap = (theirs - base_theirs) - (ours - base_ours);
    if (trade < REVERT_TRADE && gap < REVERT_GEMS) return;
    invade_off_until = now + REVERT_HOLD;
    invade_base_round = -1;
    NOTE(format("invasion off for %d rounds: since it began, deaths - kills %d, their banked lead %d", REVERT_HOLD, trade, gap));
}

double Bot::invade_rate(int u, int steps, int now, bool ahead) const {
    // Gems a round as for our own groups (travel, respawn wait, clear, capture), plus what taking it denies them, less
    // the risk of the enemies seen near it lately (fading over INVADE_RISK_MEMORY rounds).
    double rate = route_rate_steps(u, steps, now) * (1 + INVADE_DENY);
    if (u < static_cast<int>(risk_seen.size()) && now - risk_round[u] < INVADE_RISK_MEMORY) {
        const double fade = 1.0 - static_cast<double>(now - risk_round[u]) / INVADE_RISK_MEMORY;
        rate /= 1 + INVADE_RISK * risk_seen[u] * fade;
    }
    // v104 (INVADE_AMBUSH): ahead in bodies, a group coming back about when we arrive is where their farmers will be.
    if (INVADE_AMBUSH && ahead && ready_at[u] > now && ready_at[u] - now <= steps + INVADE_AMBUSH_SLACK)
        rate *= 1 + INVADE_AMBUSH_BONUS;
    return rate;
}

std::optional<int> Bot::invade_target(const bc::champion& me, int s, int normal, int capture, std::string& why) {
    // v104 (INVADE): an enemy-side group whose priced rate is at least the best of ours (the normal pick). A group
    // being cleared is finished first, and the current invasion target keeps a margin so the team does not flip.
    if (mid_clear(normal) && !enemy_side(normal)) return std::nullopt;
    if (enemy_side(s) && mid_clear(s)) {
        LOG(why = format("invasion: finish s%d", s););
        return s;
    }
    const int now = static_cast<int>(bc::round_number());
    const auto position = me.position();
    const int saved_cap = zone_cap;
    zone_cap = ZONE_UNSAFE;
    const auto& steps = travel(position, capture);
    zone_cap = saved_cap;
    if (!steps.has(normal)) return std::nullopt;
    double own = route_rate_steps(normal, steps.at(normal), now);
    // v137 (INVADE_OWN_KEEP): our current own-side target counts too (a loot pick seen from some tiles only flipped it).
    if (INVADE_OWN_KEEP && s != normal && !enemy_side(s) && steps.has(s)) own = std::max(own, route_rate_steps(s, steps.at(s), now));
    const bool ahead = allies_alive() > enemies_alive();
    std::optional<int> best;
    double best_rate = 0;
    for (int u = 0; u < static_cast<int>(steps.steps.size()); ++u) {
        if (!steps.has(u) || !enemy_side(u) || is_monolith(u) || gemless[u]) continue;
        // Still down when we would get there: nothing to take or deny yet (unless it is an ambush, below).
        const int late = ready_at[u] - now - static_cast<int>(steps.at(u));
        const bool ambush = INVADE_AMBUSH && ahead && late <= INVADE_AMBUSH_SLACK;
        if (late > INVADE_MAX_WAIT && !ambush) continue;
        double rate = invade_rate(u, steps.at(u), now, ahead);
        if (u == s) rate *= INVADE_KEEP;
        if (!best || rate > best_rate) {
            best = u;
            best_rate = rate;
        }
    }
    if (!best || best_rate < own * INVADE_MARGIN) return std::nullopt;
    LOG(why = format("invasion: s%d pays %.2f a round priced, ours s%d %.2f", *best, best_rate, normal, own););
    return best;
}

int Bot::enemies_alive() const {
    int n = 0;
    for (const int since : enemy_dead_since)
        if (since < 0) ++n;
    return n;
}

int Bot::allies_alive() const {
    int n = 0;
    for (const auto& ally : seen::roster(Team::ally)) {
        try {
            if (ally.is_alive()) ++n;
        } catch (const bc::error&) {
        }
    }
    return n;
}

int Bot::first_respawn() const {
    int first = -1;
    for (const int since : enemy_dead_since)
        if (since >= 0) {
            const int back = since + static_cast<int>(bc::constants::respawn_time);
            if (first < 0 || back < first) first = back;
        }
    return first;
}

bool Bot::recent_kill() const {
    const int now = static_cast<int>(bc::round_number());
    for (const int since : enemy_dead_since)
        if (since >= 0 && now - since <= RAID_RECENT) return true;
    return false;
}

// Who is with us

bool Bot::ally_near(const bc::champion& me) {
    const auto position = me.position();
    for (const auto& a : seen::champions(Team::ally))
        if (a != me && !channelling(a) && distance(position, a.position()) <= TEAM_RANGE) return true;
    return false;
}

int Bot::together(const bc::champion& me, bool healthy) {
    // Visible allies are judged on what we see now; one last seen beside a bush within ALLY_GRACE rounds (and still
    // alive) may only be hidden in it, so it counts too, unless health matters (out of sight it is unknown).
    // One raw read per roster ally (position, health and type at once; out of sight it reports no tile).
    const double share = healthy ? RAID_HEALTH : RECALL_HEALTH;
    if (!me.is_alive()) return 0;
    const auto position = me.position();
    const int now = static_cast<int>(bc::round_number());
    const auto& allies = seen::roster(Team::ally);
    const auto& bushes = bc::map::bushes();
    int count = 0;
    for (std::size_t i = 0; i < allies.size(); ++i) {
        const auto& a = allies[i];
        bc::detail::champion_info info{};
        const auto handle = static_cast<std::uint32_t>(static_cast<bc::handle_champion_t>(a));
        const bool visible = bc::detail::sdk_champion(handle, &info) == 0 && info.x >= 0 && info.y >= 0;
        if (visible) {
            if (info.health <= 0 || distance(position, vec2{info.x, info.y}) > TEAM_RANGE) continue;
            if (info.health < info.max_health * share || channelling(a)) continue;
            ++count;
            continue;
        }
        if (healthy || i >= ally_keys.size() || !ally_keys[i]) continue;
        const int ident = *ally_keys[i] + 1;
        if (ident < 0 || ident >= 16 || !ally_seen[ident]) continue;
        const auto [round, tile] = *ally_seen[ident];
        if (now - round > ALLY_GRACE || distance(position, tile) > TEAM_RANGE) continue;
        if ((bushes & tiles_around(tile, 1)).is_empty()) continue;
        std::int32_t living = 0;
        if (bc::detail::sdk_champion_is_alive(handle, &living) == 0 && living) ++count;
    }
    return count;
}

bool Bot::emblem_edge() {
    // CENTRE_EMBLEM_EDGE more emblems per champion than them. Once every enemy has been seen, from the whole rosters
    // (ours from the emblem record, theirs from sightings); before that, from the champions in our sight.
    const auto& foes = seen::roster(Team::enemy);
    const auto& friends = seen::roster(Team::ally);
    if (foes.empty() || friends.empty() || enemy_emblems.size() != foes.size()) return false;
    bool all_seen = true;
    int theirs = 0;
    for (std::size_t i = 0; i < foes.size(); ++i) {
        if (enemy_emblem_round[i] <= -1000) all_seen = false;
        theirs += enemy_emblems[i];
    }
    if (all_seen)
        return static_cast<double>(team_emblems()) / friends.size() - static_cast<double>(theirs) / foes.size()
            >= CENTRE_EMBLEM_EDGE;
    const auto& visible = seen::champions(Team::enemy);
    if (visible.empty()) return false;
    int their_sight = 0;
    for (const auto& enemy : visible)
        for (std::size_t i = 0; i < foes.size(); ++i)
            if (foes[i] == enemy) {
                their_sight += enemy_emblems[i];
                break;
            }
    const auto& me = bc::self();
    int ours = 0, bodies = 0;
    bool me_listed = false;
    for (const auto& a : seen::champions(Team::ally)) {
        if (a == me) me_listed = true;
        const int ident = ident_of(a);
        if (ident < 1) continue;
        const auto it = emblems.find(ident - 1);
        ours += it == emblems.end() ? 0 : it->second;
        ++bodies;
    }
    if (!me_listed) {
        ours += emblem_count(me);
        ++bodies;
    }
    return static_cast<double>(ours) / bodies - static_cast<double>(their_sight) / visible.size() >= CENTRE_EMBLEM_EDGE;
}

bool Bot::centre_ok(const bc::champion& me, int shared_phase) {
    // v86b: the leader's own sight undercounts (it walks ahead; allies stand in bushes or out of its view), which shut
    // the centre on a team that was fine (20-14 vs 22-12 without the gate). Life reads every champion: the centre
    // stays open unless we are CENTRE_DOWN or more champions down; then CENTRE_DOWN_BODIES together or the emblem edge.
    const int ours = allies_alive(), theirs = enemies_alive();
    if (ours >= CENTRE_MIN_BODIES && ours > theirs - CENTRE_DOWN) return true;
    const int need = shared_phase == PHASE_CENTRE ? CENTRE_KEEP_BODIES : CENTRE_DOWN_BODIES;
    if (ours >= CENTRE_MIN_BODIES && together(me, false) >= need) return true;
    return emblem_edge();
}

// Phases

std::optional<int> Bot::raid_target(const bc::champion& me, int shared_phase, int s, int capture, std::string& why) {
    // Right after a kill, with more of us together (all healthy) than enemies alive and nothing safe or uncertain up
    // soon: one unsafe group that we reach, clear and capture before the first dead enemy is back.
    const int respawn = first_respawn();
    if (respawn < 0) return std::nullopt;
    const bool going = shared_phase == PHASE_RAID && zone(s) == ZONE_UNSAFE;
    if (!going && !recent_kill()) return std::nullopt;
    const int healthy = together(me, true);
    // v87: the man advantage is counted from life reads (more of us alive than them), with RAID_MIN_BODIES healthy ones
    // together; "together > enemies alive" almost never held (1 raid in 34 games) and left their side to them.
    const bool edge = RAID_BY_ALIVE ? allies_alive() > enemies_alive() && healthy >= RAID_MIN_BODIES : healthy > enemies_alive();
    if (!edge || together(me, false) > healthy) return std::nullopt;
    const int now = static_cast<int>(bc::round_number());
    const int left = respawn - now - RAID_MARGIN;
    const double scale = static_cast<double>(std::max<std::size_t>(1, seen::roster(Team::ally).size())) / healthy;
    const auto position = me.position();
    const double capture_time = capture_rounds;
    if (going) {
        // Captured (respawning) or out of time: the raid is over.
        if (ready_at[s] > now) return std::nullopt;
        double need = capture_time;
        if (!(camps_at(s).empty() && distance(position, centers[s]) <= 2)) need += distance(position, centers[s]) + clears[s] * scale;
        if (need > left) return std::nullopt;
        LOG(why = format("raid goes on: %d of us v %d alive, %d rounds before a respawn", healthy, enemies_alive(), left););
        return s;
    }
    zone_cap = ZONE_UNCERTAIN;
    pair_filter = 0;
    capture_near = -1;
    const int normal = pick_target(s, position, capture);
    // v87 (RAID_BY_RATE): with the man advantage, an unsafe group that pays better per round than the normal target is
    // taken even while something safe or uncertain is up (their side left alone, they farmed 906 a game against v86).
    const bool idle = wait_at(normal, position) > RAID_IDLE;
    if (mid_clear(normal) || (!RAID_BY_RATE && !idle)) return std::nullopt;
    zone_cap = ZONE_UNSAFE;
    const auto& steps = travel(position, capture);
    std::optional<int> best;
    double best_rate = 0;
    for (int u = 0; u < static_cast<int>(steps.steps.size()); ++u) {
        if (!steps.has(u) || zone(u) != ZONE_UNSAFE || is_monolith(u)) continue;
        if (ready_at[u] > now + steps.at(u)) continue;
        if (steps.at(u) + clears[u] * scale + capture_time > left) continue;
        const double rate = route_rate_steps(u, steps.at(u));
        if (!best || rate > best_rate) {
            best = u;
            best_rate = rate;
        }
    }
    if (best && RAID_BY_RATE && !idle && best_rate <= route_rate_steps(normal, distance(position, centers[normal])))
        return std::nullopt;
    if (best)
        LOG(why = format("after a kill, %d of us v %d alive: raid s%d before a respawn in %d", healthy, enemies_alive(), *best,
                         respawn - now););
    return best;
}

std::optional<int> Bot::split_candidate(int target, vec2 origin) {
    // A safe gem group the pair can clear, up by the time it could get there, SPLIT_APART from the main target.
    const int now = static_cast<int>(bc::round_number());
    // v159 (SATELLITE): the best satellite group, kept only if it pays SAT_EDGE x the satellite's share of the target.
    std::optional<int> sat_best;
    double sat_best_rate = 0;
    bool sat_elk_built = false;
    if (SAT_ELK && satellite_on()) {
        const auto e = emblems.find(champ_id(*sat_kind()));
        sat_elk_built = e != emblems.end() && e->second >= SAT_ELK_EMBLEMS;
    }
    for (int u = 0; u < static_cast<int>(route.size()); ++u) {
        if (u == target || zone(u) > split_zone() || is_monolith(u)) continue;
        // v159 (SATELLITE): clusters (gold ones too) and solo-safe wolves, SAT_APART to SAT_RANGE from the main target.
        if (satellite_on()) {
            // A cleared group whose reward waits is someone's capture, not a satellite group.
            if (!sat_ok[u] || u == capture_slot || rewards.count(u)) continue;
            if (sat_elk[u] && !sat_elk_built) continue;
            const int d = distance(centers[u], centers[target]);
            const int walk = distance(origin, centers[u]);
            if (d < SAT_APART || d > SAT_RANGE || walk > SAT_RANGE + SAT_APART) continue;
            if (ready_at[u] > now + walk) continue;
            const double rate = values[aggressive][u] / (walk + static_cast<double>(sat_hp[u]) / SAT_STATIC_DAMAGE + capture_rounds) * mf_bias(u);
            if (!sat_best || rate > sat_best_rate) {
                sat_best = u;
                sat_best_rate = rate;
            }
            continue;
        }
        if (!pair_ok[u] || gemless[u]) continue;
        const int apart = split_apart();
        if (distance(centers[u], centers[target]) < apart || distance(centers[u], centers[target]) > apart + SPLIT_MAX_WALK
            || distance(origin, centers[u]) > apart + SPLIT_MAX_WALK) continue;
        if (ready_at[u] > now + distance(origin, centers[u])) continue;
        return u;
    }
    if (satellite_on() && sat_best
        && sat_best_rate >= SAT_EDGE * route_rate_steps(target, distance(origin, centers[target])) / std::max(1, allies_alive()))
        return sat_best;
    return std::nullopt;
}

int Bot::choose_phase(const bc::champion& me, int s, int shared_phase, int& target, int capture, std::string& why) {
    const auto position = me.position();
    const int now = static_cast<int>(bc::round_number());
    // 1. A raid outranks everything: it only lasts until the dead enemy is back.
    // v177 (CLOSE_OUT): no raid while we lead late.
    const bool closing = close_out();
    if (V86_RAID && !closing && !OUR_SIDE_ONLY)
        if (const auto r = raid_target(me, shared_phase, s, capture, why)) {
            target = *r;
            return PHASE_RAID;
        }
    // While a reward outside the safe zone is captured, the team stays within CAPTURE_NEAR of it.
    capture_near = V86_CAPTURE_NEAR && capture != NO_CAPTURE && capture < static_cast<int>(route.size())
            && zone(capture) != ZONE_SAFE ? capture : -1;
    // 2. A split goes on while the main team's own safe groups keep it busy.
    // v99: types never change, so whether the pair was drafted is read once (a type read per ally every call).
    if (!pair_drafted_cache) {
        pair_drafted_cache = false;
        for (const auto& ally : seen::roster(Team::ally)) {
            try {
                const auto kind = ally.champion_name();
                if (split_member(kind)) pair_drafted_cache = true;
            } catch (const bc::error&) {
            }
        }
    }
    bool pair_alive = true;
    const bool pair_drafted = *pair_drafted_cache;
    if (pair_drafted)
        for (const auto& ally : seen::roster(Team::ally)) {
            try {
                const auto kind = ally.champion_name();
                if (split_member(kind) && !ally.is_alive()) pair_alive = false;
            } catch (const bc::error&) {
            }
        }
    // v159 (SATELLITE): the leader's gates; the split itself is decided from the normal pick below (one pick a turn).
    // The satellite often leads (it acts first): with the team then, it may open its own split.
    const bool sat_gates = satellite_on() && V86_SPLIT && pair_drafted && pair_alive && !win_tribute && mode == FARM
        && now - last_enemy_round >= SAT_QUIET && !ping_slot && now - split_end_round >= SAT_COOLDOWN && sat_ready();
    // v158: the Prism team never splits.
    const bool split_ok = !satellite_on() && V86_SPLIT && !prism_team() && pair_drafted && pair_alive && !in_pair() && !win_tribute
        && now - last_enemy_round >= (SPLIT_SMALL ? SPLIT_QUIET_SMALL : SPLIT_QUIET) && !ping_slot
        && now - split_end_round >= SPLIT_COOLDOWN && split_equipped();
    if (split_ok && shared_phase == PHASE_SPLIT) {
        zone_cap = split_zone();
        pair_filter = 1;
        const int t = pick_target(s, position, capture);
        if (zone(t) <= split_zone() && (mid_clear(t) || wait_at(t, position) <= RAID_IDLE)) {
            target = t;
            capture_near = -1;
            LOG(why = "the pair keeps its own group";);
            return PHASE_SPLIT;
        }
        LOG(why = "split over: the main team's safe groups are down; ";);
    }
    // 3. Centre (safe and uncertain) with enough of us together or the advantage, else the safe zone only.
    // v159 (SATELLITE): a satellite split counts as the centre for its hysteresis.
    const bool centre = !V86_CENTRE || centre_ok(me, satellite_on() && shared_phase == PHASE_SPLIT ? PHASE_CENTRE : shared_phase);
    const int ph = centre ? PHASE_CENTRE : PHASE_GROUP;
    zone_cap = zone_cap_for(ph);
    pair_filter = 0;
    target = pick_target(s, position, capture);
    // v135 (CAPTURE_STAGE): no group near the reward left the table empty and handed back the reward itself; the next
    // group is picked from the whole table instead, and farm() keeps the team at the reward's edge meanwhile.
    if (CAPTURE_STAGE && CAPTURE_REPICK && capture != NO_CAPTURE && target == capture && capture_near >= 0) {
        capture_near = -1;
        const int next = pick_target(s == capture ? capture : s, position, capture);
        if (next != capture) {
            LOG(why += (why.empty() ? "" : "; ") + format("reward s%d is captured: the next group s%d, waiting at its edge", capture, next););
            target = next;
        }
    }
    capture_near = -1;
    // v159 (SATELLITE): with the centre open, a split goes on, or opens for a solo group worth the satellite's share of
    // the main target. The main team never takes the satellite's group (re-picked in the rare case it was chosen).
    if (sat_gates && centre) {
        const bool keep = shared_phase == PHASE_SPLIT;
        if (keep && target == target2) {
            pair_filter = 1;
            target = pick_target(s, position, capture);
        }
        LOG(if (!keep && now % 20 == 0) note(format("satellite: gates open, candidate %d", split_candidate(target, position) ? 1 : 0)););
        if (keep || split_candidate(target, position)) {
            LOG(why += (why.empty() ? "" : "; ") + std::string(keep ? "the satellite keeps its own group" : "quiet, and a solo group is up for the satellite"););
            return PHASE_SPLIT;
        }
    }
    // v104 (INVADE): aggressive, the centre open, and invading not reverted: a priced enemy-side group.
    if (invade_on() && aggressive && centre && !win_tribute && now >= invade_off_until
        && !(((PRISM_OUR_SIDE && prism_team()) || MULTIFARM) && zone(target) == ZONE_SAFE))
        if (const auto u = invade_target(me, s, target, capture, why)) {
            target = *u;
            return PHASE_RAID;
        }
    // v150 (SPLIT_SMALL, debug only): which split gate holds, every 20 rounds.
    LOG(if (SPLIT_SMALL && pair_drafted && now % 20 == 0 && shared_phase != PHASE_SPLIT)
            note(format("split gates: alive %d quiet %d ping %d cooldown %d equipped %d target safe %d candidate %d",
                        pair_alive ? 1 : 0, now - last_enemy_round >= SPLIT_QUIET_SMALL ? 1 : 0, ping_slot ? 0 : 1,
                        now - split_end_round >= SPLIT_COOLDOWN ? 1 : 0, split_equipped() ? 1 : 0,
                        zone(target) <= split_zone() ? 1 : 0, split_candidate(target, position) ? 1 : 0)););
    // 4. A new split: quiet, the main target safe, and a far safe group the pair can clear is up.
    if (split_ok && shared_phase != PHASE_SPLIT && zone(target) <= split_zone() && split_candidate(target, position)) {
        LOG(why += "quiet, and a far safe group is up for the pair";);
        return PHASE_SPLIT;
    }
    if (ph != shared_phase) LOG(why += centre ? "centre open: enough of us alive, together or ahead" : "too few of us together: safe zone only";);
    return ph;
}

// The split pair

int Bot::pick_pair_target(const bc::champion& me, int main, int exclude) {
    const int n = static_cast<int>(route.size());
    if (main < 0 || main >= n) return -1;
    const auto position = me.position();
    auto eligible = [&](int u) {
        return u >= 0 && u < n && u != exclude && pair_ok[u] && zone(u) <= split_zone() && !is_monolith(u)
            && distance(centers[u], centers[main]) >= split_apart() && distance(centers[u], centers[main]) <= split_apart() + SPLIT_MAX_WALK;
    };
    int start = eligible(target2) ? target2 : -1;
    if (start < 0)
        for (int u = 0; u < n; ++u)
            if (eligible(u) && (start < 0 || distance(position, centers[u]) < distance(position, centers[start]))) start = u;
    if (start < 0) return -1;
    zone_cap = split_zone();
    pair_filter = 2;
    main_slot = main;
    const int t = pick_target(start, position, exclude);
    return eligible(t) ? t : start;
}

void Bot::end_split(const char* why) {
    write_phase(PHASE_GROUP);
    phase = PHASE_GROUP;
    split_end_round = static_cast<int>(bc::round_number());
    zone_cap = zone_cap_for(PHASE_GROUP);
    pair_filter = 0;
    pair_capture = -1;
    partner_gone = 0;
    // Far from the team: it may not lead until it is back with it.
    rejoining = true;
    NOTE(std::string("split ends: ") + why);
}

void Bot::play_pair(const bc::champion& me) {
    // Issac picks the pair's group and opens the next one while Cassi captures; Cassi captures what they clear.
    const int now = static_cast<int>(bc::round_number());
    const int n = static_cast<int>(route.size());
    if (n == 0) return;
    if (!near_enemies().empty()) {
        end_split("enemy in sight");
        return;
    }
    if (escort_bag(static_cast<int>(me.gem_count()), owned_of(me).passives)) {
        end_split("our bag needs an escort");
        return;
    }
    // v159 (SATELLITE): the team's fight, our health or a full bag brings the satellite back.
    if (satellite_on()) {
        if (mode == FIGHT) {
            end_split("the team fights: the satellite rejoins");
            return;
        }
        // Stage 2: a started clear of our group is finished down to SAT_FLOOR.
        const bool finishing = SAT_ELK && target2 < static_cast<int>(route.size()) && mid_clear(target2);
        if (me.health() < me.max_health() * (finishing ? SAT_FLOOR : SAT_HEALTH)) {
            end_split("the satellite is hurt");
            return;
        }
        // Stage 3 (SAT_BANK_OWN): it banks the bag itself (the turn's sat.bank step) when an altar is in range.
        if (static_cast<int>(me.gem_count()) >= SAT_BAG && !(SAT_BANK_OWN && !sat_bank_altars(me.position()).empty())) {
            end_split("the satellite's bag is full");
            return;
        }
    }
    // v150 (SPLIT_SMALL): the lead does Issac's part, the other member Cassi's; a lone member does both.
    const auto kind = me.champion_name();
    const bool issac = split_lead(kind);
    const bool solo = split_solo();
    if (SPLIT_SMALL && emblem_count(me) < SPLIT_MIN_EMBLEMS) {
        end_split("our build is short of the split bar");
        return;
    }
    std::optional<vec2> partner;
    for (const auto& a : seen::champions(Team::ally))
        if (a != me && !channelling(a) && split_member(a.champion_name())) partner = a.position();
    partner_gone = partner || solo ? 0 : partner_gone + 1;
    if (partner_gone > SPLIT_PARTNER_LOST) {
        end_split("partner out of sight");
        return;
    }
    const int main = main_slot;
    int t = target2 < n ? target2 : -1;
    if (!issac || solo) {
        // Cassi holds a cleared pair group until its reward lands (or the claim would have timed out).
        if (pair_capture >= 0
            && (ready_at[pair_capture] > now || now - pair_capture_from > CAPTURE_TIMEOUT || !camps_at(pair_capture).empty()))
            pair_capture = -1;
        if (pair_capture < 0 && t >= 0 && rewards.count(t)) {
            pair_capture = t;
            pair_capture_from = now;
        }
        if (pair_capture >= 0) {
            slot = pair_capture;
            NOTE(format("pair: %s holds s%d for its reward", solo ? "the lone member" : "the capturer", pair_capture));
            return;
        }
    }
    const bool down = t >= 0 && ready_at[t] > now && camps_at(t).empty();
    // Cassi standing on a cleared group's tile is capturing it: Issac moves on, but only to a group within
    // CAPTURE_NEAR of it, so she stays in his sight.
    int held = -1;
    if (issac && partner) {
        const int on = slot_at(*partner);
        if (on >= 0 && camps_at(on).empty() && ready_at[on] <= now) held = on;
    }
    const bool cassi_on = held >= 0;
    if (cassi_on && t != held && t >= 0 && distance(centers[t], centers[held]) > CAPTURE_NEAR) t = -1;
    if (t < 0 || (issac && (down || cassi_on || now - pair_pick_round >= SPLIT_REPICK))) {
        capture_near = held;
        const int pick = satellite_on() ? sat_pick(me, main, cassi_on ? held : NO_CAPTURE)
                                        : pick_pair_target(me, main, cassi_on ? held : NO_CAPTURE);
        capture_near = -1;
        pair_pick_round = now;
        if (cassi_on && (pick < 0 || distance(centers[pick], centers[held]) > CAPTURE_NEAR)) {
            // Nothing near: wait beside her (and share the reward).
            t = held;
            if (target2 != held) write_target2(held);
            target2 = held;
        } else if (pick >= 0) {
            if (pick != t) {
                write_target2(pick);
                NOTE(format("pair: next group s%d", pick));
            }
            t = pick;
            target2 = pick;
        } else if (!cassi_on && pick < 0) {
            end_split("no safe group for the pair");
            return;
        }
    }
    if (t < 0) {
        end_split("no pair target");
        return;
    }
    slot = t;
}

// Per-champion steps

bool Bot::finish_ok(const bc::champion& me, std::string& why) {
    // Under recall health, keep hitting a started camp of our target while finishing it leaves FINISH_HEALTH.
    if (route.empty() || static_cast<int>(me.gem_count()) >= CARRY_GEMS) return false;
    const double top = me.max_health();
    const double health = me.health();
    if (health < top * FINISH_HEALTH) return false;
    if (!seen::champions(Team::enemy).empty() || hidden_active()) return false;
    const int s = slot % static_cast<int>(route.size());
    const auto camps = camps_at(s);
    if (camps.empty()) return false;
    double left = 0, strength = 0;
    bool started = false;
    const auto position = me.position();
    int my_gap = 1000;
    for (const auto& c : camps) {
        left += c.health();
        strength += c.strength();
        if (c.health() < c.max_health()) started = true;
        my_gap = std::min(my_gap, distance(position, c.position()));
    }
    if (!started) return false;
    // v148-normal (FINISH_ORDER): only the camps that hit us, each until it dies in kill order.
    if (FINISH_ORDER) {
        double rounds = 0;
        bool tanked = false;
        const double take = finish_take(me, s, rounds, &tanked);
        if (rounds <= 0 || health - take < top * FINISH_HEALTH) return false;
        LOG(why = format("finishes s%d before recalling: no enemy, ~%.0f rounds left, takes ~%.0f, stays over %.0f%%%s", s,
                         rounds, take, FINISH_HEALTH * 100, tanked ? ", an ally tanks some" : ""););
        return true;
    }
    // v137 (FINISH_TANK): the gap we will have while hitting, so stepping in does not turn the answer around.
    if (FINISH_TANK) my_gap = std::min(my_gap, static_cast<int>(me.attack_range()));
    // Our damage and that of allies near the group; the camps hit the nearest champion, so a closer ally tanks.
    // v134 (ITEM_DAMAGE): Monster and Collectors add to camp damage; Jungle and Durability cut what the camps hit us for.
    const auto mine = owned_of(me).passives;
    double damage = me.strength() * camp_dmg_mult(mine);
    bool tanked = false;
    for (const auto& a : seen::champions(Team::ally)) {
        if (a == me || channelling(a)) continue;
        int gap = 1000;
        for (const auto& c : camps) gap = std::min(gap, distance(a.position(), c.position()));
        if (gap <= a.attack_range() + 1) damage += a.strength() * camp_dmg_mult(owned_of(a).passives);
        // v94: reach is per camp (elks and golems 4, the monolith 5).
        bool in_reach = false;
        for (const auto& c : camps)
            if (c.strength() > 0 && distance(a.position(), c.position()) <= c.attack_range()) in_reach = true;
        if (gap < my_gap && in_reach) tanked = true;
    }
    if (damage <= 0) return false;
    const double rounds = std::ceil(left / damage);
    const double take = tanked ? 0 : rounds * strength * camp_taken_mult(mine);
    if (health - take < top * FINISH_HEALTH) return false;
    LOG(why = format("finishes s%d before recalling: no enemy, ~%.0f rounds left, stays over %.0f%%%s", s, rounds,
                     FINISH_HEALTH * 100, tanked ? ", an ally tanks" : ""););
    return true;
}

double Bot::finish_take(const bc::champion& me, int s, double& rounds, bool* tanked) {
    // v148-normal (FINISH_ORDER): the team hits the weakest camp first; each camp hits us (no ally strictly nearer, or
    // level and acting before us: CAMP_TIE_ORDER) only until it dies. Asleep camps count: our first hit wakes them.
    rounds = 0;
    const auto camps = camps_at(s);
    if (camps.empty()) return 0;
    const auto position = me.position();
    const auto mine = owned_of(me).passives;
    double damage = me.strength() * camp_dmg_mult(mine);
    const auto& allies = seen::champions(Team::ally);
    const auto& tiles = seen::positions(Team::ally);
    struct Left {
        double health, strength;
        bool hits;
    };
    std::array<Left, 16> left{};
    std::size_t n = 0;
    std::array<int, 16> ally_gap{};
    const std::size_t a_n = std::min({allies.size(), tiles.size(), ally_gap.size()});
    for (std::size_t i = 0; i < a_n; ++i) ally_gap[i] = 1 << 10;
    for (const auto& c : camps) {
        if (n >= left.size()) break;
        const auto spot = c.position();
        const int reach = camp_kind_reach(c.kind());
        // The gap we will have while hitting (FINISH_TANK).
        const int gap = std::min(distance(position, spot), static_cast<int>(me.attack_range()));
        bool hits = c.strength() > 0 && gap <= reach;
        for (std::size_t i = 0; i < a_n; ++i) {
            if (allies[i] == me) continue;
            const int g = distance(tiles[i], spot);
            ally_gap[i] = std::min(ally_gap[i], g);
            if (hits && (g < gap || (g == gap && acts_before_me(allies[i]))) && !channelling(allies[i])) {
                hits = false;
                if (tanked) *tanked = true;
            }
        }
        left[n++] = {c.health(), c.strength(), hits};
    }
    for (std::size_t i = 0; i < a_n; ++i)
        if (allies[i] != me && !channelling(allies[i]) && ally_gap[i] <= allies[i].attack_range() + 1)
            damage += allies[i].strength() * camp_dmg_mult(owned_of(allies[i]).passives);
    if (damage <= 0) return 0;
    std::sort(left.begin(), left.begin() + n, [](const Left& a, const Left& b) { return a.health < b.health; });
    double spent = 0, take = 0;
    for (std::size_t k = 0; k < n; ++k) {
        spent += left[k].health;
        if (left[k].hits) take += left[k].strength * std::ceil(spent / damage);
    }
    rounds = std::ceil(spent / damage);
    return take * camp_taken_mult(mine);
}

bool Bot::band_step(const bc::champion& me, int now_mode) {
    // Between RECALL_HEALTH and BAND_HEALTH, off a fight: bank a bag at a near altar, farm on with an ally in sight,
    // and alone outside the safe zone go to a safe group nearby, else recall.
    if (now_mode == FIGHT || now_mode == RETREAT || is_collector(me) || win_tribute || route.empty()) return false;
    const double top = me.max_health();
    const double health = me.health();
    if (health >= top * BAND_HEALTH || health < top * RECALL_HEALTH) return false;
    const auto position = me.position();
    const int gems = static_cast<int>(me.gem_count());
    if (gems >= NEAR_BANK_GEMS && !prism_holds(me) && seen::champions(Team::enemy).empty() && !under_threat(me)) {
        const auto& spots = escort_bag(gems, owned_of(me).passives) ? solo_altars : seen::altar_list();
        std::vector<vec2> close;
        for (const auto altar : spots)
            if (altar_near(position, altar, BAND_BANK_RANGE)) close.push_back(altar);
        if (!close.empty() && bank_gems(me, close, "band.bank")) {
            LOG(branch_why = format("under %.0f%% with a bag: bank near; ", BAND_HEALTH * 100) + branch_why;);
            return true;
        }
    }
    if (ally_near(me) || zone(slot % static_cast<int>(route.size())) == ZONE_SAFE) return false;
    return farm_alone(me);
}

bool Bot::solo_cheap(const bc::champion& me) {
    if (route.empty() || !zones_known || prism_team() || in_pair() || mode != FARM || capture_slot != NO_CAPTURE) return false;
    if (!seen::champions(Team::enemy).empty() || me.health() < me.max_health() * SAT_HEALTH) return false;
    const int n = static_cast<int>(route.size());
    const int main = slot % n;
    if (is_monolith(main)) return false;
    const auto position = me.position();
    const auto& allies = seen::champions(Team::ally);
    // A big main target keeps its crew; a cheap one needs nobody.
    if (!sat_ok[main]) {
        int crew = 0;
        for (const auto& a : allies)
            if (a != me && distance(a.position(), centers[main]) <= SOLO_CREW_RANGE) ++crew;
        if (crew < SOLO_CREW) return false;
    }
    const int now = static_cast<int>(bc::round_number());
    const double damage = std::max(1.0, me.strength() + SAT_FOCUS_SHARE * me.focus());
    const double health = me.health();
    const double camp_cut = has(owned_of(me).passives, bc::item_family::jungle) ? 0.7 : 1.0;
    int best = -1;
    double best_rate = 0;
    for (int u = 0; u < n; ++u) {
        if (u == main || !sat_ok[u] || sat_elk[u] || is_monolith(u) || rewards.count(u) || zone(u) > ZONE_UNCERTAIN) continue;
        const int d = distance(position, centers[u]);
        if (d > SOLO_RANGE || ready_at[u] > now + d) continue;
        const double rounds = sat_hp[u] / damage;
        if (sat_str[u] * camp_cut * rounds > SOLO_TAKE * health) continue;
        bool taken = false;
        for (const auto& a : allies)
            if (a != me && distance(a.position(), centers[u]) <= std::max(3, d - 1)) taken = true;
        if (taken || !allowed(u) || !walkable(u, position)) continue;
        double rate = route_rate_steps(u, d);
        if (u == solo_slot) rate *= SOLO_KEEP;
        if (best < 0 || rate > best_rate) {
            best = u;
            best_rate = rate;
        }
    }
    solo_slot = best;
    if (best < 0) return false;
    const int team_slot = slot;
    slot = best;
    farm(me, nullptr, "solo");
    slot = team_slot;
    return true;
}

bool Bot::farm_alone(const bc::champion& me) {
    const auto position = me.position();
    const int now = static_cast<int>(bc::round_number());
    int best = -1;
    double best_rate = 0;
    for (int s = 0; s < static_cast<int>(route.size()); ++s) {
        if (zone(s) != ZONE_SAFE || is_monolith(s)) continue;
        if (phase == PHASE_SPLIT && !pair_split() && s == target2) continue;
        const int d = distance(position, centers[s]);
        if (d > BAND_SAFE_RANGE || ready_at[s] > now + d) continue;
        const double rate = route_rate_steps(s, d);
        if (best < 0 || rate > best_rate) {
            best = s;
            best_rate = rate;
        }
    }
    if (best >= 0) {
        slot = best;
        farm(me, nullptr, "band.safe");
        LOG(branch_why = format("under %.0f%%, alone outside the safe zone: farm a safe group; ", BAND_HEALTH * 100) + branch_why;);
        return true;
    }
    if (channel_cut || recall_zone(seen::champions(Team::enemy), true).is_set(position)) return false;
    try {
        me.recall();
        trace::act("recall");
        WHY("band.recall", format("under %.0f%%, alone outside the safe zone, no safe group within %d", BAND_HEALTH * 100,
                                  BAND_SAFE_RANGE));
        return true;
    } catch (const bc::error& error) {
        NOTE("band recall refused: " + error_text(error));
        return false;
    }
}

bool Bot::mage_cheese(const bc::champion& me, const Target& camp, const char** why) {
    const char* unused;
    const char*& reason = why ? *why : unused;
    // Without zones, v82's rule: always from outside the camp's reach.
    reason = "no zones: always stand off";
    if (!zones_known) return true;
    // A monolith hits for 200: always from outside its reach.
    reason = "a monolith: always stand off";
    if (const auto* c = camp.camp(); c && c->is_monolith()) return true;
    // v91 (V91_MAGE_NO_TANK): a mage never tanks a camp. Without a sturdy ally in the camp's reach it stands off and
    // casts (mage_camp still basic attacks when the finish is cheap); before, it basic attacked from inside the reach
    // whenever no melee was in sight (the tank gone to recall).
    reason = "nobody tanks the camp: stand off and cast";
    if (V91_MAGE_NO_TANK && !camp_tanked(me, camp)) return true;
    const auto s = camp_slot(camp);
    if (!s || zone(*s) != ZONE_SAFE) {
        reason = !s ? "camp off the route: stand off" : mid_clear(*s) ? "started camp outside the safe zone: stand off"
                                                                      : "unstarted camp outside the safe zone: hit it with the team";
        return !s || mid_clear(*s);
    }
    const bool cheese = me.health() < me.max_health() * BAND_HEALTH && (pair_split() || !ally_near(me));
    reason = cheese ? "safe zone, hurt and alone: stand off" : "safe zone with the team or healthy: hit it like a ranged";
    return cheese;
}
