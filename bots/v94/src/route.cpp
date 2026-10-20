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
    auto normal = draft::decode_pocket(memory, offset).members;
    auto aggressive_plan = draft::decode_pocket(memory, offset).members;
    auto out_of_range = [&](const std::vector<int>& plan) {
        return std::any_of(plan.begin(), plan.end(), [&](int index) { return index >= group_count; });
    };
    if (normal.empty() || out_of_range(normal)) {
        normal = draft::compute_pocket().members;
        aggressive_plan = draft::compute_pocket(true).members;
    }
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
    bool gem_income = false;
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
        if (!monoliths.count(s) && group.gem_value() > 0) gem_income = true;
        centers.push_back(data.centers[index]);
        values[0].push_back(draft::group_value(group, draft::FARM_GOLD_WEIGHT));
        values[1].push_back(draft::group_value(group));
        clears.push_back(draft::clear_rounds(group));
        gemless.push_back(group.gem_value() == 0);
    }
    route_tiles.import_linear_buffer(route_bits);
    gem_starved = !gem_income && !monoliths.empty();
    if (gem_starved) LOG(say("setup: gem-starved map, monoliths are ordinary targets"););
    ready_at.assign(route.size(), 0);
    // Ward spots and altar kinds come from the draft when its setup block is there and fits this map.
    const auto facts = draft::decode_setup(memory, offset);
    if (facts && facts->solo.size() == bc::map::altars().positions().size()) {
        ward_spots = facts->ward_spots;
        split_altars(&facts->solo);
    } else {
        ward_spots = compute_ward_spots();
        split_altars();
    }
    // v86 (V86_ZONES): the draft's zone block follows the setup block; without it every zone stays open (v82).
    if (V86_ZONES && facts)
        if (const auto zf = draft::decode_zones(memory, draft::setup_end(memory, offset))) {
            for (const int index : indexes) {
                zone_of.push_back(static_cast<std::uint8_t>(zf->zone[index]));
                pair_ok.push_back(zf->pair_ok[index] ? 1 : 0);
                home_steps.push_back(zf->ours[index]);
                enemy_steps.push_back(zf->theirs[index]);
            }
            zones_known = true;
            int safe = 0, unsafe = 0;
            for (std::size_t s = 0; s < zone_of.size(); ++s) {
                if (zone_of[s] == ZONE_SAFE && route[s]->gem_value() > 0 && !monoliths.count(static_cast<int>(s))) any_safe = true;
                safe += zone_of[s] == ZONE_SAFE;
                unsafe += zone_of[s] == ZONE_UNSAFE;
            }
            LOG(say(format("setup: zones %d safe, %d uncertain, %d unsafe%s", safe, static_cast<int>(zone_of.size()) - safe - unsafe,
                           unsafe, any_safe ? "" : " (no safe gem group: uncertain stays open)")););
        }
    // The tiles close enough to the enemy spawn to stop chasing; walking steps to it are built on first use.
    const auto& theirs = bc::map::spawn_region(Team::enemy);
    spawn_steps.reset();
    spawn_zone.clear();
    for (const auto tile : theirs.positions()) spawn_zone.draw_chebyshev(tile, SPAWN_CHASE_RANGE);
    ally_spawn_list = bc::map::spawn_region(Team::ally).positions();
    enemy_spawn_list = theirs.positions();
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

int Bot::steps_to_enemy_spawn(vec2 position) {
    // Walking steps from a tile to the enemy spawn; the field is only built once a chase needs it.
    if (!spawn_steps) {
        spawn_steps.emplace();
        bc::bitboard reached;
        spawn_steps->compute(seen::terrain(), bc::map::spawn_region(Team::enemy), reached);
    }
    return spawn_steps->get(position);
}

void Bot::split_altars(const std::vector<bool>* solo) {
    // Solo altars sit away from the centre and no closer to the enemy spawn than to ours (draft::solo_altar).
    const auto altars = bc::map::altars().positions();
    for (std::size_t i = 0; i < altars.size(); ++i) {
        const auto altar = altars[i];
        if (solo ? (*solo)[i] : draft::solo_altar(altar))
            solo_altars.push_back(altar);
        else
            escort_altars.push_back(altar);
    }
    LOG(say(format("setup: altars %zu solo, %zu escorted", solo_altars.size(), escort_altars.size())););
}

// Route memory

