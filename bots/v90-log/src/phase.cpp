// v86: the leader's phase (group, centre, split, raid), the split pair, and the per-champion farming steps.
#include "bot.h"
#include "seen.h"
#include "state.h"
#include "trace.h"
#include <algorithm>
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
    if (!in_pair_cache) {
        const auto kind = bc::self().champion_name();
        in_pair_cache = kind == Champ::cassi || kind == Champ::issac;
    }
    return *in_pair_cache;
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
            if (enemy_dead_since[i] < 0) enemy_dead_since[i] = now;
        } else {
            enemy_dead_since[i] = -1;
        }
    }
    // The dead see nothing: building the vision list would only cost fuel.
    if (!bc::self().is_alive()) return;
    const auto& visible = seen::champions(Team::enemy);
    if (visible.empty()) return;
    last_enemy_round = now;
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
    const double capture_time = bc::constants::capture_time;
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
    const auto steps = travel(position, capture);
    std::optional<int> best;
    double best_rate = 0;
    for (int u = 0; u < static_cast<int>(steps.steps.size()); ++u) {
        if (!steps.has(u) || zone(u) != ZONE_UNSAFE || monoliths.count(u)) continue;
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
    for (int u = 0; u < static_cast<int>(route.size()); ++u) {
        if (u == target || zone(u) != ZONE_SAFE || !pair_ok[u] || gemless[u] || monoliths.count(u)) continue;
        if (distance(centers[u], centers[target]) < SPLIT_APART || distance(centers[u], centers[target]) > SPLIT_APART + SPLIT_MAX_WALK
            || distance(origin, centers[u]) > SPLIT_APART + SPLIT_MAX_WALK) continue;
        if (ready_at[u] > now + distance(origin, centers[u])) continue;
        return u;
    }
    return std::nullopt;
}

