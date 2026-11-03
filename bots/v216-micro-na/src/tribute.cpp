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
    const auto steps = altar_walk(static_cast<std::size_t>(it - altars.begin())).field.get(position);
    return steps != bc::distance_field::unreachable
           && static_cast<int>(steps) <= range - static_cast<int>(bc::constants::submit_gems_range);
}

const Walk& Bot::altar_walk(std::size_t index) {
    const auto& altars = seen::altar_list();
    if (altar_walks.size() != altars.size()) altar_walks.resize(altars.size());
    auto& walk = altar_walks[index];
    if (!walk) walk = walk_field(tiles_around(altars[index], static_cast<float>(bc::constants::submit_gems_range)));
    return *walk;
}

bool Bot::bank_on_way(const bc::champion& me, const std::string& stem) {
    // v157 (BANK_ON_WAY): detour = steps to the altar's submit tiles + the cheapest submit tile's steps to the target
    // - our steps to the target. Fields not built yet are skipped (build_fields makes the group ones from spare fuel).
    if (route.empty()) return false;
    const int target = slot % static_cast<int>(route.size());
    if (target >= static_cast<int>(goal_fields.size()) || !goal_fields[target]) return false;
    const auto& field = goal_fields[target]->field;
    const auto position = me.position();
    const auto direct = field.get(position);
    if (direct == bc::distance_field::unreachable) return false;
    const auto& altars = seen::altar_list();
    const int range = bc::constants::submit_gems_range;
    std::vector<vec2> close;
    for (std::size_t i = 0; i < altars.size(); ++i) {
        // A detour of ON_WAY_BANK_SLACK can't reach an altar farther than the target plus the slack.
        if (distance(position, altars[i]) > static_cast<int>(direct) + ON_WAY_BANK_SLACK + range) continue;
        if (i >= altar_walks.size() || !altar_walks[i]) {
            if (bc::fuel_remaining() < FIELD_FUEL_RESERVE + field_cost) continue;
            altar_walk(i);
        }
        const auto to_altar = altar_walks[i]->field.get(position);
        if (to_altar == bc::distance_field::unreachable) continue;
        int onward = -1;
        for (const auto tile : tiles_around(altars[i], static_cast<float>(range)).positions()) {
            const auto steps = field.get(tile);
            if (steps != bc::distance_field::unreachable && (onward < 0 || static_cast<int>(steps) < onward)) onward = static_cast<int>(steps);
        }
        if (onward < 0 || static_cast<int>(to_altar) + onward - static_cast<int>(direct) > ON_WAY_BANK_SLACK) continue;
        close.push_back(altars[i]);
    }
    if (close.empty()) return false;
    NOTE(format("altar on the way to s%d (slack %d): bank", target, ON_WAY_BANK_SLACK));
    return bank_gems(me, close, stem);
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

vec2 Bot::prism_anchor(vec2 altar) const {
    for (const auto& ally : seen::champions(Team::ally))
        if (ally.champion_name() == Champ::prism) return ally.position();
    return altar;
}

bool Bot::prism_threatened(vec2 altar) const {
    const vec2 anchor = prism_anchor(altar);
    for (const auto& e : seen::champions(Team::enemy))
        if (distance(e.position(), anchor) <= PRISM_GUARD_REACH) return true;
    return false;
}

bool Bot::prism_guard(const bc::champion& me, vec2 altar) {
    // v180 (PRISM_GUARD): her channel is the game. Hit whatever can reach her, never chasing it further; else clear an
    // enemy lock on her altar; else stand beside her, where we see (and Willow heals) what she sees.
    const auto position = me.position();
    const vec2 anchor = prism_anchor(altar);
    const bool seen_her = anchor != altar;
    std::vector<bc::champion> threats;
    for (const auto& e : seen::champions(Team::enemy))
        if (distance(e.position(), anchor) <= PRISM_GUARD_REACH) threats.push_back(e);
    if (!threats.empty()) {
        std::vector<bc::champion> in_reach;
        for (const auto& e : threats)
            if (within(me, e, me.attack_range())) in_reach.push_back(e);
        auto target = lowest(in_reach);
        if (!target) {
            // The one closest to her is the one hitting her.
            target = threats.front();
            for (const auto& e : threats)
                if (distance(e.position(), anchor) < distance(target->position(), anchor)) target = e;
        }
        WHY("prism.guard.hit", "an enemy at " + at(target->position()) + " can reach her at " + at(anchor));
        engage(me, *target, threats, false);
        return true;
    }
    if (distance(anchor, altar) <= PRISM_GUARD_RANGE + 1) {
        for (const auto& lock : bc::vision::altar_locks(Team::enemy)) {
            if (lock.position() != altar) continue;
            if (distance(position, altar) <= static_cast<int>(me.attack_range())) {
                try {
                    me.attack(lock);
                    trace::act("attack");
                    WHY("prism.guard.lock", "hit the enemy lock on her altar " + at(altar));
                    return true;
                } catch (const bc::error& error) {
                    NOTE("lock hit refused: " + error_text(error));
                }
            } else {
                WHY("prism.guard.lock", "close on the enemy lock on her altar " + at(altar));
                if (move_towards_closest(tiles_around(altar, static_cast<float>(me.attack_range())))) return true;
            }
        }
    }
    if (distance(position, anchor) > PRISM_GUARD_RANGE) {
        WHY("prism.guard.close", std::string("to her side at ") + at(anchor) + (seen_her ? "" : " (her altar, she is out of sight)"));
        move_towards_closest(tiles_around(anchor, static_cast<float>(PRISM_GUARD_RANGE)));
    } else {
        WHY("prism.guard.hold", "beside her at " + at(anchor) + ", nothing in reach of her");
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
        WHY("shop.recall", format("spawn %d away, further than a recall (%d), no enemy can break it", home, recall_time));
        return true;
    } catch (const bc::error&) {
        return false;
    }
}
