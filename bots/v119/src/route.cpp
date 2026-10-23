// Map setup, the shared route, and the fight-or-farm score.
#include "bot.h"
#include "draft.h"
#include "seen.h"
#include "state.h"
#include <algorithm>
#include <cmath>
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
    parts[0].reserve(indexes.size());
    parts[1].reserve(indexes.size());
    // v105 (STRONG_EARLY): the team starts with no emblems, so the strong camps carry the whole extra weight.
    priced_extra = STRONG_EARLY ? STRONG_EARLY_EXTRA : 0;
    loads.reserve(indexes.size());
    gemless.reserve(indexes.size());
    bool gem_income = false;
    // v99: one pass over each group's camps (group_stats) instead of five, the spawn lists viewed in place rather
    // than copied, and the width read once: this loop was ~90k of a ~220k setup on islands, which spilled the first
    // turn (200k) into the second.
    const std::size_t width = static_cast<std::size_t>(bc::map::width());
    for (const int index : indexes) {
        const int s = static_cast<int>(route.size());
        const auto& group = groups[index];
        route.push_back(&group);
        spawn_lists.push_back(data.spawn_lists[index]);
        for (const auto tile : spawn_lists.back()) {
            const auto bit = static_cast<std::size_t>(tile.y) * width + tile.x;
            slot_grid[bit] = static_cast<std::int16_t>(s);
            route_bits[bit / 8] |= static_cast<std::uint8_t>(1u << (bit % 8));
        }
        spawns.emplace_back();
        const auto stats = draft::group_stats(group);
        if (stats.monolith) monoliths.insert(s);
        if (!stats.monolith && group.gem_value() > 0) gem_income = true;
        centers.push_back(data.centers[index]);
        parts[0].push_back(draft::value_parts(group, draft::FARM_GOLD_WEIGHT, stats));
        parts[1].push_back(draft::value_parts(group, draft::GOLD_WEIGHT, stats));
        values[0].push_back(draft::value_of(parts[0].back(), priced_extra));
        values[1].push_back(draft::value_of(parts[1].back(), priced_extra));
        clears.push_back(draft::clear_rounds(stats));
        loads.push_back(draft::group_load(stats));
        gemless.push_back(group.gem_value() == 0);
        int elks = 0, golems = 0, strong = 0;
        for (const auto& camp : group.camps()) {
            if (camp.kind() == bc::camp_type::elk) ++elks;
            if (camp.kind() == bc::camp_type::golem) ++golems;
            if (camp.kind() != bc::camp_type::gem_cluster && camp.kind() != bc::camp_type::gold_cluster) ++strong;
        }
        const int need = static_cast<int>(std::ceil(elks * ELK_EMBLEMS + golems * GOLEM_EMBLEMS - 1e-9));
        hard_need.push_back(need);
        clusters_only.push_back(strong == 0 && !group.camps().empty());
    }
    route_tiles.import_linear_buffer(route_bits);
    route_tile_count = route_tiles.count();
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
        if (const auto zones_at = draft::zones_block(memory, draft::setup_end(memory, offset))) {
            // v99: read straight from the block (3 bytes per group, see draft::decode_zones) into reserved lists; the
            // decoded copy and unreserved pushes cost ~37k of setup on islands.
            zone_of.reserve(indexes.size());
            pair_ok.reserve(indexes.size());
            home_steps.reserve(indexes.size());
            enemy_steps.reserve(indexes.size());
            for (const int index : indexes) {
                const auto at = *zones_at + 3 * static_cast<std::size_t>(index);
                zone_of.push_back(static_cast<std::uint8_t>(memory[at] & 3));
                pair_ok.push_back((memory[at] & 4) != 0 ? 1 : 0);
                home_steps.push_back(memory[at + 1]);
                enemy_steps.push_back(memory[at + 2]);
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
    ally_spawn_list = bc::map::spawn_region(Team::ally).positions();
    enemy_spawn_list = theirs.positions();
    spawn_gap.reset();
    for (const auto a : ally_spawn_list)
        for (const auto b : enemy_spawn_list) {
            const int d = distance(a, b);
            if (!spawn_gap || d < *spawn_gap) spawn_gap = d;
        }
    // v119 (RING_SCALE): the ring grows with the distance between the spawns.
    const int ring = RING_SCALE && spawn_gap ? std::clamp(*spawn_gap / RING_DIV, RING_MIN, RING_MAX) : SPAWN_CHASE_RANGE;
    LOG(say(format("setup: spawn gap %d, chase ring %d", spawn_gap ? *spawn_gap : -1, ring)););
    spawn_zone.clear();
    for (const auto tile : enemy_spawn_list) spawn_zone.draw_chebyshev(tile, static_cast<float>(ring));
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
    if (!FIX_ALTAR_GATE) return;
    // A few tile reads per altar, so worked out here rather than passed through draft memory.
    // v99: the neighbours are read directly (no board and tile list per altar); the gate is the first group in engine
    // tile order (lowest fast::rank), as the positions() walk found it.
    const int range = bc::constants::submit_gems_range;
    const auto& closed = seen::fixed_blocks();
    const int width = bc::map::width(), height = bc::map::height();
    for (const auto altar : altars) {
        int gate = -1;
        std::uint32_t gate_rank = 0;
        bool free_tile = false;
        for (int dy = -range; dy <= range; ++dy)
            for (int dx = -range; dx <= range; ++dx) {
                const vec2 tile{altar.x + dx, altar.y + dy};
                if (tile.x < 0 || tile.y < 0 || tile.x >= width || tile.y >= height || closed.is_set(tile)) continue;
                const int s = slot_at(tile);
                if (s < 0) {
                    free_tile = true;
                } else if (gate < 0 || fast::rank(tile) < gate_rank) {
                    gate = s;
                    gate_rank = fast::rank(tile);
                }
            }
        gate_altars.push_back(altar);
        gate_slots.push_back(free_tile ? -1 : gate);
        if (!free_tile && gate >= 0) LOG(say(format("setup: altar %s is gated by s%d", at(altar).c_str(), gate)););
    }
}

bool Bot::altar_open(vec2 altar) const {
    // A gated altar opens once its group is down: respawning, or cleared with its reward waiting.
    for (std::size_t i = 0; i < gate_altars.size(); ++i) {
        if (gate_altars[i] != altar) continue;
        const int g = gate_slots[i];
        if (g < 0 || g >= static_cast<int>(ready_at.size())) return true;
        const int now = static_cast<int>(bc::round_number());
        if (ready_at[g] > now || rewards.count(g) > 0) return true;
        return g >= static_cast<int>(seen_up.size()) || now - seen_up[g] > GATE_MEMORY;
    }
    return true;
}

bool Bot::all_altars_shut() const {
    if (gate_altars.empty()) return false;
    for (const auto altar : gate_altars)
        if (altar_open(altar)) return false;
    return true;
}

// Route memory

void Bot::observe() {
    // Vision is not shared, so each champion keeps its own respawn record.
    // One mask over every route spawn tile finds the groups in sight without a pass over all of them.
    // v99: into a reused board and tile buffer (the board, the & and positions() were three allocations a turn).
    static bc::bitboard visible{bc::uninitialized};
    static std::vector<vec2> tiles;
    bc::vision::visible_tiles_into(visible);
    visible &= route_tiles;
    if (visible.is_empty()) return;
    const int now = static_cast<int>(bc::round_number());
    // Ascending and without repeats, like a set, in a reused vector (a set allocates per insert).
    auto& slots = slot_scratch;
    slots.clear();
    if (tiles.size() < route_tile_count) tiles.resize(route_tile_count);
    const auto tile_n = std::min<std::size_t>(visible.positions_into(tiles), tiles.size());
    for (std::size_t t = 0; t < tile_n; ++t) slots.push_back(slot_at(tiles[t]));
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
        if (FIX_ALTAR_GATE && state == 1) {
            if (seen_up.size() != route.size()) seen_up.assign(route.size(), -GATE_MEMORY - 1);
            seen_up[index] = now;
        }
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

void Bot::reprice() {
    // v105 (STRONG_EARLY): the strong camps' extra health cost fades with the team's emblems; values[] is only worked
    // out again when the weight changes (an emblem bought), a pass of arithmetic over the slots.
    const auto team = std::max<std::size_t>(1, seen::roster(Team::ally).size());
    const double owned = static_cast<double>(team_emblems()) / (STRONG_FADE_EMBLEMS * static_cast<double>(team));
    const double extra = STRONG_EARLY_EXTRA * std::max(0.0, 1.0 - owned);
    if (extra == priced_extra) return;
    priced_extra = extra;
    for (int k = 0; k < 2; ++k)
        for (std::size_t s = 0; s < parts[k].size(); ++s) values[k][s] = draft::value_of(parts[k][s], extra);
    LOG(say(format("reprice: strong camps' health cost x%.2f (%d team emblems)", 1 + extra, team_emblems())););
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

int Bot::monolith_emblems() const {
    if (!MONOLITH_BY_SIZE) return MONOLITH_EMBLEMS;
    const auto team = seen::roster(Team::ally).size();
    return team <= 3 ? MONOLITH_EMBLEMS_3 : team == 4 ? MONOLITH_EMBLEMS_4 : MONOLITH_EMBLEMS_5;
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
    return emblem_count(champion) < VESPER_FUNNEL_EMBLEMS;
}

bool Bot::marcella_collects(const bc::champion& champion) const {
    // v113 (VARG_CLAIM): Varg, drafted in her place, collects as she does.
    const auto kind = champion.champion_name();
    if (!MARCELLA_CLAIM || !(kind == Champ::marcella || (VARG_CLAIM && kind == Champ::varg))) return false;
    return MARCELLA_FUNNEL_EMBLEMS == 0 || emblem_count(champion) < MARCELLA_FUNNEL_EMBLEMS;
}

int Bot::hard_left(int s) const {
    // Emblems the group needs past the free part (HARD_FREE, the team size, one per HARD_DECAY_ROUNDS rounds).
    int free = HARD_FREE;
    if (HARD_FREE_TEAM) free += static_cast<int>(seen::roster(Team::ally).size());
    if (HARD_DECAY_ROUNDS > 0) free += static_cast<int>(bc::round_number()) / HARD_DECAY_ROUNDS;
    return std::max(0, hard_need[s] - free);
}

bool Bot::hard_ok(int s) const {
    if (!HARD_GATE || s >= static_cast<int>(hard_need.size())) return true;
    const int left = hard_left(s);
    return left == 0 || left <= std::max(team_emblems(), hard_floor);
}

void Bot::drop_hard(Travel& travel) {
    // v114 (HARD_GATE): groups the team is not built for leave the table; if that would leave no group but monoliths,
    // the ones needing the fewest emblems stay (hard_floor), so the team farms the easiest instead of standing idle.
    hard_floor = -1;
    if (!HARD_GATE) return;
    const int owned = team_emblems();
    int easiest = -1;
    bool open = false;
    for (int s = 0; s < static_cast<int>(travel.steps.size()); ++s) {
        if (!travel.has(s) || monoliths.count(s)) continue;
        const int left = hard_left(s);
        if (left <= owned) {
            open = true;
            break;
        }
        if (easiest < 0 || left < easiest) easiest = left;
    }
    if (!open && easiest >= 0) hard_floor = easiest;
    for (int s = 0; s < static_cast<int>(travel.steps.size()); ++s) {
        if (!travel.has(s) || monoliths.count(s) || hard_ok(s)) continue;
        travel.steps[s] = -1;
        --travel.reachable;
    }
}

bool Bot::allowed(int s) {
    if (!hard_ok(s)) return false;
    if (!monoliths.count(s)) return true;
    // v81 (MAGE_MONOLITH): the mages take monoliths on their own job; the shared route only points at one on a
    // gem-starved map, under the usual gates (strong and grouped: the area is contested), and then the team holds
    // the area around it (melees guard, mages cast).
    if (monolith_delegated() && !gem_starved) return false;
    return team_emblems() >= monolith_emblems() && monolith_grouped(s);
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
    return route_rate_steps(s, steps, static_cast<int>(bc::round_number()));
}

double Bot::route_rate_steps(int s, int steps, int now) const {
    const int wait = std::max(0, ready_at[s] - now - steps);
    const double cycle = steps + wait + clears[s] + bc::constants::capture_time;
    return values[aggressive][s] / std::max(1.0, cycle);
}

int Bot::best_goal(const Travel& travel) const {
    // On a gem-starved map the monolith decides the game (gems before gold), so once its gates let it into
    // the travel table it is the goal outright.
    if (gem_starved)
        for (const int m : monoliths)
            if (travel.has(m)) return m;
    // v98 (FIX_ALTAR_GATE): with every altar shut, the gate group is the goal once its gates let it into the table.
    if (FIX_ALTAR_GATE && all_altars_shut())
        for (const int g : gate_slots)
            if (g >= 0 && travel.has(g)) return g;
    // Planned pocket: gold-weighted while farming, the centre pocket once aggressive. Without one in the table, every
    // group in it, in slot order.
    // v99: two passes over the candidates instead of building two lists, and the round read once.
    const auto& core = cores[aggressive];
    bool in_core = false;
    // v104 (INVADE_TIMERS): once aggressive, every open group is rated with its respawn wait instead of the centre
    // pocket alone (its respawn waits were 40-55% of later turns on ambush and corridors).
    if (!(INVADE_TIMERS && aggressive))
        for (const int s : core)
            if (travel.has(s)) in_core = true;
    const int count = in_core ? static_cast<int>(core.size()) : static_cast<int>(travel.steps.size());
    auto candidate = [&](int i) { return in_core ? core[i] : i; };
    // v72 (FIX_GEM_FIRST): a pocket with gem groups aims only at those; its gold clusters are fillers for the
    // respawn waits (pick_target's wait branch), not goals.
    bool gems_only = false;
    if (FIX_GEM_FIRST)
        for (int i = 0; i < count && !gems_only; ++i) {
            const int s = candidate(i);
            if (travel.has(s) && !gemless[s]) gems_only = true;
        }
    const int now = static_cast<int>(bc::round_number());
    int best = -1;
    double best_rate = 0;
    for (int i = 0; i < count; ++i) {
        const int s = candidate(i);
        if (!travel.has(s) || (gems_only && gemless[s])) continue;
        const double rate = route_rate_steps(s, travel.at(s), now);
        if (best < 0 || rate > best_rate) {
            best = s;
            best_rate = rate;
        }
    }
    return best;
}

void Bot::drop_monoliths(Travel& travel) {
    // A monolith stays only when the team is already on it, or when no other group in reach is up within
    // MONOLITH_IDLE rounds of our arrival; either way the team must own monolith_emblems() and be grouped.
    bool any = false;
    for (const int m : monoliths)
        if (travel.has(m)) any = true;
    if (!any) return;
    const int now = static_cast<int>(bc::round_number());
    bool last_resort = true;
    if (!gem_starved)
        for (int s = 0; s < static_cast<int>(travel.steps.size()) && last_resort; ++s)
        if (travel.has(s) && !monoliths.count(s) && ready_at[s] - now - travel.at(s) <= MONOLITH_IDLE) last_resort = false;
    const bool strong = team_emblems() >= monolith_emblems();
    const bool delegated = monolith_delegated();
    const bool shut = FIX_ALTAR_GATE && all_altars_shut();
    for (const int m : monoliths) {
        if (!travel.has(m)) continue;
        // v98 (FIX_ALTAR_GATE): a monolith shutting every altar is worth taking as soon as we are strong enough.
        const bool gate = shut && std::find(gate_slots.begin(), gate_slots.end(), m) != gate_slots.end();
        if (!(delegated && !gem_starved) && strong && (last_resort || gate || mid_clear(m)) && monolith_grouped(m)) continue;
        travel.steps[m] = -1;
        --travel.reachable;
    }
}

Bot::Travel Bot::travel(vec2 origin, int skip) {
    // Walking steps from origin to every allowed route group except skip. Walking is symmetric, so once
    // every group has its own field a lookup at origin replaces a fresh search of the whole map.
    const int n = static_cast<int>(route.size());
    // v99: the raw walking steps depend only on origin, skip and which goal fields exist, so they are worked out once
    // a turn: the leader asks several times with the same origin (choose_phase, raid_target, the split check), and
    // before every field is built each ask was a whole-map field plus a read per spawn tile (~20k).
    const auto now = bc::round_number();
    if (!(travel_raw_round == now && travel_raw_origin == origin && travel_raw_skip == skip
          && travel_raw_fields == goal_field_count)) {
        auto& raw = travel_raw;
        raw.steps.assign(n, -1);
        raw.reachable = 0;
        // Every group but skip; walking is symmetric, so a group's own field read at origin is its distance.
        bool need_walk = false;
        for (int s = 0; s < n; ++s) {
            if (s == skip) continue;
            if (s >= static_cast<int>(goal_fields.size()) || !goal_fields[s]) {
                need_walk = true;
                continue;
            }
            const auto steps = goal_fields[s]->field.get(origin);  // unreachable marks "not reached"
            if (steps != bc::distance_field::unreachable) {
                raw.steps[s] = steps;
                ++raw.reachable;
            }
        }
        if (need_walk) {
            // One field and boards reused across calls (compute() writes every entry); a new one cost ~2k.
            static Walk walk;
            static bc::bitboard root{bc::uninitialized};
            fast::around_into(origin, 0, root);
            walk.field.compute(seen::terrain(), root, walk.reached);
            for (int s = 0; s < n; ++s)
                if (s != skip && (s >= static_cast<int>(goal_fields.size()) || !goal_fields[s]))
                    if (const auto steps = steps_to(walk, spawn_lists[s])) {
                        raw.steps[s] = *steps;
                        ++raw.reachable;
                    }
        }
        travel_raw_round = now;
        travel_raw_origin = origin;
        travel_raw_skip = skip;
        travel_raw_fields = goal_field_count;
    }
    Travel result = travel_raw;
    // Monoliths are then dropped unless they are the last resort.
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
    // v114 (HARD_GATE): last, so its fallback only looks at groups every other rule left open.
    drop_hard(result);
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
    auto loot = visible_loot(skip);
    if (seen_only && loot.empty()) return s;
    const auto steps = travel(origin, skip);
    if (steps.empty()) return s;

    // v99: the loot filter first, in place: a follower with no loot in the table (most share_loot calls) then skips
    // best_goal, which nothing before this return reads.
    std::erase_if(loot, [&](int l) { return !steps.has(l); });
    if (seen_only && loot.empty()) return s;
    const int dest = best_goal(steps);

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
        // v72: a gold cluster costs a clear and a five-round capture for 20 gold and no gems; ladder teams skip
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

    const int now = static_cast<int>(bc::round_number());
    const int wait = std::max(0, ready_at[dest] - now - steps.at(dest));
    if (wait == 0) {
        if (steps.has(s) && s != skip) {
            const double here = route_rate_steps(s, steps.at(s), now);
            if (here * SWITCH_MARGIN >= route_rate_steps(dest, steps.at(dest), now)) return s;
        }
        return dest;
    }
    // v104 (FIX_FILL): the goal is down. A group up by the time we get there is worth its gems less what the detour
    // costs the goal (the goal's rate x the rounds it pushes our arrival past the respawn, walking back included);
    // the best positive one fills the wait, else we go and wait. Rating the goal against the fillers by gems a round,
    // wait included, kept a big down group on top (00ambush: the team sat 50+ rounds at a 38-gem group with smaller
    // groups up nearby).
    if (FIX_FILL) {
        const double dest_rate = route_rate_steps(dest, steps.at(dest), now);
        const int until = std::max(ready_at[dest] - now, static_cast<int>(steps.at(dest)));
        const double capture = bc::constants::capture_time;
        // Scored per round of the detour, so a near group beats a far one of the same worth; the filler we are already
        // on keeps it unless another scores SWITCH_MARGIN more (re-picking every round walked the team between groups
        // 20-35 tiles apart).
        int best = -1;
        double best_score = 0, current = 0;
        for (int l = 0; l < static_cast<int>(steps.steps.size()); ++l) {
            if (l == dest || !steps.has(l) || ready_at[l] - now > steps.at(l)) continue;
            if (FIX_GEM_FIRST && gemless[l] && steps.at(l) > GOLD_ONLY_NEAR) continue;
            const auto back = gap(l, dest);
            if (!back) continue;
            const double busy = steps.at(l) + clears[l] + capture;
            const double delay = std::max(0.0, busy + *back - until);
            const double gain = values[aggressive][l] - dest_rate * delay;
            if (gain <= 0) continue;
            const double score = gain / busy;
            if (l == s) current = score;
            if (score > best_score) {
                best = l;
                best_score = score;
            }
        }
        if (current > 0 && current * SWITCH_MARGIN >= best_score) return s;
        return best >= 0 ? best : dest;
    }
    // The pocket is down, so spend the wait on the best group from here.
    int best = -1;
    double best_rate = 0;
    for (int l = 0; l < static_cast<int>(steps.steps.size()); ++l) {
        if (!steps.has(l)) continue;
        const double rate = route_rate_steps(l, steps.at(l), now);
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
    // v103 (FIX_POCKET): from a hole pocket only the pocket's groups are walkable, and the walkers cannot come.
    if (pocketed(me)) return "in a hole pocket";
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
    if (pair_split() || pocketed(me)) return;
    const int s = slot % static_cast<int>(route.size());
    const auto position = me.position();
    const int pick = pick_target(s, position, capture_slot, true);
    if (pick == s || distance(centers[pick], centers[s]) > distance(position, centers[s])) return;
    slot = pick;
    set_shared_byte(SLOT_BYTE, slot);
    NOTE(format("sees loot at s%d on the way: points the team there", slot));
}

bool Bot::walkable(int s, vec2 from) const {
    if (s < 0 || s >= static_cast<int>(goal_fields.size()) || !goal_fields[s]) return true;
    return goal_fields[s]->field.get(from) != bc::distance_field::unreachable;
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
    auto can_walk = [&](const bc::champion& a) { return !FIX_WALK_CLAIM || walkable(s, a.position()); };
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
        return a.health() >= a.max_health() * RECALL_HEALTH && cached_ident(a) != carrier;
    };
    std::vector<bc::champion> allies;
    for (const auto& a : seen::champions(Team::ally))
        if (!channelling(a) && free(a) && can_walk(a)) allies.push_back(a);
    if (std::find(allies.begin(), allies.end(), me) == allies.end() && can_walk(me)) allies.push_back(me);
    if (allies.empty()) {
        NOTE(format("nobody in sight can walk to s%d's reward: no claim", s));
        return;
    }
    // v101 (WILLOW_SAFE_CLAIM): Willow does not collect a risky reward while someone else in sight can.
    if (WILLOW_SAFE_CLAIM && allies.size() > 1 && willow_claim_risky(s))
        std::erase_if(allies, [](const bc::champion& a) { return a.champion_name() == Champ::willow; });
    else if (WILLOW_CLAIM_DROP && allies.size() > 1 && !seen::champions(Team::enemy).empty())
        std::erase_if(allies, [](const bc::champion& a) { return a.champion_name() == Champ::willow; });
    std::vector<bc::champion> builders;
    for (const auto& a : allies)
        if (vesper_funnel(a) && (!FIX_CLAIM || nearest_of(a) <= VESPER_CLAIM_RANGE)) builders.push_back(a);
    // Vesper collects first while loot is funnelled to her (v71: only when she is near it); otherwise the poorest
    // visible ally, so the team catches up evenly. v71: the nearest, with CLAIM_WORTH_PER_TILE of item worth
    // counted as a tile, so the reward is not left waiting for someone across the map.
    // v110 (MARCELLA_CLAIM): with no funnelled Vesper near, a Marcella near it collects second.
    if (builders.empty())
        for (const auto& a : allies)
            if (marcella_collects(a) && nearest_of(a) <= VESPER_CLAIM_RANGE) builders.push_back(a);
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
    const int name = cached_ident(pool[chosen]) - 1;
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
    // v117 (TEAM_CHASE): a chase the team stopped gaining on is dropped for a while.
    update_team_chase(me, shared.mode);
    if (new_mode == FIGHT && team_chase_off()) {
        LOG(hyst += (hyst.empty() ? "" : "; ") + std::string("chase dropped: farm"););
        new_mode = FARM;
    }
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
    return capture_slot != NO_CAPTURE && collector == cached_ident(me);
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
    // v110 (MARCELLA_CLAIM): Marcella does the same second: not over Vesper's claim, nor with a funnelled Vesper in
    // sight within VESPER_CLAIM_RANGE of us.
    const bool vesper = vesper_funnel(me);
    if (!vesper && !marcella_collects(me)) return;
    if (is_collector(me) || under_threat(me) || route.empty()) return;
    const auto position = me.position();
    if (!vesper) {
        if (capture_slot != NO_CAPTURE && collector == champ_id(Champ::vesper_vale) + 1) return;
        for (const auto& ally : seen::champions(Team::ally))
            if (vesper_funnel(ally) && distance(position, ally.position()) <= VESPER_CLAIM_RANGE) return;
    }
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
    collector = champ_id(me.champion_name()) + 1;
    claimed = now;
    set_shared_byte(CAPTURE_SLOT_BYTE, s);
    set_shared_byte(CAPTURE_SLOT_BYTE + 1, collector);
    set_shared_byte(CLAIM_ROUND_OFFSET, now & 0xFF);
    NOTE(format(vesper ? "vesper claims s%d (loot goes to her while she builds)" : "marcella claims s%d (no vesper near)", s));
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
            && (!FIX_WALK_CLAIM || walkable(r, position))
            && (s < 0 || distance(position, centers[r]) < distance(position, centers[s])))
            s = r;
    if (s < 0) return;
    if (WILLOW_SAFE_CLAIM && me.champion_name() == Champ::willow && willow_claim_risky(s)) return;
    if (WILLOW_CLAIM_DROP && me.champion_name() == Champ::willow && !seen::champions(Team::enemy).empty()) return;
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
        if (vesper_funnel(ally) && theirs <= VESPER_CLAIM_RANGE) return;
        // v110 (MARCELLA_CLAIM): Marcella ignores nearer allies; the rest leave it to a Marcella near it.
        if (marcella_collects(me)) continue;
        if (theirs < mine || (marcella_collects(ally) && theirs <= VESPER_CLAIM_RANGE)) return;
    }
    rewards.erase(s);
    capture_slot = s;
    collector = cached_ident(me);
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
    if (FIX_WALK_CLAIM && !walkable(index, position)) {
        int nearest = distance(position, spawn_lists[index].front());
        for (const auto t : spawn_lists[index]) nearest = std::min(nearest, distance(position, t));
        if (!ability_of(me, Kind::dash) || nearest > WALK_CLAIM_DASH) {
            release_claim("no walking path to the reward");
            rewards.erase(index);
            return false;
        }
    }
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
    if (!tiles.is_set(position) && !vesper_funnel(me) && !marcella_collects(me)) {
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
        // v101: a Heart of Ruin's shield counts as health on either side.
        for (std::size_t i = 0; i < allies.size(); ++i) ally_hp.add((allies[i].health() + hor_shield_hp(allies[i])) * weight(i));
        for (const auto& e : enemies) enemy_hp.add(e.health() + hor_shield_hp(e));
        const double ally_health = ally_hp.value(), enemy_health = enemy_hp.value();
        // v115 (GRADED_EDGES): by how far ahead or behind, not just which side is.
        auto graded = [](double ours, double theirs) {
            return EDGE_WEIGHT * (ours - theirs) / std::max(1.0, std::max(ours, theirs));
        };
        const double hp_edge = GRADED_EDGES ? graded(ally_health, enemy_health)
                                            : ally_health > enemy_health ? HEALTH_EDGE : -HEALTH_EDGE;
        head += hp_edge;
        LOG(part(head_text, format("health %+.1f", hp_edge)););
        PySum ally_pw, enemy_pw;
        for (std::size_t i = 0; i < allies.size(); ++i) ally_pw.add((static_cast<double>(allies[i].strength()) + allies[i].focus()) * weight(i));
        for (const auto& e : enemies) enemy_pw.add(static_cast<double>(e.strength()) + e.focus());
        const double ally_power = ally_pw.value(), enemy_power = enemy_pw.value();
        const double power_edge = GRADED_EDGES ? graded(ally_power, enemy_power)
                                               : ally_power > enemy_power ? POWER_EDGE : -POWER_EDGE;
        head += power_edge;
        LOG(part(head_text, format("power %+.1f", power_edge)););
        double owned_w = 0.0;
        for (std::size_t i = 0; i < allies.size(); ++i) owned_w += emblem_count(allies[i]) * weight(i);
        // v115 (EMBLEM_RELATIVE): less the near enemies' emblems.
        if (EMBLEM_RELATIVE)
            for (const auto& e : enemies) owned_w -= emblem_count(e);
        const int emblem_score = static_cast<int>(std::lround(EMBLEM_BONUS * owned_w));
        head += emblem_score;
        if (emblem_score) LOG(part(head_text, format("emblems %+d", emblem_score)););
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

bool Bot::willow_claim_risky(int s) {
    // Outside the safe zone, or with an enemy champion in sight.
    if (!seen::champions(Team::enemy).empty()) return true;
    return zones_known && s >= 0 && s < static_cast<int>(route.size()) && zone(s) != ZONE_SAFE;
}
