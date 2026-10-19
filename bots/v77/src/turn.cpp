// What a champion does on its turn, in order.
#include "bot.h"
#include "seen.h"
#include "state.h"
#include "trace.h"
#include <exception>

void Bot::play_turn() {
    auto& me = bc::self();
    begin_trace();
    std::exception_ptr failure;
    const auto start_round = bc::round_number();
    try {
        play(me);
    } catch (...) {
        failure = std::current_exception();
        // A turn that ran out of fuel resumes next round at the same line, with vision lists a round old;
        // reading a unit that has since left sight throws. Play that round's turn again from scratch.
        // Costs nothing on normal turns: only a failed turn checks.
        if (bc::round_number() != start_round) {
            begin_trace();
            note(format("turn resumed after running out of fuel in round %u; replayed with fresh vision", start_round));
            failure = nullptr;
            try {
                play(me);
            } catch (...) {
                failure = std::current_exception();
            }
        }
    }
    // Like Python's finally: these run whether or not the turn failed, then the error goes on to main.
    try {
        remember_allies();
        if (FIX_HIDDEN || FIX_PING) remember_health(me);
    } catch (const std::exception& error) {
        say(std::string("ally memory error: ") + error_text(error));
    }
    try {
        log_turn(me);
    } catch (const std::exception& error) {
        say(std::string("log error: ") + error_text(error));
    }
    try {
        build_fields();
    } catch (const std::exception& error) {
        say(std::string("field build error: ") + error_text(error));
    }
    if (failure) std::rethrow_exception(failure);
}

