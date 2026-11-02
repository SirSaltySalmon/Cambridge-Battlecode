// Map setup, the shared route, and the fight-or-farm score.
#include "bot.h"
#include "draft.h"
#include "seen.h"
#include "state.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <iterator>
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
    pure_gems.reserve(indexes.size());
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
        monolith_flags.push_back(stats.monolith ? 1 : 0);
        if (!stats.monolith && group.gem_value() > 0) gem_income = true;
        centers.push_back(data.centers[index]);
        parts[0].push_back(draft::value_parts(group, draft::FARM_GOLD_WEIGHT, stats));
        parts[1].push_back(draft::value_parts(group, draft::GOLD_WEIGHT, stats));
        values[0].push_back(draft::value_of(parts[0].back(), priced_extra));
        values[1].push_back(draft::value_of(parts[1].back(), priced_extra));
        clears.push_back(draft::clear_rounds(stats));
        loads.push_back(draft::group_load(stats));
        gemless.push_back(group.gem_value() == 0);
        pure_gems.push_back(group.gem_value() > 0 && group.gold_value() == 0);
        int elks = 0, golems = 0, strong = 0, wolves = 0;
        for (const auto& camp : group.camps()) {
            if (camp.kind() == bc::camp_type::wolf) ++wolves;
            if (camp.kind() == bc::camp_type::elk) ++elks;
            if (camp.kind() == bc::camp_type::golem) ++golems;
            if (camp.kind() != bc::camp_type::gem_cluster && camp.kind() != bc::camp_type::gold_cluster) ++strong;
        }
        // v142 (GOLEM_GATE): golems ask by the champions present (golem_left), so the need here is the elks' only.
        const double golem_need = GOLEM_GATE ? 0 : golems * GOLEM_EMBLEMS;
        const int need = static_cast<int>(std::ceil(elks * ELK_EMBLEMS + golem_need - 1e-9));
        hard_need.push_back(need);
        golem_flags.push_back(static_cast<char>(std::min(golems, 127)));
        if (golems > 0) any_golem = true;
        clusters_only.push_back(strong == 0 && !group.camps().empty());
        // v159 (SATELLITE): clusters only, or wolves only within the leader's static solo bar.
        if (SATELLITE) {
            const bool wolf_ok = strong > 0 && strong == wolves
                && static_cast<double>(stats.strength) * stats.health / SAT_STATIC_DAMAGE <= SAT_STATIC_TAKE;
            // Stage 2 (SAT_ELK): one elk, otherwise clusters; the emblem and health bars are checked at the pick.
            const bool elk_ok = SAT_ELK && strong == 1 && elks == 1;
            sat_elk.push_back(elk_ok ? 1 : 0);
            sat_ok.push_back(clusters_only.back() || wolf_ok || elk_ok ? 1 : 0);
            sat_hp.push_back(stats.health);
            sat_str.push_back(stats.strength);
        }
        cluster_camps += static_cast<int>(group.camps().size()) - strong;
    }
    route_tiles.import_linear_buffer(route_bits);
    route_tile_count = route_tiles.count();
    gem_starved = !gem_income && !monoliths.empty();
    LOG(say(format("setup: %d cluster camps", cluster_camps)););
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
                if (zone_of[s] == ZONE_SAFE && route[s]->gem_value() > 0 && !is_monolith(static_cast<int>(s))) any_safe = true;
                safe += zone_of[s] == ZONE_SAFE;
                unsafe += zone_of[s] == ZONE_UNSAFE;
            }
            LOG(say(format("setup: zones %d safe, %d uncertain, %d unsafe%s", safe, static_cast<int>(zone_of.size()) - safe - unsafe,
                           unsafe, any_safe ? "" : " (no safe gem group: uncertain stays open)")););
            // v159 (SATELLITE, debug only): the solo groups by zone.
            LOG(if (SATELLITE) {
                std::string text = "setup: satellite groups";
                for (std::size_t s = 0; s < sat_ok.size(); ++s)
                    if (sat_ok[s]) text += format(" s%d z%d %s", static_cast<int>(s), zone_of[s], at(centers[s]).c_str());
                say(text);
            });
        }
    apply_contest();
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
    // v141 (REL_RING): a ring tile we reach is dropped unless it is at least REL_RING_EDGE steps nearer their spawn
    // than ours; tiles we cannot reach stay, and so do tiles far from both (behind walls beside their spawn). Dropping
    // those far ones lost reverse-spiral, starvation and broken-curve.
    // Two searches bounded at `near` steps mark the tiles sure to stay (within `near` of theirs and over
    // near + EDGE - 1 from ours: on far-apart spawns nearly the whole ring); only for the rest are full fields built
    // and read tile by tile. Reading every ring tile spilled setup into an out-of-fuel turn on ~0.6 champions a game.
    // v141 (REL_RING_HOLES): sides are measured with holes crossable. Holes stop neither basic attacks, spells nor
    // sight, and dashes cross them, so a tile just across holes from their spawn is theirs however long the walk round
    // (01gold-rush: the walking sides gave us the band across the holes from their spawn, deaths 217 vs 85).
    if (REL_RING && !ally_spawn_list.empty() && !enemy_spawn_list.empty()) {
        const int near = 2 * ring;
        const auto& their_blocks = REL_RING_HOLES ? bc::map::walls() : seen::terrain();
        const bc::bitboard our_blocks = REL_RING_HOLES ? (bc::map::walls() | bc::map::altars() | bc::map::spawn_region(Team::enemy))
                                                       : seen::fixed_blocks();
        bc::distance_field theirs, ours;
        bc::bitboard by_them, by_us, test;
        theirs.compute(their_blocks, bc::map::spawn_region(Team::enemy), by_them, static_cast<std::uint16_t>(near));
        ours.compute(our_blocks, seen::ally_spawn(), by_us, static_cast<std::uint16_t>(std::max(0, near + REL_RING_EDGE - 1)));
        fast::not_into(by_them, test);
        test |= by_us;
        test &= spawn_zone;
        fast::not_into(seen::fixed_blocks(), by_them);
        test &= by_them;
        int dropped = 0, tested = 0;
        if (!test.is_empty()) {
            // The tested tiles may lie past the bounds: full fields from here.
            theirs.compute(their_blocks, bc::map::spawn_region(Team::enemy), by_them);
            ours.compute(our_blocks, seen::ally_spawn(), by_us);
            for (const auto tile : test.positions()) {
                ++tested;
                const int us = ours.get(tile);
                if (us != bc::distance_field::unreachable && theirs.get(tile) + REL_RING_EDGE > us) {
                    spawn_zone.set(tile, false);
                    ++dropped;
                }
            }
        }
        LOG(say(format("setup: chase ring keeps their side (%d tiles); %d tested, %d dropped", spawn_zone.count(), tested,
                       dropped)););
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

int Bot::steps_to_ally_spawn(vec2 position) {
    if (!ally_spawn_steps) {
        ally_spawn_steps.emplace();
        bc::bitboard reached;
        ally_spawn_steps->compute(seen::fixed_blocks(), bc::map::spawn_region(Team::ally), reached);
    }
    return ally_spawn_steps->get(position);
}

void Bot::split_altars(const std::vector<bool>* solo) {
    // Solo altars sit away from the centre and no closer to the enemy spawn than to ours (draft::solo_altar).
    const auto altars = bc::map::altars().positions();
    single_altar = altars.size() == 1;
    small_bank = single_altar || (SMALL_BANK_THREE && seen::roster(Team::ally).size() <= 3);
    for (std::size_t i = 0; i < altars.size(); ++i) {
        const auto altar = altars[i];
        if (solo ? (*solo)[i] : draft::solo_altar(altar))
            solo_altars.push_back(altar);
        else
            escort_altars.push_back(altar);
    }
    LOG(say(format("setup: altars %zu solo, %zu escorted", solo_altars.size(), escort_altars.size())););
    // v151-f3 (ALTAR_FARM_BANK): the share of camp gems whose group lies within ALTAR_FARM_RANGE of an altar.
    if (ALTAR_FARM_BANK > 0 && altars.size() == 1) {
        long near = 0, total = 0;
        for (const auto& group : bc::map::camp_groups()) {
            if (group.camps().empty()) continue;
            const auto spot = group.camps().front().spawn_position();
            total += group.gem_value();
            for (const auto altar : altars)
                if (distance(spot, altar) <= ALTAR_FARM_RANGE) {
                    near += group.gem_value();
                    break;
                }
        }
        altar_farm = total > 0 && near >= total * ALTAR_FARM_SHARE;
        LOG(say(format("setup: %ld of %ld camp gems within %d of an altar%s", near, total, ALTAR_FARM_RANGE,
                       altar_farm ? ": altar farm, small bags" : "")););
    }
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
        // v156 (GATE_ALL_GROUPS): every distinct group on the submit tiles, not only the lowest-ranked one.
        std::vector<int> gates;
        for (int dy = -range; dy <= range; ++dy)
            for (int dx = -range; dx <= range; ++dx) {
                const vec2 tile{altar.x + dx, altar.y + dy};
                if (tile.x < 0 || tile.y < 0 || tile.x >= width || tile.y >= height || closed.is_set(tile)) continue;
                const int s = slot_at(tile);
                if (s < 0) {
                    free_tile = true;
                } else {
                    if (std::find(gates.begin(), gates.end(), s) == gates.end()) gates.push_back(s);
                    if (gate < 0 || fast::rank(tile) < gate_rank) {
                        gate = s;
                        gate_rank = fast::rank(tile);
                    }
                }
            }
        if (free_tile || !GATE_ALL_GROUPS) gates.assign(1, free_tile ? -1 : gate);
        // One (altar, group) pair per gate: the altar opens once any of its groups is down.
        for (const int g : gates) {
            gate_altars.push_back(altar);
            gate_slots.push_back(g);
            if (g >= 0) LOG(say(format("setup: altar %s is gated by s%d", at(altar).c_str(), g)););
        }
    }
}

bool Bot::altar_open(vec2 altar) const {
    // A gated altar opens once one of its groups is down: respawning, or cleared with its reward waiting.
    const int now = static_cast<int>(bc::round_number());
    bool gated = false;
    for (std::size_t i = 0; i < gate_altars.size(); ++i) {
        if (gate_altars[i] != altar) continue;
        const int g = gate_slots[i];
        if (g < 0 || g >= static_cast<int>(ready_at.size())) return true;
        if (ready_at[g] > now || rewards.count(g) > 0) return true;
        if (g >= static_cast<int>(seen_up.size()) || now - seen_up[g] > GATE_MEMORY) return true;
        gated = true;
    }
    return !gated;
}

bool Bot::altars_gated() const {
    return FIX_ALTAR_GATE && (all_altars_shut() || (GATE_TEAM && gates_closed()));
}

bool Bot::is_gate(int s) const {
    return std::find(gate_slots.begin(), gate_slots.end(), s) != gate_slots.end();
}

float Bot::monolith_left(int s) const {
    // v157 (MONO_MEMORY): the health share we last saw; full when unseen, respawning or with its reward waiting.
    if (!MONO_MEMORY || s < 0 || s >= static_cast<int>(mono_left.size())) return 1.0f;
    if (ready_at[s] > static_cast<int>(bc::round_number()) || rewards.count(s) > 0) return 1.0f;
    return mono_left[s];
}

bool Bot::monolith_damaged(int s) const {
    if (MONO_OWN_ONLY && (s >= static_cast<int>(mono_ours.size()) || !mono_ours[s])) return false;
    return MONO_RESUME && is_monolith(s) && monolith_left(s) <= MONO_DAMAGED;
}

double Bot::mono_session(int s, double& rounds) {
    // v158 (MONO_SESSIONS): the damage the champions within PRESENT_RANGE of us deal before the tank pool runs out.
    // Once a round: the leader asks per travel call, every champion in allowed().
    const int now = static_cast<int>(bc::round_number());
    if (session_round == now) {
        rounds = session_rounds;
        return session_damage;
    }
    session_round = now;
    const auto position = seen::self_position();
    double damage = 0, pool = 0, taken = 0, heal = 0;
    int n = 0, tanks = 0;
    for (const auto& a : seen::champions(Team::ally)) {
        if (distance(position, a.position()) > PRESENT_RANGE || channelling(a)) continue;
        const auto name = a.champion_name();
        const auto passives = owned_of(a).passives;
        damage += round_damage(name, a.strength(), a.focus()) * camp_dmg_mult(passives);
        // Only sturdy champions tank: the session ends before a fragile one is the nearest.
        // v159 (MONO_LEAVE): everyone takes its turn in front down to its walk-out bar (about MONO_POOL_HITS hits).
        const double floor = MONO_LEAVE ? MONO_POOL_HITS * camp_kind_strength(bc::camp_type::monolith) * camp_taken_mult(passives)
                                        : a.max_health() * RECALL_HEALTH;
        if (MONO_LEAVE || a.max_health() >= TANK_HEALTH) {
            const double usable = static_cast<double>(a.health()) - floor;
            if (usable > 0) {
                pool += usable;
                taken += camp_taken_mult(passives);
                ++tanks;
            }
        }
        if (name == Champ::willow) heal += 3.0 * a.focus() / 8.0;
        ++n;
    }
    // Only the last champion out is hit on the walk out (the monolith hits the nearest), so one walk-out for the team.
    if (!MONO_LEAVE) pool = std::max(0.0, pool - MONO_WALK_OUT);
    // Every monolith of the group hits its nearest champion each round (01boss-battle's pair: 400 a round).
    const double drain = tanks > 0 ? camp_kind_strength(bc::camp_type::monolith) * monolith_count(s) * taken / tanks - heal : 0;
    session_rounds = tanks > 0 ? (drain > 1 ? pool / drain : 1e9) : 0;
    session_damage = n > 0 ? damage * session_rounds : 0;
    LOG(note(format("mono session s%d: %d champions, %.0f a round, pool %.0f, %.1f rounds, %.0f damage", s, n, damage,
                    pool, session_rounds, session_damage)););
    rounds = session_rounds;
    return session_damage;
}

