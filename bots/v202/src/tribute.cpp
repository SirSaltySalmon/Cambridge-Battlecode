// Banking gems, escorted tributes, and recalling to shop.
#include "bot.h"
#include <algorithm>
#include "seen.h"
#include "state.h"
#include "trace.h"
#include <tuple>

bool Bot::bank_gems(const bc::champion& me, const std::vector<vec2>& altars, const std::string& stem) {
    // A locked altar is still tried: the engine turns the tribute into an attack on the lock.
    const auto origin = me.position();
    bc::bitboard goals;
    const auto& spots = altars.empty() ? seen::altar_list() : altars;
    const int range = bc::constants::submit_gems_range;
    std::optional<vec2> held;
    for (const auto altar : spots) {
        const bool in_range = distance(origin, altar) <= range;
        if (!channel_cut && in_range) {
            // v150 (LOCK_PLACE): our lock goes on first; it never blocks our own tribute.
            if (LOCK_PLACE) place_lock(me);
            LOG({
                for (const auto& lock : bc::vision::altar_locks(Team::enemy))
                    if (lock.position() == altar)
                        note(format("altar %s has an enemy lock, %d hits left; submitting", at(altar).c_str(),
                                    static_cast<int>(lock.hits_remaining())));
            });
            try {
                me.submit_gems(altar);
                tributed = true;
                trace::act("tribute");
                WHY(stem + ".submit", "altar " + at(altar) + " in submit range: tribute");
                LOG(note(format("submit at %s: submitting_gems %d", at(altar).c_str(), me.is_submitting_gems() ? 1 : 0)););
                return true;
            } catch (const bc::error& error) {
                NOTE("tribute at " + at(altar) + " refused: " + error_text(error));
                continue;
            }
        }
        if (channel_cut && in_range && !held) held = altar;
        if (BANK_OPEN && !altar_usable(altar)) {
            NOTE("altar " + at(altar) + ": every submit tile is a wall or a living camp: not walked to");
            continue;
        }
        goals |= tiles_around(altar, static_cast<float>(range));
    }
    // v137 (BANK_OPEN): a camp on a submit tile is no goal (go() seeds its field from camp tiles, so the walk ended one
    // step short of an unstandable tile: 01shortcut 755 bounces). bank_free is this vision's ~camp tiles.
    if (BANK_OPEN && !goals.is_empty()) goals &= bank_free;
    if (BANK_OPEN && goals.is_empty()) return false;
    if (held)
        WHY(stem + ".walk", "tribute just cancelled: walk rather than restart it at " + at(*held));
    else
        LOG({
            std::optional<vec2> nearest;
            for (const auto altar : spots)
                if (!nearest || distance(origin, altar) < distance(origin, *nearest)) nearest = altar;
            branch(stem + ".walk", "walk to the altar at " + (nearest ? at(*nearest) : std::string("?")));
        });
    // v132d/e (CAMP_WAIT_WALK): around awake camps we just left; with no way around, wait for them.
    if (CAMP_WAIT_WALK && camp_wait_fight(me)) {
        if (move_towards_closest(goals, &fight_wait)) return true;
        WHY(stem + ".wait", "the way to the altar crosses awake camps we just left: wait for them");
        return true;
    }
    return move_towards_closest(goals);
}

bool Bot::altar_usable(vec2 altar) {
    // v137 (BANK_OPEN): a submit tile in sight that is neither fixed (wall, hole, altar) nor a living camp; with none in
    // sight, the gate record (altar_open). Board ops only (a per-tile loop was ~25 host reads x 3 per altar).
    // 02guardians: a camp on the one free tile beside an altar; Vesper walked up to it and away for 300 rounds.
    static bc::bitboard open_ground{bc::uninitialized}, around{bc::uninitialized}, part{bc::uninitialized};
    static bool open_built = false;
    if (!open_built) {
        fast::not_into(seen::fixed_blocks(), open_ground);
        open_built = true;
    }
    const auto gen = seen::generation();
    const int now = static_cast<int>(bc::round_number());
    if (bank_seen_round != now || bank_seen_gen != gen) {
        bc::vision::visible_tiles_into(bank_seen);
        bc::vision::camp_tiles_into(bank_camps);
        fast::not_into(bank_camps, bank_free);
        bank_seen_round = now;
        bank_seen_gen = gen;
    }
    fast::around_into(altar, static_cast<float>(bc::constants::submit_gems_range), around);
    around &= open_ground;
    fast::and_into(around, bank_seen, part);
    part &= bank_free;
    if (!part.is_empty()) return true;
    // Any open submit tile out of sight: the gate record decides.
    fast::not_into(bank_seen, part);
    part &= around;
    return !part.is_empty() && altar_open(altar);
}

