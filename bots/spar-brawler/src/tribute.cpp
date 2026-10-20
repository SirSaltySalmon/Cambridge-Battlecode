// Banking gems, escorted tributes, and recalling to shop.
#include "bot.h"
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
            try {
                me.submit_gems(altar);
                tributed = true;
                trace::act("tribute");
                branch(stem + ".submit", "altar " + at(altar) + " in submit range");
                return true;
            } catch (const bc::error&) {
                continue;
            }
        }
        if (channel_cut && in_range && !held) held = altar;
        goals |= tiles_around(altar, static_cast<float>(range));
    }
    std::optional<vec2> nearest;
    for (const auto altar : spots)
        if (!nearest || distance(origin, altar) < distance(origin, *nearest)) nearest = altar;
    const std::string where = nearest ? at(*nearest) : "none";
    if (held)
        branch(stem + ".walk", "tribute just aborted, altar " + at(*held) + " in submit range, walk instead of resubmitting");
    else
        branch(stem + ".walk", "walk toward altar " + where);
    return move_towards_closest(goals);
}

// Escorted tribute

bool Bot::carrier_alive() const {
    // Out-of-sight allies may not reveal their type; an unknown carrier is assumed alive.
    for (const auto& ally : seen::roster(Team::ally)) {
        try {
            if (champ_id(ally.champion_name()) + 1 == carrier) return ally.is_alive();
        } catch (const bc::error&) {
            continue;
        }
    }
    return true;
}

void Bot::update_carrier(const bc::champion& me) {
    // A heavy bag claims the single carrier field only when every altar is a contested one;
    // the carrier gives it up once home or banked.
    const int mine = champ_id(me.champion_name()) + 1;
    const int gems = static_cast<int>(me.gem_count());
    const bool home = seen::ally_spawn().is_set(me.position());
    if (carrier == mine) {
        if (gems == 0 || home || me.is_recalling()) {
            carrier = 0;
            escort_wait = 0;
            write_carrier(0);
            say(format("round %u: champion %d escort over", bc::round_number(), mine - 1));
        }
        return;
    }
    const bool building = me.champion_name() == Champ::ren && emblem_count(me) >= REN_CORE_EMBLEMS;
    if (!carrier && gems >= ESCORT_GEMS && !building && !home && !rejoining && !me.is_recalling() && solo_altars.empty()) {
        carrier = mine;
        escort_wait = 0;
        write_carrier(mine);
        say(format("round %u: champion %d asks for an escort with %d gems", bc::round_number(), mine - 1, gems));
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
        note("carrier but no escort altar, fall through");
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
            branch_why = format("escorts %d/%d, waited %d/%d; ", escorts, need, escort_wait, ESCORT_WAIT) + branch_why;
        else
            note("carry bank failed (" + branch_why + ")");
        return ok;
    }
    ++escort_wait;
    if (!others.empty()) {
        auto nearest = others.front();
        for (const auto& a : others)
            if (distance(me.position(), a.position()) < distance(me.position(), nearest.position())) nearest = a;
        branch("carry.regroup", format("escorts %d/%d, waited %d, walk to ally ", escorts, need, escort_wait) + at(nearest.position()));
        move_towards_closest(tiles_around(nearest.position(), 1));
    } else {
        branch("carry.stage", format("escorts %d/%d, no ally in vision, stage %d from altar ", escorts, need, STAGING_RANGE) + at(*altar));
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
        if (champ_id(a.champion_name()) + 1 == carrier) {
            carried = a;
            break;
        }
    if (carried && !carried->is_submitting_gems()) {
        const int g = distance(me.position(), carried->position());
        std::string cname;
        try {
            cname = std::to_string(champ_id(carried->champion_name()));
        } catch (const bc::error&) {
            cname = "?";
        }
        if (g > ESCORT_RANGE - 1) {
            branch("escort.follow", "carrier c" + cname + at(carried->position()) + format(" gap %d > %d", g, ESCORT_RANGE - 1));
            move_towards_closest(tiles_around(carried->position(), ESCORT_RANGE - 1));
        } else {
            branch("escort.hold", "beside carrier c" + cname + format(" gap %d <= %d", g, ESCORT_RANGE - 1));
        }
        return true;
    }
    const bool building = me.champion_name() == Champ::ren && emblem_count(me) >= REN_CORE_EMBLEMS;
    if (!building && static_cast<int>(me.gem_count()) >= GEM_BANK && distance(me.position(), *altar) <= PROTECT_RANGE
        && bank_gems(me, {*altar}, "escort")) {
        branch_why = format("carrier busy, bag ready within %d; ", PROTECT_RANGE) + branch_why;
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
        branch("guard.camp", "free camp " + at(target->position()) + (in_reach.empty() ? " closing" : " in range")
                                 + format(", within %d of altar ", PROTECT_RANGE) + at(altar));
        engage(me, *target, {}, true);
    } else if (distance(position, altar) > STAGING_RANGE) {
        branch("guard.close", format("no free camp, %d from altar ", distance(position, altar)) + at(altar) + format(" > %d", STAGING_RANGE));
        move_towards_closest(tiles_around(altar, STAGING_RANGE));
    } else {
        branch("guard.hold", format("no free camp, staged %d <= %d from altar ", distance(position, altar), STAGING_RANGE) + at(altar));
    }
    return true;
}

bool Bot::recall_to_shop(const bc::champion& me) {
    const auto position = me.position();
    int home = distance(position, ally_spawn_list.front());
    for (const auto tile : ally_spawn_list) home = std::min(home, distance(position, tile));
    const int recall_time = bc::constants::recall_time;
    if (home <= recall_time) {
        branch("shop.walk", format("spawn is %d tiles, recall takes %d", home, recall_time));
        const bool ok = move_towards_closest(seen::ally_spawn());
        if (!ok) note("shop walk failed: " + (trace::stall.empty() ? std::string("None") : trace::stall));
        return ok;
    }
    if (channel_cut) {
        note(format("shop recall held, channel just aborted, spawn is %d tiles away", home));
        return false;
    }
    const auto& enemies = seen::champions(Team::enemy);
    if (recall_zone(enemies, true).is_set(position)) {
        if (RECALL_SPELL_CAUTION)
            if (const auto caster = recall_caster(enemies, position))
                note(format("shop recall held: c%d %d off, within its reach %d + %d", champ_id(caster->champion_name()),
                            distance(position, caster->position()), recall_reach(*caster), RECALL_SPELL_STEP));
        return false;
    }
    try {
        me.recall();
        trace::act("recall");
        branch("shop.recall", format("spawn is %d tiles > recall %d, tile safe", home, recall_time));
        return true;
    } catch (const bc::error&) {
        return false;
    }
}