bool Bot::mono_contested(int s) {
    // v161 (MONO_SIDE): not clearly on our side by walking steps from each spawn; worked out once per slot.
    if (mono_side.size() != route.size()) mono_side.assign(route.size(), -1);
    if (s < 0 || s >= static_cast<int>(route.size())) return true;
    if (mono_side[s] < 0) {
        const int ours = steps_to_ally_spawn(centers[s]), theirs = steps_to_enemy_spawn(centers[s]);
        mono_side[s] = ours < 0 || theirs < 0 || ours + MONO_SIDE_MARGIN > theirs ? 1 : 0;
        LOG(say(format("monolith s%d: %d steps from our spawn, %d from theirs: %s", s, ours, theirs,
                       mono_side[s] ? "contested" : "our side")););
    }
    return mono_side[s] != 0;
}

int Bot::monolith_count(int s) {
    // v158: monolith camps in the group, counted once per slot.
    if (mono_count.size() != route.size()) mono_count.assign(route.size(), -1);
    if (s < 0 || s >= static_cast<int>(route.size())) return 1;
    if (mono_count[s] < 0) {
        int n = 0;
        for (const auto& camp : route[s]->camps())
            if (camp.kind() == bc::camp_type::monolith) ++n;
        mono_count[s] = std::max(1, n);
    }
    return mono_count[s];
}

bool Bot::monolith_ready(int s) {
    // v158 (MONO_SESSIONS): the session estimate in place of the emblem gate: a session under way goes on while the
    // tank pool lasts MONO_KEEP_ROUNDS; a start (or resume) needs a session worth min(health left, MONO_SESSION_MIN).
    // Only a session we started keeps going on the looser rule: a monolith the enemy hit is mid-clear too.
    if (!MONO_SESSIONS) return team_emblems() >= monolith_need(s);
    // v159 (MONO_LEAVE): not while resting off a monolith; (MONO_ENEMY_END) an enemy in sight ends the session.
    if (MONO_LEAVE && mono_rest) return false;
    // The only monolith that scores (gem-starved, or a gate shutting every altar) keeps v156's rule: enemies are always
    // near it there (00contest: no capture with this rule, 5 without).
    if (MONO_ENEMY_END && !sole_monolith(s) && !seen::champions(Team::enemy).empty()) return false;
    // A heavy bag is banked first (a camp kill destroys it), unless no altar takes it until a monolith falls.
    if (static_cast<int>(bc::self().gem_count()) >= MONO_BAG && !gem_starved && !altars_gated()) return false;
    double rounds = 0;
    const double damage = mono_session(s, rounds);
    const int now = static_cast<int>(bc::round_number());
    if (session_on.size() != route.size()) session_on.assign(route.size(), -MONO_SESSION_GAP - 1);
    bool ready;
    if (mid_clear(s) && now - session_on[s] <= MONO_SESSION_GAP) {
        ready = rounds >= MONO_KEEP_ROUNDS;
    } else {
        const double left = monolith_left(s) * MONOLITH_HEALTH * monolith_count(s);
        ready = damage >= std::min(left, MONO_SESSION_MIN);
        // v161 (MONO_SIDE): a contested monolith, or a small team, also waits for the emblem gate.
        if (ready && MONO_SIDE && (mono_contested(s) || seen::roster(Team::ally).size() <= 3))
            ready = team_emblems() >= monolith_emblems();
    }
    if (ready) session_on[s] = now;
    return ready;
}

int Bot::monolith_need(int s) const {
    // v157 (MONO_SCALE): a damaged monolith asks only its remaining share of the emblem gate.
    const int full = monolith_emblems();
    if (!MONO_SCALE || !monolith_damaged(s)) return full;
    return static_cast<int>(std::ceil(full * monolith_left(s) - 1e-9));
}

bool Bot::sole_monolith(int s) const {
    // v156 (GATE_GROUP_LOOSE): a gate monolith shutting every altar is the only way to score, as on a gem-starved map.
    return gem_starved || (GATE_GROUP_LOOSE && is_gate(s) && altars_gated());
}

bool Bot::gates_closed() const {
    if (gate_altars.empty()) return false;
    const int now = static_cast<int>(bc::round_number());
    for (const int g : gate_slots)
        if (g < 0 || g >= static_cast<int>(ready_at.size()) || ready_at[g] > now || rewards.count(g) > 0) return false;
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
    // Without repeats, in a reused vector. v133t: a per-group stamp of this pass replaces the sort and unique over every
    // visible spawn tile (each group is handled on its own, so the order does not matter).
    auto& slots = slot_scratch;
    slots.clear();
    static std::vector<int> stamp;
    static int pass = 0;
    ++pass;
    if (stamp.size() != route.size()) stamp.assign(route.size(), 0);
    if (tiles.size() < route_tile_count) tiles.resize(route_tile_count);
    const auto tile_n = std::min<std::size_t>(visible.positions_into(tiles), tiles.size());
    for (std::size_t t = 0; t < tile_n; ++t) {
        const int index = slot_at(tiles[t]);
        if (index < 0 || index >= static_cast<int>(stamp.size()) || stamp[index] == pass) continue;
        stamp[index] = pass;
        slots.push_back(index);
    }
    for (const int index : slots) {
        // The group is in sight (one of its spawn tiles is), so the raw state read replaces group.state(),
        // which fetched a whole vision board per group to check that. 1 alive, 2 reward, 3 respawning.
        const auto handle = static_cast<std::uint32_t>(static_cast<bc::handle_camp_group_t>(*route[index]));
        std::uint32_t state = 0;
        if (bc::detail::sdk_camp_group_state(handle, &state) != 0 || state < 1 || state > 3) continue;
        std::int32_t timer = 0;
        if (state == 3 && bc::detail::sdk_camp_group_cooldown(handle, &timer) != 0) continue;
        // v137 (CAPTURE_NEWS): a respawn timer we are the first to see goes to the team.
        const bool fresh = CAPTURE_NEWS && state == 3 && ready_at[index] <= now;
        ready_at[index] = state == 3 ? now + timer : 0;
        if (fresh) announce_capture(index);
        if (FIX_ALTAR_GATE && state == 1) {
            if (seen_up.size() != route.size()) seen_up.assign(route.size(), -GATE_MEMORY - 1);
            seen_up[index] = now;
        }
        // v157 (MONO_MEMORY): the health share of a monolith in sight; a reward or respawn timer means it comes back full.
        if (MONO_MEMORY && is_monolith(index)) {
            if (mono_left.size() != route.size()) mono_left.assign(route.size(), 1.0f);
            if (state != 1) {
                mono_left[index] = 1.0f;
            } else {
                float health = 0, max_health = 0;
                for (const auto& camp : seen::camps())
                    if (slot_at(camp.spawn_position()) == index) {
                        health += camp.health();
                        max_health += camp.max_health();
                    }
                if (max_health > 0) {
                    const float share = health / max_health;
                    // v158 (MONO_OWN_ONLY): new damage with an ally near it and no enemy in sight is ours.
                    if (MONO_OWN_ONLY && share < mono_left[index]) {
                        if (mono_ours.size() != route.size()) mono_ours.assign(route.size(), 0);
                        bool ally = false;
                        for (const auto& a : seen::champions(Team::ally))
                            if (distance(a.position(), centers[index]) <= MONOLITH_REACH + 1) ally = true;
                        mono_ours[index] = ally && seen::champions(Team::enemy).empty() ? 1 : 0;
                    }
                    mono_left[index] = share;
                }
            }
        }
        if (state == 2)
            rewards.insert(index);
        else
            rewards.erase(index);
    }
}

void Bot::announce_capture(int s) {
    // v137 (CAPTURE_NEWS): only while no claim is open: the collector and claim-round bytes are free then.
    if (!CAPTURE_NEWS || s < 0 || s >= static_cast<int>(route.size()) || shared_byte(CAPTURE_SLOT_BYTE) != NO_CAPTURE) return;
    const int start = ready_at[s] - route[s]->max_respawn_timer();
    set_shared_byte(CAPTURE_SLOT_BYTE + 1, CAPTURED_MARK | (start & CAPTURED_ROUND_MASK));
    set_shared_byte(CLAIM_ROUND_OFFSET, s);
}

void Bot::read_capture_news(int collector_byte, int slot_byte) {
    if (!(collector_byte & CAPTURED_MARK) || slot_byte < 0 || slot_byte >= static_cast<int>(route.size())) return;
    const int now = static_cast<int>(bc::round_number());
    // The low bits of the round its timer started; news is at most a few rounds old.
    const int start = now - ((now - (collector_byte & CAPTURED_ROUND_MASK)) & CAPTURED_ROUND_MASK);
    const int ready = start + route[slot_byte]->max_respawn_timer();
    if (ready <= now || ready_at[slot_byte] > now) return;
    ready_at[slot_byte] = ready;
    rewards.erase(slot_byte);
    NOTE(format("team news: s%d respawns in %d", slot_byte, ready - now));
}

int Bot::wait_at(int s, vec2 origin) const {
    // Rounds left to wait on arrival at a route group.
    const int travel = distance(origin, centers[s]);
    return std::max(0, ready_at[s] - static_cast<int>(bc::round_number()) - travel);
}