bool Bot::altar_near(vec2 position, vec2 altar, int range) {
    // v122 (BANK_STEPS): within range by walking steps to a submit tile (range less the submit range, which matches
    // straight distance in the open), not by straight distance through walls. The field per altar is built once.
    if (!BANK_STEPS) return distance(position, altar) <= range;
    if (distance(position, altar) > range + BANK_STEPS_SLACK) return false;
    const auto& altars = seen::altar_list();
    const auto it = std::find(altars.begin(), altars.end(), altar);
    if (it == altars.end()) return distance(position, altar) <= range;
    const auto index = static_cast<std::size_t>(it - altars.begin());
    if (altar_walks.size() != altars.size()) altar_walks.resize(altars.size());
    auto& walk = altar_walks[index];
    if (!walk) walk = walk_field(tiles_around(altar, static_cast<float>(bc::constants::submit_gems_range)));
    const auto steps = walk->field.get(position);
    return steps != bc::distance_field::unreachable
           && static_cast<int>(steps) <= range - static_cast<int>(bc::constants::submit_gems_range);
}

// Escorted tribute

bool Bot::carrier_alive() const {
    // Out-of-sight allies may not reveal their type; an unknown carrier is assumed alive.
    for (const auto& ally : seen::roster(Team::ally)) {
        try {
            if (cached_ident(ally) == carrier) return ally.is_alive();
        } catch (const bc::error&) {
            continue;
        }
    }
    return true;
}

void Bot::update_carrier(const bc::champion& me) {
    // A heavy bag claims the single carrier field only when every altar is a contested one;
    // the carrier gives it up once home or banked.
    const int mine = cached_ident(me);
    const int gems = static_cast<int>(me.gem_count());
    const bool home = seen::ally_spawn().is_set(me.position());
    if (carrier == mine) {
        if (gems == 0 || home || me.is_recalling()) {
            carrier = 0;
            escort_wait = 0;
            write_carrier(0);
            NOTE(gems == 0 ? "escort over: bag banked" : "escort over: home or recalling");
        }
        return;
    }
    if (NO_ESCORT) return;
    // v123 (PRISM_HOLD): a holding Prism asks for no escort: she is not going to an altar.
    const bool building = (me.champion_name() == Champ::ren && emblem_count(me) >= REN_CORE_EMBLEMS) || prism_holds(me);
    if (!carrier && escort_bag(gems, owned_of(me).passives) && !building && !home && !rejoining && !me.is_recalling() && solo_altars.empty()) {
        carrier = mine;
        escort_wait = 0;
        write_carrier(mine);
        NOTE("asks for an escort: heavy bag and every altar is contested");
    }
}

std::optional<vec2> Bot::escort_altar() const {
    // Every champion derives the same altar from the shared target, which holds during an escort.
    if (escort_altars.empty() || route.empty()) return std::nullopt;
    const auto spot = centers[slot % static_cast<int>(route.size())];
    auto key = [&](vec2 a) { return std::make_tuple(distance(a, spot), a.x, a.y); };
    auto best = escort_altars.front();
    for (const auto altar : escort_altars)
        if (key(altar) < key(best)) best = altar;
    return best;
}

