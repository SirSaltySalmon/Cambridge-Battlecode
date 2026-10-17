// Map setup, the shared route, and the fight-or-farm score.
#include "bot.h"
#include "draft.h"
#include "seen.h"
#include "state.h"
#include <algorithm>
#include <cstring>
#include <tuple>

void Bot::setup() {
    const auto& data = draft::group_data();
    const auto& groups = data.groups;
    const int group_count = static_cast<int>(groups.size());
    // One bulk copy: pushing the 1024 bytes one by one cost ~50k fuel.
    std::vector<std::uint8_t> memory(bc::draft_memory.size());
    if (!memory.empty()) std::memcpy(memory.data(), bc::draft_memory.data(), memory.size());
    std::size_t offset = 0;
    auto theirs = draft::decode_theirs(memory, offset, groups.size());
    std::vector<int> normal, aggressive_plan;
    if (!theirs.empty() || groups.empty()) {
        normal = draft::decode_pocket(memory, offset).members;
        aggressive_plan = draft::decode_pocket(memory, offset).members;
    }
    auto out_of_range = [&](const std::vector<int>& plan) {
        return std::any_of(plan.begin(), plan.end(), [&](int index) { return index >= group_count; });
    };
    if (normal.empty() || out_of_range(normal)) {
        normal = draft::compute_pocket().members;
        aggressive_plan = draft::compute_pocket(true).members;
    }
    if (theirs.size() != groups.size()) theirs = draft::theirs_groups();
    if (out_of_range(aggressive_plan)) aggressive_plan.clear();
    if (normal.empty())
        for (int index = 0; index < group_count; ++index) normal.push_back(index);

    // Every champion builds the same slot order, so shared slots agree; both plans share one slot space.
    // Flat arrays by group index: set and map nodes are an allocation each.
    std::vector<int> indexes;
    std::vector<int> slot_of(group_count, -1);
    for (const auto* plan : {&normal, &aggressive_plan})
        for (const int index : *plan)
            if (slot_of[index] < 0) {
                slot_of[index] = static_cast<int>(indexes.size());
                indexes.push_back(index);
            }
    for (int index = 0; index < group_count; ++index)
        if (slot_of[index] < 0 && data.has_camps[index]) {
            slot_of[index] = static_cast<int>(indexes.size());
            indexes.push_back(index);
        }
    if (indexes.size() > static_cast<std::size_t>(NO_CAPTURE)) {
        for (std::size_t i = NO_CAPTURE; i < indexes.size(); ++i) slot_of[indexes[i]] = -1;
        indexes.resize(NO_CAPTURE);
    }
    for (const int index : normal)
        if (slot_of[index] >= 0) cores[0].push_back(slot_of[index]);
    for (const int index : aggressive_plan)
        if (slot_of[index] >= 0) cores[1].push_back(slot_of[index]);
    if (cores[1].empty()) cores[1] = cores[0];

    slot_grid.assign(static_cast<std::size_t>(bc::map::width()) * bc::map::height(), -1);
    // Every route spawn tile, gathered as a row-major bit buffer (least significant bit first) and
    // handed to the engine in one call.
    std::vector<std::uint8_t> route_bits(bc::bitboard::linear_buffer_size(), 0);
    for (auto* list : {&centers}) list->reserve(indexes.size());
    route.reserve(indexes.size());
    spawns.reserve(indexes.size());
    spawn_lists.reserve(indexes.size());
    clears.reserve(indexes.size());
    values[0].reserve(indexes.size());
    values[1].reserve(indexes.size());
    int theirs_slots = 0;
    for (const int index : indexes) {
        const int s = static_cast<int>(route.size());
        const auto& group = groups[index];
        route.push_back(&group);
        spawn_lists.push_back(data.spawn_lists[index]);
        for (const auto tile : spawn_lists.back()) {
            slot_grid[static_cast<std::size_t>(tile.y) * bc::map::width() + tile.x] = static_cast<std::int16_t>(s);
            const auto bit = static_cast<std::size_t>(tile.y) * bc::map::width() + tile.x;
            route_bits[bit / 8] |= static_cast<std::uint8_t>(1u << (bit % 8));
        }
        spawns.emplace_back();
        for (const auto& camp : group.camps())
            if (camp.kind() == bc::camp_type::monolith) monoliths.insert(s);
        centers.push_back(data.centers[index]);
        const double share = theirs[index] ? draft::THEIRS_VALUE : 1.0;
        if (theirs[index]) ++theirs_slots;
        values[0].push_back(draft::group_value(group, draft::FARM_GOLD_WEIGHT) * share);
        values[1].push_back(draft::group_value(group) * share);
        clears.push_back(draft::clear_rounds(group));
    }
    route_tiles.import_linear_buffer(route_bits);
    ready_at.assign(route.size(), 0);
    say(format("groups: %d of %zu route slots on their side", theirs_slots, route.size()));
    // Their home zone stops chasing; walking steps to their spawn are built on first use.
    spawn_steps.reset();
    ward_spots = compute_ward_spots(build_home_zone());
    split_altars();
    ally_spawn_list = bc::map::spawn_region(Team::ally).positions();
    enemy_spawn_list = bc::map::spawn_region(Team::enemy).positions();
    spawn_gap.reset();
    for (const auto a : ally_spawn_list)
        for (const auto b : enemy_spawn_list) {
            const int d = distance(a, b);
            if (!spawn_gap || d < *spawn_gap) spawn_gap = d;
        }
    ally_in_ring = 0;
    for (const auto tile : ally_spawn_list)
        if (spawn_zone.is_set(tile)) ++ally_in_ring;
    ready = true;
}

