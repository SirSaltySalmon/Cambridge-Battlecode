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
            NOTE(format("turn of round %u ran out of fuel; replayed with fresh vision", start_round));
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
    // v81: the decision log costs ~9k fuel a turn; inside debug_block it costs none (v90-log: all of it, see LOG).
    LOG(log_turn(me););
    try {
        build_fields();
    } catch (const std::exception& error) {
        say(std::string("field build error: ") + error_text(error));
    }
    if (failure) std::rethrow_exception(failure);
}

void Bot::play(const bc::champion& me) {
    channel_cut = false;
    if (!ready) {
        setup();
        // v99: a big map's setup leaves too little of the first turn to play it, and a turn that runs out resumes
        // next round anyway (v98 spilled setup into round 2 on islands and ambush). Stopping here instead plays round
        // 2 from a full budget.
        if (bc::fuel_remaining() < SETUP_PLAY_FUEL) {
            NOTE("setup used most of the first turn: play from the next");
            return;
        }
    }
    if (!priority_set) {
        me.set_priority(priority_of(me.champion_name()));
        priority_set = true;
    }
    if (me.is_alive()) {
        observe();
        if (FIX_HIDDEN) check_hidden(me);
    }
    update_emblems();
    if (STRONG_EARLY && ready) reprice();
    // v86: enemy deaths (for raids) and the emblems of enemies in sight; dead champions read them too.
    if (V86_ZONES && zones_known) update_enemies();
    if (INVADE && V86_ZONES && zones_known) update_invasion();
    if (FIX_PING) update_ping(me);
    sync_decision(me);

    // Dead champions still get a turn to shop.
    if (!me.is_alive()) {
        buy_items(me);
        WHY("dead.shop", "dead: shop from spawn");
        return;
    }
    update_carrier(me);

    // v117 (RECALL_CANCEL): healed back up during a low-health recall (and short of the shop bar): stay and play on.
    if (!me.is_recalling()) hurt_recall = false;
    if (RECALL_CANCEL && hurt_recall && me.is_recalling() && me.health() >= me.max_health() * RECALL_CANCEL_HEALTH
        && static_cast<int>(me.gold_count()) < RECALL_GOLD) {
        try {
            me.cancel_recall();
            hurt_recall = false;
            NOTE(format("recall cancelled: healed to %.0f%% and nothing to buy", 100.0 * me.health() / me.max_health()));
        } catch (const bc::error& error) {
            NOTE("recall cancel refused: " + error_text(error));
        }
    }
    // 1. Stay on a channel an enemy cannot hit. One step out is safe when it ends next turn.
    if (me.is_recalling() || me.is_submitting_gems() || me.is_teleporting()) {
        const std::string kind = me.is_recalling() ? "recall" : me.is_submitting_gems() ? "tribute" : "teleport";
        // v101 (WILLOW_CAMP_SAFETY): a recall still running this turn counts as tried, so a break next turn is seen.
        if (WILLOW_CAMP_SAFETY && kind == "recall" && me.champion_name() == Champ::willow)
            willow_recall_round = static_cast<int>(bc::round_number());
        if (kind == "teleport") {
            WHY("channel.teleport", "a teleport can't be cancelled");
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
                LOG(why = "a hidden attacker would kill us before it ends";);
            }
        }
        if (!threat || !stop_channel(me)) {
            LOG(if (threat) why = "cancel refused; " + why;);
            WHY("channel." + kind, why);
            return;
        }
        channel_cut = true;
        NOTE("cancels " + kind + ": " + why);
    }

    // v71: someone hidden in a bush is hitting us: reveal them with the ward, or step out of their reach.
    if (FIX_HIDDEN && hidden_active() && answer_hidden(me)) return;

    // Placing a ward costs no move, attack or cast, so it is tried every turn; an unused one may go
    // into a key bush instead.
    // v72: with FIX_SCOUT_WARD the ward follows us instead; only kept posts (SCOUT_KEEP_POSTS) use the old rules.
    const bool scouting = FIX_SCOUT_WARD && (!SCOUT_KEEP_POSTS || (!ward_slot && !bush_warded));
    // v81: a mage on a monolith keeps its ward for the monolith's bush (mono_ward).
    const bool mono_ward_due = monolith_delegated() && mono_job >= 0 && distance(me.position(), centers[mono_job]) <= MAGE_CAST_RANGE + 4;
    if (FIX_SCOUT_WARD && !mono_ward_due && scout_ward(me)) {
    } else if (!scouting && !place_ward(me)) {
        bush_ward(me);
    }

    // 2. On spawn: buy, stay until healed (spawn heals at turn end), then head out.
    const auto position = me.position();
    if (!seen::ally_spawn().is_set(position)) spawn_waits = 0;
    if (seen::ally_spawn().is_set(position)) {
        tributed = false;
        exit_hurt = false;
        buy_items(me);
        // v76 (FIX_STUCK_HEAL): the spawn heals to full at each turn end, so being short of full at turn start means an
        // enemy hit us since; waiting for full then never ends (rushbait_v2: ~320 rounds). After SPAWN_WAIT_LIMIT
        // waits in a row we play on from the spawn.
        const bool under_fire = FIX_STUCK_HEAL && spawn_waits >= SPAWN_WAIT_LIMIT;
        if (me.health() < me.max_health() && !under_fire) {
            ++spawn_waits;
            WHY("spawn.heal", "on spawn, not full: wait for the heal");
            return;
        }
        if (win_tribute && mode != FIGHT) {
            if (me.gem_count() > 0)
                bank_gems(me, {}, "win");
            else
                WHY("win.tribute.wait", "tribute wins the game; no gems, hold on spawn");
            return;
        }
        // v74: a champion that would bank off spawn banks from here; otherwise a bank walk back over the spawn
        // and spawn.leave undo each other every round (seen for up to ~440 rounds on corners, ambush, packs).
        if (FIX_SPAWN_BANK && bank_from_spawn(me)) return;
        if (!route.empty()) {
            WHY("spawn.leave", "healed: leave toward " + slot_text());
            move_towards_closest(spawn_board(slot % static_cast<int>(route.size())));
        } else {
            WHY("spawn.stay", "healed, but no route");
        }
        return;
    }

    // v85 (WILLOW_HEAL_STEP): Willow's heal allows walking, so it is spent at the start of any turn, not only on
    // attack turns (retreats, dodges and escorts are when allies need it). Cached vision is checked first, so the
    // ability is only read when a hurt ally is in reach.
    if (WILLOW_HEAL_STEP && me.champion_name() == Champ::willow && !channelling(me)) {
        bool hurt_near = false;
        const double need = WILLOW_HEAL_THRESHOLD ? HEAL_MIN_SHARE * 3.0 * me.focus() : 1.0;
        for (const auto& a : seen::champions(Team::ally))
            if (a != me && a.max_health() - a.health() >= need && distance(position, a.position()) <= HEAL_REACH
                && !(WILLOW_HEAL_SKIP_RECALL && a.is_recalling())) hurt_near = true;
        if (hurt_near) heal_lowest_ally(me);
    }

    // 3. Recall when hurt and nothing can interrupt it; otherwise step out of reach first.
    // v72: a carrier recalls (or steps out of reach) earlier, since its death hands the bag over.
    const double recall_share = FIX_CARRIER && static_cast<int>(me.gem_count()) >= CARRY_GEMS ? CARRY_RECALL_HEALTH : RECALL_HEALTH;
    // v86 (V86_FINISH): a started camp with nobody else around is finished first, down to FINISH_HEALTH.
    // v93 (V93_CAMP_EXIT): also hurt when our health would not outlast the walk out of the awake camps hitting us; at
    // the fixed bar a tank one tile from two elks was already too low to get out (48 of 66 melee deaths to camps died
    // on their first or second dodge turn, Cassi with every Issac).
    const bool under_bar = me.health() < me.max_health() * recall_share;
    // Over BAND_HEALTH the walk out is always affordable, so the camp scan (~1k fuel a turn) is skipped there.
    const bool banded = me.health() < me.max_health() * BAND_HEALTH;
    if (!banded) exit_hurt = false;
    const bool exit_due = V93_CAMP_EXIT && !under_bar && banded && (exit_hurt || me.health() <= camp_exit_cost(me));
    if (exit_due) exit_hurt = true;
    std::string finish_why;
    const bool finishing = V86_FINISH && (under_bar || exit_due) && finish_ok(me, finish_why);
    if (finishing) NOTE(finish_why);
    // v101 (HOR_HOLD): Heart of Ruin stays in a fight under the bar while his health and shields outlast the burst.
    if (!finishing && (under_bar || exit_due) && !hor_holds(me)) {
        const auto& enemies = seen::champions(Team::enemy);
        auto zone = recall_zone(enemies, true);
        if (FIX_HIDDEN && hidden_active()) zone |= hidden_zone;
        std::string hurt;
        LOG({
            hurt = under_bar ? format("under the %.0f%% recall bar", recall_share * 100)
                             : std::string("too low to walk out of the awake camps' reach later");
            if (RECALL_SPELL_CAUTION && zone.is_set(position))
                if (const auto caster = recall_caster(enemies, position))
                    hurt += "; a caster at " + at(caster->position()) + " would break a recall";
        });
        // v101 (WILLOW_CAMP_SAFETY): our recall last turn was broken by a hit from nothing in sight (a camp we cannot
        // see): restarting it here only gets it broken again, so step toward an ally or the spawn first.
        const int round_now = static_cast<int>(bc::round_number());
        if (WILLOW_CAMP_SAFETY && me.champion_name() == Champ::willow && willow_recall_round == round_now - 1
            && last_health_round == round_now - 1 && static_cast<int>(me.health()) < last_health) {
            std::optional<vec2> ally;
            for (const auto& a : seen::champions(Team::ally))
                if (a != me && !channelling(a) && (!ally || distance(position, a.position()) < distance(position, *ally))) ally = a.position();
            const auto goal = ally ? tiles_around(*ally, 1) : seen::ally_spawn();
            WHY("hurt.move", "recall broken by a hit from out of sight: step toward " + std::string(ally ? "an ally" : "the spawn"));
            if (!move_towards_closest(goal, &zone)) move_towards_closest(goal);
            return;
        }
        if (!channel_cut && !zone.is_set(position)) {
            if (me.champion_name() == Champ::willow) willow_recall_round = round_now;
            try {
                me.recall();
                hurt_recall = true;
                trace::act("recall");
                WHY("hurt.recall", hurt + "; no enemy reaches this tile");
                return;
            } catch (const bc::error& error) {
                NOTE("recall refused: " + error_text(error));
            }
        }
        // v105 (CAMP_EXIT_REACT): with an enemy near and awake camps hitting us, leave the camps first, away from it.
        if (CAMP_EXIT_REACT && me.champion_name() != Champ::willow && !near_enemies().empty()
            && camp_exit(me, near_enemies(), "hurt")) {
            if (me.champion_name() == Champ::heart_of_ruin) hor_shield(me);
            return;
        }
        WHY("hurt.dodge", hurt + (channel_cut ? "; channel just cancelled, step out of reach" : "; in enemy reach, step out first"));
        bool dodged = false;
        if (FIX_DODGE) {
            escape(me, zone);
            dodged = true;
        } else if (WILLOW_DODGE && me.champion_name() == Champ::willow) {
            // v85: the healer steps out of reach toward the nearest visible ally, who shields her and whom she heals.
            std::optional<vec2> ally;
            for (const auto& a : seen::champions(Team::ally))
                if (a != me && (!ally || distance(position, a.position()) < distance(position, *ally))) ally = a.position();
            if (ally) {
                auto near_ally = tiles_around(*ally, WILLOW_DODGE_RANGE);
                near_ally &= ~zone;
                // v101 (WILLOW_SAFE_DODGE): only along a path that stays out of reach (7 of her 10 camp deaths were
                // dodges toward an ally through a camp's reach); otherwise the nearest safe tile below.
                if (!near_ally.is_empty()) dodged = move_towards_closest(near_ally, WILLOW_SAFE_DODGE ? &zone : nullptr);
            }
        }
        if (!dodged) move_towards_closest(~zone);
        // v101: the shield allows walking; unless the dash took the turn's cast, it goes up on the way out.
        if (me.champion_name() == Champ::heart_of_ruin) hor_shield(me);
        return;
    }

    // v81 (PING_RETREAT): we called for help and are losing this fight alone: go to the help instead.
    if (FIX_PING && PING_RETREAT && retreat_to_allies(me)) return;

    // 4. The one champion collecting a cleared group stays on it unless something can hit it.
    if (!win_tribute) {
        vesper_claim(me);
        share_loot(me);
        // v86: the split pair captures its own rewards (play_pair).
        if (FIX_SELF_CLAIM && !pair_split()) self_claim(me);
        // v71: camp hits do not stop a capture, so only an enemy champion in reach sends the collector off
        // (collect() itself handles camps), and it gives the claim back for someone free to take.
        // v79 (PING_COLLECTOR): a fight beats a reward; a collector who can answer gives the claim back and goes.
        if (FIX_PING && PING_COLLECTOR && is_collector(me) && ping_slot && ping_mine_round < static_cast<int>(bc::round_number()) - PING_FRESH
            && can_answer(me, static_cast<int>(bc::round_number()))) {
            release_claim("answering a ping");
            if (answer_ping(me)) return;
        }
        // v101 (WILLOW_SAFE_CLAIM): Willow hands a claim back once an enemy champion is near.
        if (WILLOW_SAFE_CLAIM && is_collector(me) && me.champion_name() == Champ::willow && !near_enemies().empty())
            release_claim("willow: enemy near, a sturdier ally collects");
        // v101 (WILLOW_CLAIM_DROP): any enemy champion in her sight.
        else if (WILLOW_CLAIM_DROP && is_collector(me) && me.champion_name() == Champ::willow && !seen::champions(Team::enemy).empty())
            release_claim("willow: enemy champion in sight, a sturdier ally collects");
        if (is_collector(me) && (FIX_CAMP_COLLECT ? enemy_threat(me) : under_threat(me))) {
            NOTE(format("collector of s%d: under threat, leaves it", capture_slot));
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
    // v94: Vesper builds her core (Treasure, then three more) on loot funnelled to her.
    const bool vesper = me.champion_name() == Champ::vesper_vale;
    const bool vesper_building = vesper && emblem_count(me) < VESPER_CORE_EMBLEMS;
    int now_mode = mode;
    // v86 (V86_SPLIT): the main team's fight is not the pair's while it sees nobody (a ping still calls it).
    if (pair_split() && (now_mode == FIGHT || now_mode == RETREAT || now_mode == TRIBUTE) && near_enemies().empty()) now_mode = FARM;
    // Enemies far off are information only, so only the near ones trigger a rescore.
    if (!near_enemies().empty() && (now_mode == FARM || now_mode == TRIBUTE)) {
        const int personal = local_prev;
        now_mode = score_mode(position, personal);
        // v90-log: only a disagreement with the shared mode is worth a line.
        LOG({
            if (now_mode != mode && (now_mode != own_logged || mode != own_logged_shared))
                local_why = std::string("own sight disagrees with ") + MODE_NAMES[mode] + ": " + score_why;
            own_logged = now_mode != mode ? now_mode : -1;
            own_logged_shared = mode;
        });
        // A built Ren takes a fight his own score likes only with an ally beside him, not alone.
        if (ren && !ren_rush && now_mode == FIGHT) {
            bool backed = false;
            for (const auto& ally : seen::champions(Team::ally))
                if (ally != me && distance(position, ally.position()) <= REN_FIGHT_ALLY_RANGE) backed = true;
            if (!backed) {
                now_mode = mode;
                NOTE(format("ren won't fight alone (no ally within %d): keeps %s", REN_FIGHT_ALLY_RANGE, MODE_NAMES[mode]));
            }
        }
    }
    // v117 (TEAM_CHASE): a dropped chase holds for our own rescore too.
    update_team_chase(me, now_mode);
    if (now_mode == FIGHT && team_chase_off()) now_mode = FARM;
    local_prev = now_mode;
    if (win_tribute && now_mode != FIGHT) {
        if (gems > 0)
            bank_gems(me, {}, "win");
        else
            WHY("win.tribute.wait", "tribute wins the game; no gems to submit");
        return;
    }
    // v72: any carrier below CARRY_FRONT_HEALTH also stays behind the front, not only a heavy bag.
    const bool hurt_carrier = FIX_CARRIER && gems >= CARRY_GEMS && me.health() < me.max_health() * CARRY_FRONT_HEALTH;
    if (((gems >= ESCORT_GEMS && me.health() < FRONT_HEALTH) || hurt_carrier) && (now_mode == FIGHT || now_mode == RETREAT)) {
        if (retreat(me)) {
            LOG(branch_name = "front." + branch_name; branch_why = "heavy bag, too hurt for the front; " + branch_why;);
            return;
        }
    } else if (now_mode == FIGHT) {
        if (fight(me)) return;
        NOTE("fight mode, but no enemy to fight: go on");
    }
    // v78/v79 (PING_ANSWER_LIMIT): a nearby ally under attack is answered even while the team farms, before
    // any banking (v79 moved this up from the farm step).
    if (FIX_PING && PING_ANSWER_LIMIT && now_mode == FARM && answer_ping(me)) {
        LOG(branch_name = "ping.answer";);
        return;
    }
    // v86 (V86_BAND): 35-60% health: bank near, farm on with the team, or alone go safe (else recall).
    if (V86_BAND && zones_known && band_step(me, now_mode)) return;

    // 5b. A built Ren banks his capped bag before anything but a fight (a retreat counts as one): alone,
    // at the nearest altar, and then recalls to shop once the tribute is in (step 8).
    if (ren && !ren_rush && gems >= REN_BANK_GEMS && now_mode != RETREAT && bank_gems(me, {}, "ren.bank")) {
        LOG(branch_why = "ren's bag is at his cap; " + branch_why;);
        return;
    }

    // 6. A heavy bag goes to the altar with the team; everyone else escorts it there.
    const int mine = cached_ident(me);
    if (carrier == mine && carry(me)) return;
    if (now_mode == TRIBUTE && carrier != mine) {
        if (escort(me)) return;
        NOTE("tribute mode, but no escort to do: go on");
    }

    // v72: an altar close by makes banking cheap, so a small bag goes in before shopping or farming on.
    if (BANK_STICKY && gems < NEAR_BANK_GEMS) bank_altar.reset();
    if (FIX_NEAR_BANK && gems >= NEAR_BANK_GEMS && now_mode != FIGHT && now_mode != RETREAT && !under_threat(me)) {
        std::vector<vec2> close;
        for (const auto altar : seen::altar_list())
            if (altar_near(position, altar, NEAR_BANK_RANGE)) close.push_back(altar);
        // v122 (BANK_STICKY): the altar we set out for stays the goal for a while, in range or not.
        const int round = static_cast<int>(bc::round_number());
        if (BANK_STICKY && close.empty() && bank_altar && round - bank_round <= BANK_STICKY_ROUNDS) close.push_back(*bank_altar);
        if (!close.empty() && bank_gems(me, close, "bank.near")) {
            if (BANK_STICKY && !bank_altar) {
                bank_altar = close.front();
                for (const auto altar : close)
                    if (distance(position, altar) < distance(position, *bank_altar)) bank_altar = altar;
                bank_round = round;
            }
            return;
        }
        bank_altar.reset();
    }

    // v72: enemy champions in sight hunt bags: a carrier cashes in rather than farming on.
    if (FIX_CARRIER && gems >= CARRY_GEMS && now_mode != FIGHT && !seen::champions(Team::enemy).empty() && bank_gems(me, {}, "bank.hunted"))
        return;

    // 7a. While rushing his build, Ren goes to shop at every emblem's worth of gold, before banking and
    // with his bag still carried: items come first.
    if (ren_rush && static_cast<int>(me.gold_count()) >= REN_RECALL_GOLD && recall_to_shop(me)) {
        LOG(branch_why = "ren rush: gold covers an emblem; " + branch_why;);
        return;
    }
    // v94: Vesper too, at her next build step: the Treasure emblem first, then the rest of her core in one trip.
    if (vesper_building && static_cast<int>(me.gold_count()) >= vesper_recall_gold(me) && recall_to_shop(me)) {
        LOG(branch_why = (emblem_count(me) == 0 ? "vesper build: gold covers the treasure emblem; "
                                                : "vesper build: gold covers the rest of her core; ") + branch_why;);
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
    // v94: Vesper's bar is her build step, then VESPER_RECALL_GOLD batches once her core is done.
    const int recall_gold = ren_rush ? REN_RECALL_GOLD : vesper ? vesper_recall_gold(me) : RECALL_GOLD;
    // v86 (V86_GOLD_RECALL): no bank first (a recall keeps the bag) while the target is safe or nobody is with us.
    const bool v86_shop = V86_GOLD_RECALL && zones_known && !route.empty()
        && (zone(slot % static_cast<int>(route.size())) == ZONE_SAFE || !ally_near(me));
    const bool ready_to_shop = ren_rush || vesper_building || (tributed && gems == 0) || v86_shop;
    if (static_cast<int>(me.gold_count()) >= recall_gold) {
        if (!ready_to_shop) {
            NOTE(gems > 0 ? "gold at the shop bar; the bag is banked first" : "gold at the shop bar; waits for a tribute first");
        } else if (recall_to_shop(me)) {
            LOG(branch_why = "gold at the shop bar; " + branch_why;);
            return;
        } else {
            NOTE("gold at the shop bar, but the recall was refused");
        }
    }

    // 9-10. Follow the mode; a quiet farm turn may walk over to an uncovered ward spot.
    if (now_mode == RETREAT && retreat(me)) return;
    if (now_mode == FARM && !(FIX_SCOUT_WARD && !SCOUT_KEEP_POSTS) && ward_detour(me, gems)) return;
    // v81 (MAGE_MONOLITH): a mage spends quiet farm turns on a monolith, with spells from outside its reach.
    if (now_mode == FARM && is_mage(me.champion_name()) && monolith_delegated() && mage_monolith(me)) return;
    if (now_mode != FARM && now_mode != RETREAT) NOTE(std::string(MODE_NAMES[now_mode]) + " mode had nothing to do: farm");
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
            if (altar_near(position, altar, NEAR_BANK_RANGE)) close.push_back(altar);
        if (!close.empty() && bank_gems(me, close, "spawn.bank.near")) return true;
    }
    if (FIX_CARRIER && gems >= CARRY_GEMS && mode != FIGHT && !seen::champions(Team::enemy).empty()
        && bank_gems(me, {}, "spawn.bank.hunted"))
        return true;
    if (building) return false;
    const int mine = cached_ident(me);
    if (gems >= ESCORT_GEMS && carrier != mine) return !solo_altars.empty() && bank_gems(me, solo_altars, "spawn.bank.solo");
    const bool idle = !route.empty() && wait_at(slot % static_cast<int>(route.size()), position) > 0;
    return (gems >= GEM_BANK || (idle && gems >= IDLE_BANK)) && bank_gems(me, {}, "spawn.bank");
}