void Bot::play(const bc::champion& me) {
    channel_cut = false;
    if (!ready) setup();
    if (!priority_set) {
        me.set_priority(priority_of(me.champion_name()));
        priority_set = true;
    }
    if (me.is_alive()) {
        observe();
        if (FIX_HIDDEN) check_hidden(me);
    }
    update_emblems();
    if (FIX_PING) update_ping(me);
    sync_decision(me);

    // Dead champions still get a turn to shop.
    if (!me.is_alive()) {
        buy_items(me);
        branch("dead.shop", "dead, buy from spawn shop");
        return;
    }
    update_carrier(me);

    // 1. Stay on a channel an enemy cannot hit. One step out is safe when it ends next turn.
    if (me.is_recalling() || me.is_submitting_gems() || me.is_teleporting()) {
        const std::string kind = me.is_recalling() ? "recall" : me.is_submitting_gems() ? "tribute" : "teleport";
        if (kind == "teleport") {
            branch("channel.teleport", "teleport cannot be cancelled, " + channel_left(me));
            return;
        }
        auto [threat, why] = channel_threat(me);
        // v71: someone hidden is hitting us, and keeps doing so until the channel would end: stop it.
        if (FIX_HIDDEN && !threat && hidden_until == static_cast<int>(bc::round_number()) + HIDDEN_ROUNDS - 1) {
            int left = 1;
            try {
                left = me.remaining_channel_time();
            } catch (const bc::error&) {
            }
            if (hidden_lost * (left + 1) >= static_cast<int>(me.health())) {
                threat = true;
                why = format("hidden attacker took %d, %d left would not survive", hidden_lost, left);
            }
        }
        if (!threat || !stop_channel(me)) {
            if (threat) why = "cancel rejected; " + why;
            branch("channel." + kind, why);
            return;
        }
        channel_cut = true;
        note("aborts " + kind + ": " + why);
    }

    // v71: someone hidden in a bush is hitting us: reveal them with the ward, or step out of their reach.
    if (FIX_HIDDEN && hidden_active() && answer_hidden(me)) return;

    // Placing a ward costs no move, attack or cast, so it is tried every turn; an unused one may go
    // into a key bush instead.
    // v72: with FIX_SCOUT_WARD the ward follows us instead; only kept posts (SCOUT_KEEP_POSTS) use the old rules.
    const bool scouting = FIX_SCOUT_WARD && (!SCOUT_KEEP_POSTS || (!ward_slot && !bush_warded));
    if (FIX_SCOUT_WARD && scout_ward(me)) {
    } else if (!scouting && !place_ward(me)) {
        bush_ward(me);
    }

    // 2. On spawn: buy, stay until healed (spawn heals at turn end), then head out.
    const auto position = me.position();
    if (!seen::ally_spawn().is_set(position)) spawn_waits = 0;
    if (seen::ally_spawn().is_set(position)) {
        tributed = false;
        buy_items(me);
        // v76 (FIX_STUCK_HEAL): the spawn heals to full at each turn end, so being short of full at turn start means an
        // enemy hit us since; waiting for full then never ends (rushbait_v2: ~320 rounds). After SPAWN_WAIT_LIMIT
        // waits in a row we play on from the spawn.
        const bool under_fire = FIX_STUCK_HEAL && spawn_waits >= SPAWN_WAIT_LIMIT;
        if (me.health() < me.max_health() && !under_fire) {
            ++spawn_waits;
            branch("spawn.heal", "on ally spawn, not full, wait for heal");
            return;
        }
        if (win_tribute && mode != FIGHT) {
            if (me.gem_count() > 0)
                bank_gems(me, {}, "win");
            else
                branch("win.tribute.wait", "no gems to submit; hold on spawn");
            return;
        }
        // v74: a champion that would bank off spawn banks from here; otherwise a bank walk back over the spawn
        // and spawn.leave undo each other every round (seen for up to ~440 rounds on corners, ambush, packs).
        if (FIX_SPAWN_BANK && bank_from_spawn(me)) return;
        if (!route.empty()) {
            branch("spawn.leave", "full hp, leave toward " + slot_text());
            move_towards_closest(spawn_board(slot % static_cast<int>(route.size())));
        } else {
            branch("spawn.stay", "full hp but no route");
        }
        return;
    }

    // 3. Recall when hurt and nothing can interrupt it; otherwise step out of reach first.
    // v72: a carrier recalls (or steps out of reach) earlier, since its death hands the bag over.
    const double recall_share = FIX_CARRIER && static_cast<int>(me.gem_count()) >= CARRY_GEMS ? CARRY_RECALL_HEALTH : RECALL_HEALTH;
    if (me.health() < me.max_health() * recall_share) {
        auto zone = threat_zone(seen::champions(Team::enemy), true);
        if (FIX_HIDDEN && hidden_active()) zone |= hidden_zone;
        const std::string hurt = format("below %.0f%% health", recall_share * 100);
        if (!channel_cut && !zone.is_set(position)) {
            try {
                me.recall();
                trace::act("recall");
                branch("hurt.recall", hurt + ", tile not in threat");
                return;
            } catch (const bc::error& error) {
                note(std::string("recall rejected (") + error_text(error) + ")");
            }
        }
        const std::string why = channel_cut ? "channel just aborted, not recalling again this turn" : "stepping out of threat";
        branch("hurt.dodge", hurt + ", " + why);
        if (FIX_DODGE)
            escape(me, zone);
        else
            move_towards_closest(~zone);
        return;
    }

    // 4. The one champion collecting a cleared group stays on it unless something can hit it.
    if (!win_tribute) {
        ren_claim(me);
        share_loot(me);
        if (FIX_SELF_CLAIM) self_claim(me);
        // v71: camp hits do not stop a capture, so only an enemy champion in reach sends the collector off
        // (collect() itself handles camps), and it gives the claim back for someone free to take.
        if (is_collector(me) && (FIX_CAMP_COLLECT ? enemy_threat(me) : under_threat(me))) {
            note(format("collector of slot %d skipped, under threat", capture_slot));
            if (FIX_CLAIM) release_claim("collector under threat");
        } else if (is_collector(me) && collect(me)) {
            return;
        }
    }

    // 5. Engaging outranks any tribute. The shared mode comes from the leader's vision, so enemies
    // in our own sight are scored here too. A carrier too hurt for the front line stays behind.
    const int gems = static_cast<int>(me.gem_count());
    if (gems > 0) tributed = false;
    // v70: Ren rushes his core build first; afterwards he is the team's carry.
    const bool ren = me.champion_name() == Champ::ren;
    const bool ren_rush = ren && emblem_count(me) < REN_CORE_EMBLEMS;
    int now_mode = mode;
    // Enemies far off are information only, so only the near ones trigger a rescore.
    if (!near_enemies().empty() && (now_mode == FARM || now_mode == TRIBUTE)) {
        const int personal = local_prev;
        now_mode = score_mode(position, personal);
        local_why = std::string("rescore shared ") + MODE_NAMES[mode] + " with personal " + MODE_NAMES[personal] + ": " + score_why;
        // A built Ren takes a fight his own score likes only with an ally beside him, not alone.
        if (ren && !ren_rush && now_mode == FIGHT) {
            bool backed = false;
            for (const auto& ally : seen::champions(Team::ally))
                if (ally != me && distance(position, ally.position()) <= REN_FIGHT_ALLY_RANGE) backed = true;
            if (!backed) {
                now_mode = mode;
                note(format("ren would fight but no ally within %d, keeps shared %s", REN_FIGHT_ALLY_RANGE, MODE_NAMES[mode]));
            }
        }
    }
    local_prev = now_mode;
    if (win_tribute && now_mode != FIGHT) {
        if (gems > 0)
            bank_gems(me, {}, "win");
        else
            branch("win.tribute.wait", "team can win; no gems to submit");
        return;
    }
    // v72: any carrier below CARRY_FRONT_HEALTH also stays behind the front, not only a heavy bag.
    const bool hurt_carrier = FIX_CARRIER && gems >= CARRY_GEMS && me.health() < me.max_health() * CARRY_FRONT_HEALTH;
    if (((gems >= ESCORT_GEMS && me.health() < FRONT_HEALTH) || hurt_carrier) && (now_mode == FIGHT || now_mode == RETREAT)) {
        if (retreat(me)) {
            branch_name = "front." + branch_name;
            branch_why = "heavy bag, too hurt for the front; " + branch_why;
            return;
        }
    } else if (now_mode == FIGHT) {
        if (fight(me)) return;
        note("mode fight but no living enemy, fall through");
    }
    // 5b. A built Ren banks his capped bag before anything but a fight (a retreat counts as one): alone,
    // at the nearest altar, and then recalls to shop once the tribute is in (step 8).
    if (ren && !ren_rush && gems >= REN_BANK_GEMS && now_mode != RETREAT && bank_gems(me, {}, "ren.bank")) {
        branch_why = format("ren bag %d >= cap %d; ", gems, REN_BANK_GEMS) + branch_why;
        return;
    }

    // 6. A heavy bag goes to the altar with the team; everyone else escorts it there.
    const int mine = champ_id(me.champion_name()) + 1;
    if (carrier == mine && carry(me)) return;
    if (now_mode == TRIBUTE && carrier != mine) {
        if (escort(me)) return;
        note("tribute but escort declined, fall through");
    }

    // v72: an altar close by makes banking cheap, so a small bag goes in before shopping or farming on.
    if (FIX_NEAR_BANK && gems >= NEAR_BANK_GEMS && now_mode != FIGHT && now_mode != RETREAT && !under_threat(me)) {
        std::vector<vec2> close;
        for (const auto altar : seen::altar_list())
            if (distance(position, altar) <= NEAR_BANK_RANGE) close.push_back(altar);
        if (!close.empty() && bank_gems(me, close, "bank.near")) return;
    }

    // v72: enemy champions in sight hunt bags: a carrier cashes in rather than farming on.
    if (FIX_CARRIER && gems >= CARRY_GEMS && now_mode != FIGHT && !seen::champions(Team::enemy).empty() && bank_gems(me, {}, "bank.hunted"))
        return;

    // 7a. While rushing his build, Ren goes to shop at every emblem's worth of gold, before banking and
    // with his bag still carried: items come first.
    if (ren_rush && static_cast<int>(me.gold_count()) >= REN_RECALL_GOLD && recall_to_shop(me)) {
        branch_why = "ren rush: purse covers an emblem; " + branch_why;
        return;
    }

    // 7. Bank a full bag, or a smaller one while the target is still respawning.
    // A built Ren keeps his bag until it reaches his cap (step 5b), so he never banks here.
    const bool building = ren && !ren_rush;
    // A heavy bag goes alone to a solo altar; a locked one still works, as tributing breaks the lock.
    const bool idle = !route.empty() && wait_at(slot % static_cast<int>(route.size()), position) > 0;
    if (!building && gems >= ESCORT_GEMS && carrier != mine) {
        if (!solo_altars.empty() && bank_gems(me, solo_altars, "bank.solo")) return;
    } else if (!building && (gems >= GEM_BANK || (idle && gems >= IDLE_BANK)) && bank_gems(me)) {
        return;
    }

    // 8. Return to shop at the gold threshold; Ren goes at every emblem's worth, with his bag still carried.
    const int recall_gold = ren_rush ? REN_RECALL_GOLD : RECALL_GOLD;
    const bool ready_to_shop = ren_rush || (tributed && gems == 0);
    if (static_cast<int>(me.gold_count()) >= recall_gold) {
        if (!ready_to_shop) {
            note(std::string("purse at recall bar but shop gated (ren ") + (ren ? "True" : "False") + ", tributed "
                 + (tributed ? "True" : "False") + ", building " + (building ? "True" : "False") + ")");
        } else if (recall_to_shop(me)) {
            branch_why = "purse at recall bar; " + branch_why;
            return;
        } else {
            note("purse at recall bar but recall refused");
        }
    }

    // 9-10. Follow the mode; a quiet farm turn may walk over to an uncovered ward spot.
    if (now_mode == RETREAT && retreat(me)) return;
    if (now_mode == FARM && !(FIX_SCOUT_WARD && !SCOUT_KEEP_POSTS) && ward_detour(me, gems)) return;
    if (now_mode != FARM && now_mode != RETREAT) note(std::string("mode ") + MODE_NAMES[now_mode] + " reached farm fallback");
    farm(me);
}