bc::bitboard Bot::build_home_zone() {
    // Their home zone: tiles within SPAWN_CHASE_RANGE walking steps of their spawn and at least HOME_RATIO
    // times as far from ours. Both searches stop early, and a tile is only read one by one when it is
    // near both spawns, so normal maps read none. Returns the zone's ward ring.
    const auto& terrain = seen::terrain();
    const auto& their_spawn = bc::map::spawn_region(Team::enemy);
    bc::distance_field from_theirs, from_ours;
    bc::bitboard near_ours;
    spawn_zone.clear();
    from_theirs.compute(terrain, their_spawn, spawn_zone, SPAWN_CHASE_RANGE);
    // A zone tile fails the ratio only when our spawn is under HOME_RATIO * SPAWN_CHASE_RANGE steps away.
    from_ours.compute(terrain, bc::map::spawn_region(Team::ally), near_ours, HOME_RATIO * SPAWN_CHASE_RANGE - 1);
    const auto mixed = spawn_zone & near_ours;
    if (!mixed.is_empty())
        for (const auto tile : mixed.positions())
            if (from_ours.get(tile) < HOME_RATIO * from_theirs.get(tile)) spawn_zone.set(tile, false);

    // Ward ring: zone tiles exactly WARD_RING steps out; a smaller zone uses its furthest tiles instead.
    bc::distance_field scratch;
    bc::bitboard outer, inner;
    scratch.compute(terrain, their_spawn, outer, WARD_RING);
    scratch.compute(terrain, their_spawn, inner, WARD_RING - 1);
    auto ring = outer & ~inner & spawn_zone;
    if (ring.is_empty()) {
        int furthest = 0;
        const auto tiles = spawn_zone.positions();
        for (const auto tile : tiles) furthest = std::max<int>(furthest, from_theirs.get(tile));
        if (furthest > 0)
            for (const auto tile : tiles)
                if (from_theirs.get(tile) == furthest) ring.set(tile);
    }
    return ring;
}

int Bot::steps_to_enemy_spawn(vec2 position) {
    // Walking steps from a tile to the enemy spawn; the field is only built once a chase needs it.
    if (!spawn_steps) {
        spawn_steps.emplace();
        bc::bitboard reached;
        spawn_steps->compute(seen::terrain(), bc::map::spawn_region(Team::enemy), reached);
    }
    return spawn_steps->get(position);
}