bool Bot::carry(const bc::champion& me) {
    // Regroup first: walk to the nearest ally, or wait beside the altar where escorts gather.
    const auto altar = escort_altar();
    if (!altar) {
        NOTE("carrier, but no escort altar: go on");
        return false;
    }
    std::vector<bc::champion> others;
    for (const auto& a : seen::champions(Team::ally))
        if (a != me && !channelling(a)) others.push_back(a);
    int living = 0;
    for (const auto& a : seen::roster(Team::ally))
        if (a != me && a.is_alive()) ++living;
    int escorts = 0;
    for (const auto& a : others)
        if (within(me, a, ESCORT_RANGE)) ++escorts;
    const int need = std::min(ESCORT_MIN, living);
    if (escorts >= need || escort_wait >= ESCORT_WAIT) {
        const bool ok = bank_gems(me, {*altar}, "carry");
        if (ok)
            LOG(branch_why = (escorts >= need ? format("%d escorts with us; ", escorts) : format("waited %d rounds for escorts; ", escort_wait))
                             + branch_why;);
        else
            NOTE("carrier bank failed: " + branch_why);
        return ok;
    }
    ++escort_wait;
    if (!others.empty()) {
        auto nearest = others.front();
        for (const auto& a : others)
            if (distance(me.position(), a.position()) < distance(me.position(), nearest.position())) nearest = a;
        WHY("carry.regroup", format("carrier with %d of %d escorts: walk to the nearest ally ", escorts, need) + at(nearest.position()));
        move_towards_closest(tiles_around(nearest.position(), 1));
    } else {
        WHY("carry.stage", format("carrier, no ally in sight: wait %d from the altar ", STAGING_RANGE) + at(*altar));
        move_towards_closest(tiles_around(*altar, STAGING_RANGE));
    }
    return true;
}

bool Bot::escort(const bc::champion& me) {
    // Stay beside the walking carrier; once it channels (or is out of sight) hold the altar.
    const auto altar = escort_altar();
    if (!altar) return false;
    std::optional<bc::champion> carried;
    for (const auto& a : seen::champions(Team::ally))
        if (cached_ident(a) == carrier) {
            carried = a;
            break;
        }
    if (carried && !carried->is_submitting_gems()) {
        const int g = distance(me.position(), carried->position());
        if (g > ESCORT_RANGE - 1) {
            WHY("escort.follow", format("carrier %d away: catch up", g));
            move_towards_closest(tiles_around(carried->position(), ESCORT_RANGE - 1));
        } else {
            WHY("escort.hold", "beside the carrier");
        }
        return true;
    }
    const bool building = (me.champion_name() == Champ::ren && emblem_count(me) >= REN_CORE_EMBLEMS) || prism_holds(me);
    if (!building && static_cast<int>(me.gem_count()) >= GEM_BANK && distance(me.position(), *altar) <= PROTECT_RANGE
        && bank_gems(me, {*altar}, "escort")) {
        LOG(branch_why = "carrier channelling, our bag is ready and the altar near; " + branch_why;);
        return true;
    }
    return guard(me, *altar);
}

bool Bot::guard(const bc::champion& me, vec2 altar) {
    // Farm free camps near the channelling carrier, never leaving protecting range of the altar.
    const auto position = me.position();
    std::vector<bc::camp> camps;
    for (const auto& c : seen::camps()) {
        const auto kind = c.kind();
        // Escorts guarding a channelling carrier only farm wolves and clusters.
        const bool free_kind = kind == bc::camp_type::wolf || kind == bc::camp_type::gem_cluster
            || kind == bc::camp_type::gold_cluster;
        if (free_kind && distance(c.position(), altar) <= PROTECT_RANGE) camps.push_back(c);
    }
    if (!camps.empty()) {
        std::vector<bc::camp> in_reach;
        for (const auto& c : camps)
            if (within(me, c, me.attack_range())) in_reach.push_back(c);
        auto target = lowest(in_reach);
        if (!target) {
            target = camps.front();
            for (const auto& c : camps)
                if (distance(position, c.position()) < distance(position, target->position())) target = c;
        }
        WHY("guard.camp", "carrier channelling: farm a free camp near the altar " + at(target->position()));
        engage(me, *target, {}, true);
    } else if (distance(position, altar) > STAGING_RANGE) {
        WHY("guard.close", "carrier channelling, no free camp: close in on the altar " + at(altar));
        move_towards_closest(tiles_around(altar, STAGING_RANGE));
    } else {
        WHY("guard.hold", "carrier channelling, no free camp: hold by the altar");
    }
    return true;
}