int Bot::choose_phase(const bc::champion& me, int s, int shared_phase, int& target, int capture, std::string& why) {
    const auto position = me.position();
    const int now = static_cast<int>(bc::round_number());
    // 1. A raid outranks everything: it only lasts until the dead enemy is back.
    if (V86_RAID)
        if (const auto r = raid_target(me, shared_phase, s, capture, why)) {
            target = *r;
            return PHASE_RAID;
        }
    // While a reward outside the safe zone is captured, the team stays within CAPTURE_NEAR of it.
    capture_near = V86_CAPTURE_NEAR && capture != NO_CAPTURE && capture < static_cast<int>(route.size())
            && zone(capture) != ZONE_SAFE ? capture : -1;
    // 2. A split goes on while the main team's own safe groups keep it busy.
    bool pair_alive = true;
    bool pair_drafted = false;
    for (const auto& ally : seen::roster(Team::ally)) {
        try {
            const auto kind = ally.champion_name();
            if (kind == Champ::cassi || kind == Champ::issac) {
                pair_drafted = true;
                if (!ally.is_alive()) pair_alive = false;
            }
        } catch (const bc::error&) {
        }
    }
    const bool split_ok = V86_SPLIT && pair_drafted && pair_alive && !in_pair() && !win_tribute
        && now - last_enemy_round >= SPLIT_QUIET && !ping_slot && now - split_end_round >= SPLIT_COOLDOWN;
    if (split_ok && shared_phase == PHASE_SPLIT) {
        zone_cap = ZONE_SAFE;
        pair_filter = 1;
        const int t = pick_target(s, position, capture);
        if (zone(t) == ZONE_SAFE && (mid_clear(t) || wait_at(t, position) <= RAID_IDLE)) {
            target = t;
            capture_near = -1;
            LOG(why = "the pair keeps its own group";);
            return PHASE_SPLIT;
        }
        LOG(why = "split over: the main team's safe groups are down; ";);
    }
    // 3. Centre (safe and uncertain) with enough of us together or the advantage, else the safe zone only.
    const bool centre = !V86_CENTRE || centre_ok(me, shared_phase);
    const int ph = centre ? PHASE_CENTRE : PHASE_GROUP;
    zone_cap = zone_cap_for(ph);
    pair_filter = 0;
    target = pick_target(s, position, capture);
    capture_near = -1;
    // 4. A new split: quiet, the main target safe, and a far safe group the pair can clear is up.
    if (split_ok && shared_phase != PHASE_SPLIT && zone(target) == ZONE_SAFE && split_candidate(target, position)) {
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
        return u >= 0 && u < n && u != exclude && pair_ok[u] && zone(u) == ZONE_SAFE && !monoliths.count(u)
            && distance(centers[u], centers[main]) >= SPLIT_APART && distance(centers[u], centers[main]) <= SPLIT_APART + SPLIT_MAX_WALK;
    };
    int start = eligible(target2) ? target2 : -1;
    if (start < 0)
        for (int u = 0; u < n; ++u)
            if (eligible(u) && (start < 0 || distance(position, centers[u]) < distance(position, centers[start]))) start = u;
    if (start < 0) return -1;
    zone_cap = ZONE_SAFE;
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
    if (static_cast<int>(me.gem_count()) >= ESCORT_GEMS) {
        end_split("our bag needs an escort");
        return;
    }
    const bool issac = me.champion_name() == Champ::issac;
    const Champ other = issac ? Champ::cassi : Champ::issac;
    std::optional<vec2> partner;
    for (const auto& a : seen::champions(Team::ally))
        if (a != me && !channelling(a) && a.champion_name() == other) partner = a.position();
    partner_gone = partner ? 0 : partner_gone + 1;
    if (partner_gone > SPLIT_PARTNER_LOST) {
        end_split("partner out of sight");
        return;
    }
    const int main = main_slot;
    int t = target2 < n ? target2 : -1;
    if (!issac) {
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
            NOTE(format("pair: cassi holds s%d for its reward", pair_capture));
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
        const int pick = pick_pair_target(me, main, cassi_on ? held : NO_CAPTURE);
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
    // Our damage and that of allies near the group; the camps hit the nearest champion, so a closer ally tanks.
    double damage = me.strength();
    bool tanked = false;
    for (const auto& a : seen::champions(Team::ally)) {
        if (a == me || channelling(a)) continue;
        int gap = 1000;
        for (const auto& c : camps) gap = std::min(gap, distance(a.position(), c.position()));
        if (gap <= a.attack_range() + 1) damage += a.strength();
        if (gap < my_gap && gap <= CAMP_REACH) tanked = true;
    }
    if (damage <= 0) return false;
    const double rounds = std::ceil(left / damage);
    const double take = tanked ? 0 : rounds * strength;
    if (health - take < top * FINISH_HEALTH) return false;
    LOG(why = format("finishes s%d before recalling: no enemy, ~%.0f rounds left, stays over %.0f%%%s", s, rounds,
                     FINISH_HEALTH * 100, tanked ? ", an ally tanks" : ""););
    return true;
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
    if (gems >= NEAR_BANK_GEMS && seen::champions(Team::enemy).empty() && !under_threat(me)) {
        const auto& spots = gems >= ESCORT_GEMS ? solo_altars : seen::altar_list();
        std::vector<vec2> close;
        for (const auto altar : spots)
            if (distance(position, altar) <= BAND_BANK_RANGE) close.push_back(altar);
        if (!close.empty() && bank_gems(me, close, "band.bank")) {
            LOG(branch_why = format("under %.0f%% with a bag: bank near; ", BAND_HEALTH * 100) + branch_why;);
            return true;
        }
    }
    if (ally_near(me) || zone(slot % static_cast<int>(route.size())) == ZONE_SAFE) return false;
    return farm_alone(me);
}

bool Bot::farm_alone(const bc::champion& me) {
    const auto position = me.position();
    const int now = static_cast<int>(bc::round_number());
    int best = -1;
    double best_rate = 0;
    for (int s = 0; s < static_cast<int>(route.size()); ++s) {
        if (zone(s) != ZONE_SAFE || monoliths.count(s)) continue;
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