void Bot::split_altars() {
    // Solo altars sit away from the centre and no closer to the enemy spawn than to ours.
    const vec2 middle{bc::map::width() / 2, bc::map::height() / 2};
    const auto ours = bc::map::spawn_region(Team::ally).positions();
    const auto theirs = bc::map::spawn_region(Team::enemy).positions();
    auto nearest = [](vec2 altar, const std::vector<vec2>& tiles) {
        std::optional<int> best;
        for (const auto t : tiles)
            if (!best || distance(altar, t) < *best) best = distance(altar, t);
        return best.value_or(0);
    };
    for (const auto altar : bc::map::altars().positions()) {
        const int home = nearest(altar, ours);
        const int away = nearest(altar, theirs);
        if (distance(altar, middle) > CENTRAL_ALTAR_RANGE && home <= away)
            solo_altars.push_back(altar);
        else
            escort_altars.push_back(altar);
    }
    say(format("altars: %zu solo, %zu escorted", solo_altars.size(), escort_altars.size()));
}

// Route memory

void Bot::observe() {
    // Vision is not shared, so each champion keeps its own respawn record.
    // One mask over every route spawn tile finds the groups in sight without a pass over all of them.
    const auto visible = bc::vision::visible_tiles() & route_tiles;
    if (visible.is_empty()) return;
    const int now = static_cast<int>(bc::round_number());
    std::set<int> slots;
    for (const auto tile : visible.positions()) slots.insert(slot_at(tile));
    for (const int index : slots) {
        const auto& group = *route[index];
        bc::map::camp_group_state state;
        try {
            state = group.state();
        } catch (const bc::error&) {
            continue;
        }
        const auto* respawning = std::get_if<bc::map::camp_group_respawning>(&state);
        ready_at[index] = respawning ? now + respawning->timer : 0;
        if (std::holds_alternative<bc::map::camp_group_reward_available>(state))
            rewards.insert(index);
        else
            rewards.erase(index);
    }
}

int Bot::wait_at(int s, vec2 origin) const {
    // Rounds left to wait on arrival at a route group.
    const int travel = distance(origin, centers[s]);
    return std::max(0, ready_at[s] - static_cast<int>(bc::round_number()) - travel);
}

void Bot::update_emblems() {
    // A champion's type never changes, so each ally's key is read once.
    const auto& allies = seen::roster(Team::ally);
    if (ally_keys.size() != allies.size()) ally_keys.assign(allies.size(), std::nullopt);
    for (std::size_t index = 0; index < allies.size(); ++index) {
        const auto& ally = allies[index];
        if (!ally_keys[index]) {
            try {
                ally_keys[index] = champ_id(ally.champion_name());
            } catch (const bc::error&) {
                continue;
            }
        }
        const int key = *ally_keys[index];
        emblems[key] = std::max(emblems.count(key) ? emblems[key] : 0, emblem_count(ally));
    }
}

int Bot::team_emblems() const {
    // Items are only readable in vision, so each champion keeps the most it has seen.
    // Vesper's done flag means she finished her build even if nobody else can see her bag.
    int total = 0;
    for (const auto& [key, count] : emblems) total += count;
    if (vesper_done) {
        const auto it = emblems.find(champ_id(Champ::vesper_vale));
        const int have = it == emblems.end() ? 0 : it->second;
        if (have < VESPER_CORE_EMBLEMS) total += VESPER_CORE_EMBLEMS - have;
    }
    return total;
}

bool Bot::allowed(int s) const {
    return !monoliths.count(s) || team_emblems() >= MONOLITH_EMBLEMS;
}

bool Bot::can_take_camp(const bc::camp& camp) const {
    const int s = slot_at(camp.spawn_position());
    return s < 0 || allowed(s);
}

const bc::bitboard& Bot::spawn_board(int s) {
    auto& board = spawns[s];
    if (!board) {
        board.emplace();
        for (const auto tile : spawn_lists[s]) board->set(tile);
    }
    return *board;
}

const Walk& Bot::group_field(int s) {
    if (goal_fields.size() != route.size()) goal_fields.resize(route.size());
    auto& walk = goal_fields[s];
    if (!walk) {
        walk = walk_field(spawn_board(s));
        ++goal_field_count;
    }
    return *walk;
}

void Bot::build_fields() {
    // Each route group's walking field is built one by one from fuel left over at the end of a turn.
    // Once all exist, travel() reads them instead of searching the map. A field's cost grows with the
    // map (a big map costs several times a small one), so the reserve is the dearest field seen so far
    // plus FIELD_FUEL_RESERVE: starting one that cannot finish would spill into the next turn.
    if (!ready || goal_field_count == static_cast<int>(route.size())) return;
    for (int s = 0; s < static_cast<int>(route.size()); ++s) {
        if (goal_fields.size() == route.size() && goal_fields[s]) continue;
        const auto before = bc::fuel_remaining();
        if (before < FIELD_FUEL_RESERVE + field_cost) return;
        group_field(s);
        field_cost = std::max(field_cost, before - bc::fuel_remaining());
    }
}