void Bot::observe() {
    // Vision is not shared, so each champion keeps its own respawn record.
    // One mask over every route spawn tile finds the groups in sight without a pass over all of them.
    const auto visible = bc::vision::visible_tiles() & route_tiles;
    if (visible.is_empty()) return;
    const int now = static_cast<int>(bc::round_number());
    // Ascending and without repeats, like a set, in a reused vector (a set allocates per insert).
    auto& slots = slot_scratch;
    slots.clear();
    for (const auto tile : visible.positions()) slots.push_back(slot_at(tile));
    std::sort(slots.begin(), slots.end());
    slots.erase(std::unique(slots.begin(), slots.end()), slots.end());
    for (const int index : slots) {
        // The group is in sight (one of its spawn tiles is), so the raw state read replaces group.state(),
        // which fetched a whole vision board per group to check that. 1 alive, 2 reward, 3 respawning.
        const auto handle = static_cast<std::uint32_t>(static_cast<bc::handle_camp_group_t>(*route[index]));
        std::uint32_t state = 0;
        if (bc::detail::sdk_camp_group_state(handle, &state) != 0 || state < 1 || state > 3) continue;
        std::int32_t timer = 0;
        if (state == 3 && bc::detail::sdk_camp_group_cooldown(handle, &timer) != 0) continue;
        ready_at[index] = state == 3 ? now + timer : 0;
        if (state == 2)
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
    // v86: emblems only change at a shop, so the item reads (~3k fuel for the team) run every EMBLEM_READ_EVERY rounds.
    const int now = static_cast<int>(bc::round_number());
    if (ally_keys.size() == allies.size() && now - emblem_read_round < EMBLEM_READ_EVERY) return;
    emblem_read_round = now;
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
    // Items are only readable in vision, so each champion keeps the most it has seen. Vesper's done flag
    // means she reached her core build even if nobody else can see her bag.
    int total = 0;
    for (const auto& [key, count] : emblems) total += count;
    auto at_least = [&](bool done, Champ kind, int core) {
        if (!done) return;
        const auto it = emblems.find(champ_id(kind));
        const int have = it == emblems.end() ? 0 : it->second;
        if (have < core) total += core - have;
    };
    at_least(vesper_done, Champ::vesper_vale, VESPER_CORE_EMBLEMS);
    return total;
}

bool Bot::drafted_vesper() {
    // Whether this team has Vesper (types are readable out of sight). Read once.
    if (!has_vesper) {
        has_vesper = false;
        for (const auto& ally : seen::roster(Team::ally))
            if (ally.champion_name() == Champ::vesper_vale) has_vesper = true;
    }
    return *has_vesper;
}

bool Bot::vesper_funnel(const bc::champion& champion) const {
    if (champion.champion_name() != Champ::vesper_vale) return false;
    return emblem_count(champion) < VESPER_CORE_EMBLEMS;
}

bool Bot::allowed(int s) {
    if (!monoliths.count(s)) return true;
    // v81 (MAGE_MONOLITH): the mages take monoliths on their own job; the shared route only points at one on a
    // gem-starved map, under the usual gates (strong and grouped: the area is contested), and then the team holds
    // the area around it (melees guard, mages cast).
    if (monolith_delegated() && !gem_starved) return false;
    return team_emblems() >= MONOLITH_EMBLEMS && monolith_grouped(s);
}

bool Bot::monolith_grouped(int s) {
    // Strict to start, looser to keep going, so one ally stepping away doesn't flip the team off it.
    // Vision only lists allies in sight, so the team size is every ally ever seen (the emblem record
    // has one key per ally, and everyone starts together on spawn). Dead allies count as missing.
    const auto& me = bc::self();
    const int living = static_cast<int>(emblems.size());
    const auto position = me.position();
    // Counted without building a list: the visible allies in range and not channelling, plus us.
    int near = 0;
    bool me_counted = false;
    for (const auto& ally : seen::champions(Team::ally)) {
        if (distance(position, ally.position()) > MONOLITH_GROUP_RANGE || channelling(ally)) continue;
        ++near;
        if (ally == me) me_counted = true;
    }
    int need = living;
    if (gem_starved) {
        // The monolith is the only gem source: someone is nearly always dead or recalling here, so the start
        // only needs MONOLITH_KEEP + 1 of the living allies (or all of them if fewer live).
        int alive = 0;
        for (const auto& ally : seen::roster(Team::ally))
            if (ally.is_alive()) ++alive;
        need = std::min(MONOLITH_KEEP + 1, alive);
    }
    if (!me_counted) ++near;
    // Once the team is on it (hitting it, or on a gem-starved map already heading there) the looser keep
    // rule holds, so one ally stepping away does not flip the target every round.
    const bool committed = mid_clear(s) || (gem_starved && !route.empty() && slot % static_cast<int>(route.size()) == s);
    if (committed) return near >= std::min(MONOLITH_KEEP, living);
    if (!seen::champions(Team::enemy).empty() || near < need) return false;
    if (me.health() < me.max_health() * MONOLITH_START_HEALTH) return false;
    for (const auto& ally : seen::champions(Team::ally))
        if (distance(position, ally.position()) <= MONOLITH_GROUP_RANGE && !channelling(ally)
            && ally.health() < ally.max_health() * MONOLITH_START_HEALTH)
            return false;
    return true;
}

bool Bot::can_take_camp(const bc::camp& camp) {
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
    if (!ready) return;
    // The key-bush boards first: a few thousand fuel, once.
    if (!key_bushes) {
        if (bc::fuel_remaining() < FIELD_FUEL_RESERVE + BUSH_BOARD_FUEL) return;
        build_bush_boards();
    }
    if (goal_field_count == static_cast<int>(route.size())) return;
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
    // Straight off the vision list: building camps_at()'s list would allocate.
    for (const auto& camp : seen::camps())
        if (slot_at(camp.spawn_position()) == s && camp.health() < camp.max_health()) return true;
    return false;
}

std::vector<int> Bot::visible_loot(int skip) {
    // Groups whose camps or leftover reward are in our own sight. Ascending and without repeats, like a
    // set, in a reused vector (a set allocates per insert).
    auto& loot = slot_scratch;
    loot.assign(rewards.begin(), rewards.end());
    for (const auto& camp : seen::camps()) {
        const int s = slot_at(camp.spawn_position());
        if (s >= 0) loot.push_back(s);
    }
    std::sort(loot.begin(), loot.end());
    loot.erase(std::unique(loot.begin(), loot.end()), loot.end());
    loot.erase(std::remove(loot.begin(), loot.end(), skip), loot.end());
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
    // On a gem-starved map the monolith decides the game (gems before gold), so once its gates let it into
    // the travel table it is the goal outright.
    if (gem_starved)
        for (const int m : monoliths)
            if (travel.has(m)) return m;
    // Planned pocket: gold-weighted while farming, the centre pocket once aggressive.
    std::vector<int> cores_in;
    for (const int s : cores[aggressive])
        if (travel.has(s)) cores_in.push_back(s);
    if (cores_in.empty())
        for (int s = 0; s < static_cast<int>(travel.steps.size()); ++s)
            if (travel.has(s)) cores_in.push_back(s);
    // v72 (FIX_GEM_FIRST): a pocket with gem groups aims only at those; its gold clusters are fillers for the
    // respawn waits (pick_target's wait branch), not goals.
    if (FIX_GEM_FIRST) {
        std::vector<int> gem_cores;
        for (const int s : cores_in)
            if (!gemless[s]) gem_cores.push_back(s);
        if (!gem_cores.empty()) cores_in = std::move(gem_cores);
    }
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

void Bot::drop_monoliths(Travel& travel) {
    // A monolith stays only when the team is already on it, or when no other group in reach is up within
    // MONOLITH_IDLE rounds of our arrival; either way the team must own MONOLITH_EMBLEMS and be grouped.
    bool any = false;
    for (const int m : monoliths)
        if (travel.has(m)) any = true;
    if (!any) return;
    const int now = static_cast<int>(bc::round_number());
    bool last_resort = true;
    if (!gem_starved)
        for (int s = 0; s < static_cast<int>(travel.steps.size()) && last_resort; ++s)
        if (travel.has(s) && !monoliths.count(s) && ready_at[s] - now - travel.at(s) <= MONOLITH_IDLE) last_resort = false;
    const bool strong = team_emblems() >= MONOLITH_EMBLEMS;
    const bool delegated = monolith_delegated();
    for (const int m : monoliths) {
        if (!travel.has(m)) continue;
        if (!(delegated && !gem_starved) && strong && (last_resort || mid_clear(m)) && monolith_grouped(m)) continue;
        travel.steps[m] = -1;
        --travel.reachable;
    }
}

Bot::Travel Bot::travel(vec2 origin, int skip) {
    // Walking steps from origin to every allowed route group except skip. Walking is symmetric, so once
    // every group has its own field a lookup at origin replaces a fresh search of the whole map.
    const int n = static_cast<int>(route.size());
    Travel result;
    result.steps.assign(n, -1);
    // Every group but skip first; monoliths are then dropped unless they are the last resort.
    if (goal_field_count == n) {
        for (int s = 0; s < n; ++s) {
            if (s == skip) continue;
            const auto steps = goal_fields[s]->field.get(origin);  // unreachable marks "not reached"
            if (steps != bc::distance_field::unreachable) {
                result.steps[s] = steps;
                ++result.reachable;
            }
        }
    } else {
        const auto walk = walk_field(tiles_around(origin, 0));
        for (int s = 0; s < n; ++s)
            if (s != skip) {
                const auto steps = steps_to(walk, spawn_lists[s]);
                if (steps) {
                    result.steps[s] = *steps;
                    ++result.reachable;
                }
            }
    }
    drop_monoliths(result);
    // v86: zones the phase keeps closed, the pair's target (main team) or what the pair may not take, and groups too
    // far from a reward being captured outside the safe zone.
    if (zones_known || capture_near >= 0)
        for (int s = 0; s < n; ++s) {
            if (!result.has(s)) continue;
            bool drop = !zone_open(s);
            if (pair_filter == 1 && s == target2) drop = true;
            if (pair_filter == 2
                && (!pair_ok[s] || zone(s) != ZONE_SAFE || (main_slot >= 0 && distance(centers[s], centers[main_slot]) < SPLIT_APART)))
                drop = true;
            if (capture_near >= 0 && distance(centers[s], centers[capture_near]) > CAPTURE_NEAR) drop = true;
            if (drop) {
                result.steps[s] = -1;
                --result.reachable;
            }
        }
    return result;
}

int Bot::zone_cap_for(int ph) const {
    // Without zones (or a safe gem group) the old rules hold: everything but the unsafe zone stays open.
    if (!zones_known) return ZONE_UNSAFE;
    const int base = any_safe && V86_CENTRE ? ZONE_SAFE : ZONE_UNCERTAIN;
    if (ph == PHASE_RAID) return ZONE_UNSAFE;
    if (ph == PHASE_CENTRE) return ZONE_UNCERTAIN;
    return base;
}

int Bot::pick_target(int s, vec2 origin, int skip, bool seen_only) {
    // Walk toward the planned goal, but take every visible group that sits on that walk
    // (or close enough that turning aside is cheap). A group already under attack is finished.
    // The group being collected is skipped so everyone else moves on. Monoliths wait until
    // the team is strong enough. With seen_only, only loot this champion can see is offered.
    if (s != skip && mid_clear(s) && allowed(s)) return s;
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
    for (const int l : loot) {
        // v72: a gold cluster costs a clear and a five-round capture for 35 gold and no gems; ladder teams skip
        // them (we captured 2x as many on monolith, swirl and lanes), so one is only taken right beside us.
        if (FIX_GEM_FIRST && gemless[l] && steps.at(l) > GOLD_ONLY_NEAR) continue;
        if (extra(l) <= ON_WAY_SLACK || steps.at(l) <= NEAR_LOOT) {
            const std::tuple<int, double, int> key{steps.at(l), -values[aggressive][l], l};
            if (!on_way || key < on_way_key) {
                on_way = l;
                on_way_key = key;
            }
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
    if (!me.is_alive()) {
        walked_last_turn = false;
        return;
    }
    const auto position = me.position();
    const bool on_spawn = seen::ally_spawn().is_set(position);
    // v74: only an arrival (respawn, recall, match start) makes us rejoin; walking back over a spawn tile
    // does not, or the champion could never lead or call an escort while passing through.
    const bool arrived = !FIX_SPAWN_BANK || !walked_last_turn;
    walked_last_turn = !on_spawn && !me.is_recalling();
    if (on_spawn && arrived)
        rejoining = true;
    else if (rejoining && !route.empty()
             && distance(position, centers[s % static_cast<int>(route.size())]) <= TEAM_RANGE)
        rejoining = false;
}

std::optional<std::string> Bot::lead_block(const bc::champion& me, int stamp, int now, int target) {
    // Only a champion with the team settles the target: one on spawn or walking back from it would
    // pick from its own position and drag the shared target away from where the team is farming.
    // If nobody led last round the whole team is away, so anyone may re-pick from where they are.
    // The stamp is round + 1, so it matches this round once someone has already led.
    // v86: the one-byte stamp matches this round once someone has led, and last round's while it is fresh.
    const bool fresh = stamp == stamp_of(now) || stamp == stamp_of(now - 1);
    if (stamp == stamp_of(now)) return "already settled this round";
    if (!me.is_alive()) return "dead";
    if (channelling(me)) return "channelling";
    if (me.is_teleporting()) return "teleporting";
    // v86 (V86_SPLIT): the pair farms its own group and sees nothing of the main team's.
    if (pair_split()) return "split pair";
    if (fresh && rejoining) return "rejoining, last decision still fresh";
    // v71: one off on its own would score the team's mode and pick its target and captures from what only
    // it can see, so while the last decision is fresh it leaves it to someone with the team.
    if (FIX_LEAD && fresh && !route.empty()) {
        const auto position = me.position();
        if (distance(position, centers[target % static_cast<int>(route.size())]) > TEAM_RANGE) {
            for (const auto& ally : seen::champions(Team::ally))
                if (ally != me && distance(position, ally.position()) <= TEAM_RANGE) return std::nullopt;
            return "away from the team, last decision still fresh";
        }
    }
    return std::nullopt;
}

void Bot::share_loot(const bc::champion& me) {
    // Vision isn't shared, so a follower that sees on-the-way loot the leader can't points the team at it.
    // Loot behind the follower (further from the target than it is) would drag the team back.
    if (led || rejoining || mode != FARM || route.empty() || !me.is_alive() || channelling(me) || under_threat(me))
        return;
    if (pair_split()) return;
    const int s = slot % static_cast<int>(route.size());
    const auto position = me.position();
    const int pick = pick_target(s, position, capture_slot, true);
    if (pick == s || distance(centers[pick], centers[s]) > distance(position, centers[s])) return;
    slot = pick;
    set_shared_byte(SLOT_BYTE, slot);
    NOTE(format("sees loot at s%d on the way: points the team there", slot));
}

void Bot::update_capture(const bc::champion& me, int& capture, int& who, int& when) {
    // Drops finished or stale capture claims and claims a fresh reward.
    const int now = static_cast<int>(bc::round_number());
    // v71: a dead collector cannot capture, so its claim goes at once. Life reads out of sight; the type
    // comes from the keys update_emblems() cached, as reading it can throw.
    auto collector_dead = [&] {
        const auto& allies = seen::roster(Team::ally);
        for (std::size_t i = 0; i < allies.size() && i < ally_keys.size(); ++i)
            if (ally_keys[i] && *ally_keys[i] + 1 == who) return !allies[i].is_alive();
        return false;
    };
    if (capture != NO_CAPTURE && (capture >= static_cast<int>(route.size()) || ready_at[capture] > now
                                  || ((now - when) & 0xFF) > CAPTURE_TIMEOUT || (FIX_CLAIM && collector_dead())))
        capture = NO_CAPTURE;
    if (capture != NO_CAPTURE || rewards.empty()) return;

    const auto position = me.position();
    int s = *rewards.begin();
    for (const int r : rewards)
        if (distance(position, centers[r]) < distance(position, centers[s])) s = r;
    // Only a fresh sighting can claim this group again, so a stale record can't re-send a collector.
    rewards.erase(s);
    const auto& tiles = spawn_lists[s];
    auto nearest_of = [&](const bc::champion& a) {
        int nearest = 0;
        bool first = true;
        for (const auto t : tiles) {
            const int d = distance(a.position(), t);
            if (first || d < nearest) nearest = d;
            first = false;
        }
        return nearest;
    };
    // v71: an ally about to recall or carrying the escorted bag is going elsewhere, so it is not asked.
    auto free = [&](const bc::champion& a) {
        // v86: the split pair captures its own rewards.
        if (V86_SPLIT && phase == PHASE_SPLIT && a != me) {
            const auto kind = a.champion_name();
            if (kind == Champ::cassi || kind == Champ::issac) return false;
        }
        if (!FIX_CLAIM || a == me) return true;
        return a.health() >= a.max_health() * RECALL_HEALTH && champ_id(a.champion_name()) + 1 != carrier;
    };
    std::vector<bc::champion> allies;
    for (const auto& a : seen::champions(Team::ally))
        if (!channelling(a) && free(a)) allies.push_back(a);
    if (std::find(allies.begin(), allies.end(), me) == allies.end()) allies.push_back(me);
    std::vector<bc::champion> builders;
    for (const auto& a : allies)
        if (vesper_funnel(a) && (!FIX_CLAIM || nearest_of(a) <= VESPER_CLAIM_RANGE)) builders.push_back(a);
    // Vesper collects first while loot is funnelled to her (v71: only when she is near it); otherwise the poorest
    // visible ally, so the team catches up evenly. v71: the nearest, with CLAIM_WORTH_PER_TILE of item worth
    // counted as a tile, so the reward is not left waiting for someone across the map.
    const auto& pool = builders.empty() ? allies : builders;
    auto key = [&](const bc::champion& a) {
        const int nearest = nearest_of(a);
        if (FIX_CLAIM) return std::make_pair(nearest * CLAIM_WORTH_PER_TILE + item_worth(a), nearest);
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
    NOTE(format("c%d claims s%d's reward (nearest, poorest)", name, s));
    capture = s;
    who = name + 1;
    when = now;
}

// Shared decision

void Bot::sync_decision(const bc::champion& me) {
    auto shared = read_shared();
    const int now = static_cast<int>(bc::round_number());
    if (shared.mode != mode) mode_changed = now;
    // Only Vesper can read her own items, so she raises the shared flag once her core build is done.
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
    // v86: the phase and the pair's target; a split that ended since our last turn starts the cooldown.
    if (phase_seen == PHASE_SPLIT && shared.phase != PHASE_SPLIT) split_end_round = now;
    phase_seen = shared.phase;
    phase = V86_ZONES && zones_known ? shared.phase : PHASE_GROUP;
    target2 = shared.target2;
    zone_cap = zone_cap_for(phase);
    pair_filter = phase == PHASE_SPLIT ? (in_pair() ? 2 : 1) : 0;
    main_slot = route.empty() ? -1 : shared.slot % static_cast<int>(route.size());
    // Without a leader this round, last round's decision stands.
    update_rejoining(me, shared.slot);
    const auto block = lead_block(me, shared.stamp, now, shared.slot);
    led = !block;
    if (!led) {
        mode = shared.mode;
        slot = shared.slot;
        aggressive = shared.aggressive;
        capture_slot = shared.capture_slot;
        collector = shared.collector;
        claimed = shared.claimed;
        // v90-log: a follower's shared line repeated the leader's every turn; only the split pair's is worth it.
        LOG(if (pair_split()) shared_why = "split pair: own target, " + *block;);
        // v86 (V86_SPLIT): the pair works its own target; without a leader for two rounds the split is over.
        if (pair_split() && me.is_alive()) {
            if (shared.stamp != stamp_of(now) && shared.stamp != stamp_of(now - 1))
                end_split("no leader for two rounds");
            else
                play_pair(me);
        }
        return;
    }

    // First champion with the team this round settles the capture, picks the target from its own
    // respawn record, then scores. A dead carrier's escort is called off, as is one running too long.
    if (carrier && (!carrier_alive() || now - carrier_since > ESCORT_TIMEOUT)) {
        NOTE(carrier_alive() ? "escort called off: it ran too long" : "escort called off: the carrier died");
        carrier = 0;
    }
    // Aggressive once Vesper has her core build (or the draft did not get her) and the team owns
    // TEAM_AGGRESSIVE_EMBLEMS, with the whole team alive.
    const bool aggressive_now = (shared.vesper_done || !drafted_vesper()) && team_emblems() >= TEAM_AGGRESSIVE_EMBLEMS
        && team_alive();
    if (aggressive_now != aggressive)
        NOTE(aggressive_now ? "aggressive route on: vesper built, team emblems reached, all alive"
                            : (team_alive() ? "aggressive route off" : "aggressive route off: an ally is dead"));
    aggressive = aggressive_now;
    int s = route.empty() ? 0 : shared.slot % static_cast<int>(route.size());
    int capture = shared.capture_slot, who = shared.collector, when = shared.claimed;
    int new_phase = phase;
    std::string phase_why;
    if (!route.empty()) {
        update_capture(me, capture, who, when);
        // The escort altar is picked from the target, so the target holds while an escort runs.
        // v86: the phase decides which zones the pick may use (and a split's or a raid's target).
        if (!carrier && V86_ZONES && zones_known) {
            new_phase = choose_phase(me, s, shared.phase, s, capture, phase_why);
        } else if (!carrier) {
            s = pick_target(s, me.position(), capture);
        } else if (V86_ZONES && zones_known) {
            new_phase = !V86_CENTRE || centre_ok(me, shared.phase) ? PHASE_CENTRE : PHASE_GROUP;
            LOG(phase_why = "phase held for the escort";);
        }
        // v79 (PING_RALLY): point the team at a fresh ping it can answer; farming champions walk to the target.
        if (FIX_PING && PING_RALLY && ping_slot && !carrier && !spawn_zone.is_set(centers[*ping_slot])) {
            int helpers = 0;
            for (const auto& ally : seen::champions(Team::ally))
                if (can_answer(ally, now)) ++helpers;
            if (helpers >= RALLY_HELPERS) {
                if (s != *ping_slot) NOTE(format("rally to the ping at s%d: %d allies can answer", *ping_slot, helpers));
                s = *ping_slot;
            }
        }
    }
    // Fighting outranks the escort; the escort outranks farming.
    // v78 (PING_SCORE): a fresh ping near us counts in the score rather than forcing a fight.
    ping_bonus = FIX_PING && PING_SCORE && ping_slot && distance(me.position(), centers[*ping_slot]) <= PING_RANGE;
    int new_mode = score_mode(route.empty() ? std::nullopt : std::optional<vec2>{centers[s]}, shared.mode);
    // A fresh switch between fight and retreat holds for a few rounds before it can flip back.
    std::string hyst;
    const bool flip = (new_mode == FIGHT && shared.mode == RETREAT) || (new_mode == RETREAT && shared.mode == FIGHT);
    if (flip && now - mode_changed < MIN_HOLD) {
        LOG(hyst = format("keeps %s: switched %d rounds ago (hold %d)", MODE_NAMES[shared.mode], now - mode_changed, MIN_HOLD););
        new_mode = shared.mode;
    }
    ping_bonus = false;
    std::string ping_note;
    if (FIX_PING && !PING_SCORE && ping_slot && new_mode != FIGHT && !win_tribute
        && distance(me.position(), centers[*ping_slot]) <= PING_RANGE) {
        LOG(ping_note = format("ping at s%d calls a fight", *ping_slot););
        new_mode = FIGHT;
    }
    if (new_mode != shared.mode) mode_changed = now;
    std::string escort_note;
    if (new_mode == FARM && carrier) {
        LOG(escort_note = "an escort turns farm into tribute";);
        new_mode = TRIBUTE;
    }
    if (win_tribute && new_mode != FIGHT) new_mode = TRIBUTE;
    mode = new_mode;
    slot = s;
    capture_slot = capture;
    collector = who;
    claimed = when;
    // v86: a new split starts without a pair target (the pair picks one); the phase rides in the shared write.
    if (new_phase != shared.phase) {
        NOTE(format("phase %s -> %s: ", PHASE_NAMES[shared.phase], PHASE_NAMES[new_phase]) + phase_why);
    }
    if (new_phase == PHASE_SPLIT && shared.phase != PHASE_SPLIT) write_target2(NO_TARGET2);
    phase = new_phase;
    zone_cap = zone_cap_for(phase);
    pair_filter = phase == PHASE_SPLIT ? (in_pair() ? 2 : 1) : 0;
    main_slot = slot % std::max(1, static_cast<int>(route.size()));
    write_shared(mode, slot, capture, who, when, shared.vesper_done, aggressive, carrier, phase);
    // v86: log text only (debug builds; release skips it): ~3k fuel a turn.
    LOG({
        // "lead s6 (13,26) -> fight: <score>", naming the final mode first only when it is not the score's own.
        std::string text = "lead " + slot_text() + " -> ";
        if (score_why.rfind(MODE_NAMES[mode], 0) != 0) text += std::string(MODE_NAMES[mode]) + ", ";
        text += score_why;
        for (const auto* extra : {&hyst, &escort_note, &ping_note})
            if (!extra->empty()) text += "; " + *extra;
        if (!phase_why.empty() && new_phase == shared.phase) text += "; " + phase_why;
        const auto tags = shared_tags();
        if (!tags.empty()) text += " | " + tags;
        // The same decision as the last time we led is not printed again; the score's distance term moves every round,
        // so the comparison leaves the numbers out.
        std::string key;
        for (const char ch : text)
            if (!(ch >= '0' && ch <= '9') && ch != '.' && ch != '-') key += ch;
        if (key != lead_logged) shared_why = text;
        lead_logged = key;
    });
}

bool Bot::is_collector(const bc::champion& me) const {
    return capture_slot != NO_CAPTURE && collector == champ_id(me.champion_name()) + 1;
}

void Bot::release_claim(const char* why) {
    // Back into our own record, so whoever leads next with it in sight can hand it to someone free.
    if (capture_slot == NO_CAPTURE) return;
    NOTE(format("gives back the claim on s%d: ", capture_slot) + why);
    if (capture_slot < static_cast<int>(route.size())) rewards.insert(capture_slot);
    set_shared_byte(CAPTURE_SLOT_BYTE, NO_CAPTURE);
    capture_slot = NO_CAPTURE;
}

bool Bot::enemy_threat(const bc::champion& me) const {
    const auto position = me.position();
    for (const auto& enemy : seen::champions(Team::enemy))
        if (distance(position, enemy.position()) <= enemy.attack_range()) return true;
    return hidden_active() && hidden_zone.is_set(position);
}

void Bot::vesper_claim(const bc::champion& me) {
    // The leader may not see Vesper, so while loot is funnelled to her she claims nearby loot herself; the
    // old collector moves on.
    if (!vesper_funnel(me) || is_collector(me) || under_threat(me) || route.empty()) return;
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
    NOTE(format("vesper claims s%d (loot goes to her while she builds)", s));
}

void Bot::self_claim(const bc::champion& me) {
    // The leader only claims what it sees itself, so a reward in our own sight could wait for it to pass by.
    // With no claim open, no enemy near and nobody visible nearer to it, we claim it ourselves; a funnelled
    // Vesper in sight claims her own (vesper_claim()).
    if (capture_slot != NO_CAPTURE || rewards.empty() || route.empty() || channelling(me) || enemy_threat(me)) return;
    if (!near_enemies().empty()) return;
    const auto position = me.position();
    int s = -1;
    for (const int r : rewards)
        if (r < static_cast<int>(route.size()) && distance(position, centers[r]) <= SELF_CLAIM_RANGE
            && (s < 0 || distance(position, centers[r]) < distance(position, centers[s])))
            s = r;
    if (s < 0) return;
    const int now = static_cast<int>(bc::round_number());
    if (ready_at[s] > now) return;
    auto nearest_of = [&](vec2 from) {
        int best = distance(from, spawn_lists[s].front());
        for (const auto t : spawn_lists[s]) best = std::min(best, distance(from, t));
        return best;
    };
    const int mine = nearest_of(position);
    for (const auto& ally : seen::champions(Team::ally)) {
        if (ally == me || channelling(ally)) continue;
        const int theirs = nearest_of(ally.position());
        if (theirs < mine || (vesper_funnel(ally) && theirs <= VESPER_CLAIM_RANGE)) return;
    }
    rewards.erase(s);
    capture_slot = s;
    collector = champ_id(me.champion_name()) + 1;
    claimed = now;
    set_shared_byte(CAPTURE_SLOT_BYTE, s);
    set_shared_byte(CAPTURE_SLOT_BYTE + 1, collector);
    set_shared_byte(CLAIM_ROUND_OFFSET, now & 0xFF);
    NOTE(format("claims s%d itself: nobody nearer, no claim open", s));
}

bool Bot::collect(const bc::champion&) {
    // Stand on the cleared group until its reward lands, then release the claim for everyone.
    const int index = capture_slot;
    if (index >= static_cast<int>(route.size())) return false;
    if (ready_at[index] > static_cast<int>(bc::round_number())) {
        set_shared_byte(CAPTURE_SLOT_BYTE, NO_CAPTURE);
        capture_slot = NO_CAPTURE;
        NOTE(format("claim on s%d over: the group is respawning", index));
        return false;
    }
    if (!camps_at(index).empty()) {
        NOTE(format("collector of s%d, but its camps are still up: go on", index));
        return false;
    }
    const auto& tiles = spawn_board(index);
    if (!FIX_CAMP_COLLECT) {
        WHY("collect.loot", format("collector of s%d: walk onto the reward", index));
        move_towards_closest(tiles);
        return true;
    }
    // v71: a camp hit does not stop the capture, only an empty or contested region does. A neighbouring
    // group's awake camps may reach the reward, so a tile out of their reach is taken when there is one;
    // with none, a healthy collector holds the reward anyway and a hurt one gives the claim back.
    const auto& me = bc::self();
    const auto position = me.position();
    const bc::bitboard* goals = &tiles;
    bc::bitboard reach, safe;
    bool awake = false;
    for (const auto& camp : seen::camps())
        if (camp.strength() > 0 && camp.health() < camp.max_health()) {
            reach.draw_chebyshev(camp.position(), static_cast<float>(camp.attack_range()));
            awake = true;
        }
    const char* how = "walk onto the reward";
    if (awake) {
        safe = tiles;
        safe &= ~reach;
        if (!safe.is_empty()) {
            goals = &safe;
            how = "walk onto a reward tile out of awake camps' reach";
        } else if (me.health() < me.max_health() * COLLECT_CAMP_HEALTH) {
            release_claim("every reward tile is in awake camps' reach and we are too hurt to hold it");
            return false;
        } else {
            how = "hold the reward under camp fire (healthy enough)";
        }
    }
    // An ally already on the reward is capturing it (allies split it, so a second body adds nothing): the
    // claim is done. Vesper still joins while loot is funnelled to her.
    if (!tiles.is_set(position) && !vesper_funnel(me)) {
        auto held = tiles;
        held &= bc::vision::champion_tiles(Team::ally);
        if (!held.is_empty()) {
            release_claim("an ally already stands on the reward");
            rewards.erase(index);
            return false;
        }
    }
    WHY("collect.loot", format("collector of s%d: ", index) + how);
    move_towards_closest(*goals);
    return true;
}

int Bot::score_mode(std::optional<vec2> origin, int current) {
    // Enemies further from origin (the target, or our own tile) count for less.
    const auto& enemies = near_enemies();
    if (enemies.empty()) {
        LOG(score_why = far_skipped ? format("farm: the %d enemies in sight are over %d steps away", far_skipped, FIGHT_STEPS)
                                    : std::string("farm: no enemy in sight"););
        return FARM;
    }
    // v75 (FIX_FIGHT_BODIES): only allies who can join this fight count: not channelling (a recalling or
    // tributing Ren still carried his emblems, health and power into the score) and within FIGHT_ALLY_RANGE of
    // the enemy nearest the fight. Bodies, health, power and emblems all use the same set.
    std::vector<bc::champion> fighters;
    std::vector<double> fighter_weight;
    const auto& seen_allies = seen::champions(Team::ally);
    if (FIX_FIGHT_BODIES) {
        const vec2 here = origin ? *origin : bc::self().position();
        vec2 front = enemies[0].position();
        for (const auto& e : enemies)
            if (distance(e.position(), here) < distance(front, here)) front = e.position();
        for (const auto& a : seen_allies) {
            if (channelling(a) || a.is_teleporting()) continue;
            const int d = distance(a.position(), front);
            const int gap = FIGHT_WEIGHTING == 1 ? d - static_cast<int>(a.attack_range()) : d;
            const int limit = FIGHT_WEIGHTING == 2 ? SOFT_FAR : FIGHT_ALLY_RANGE;
            if (gap <= limit) {
                fighters.push_back(a);
                fighter_weight.push_back(FIGHT_WEIGHTING == 2 && d > SOFT_NEAR
                                             ? static_cast<double>(SOFT_FAR - d) / (SOFT_FAR - SOFT_NEAR) : 1.0);
            }
        }
    }
    const auto& allies = FIX_FIGHT_BODIES ? fighters : seen_allies;
    // v81: everything but the distance and the ping bonus is the same for every origin, so it is worked out once per
    // round and vision list (score_mode runs twice on a leader's fight turn: the shared decision, then step 5) and
    // reused with its reason text. Fight bodies depend on the origin, so they are never cached.
    const bool cacheable = !FIX_FIGHT_BODIES;
    const auto round = bc::round_number();
    const bool hit = cacheable && score_base.round == round && score_base.enemies == near_src && score_base.allies == &seen_allies
        && score_base.valid;
    if (!hit) {
        const int ne = static_cast<int>(enemies.size());
        auto weight = [&](std::size_t i) { return FIX_FIGHT_BODIES && i < fighter_weight.size() ? fighter_weight[i] : 1.0; };
        double na_w = 0.0;
        for (std::size_t i = 0; i < allies.size(); ++i) na_w += weight(i);
        const int na = static_cast<int>(allies.size());

        double head = 0.0, tail = 0.0;
        // The reason text: each term's points, and the head count (v90-log: only inside LOG; no health or power totals).
        std::string head_text, tail_text;
        auto part = [](std::string& joined, const std::string& text) {
            if (!joined.empty()) joined += ", ";
            joined += text;
        };
        if (far_skipped) LOG(part(head_text, format("%d far enemies left out", far_skipped)););
        if (na_w > ne) {
            head += OUTNUMBER_BONUS;
            LOG(part(head_text, format("bodies +%d %dv%d", OUTNUMBER_BONUS, na, ne)););
        } else {
            const int pen = static_cast<int>(MISSING_BODY_PENALTY * (ne - na_w) + 0.5);
            head -= pen;
            LOG(part(head_text, format("bodies -%d %dv%d", pen, na, ne)););
        }
        PySum ally_hp, enemy_hp;
        for (std::size_t i = 0; i < allies.size(); ++i) ally_hp.add(allies[i].health() * weight(i));
        for (const auto& e : enemies) enemy_hp.add(e.health());
        const double ally_health = ally_hp.value(), enemy_health = enemy_hp.value();
        const int hp_edge = ally_health > enemy_health ? HEALTH_EDGE : -HEALTH_EDGE;
        head += hp_edge;
        LOG(part(head_text, format("health %+d", hp_edge)););
        PySum ally_pw, enemy_pw;
        for (std::size_t i = 0; i < allies.size(); ++i) ally_pw.add((static_cast<double>(allies[i].strength()) + allies[i].focus()) * weight(i));
        for (const auto& e : enemies) enemy_pw.add(static_cast<double>(e.strength()) + e.focus());
        const double ally_power = ally_pw.value(), enemy_power = enemy_pw.value();
        const int power_edge = ally_power > enemy_power ? POWER_EDGE : -POWER_EDGE;
        head += power_edge;
        LOG(part(head_text, format("power %+d", power_edge)););
        double owned_w = 0.0;
        for (std::size_t i = 0; i < allies.size(); ++i) owned_w += emblem_count(allies[i]) * weight(i);
        const int emblem_score = static_cast<int>(EMBLEM_BONUS * owned_w + 0.5);
        head += emblem_score;
        if (emblem_score) LOG(part(head_text, format("emblems +%d", emblem_score)););
        if (!allies.empty() && squishy_overextended(allies, enemies)) {
            tail -= OVEREXTEND_PENALTY;
            LOG(part(tail_text, format("squishy in front -%d", OVEREXTEND_PENALTY)););
        }
        if (seen::ally_spawn().is_set(bc::self().position())) {
            tail += SPAWN_BONUS;
            LOG(part(tail_text, format("on our spawn +%d", SPAWN_BONUS)););
        }
        const auto& enemy_roster = seen::roster(Team::enemy);
        int dead_enemies = 0;
        for (const auto& enemy : enemy_roster)
            if (!enemy.is_alive()) ++dead_enemies;
        const int kill_window = kill_window_dead_enemies(enemy_roster.size());
        if (KILL_WINDOW && kill_window >= 0 && dead_enemies >= kill_window) {
            tail -= KILL_WINDOW_FIGHT_PENALTY;
            LOG(part(tail_text, format("kill window -%d (%d dead)", KILL_WINDOW_FIGHT_PENALTY, dead_enemies)););
        }
        // v86 (Mike): DEAD_ENEMY_SCORE per dead enemy (plus: press the man advantage; minus: farm it).
        if (DEAD_ENEMY_SCORE != 0 && dead_enemies > 0) {
            tail += DEAD_ENEMY_SCORE * dead_enemies;
            LOG(part(tail_text, format("enemies dead %+d", DEAD_ENEMY_SCORE * dead_enemies)););
        }
        score_base = {round, near_src, &seen_allies, head, tail, std::move(head_text), std::move(tail_text), true};
        if (!cacheable) score_base.valid = false;
    }

    // Same terms, same order as before: the cached head, the distance, the cached tail, then the ping.
    double score = score_base.head;
    double dist = 0;
    if (origin) {
        int nearest = distance(enemies[0].position(), *origin);
        for (const auto& e : enemies) nearest = std::min(nearest, distance(e.position(), *origin));
        dist = DISTANCE_PENALTY * nearest;
        score -= dist;
    }
    score += score_base.tail;
    if (ping_bonus) score += PING_SCORE_BONUS;
    const int bar = current == FIGHT ? FIGHT_THRESHOLD - FIGHT_HYSTERESIS : FIGHT_THRESHOLD;
    const int choice = score >= bar ? FIGHT : RETREAT;
    LOG({
        std::string joined = score_base.head_text;
        auto part = [&](const std::string& text) {
            if (!joined.empty()) joined += ", ";
            joined += text;
        };
        if (origin) part(format("distance -%.1f", dist));
        if (!score_base.tail_text.empty()) part(score_base.tail_text);
        if (ping_bonus) part(format("ping +%d", PING_SCORE_BONUS));
        score_why = format("%s %.1f %s %d (", MODE_NAMES[choice], score, choice == FIGHT ? ">=" : "<", bar) + joined + ")";
    });
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
