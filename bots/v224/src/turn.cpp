// What a champion does on its turn, in order.
#include "bot.h"
#include "crystal.h"
#include "seen.h"
#include "state.h"
#include "trace.h"
#include <exception>

void Bot::play_turn() {
    auto& me = bc::self();
    // v202 (PREARM): cancel the recall armed at our last turn end before anything reads the channel.
    if (PREARM && prearmed) {
        prearmed = false;
        try {
            if (me.is_alive() && me.is_recalling()) me.cancel_recall();
        } catch (const std::exception&) {
        }
    }
    begin_trace();
    std::exception_ptr failure;
    const auto start_round = bc::round_number();
    // v221 (JOB_BREAKER): this turn's start tile and kind go into the history when the turn is over.
    job_turn = false;
    vec2 job_start{-1, -1};
    try {
        if (JOBS && JOB_BREAKER && me.is_alive()) job_start = me.position();
    } catch (const std::exception&) {
    }
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
    if (JOBS && JOB_BREAKER && job_start.x >= 0) {
        for (int i = 3; i > 0; --i) {
            job_hist_at[i] = job_hist_at[i - 1];
            job_hist_job[i] = job_hist_job[i - 1];
        }
        job_hist_at[0] = job_start;
        job_hist_job[0] = job_turn ? 1 : 0;
        if (job_hist_n < 4) ++job_hist_n;
    }
    try {
        claims_write();
    } catch (const std::exception& error) {
        say(std::string("claim write error: ") + error_text(error));
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
    // v202 (PREARM): the turn's last act, off spawn and with no channel running: arm a recall (cancelled next turn).
    if (PREARM && !failure) {
        try {
            if (me.is_alive() && !me.is_recalling() && !me.is_submitting_gems() && !me.is_teleporting()
                && !seen::ally_spawn().is_set(me.position())) {
                me.recall();
                prearmed = true;
            }
        } catch (const std::exception&) {
        }
    }
    if (failure) std::rethrow_exception(failure);
}

void Bot::play(const bc::champion& me) {
    channel_cut = false;
    crystal::gate = false;
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
    walk_guard = nullptr;
    land_bar = nullptr;
    turn_round = static_cast<int>(bc::round_number());
    guard_any = false;
    if (FOCUS && FOCUS_KEEP) watch_focus();
    if (me.is_alive()) {
        observe();
        prearm_observe();
        if (WALK_GUARD) build_walk_guard(me);
        if (SAFE_LAND && cached_ident(me) == champ_id(Champ::heart_of_ruin) + 1) build_land_bar(me);
        if (GO_KITE || CHASE_STEP || CATCH_ACROSS || RETREAT_LEAD) track_enemies();
        if (FIX_HIDDEN) check_hidden(me);
        LOG(peel_report(););
    }
    update_emblems();
    // v202 (EARLY_GOLD): gold-only groups' worth follows the team's emblems.
    if (SHOP_CADENCE && EARLY_GOLD) update_gold_fade();
    if ((STRONG_EARLY || ECON_PHASES || CLOSE_OUT) && ready) reprice();
    // v86: enemy deaths (for raids) and the emblems of enemies in sight; dead champions read them too.
    if (V86_ZONES && zones_known) update_enemies();
    if (invade_on() && V86_ZONES && zones_known) update_invasion();
    if (FIX_PING) update_ping(me);
    update_pickers();
    if (SPREAD || JOBS) spread_update(me);
    sync_decision(me);
    update_prism_cash(me);

    // Dead champions still get a turn to shop.
    if (!me.is_alive()) {
        buy_items(me);
        capture_home = -1;  // v202: a recall from a reward ends with us
        WHY("dead.shop", "dead: shop from spawn");
        return;
    }
    update_carrier(me);
    // v215 (TP): travel moves may launch a crystal: not fighting or retreating, no enemy champion in sight, healthy.
    crystal::gate = TP && crystal::carrying && crystal::map_on() && mode != FIGHT && mode != RETREAT
        && seen::champions(Team::enemy).empty() && me.health() >= me.max_health() * TP_HEALTH;

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
        // v158: damage does not break a tribute, and restarting her bag costs its whole length again.
        if (threat && kind == "tribute" && prism_team() && me.champion_name() == Champ::prism
            && me.health() >= me.max_health() * PRISM_CASH_STAY_HEALTH) {
            threat = false;
            LOG(why = "prism stays on her tribute: " + why;);
        }
        // v221 (TRIBUTE_STAY): for everyone, above TRIBUTE_STAY_HEALTH.
        if (TRIBUTE_STAY && threat && kind == "tribute" && me.health() >= me.max_health() * TRIBUTE_STAY_HEALTH) {
            threat = false;
            LOG(why = "stays on the tribute (damage does not break it): " + why;);
        }
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
    // v202 (SHOP_CAPTURE_RECALL): the recall we started on a reward is over (home, or broken on it).
    if (capture_home >= 0) capture_home_arrival(me);

    // v150 (LOCK_PLACE): a carried lock goes on any unlocked altar in reach (no action spent).
    if (LOCK_PLACE) place_lock(me);

    // v71: someone hidden in a bush is hitting us: reveal them with the ward, or step out of their reach.
    if (FIX_HIDDEN && hidden_active() && answer_hidden(me)) return;

    // Placing a ward costs no move, attack or cast, so it is tried every turn.
    // v140 (GHOST_WARD): the ward goes after a champion we lost sight of first; else (v72, FIX_SCOUT_WARD) it follows
    // us. v214-ward-cleanup: the fixed posts and key-bush ward that came after are gone (never reached since v72).
    if (!(GHOST_WARD && ghost_ward(me)) && FIX_SCOUT_WARD) scout_ward(me);

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
        // v158: the Prism team walks to her altar from spawn too; no gems is no reason to wait here.
        // v180 (PRISM_GUARD): her guards go to her in any mode.
        // v181 (PRISM_CASH_ANY_MODE): her cash-in goes ahead in any mode too.
        const bool guarding = (PRISM_GUARD && me.champion_name() != Champ::prism)
                              || (PRISM_CASH_ANY_MODE && me.champion_name() == Champ::prism && !prism_holds(me));
        if (prism_team() && (mode != FIGHT || guarding) && prism_cash_step(me, static_cast<int>(me.gem_count()))) return;
        if (win_tribute && mode != FIGHT && !prism_team()) {
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
            // v158: a collector leaves toward its reward: past our spawn, the walk there and spawn.leave undid each other.
            // v221 (JOBS): toward our own job (picked here when we have none): leaving toward the shared target and the
            // job's walk back over the spawn undid each other (04barbell: 4,411 spawn.leave turns, 0 gems).
            if (JOBS && spread_on() && job_taker && mode == FARM && seen::champions(Team::enemy).empty()
                && !(job_mode == JOB_MODE_CARRY && JOB_CARRY_STRENGTH > 0 && me.strength() < JOB_CARRY_STRENGTH)) {
                const int now_round = static_cast<int>(bc::round_number());
                if (spread_job >= 0 && ready_at[spread_job] > now_round + JOB_MAX_WALK) spread_job = -1;
                if (spread_job < 0 && now_round >= spread_cool_until) {
                    spread_job = job_pick(me);
                    spread_pick_round = now_round;
                }
            }
            const int leave_to = prism_team() && is_collector(me) && capture_slot < static_cast<int>(route.size())
                ? capture_slot : JOBS && spread_job >= 0 ? spread_job : slot % static_cast<int>(route.size());
            if (move_towards_closest(spawn_board(leave_to)) || !SPAWN_PLAY_ON) return;
            NOTE("spawn.leave found no step: play the turn from the spawn");
        } else {
            WHY("spawn.stay", "healed, but no route");
            return;
        }
    }

    // v85 (WILLOW_HEAL_STEP): Willow's heal allows walking, so it is spent at the start of any turn, not only on
    // attack turns (retreats, dodges and escorts are when allies need it). Cached vision is checked first, so the
    // ability is only read when a hurt ally is in reach.
    if (WILLOW_HEAL_STEP && me.champion_name() == Champ::willow && !channelling(me)) {
        bool hurt_near = false;
        const double need = WILLOW_HEAL_THRESHOLD ? HEAL_MIN_SHARE * 3.0 * me.focus() : 1.0;
        for (const auto& a : seen::champions(Team::ally))
            if (a != me && a.max_health() - a.health() >= need && distance(position, a.position()) <= HEAL_REACH
                && !(WILLOW_HEAL_SKIP_RECALL && recall_counts(a))) hurt_near = true;
        if (hurt_near) heal_lowest_ally(me);
    }

    // v129 (CAMP_EXIT_ALWAYS): awake camps not ours hit us: out of them first, away from any near enemy. Willow keeps
    // her backline step while an enemy is near.
    if (CAMP_EXIT_ALWAYS && !(me.champion_name() == Champ::willow && !near_enemies().empty()) && !is_collector(me)
        && !(CAPTURE_KEEP && keep_hold >= 0 && keep_hold == slot_at(position) && me.health() >= me.max_health() * COLLECT_CAMP_HEALTH)
        && exit_turn_steps(me) > 0 && foreign_camp_hit(me) && camp_exit(me, near_enemies(), "camp")) {
        if (me.champion_name() == Champ::heart_of_ruin) hor_shield(me);
        return;
    }

    // v159 (MONO_LEAVE): the champion a monolith hits leaves on its own walk-out bar, then rests until healed.
    if (mono_leave(me)) return;

    // 3. Recall when hurt and nothing can interrupt it; otherwise step out of reach first.
    // v72: a carrier recalls (or steps out of reach) earlier, since its death hands the bag over.
    double recall_share = FIX_CARRIER && static_cast<int>(me.gem_count()) >= CARRY_GEMS ? CARRY_RECALL_HEALTH : RECALL_HEALTH;
    if (PAIR_RECALL_HEALTH > 0 && (me.champion_name() == Champ::cassi || me.champion_name() == Champ::issac))
        recall_share = std::max(recall_share, PAIR_RECALL_HEALTH);
    if (HOR_RECALL_HEALTH > 0 && me.champion_name() == Champ::heart_of_ruin) recall_share = std::max(recall_share, HOR_RECALL_HEALTH);
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
    // v135 (TANK_SWAP): a sturdy ally can take the camps: stay behind it rather than leave.
    if ((under_bar || exit_due) && swap_hurt(me)) return;
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
            // v202d (CAPTURE_RECALL): on a reward whose capture would not land inside the recall: hold it first.
            if (hurt_capture_hold(me)) return;
            if (me.champion_name() == Champ::willow) willow_recall_round = round_now;
            try {
                me.recall();
                hurt_recall = true;
                trace::act("recall");
                WHY("hurt.recall", hurt + "; no enemy reaches this tile");
                // v202d (CAPTURE_RECALL): a recalling collector's claim goes back at once (it held the one claim up to
                // CAPTURE_TIMEOUT rounds); a reward under us still lands, and nobody is sent to the one we stand on.
                // v202 (merge fix): on a reward that lands inside the recall, its arrival bookkeeping instead (the claim
                // stays ours until we are home, and the news goes out then).
                const int on_slot = slot_at(position);
                const int on_left = CAPTURE_RECALL && !prism_team() && on_slot >= 0 && on_slot < static_cast<int>(route.size())
                                            && rewards.count(on_slot) ? capture_left(on_slot) : -1;
                if (on_left > 0 && on_left <= recall_ticks(owned_of(me).passives)) {
                    capture_home = on_slot;
                    capture_home_landed = turn_round + on_left;
                    capture_home_purse = static_cast<int>(me.gold_count()) + static_cast<int>(me.gem_count());
                } else if (CAPTURE_RECALL && !prism_team() && is_collector(me)) {
                    release_claim("recalls hurt");
                }
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
        if (WILLOW_DODGE && me.champion_name() == Champ::willow) {
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
        // v132: escape tools for champions without a dash (a Ryth / Prism stun spends the walk).
        if (!dodged && escape_tools(me)) dodged = true;
        // v137 (VESPER_ESCAPE): Vesper's own dodge by the enemies' walking steps.
        if (!dodged && vesper_escape(me, zone)) return;
        // v130 (ESCAPE_DASH): dash away first (escape_rank), then walk on unless the walk is spent.
        Go out;
        out.dash = getaway();
        out.walk = !dodged;
        out.guard_dash = GO_GUARD_DASH;
        out.rank = escape_rank(me, zone);
        if (!dodged || out.rank) go(~zone, out);
        // v101: the shield allows walking; unless the dash took the turn's cast, it goes up on the way out.
        if (me.champion_name() == Champ::heart_of_ruin) hor_shield(me);
        return;
    }

    // v81 (PING_RETREAT): we called for help and are losing this fight alone: go to the help instead.
    if (FIX_PING && PING_RETREAT && retreat_to_allies(me)) return;

    // v210 (OUTCLASS_FIRST): out-emblemed here, we leave before any claim, capture help or reward pick (each returned
    // first in v209, so a collector walked on into them); a collector gives its claim back.
    if (OUTCLASS_FIRST && OUTCLASS && !prism_team() && !win_tribute && outclass_step(me)) {
        if (is_collector(me)) release_claim("out-emblemed here: leave");
        return;
    }

    // 4. The one champion collecting a cleared group stays on it unless something can hit it.
    if (!win_tribute) {
        vesper_claim(me);
        share_loot(me);
        // v86: the split pair captures its own rewards (play_pair).
        if (FIX_SELF_CLAIM && !pair_split()) self_claim(me);
        // v71: camp hits do not stop a capture, so only an enemy champion in reach sends the collector off
        // (collect() itself handles camps), and it gives the claim back for someone free to take.
        // v79 (PING_COLLECTOR): a fight beats a reward; a collector who can answer gives the claim back and goes.
        // v128 (PING_IDLE_ONLY): a collector is busy and never gives its claim back for a ping.
        if (FIX_PING && PING_COLLECTOR && !PING_IDLE_ONLY && is_collector(me) && ping_slot
            && ping_mine_round < static_cast<int>(bc::round_number()) - PING_FRESH && can_answer(me, static_cast<int>(bc::round_number()))) {
            release_claim("answering a ping");
            if (answer_ping(me)) return;
        }
        // v101 (WILLOW_SAFE_CLAIM): Willow hands a claim back once an enemy champion is near.
        if (WILLOW_SAFE_CLAIM && is_collector(me) && me.champion_name() == Champ::willow && !near_enemies().empty())
            release_claim("willow: enemy near, a sturdier ally collects");
        // v101 (WILLOW_CLAIM_DROP): any enemy champion in her sight.
        else if (WILLOW_CLAIM_DROP && is_collector(me) && me.champion_name() == Champ::willow && !seen::champions(Team::enemy).empty())
            release_claim("willow: enemy champion in sight, a sturdier ally collects");
        // v135 (COLLECT_FIGHT): an enemy on the reward, or the team fighting with an enemy near before we stand on it.
        if (COLLECT_FIGHT && is_collector(me) && capture_slot < static_cast<int>(route.size())) {
            const auto& tiles = spawn_board(capture_slot);
            auto held = tiles;
            held &= bc::vision::champion_tiles(Team::enemy);
            if (!held.is_empty())
                release_claim("an enemy stands on the reward: fight first");
            else if (!tiles.is_set(position) && mode == FIGHT && !near_enemies().empty())
                release_claim("the team fights an enemy near us before we reach the reward: fight first");
        }
        if (is_collector(me) && (FIX_CAMP_COLLECT ? enemy_threat(me) : under_threat(me))) {
            NOTE(format("collector of s%d: under threat, leaves it", capture_slot));
            if (FIX_CLAIM) release_claim("collector under threat");
        } else if (is_collector(me) && collect(me)) {
            return;
        }
        if (capture_help(me)) return;
        // v202d (CAPTURE_KEEP, CAPTURE_CLEARER): on a cleared reward, hold it; just cleared one within reach, walk on.
        if (keep_capture(me)) return;
        if (picker_collect(me)) return;
        if (mode != FIGHT && near_enemies().empty() && leave_prism_reward(me)) return;
    }

    // 5. Engaging outranks any tribute. The shared mode comes from the leader's vision, so enemies
    // in our own sight are scored here too. A carrier too hurt for the front line stays behind.
    const int gems = static_cast<int>(me.gem_count());
    if (gems > 0) tributed = false;
    // v123 (PRISM_HOLD): Prism keeps her bag; none of the bank steps below apply to her until she may cash in.
    const bool holding = prism_holds(me);
    // v70: Ren rushes his core build first; afterwards he is the team's carry.
    const bool ren = me.champion_name() == Champ::ren;
    const bool ren_rush = ren && emblem_count(me) < REN_CORE_EMBLEMS;
    // v94: Vesper builds her core (Treasure, then three more) on loot funnelled to her.
    const bool vesper = me.champion_name() == Champ::vesper_vale;
    const bool vesper_building = vesper && emblem_count(me) < VESPER_CORE_EMBLEMS;
    int now_mode = mode;
    // v86 (V86_SPLIT): the main team's fight is not the pair's while it sees nobody (a ping still calls it).
    if (pair_split() && (now_mode == FIGHT || now_mode == RETREAT || now_mode == TRIBUTE) && near_enemies().empty()) now_mode = FARM;
    // v202 (SPREAD): nor a far solo's, unless the fight is within SPREAD_HELP of us (spread_step calls us back then).
    if (spread_out() && (now_mode == FIGHT || now_mode == TRIBUTE) && near_enemies().empty() && !route.empty()
        && distance(position, centers[slot % static_cast<int>(route.size())]) > SPREAD_HELP)
        now_mode = FARM;
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
    // v204 (OUTCLASS): out-emblemed here: leave and stay away (not on the Prism team, nor while carrying the win).
    if (!OUTCLASS_FIRST && OUTCLASS && !prism_team() && !win_tribute && outclass_step(me)) return;
    // v158: the Prism team's cash-in: she walks to her altar and tributes, the others guard it.
    // v180 (PRISM_GUARD): her guards guard in any mode; a fight away from her waits (prism_guard hits what reaches her).
    const bool cash_mode = win_tribute ? now_mode != FIGHT : now_mode == FARM || now_mode == TRIBUTE;
    // v181 (PRISM_CASH_ANY_MODE): a Prism whose cash-in is due goes to her altar in any mode: KILL_TIME counts her bag, so
    // with an enemy in sight the score said FIGHT and she never left (ladder Noobot gemcombat3: a 1,316 winning bag held
    // 400 rounds, then lost). Her guards take the fight.
    const bool cash_due = PRISM_CASH_ANY_MODE && me.champion_name() == Champ::prism && !holding;
    if (prism_team() && (cash_mode || cash_due || (PRISM_GUARD && me.champion_name() != Champ::prism)) && prism_cash_step(me, gems))
        return;
    if (win_tribute && now_mode != FIGHT && !(PRISM_STRICT && holding)) {  // v180: a holding Prism keeps her bag
        if (gems > 0)
            bank_gems(me, {}, "win");
        else
            WHY("win.tribute.wait", "tribute wins the game; no gems to submit");
        return;
    }
    // v169 (PRISM_STAY_BACK): a bare Prism fights from behind her melee allies.
    if (now_mode == FIGHT && prism_stay_back(me)) return;
    // v160 (PRISM_FIGHT_NEAR): no fight away from her, unless we are caught.
    if (now_mode == FIGHT && prism_away(me) && run_to_prism(me)) return;
    // v72: any carrier below CARRY_FRONT_HEALTH also stays behind the front, not only a heavy bag.
    const bool hurt_carrier = FIX_CARRIER && gems >= CARRY_GEMS && me.health() < me.max_health() * CARRY_FRONT_HEALTH;
    // v123 (PRISM_CARRY_FIGHT): Prism fights the same with or without a bag.
    // v158: not on the Prism team, where a kill hands the enemy the game's gems.
    const bool bag_counts = !(PRISM_CARRY_FIGHT && me.champion_name() == Champ::prism && !prism_team());
    // v151-f3 (ALTAR_FARM_BANK): a short tribute beside the altar beats carrying the bag through the fight.
    if (altar_farm && !holding && !channel_cut && (now_mode == FIGHT || now_mode == RETREAT) && gems >= NEAR_BANK_GEMS
        && gems <= ALTAR_QUICK_GEMS && me.health() >= me.max_health() * ALTAR_QUICK_HEALTH) {
        std::vector<vec2> close;
        for (const auto altar : seen::altar_list())
            if (distance(position, altar) <= static_cast<int>(bc::constants::submit_gems_range)) close.push_back(altar);
        if (!close.empty() && bank_gems(me, close, "bank.quick")) return;
    }
    // v160: FRONT_HEALTH is a fixed bar above her bare 700: she would always count as hurt (a 188-round bounce between her
    // altar and the front, adgato-gemcombat4), so the Prism team goes by her share of health alone.
    if (bag_counts && ((gems >= ESCORT_GEMS && me.health() < FRONT_HEALTH && !prism_team()) || hurt_carrier)
        && (now_mode == FIGHT || now_mode == RETREAT)) {
        if (retreat(me)) {
            LOG(branch_name = "front." + branch_name; branch_why = "heavy bag, too hurt for the front; " + branch_why;);
            return;
        }
    } else if (now_mode == FIGHT) {
        if (fight(me)) return;
        NOTE("fight mode, but no enemy to fight: go on");
    }
    // v140 (GHOST): no enemy near, but the one we fought just left our sight: after it, a few rounds.
    if (GHOST && (now_mode == FIGHT || now_mode == FARM) && !hurt_carrier && near_enemies().empty() && !prism_away(me) && chase_ghost(me)) return;
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
    if (FIX_NEAR_BANK && !holding && gems >= NEAR_BANK_GEMS && now_mode != FIGHT && now_mode != RETREAT && !under_threat(me)) {
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
    // v157 (BANK_ON_WAY): an altar a few steps off the walk to the target.
    if (BANK_ON_WAY && !as_v170() && !holding && gems >= ON_WAY_GEMS && now_mode == FARM && !under_threat(me) && bank_on_way(me, "bank.way"))
        return;
    // v159 stage 3 (SAT_BANK_OWN): a satellite banks its own bag at an own-side or middle altar, then farms on.
    if (SAT_BANK_OWN && satellite_on() && pair_split() && gems >= SAT_BANK && pair_capture < 0 && now_mode == FARM
        && !under_threat(me)) {
        const auto& altars = sat_bank_altars(position);
        if (!altars.empty() && bank_gems(me, altars, "sat.bank")) return;
    }

    // v72: enemy champions in sight hunt bags: a carrier cashes in rather than farming on.
    if (FIX_CARRIER && !holding && gems >= hunted_bar() && now_mode != FIGHT && !seen::champions(Team::enemy).empty()
        && bank_gems(me, {}, "bank.hunted"))
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
    if (holding) {
        // v123 (PRISM_HOLD): no bank.
    } else if (!building && escort_bag(gems, owned_of(me).passives) && carrier != mine) {
        if (!solo_altars.empty() && bank_gems(me, solo_altars, "bank.solo")) return;
    } else if (!building && (gems >= (JOBS && spread_on() && job_taker ? job_bag_bar(position) : bank_bar())
                             || (idle && gems >= IDLE_BANK && !(JOBS && spread_on() && job_taker))
                             || (BANK_KEEP && gems >= IDLE_BANK && bank_walk_round >= static_cast<int>(bc::round_number()) - 1))
               && bank_gems(me)) {
        bank_walk_round = static_cast<int>(bc::round_number());
        return;
    }

    // 8. Return to shop at the gold threshold; Ren goes at every emblem's worth, with his bag still carried.
    // v202 (SHOP_INCOME): first the income trip of a quiet farm turn, whatever the bag (a recall keeps it).
    if (income_shop(me, now_mode)) return;
    // v94: Vesper's bar is her build step, then VESPER_RECALL_GOLD batches once her core is done.
    const int recall_gold = ren_rush ? REN_RECALL_GOLD : vesper ? vesper_recall_gold(me) : RECALL_GOLD;
    // v86 (V86_GOLD_RECALL): no bank first (a recall keeps the bag) while the target is safe or nobody is with us.
    const bool v86_shop = V86_GOLD_RECALL && zones_known && !route.empty()
        && (zone(slot % static_cast<int>(route.size())) == ZONE_SAFE || !ally_near(me));
    // v123 (PRISM_HOLD): a holding Prism never tributes, so she shops with her bag (a recall keeps it).
    // v156 (SHOP_NO_TRIBUTE): no tribute to wait for with an empty bag, on a gem-starved map, or with every altar shut.
    // v158: Prism team allies rarely tribute at all, so an empty bag is enough (v171: there only that, as v170).
    const bool ready_to_shop = ren_rush || vesper_building || (tributed && gems == 0) || v86_shop || holding
        || (!as_v170() && ((SHOP_NO_TRIBUTE && (gem_starved || altars_gated())) || (SHOP_EMPTY_BAG && gems == 0)))
        || (prism_team() && gems == 0);
    // v215 (TP_RECALL): the first turn after a tribute, no crystal in hand: recall to restock when that beats the walk.
    if (gems > 0) tp_recall_checked = false;
    if (TP && TP_RECALL && !tp_recall_checked && tributed && gems == 0 && crystal::map_on() && !crystal::carrying
        && now_mode == FARM && !route.empty() && seen::champions(Team::enemy).empty()) {
        tp_recall_checked = true;
        if (crystal::recall_pays(position, spawn_board(slot % static_cast<int>(route.size())), recall_ticks(owned_of(me).passives),
                                 TP_RECALL_MARGIN)
            && recall_to_shop(me)) {
            LOG(branch_why = "tribute in, no crystal: recall to restock beats the walk; " + branch_why;);
            return;
        }
    }
    if (static_cast<int>(me.gold_count()) >= recall_gold && prism_mid_clear(me)) {
        NOTE("gold at the shop bar; finishes the group in front of her first");
    } else if (static_cast<int>(me.gold_count()) >= recall_gold) {
        if (!ready_to_shop) {
            NOTE(gems > 0 ? "gold at the shop bar; the bag is banked first" : "gold at the shop bar; waits for a tribute first");
        } else if (recall_to_shop(me)) {
            LOG(branch_why = "gold at the shop bar; " + branch_why;);
            return;
        } else {
            NOTE("gold at the shop bar, but the recall was refused");
        }
    }

    // v159 (MONO_COVER): a fragile champion keeps behind the tank at an awake monolith.
    if (now_mode == FARM && mono_cover(me)) return;

    // 9-10. Follow the mode.
    if (now_mode == RETREAT && retreat(me)) return;
    // v167 (PRISM_WILLOW_FOLLOW): Willow stays with Prism first.
    if (now_mode == FARM && willow_follow(me)) return;
    // v143 (WILLOW_REGROUP): a Willow no tank came to finds the team before farming again.
    if (now_mode == FARM && regroup_since >= 0 && willow_regroup(me)) return;
    // v202 (SPREAD): a solo farms its own group.
    if (spread_step(me, now_mode)) return;
    // v221 (JOBS): no job and the shared target is respawning: a small bag goes in meanwhile (the old idle rule).
    if (JOBS && spread_on() && job_taker && now_mode == FARM && !holding && !building && idle && gems >= IDLE_BANK && carrier != mine
        && bank_gems(me)) {
        bank_walk_round = static_cast<int>(bc::round_number());
        return;
    }
    if (now_mode != FARM && now_mode != RETREAT) NOTE(std::string(MODE_NAMES[now_mode]) + " mode had nothing to do: farm");
    farm(me);
}

bool Bot::bank_from_spawn(const bc::champion& me) {
    // The off-spawn bank rules (5b Ren's cap, near-bank, hunted, 7), checked in the same order for a champion on
    // spawn, so whatever would send it to an altar off spawn also does on spawn.
    const int gems = static_cast<int>(me.gem_count());
    if (gems <= 0 || prism_holds(me)) return false;
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
    if (BANK_ON_WAY && !as_v170() && gems >= ON_WAY_GEMS && mode == FARM && bank_on_way(me, "spawn.bank.way")) return true;
    if (FIX_CARRIER && gems >= hunted_bar() && mode != FIGHT && !seen::champions(Team::enemy).empty()
        && bank_gems(me, {}, "spawn.bank.hunted"))
        return true;
    if (building) return false;
    const int mine = cached_ident(me);
    if (escort_bag(gems, owned_of(me).passives) && carrier != mine) return !solo_altars.empty() && bank_gems(me, solo_altars, "spawn.bank.solo");
    const bool idle = !route.empty() && wait_at(slot % static_cast<int>(route.size()), position) > 0;
    if (JOBS && spread_on() && job_taker) return gems >= job_bag_bar(position) && bank_gems(me, {}, "spawn.bank");
    return (gems >= bank_bar() || (idle && gems >= IDLE_BANK)) && bank_gems(me, {}, "spawn.bank");
}