std::optional<int> Bot::gap(int start, int goal) {
    const auto key = std::make_pair(start, goal);
    auto it = gaps.find(key);
    if (it == gaps.end()) it = gaps.emplace(key, steps_to(group_field(goal), spawn_lists[start])).first;
    return it->second;
}

bool Bot::mid_clear(int s) {
    for (const auto& camp : camps_at(s))
        if (camp.health() < camp.max_health()) return true;
    return false;
}

std::vector<int> Bot::visible_loot(int skip) {
    // Groups whose camps or leftover reward are in our own sight.
    std::set<int> loot = rewards;
    for (const auto& camp : seen::camps()) {
        const int s = slot_at(camp.spawn_position());
        if (s >= 0) loot.insert(s);
    }
    loot.erase(skip);
    const int now = static_cast<int>(bc::round_number());
    std::vector<int> result;
    for (const int s : loot)
        if (ready_at[s] <= now || rewards.count(s)) result.push_back(s);
    return result;
}

double Bot::route_rate_steps(int s, int steps) const {
    const int wait = std::max(0, ready_at[s] - static_cast<int>(bc::round_number()) - steps);
    const double cycle = steps + wait + clears[s] + bc::constants::capture_time;
    return values[aggressive][s] / std::max(1.0, cycle);
}

int Bot::best_goal(const Travel& travel) const {
    // Planned pocket: gold-weighted while farming, the centre pocket once aggressive.
    std::vector<int> cores_in;
    for (const int s : cores[aggressive])
        if (travel.has(s)) cores_in.push_back(s);
    if (cores_in.empty())
        for (int s = 0; s < static_cast<int>(travel.steps.size()); ++s)
            if (travel.has(s)) cores_in.push_back(s);
    int best = cores_in.front();
    double best_rate = route_rate_steps(best, travel.at(best));
    for (const int s : cores_in) {
        const double rate = route_rate_steps(s, travel.at(s));
        if (rate > best_rate) {
            best = s;
            best_rate = rate;
        }
    }
    return best;
}

Bot::Travel Bot::travel(vec2 origin, int skip) {
    // Walking steps from origin to every allowed route group except skip. Walking is symmetric, so once
    // every group has its own field a lookup at origin replaces a fresh search of the whole map.
    const int n = static_cast<int>(route.size());
    Travel result;
    result.steps.assign(n, -1);
    // allowed() recounts the team's emblems, so the answer for monoliths is worked out once here.
    const bool monoliths_ok = team_emblems() >= MONOLITH_EMBLEMS;
    auto allowed_here = [&](int s) { return monoliths_ok || !monoliths.count(s); };
    if (goal_field_count == n) {
        for (int s = 0; s < n; ++s) {
            if (s == skip || !allowed_here(s)) continue;
            const auto steps = goal_fields[s]->field.get(origin);  // unreachable marks "not reached"
            if (steps != bc::distance_field::unreachable) {
                result.steps[s] = steps;
                ++result.reachable;
            }
        }
        return result;
    }
    const auto walk = walk_field(tiles_around(origin, 0));
    for (int s = 0; s < n; ++s)
        if (s != skip && allowed_here(s)) {
            const auto steps = steps_to(walk, spawn_lists[s]);
            if (steps) {
                result.steps[s] = *steps;
                ++result.reachable;
            }
        }
    return result;
}