bool Bot::recall_to_shop(const bc::champion& me) {
    const auto position = me.position();
    int home = distance(position, ally_spawn_list.front());
    for (const auto tile : ally_spawn_list) home = std::min(home, distance(position, tile));
    // v134 (ITEM_CHANNELS): a Home recall is 3 ticks.
    const int recall_time = recall_ticks(owned_of(me).passives);
    if (home <= recall_time) {
        WHY("shop.walk", format("spawn %d away, no further than a recall (%d): walk", home, recall_time));
        const bool ok = move_towards_closest(seen::ally_spawn());
        if (!ok) NOTE("shop walk failed: " + trace::stall);
        return ok;
    }
    if (channel_cut) {
        NOTE("shop recall held: a channel was just cancelled");
        return false;
    }
    const auto& enemies = seen::champions(Team::enemy);
    if (recall_zone(enemies, true).is_set(position)) {
        LOG({
            const auto caster = RECALL_SPELL_CAUTION ? recall_caster(enemies, position) : std::nullopt;
            note(caster ? "shop recall held: a caster at " + at(caster->position()) + " would break it"
                        : std::string("shop recall held: an enemy could break it"));
        });
        return false;
    }
    try {
        me.recall();
        trace::act("recall");
        // v202 (SHOP_CADENCE): spaces the next shop recall (SHOP_SPACING).
        shop_trip_round = static_cast<int>(bc::round_number());
        WHY("shop.recall", format("spawn %d away, further than a recall (%d), no enemy can break it", home, recall_time));
        return true;
    } catch (const bc::error&) {
        return false;
    }
}

bool Bot::income_shop(const bc::champion& me, int now_mode) {
    // v202 (SHOP_INCOME): the purse goes home as it fills, on a quiet farm turn: once it finishes the emblem being built,
    // or holds three shards when the trip is cheap (hurt: the spawn heals; our own capture just landed; home and back to
    // the target ends about when we would start on it anyway). A recall keeps the bag, so no tribute comes first.
    if (!SHOP_CADENCE || !SHOP_INCOME || prism_team() || route.empty() || now_mode != FARM || channel_cut) return false;
    const int gold = static_cast<int>(me.gold_count());
    if (gold < SHOP_GOLD || turn_round - shop_trip_round < SHOP_SPACING) return false;
    if (is_collector(me) || !near_enemies().empty() || !seen::champions(Team::enemy).empty()) return false;
    const auto position = me.position();
    const int s = slot % static_cast<int>(route.size());
    // A started target group near us is finished first: the team's clear needs our hits.
    if (mid_clear(s) && distance(position, centers[s]) <= SHOP_FINISH_RANGE) return false;
    // v202 (merge fix): nor a group PT_COMMIT has us finishing after the team target moved on.
    if (PT_COMMIT && commit_slot >= 0 && commit_slot < static_cast<int>(route.size()) && turn_round - commit_round <= 1
        && mid_clear(commit_slot) && distance(position, centers[commit_slot]) <= SHOP_FINISH_RANGE)
        return false;
    const bool hurt = me.health() < me.max_health() * SHOP_HEALTH;
    const bool captured = turn_round - captured_round <= SHOP_AFTER_CAPTURE;
    // The trip to the target through the spawn (the recall's ticks, a healing turn there when hurt, the draft's walking
    // steps from the spawn to the group) against when we would start on it from here (our walk, or its respawn).
    int trip = -1, stay = 0;
    if (zones_known && s < static_cast<int>(home_steps.size()) && home_steps[s] < 255) {
        int here = distance(position, centers[s]);
        if (s < static_cast<int>(goal_fields.size()) && goal_fields[s]) {
            const auto steps = goal_fields[s]->field.get(position);
            if (steps != bc::distance_field::unreachable) here = static_cast<int>(steps);
        }
        stay = std::max(here, ready_at[s] - turn_round);
        trip = recall_ticks(owned_of(me).passives) + (hurt ? 1 : 0) + home_steps[s];
    }
    const bool free = trip >= 0 && trip <= stay + SHOP_FREE_SLACK;
    const int bar = shop_bar(me, hurt || captured || free);
    if (gold < bar) return false;
    if (!recall_to_shop(me)) {
        NOTE(format("income shop at %d gold (bar %d), but the recall was refused", gold, bar));
        return false;
    }
    LOG({
        const std::string why = hurt ? format("under %.0f%% health", SHOP_HEALTH * 100)
                                : captured ? std::string("our capture just landed")
                                : free ? format("the trip (%d) fits our way to s%d (%d)", trip, s, stay)
                                       : std::string("it finishes the emblem");
        branch_why = format("income: %d gold, bar %d (", gold, bar) + why + "); " + branch_why;
    });
    return true;
}