bool Bot::bank_from_spawn(const bc::champion& me) {
    // The off-spawn bank rules (5b Ren's cap, near-bank, hunted, 7), checked in the same order for a champion on
    // spawn, so whatever would send it to an altar off spawn also does on spawn.
    const int gems = static_cast<int>(me.gem_count());
    if (gems <= 0) return false;
    const auto position = me.position();
    const bool ren = me.champion_name() == Champ::ren;
    const bool ren_rush = ren && emblem_count(me) < REN_CORE_EMBLEMS;
    const bool building = ren && !ren_rush;
    if (building && gems >= REN_BANK_GEMS && mode != RETREAT && bank_gems(me, {}, "spawn.bank.ren")) return true;
    if (FIX_NEAR_BANK && gems >= NEAR_BANK_GEMS && mode != FIGHT && mode != RETREAT) {
        std::vector<vec2> close;
        for (const auto altar : seen::altar_list())
            if (distance(position, altar) <= NEAR_BANK_RANGE) close.push_back(altar);
        if (!close.empty() && bank_gems(me, close, "spawn.bank.near")) return true;
    }
    if (FIX_CARRIER && gems >= CARRY_GEMS && mode != FIGHT && !seen::champions(Team::enemy).empty()
        && bank_gems(me, {}, "spawn.bank.hunted"))
        return true;
    if (building) return false;
    const int mine = champ_id(me.champion_name()) + 1;
    if (gems >= ESCORT_GEMS && carrier != mine) return !solo_altars.empty() && bank_gems(me, solo_altars, "spawn.bank.solo");
    const bool idle = !route.empty() && wait_at(slot % static_cast<int>(route.size()), position) > 0;
    return (gems >= GEM_BANK || (idle && gems >= IDLE_BANK)) && bank_gems(me, {}, "spawn.bank");
}