int Bot::pick_target(int s, vec2 origin, int skip, bool seen_only) {
    // Walk toward the planned goal, but take every visible group that sits on that walk
    // (or close enough that turning aside is cheap). A group already under attack is finished.
    // The group being collected is skipped so everyone else moves on. Monoliths wait until
    // the team is strong enough. With seen_only, only loot this champion can see is offered.
    if (s != skip && mid_clear(s)) return s;
    const auto seen_loot = visible_loot(skip);
    if (seen_only && seen_loot.empty()) return s;
    const auto steps = travel(origin, skip);
    if (steps.empty()) return s;

    const int dest = best_goal(steps);
    std::vector<int> loot;
    for (const int l : seen_loot)
        if (steps.has(l)) loot.push_back(l);
    if (seen_only && loot.empty()) return s;

    const int dest_steps = steps.has(dest) ? steps.at(dest) : 0;
    auto extra = [&](int l) {
        if (l == dest) return 0;
        const auto g = gap(l, dest);
        if (!g) return 255;
        return steps.at(l) + *g - dest_steps;
    };

    std::optional<int> on_way;
    std::tuple<int, double, int> on_way_key{};
    for (const int l : loot)
        if (extra(l) <= ON_WAY_SLACK || steps.at(l) <= NEAR_LOOT) {
            const std::tuple<int, double, int> key{steps.at(l), -values[aggressive][l], l};
            if (!on_way || key < on_way_key) {
                on_way = l;
                on_way_key = key;
            }
        }
    if (on_way) return *on_way;

    if (seen_only) return s;

    const int wait = std::max(0, ready_at[dest] - static_cast<int>(bc::round_number()) - steps.at(dest));
    if (wait == 0) {
        if (steps.has(s) && s != skip) {
            const double here = route_rate_steps(s, steps.at(s));
            if (here * SWITCH_MARGIN >= route_rate_steps(dest, steps.at(dest))) return s;
        }
        return dest;
    }
    // The pocket is down, so spend the wait on the best group from here.
    int best = -1;
    double best_rate = 0;
    for (int l = 0; l < static_cast<int>(steps.steps.size()); ++l) {
        if (!steps.has(l)) continue;
        const double rate = route_rate_steps(l, steps.at(l));
        if (best < 0 || rate > best_rate) {
            best = l;
            best_rate = rate;
        }
    }
    return best;
}

void Bot::update_rejoining(const bc::champion& me, int s) {
    if (!me.is_alive()) return;
    const auto position = me.position();
    if (seen::ally_spawn().is_set(position))
        rejoining = true;
    else if (rejoining && !route.empty()
             && distance(position, centers[s % static_cast<int>(route.size())]) <= TEAM_RANGE)
        rejoining = false;
}

std::optional<std::string> Bot::lead_block(const bc::champion& me, int stamp, int now) {
    // Only a champion with the team settles the target: one on spawn or walking back from it would
    // pick from its own position and drag the shared target away from where the team is farming.
    // If nobody led last round the whole team is away, so anyone may re-pick from where they are.
    // The stamp is round + 1, so it matches this round once someone has already led.
    if (stamp == now + 1) return "already settled this round";
    if (!me.is_alive()) return "dead";
    if (channelling(me)) return "channelling";
    if (me.is_teleporting()) return "teleporting";
    if (stamp >= now && rejoining) return "rejoining, last decision still fresh";
    return std::nullopt;
}

void Bot::share_loot(const bc::champion& me) {
    // Vision isn't shared, so a follower that sees on-the-way loot the leader can't points the team at it.
    // Loot behind the follower (further from the target than it is) would drag the team back.
    if (led || rejoining || mode != FARM || route.empty() || !me.is_alive() || channelling(me) || under_threat(me))
        return;
    const int s = slot % static_cast<int>(route.size());
    const auto position = me.position();
    const int pick = pick_target(s, position, capture_slot, true);
    if (pick == s || distance(centers[pick], centers[s]) > distance(position, centers[s])) return;
    slot = pick;
    set_shared_byte(SLOT_BYTE, slot);
    note(format("spots loot at slot %d", slot));
    say(format("round %u: champion %d spots loot at route slot %d", bc::round_number(), champ_id(me.champion_name()), slot));
}

