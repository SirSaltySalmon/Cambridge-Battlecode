// v216 (CONTRACT): one engagement decision per contact (fight_coord_plan.md F2; constants.h has the layout).
// v218: the help ping beside it (HELP), CONTRACT_HOLD, CONTRACT_SKIP_RESCORE.
#include "bot.h"
#include "seen.h"
#include "state.h"

void Bot::contract_turn(const bc::champion& me) {
    contract_role = CR_NONE;
    contract_state = C_NONE;
    contract_anchor = -1;
    if (!contract_bits || route.empty()) return;
    const int now = static_cast<int>(bc::round_number());

    // The contract as the champions before us left it: fresh while its age is at most CONTRACT_FRESH, settled this
    // round when written this round (update_ping clears byte 7 past PING_FRESH, so age 0 is never 4 rounds old).
    // v218: byte 7 as update_ping left it this turn (no second read), and the state only under a fresh anchor.
    int anchor = -1, age = 4, state = C_NONE;
    if (CONTRACT_MODE == 2) {
        const int value = ping_byte_round == now ? ping_byte_seen : shared_byte(PING_BYTE);
        anchor = (value & 63) - 1;
        age = (now - (value >> 6)) & 3;
        if (anchor < 0 || anchor >= static_cast<int>(route.size()) || age > CONTRACT_FRESH) anchor = -1;
        if (anchor >= 0) state = read_contract_state();
        // v218 (CONTRACT_HOLD): every champion, every turn, notes when the state it reads changed.
        see_contract(state, now);
    }
    if (!me.is_alive() || channelling(me) || me.is_teleporting()) return;
    const auto& near = near_enemies();
    if (near.empty()) return;
    // v218 merge: a champion under the recall bar leaves on its own (hurt recall / dodge run after this) and plays as
    // v213; it neither settles the contact for the team nor follows it. The next champion with contact settles.
    if (me.health() < me.max_health() * RECALL_HEALTH) {
        NOTE("contract: under the recall bar, no part in it");
        return;
    }
    const auto position = seen::self_position();
    // v218 (HELP): a fresh help ping owns byte 7's anchor: settlers keep it, and a far contact plays alone.
    const int k = help_bits && anchor >= 0 && ping_byte_round == now ? help_k_seen : 0;
    const bool help_up = k > 0;
    const bool same = anchor >= 0 && distance(position, centers[anchor]) <= CONTRACT_ARRIVE;
    const bool settled = anchor >= 0 && age == 0;
    if ((settled || help_up) && !same) {
        contract_role = CR_OWN;
        NOTE(format("contract: own contact, %d from the anchor s%d (%s%s): play it alone", distance(position, centers[anchor]),
                    anchor, CONTRACT_NAMES[state], help_up && !settled ? ", help ping" : ""));
        return;
    }
    if (settled) {
        contract_role = CR_FOLLOW;
        contract_state = state;
        contract_anchor = anchor;
        // One veto, toward safety only: far under our own stay bar.
        if (CONTRACT_VETO > 0 && state == C_ENGAGE) {
            score_mode(position, FIGHT);
            const double stay = last_base_bar - FIGHT_HYSTERESIS;
            if (last_score < stay - CONTRACT_VETO) {
                write_contract_state(C_DISENGAGE);
                contract_state = C_DISENGAGE;
                see_contract(C_DISENGAGE, now);
                NOTE(format("contract: engage -> disengage, veto at %.1f (stay bar %.0f): ", last_score, stay) + score_why);
            }
        }
        return;
    }

    // We settle this contact. The anchor: the help ping's while it is fresh, else the route slot nearest us.
    int mine = help_up ? anchor : 0;
    if (!help_up) {
        int gap = distance(position, centers[0]);
        for (int s = 1; s < static_cast<int>(centers.size()); ++s) {
            const int d = distance(position, centers[s]);
            if (d < gap) gap = d, mine = s;
        }
    }
    if (mine >= 63) return;
    // Hysteresis comes from the contract's state, not our own last decision.
    const int prev = same ? state : C_NONE;
    int next = C_NONE;
    std::string why;
    // v218 (CONTRACT_SKIP_RESCORE): the shared mode stands where v213 would not rescore and the state agrees.
    if (CONTRACT_SKIP_RESCORE && mode == FIGHT && prev != C_DISENGAGE) {
        next = C_ENGAGE;
        LOG(why = "the team's mode is fight";);
    } else if (CONTRACT_SKIP_RESCORE && mode == RETREAT && prev != C_ENGAGE) {
        next = C_DISENGAGE;
        LOG(why = "the team's mode is retreat";);
    } else {
        const int choice = score_mode(position, prev == C_ENGAGE ? FIGHT : FARM);
        bool engage = choice == FIGHT;
        // v218 merge: re-entry also needs score_mode's own FIGHT, so FIGHT_SIM 4's veto holds here too.
        if (prev == C_DISENGAGE) engage = engage && last_score >= last_base_bar + CONTRACT_REENTER;
        next = engage ? C_ENGAGE : C_DISENGAGE;
        LOG(why = score_why;);
    }
    // v218 (CONTRACT_HOLD): a flip waits until the state has held CONTRACT_HOLD rounds (as we saw it).
    const bool holding = (prev == C_ENGAGE || prev == C_DISENGAGE) && now - contract_seen_since < CONTRACT_HOLD;
    if (holding && (next == C_ENGAGE || next == C_DISENGAGE) && next != prev) {
        LOG(why = format("held %d of %d rounds; ", now - contract_seen_since, CONTRACT_HOLD) + CONTRACT_NAMES[next] + " wanted: " + why;);
        NOTE(format("contract: keeps %s at s%d: ", CONTRACT_NAMES[prev], mine) + why);
        next = prev;
    }
    // OUTCLASS as an input: out-emblemed here, the whole contact leaves. v218: not read while a hold would keep ENGAGE
    // anyway (outclass_gap is shared with this turn's outclass_step).
    if (next == C_ENGAGE && CONTRACT_OUTCLASS && OUTCLASS && !prism_team() && !win_tribute
        && !(CONTRACT_HOLD_OUTCLASS && holding && prev == C_ENGAGE)) {
        bool close = false, finish = false;
        const int gap = outclass_gap(me, close, finish);
        if (close && !finish && gap <= OUTCLASS_GAP) {
            next = C_DISENGAGE;
            LOG(why = format("out-emblemed by %d; ", -gap) + why;);
        }
    }
    // A chase the team stopped gaining on ends the contract (TEAM_CHASE, from our last update).
    if (next == C_ENGAGE && team_chase_off()) {
        next = C_NONE;
        LOG(why = "chase dropped; " + why;);
    }
    write_contract_state(next);
    if (CONTRACT_MODE == 2) {
        set_shared_byte(PING_BYTE, (mine + 1) | ((now & 3) << 6));
        ping_byte_seen = (mine + 1) | ((now & 3) << 6);
        // v218 (HELP): the ping was sent age + k - 1 rounds ago; from this write (B = now) that is K = age + k.
        if (help_up) {
            write_help(age + k);
            help_k_seen = age + k;
        }
        see_contract(next, now);
    }
    if (CONTRACT_MODE != 2) {
        NOTE(format("contract (not acted on): %s at s%d: ", CONTRACT_NAMES[next], mine) + why);
        return;
    }
    contract_role = CR_SETTLER;
    contract_state = next;
    contract_anchor = mine;
    if (next != prev)
        NOTE(format("contract: %s -> %s at s%d: ", CONTRACT_NAMES[prev], CONTRACT_NAMES[next], mine) + why);
}

void Bot::publish_help(const bc::champion& me) {
    // v218 (HELP): update_ping chose to ping (v213's rule); byte 7 takes our slot with B = now and K = 1. A contract
    // settled this round on our contact keeps its state; any other (stale, unsettled or far) reads NONE this round, so
    // later contact holders score for themselves as in v213 rather than follow a state nobody settled.
    const int best = help_publish;
    help_publish = -1;
    if (best < 0 || !help_bits || route.empty()) return;
    const int now = static_cast<int>(bc::round_number());
    const int value = ping_byte_round == now ? ping_byte_seen : shared_byte(PING_BYTE);
    const int anchor = (value & 63) - 1;
    const bool settled = anchor >= 0 && anchor < static_cast<int>(route.size()) && ((now - (value >> 6)) & 3) == 0
                         && distance(seen::self_position(), centers[anchor]) <= CONTRACT_ARRIVE;
    if (!settled) {
        write_contract_state(C_NONE);
        see_contract(C_NONE, now);
    }
    const int byte = (best + 1) | ((now & 3) << 6);
    set_shared_byte(PING_BYTE, byte);
    ping_byte_seen = byte;
    ping_byte_round = now;
    write_help(1);
    help_k_seen = 1;
    (void)me;
}