bool Bot::capture_recall(const bc::champion& me, int s) {
    // v202 (SHOP_CAPTURE_RECALL): capturing is standing still, and so is a recall. Started with the capture timer within
    // the recall's ticks, the reward lands in a capture phase before the recall ends at our turn start, so a collector
    // whose purse with its share of the reward reaches SHOP_GOLD goes home from the reward rather than after it.
    if (!SHOP_CADENCE || !SHOP_CAPTURE_RECALL || prism_team() || mode != FARM || channel_cut || me.is_recalling()) return false;
    if (turn_round - shop_trip_round < SHOP_SPACING || !near_enemies().empty() || !seen::champions(Team::enemy).empty())
        return false;
    const auto position = me.position();
    int home = distance(position, ally_spawn_list.front());
    for (const auto tile : ally_spawn_list) home = std::min(home, distance(position, tile));
    const int ticks = recall_ticks(owned_of(me).passives);
    // A walk home after the reward is as quick (income_shop does it).
    if (home <= ticks) return false;
    // Our share of the reward's gold: the allies on the region split it; Vesper's and Treasure's bonuses on top.
    auto on = spawn_board(s);
    on &= bc::vision::champion_tiles(Team::ally);
    const int sharers = std::max(1, static_cast<int>(on.count()));
    double bonus = 1.0;
    if (me.champion_name() == Champ::vesper_vale) bonus += VESPER_GOLD_BONUS;
    if (has(owned_of(me).passives, bc::item_family::treasure)) bonus += TREASURE_GOLD_BONUS;
    const int purse = static_cast<int>(me.gold_count()) + static_cast<int>(route[s]->gold_value() * bonus / sharers);
    if (purse < shop_bar(me, true)) return false;
    bc::map::camp_group_capture_state st{};
    try {
        st = route[s]->capture_state();
    } catch (const bc::error&) {
        return false;
    }
    if (st.state != bc::map::camp_group_capture_type::ally_capturing || st.timer > ticks) return false;
    const int before = static_cast<int>(me.gold_count()) + static_cast<int>(me.gem_count());
    if (!recall_to_shop(me)) return false;
    capture_home = s;
    capture_home_landed = turn_round + std::max(0, st.timer);
    capture_home_purse = before;
    LOG(branch_name = "capture." + branch_name;
        branch_why = format("s%d's capture timer %d is within the recall's %d ticks, purse %d with the reward; ", s, st.timer,
                            ticks, purse) + branch_why;);
    return true;
}

void Bot::capture_home_arrival(const bc::champion& me) {
    // v202 (SHOP_CAPTURE_RECALL): the recall we started on a reward is over. Home with more gold or gems than we left
    // with: the reward landed, so the group respawns from then and the claim is done (nothing else would tell us: it is
    // out of sight, and the claim would walk us back to it). Home without: the claim goes back for someone else. Broken
    // on the way (not home): we still stand on the reward and collect it as before.
    const int s = capture_home;
    capture_home = -1;
    if (s < 0 || s >= static_cast<int>(route.size()) || !seen::ally_spawn().is_set(me.position())) return;
    const int purse = static_cast<int>(me.gold_count()) + static_cast<int>(me.gem_count());
    if (purse > capture_home_purse) {
        ready_at[s] = std::max(ready_at[s], capture_home_landed + route[s]->max_respawn_timer());
        rewards.erase(s);
        if (capture_slot == s && is_collector(me)) {
            set_shared_byte(CAPTURE_SLOT_BYTE, NO_CAPTURE);
            capture_slot = NO_CAPTURE;
        }
        // v202 (merge fix): the news goes out whoever captured (a keeper, a hurt recall); announce_capture does nothing
        // while a claim is open.
        announce_capture(s);
        NOTE(format("home from s%d's reward: it landed, respawns in %d", s, ready_at[s] - turn_round));
    } else if (capture_slot == s && is_collector(me)) {
        release_claim("home from the reward, but it did not land");
    }
}