void Bot::update_capture(const bc::champion& me, int& capture, int& who, int& when) {
    // Drops finished or stale capture claims and claims a fresh reward.
    const int now = static_cast<int>(bc::round_number());
    if (capture != NO_CAPTURE && (capture >= static_cast<int>(route.size()) || ready_at[capture] > now
                                  || ((now - when) & 0xFF) > CAPTURE_TIMEOUT))
        capture = NO_CAPTURE;
    if (capture != NO_CAPTURE || rewards.empty()) return;

    const auto position = me.position();
    int s = *rewards.begin();
    for (const int r : rewards)
        if (distance(position, centers[r]) < distance(position, centers[s])) s = r;
    // Only a fresh sighting can claim this group again, so a stale record can't re-send a collector.
    rewards.erase(s);
    const auto& tiles = spawn_lists[s];
    std::vector<bc::champion> allies;
    for (const auto& a : seen::champions(Team::ally))
        if (!channelling(a)) allies.push_back(a);
    if (std::find(allies.begin(), allies.end(), me) == allies.end()) allies.push_back(me);
    std::vector<bc::champion> builders;
    for (const auto& a : allies)
        if (a.champion_name() == Champ::vesper_vale && emblem_count(a) < VESPER_CORE_EMBLEMS) builders.push_back(a);
    // A building Vesper collects first; otherwise the poorest visible ally, so the team catches up evenly.
    const auto& pool = builders.empty() ? allies : builders;
    auto key = [&](const bc::champion& a) {
        int nearest = 0;
        bool first = true;
        for (const auto t : tiles) {
            const int d = distance(a.position(), t);
            if (first || d < nearest) nearest = d;
            first = false;
        }
        return std::make_pair(item_worth(a), nearest);
    };
    std::size_t chosen = 0;
    auto chosen_key = key(pool[0]);
    for (std::size_t i = 1; i < pool.size(); ++i) {
        const auto k = key(pool[i]);
        if (k < chosen_key) {
            chosen = i;
            chosen_key = k;
        }
    }
    const int name = champ_id(pool[chosen].champion_name());
    say(format("round %d: champion %d collects route slot %d", now, name, s));
    capture = s;
    who = name + 1;
    when = now;
}

// Shared decision

void Bot::sync_decision(const bc::champion& me) {
    auto shared = read_shared();
    const int now = static_cast<int>(bc::round_number());
    if (shared.mode != mode) mode_changed = now;
    if (!shared.vesper_done && me.champion_name() == Champ::vesper_vale && emblem_count(me) >= VESPER_CORE_EMBLEMS) {
        shared.vesper_done = true;
        mark_vesper_done();
    }
    vesper_done = shared.vesper_done;
    const int new_carrier = read_carrier();
    if (new_carrier != carrier) carrier_since = now;
    carrier = new_carrier;
    int carried = 0;
    for (const auto& ally : seen::roster(Team::ally)) carried += gems_carried(ally);
    win_tribute = static_cast<int>(bc::global_submitted_gems(Team::ally)) + carried >= WIN_GEMS;
    // Without a leader this round, last round's decision stands. The stamp is round + 1.
    update_rejoining(me, shared.slot);
    const auto block = lead_block(me, shared.stamp, now);
    led = !block;
    if (!led) {
        mode = shared.mode;
        slot = shared.slot;
        aggressive = shared.aggressive;
        capture_slot = shared.capture_slot;
        collector = shared.collector;
        claimed = shared.claimed;
        shared_why = std::string("follow ") + MODE_NAMES[mode] + " " + slot_text() + " (" + *block + ") " + shared_tags();
        return;
    }

    // First champion with the team this round settles the capture, picks the target from its own
    // respawn record, then scores. A dead carrier's escort is called off, as is one running too long.
    if (carrier && (!carrier_alive() || now - carrier_since > ESCORT_TIMEOUT)) {
        say(format("round %d: escort for champion %d called off", now, carrier - 1));
        carrier = 0;
    }
    const bool aggressive_now = shared.vesper_done && team_alive();
    if (aggressive_now != aggressive)
        say(format("round %d: aggressive mode %s", now, aggressive_now ? "on" : "off"));
    aggressive = aggressive_now;
    int s = route.empty() ? 0 : shared.slot % static_cast<int>(route.size());
    int capture = shared.capture_slot, who = shared.collector, when = shared.claimed;
    if (!route.empty()) {
        update_capture(me, capture, who, when);
        // The escort altar is picked from the target, so the target holds while an escort runs.
        if (!carrier) s = pick_target(s, me.position(), capture);
    }
    // Fighting outranks the escort; the escort outranks farming.
    int new_mode = score_mode(route.empty() ? std::nullopt : std::optional<vec2>{centers[s]}, shared.mode);
    // A fresh switch between fight and retreat holds for a few rounds before it can flip back.
    std::string hyst;
    const bool flip = (new_mode == FIGHT && shared.mode == RETREAT) || (new_mode == RETREAT && shared.mode == FIGHT);
    if (flip && now - mode_changed < MIN_HOLD) {
        hyst = format("hysteresis %d/%d keeps %s", now - mode_changed, MIN_HOLD, MODE_NAMES[shared.mode]);
        new_mode = shared.mode;
    }
    if (new_mode != shared.mode) mode_changed = now;
    std::string escort_note;
    if (new_mode == FARM && carrier) {
        escort_note = format("escort c%d lifts farm to tribute", carrier - 1);
        new_mode = TRIBUTE;
    }
    if (win_tribute && new_mode != FIGHT) new_mode = TRIBUTE;
    if (new_mode != mode || s != slot) say(format("round %d: %s, route slot %d", now, MODE_NAMES[new_mode], s));
    mode = new_mode;
    slot = s;
    capture_slot = capture;
    collector = who;
    claimed = when;
    write_shared(mode, slot, capture, who, when, shared.vesper_done, aggressive, carrier);
    shared_why = std::string("lead ") + MODE_NAMES[mode] + " " + slot_text() + " " + score_why;
    if (!hyst.empty()) shared_why += " " + hyst;
    if (!escort_note.empty()) shared_why += " " + escort_note;
    shared_why += " " + shared_tags();
}

