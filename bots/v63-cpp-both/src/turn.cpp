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
    try {
        play(me);
    } catch (...) {
        failure = std::current_exception();
    }
    // Like Python's finally: these run whether or not the turn failed, then the error goes on to main.
    try {
        remember_allies();
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
    if (me.is_alive()) observe();
    update_emblems();
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
        if (!threat || !stop_channel(me)) {
            if (threat) why = "cancel rejected; " + why;
            branch("channel." + kind, why);
            return;
        }
        channel_cut = true;
        note("aborts " + kind + ": " + why);
    }

    // Placing a ward costs no move, attack or cast, so it is tried every turn.
    place_ward(me);

    // 2. On spawn: buy, stay until healed (spawn heals at turn end), then head out.
    const auto position = me.position();
    if (seen::ally_spawn().is_set(position)) {
        tributed = false;
        buy_items(me);
        if (me.health() < me.max_health()) {
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
        if (!route.empty()) {
            branch("spawn.leave", "full hp, leave toward " + slot_text());
            move_towards_closest(spawn_board(slot % static_cast<int>(route.size())));
        } else {
            branch("spawn.stay", "full hp but no route");
        }
        return;
    }

    // 3. Recall when hurt and nothing can interrupt it; otherwise step out of reach first.
    if (me.health() < me.max_health() * RECALL_HEALTH) {
        const auto zone = threat_zone(seen::champions(Team::enemy), true);
        const std::string hurt = format("below %.0f%% health", RECALL_HEALTH * 100);
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
        move_towards_closest(~zone);
        return;
    }

    // 4. The one champion collecting a cleared group stays on it unless something can hit it.
    if (!win_tribute) {
        vesper_claim(me);
        share_loot(me);
        if (is_collector(me) && under_threat(me))
            note(format("collector of slot %d skipped, under threat", capture_slot));
        else if (is_collector(me) && collect(me))
            return;
    }

    // 5. Engaging outranks any tribute. The shared mode comes from the leader's vision, so enemies
    // in our own sight are scored here too. A carrier too hurt for the front line stays behind.
    const int gems = static_cast<int>(me.gem_count());
    if (gems > 0) tributed = false;
    int now_mode = mode;
    // Enemies healing on their own spawn are not worth a fight, so only the rest trigger a rescore.
    if (!fightable_enemies().empty() && (now_mode == FARM || now_mode == TRIBUTE)) {
        const int personal = local_prev;
        now_mode = score_mode(position, personal);
        local_why = std::string("rescore shared ") + MODE_NAMES[mode] + " with personal " + MODE_NAMES[personal] + ": " + score_why;
    }
    local_prev = now_mode;
    if (win_tribute && now_mode != FIGHT) {
        if (gems > 0)
            bank_gems(me, {}, "win");
        else
            branch("win.tribute.wait", "team can win; no gems to submit");
        return;
    }
    if (gems >= ESCORT_GEMS && me.health() < FRONT_HEALTH && (now_mode == FIGHT || now_mode == RETREAT)) {
        if (retreat(me)) {
            branch_name = "front." + branch_name;
            branch_why = "heavy bag, too hurt for the front; " + branch_why;
            return;
        }
    } else if (now_mode == FIGHT) {
        if (fight(me)) return;
        note("mode fight but no living enemy, fall through");
    }
    // 6. A heavy bag goes to the altar with the team; everyone else escorts it there.
    const int mine = champ_id(me.champion_name()) + 1;
    if (carrier == mine && carry(me)) return;
    if (now_mode == TRIBUTE && carrier != mine) {
        if (escort(me)) return;
        note("tribute but escort declined, fall through");
    }

    // 7. Bank a full bag, or a smaller one while the target is still respawning.
    // Vesper keeps farming instead of tributing until her build is done.
    const bool vesper = me.champion_name() == Champ::vesper_vale;
    const bool building = vesper && emblem_count(me) < VESPER_CORE_EMBLEMS;
    // A heavy bag goes alone to a solo altar; a locked one still works, as tributing breaks the lock.
    const bool idle = !route.empty() && wait_at(slot % static_cast<int>(route.size()), position) > 0;
    if (!building && gems >= ESCORT_GEMS && carrier != mine) {
        if (!solo_altars.empty() && bank_gems(me, solo_altars, "bank.solo")) return;
    } else if (!building && (gems >= GEM_BANK || (idle && gems >= IDLE_BANK)) && bank_gems(me)) {
        return;
    }

    // 8. Return to shop at the gold threshold; Vesper can recall with gems still carried.
    const int recall_gold = vesper ? vesper_recall_gold(me) : RECALL_GOLD;
    const bool ready_to_shop = vesper || (tributed && gems == 0);
    if (static_cast<int>(me.gold_count()) >= recall_gold) {
        if (!ready_to_shop) {
            note(std::string("purse at recall bar but shop gated (vesper ") + (vesper ? "True" : "False") + ", tributed "
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
    if (now_mode == FARM && ward_detour(me, gems)) return;
    if (now_mode != FARM && now_mode != RETREAT) note(std::string("mode ") + MODE_NAMES[now_mode] + " reached farm fallback");
    farm(me);
}