void Bot::update_emblems() {
    // A champion's type never changes, so each ally's key is read once.
    const auto& allies = seen::roster(Team::ally);
    // v134 (CAPTURE_PRICING): a living Capture emblem owner cuts every capture we price to CAPTURE_EMBLEM_TIME. Only
    // owners (cached) cost a life read.
    capture_rounds = bc::constants::capture_time;
    if (CAPTURE_PRICING)
        for (const auto& ally : allies)
            if (has(owned_of(ally).passives, bc::item_family::capture) && ally.is_alive()) {
                capture_rounds = std::min(capture_rounds, CAPTURE_EMBLEM_TIME);
                break;
            }
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

void Bot::apply_contest() {
    // v144 (CONTEST_VALUE): values[] x (1 + CONTEST_DENY x contest), from the draft's walking steps to each spawn.
    contest_mult.assign(route.size(), 1.0);
    if (!CONTEST_VALUE || !zones_known) return;
    int contested = 0;
    for (std::size_t s = 0; s < route.size() && s < home_steps.size(); ++s) {
        const int ours = home_steps[s], theirs = enemy_steps[s];
        if (ours >= 255 || theirs >= 255 || ours + theirs == 0) continue;
        const double share = static_cast<double>(ours) / (ours + theirs);
        const double contest = std::max(0.0, 1.0 - std::abs(share - 0.5) / CONTEST_WIDTH);
        contest_mult[s] = 1.0 + CONTEST_DENY * contest;
        for (int k = 0; k < 2; ++k) values[k][s] *= contest_mult[s];
        if (contest > 0) ++contested;
    }
    LOG(say(format("setup: %d contested groups priced up (deny %.1f, width %.2f)", contested, CONTEST_DENY, CONTEST_WIDTH)););
}

void Bot::reprice() {
    // v105 (STRONG_EARLY): the strong camps' extra health cost fades with the team's emblems; values[] is only worked
    // out again when the weight changes (an emblem bought), a pass of arithmetic over the slots.
    const auto team = std::max<std::size_t>(1, seen::roster(Team::ally).size());
    const double owned = static_cast<double>(team_emblems()) / (STRONG_FADE_EMBLEMS * static_cast<double>(team));
    const double extra = STRONG_EARLY ? STRONG_EARLY_EXTRA * std::max(0.0, 1.0 - owned) : 0.0;
    // v177 (ECON_PHASES, CLOSE_OUT): the farm phase and the close-out (no contest pricing) also change values[].
    const bool phased = ECON_PHASES && !as_v170();
    const bool built = econ_built(), close = CLOSE_OUT_CONTEST && close_out();
    if (priced_once && extra == priced_extra && (!phased || built == priced_built) && close == priced_close) return;
    priced_once = true;
    priced_extra = extra;
    priced_built = built;
    priced_close = close;
    for (int k = 0; k < 2; ++k)
        for (std::size_t s = 0; s < parts[k].size(); ++s)
            values[k][s] = (phased ? draft::value_phased(parts[k][s], extra, built) : draft::value_of(parts[k][s], extra))
                           * (!close && s < contest_mult.size() ? contest_mult[s] : 1.0);
    LOG(say(format("reprice: strong camps' health cost x%.2f (%d team emblems)%s%s", 1 + extra, team_emblems(),
                   phased ? (built ? ", built: elk and cluster gems up" : ", building: gold clusters up") : "",
                   close ? ", close-out: no contest pricing" : "")););
}

bool Bot::econ_built() const {
    // v177 (ECON_PHASES): BUILT_EMBLEMS team emblems per champion of the roster.
    const auto team = std::max<std::size_t>(1, seen::roster(Team::ally).size());
    return team_emblems() >= BUILT_EMBLEMS * static_cast<double>(team);
}

bool Bot::close_out() const {
    // v177 (CLOSE_OUT): from CLOSE_OUT_ROUND, our tributed gems ahead of theirs (global, so every champion agrees).
    if (!CLOSE_OUT || as_v170() || static_cast<int>(bc::round_number()) < CLOSE_OUT_ROUND) return false;
    return bc::global_submitted_gems(Team::ally) > bc::global_submitted_gems(Team::enemy);
}

bool Bot::enemy_half(vec2 p) const {
    if (!spawn_centres) {
        auto centre = [](const bc::bitboard& board) {
            long x = 0, y = 0, n = 0;
            for (const auto tile : board.positions()) {
                x += tile.x;
                y += tile.y;
                ++n;
            }
            return n ? vec2{static_cast<int>(x / n), static_cast<int>(y / n)} : vec2{0, 0};
        };
        spawn_centres = std::pair{centre(bc::map::spawn_region(Team::ally)), centre(bc::map::spawn_region(Team::enemy))};
    }
    return distance(p, spawn_centres->second) < distance(p, spawn_centres->first);
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

int Bot::roster_size() const {
    if (roster_size_cache == 0) roster_size_cache = static_cast<int>(seen::roster(Team::ally).size());
    return roster_size_cache;
}

bool Bot::small_team() const {
    // v150 (SMALL_FUNNEL_TEAM): read once; the roster never changes size.
    if (!small_team_cache) small_team_cache = SMALL_FUNNEL_TEAM > 0 && roster_size() < SMALL_FUNNEL_TEAM;
    return *small_team_cache;
}

bool Bot::vesper_funnel(const bc::champion& champion) const {
    if (champion.champion_name() != Champ::vesper_vale) return false;
    return emblem_count(champion) < (small_team() ? SMALL_FUNNEL_EMBLEMS : VESPER_FUNNEL_EMBLEMS);
}

bool Bot::marcella_collects(const bc::champion& champion) const {
    // v113 (VARG_CLAIM): Varg, drafted in her place, collects as she does.
    const auto kind = champion.champion_name();
    if (!MARCELLA_CLAIM || !(kind == Champ::marcella || (VARG_CLAIM && kind == Champ::varg))) return false;
    if (SMALL_CLAIM_NEUTRAL && small_team()) return false;
    return MARCELLA_FUNNEL_EMBLEMS == 0 || emblem_count(champion) < MARCELLA_FUNNEL_EMBLEMS;
}

bool Bot::drafted_prism() const {
    if (!has_prism) {
        has_prism = false;
        for (const auto& ally : seen::roster(Team::ally))
            if (ally.champion_name() == Champ::prism) has_prism = true;
    }
    return *has_prism;
}

bool Bot::prism_team() const {
    if (!prism_team_cache) prism_team_cache = draft::prism_map() && drafted_prism();
    return *prism_team_cache;
}

bool Bot::prism_free() const {
    if (target2 != NO_TARGET2) return false;
    for (const auto& ally : seen::roster(Team::ally))
        if (ally.champion_name() == Champ::prism) return ally.is_alive();
    return false;
}

bool Bot::prism_wants(int s) const {
    if (!prism_free()) return false;
    if (!PRISM_CLAIM_NEAR) return true;
    for (const auto& ally : seen::champions(Team::ally))
        if (ally.champion_name() == Champ::prism) return distance(ally.position(), centers[s]) <= VESPER_CLAIM_RANGE;
    return false;
}

bool Bot::prism_stay_back(const bc::champion& me) {
    if (!PRISM_STAY_BACK || !prism_team() || me.champion_name() != Champ::prism || me.max_health() >= TANK_HEALTH) return false;
    const auto& enemies = near_enemies();
    if (enemies.empty() || !me.can_move()) return false;
    const auto position = me.position();
    auto gap = [&](vec2 from) {
        int best = distance(from, enemies.front().position());
        for (const auto& e : enemies) best = std::min(best, distance(from, e.position()));
        return best;
    };
    const int mine = gap(position);
    int ahead = 1000;
    std::optional<vec2> nearest_melee;
    for (const auto& ally : seen::champions(Team::ally)) {
        if (ally == me || ally.attack_range() > 1 || channelling(ally)) continue;
        const auto spot = ally.position();
        ahead = std::min(ahead, gap(spot));
        if (!nearest_melee || distance(position, spot) < distance(position, *nearest_melee)) nearest_melee = spot;
    }
    // A melee ally is ahead of her.
    if (ahead < mine) {
        if (mine <= PRISM_SPELL_REACH || mine > ahead + 1) return false;
        WHY("prism.back", "bare, a melee ally only just ahead: hold rather than close");
        return true;
    }
    if (nearest_melee) {
        if (distance(position, *nearest_melee) <= 1) return false;
        WHY("prism.back", "bare and in front: back to the melee ally at " + at(*nearest_melee));
        return move_towards_closest(tiles_around(*nearest_melee, 1));
    }
    if (route.empty()) return false;
    const auto spot = centers[slot % static_cast<int>(route.size())];
    if (distance(position, spot) <= 2) return false;
    WHY("prism.back", "bare and no melee ally in sight: to the team target " + at(spot));
    return move_towards_closest(tiles_around(spot, 2));
}

bool Bot::willow_follow(const bc::champion& me) {
    if (!PRISM_WILLOW_FOLLOW || !prism_team() || me.champion_name() != Champ::willow || route.empty()) return false;
    const auto position = me.position();
    for (const auto& ally : seen::champions(Team::ally))
        if (ally.champion_name() == Champ::prism) {
            if (distance(position, ally.position()) <= PRISM_WILLOW_RANGE) return false;
            heal_lowest_ally(me);
            WHY("willow.follow", "stay with Prism at " + at(ally.position()));
            return move_towards_closest(tiles_around(ally.position(), PRISM_WILLOW_RANGE - 1));
        }
    if (!prism_free()) return false;
    const bool hers = capture_slot != NO_CAPTURE && capture_slot < static_cast<int>(route.size())
                      && collector == champ_id(Champ::prism) + 1;
    const auto spot = centers[hers ? capture_slot : slot % static_cast<int>(route.size())];
    if (distance(position, spot) <= PRISM_WILLOW_RANGE) return false;
    WHY("willow.follow", "to where Prism is, " + at(spot));
    return move_towards_closest(tiles_around(spot, PRISM_WILLOW_RANGE - 1));
}

bool Bot::ally_lacks_prospector() const {
    for (const auto& ally : seen::champions(Team::ally))
        if (ally.champion_name() != Champ::prism && !has(owned_of(ally).passives, bc::item_family::prospector)) return true;
    return false;
}

bool Bot::picker_reward(int r) const {
    if (!PRISM_PICKERS || !prism_team() || r < 0 || r >= static_cast<int>(route.size()) || gemless[r]) return false;
    if (reward_gems.empty()) {
        reward_gems.reserve(route.size());
        int total = 0;
        for (const auto* group : route) {
            reward_gems.push_back(static_cast<int>(group->gem_value()));
            total += reward_gems.back();
        }
        auto sorted = reward_gems;
        std::sort(sorted.begin(), sorted.end());
        int sum = 0;
        for (const int gems : sorted) {
            sum += gems;
            if (2 * sum >= total) {
                gem_median = gems;
                break;
            }
        }
    }
    const int now = static_cast<int>(bc::round_number());
    if (r < static_cast<int>(reward_seen.size()) && reward_seen[r] >= 0 && now - reward_seen[r] >= PRISM_PICKER_WAIT) return false;
    if (picker_alive_round != now) {
        picker_alive_round = now;
        picker_alive = false;
        for (const auto& ally : seen::roster(Team::ally)) {
            const auto kind = ally.champion_name();
            if ((kind == Champ::marcella || kind == Champ::heart_of_ruin) && ally.is_alive()) picker_alive = true;
        }
    }
    if (!picker_alive) return false;
    if (reward_gems[r] < gem_median) return true;
    return reward_gems[r] < 2 * gem_median && capture_slot != NO_CAPTURE && capture_slot != r && collector == champ_id(Champ::prism) + 1;
}

void Bot::update_pickers() {
    if (!PRISM_PICKERS || !prism_team() || route.empty()) return;
    const int now = static_cast<int>(bc::round_number());
    reward_seen.resize(route.size(), -1);
    for (int r = 0; r < static_cast<int>(route.size()); ++r) {
        if (!rewards.count(r)) reward_seen[r] = -1;
        else if (reward_seen[r] < 0) reward_seen[r] = now;
    }
}

bool Bot::picker_collect(const bc::champion& me) {
    if (!PRISM_PICKERS || !prism_team() || route.empty()) return false;
    const auto kind = me.champion_name();
    if (kind != Champ::marcella && kind != Champ::heart_of_ruin) return false;
    const int now = static_cast<int>(bc::round_number());
    if (mode == FIGHT || !near_enemies().empty() || enemy_threat(me) || channelling(me)) {
        picker_slot = -1;
        return false;
    }
    if (picker_slot >= 0 && (ready_at[picker_slot] > now || now - picker_since > PRISM_PICKER_TIMEOUT || !camps_at(picker_slot).empty())) {
        if (ready_at[picker_slot] > now) announce_capture(picker_slot);
        picker_slot = -1;
    }
    const auto position = me.position();
    if (picker_slot < 0) {
        if (static_cast<int>(me.gem_count()) >= bank_bar()) return false;
        const bool dasher = dash_kind(kind);
        const auto bodies = bc::vision::champion_tiles(Team::ally) | bc::vision::champion_tiles(Team::enemy);
        for (const int r : rewards) {
            const int mine = distance(position, centers[r]);
            if (mine > PRISM_PICKER_RANGE || !picker_reward(r) || ready_at[r] > now || !can_reach(r, position, dasher)) continue;
            if (picker_slot >= 0 && mine >= distance(position, centers[picker_slot])) continue;
            auto held = spawn_board(r);
            held &= bodies;
            if (!held.is_empty() && !spawn_board(r).is_set(position)) continue;
            bool other = false;
            for (const auto& ally : seen::champions(Team::ally)) {
                if (ally == me) continue;
                const auto theirs = ally.champion_name();
                if (theirs != Champ::marcella && theirs != Champ::heart_of_ruin) continue;
                const int d = distance(ally.position(), centers[r]);
                if (d < mine || (d == mine && theirs == Champ::marcella)) other = true;
            }
            if (!other) picker_slot = r;
        }
        if (picker_slot < 0) return false;
        rewards.erase(picker_slot);
        picker_since = now;
        NOTE(format("picker takes s%d's reward", picker_slot));
    }
    const auto& tiles = spawn_board(picker_slot);
    if (tiles.is_set(position)) {
        WHY("picker.hold", format("capturing s%d", picker_slot));
        return true;
    }
    WHY("picker.walk", format("to s%d's reward", picker_slot));
    if (move_towards_closest(tiles)) return true;
    picker_slot = -1;
    return false;
}

bool Bot::prism_away(const bc::champion& me) const {
    if (!PRISM_FIGHT_NEAR || !prism_team() || me.champion_name() == Champ::prism) return false;
    for (const auto& ally : seen::champions(Team::ally))
        if (ally.champion_name() == Champ::prism) return distance(me.position(), ally.position()) > PRISM_WITH_RANGE;
    for (const auto& ally : seen::roster(Team::ally))
        if (ally.champion_name() == Champ::prism) return ally.is_alive();
    return false;
}

bool Bot::run_to_prism(const bc::champion& me) {
    if (route.empty() || !me.can_move()) return false;
    for (const auto& enemy : seen::champions(Team::enemy))
        if (enemy.attack_range() <= 1 && distance(me.position(), enemy.position()) <= 1) return false;
    const bool hers = capture_slot != NO_CAPTURE && capture_slot < static_cast<int>(route.size())
                      && collector == champ_id(Champ::prism) + 1;
    const auto spot = centers[hers ? capture_slot : slot % static_cast<int>(route.size())];
    if (distance(me.position(), spot) <= 2) return false;
    WHY("prism.rejoin", "a fight without her: go to where she is, " + at(spot));
    return move_towards_closest(tiles_around(spot, 2));
}

void Bot::update_prism_cash(const bc::champion& me) {
    if (!prism_team() || me.champion_name() != Champ::prism) return;
    const int now = static_cast<int>(bc::round_number());
    const int enemy = static_cast<int>(bc::global_submitted_gems(Team::enemy));
    const int banked = static_cast<int>(bc::global_submitted_gems(Team::ally));
    const int ours = banked + (me.is_alive() ? static_cast<int>(me.gem_count()) : 0);
    if (race_new_round < 0 || now - race_new_round >= PRISM_RACE_ROUNDS) {
        race_old_round = race_new_round;
        race_old_gems = race_new_gems;
        race_old_ours = race_new_ours;
        race_new_round = now;
        race_new_gems = enemy;
        race_new_ours = ours;
    }
    auto post = [&](int value) {
        if (target2 != value) write_target2(value);
        target2 = value;
    };
    if (!me.is_alive()) {
        post(NO_TARGET2);
        return;
    }
    if (!prism_built && has(owned_of(me).passives, bc::item_family::durability)
        && has(owned_of(me).passives, bc::item_family::prospector))
        mark_vesper_done();
    const int bag = static_cast<int>(me.gem_count());
    const auto& altars = seen::altar_list();
    const auto position = me.position();
    int nearest = 0;
    for (std::size_t i = 1; i < altars.size(); ++i)
        if (distance(position, altars[i]) < distance(position, altars[nearest])) nearest = static_cast<int>(i);
    if (!prism_cash_on && bag > 0 && !altars.empty()) {
        const int need = tribute_ticks(bag, owned_of(me).passives) + distance(position, altars[nearest]) * PRISM_CASH_WALK
                         + PRISM_CASH_MARGIN;
        const bool late = now + need >= PRISM_ROUND_LIMIT;
        const int gained = enemy - race_old_gems;
        const bool race = !PRISM_STRICT && race_old_round >= 0 && gained > 0 && enemy < WIN_GEMS
                          && (WIN_GEMS - enemy) * (now - race_old_round) <= need * gained;
        if (late || race) {
            prism_cash_on = true;
            NOTE(format("prism cash-in: %d gems take %d rounds, %s", bag, need, late ? "the round limit is near" : "the enemy would win first"));
        }
        // v160 (PRISM_EARLY_CASH): the allies' income during her walk and channel can finish the 1000.
        const int walk_channel = tribute_ticks(bag, owned_of(me).passives) + distance(position, altars[nearest]) * PRISM_CASH_WALK;
        if (PRISM_EARLY_CASH && !PRISM_STRICT && !prism_cash_on && race_old_round >= 0 && ours > race_old_ours && bag >= PRISM_CASH_GEMS
            && ours + PRISM_EARLY_INCOME * (ours - race_old_ours) * walk_channel / (now - race_old_round) >= WIN_GEMS) {
            prism_cash_on = true;
            prism_early = true;
            NOTE(format("prism early cash-in: %d gems, %d tributed, %d rounds to finish", bag, banked, walk_channel));
        }
    }
    const bool cash = !prism_holds(me) && bag > 0 && (bag >= PRISM_CASH_GEMS || win_tribute);
    // v160: the altar index in the low bits; PRISM_NO_GUARD_BIT says the allies farm on (an early start, until it wins).
    if (!cash || altars.empty() || static_cast<int>(altars.size()) >= PRISM_NO_GUARD_BIT) {
        post(NO_TARGET2);
    } else {
        const int altar = target2 == NO_TARGET2 ? nearest : target2 & (PRISM_NO_GUARD_BIT - 1);
        post(altar | (prism_early && !win_tribute ? PRISM_NO_GUARD_BIT : 0));
    }
}

bool Bot::prism_mid_clear(const bc::champion& me) {
    if (!prism_team() || me.champion_name() != Champ::prism || route.empty()) return false;
    const int s = slot % static_cast<int>(route.size());
    return !gemless[s] && !camps_at(s).empty();
}

bool Bot::leave_prism_reward(const bc::champion& me) {
    if (!prism_team() || me.champion_name() == Champ::prism || route.empty() || !prism_free()) return false;
    const auto position = seen::self_position();
    const int s = slot_at(position);
    if (s >= 0 && s < static_cast<int>(route.size()) && !prism_wants(s)) return false;
    if (s == picker_slot) return false;
    reward_stay_turns = s == reward_stay_slot ? reward_stay_turns + 1 : 1;
    reward_stay_slot = s;
    if (s < 0 || s >= static_cast<int>(route.size()) || gemless[s] || reward_stay_turns < PRISM_STEP_OFF_TURNS
        || !camps_at(s).empty()) return false;
    const auto& tiles = spawn_board(s);
    bc::bitboard off;
    off.draw_chebyshev(position, 1);
    off &= ~tiles;
    if (off.is_empty()) {
        off.draw_chebyshev(position, PRISM_STEP_OFF_RANGE);
        off &= ~tiles;
    }
    WHY("prism.stepoff", format("s%d's reward is hers: off its tiles", s));
    return move_towards_closest(off);
}

bool Bot::prism_cash_step(const bc::champion& me, int gems) {
    const auto& altars = seen::altar_list();
    if (target2 == NO_TARGET2) return false;
    const int index = target2 & (PRISM_NO_GUARD_BIT - 1);
    if (index >= static_cast<int>(altars.size())) return false;
    if ((target2 & PRISM_NO_GUARD_BIT) && me.champion_name() != Champ::prism) return false;
    const auto altar = altars[index];
    if (me.champion_name() == Champ::prism) return bank_gems(me, {altar}, "prism.cash");
    // v180 (PRISM_GUARD): an ally's own bag waits while an enemy can reach her: its channel would leave her unguarded.
    if (gems > 0 && !(PRISM_GUARD && prism_threatened(altar)) && bank_gems(me, {altar}, "prism.cash")) return true;
    return PRISM_GUARD ? prism_guard(me, altar) : guard(me, altar);
}

int Bot::kind_rank(Champ kind, int s, bool funnelled) const {
    // v123 (PRISM_CLAIM): Prism comes first on gem-only rewards and second (Marcella's place) on the rest; a funnelled
    // Vesper the other way round. Without Prism on the team Vesper stays first everywhere, as before.
    const bool pure = PRISM_CLAIM && s >= 0 && s < static_cast<int>(pure_gems.size()) && pure_gems[s];
    // v165 (VESPER_GOLD_ONLY): her bonus is gold only, so a gold-less reward is not funnelled to her.
    if (kind == Champ::vesper_vale && VESPER_GOLD_ONLY && s >= 0 && s < static_cast<int>(pure_gems.size()) && pure_gems[s]) return 0;
    if (kind == Champ::vesper_vale) return !funnelled ? 0 : pure && drafted_prism() ? 2 : 1;
    if (SMALL_CLAIM_NEUTRAL && small_team()) return 0;
    if (PRISM_CLAIM && kind == Champ::prism) return pure ? 1 : 2;
    if (MARCELLA_CLAIM && (kind == Champ::marcella || (VARG_CLAIM && kind == Champ::varg))) return 2;
    return 0;
}

int Bot::collect_rank(const bc::champion& champion, int s) const {
    const auto kind = champion.champion_name();
    // v158: gems always go to Prism. Gold goes to her until her two emblems are done, then to allies without Prospector.
    if (prism_team()) {
        // v161: a picker's reward is not hers, and they claim it privately.
        if (picker_reward(s)) return 0;
        const bool gold_only = s >= 0 && s < static_cast<int>(gemless.size()) && gemless[s];
        if (kind == Champ::prism) return !gold_only || !prism_built || !ally_lacks_prospector() ? 1 : 0;
        return gold_only && prism_built && !has(owned_of(champion).passives, bc::item_family::prospector) ? 1 : 0;
    }
    if (kind == Champ::vesper_vale) return kind_rank(kind, s, vesper_funnel(champion));
    if (kind == Champ::marcella || kind == Champ::varg) return marcella_collects(champion) ? kind_rank(kind, s, true) : 0;
    return kind_rank(kind, s, true);
}

bool Bot::prism_holds(const bc::champion& me) const {
    // v123 (PRISM_HOLD): her Focus grows with her bag, so she banks only near the round limit or when it wins.
    if (!PRISM_HOLD || me.champion_name() != Champ::prism) return false;
    // v158: the Prism team holds to the win; only a safety start (update_prism_cash) ends it.
    if (prism_team())
        return !prism_cash_on && (PRISM_STRICT || !win_tribute)
               && static_cast<int>(bc::global_submitted_gems(Team::ally)) + static_cast<int>(me.gem_count()) < WIN_GEMS;
    if (win_tribute || static_cast<int>(bc::round_number()) >= PRISM_CASH_ROUND) return false;
    const int bag = static_cast<int>(me.gem_count());
    // v123 cash-ins: the enemy near a win, or a bag at the cap.
    if (PRISM_CASH_ENEMY_GEMS > 0 && static_cast<int>(bc::global_submitted_gems(Team::enemy)) >= PRISM_CASH_ENEMY_GEMS)
        return false;
    if (PRISM_BAG_CAP > 0 && bag >= PRISM_BAG_CAP) return false;
    return static_cast<int>(bc::global_submitted_gems(Team::ally)) + bag < WIN_GEMS;
}

int Bot::hard_left(int s) const {
    // Emblems the group needs past the free part (HARD_FREE, the team size, one per HARD_DECAY_ROUNDS rounds).
    return std::max(0, hard_need[s] - hard_free()) + golem_left(s, gate_count());
}

int Bot::gate_count() const {
    if (PRESENT_GATE) return team_present().n;
    return HARD_FREE_TEAM ? static_cast<int>(seen::roster(Team::ally).size()) : 1;
}

int Bot::hard_free() const {
    int free = HARD_FREE;
    if (PRESENT_GATE || HARD_FREE_TEAM) free += gate_count();
    if (HARD_DECAY_ROUNDS > 0) free += static_cast<int>(bc::round_number()) / HARD_DECAY_ROUNDS;
    return free;
}

int Bot::golem_left(int s, int n) const {
    // v142 (GOLEM_GATE): per golem, alone by our own attack (melee or ranged), else by how many of us are there.
    if (!GOLEM_GATE || !is_golem(s)) return 0;
    int per;
    if (n <= 1) {
        if (self_ranged < 0) self_ranged = bc::self().attack_range() > 1 ? 1 : 0;
        per = self_ranged ? GOLEM_SOLO_RANGED : GOLEM_SOLO_MELEE;
    } else {
        constexpr int last = static_cast<int>(std::size(GOLEM_PRESENT_EMBLEMS)) - 1;
        per = GOLEM_PRESENT_EMBLEMS[std::min(n, last)];
    }
    return golem_flags[s] * per;
}

Bot::Present Bot::present_near(vec2 at, int range) const {
    Present p;
    const auto& allies = seen::champions(Team::ally);
    const auto& tiles = seen::positions(Team::ally);
    for (std::size_t i = 0; i < allies.size() && i < tiles.size(); ++i) {
        if (distance(tiles[i], at) > range) continue;
        ++p.n;
        const auto it = emblems.find(cached_ident(allies[i]) - 1);
        if (it != emblems.end()) p.emblems += it->second;
    }
    return p;
}

const Bot::Present& Bot::team_present() const {
    const int now = turn_round;
    if (present_round != now) {
        present_round = now;
        present_cache = present_near(seen::self_position(), PRESENT_RANGE);
        if (present_cache.n == 0) present_cache.n = 1;  // channelling ourselves: still one of us
    }
    return present_cache;
}

bool Bot::hard_ok(int s) const {
    if (!HARD_GATE || s >= static_cast<int>(hard_need.size())) return true;
    const int left = hard_left(s);
    // v142 (GOLEM_GATE): a golem group never passes on the fallback floor.
    const int floor = GOLEM_GATE && is_golem(s) ? -1 : hard_floor;
    return left == 0 || left <= std::max(PRESENT_GATE ? team_present().emblems : team_emblems(), floor);
}

void Bot::drop_hard(Travel& travel) {
    // v114 (HARD_GATE): groups the team is not built for leave the table; if that would leave no group but monoliths,
    // the ones needing the fewest emblems stay (hard_floor), so the team farms the easiest instead of standing idle.
    hard_floor = -1;
    if (!HARD_GATE) return;
    const int owned = PRESENT_GATE ? team_present().emblems : team_emblems();
    // v133t: the free part once for both passes (hard_left read it per group).
    const int free = hard_free();
    const int n = gate_count();
    auto left_of = [&](int s) { return std::max(0, hard_need[s] - free) + golem_left(s, n); };
    int easiest = -1;
    bool open = false;
    for (int s = 0; s < static_cast<int>(travel.steps.size()); ++s) {
        if (!travel.has(s) || is_monolith(s)) continue;
        const int left = left_of(s);
        if (left <= owned) {
            open = true;
            break;
        }
        // v142 (GOLEM_GATE): golem groups never set the floor (monoliths never did).
        if (GOLEM_GATE && is_golem(s)) continue;
        if (easiest < 0 || left < easiest) easiest = left;
    }
    if (!open && easiest >= 0) hard_floor = easiest;
    // hard_ok(s) with the team's emblems counted once (team_emblems walks the emblem map).
    const int bar = std::max(owned, hard_floor);
    for (int s = 0; s < static_cast<int>(travel.steps.size()); ++s) {
        if (!travel.has(s) || is_monolith(s) || s >= static_cast<int>(hard_need.size())) continue;
        const int left = left_of(s);
        if (left == 0 || left <= (GOLEM_GATE && is_golem(s) ? owned : bar)) continue;
        travel.steps[s] = -1;
        --travel.reachable;
    }
}

bool Bot::allowed(int s) {
    if (!hard_ok(s)) return false;
    // v137 (GOLEM_LAST): farm()'s own picks hold golems back too.
    if (GOLEM_LAST && any_golem && is_golem(s) && golems_held() && !golem_keeps(s)) return false;
    if (!is_monolith(s)) return true;
    return monolith_ready(s) && monolith_grouped(s);
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
    // v158 (MONO_SESSIONS): the session estimate covers the team's strength, so a start needs MONO_START_NEAR near.
    int need = MONO_SESSIONS ? std::min(MONO_START_NEAR, living) : living;
    const bool sole = sole_monolith(s);
    // v156 (MONOLITH_GATHER): the team now heads there ungrouped, so heading there is no commitment: the start needs
    // MONOLITH_KEEP allies near (it is the goal, so the rest are on the way), healthy and with no enemy in sight.
    const bool gather = MONOLITH_GATHER && sole;
    if (sole) {
        // The monolith is the only gem source: someone is nearly always dead or recalling here, so the start
        // only needs MONOLITH_KEEP + 1 of the living allies (or all of them if fewer live).
        int alive = 0;
        for (const auto& ally : seen::roster(Team::ally))
            if (ally.is_alive()) ++alive;
        need = std::min(gather ? MONOLITH_KEEP : MONOLITH_KEEP + 1, alive);
    }
    if (!me_counted) ++near;
    // Once the team is on it (hitting it, or on a gem-starved map already heading there) the looser keep
    // rule holds, so one ally stepping away does not flip the target every round.
    const bool committed = mid_clear(s) || (sole && !gather && !route.empty() && slot % static_cast<int>(route.size()) == s);
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
    const auto n = route.size();
    if (gap_cache.size() != n * n) gap_cache.assign(n * n, GAP_UNKNOWN);
    int& cached = gap_cache[static_cast<std::size_t>(start) * n + static_cast<std::size_t>(goal)];
    if (cached == GAP_UNKNOWN) {
        const auto steps = steps_to(group_field(goal), spawn_lists[start]);
        cached = steps ? *steps : GAP_NONE;
    }
    if (cached == GAP_NONE) return std::nullopt;
    return cached;
}

bool Bot::mid_clear(int s) {
    // Straight off the vision list: building camps_at()'s list would allocate. v137: memo per slot and vision generation
    // (every action drops the lists, so a hit of ours this turn is seen).
    const auto gen = seen::generation();
    const bool memo = s >= 0 && s < static_cast<int>(route.size());
    if (memo) {
        if (mid_gen.size() != route.size()) {
            mid_gen.assign(route.size(), 0);
            mid_val.assign(route.size(), 0);
        }
        if (mid_gen[s] == gen) return mid_val[s] != 0;
    }
    bool started = false;
    for (const auto& camp : seen::camps())
        if (slot_at(camp.spawn_position()) == s && camp.health() < camp.max_health()) {
            started = true;
            break;
        }
    if (memo) {
        mid_gen[s] = gen;
        mid_val[s] = started ? 1 : 0;
    }
    return started;
}

std::vector<int>& Bot::visible_loot(int skip) {
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
    auto& result = loot_list;
    result.clear();
    for (const int s : loot)
        if (ready_at[s] <= now || rewards.count(s)) result.push_back(s);
    return result;
}

double Bot::route_rate_steps(int s, int steps) const {
    return route_rate_steps(s, steps, static_cast<int>(bc::round_number()));
}

double Bot::route_rate_steps(int s, int steps, int now) const {
    const int wait = std::max(0, ready_at[s] - now - steps);
    // v157 (MONO_SCALE): a damaged monolith costs only its remaining share of the clear.
    const double clear = MONO_SCALE && monolith_damaged(s) ? clears[s] * monolith_left(s) : clears[s];
    const double cycle = steps + wait + clear + capture_rounds;
    return values[aggressive][s] / std::max(1.0, cycle);
}

int Bot::best_goal(const Travel& travel) const {
    // On a gem-starved map the monolith decides the game (gems before gold), so once its gates let it into
    // the travel table it is the goal outright.
    if (gem_starved)
        for (const int m : monoliths)
            if (travel.has(m)) return m;
    // v98 (FIX_ALTAR_GATE): with every altar shut, the gate group is the goal once its gates let it into the table.
    if (FIX_ALTAR_GATE && (all_altars_shut() || (GATE_TEAM && gates_closed())))
        for (const int g : gate_slots)
            if (g >= 0 && travel.has(g)) return g;
    // v168 (PRISM_OUR_SIDE): the best group on our side up soon.
    if (PRISM_OUR_SIDE && prism_team()) {
        const int now = static_cast<int>(bc::round_number());
        int side_best = -1;
        double side_rate = 0;
        for (int s = 0; s < static_cast<int>(travel.steps.size()); ++s) {
            if (!travel.has(s) || gemless[s] || zone(s) != ZONE_SAFE || ready_at[s] - now - travel.at(s) > PRISM_SIDE_WAIT) continue;
            const double rate = route_rate_steps(s, travel.at(s), now);
            if (side_best < 0 || rate > side_rate) {
                side_best = s;
                side_rate = rate;
            }
        }
        if (side_best >= 0) return side_best;
    }
    // Planned pocket: gold-weighted while farming, the centre pocket once aggressive. Without one in the table, every
    // group in it, in slot order.
    // v99: two passes over the candidates instead of building two lists, and the round read once.
    // v144 (MIDDLE_FIRST): the centre pocket from round 0; (CONTEST_ANYWHERE) before aggressive, no pocket at all.
    const auto& core = cores[MIDDLE_FIRST ? 1 : aggressive];
    bool in_core = false;
    // v104 (INVADE_TIMERS): once aggressive, every open group is rated with its respawn wait instead of the centre
    // pocket alone (its respawn waits were 40-55% of later turns on ambush and corridors).
    // v177 (ECON_PHASES): once built, every group is rated (the edge elk groups lie outside a middle-line pocket).
    const bool phased = ECON_PHASES && !as_v170();
    const bool built = phased && priced_built;
    if (!(INVADE_TIMERS && aggressive) && !(CONTEST_ANYWHERE && !aggressive) && !built)
        for (const int s : core)
            if (travel.has(s)) in_core = true;
    const int count = in_core ? static_cast<int>(core.size()) : static_cast<int>(travel.steps.size());
    auto candidate = [&](int i) { return in_core ? core[i] : i; };
    // v72 (FIX_GEM_FIRST): a pocket with gem groups aims only at those; its gold clusters are fillers for the
    // respawn waits (pick_target's wait branch), not goals.
    // v177 (ECON_PHASES): before the team is built, gold-only groups may be the goal too.
    bool gems_only = false;
    if (FIX_GEM_FIRST && !(phased && !built))
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
        if (travel.has(s) && !is_monolith(s) && ready_at[s] - now - travel.at(s) <= MONOLITH_IDLE) last_resort = false;
    const int owned = team_emblems();
    const bool shut = FIX_ALTAR_GATE && (all_altars_shut() || (GATE_TEAM && gates_closed()));
    for (const int m : monoliths) {
        if (!travel.has(m)) continue;
        // v98 (FIX_ALTAR_GATE): a monolith shutting every altar is worth taking as soon as we are strong enough.
        const bool gate = shut && std::find(gate_slots.begin(), gate_slots.end(), m) != gate_slots.end();
        // v156 (MONOLITH_GATHER): the only monolith that scores stays the goal ungrouped, so the team gathers on it.
        // v157 (MONO_RESUME): a damaged one we left is a stop like one mid-clear, and the team gathers on it.
        // v158 (MONO_SESSIONS, MONO_EARLY): the session estimate replaces the emblem gate, and a monolith it allows is a
        // stop like any group, gathered on ungrouped.
        const bool damaged = monolith_damaged(m);
        const bool ready = MONO_SESSIONS ? monolith_ready(m) : owned >= monolith_need(m);
        const bool early = MONO_EARLY && MONO_SESSIONS && ready;
        if (ready
            && (last_resort || gate || mid_clear(m) || damaged || early)
            && ((MONOLITH_GATHER && sole_monolith(m)) || damaged || early || monolith_grouped(m)))
            continue;
        travel.steps[m] = -1;
        --travel.reachable;
    }
}

const Bot::Travel& Bot::travel(vec2 origin, int skip) {
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
    // Copy-assigned into the reused table: its storage stays, so no allocation once it has grown.
    auto& result = travel_out;
    result = travel_raw;
    // Monoliths are then dropped unless they are the last resort.
    drop_monoliths(result);
    if (SEALED_DROP) drop_sealed(result);
    // v86: zones the phase keeps closed, the pair's target (main team) or what the pair may not take, and groups too
    // far from a reward being captured outside the safe zone.
    // v177 (CLOSE_OUT): while we lead late, no group in the enemy's half.
    const bool closing = CLOSE_OUT_HALF && close_out();
    if (zones_known || capture_near >= 0 || TEAM_ABANDON || closing)
        for (int s = 0; s < n; ++s) {
            if (!result.has(s)) continue;
            bool drop = !zone_open(s) || (closing && enemy_half(centers[s]));
            if (pair_filter == 1 && s == target2) drop = true;
            if (pair_filter == 2
                && (!(satellite_on() ? sat_ok[s] : pair_ok[s]) || zone(s) > split_zone() || (main_slot >= 0 && distance(centers[s], centers[main_slot]) < split_apart())))
                drop = true;
            if (capture_near >= 0 && distance(centers[s], centers[capture_near]) > CAPTURE_NEAR) drop = true;
            if (abandoned(s)) drop = true;
            if (drop) {
                result.steps[s] = -1;
                --result.reachable;
            }
        }
    // v114 (HARD_GATE): last, so its fallback only looks at groups every other rule left open.
    drop_hard(result);
    drop_golems(result);
    return result;
}

bool Bot::monolith_up(int m) const {
    // Up unless respawning or its reward waits; unseen for GATE_MEMORY rounds it may have died (altar_open's rule).
    if (m < 0 || m >= static_cast<int>(ready_at.size())) return false;
    const int now = static_cast<int>(bc::round_number());
    if (ready_at[m] > now || rewards.count(m) > 0) return false;
    return m < static_cast<int>(seen_up.size()) && now - seen_up[m] <= GATE_MEMORY;
}

void Bot::drop_sealed(Travel& travel) {
    // v151-f4 (SEALED_DROP): one reach mask from our spawn with every monolith tile a wall, built on first use.
    const int n = static_cast<int>(route.size());
    if (!sealed_built) {
        sealed_built = true;
        sealed.assign(n, 0);
        if (monoliths.empty()) return;
        bc::bitboard blocks = seen::fixed_blocks();
        for (const int m : monoliths) blocks |= spawn_board(m);
        bc::bitboard open{bc::uninitialized};
        blocks.reachable_tiles_into(ally_spawn_list.front(), 0xFFFF, open);
        int count = 0;
        for (int s = 0; s < n; ++s)
            if (!is_monolith(s) && (spawn_board(s) & open).is_empty()) sealed[s] = 1, ++count;
        LOG(say(format("setup: %d groups lie behind a monolith", count)););
    }
    bool up = false;
    for (const int m : monoliths)
        if (monolith_up(m)) up = true;
    if (!up) return;
    for (int s = 0; s < n && s < static_cast<int>(sealed.size()); ++s)
        if (sealed[s] && travel.has(s)) {
            travel.steps[s] = -1;
            --travel.reachable;
        }
}

bool Bot::golem_keeps(int s) {
    // v137 (GOLEM_LAST): a golem group started, with its reward lying there, or shutting every altar is never dropped.
    if (rewards.count(s) || mid_clear(s)) return true;
    return FIX_ALTAR_GATE && all_altars_shut() && std::find(gate_slots.begin(), gate_slots.end(), s) != gate_slots.end();
}

void Bot::drop_golems(Travel& travel) {
    // v137 (GOLEM_LAST): wolves, elks and gem clusters first; golems only while none of those in reach is up within
    // GOLEM_IDLE rounds of our arrival. One pass to find an easier group, a second only when golems are in the table.
    if (!GOLEM_LAST || !any_golem) return;
    const int now = static_cast<int>(bc::round_number());
    bool easier = false, any = false;
    const int n = static_cast<int>(travel.steps.size());
    for (int s = 0; s < n && !(easier && any); ++s) {
        if (!travel.has(s)) continue;
        if (golem_flags[s]) {
            any = true;
        } else if (!easier && !is_monolith(s) && !gemless[s] && ready_at[s] - now - travel.at(s) <= GOLEM_IDLE) {
            easier = true;
        }
    }
    golem_held_round = now;
    golem_held = easier;
    if (!any || !easier) return;
    for (int s = 0; s < n; ++s) {
        if (!golem_flags[s] || !travel.has(s) || golem_keeps(s)) continue;
        travel.steps[s] = -1;
        --travel.reachable;
    }
}

bool Bot::golems_held() {
    // This round's answer from the last travel table; without one, any easier gem group up within GOLEM_IDLE rounds
    // in an open zone (no walking steps: a group out of reach counts, so golems stay held when unsure).
    const int now = static_cast<int>(bc::round_number());
    if (golem_held_round == now) return golem_held;
    golem_held_round = now;
    golem_held = false;
    for (int s = 0; s < static_cast<int>(route.size()) && !golem_held; ++s)
        if (!golem_flags[s] && !is_monolith(s) && !gemless[s] && ready_at[s] - now <= GOLEM_IDLE && zone_open(s)) golem_held = true;
    return golem_held;
}

int Bot::zone_cap_for(int ph) const {
    // Without zones (or a safe gem group) the old rules hold: everything but the unsafe zone stays open.
    if (!zones_known) return ZONE_UNSAFE;
    const int base = any_safe && V86_CENTRE ? ZONE_SAFE : ZONE_UNCERTAIN;
    if (ph == PHASE_RAID) return ZONE_UNSAFE;
    if (ph == PHASE_CENTRE) return ZONE_UNCERTAIN;
    // v159 (SATELLITE): a satellite split opens only with the centre open, which stays open for the main team.
    if (ph == PHASE_SPLIT && satellite_on()) return ZONE_UNCERTAIN;
    return base;
}

int Bot::roam_target(int s, int skip) {
    // v142 (NOTHING_ROAM): nothing we may farm in the table: the group nearest the map centre, one we may farm and is up
    // first, so the team roams the middle (gold, and enemies to meet) instead of waiting at a gated group.
    const vec2 mid{static_cast<int>(bc::map::width()) / 2, static_cast<int>(bc::map::height()) / 2};
    const int now = static_cast<int>(bc::round_number());
    int best = -1;
    std::pair<bool, int> best_key{};
    for (int u = 0; u < static_cast<int>(route.size()); ++u) {
        if (u == skip || is_monolith(u)) continue;
        const std::pair<bool, int> key{!(ready_at[u] <= now && allowed(u)), distance(centers[u], mid)};
        if (best < 0 || key < best_key) {
            best = u;
            best_key = key;
        }
    }
    if (best < 0) return s;
    LOG(if (best != s) say(format("nothing to farm in the table: roam to s%d near the centre", best)););
    return best;
}

int Bot::pick_target(int s, vec2 origin, int skip, bool seen_only) {
    // Walk toward the planned goal, but take every visible group that sits on that walk
    // (or close enough that turning aside is cheap). A group already under attack is finished.
    // The group being collected is skipped so everyone else moves on. Monoliths wait until
    // the team is strong enough. With seen_only, only loot this champion can see is offered.
    if (s != skip && mid_clear(s) && allowed(s) && !abandoned(s)) return s;
    auto& loot = visible_loot(skip);
    if (seen_only && loot.empty()) return s;
    const auto& steps = travel(origin, skip);
    if (steps.empty()) return NOTHING_ROAM && !seen_only ? roam_target(s, skip) : s;

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
    const bool team_of_prism = prism_team();
    constexpr int PRISM_LOOT_TIER = 1000;
    for (const int l : loot) {
        // v72: a gold cluster costs a clear and a five-round capture for 20 gold and no gems; ladder teams skip
        // them (we captured 2x as many on monolith, swirl and lanes), so one is only taken right beside us.
        if (FIX_GEM_FIRST && gemless[l] && steps.at(l) > gold_only_near()) continue;
        // v158: gold clusters only fill a wait for the gem goal, and gem clusters come before other loot on the way.
        // v168 (PRISM_OUR_SIDE): nor other-side loot while the goal is on our side.
        if (PRISM_OUR_SIDE && team_of_prism && zone(l) != ZONE_SAFE && zone(dest) == ZONE_SAFE) continue;
        if (team_of_prism && gemless[l] && steps.has(dest)
            && ready_at[dest] - static_cast<int>(bc::round_number()) <= steps.at(dest)) continue;
        if (extra(l) <= ON_WAY_SLACK || steps.at(l) <= NEAR_LOOT) {
            const std::tuple<int, double, int> key{steps.at(l) + (team_of_prism && !clusters_only[l] ? PRISM_LOOT_TIER : 0),
                                                   -values[aggressive][l], l};
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
        const double capture = capture_rounds;
        // Scored per round of the detour, so a near group beats a far one of the same worth; the filler we are already
        // on keeps it unless another scores SWITCH_MARGIN more (re-picking every round walked the team between groups
        // 20-35 tiles apart).
        int best = -1;
        double best_score = 0, current = 0;
        for (int l = 0; l < static_cast<int>(steps.steps.size()); ++l) {
            if (l == dest || !steps.has(l) || ready_at[l] - now > steps.at(l)) continue;
            if (FIX_GEM_FIRST && gemless[l] && steps.at(l) > gold_only_near()) continue;
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
    // v137 (SHARE_LOOT_TEAM): only with the team (before the pick: a straggler skips pick_target's travel table).
    if (SHARE_LOOT_TEAM) {
        bool with = false;
        const auto& tiles = seen::positions(Team::ally);
        const auto& allies = seen::champions(Team::ally);
        for (std::size_t i = 0; i < allies.size() && i < tiles.size() && !with; ++i)
            with = allies[i] != me && distance(tiles[i], position) <= TEAM_RANGE && !channelling(allies[i]);
        if (!with) return;
    }
    const int pick = pick_target(s, position, capture_slot, true);
    if (pick == s || distance(centers[pick], centers[s]) > distance(position, centers[s])) return;
    // v137 (SHARE_LOOT_COOL): we pointed here lately and the team is not on it: the leader steered away.
    const int now_round = static_cast<int>(bc::round_number());
    if (SHARE_LOOT_COOL && pick == loot_pointed && now_round - loot_pointed_round <= SHARE_LOOT_COOL_ROUNDS) {
        NOTE(format("sees loot at s%d again, but the leader steered away from it: follow s%d", pick, s));
        return;
    }
    loot_pointed = pick;
    loot_pointed_round = now_round;
    slot = pick;
    set_shared_byte(SLOT_BYTE, slot);
    hold_slot = slot;
    hold_since = now_round;
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
    const int was = capture;
    if (capture != NO_CAPTURE && (capture >= static_cast<int>(route.size()) || ready_at[capture] > now
                                  || ((now - when) & 0xFF) > CAPTURE_TIMEOUT || (FIX_CLAIM && collector_dead())))
        capture = NO_CAPTURE;
    // v137 (CAPTURE_NEWS): a claim that ended in a respawn tells the team, in the bytes the claim used.
    if (CAPTURE_NEWS && capture == NO_CAPTURE && was != NO_CAPTURE && was < static_cast<int>(route.size()) && ready_at[was] > now) {
        who = CAPTURED_MARK | ((ready_at[was] - route[was]->max_respawn_timer()) & CAPTURED_ROUND_MASK);
        when = was;
    }
    if (capture != NO_CAPTURE || rewards.empty()) return;
    // v135 (COLLECT_FIGHT): no new claim while the team fights (a collector off the reward gives it back then, and a
    // claim each round re-sent it every round: 01gold-rush r1015-1023).
    if (COLLECT_FIGHT && mode == FIGHT) return;

    const auto position = me.position();
    // v159 (SATELLITE): the satellite captures its own group; the main team's claims leave it.
    const int sat_slot = satellite_on() && phase == PHASE_SPLIT ? target2 : -1;
    // v161 (PRISM_PICKERS): the shared claim leaves their rewards and any with an ally already on it.
    const bool team_of_prism = PRISM_PICKERS && prism_team();
    int s = -1;
    for (const int r : rewards) {
        if (r == sat_slot) continue;
        if (team_of_prism) {
            if (picker_reward(r)) continue;
            auto held = spawn_board(r);
            held &= bc::vision::champion_tiles(Team::ally);
            if (!held.is_empty()) continue;
        }
        if (s < 0 || distance(position, centers[r]) < distance(position, centers[s])) s = r;
    }
    if (s < 0) return;
    // v135 (COLLECT_FIGHT): an enemy on the reward is fought first, not claimed.
    if (COLLECT_FIGHT) {
        auto held = spawn_board(s);
        held &= bc::vision::champion_tiles(Team::enemy);
        if (!held.is_empty()) {
            NOTE(format("an enemy stands on s%d's reward: no claim until it is gone", s));
            return;
        }
    }
    // Only a fresh sighting can claim this group again, so a stale record can't re-send a collector.
    rewards.erase(s);
    // v158: a free Prism takes a gem reward even out of the leader's sight, so nobody else splits or banks it.
    if (!PRISM_CLAIM_NEAR && prism_team() && !gemless[s] && prism_free() && can_reach(s, position, false)) {
        NOTE(format("prism team: s%d's reward is hers", s));
        capture = s;
        who = champ_id(Champ::prism) + 1;
        when = now;
        return;
    }
    auto can_walk = [&](const bc::champion& a) {
        if (!FIX_WALK_CLAIM) return true;
        if (!DASH_REACH) return walkable(s, a.position());
        return can_reach(s, a.position(), dash_kind(static_cast<Champ>(cached_ident(a) - 1)));
    };
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
            if (split_member(kind)) return false;
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
    // Vesper collects first while loot is funnelled to her (v71: only when she is near it); otherwise the poorest
    // visible ally, so the team catches up evenly. v71: the nearest, with CLAIM_WORTH_PER_TILE of item worth
    // counted as a tile, so the reward is not left waiting for someone across the map.
    // v110 (MARCELLA_CLAIM): with no funnelled Vesper near, a Marcella near it collects second.
    // v123 (PRISM_CLAIM): by collect_rank: Prism first on gem-only rewards, second on the rest.
    for (int rank = 1; rank <= 2 && builders.empty(); ++rank)
        for (const auto& a : allies)
            if (collect_rank(a, s) == rank
                && (nearest_of(a) <= VESPER_CLAIM_RANGE || (!FIX_CLAIM && a.champion_name() == Champ::vesper_vale)))
                builders.push_back(a);
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
    if (CAPTURE_NEWS && shared.capture_slot == NO_CAPTURE && !route.empty()) read_capture_news(shared.collector, shared.claimed);
    if (shared.mode != mode) mode_changed = now;
    // Only Vesper can read her own items, so she raises the shared flag once her core build is done.
    if (!shared.vesper_done && me.champion_name() == Champ::vesper_vale && emblem_count(me) >= VESPER_CORE_EMBLEMS) {
        shared.vesper_done = true;
        mark_vesper_done();
    }
    // v158: on the Prism team this flag says her two emblems are done, and no Vesper counts it.
    if (prism_team()) {
        prism_built = shared.vesper_done;
    } else {
        vesper_done = shared.vesper_done;
    }
    const int new_carrier = NO_ESCORT ? 0 : read_carrier();
    if (new_carrier != carrier) carrier_since = now;
    carrier = new_carrier;
    int carried = 0;
    for (const auto& ally : seen::roster(Team::ally)) carried += gems_carried(ally);
    win_tribute = static_cast<int>(bc::global_submitted_gems(Team::ally)) + carried >= WIN_GEMS
                  && !(GATE_TEAM && gates_closed());
    // v86: the phase and the pair's target; a split that ended since our last turn starts the cooldown.
    if (phase_seen == PHASE_SPLIT && shared.phase != PHASE_SPLIT) split_end_round = now;
    phase_seen = shared.phase;
    phase = V86_ZONES && zones_known ? shared.phase : PHASE_GROUP;
    target2 = shared.target2;
    zone_cap = zone_cap_for(phase);
    pair_filter = phase == PHASE_SPLIT ? (in_pair() ? 2 : 1) : 0;
    main_slot = route.empty() ? -1 : shared.slot % static_cast<int>(route.size());
    // v137 (TARGET_HOLD): every champion notes when the shared target last changed.
    if (TARGET_HOLD && shared.slot != hold_slot) {
        hold_slot = shared.slot;
        hold_since = now;
    }
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
    // v177 (CLOSE_OUT): never while we lead late.
    const bool aggressive_now = (shared.vesper_done || !drafted_vesper()) && team_emblems() >= TEAM_AGGRESSIVE_EMBLEMS
        && team_alive() && !close_out();
    if (aggressive_now != aggressive)
        NOTE(aggressive_now ? "aggressive route on: vesper built, team emblems reached, all alive"
                            : close_out() ? "aggressive route off: we lead, close the game out"
                            : (team_alive() ? "aggressive route off" : "aggressive route off: an ally is dead"));
    aggressive = aggressive_now;
    int s = route.empty() ? 0 : shared.slot % static_cast<int>(route.size());
    int capture = shared.capture_slot, who = shared.collector, when = shared.claimed;
    int new_phase = phase;
    std::string phase_why;
    if (!route.empty()) {
        if (TEAM_ABANDON) update_abandon(me, s);
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
        // v137 (TARGET_HOLD): a new target holds TARGET_HOLD_ROUNDS before another switch, unless the held one went
        // down, the new one is already being cleared, or a kill raid or split starts (a ping rally below still goes).
        if (TARGET_HOLD && !carrier && now - hold_since < TARGET_HOLD_ROUNDS) {
            const int held = shared.slot % static_cast<int>(route.size());
            // v159 (SATELLITE): a satellite split keeps the main team's hold (without it the main target flipped about
            // twice as often while split); the hold never takes the satellite's group, nor undoes the split.
            const bool sat_split = satellite_on() && (new_phase == PHASE_SPLIT || shared.phase == PHASE_SPLIT);
            if (s != held && ready_at[held] <= now && (sat_split ? held != target2 : new_phase != PHASE_SPLIT && shared.phase != PHASE_SPLIT)
                && !(new_phase == PHASE_RAID && recent_kill()) && !mid_clear(s) && allowed(held)) {
                LOG(phase_why += (phase_why.empty() ? "" : "; ")
                                 + format("holds s%d, chosen %d rounds ago (over s%d)", held, now - hold_since, s););
                s = held;
                if (!sat_split) new_phase = shared.phase;
            }
        }
        // v160 (PRISM_MOVE_WITH): no new group far from the reward she is capturing.
        if (PRISM_MOVE_WITH && prism_team() && capture != NO_CAPTURE && capture < static_cast<int>(route.size())
            && who == champ_id(Champ::prism) + 1 && distance(centers[s], centers[capture]) > PRISM_WITH_RANGE && !mid_clear(s)) {
            LOG(phase_why += (phase_why.empty() ? "" : "; ") + format("stays with her at s%d (over s%d)", capture, s););
            s = capture;
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
    write_shared(mode, slot, capture, who, when, shared.vesper_done, aggressive, NO_ESCORT ? shared_tank() : carrier, phase);
    if (TARGET_HOLD && slot != hold_slot) {
        hold_slot = slot;
        hold_since = now;
    }
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
    // v123 (PRISM_CLAIM): by collect_rank per reward: one first in line takes any claim; one second in line takes none
    // that a first-in-line collector holds, nor one with a first-in-line ally in sight within VESPER_CLAIM_RANGE of us.
    if (!vesper_funnel(me) && !marcella_collects(me) && !(PRISM_CLAIM && me.champion_name() == Champ::prism)
        && !prism_team()) return;
    // v158: allies take gold only with no claim open, or two of them would pass it back and forth.
    if (prism_team() && me.champion_name() != Champ::prism && capture_slot != NO_CAPTURE) return;
    if (is_collector(me) || under_threat(me) || route.empty()) return;
    // v135 (COLLECT_FIGHT): the team's fight comes first.
    if (COLLECT_FIGHT && mode == FIGHT && !near_enemies().empty()) return;
    const auto position = me.position();
    const bool dasher = dash_kind(me.champion_name());
    auto takes = [&](int r) {
        const int rank = collect_rank(me, r);
        if (rank == 0) return false;
        // v135 (DASH_REACH): never a reward we cannot get to, on foot or by dash.
        if (DASH_REACH && !can_reach(r, position, dasher)) return false;
        if (COLLECT_FIGHT) {
            auto held = spawn_board(r);
            held &= bc::vision::champion_tiles(Team::enemy);
            if (!held.is_empty()) return false;
        }
        if (rank == 1) return true;
        if (capture_slot != NO_CAPTURE && collector > 0
            && kind_rank(static_cast<Champ>(collector - 1), r, true) == 1) return false;
        for (const auto& ally : seen::champions(Team::ally))
            if (ally != me && collect_rank(ally, r) == 1 && distance(position, ally.position()) <= VESPER_CLAIM_RANGE)
                return false;
        return true;
    };
    int s = NO_CAPTURE;
    if (capture_slot != NO_CAPTURE && capture_slot < static_cast<int>(route.size())) {
        if (takes(capture_slot)) s = capture_slot;
    } else {
        for (const int r : rewards)
            if (distance(position, centers[r]) <= VESPER_CLAIM_RANGE && takes(r)
                && (s == NO_CAPTURE || distance(position, centers[r]) < distance(position, centers[s])))
                s = r;
    }
    if (s == NO_CAPTURE || distance(position, centers[s]) > VESPER_CLAIM_RANGE) return;
    const char* who_text = me.champion_name() == Champ::vesper_vale ? "vesper claims s%d (loot goes to her while she builds)"
        : me.champion_name() == Champ::prism ? (collect_rank(me, s) == 1 ? "prism claims s%d (gem-only: hers first)"
                                                                          : "prism claims s%d (no vesper near)")
        : "marcella claims s%d (no vesper near)";
    rewards.erase(s);
    const int now = static_cast<int>(bc::round_number());
    capture_slot = s;
    collector = champ_id(me.champion_name()) + 1;
    claimed = now;
    set_shared_byte(CAPTURE_SLOT_BYTE, s);
    set_shared_byte(CAPTURE_SLOT_BYTE + 1, collector);
    set_shared_byte(CLAIM_ROUND_OFFSET, now & 0xFF);
    NOTE(format(who_text, s));
}

void Bot::self_claim(const bc::champion& me) {
    if (COLLECT_FIGHT && mode == FIGHT) return;  // v135: the team's fight first
    // The leader only claims what it sees itself, so a reward in our own sight could wait for it to pass by.
    // With no claim open, no enemy near and nobody visible nearer to it, we claim it ourselves; a funnelled
    // Vesper in sight claims her own (vesper_claim()).
    if (capture_slot != NO_CAPTURE || rewards.empty() || route.empty() || channelling(me) || enemy_threat(me)) return;
    if (!near_enemies().empty()) return;
    const auto position = me.position();
    int s = -1;
    const int sat_slot = satellite_on() && phase == PHASE_SPLIT ? target2 : -1;
    const bool hers = !PRISM_CLAIM_NEAR && prism_team() && me.champion_name() != Champ::prism && prism_free();
    for (const int r : rewards)
        if (r != sat_slot && r < static_cast<int>(route.size()) && !(hers && !gemless[r]) && !picker_reward(r)
            && distance(position, centers[r]) <= SELF_CLAIM_RANGE
            && (!FIX_WALK_CLAIM || (DASH_REACH ? can_reach(r, position, dash_kind(me.champion_name())) : walkable(r, position)))
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
    // v123 (PRISM_CLAIM): by collect_rank; one first in line for the reward ignores everyone.
    const int my_rank = collect_rank(me, s);
    for (const auto& ally : seen::champions(Team::ally)) {
        if (ally == me || channelling(ally)) continue;
        if (my_rank == 1) break;
        const int theirs = nearest_of(ally.position());
        const int their_rank = collect_rank(ally, s);
        if (their_rank == 1 && theirs <= VESPER_CLAIM_RANGE) return;
        // v110 (MARCELLA_CLAIM): Marcella ignores nearer allies; the rest leave it to a Marcella near it.
        if (my_rank == 2) continue;
        if (theirs < mine || (their_rank == 2 && theirs <= VESPER_CLAIM_RANGE)) return;
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
        announce_capture(index);
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
        const bool dasher = ability_of(me, Kind::dash).has_value();
        if (DASH_REACH ? !can_reach(index, position, dasher) : (!dasher || nearest > WALK_CLAIM_DASH)) {
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
    if (!tiles.is_set(position) && collect_rank(me, index) == 0) {
        auto held = tiles;
        held &= bc::vision::champion_tiles(Team::ally);
        if (!held.is_empty()) {
            release_claim("an ally already stands on the reward");
            rewards.erase(index);
            return false;
        }
    }
    WHY("collect.loot", format("collector of s%d: ", index) + how);
    // v165 (COLLECT_SHOOT): a capture is not stopped by our own attacks, so on the reward we hit the weakest awake camp in
    // reach (never a sleeping one: that would wake a group onto us). Vesper sat idle on 41% of her collect turns.
    if (COLLECT_SHOOT && tiles.is_set(position) && me.can_attack()) {
        std::optional<bc::camp> pick;
        for (const auto& camp : seen::camps())
            if (camp.strength() > 0 && camp.health() > 0 && camp.health() < camp.max_health()
                && distance(position, camp.position()) <= static_cast<int>(me.attack_range())
                && (!pick || camp.health() < pick->health()))
                pick = camp;
        if (pick && attack_if_in_range(*pick)) NOTE("collect.shoot: hits an awake camp in reach while capturing");
    }
    // v135 (CLAIM_HANDOFF): a free reward tile we still cannot step toward is a blocked path (every tile taken is
    // not: whoever stands there captures it).
    bool free_goal = false;
    // A stun or a spent walk is not a blocked path (02gemfight r480: a stunned Vesper handed off mid-fight).
    if (CLAIM_HANDOFF && !tiles.is_set(position) && me.can_move()) {
        auto open = *goals;
        open &= ~bc::vision::champion_tiles(Team::ally);
        open &= ~bc::vision::champion_tiles(Team::enemy);
        // Tiles the walk guard holds (an awake camp's reach) wait for the camp to sleep: anyone else would wait too.
        if (walk_guard && !walk_guard->is_set(position)) open &= ~*walk_guard;
        free_goal = !open.is_empty();
    }
    const bool stepped = move_towards_closest(*goals);
    // v135 (CLAIM_HANDOFF): refused a step turn after turn off the reward: someone nearer takes it.
    if (CLAIM_HANDOFF && !tiles.is_set(position)) {
        const int now = static_cast<int>(bc::round_number());
        const bool blocked = !stepped && free_goal;
        // Debug only: what cuts the path (terrain, terrain + walk guard, terrain + units), and the goal tiles.
        LOG(if (!stepped) {
            bc::bitboard root{bc::uninitialized}, reach{bc::uninitialized}, wall{bc::uninitialized};
            bc::distance_field f;
            auto test = [&](const bc::bitboard& walls) {
                auto w = walls;
                w &= ~*goals;
                w.set(position, false);
                fast::around_into(position, 0, root);
                f.compute_to(w, *goals, root, reach);
                return reach.is_set(position);
            };
            const bool terrain = test(seen::terrain());
            wall = seen::terrain();
            if (walk_guard) wall |= *walk_guard;
            const bool guard = test(wall);
            blocked_tiles_into(wall);
            const bool units = test(wall);
            std::string g;
            for (const auto t : goals->positions()) g += " " + at(t);
            note(format("collect path: terrain %s, +guard %s, +units %s; goals", terrain ? "ok" : "CUT", guard ? "ok" : "CUT",
                        units ? "ok" : "CUT") + g + (walk_guard && walk_guard->is_set(position) ? "; we stand in the guard" : ""));
        });
        collect_stuck = !blocked ? 0 : (collect_stuck_round == now - 1 ? collect_stuck + 1 : 1);
        collect_stuck_round = now;
        if (collect_stuck >= CLAIM_STUCK) {
            collect_stuck = 0;
            hand_claim(me, index);
        }
    }
    return true;
}

bool Bot::can_reach(int s, vec2 from, bool dasher) {
    if (walkable(s, from)) return true;
    if (!DASH_REACH || !dasher || s < 0 || s >= static_cast<int>(route.size()) || bc::fuel_remaining() < HOP_FUEL) return false;
    return hop_steps(from, spawn_board(s)).has_value();
}

void Bot::hand_claim(const bc::champion& me, int index) {
    // The visible ally nearest the reward on foot (the group's walk field), not channelling, and nearer than us (no
    // hand back and forth between two blocked champions); none: we keep the claim and keep trying.
    const auto& allies = seen::champions(Team::ally);
    const auto& tiles = seen::positions(Team::ally);
    const bool fielded = index < static_cast<int>(goal_fields.size()) && goal_fields[index];
    int best = -1;
    unsigned best_steps = fielded ? goal_fields[index]->field.get(seen::self_position())
                                  : static_cast<unsigned>(distance(seen::self_position(), centers[index]));
    for (std::size_t i = 0; i < allies.size() && i < tiles.size(); ++i) {
        if (allies[i] == me || channelling(allies[i])) continue;
        unsigned steps = static_cast<unsigned>(distance(tiles[i], centers[index]));
        if (fielded) steps = goal_fields[index]->field.get(tiles[i]);
        if (steps < best_steps) {
            best_steps = steps;
            best = cached_ident(allies[i]);
        }
    }
    if (best < 0) {
        NOTE(format("refused a step toward s%d's reward, no ally nearer to take it: keep trying", index));
        return;
    }
    const int now = static_cast<int>(bc::round_number());
    collector = best;
    claimed = now;
    set_shared_byte(CAPTURE_SLOT_BYTE + 1, best);
    set_shared_byte(CLAIM_ROUND_OFFSET, now & 0xFF);
    NOTE(format("hands the claim on s%d to c%d: refused a step toward the reward %d turns running", index, best - 1, CLAIM_STUCK));
}

bool Bot::capture_help(const bc::champion& me) {
    // A Capture emblem caps the remaining capture timer at CAPTURE_EMBLEM_TIME at each capture phase it stands on the
    // region for, and the cut stays once it leaves while an ally keeps capturing. Rewards go to whoever is on the region
    // when the capture ends, so the owner steps on, waits for the cut, and steps off: the collector keeps the reward.
    if (!CAPTURE_HELP || route.empty() || is_collector(me) || channelling(me)) return false;
    if (!has(owned_of(me).passives, bc::item_family::capture)) return false;
    if (mode == FIGHT || mode == RETREAT || !near_enemies().empty() || enemy_threat(me)) return false;
    if (static_cast<int>(me.gem_count()) >= CARRY_GEMS || me.health() < me.max_health() * RECALL_HEALTH) return false;
    const auto position = seen::self_position();
    const int mine = cached_ident(me);
    const auto& ally_list = seen::champions(Team::ally);
    const auto& ally_tiles = seen::positions(Team::ally);
    for (int s = 0; s < static_cast<int>(route.size()); ++s) {
        if (distance(position, centers[s]) > CAPTURE_HELP_STEPS + 3 || !camps_at(s).empty()) continue;
        const bool claimed = capture_slot == s && collector != mine;
        bc::map::camp_group_capture_state st{};
        try {
            st = route[s]->capture_state();
        } catch (const bc::error&) {
            continue;
        }
        const bool capturing = st.state == bc::map::camp_group_capture_type::ally_capturing;
        if (!capturing && !(claimed && st.state == bc::map::camp_group_capture_type::capture_available)) continue;
        const auto& tiles = spawn_board(s);
        const bool on = tiles.is_set(position);
        bc::bitboard taken = bc::vision::champion_tiles(Team::ally);
        taken |= bc::vision::champion_tiles(Team::enemy);
        // Allies other than us on the region, one near it (no helping a capture nobody else comes to), and the claimed
        // collector's steps to a free reward tile.
        int others = 0, theirs = 1000;
        bool near_ally = false;
        for (std::size_t i = 0; i < ally_list.size(); ++i) {
            if (ally_list[i] == me) continue;
            if (tiles.is_set(ally_tiles[i])) ++others;
            if (distance(ally_tiles[i], centers[s]) <= CAPTURE_HELP_ALLY) near_ally = true;
            if (claimed && cached_ident(ally_list[i]) == collector)
                for (const auto t : spawn_lists[s])
                    if (!taken.is_set(t)) theirs = std::min(theirs, distance(ally_tiles[i], t));
        }
        auto step_off = [&](const char* why) {
            bc::bitboard off;
            off.draw_chebyshev(position, 1);
            off &= ~tiles;
            NOTE(format("capture.help.leave s%d (timer %d): ", s, st.timer) + why);
            move_towards_closest(off);
        };
        if (on) {
            if (others == 0) {
                // We act before the collector, so we can only step off once it stands on the region: hold while it can
                // still arrive with a capture phase to spare; a claim it cannot reach in time is not ours to take.
                if (claimed && theirs >= std::max(1, st.timer)) {
                    step_off("alone, the collector is too far to arrive in time");
                    return false;
                }
                WHY("capture.help.hold", format("s%d: alone on the reward (timer %d), hold it", s, st.timer));
                return true;
            }
            if (capturing && st.timer <= CAPTURE_EMBLEM_TIME) {
                step_off("timer cut, an ally holds it");
                return false;
            }
            WHY("capture.help.hold", format("s%d: on the reward until the timer is cut (now %d)", s, st.timer));
            return true;
        }
        if (!near_ally) continue;
        // Nearest free reward tile; arriving must still leave the timer over the cut.
        int d = 1000;
        for (const auto t : spawn_lists[s])
            if (!taken.is_set(t)) d = std::min(d, distance(position, t));
        if (d > CAPTURE_HELP_STEPS) continue;
        if (capturing && d >= st.timer - CAPTURE_EMBLEM_TIME) continue;
        // Not started: only with the collector in sight and no farther than us, so we never hold the reward alone
        // (we act first and could not step off before the collector steps on).
        if (!capturing && theirs > d) continue;
        auto goals = tiles;
        goals &= ~taken;
        WHY("capture.help.join", format("s%d: %s, reward %d away: join to cut the timer", s,
                                        capturing ? format("ally capturing (timer %d)", st.timer).c_str() : "claimed, not started", d));
        move_towards_closest(goals);
        return true;
    }
    return false;
}

namespace {
// v124 (FIGHT_ALLY_WEIGH, FIGHT_ENEMY_WEIGH): the share of allies that many tiles off who joined a fight within 4 rounds.
double join_weight(int tiles) {
    constexpr int last = static_cast<int>(std::size(JOIN_WEIGHT)) - 1;
    return JOIN_WEIGHT[std::clamp(tiles, 0, last)];
}

// v124 (FIGHT_ALLY_WEIGH): an ally our own rules keep out of a fight: under RECALL_HEALTH (recalls or dodges), or a
// carrier the front rule sends back (turn.cpp; never Prism, PRISM_CARRY_FIGHT).
bool stays_out(const bc::champion& a) {
    try {
        const double health = a.health(), top = a.max_health();
        if (health < top * RECALL_HEALTH) return true;
        // v158: the Prism team's bag is the game, so she does count as a carrier there.
        if (PRISM_CARRY_FIGHT && a.champion_name() == Champ::prism && !draft::prism_map()) return false;
        const int gems = static_cast<int>(a.gem_count());
        return (gems >= ESCORT_GEMS && health < FRONT_HEALTH && !draft::prism_map()) || (FIX_CARRIER && gems >= CARRY_GEMS && health < top * CARRY_FRONT_HEALTH);
    } catch (const bc::error&) {
        return false;
    }
}
}  // namespace

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
    const auto& allies = FIX_FIGHT_BODIES ? fighters : FIGHT_LOCAL ? fight_allies : seen_allies;
    // v81: everything but the distance and the ping bonus is the same for every origin, so it is worked out once per
    // round and vision list (score_mode runs twice on a leader's fight turn: the shared decision, then step 5) and
    // reused with its reason text. Fight bodies depend on the origin, so they are never cached.
    const bool cacheable = !FIX_FIGHT_BODIES;
    const auto round = bc::round_number();
    const bool hit = cacheable && score_base.round == round && score_base.enemies == near_src && score_base.allies == &seen_allies
        && score_base.valid;
    if (!hit) {
        if (FIGHT_LOCAL && !FIX_FIGHT_BODIES) {
            // v123: allies who can assist us: within FIGHT_LOCAL_RANGE (tiles, or walking steps with FIGHT_LOCAL_WALK)
            // of us or of the near enemy closest to us, and free. Built only on a cache miss, into a reused vector.
            // v124 (FIGHT_ALLY_WEIGH): one that stays out (stays_out) does not count; the rest by join_weight.
            const auto& self = bc::self();
            const vec2 here = self.position();
            vec2 front = enemies[0].position();
            for (const auto& e : enemies)
                if (distance(e.position(), here) < distance(front, here)) front = e.position();
            fight_allies.clear();
            fight_ally_w.clear();
            LOG(leavers_left = 0;);
            for (const auto& a : seen_allies) {
                if (a == self) {
                    fight_allies.push_back(a);
                    fight_ally_w.push_back(1.0);
                    continue;
                }
                if (channelling(a) || a.is_teleporting()) continue;
                const vec2 p = a.position();
                if (!assists(p, here) && !assists(p, front)) continue;
                if ((FIGHT_ALLY_WEIGH || FIGHT_SKIP_LEAVERS) && stays_out(a)
                    && !(FIGHT_SKIP_LEAVERS && !FIGHT_ALLY_WEIGH && LEAVERS_KEEP_HOR && a.champion_name() == Champ::heart_of_ruin)) {
                    LOG(++leavers_left;);
                    continue;
                }
                fight_allies.push_back(a);
                fight_ally_w.push_back(FIGHT_ALLY_WEIGH ? join_weight(std::min(distance(p, here), distance(p, front))) : 1.0);
            }
            // v124 (FIGHT_ENEMY_WEIGH): a busy (channelling) or hurt enemy does not count; the rest by join_weight from
            // the enemy nearest us (the one we would pick on).
            fight_enemy_w.clear();
            for (const auto& e : enemies) {
                double w = 1.0;
                if (FIGHT_ENEMY_WEIGH) {
                    try {
                        w = channelling(e) || e.is_teleporting() || e.health() < e.max_health() * RECALL_HEALTH
                                ? 0.0 : join_weight(distance(e.position(), front));
                    } catch (const bc::error&) {
                    }
                }
                fight_enemy_w.push_back(w);
            }
        }
        const bool local_w = FIGHT_LOCAL && !FIX_FIGHT_BODIES;
        auto weight = [&](std::size_t i) {
            if (FIX_FIGHT_BODIES) return i < fighter_weight.size() ? fighter_weight[i] : 1.0;
            return local_w && i < fight_ally_w.size() ? fight_ally_w[i] : 1.0;
        };
        auto enemy_weight = [&](std::size_t j) { return local_w && j < fight_enemy_w.size() ? fight_enemy_w[j] : 1.0; };
        const int ne = static_cast<int>(enemies.size());
        double na_w = 0.0, ne_w = 0.0;
        for (std::size_t i = 0; i < allies.size(); ++i) na_w += weight(i);
        for (std::size_t j = 0; j < enemies.size(); ++j) ne_w += enemy_weight(j);
        const int na = static_cast<int>(allies.size());

        double head = 0.0, tail = 0.0;
        // The reason text: each term's points, and the head count (v90-log: only inside LOG; no health or power totals).
        std::string head_text, tail_text;
        auto part = [](std::string& joined, const std::string& text) {
            if (!joined.empty()) joined += ", ";
            joined += text;
        };
        if (far_skipped) LOG(part(head_text, format("%d far enemies left out", far_skipped)););
        // v123 (FIGHT_LOCAL): allies in sight (our ward too) but too far or busy to assist.
        if (FIGHT_LOCAL && !FIX_FIGHT_BODIES && fight_allies.size() < seen_allies.size())
            LOG(part(head_text, format("%d far allies left out", static_cast<int>(seen_allies.size() - fight_allies.size()))););
        // v133 (FIGHT_SKIP_LEAVERS): of those, the ones leaving the fight.
        if (FIGHT_SKIP_LEAVERS) LOG(if (leavers_left) part(head_text, format("%d leaving allies not counted", leavers_left)););
        // v124: weighted heads (FIGHT_ALLY_WEIGH, FIGHT_ENEMY_WEIGH) are logged beside the plain count.
        if (na_w > ne_w) {
            head += OUTNUMBER_BONUS;
            LOG(part(head_text, format("bodies +%d %dv%d", OUTNUMBER_BONUS, na, ne)
                                     + (FIGHT_ALLY_WEIGH || FIGHT_ENEMY_WEIGH ? format(" (%.1fv%.1f)", na_w, ne_w) : std::string())););
        } else {
            const int pen = static_cast<int>(MISSING_BODY_PENALTY * (ne_w - na_w) + 0.5);
            head -= pen;
            LOG(part(head_text, format("bodies -%d %dv%d", pen, na, ne)
                                     + (FIGHT_ALLY_WEIGH || FIGHT_ENEMY_WEIGH ? format(" (%.1fv%.1f)", na_w, ne_w) : std::string())););
        }
        // v134 (FIGHT_ITEM_POWER): each side's cached emblem families, read once here.
        std::array<Passives, 8> ally_pass{}, enemy_pass{};
        for (std::size_t i = 0; i < allies.size() && i < ally_pass.size(); ++i) ally_pass[i] = owned_of(allies[i]).passives;
        for (std::size_t j = 0; j < enemies.size() && j < enemy_pass.size(); ++j) enemy_pass[j] = owned_of(enemies[j]).passives;
        auto taken = [&](const std::array<Passives, 8>& pass, std::size_t k) {
            return FIGHT_ITEM_POWER && k < pass.size() ? champ_taken_mult(pass[k]) : 1.0;
        };
        auto dealt = [&](const std::array<Passives, 8>& pass, std::size_t k) {
            return FIGHT_ITEM_POWER && k < pass.size() ? champ_dmg_mult(pass[k]) : 1.0;
        };
        PySum ally_hp, enemy_hp;
        // v101: a Heart of Ruin's shield counts as health on either side. v134: health / damage taken (Durability).
        for (std::size_t i = 0; i < allies.size(); ++i)
            ally_hp.add((allies[i].health() + hor_shield_hp(allies[i])) / taken(ally_pass, i) * weight(i));
        for (std::size_t j = 0; j < enemies.size(); ++j)
            enemy_hp.add((enemies[j].health() + hor_shield_hp(enemies[j])) / taken(enemy_pass, j) * enemy_weight(j));
        const double ally_health = ally_hp.value(), enemy_health = enemy_hp.value();
        // v115 (GRADED_EDGES): by how far ahead or behind, not just which side is.
        auto graded = [](double ours, double theirs) {
            return EDGE_WEIGHT * (ours - theirs) / std::max(1.0, std::max(ours, theirs));
        };
        double hp_edge = 0.0, power_edge = 0.0;
        if (KILL_TIME) {
            // v181 (KILL_TIME): rounds they need to kill us over rounds we need to kill them, in log. Our damage per round
            // over their effective health against theirs over ours: log(ally_health x ally_damage / (enemy_health x
            // enemy_damage)). One term for health and power; behind on it is behind on both.
            PySum ally_dmg, enemy_dmg;
            for (std::size_t i = 0; i < allies.size(); ++i)
                ally_dmg.add(round_damage(allies[i].champion_name(), allies[i].strength(), allies[i].focus())
                             * dealt(ally_pass, i) * weight(i));
            for (std::size_t j = 0; j < enemies.size(); ++j)
                enemy_dmg.add(round_damage(enemies[j].champion_name(), enemies[j].strength(), enemies[j].focus())
                              * dealt(enemy_pass, j) * enemy_weight(j));
            const double ours = std::max(1.0, ally_health) * std::max(1.0, ally_dmg.value());
            const double theirs = std::max(1.0, enemy_health) * std::max(1.0, enemy_dmg.value());
            const double kill = std::clamp(KILL_WEIGHT * std::log(ours / theirs), -KILL_CAP, KILL_CAP);
            hp_edge = power_edge = kill;
            head += kill;
            LOG(part(head_text, format("kill time %+.1f (damage %.0f v %.0f)", kill, ally_dmg.value(), enemy_dmg.value())););
        } else {
            hp_edge = GRADED_EDGES ? graded(ally_health, enemy_health)
                                   : ally_health > enemy_health ? HEALTH_EDGE : -HEALTH_EDGE;
            head += hp_edge;
            LOG(part(head_text, format("health %+.1f", hp_edge)););
            PySum ally_pw, enemy_pw;
            // v134 (FIGHT_ITEM_POWER): x damage dealt to champions (Skirmish, Collectors).
            for (std::size_t i = 0; i < allies.size(); ++i)
                ally_pw.add((static_cast<double>(allies[i].strength()) + allies[i].focus()) * dealt(ally_pass, i) * weight(i));
            for (std::size_t j = 0; j < enemies.size(); ++j)
                enemy_pw.add((static_cast<double>(enemies[j].strength()) + enemies[j].focus()) * dealt(enemy_pass, j) * enemy_weight(j));
            const double ally_power = ally_pw.value(), enemy_power = enemy_pw.value();
            power_edge = GRADED_EDGES ? graded(ally_power, enemy_power)
                                      : ally_power > enemy_power ? POWER_EDGE : -POWER_EDGE;
            head += power_edge;
            LOG(part(head_text, format("power %+.1f", power_edge)););
        }
        double owned_w = 0.0;
        // v134: the cached counts (update_emblems, the enemy sightings, our own shop), not an item read per champion.
        for (std::size_t i = 0; i < allies.size(); ++i) owned_w += owned_of(allies[i]).emblems * weight(i);
        // v115 (EMBLEM_RELATIVE): less the near enemies' emblems.
        if (EMBLEM_RELATIVE)
            for (std::size_t j = 0; j < enemies.size(); ++j) owned_w -= owned_of(enemies[j]).emblems * enemy_weight(j);
        // v181 (EMBLEM_GAP): ours less theirs, capped on the plus side.
        if (EMBLEM_GAP && !EMBLEM_RELATIVE)
            for (std::size_t j = 0; j < enemies.size(); ++j) owned_w -= owned_of(enemies[j]).emblems * enemy_weight(j);
        int emblem_score = EMBLEM_GAP ? static_cast<int>(std::lround(EMBLEM_GAP_BONUS * std::min(owned_w, EMBLEM_GAP_CAP)))
                                      : static_cast<int>(std::lround(EMBLEM_BONUS * owned_w));
        // v134: behind on both health and power (with the edges as scored), the emblems do not carry the fight.
        const bool behind_both = hp_edge < 0 && power_edge < 0;
        if (EMB_DOUBLE_DEFICIT && behind_both && !(na_w > ne_w) && emblem_score > 0) {
            LOG(part(head_text, format("emblems +%d dropped (behind on health and power)", emblem_score)););
            emblem_score = 0;
        }
        if (EMB_CAP_VALUE > 0 && emblem_score > EMB_CAP_VALUE) {
            LOG(part(head_text, format("emblems capped %d -> %d", emblem_score, EMB_CAP_VALUE)););
            emblem_score = EMB_CAP_VALUE;
        }
        head += emblem_score;
        if (emblem_score) LOG(part(head_text, format("emblems %+d", emblem_score)););
        // v134 (FIGHT_SKIRMISH, FIGHT_HUNTER): + per ally, - per enemy owning the emblem, weighted like the bodies.
        auto family_score = [&](bc::item_family f) {
            double s = 0;
            for (std::size_t i = 0; i < allies.size() && i < ally_pass.size(); ++i)
                if (has(ally_pass[i], f)) s += weight(i);
            for (std::size_t j = 0; j < enemies.size() && j < enemy_pass.size(); ++j)
                if (has(enemy_pass[j], f)) s -= enemy_weight(j);
            return s;
        };
        if (FIGHT_SKIRMISH) {
            const double s = SKIRMISH_SCORE * family_score(bc::item_family::skirmish);
            head += s;
            if (s != 0) LOG(part(head_text, format("skirmish %+.1f", s)););
        }
        if (FIGHT_HUNTER) {
            const double s = HUNTER_SCORE * family_score(bc::item_family::hunter);
            head += s;
            if (s != 0) LOG(part(head_text, format("hunter %+.1f", s)););
        }
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
        score_base = {round, near_src, &seen_allies, head, tail, std::move(head_text), std::move(tail_text), true, behind_both};
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
    // v177 (CLOSE_OUT): we lead late; a fight whose nearest enemy stands in their half is not worth the lead.
    bool far_fight = false;
    if (CLOSE_OUT && CLOSE_OUT_FIGHT && close_out()) {
        const auto from = origin ? *origin : bc::self().position();
        const bc::champion* nearest_enemy = &enemies[0];
        for (const auto& e : enemies)
            if (distance(e.position(), from) < distance(nearest_enemy->position(), from)) nearest_enemy = &e;
        far_fight = enemy_half(nearest_enemy->position());
        if (far_fight) score -= CLOSE_OUT_FIGHT_PENALTY;
    }
    // v134 (EMB_STAY_STRICT): no stay-in slack while behind on both health and power.
    const bool slack = current == FIGHT && !(EMB_STAY_STRICT && score_base.behind_both);
    // v181 (FIGHT_RECENTRE): the bar for a score without the emblem offset.
    const int base_bar = FIGHT_RECENTRE ? FIGHT_BAR : FIGHT_THRESHOLD;
    const int bar = slack ? base_bar - (FIGHT_RECENTRE ? FIGHT_STAY_SLACK : FIGHT_HYSTERESIS) : base_bar;
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
        if (far_fight) part(format("we lead, their half -%d", CLOSE_OUT_FIGHT_PENALTY));
        score_why = format("%s %.1f %s %d (", MODE_NAMES[choice], score, choice == FIGHT ? ">=" : "<", bar) + joined + ")";
    });
    return choice;
}

bool Bot::squishy_overextended(const std::vector<bc::champion>& allies, const std::vector<bc::champion>& enemies) {
    // Melee allies are the tanks. A squishy with none of them closer to the nearest enemy is in front.
    std::vector<bc::champion> tanks;
    for (const auto& ally : allies)
        if (ally.attack_range() <= 1) tanks.push_back(ally);
    // v133t: each enemy's and tank's tile read once, not once per comparison (a side has at most six champions).
    std::array<vec2, 16> spots, tank_at;
    std::size_t spot_n = 0, tank_n = 0;
    for (const auto& e : enemies)
        if (spot_n < spots.size()) spots[spot_n++] = e.position();
    for (const auto& t : tanks)
        if (tank_n < tank_at.size()) tank_at[tank_n++] = t.position();
    if (spot_n == 0) return false;
    for (const auto& squishy : allies) {
        if (squishy.max_health() > SQUISHY_HEALTH) continue;
        const auto here = squishy.position();
        std::size_t nearest = 0;
        for (std::size_t i = 1; i < spot_n; ++i)
            if (distance(here, spots[i]) < distance(here, spots[nearest])) nearest = i;
        const auto spot = spots[nearest];
        const int gap = distance(here, spot);
        bool covered = false;
        for (std::size_t i = 0; i < tank_n; ++i)
            if (distance(tank_at[i], spot) < gap) covered = true;
        if (!covered) return true;
    }
    return false;
}

bool Bot::willow_claim_risky(int s) {
    // Outside the safe zone, or with an enemy champion in sight.
    if (!seen::champions(Team::enemy).empty()) return true;
    return zones_known && s >= 0 && s < static_cast<int>(route.size()) && zone(s) != ZONE_SAFE;
}