bool Bot::is_collector(const bc::champion& me) const {
    return capture_slot != NO_CAPTURE && collector == champ_id(me.champion_name()) + 1;
}

void Bot::vesper_claim(const bc::champion& me) {
    // The leader may not see Vesper, so she claims nearby loot herself; the old collector moves on.
    if (me.champion_name() != Champ::vesper_vale || emblem_count(me) >= VESPER_CORE_EMBLEMS || is_collector(me)
        || under_threat(me) || route.empty())
        return;
    const auto position = me.position();
    int s = capture_slot;
    if (s == NO_CAPTURE || s >= static_cast<int>(route.size())) {
        s = NO_CAPTURE;
        for (const int r : rewards)
            if (s == NO_CAPTURE || distance(position, centers[r]) < distance(position, centers[s])) s = r;
    }
    if (s == NO_CAPTURE || distance(position, centers[s]) > VESPER_CLAIM_RANGE) return;
    rewards.erase(s);
    const int now = static_cast<int>(bc::round_number());
    capture_slot = s;
    collector = champ_id(Champ::vesper_vale) + 1;
    claimed = now;
    set_shared_byte(CAPTURE_SLOT_BYTE, s);
    set_shared_byte(CAPTURE_SLOT_BYTE + 1, collector);
    set_shared_byte(CLAIM_ROUND_OFFSET, now & 0xFF);
    note(format("vesper claims slot %d", s));
    say(format("round %d: vesper takes route slot %d", now, s));
}

bool Bot::collect(const bc::champion&) {
    // Stand on the cleared group until its reward lands, then release the claim for everyone.
    const int index = capture_slot;
    if (index >= static_cast<int>(route.size())) return false;
    if (ready_at[index] > static_cast<int>(bc::round_number())) {
        set_shared_byte(CAPTURE_SLOT_BYTE, NO_CAPTURE);
        capture_slot = NO_CAPTURE;
        note(format("claim slot %d cleared, group respawning", index));
        return false;
    }
    if (!camps_at(index).empty()) {
        note(format("collector of slot %d but camps still up, fall through", index));
        return false;
    }
    branch("collect.loot", format("claim slot %d, walk onto the reward", index));
    move_towards_closest(spawn_board(index));
    return true;
}

