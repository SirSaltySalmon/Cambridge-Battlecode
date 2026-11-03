// v216 (CONTRACT): one engagement decision per contact (fight_coord_plan.md F2; constants.h has the layout).
#include "bot.h"
#include "seen.h"
#include "state.h"

void Bot::contract_turn(const bc::champion& me) {
    contract_role = CR_NONE;
    contract_state = C_NONE;
    contract_anchor = -1;
    if (!contract_bits || route.empty()) return;
    if (!me.is_alive() || channelling(me) || me.is_teleporting()) return;
    const auto& near = near_enemies();
    if (near.empty()) return;
    const int now = static_cast<int>(bc::round_number());
    const auto position = seen::self_position();
    // The route slot nearest us: our contact's anchor.
    int mine = 0;
    for (int s = 1; s < static_cast<int>(centers.size()); ++s)
        if (distance(position, centers[s]) < distance(position, centers[mine])) mine = s;
    if (mine >= 63) return;

    // The contract as the champions before us left it: fresh while its age is at most CONTRACT_FRESH, settled this
    // round when written this round (update_ping clears byte 7 past PING_FRESH, so age 0 is never 4 rounds old).
    int anchor = -1, age = 4, state = C_NONE;
    if (CONTRACT_MODE == 2) {
        const int value = shared_byte(PING_BYTE);
        anchor = (value & 63) - 1;
        age = (now - (value >> 6)) & 3;
        if (anchor < 0 || anchor >= static_cast<int>(route.size()) || age > CONTRACT_FRESH) anchor = -1;
        if (anchor >= 0) state = read_contract_state();
    }
    const bool same = anchor >= 0 && distance(position, centers[anchor]) <= CONTRACT_ARRIVE;
    if (same && state == C_GATHER) {
        if (gather_seen < now - 1) gather_since = now;
        gather_seen = now;
    }
    const bool settled = anchor >= 0 && age == 0;
    if (settled && !same) {
        contract_role = CR_OWN;
        NOTE(format("contract: own contact, %d from the anchor s%d (%s): play it alone", distance(position, centers[anchor]),
                    anchor, CONTRACT_NAMES[state]));
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
                NOTE(format("contract: engage -> disengage, veto at %.1f (stay bar %.0f): ", last_score, stay) + score_why);
            }
        }
        return;
    }

    // We settle this contact. Hysteresis comes from the contract's state, not our own last decision.
    const int prev = same ? state : C_NONE;
    const int choice = score_mode(position, prev == C_ENGAGE ? FIGHT : FARM);
    bool engage = choice == FIGHT;
    if (prev == C_DISENGAGE) engage = last_score >= last_base_bar + CONTRACT_REENTER;
    int next = engage ? C_ENGAGE : C_DISENGAGE;
    std::string why;
    LOG(why = score_why;);
    // GATHER: short of bodies, and living allies outside this fight would make up the score.
    if (!engage && CONTRACT_GATHER && last_body_gap > 0 && (prev != C_GATHER || now - gather_since < CONTRACT_GATHER_ROUNDS)) {
        int spare = 0;
        for (const auto& a : seen::roster(Team::ally))
            if (a != me && a.is_alive()) ++spare;
        spare -= static_cast<int>(fight_allies.size()) - 1;
        const double fill = std::min({static_cast<double>(std::max(spare, 0)), last_body_gap,
                                      static_cast<double>(CONTRACT_GATHER_BODIES)});
        const double bar = prev == C_DISENGAGE ? last_base_bar + CONTRACT_REENTER : last_base_bar;
        if (fill > 0 && last_score + MISSING_BODY_PENALTY * fill >= bar) {
            next = C_GATHER;
            LOG(why = format("%.1f allies away would make it; ", fill) + why;);
        }
    }
    // OUTCLASS as an input: out-emblemed here, the whole contact leaves.
    if (next == C_ENGAGE && CONTRACT_OUTCLASS && OUTCLASS && !prism_team() && !win_tribute) {
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
    if (CONTRACT_MODE == 2) set_shared_byte(PING_BYTE, (mine + 1) | ((now & 3) << 6));
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

bool Bot::contract_hold(const bc::champion& me) {
    // GATHER for a contact holder: out of enemy reach, close on the nearest ally, and wait for the rest. Anyone an
    // enemy can already hit fights on (holding there only takes hits), as in join_first.
    const auto position = seen::self_position();
    const auto zone = threat_zone(seen::champions(Team::enemy), false);
    if (zone.is_set(position)) return false;
    std::optional<vec2> nearest;
    int near_d = 1 << 20;
    const auto& allies = seen::champions(Team::ally);
    const auto& tiles = seen::positions(Team::ally);
    for (std::size_t i = 0; i < allies.size() && i < tiles.size(); ++i) {
        if (allies[i] == me || channelling(allies[i])) continue;
        const int d = distance(position, tiles[i]);
        if (d < near_d) near_d = d, nearest = tiles[i];
    }
    if (nearest && near_d > 1) {
        WHY("contract.gather", format("gather: out of reach, to the nearest ally %d away", near_d));
        if (!move_towards_closest(tiles_around(*nearest, 1), &zone)) NOTE("gather: no step toward the ally");
    } else {
        WHY("contract.gather", "gather: out of reach, hold for the allies coming");
    }
    return true;
}