int Bot::score_mode(std::optional<vec2> origin, int current) {
    // Enemies further from origin (the target, or our own tile) count for less.
    const auto& enemies = seen::champions(Team::enemy);
    if (enemies.empty()) {
        score_why = "no enemies -> farm";
        return FARM;
    }
    const auto& allies = seen::champions(Team::ally);
    const int na = static_cast<int>(allies.size()), ne = static_cast<int>(enemies.size());

    double score = 0.0;
    // The reason text, built into one string (each separate std::string costs an allocation).
    std::string joined;
    joined.reserve(200);
    auto part = [&](const std::string& text) {
        if (!joined.empty()) joined += ", ";
        joined += text;
    };
    if (na > ne) {
        score += OUTNUMBER_BONUS;
        part(format("nums +%d %dv%d", OUTNUMBER_BONUS, na, ne));
    } else {
        const int pen = MISSING_BODY_PENALTY * (ne - na);
        score -= pen;
        part(format("nums -%d %dv%d", pen, na, ne));
    }
    PySum ally_hp, enemy_hp;
    for (const auto& a : allies) ally_hp.add(a.health());
    for (const auto& e : enemies) enemy_hp.add(e.health());
    const double ally_health = ally_hp.value(), enemy_health = enemy_hp.value();
    const int hp_edge = ally_health > enemy_health ? HEALTH_EDGE : -HEALTH_EDGE;
    score += hp_edge;
    part(format("hp %+d %.0f/%.0f", hp_edge, ally_health, enemy_health));
    PySum ally_pw, enemy_pw;
    for (const auto& a : allies) ally_pw.add(static_cast<double>(a.strength()) + a.focus());
    for (const auto& e : enemies) enemy_pw.add(static_cast<double>(e.strength()) + e.focus());
    const double ally_power = ally_pw.value(), enemy_power = enemy_pw.value();
    const int power_edge = ally_power > enemy_power ? POWER_EDGE : -POWER_EDGE;
    score += power_edge;
    part(format("power %+d %.0f/%.0f", power_edge, ally_power, enemy_power));
    int owned = 0;
    for (const auto& a : allies) owned += emblem_count(a);
    const int emblem_score = EMBLEM_BONUS * owned;
    score += emblem_score;
    part(format("emblems +%d", emblem_score));
    if (origin) {
        int nearest = distance(enemies[0].position(), *origin);
        for (const auto& e : enemies) nearest = std::min(nearest, distance(e.position(), *origin));
        const double dist = DISTANCE_PENALTY * nearest;
        score -= dist;
        part(format("dist -%.1f", dist));
    }
    if (squishy_overextended(allies, enemies)) {
        score -= OVEREXTEND_PENALTY;
        part(format("overextend -%d", OVEREXTEND_PENALTY));
    }
    if (seen::ally_spawn().is_set(bc::self().position())) {
        score += SPAWN_BONUS;
        part(format("ally-spawn +%d", SPAWN_BONUS));
    }
    const auto& enemy_roster = seen::roster(Team::enemy);
    int dead_enemies = 0;
    for (const auto& enemy : enemy_roster)
        if (!enemy.is_alive()) ++dead_enemies;
    const int kill_window = kill_window_dead_enemies(enemy_roster.size());
    if (kill_window >= 0 && dead_enemies >= kill_window) {
        score -= KILL_WINDOW_FIGHT_PENALTY;
        part(format("kill-window -%d (%d/%zu dead)", KILL_WINDOW_FIGHT_PENALTY, dead_enemies, enemy_roster.size()));
    }
    const int bar = current == FIGHT ? FIGHT_THRESHOLD - FIGHT_HYSTERESIS : FIGHT_THRESHOLD;
    const int choice = score >= bar ? FIGHT : RETREAT;
    score_why = format("%.1f/%d %s (", score, bar, MODE_NAMES[choice]) + joined + ")";
    return choice;
}

bool Bot::squishy_overextended(const std::vector<bc::champion>& allies, const std::vector<bc::champion>& enemies) {
    // Melee allies are the tanks. A squishy with none of them closer to the nearest enemy is in front.
    std::vector<bc::champion> tanks;
    for (const auto& ally : allies)
        if (ally.attack_range() <= 1) tanks.push_back(ally);
    for (const auto& squishy : allies) {
        if (squishy.max_health() > SQUISHY_HEALTH) continue;
        const auto here = squishy.position();
        std::size_t nearest = 0;
        for (std::size_t i = 1; i < enemies.size(); ++i)
            if (distance(here, enemies[i].position()) < distance(here, enemies[nearest].position())) nearest = i;
        const auto spot = enemies[nearest].position();
        const int gap = distance(here, spot);
        bool covered = false;
        for (const auto& tank : tanks)
            if (distance(tank.position(), spot) < gap) covered = true;
        if (!covered) return true;
    }
    return false;
}
