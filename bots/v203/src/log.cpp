// The decision log printed at the end of each turn (v90-log: built only inside debug_block, see LOG in bot.h).
//
// One line per champion turn, then indented lines only when they add a reason:
//   r300 c3 (21,14) fight.hit: closing on (5,15) | move,attack
//     lead fight s6 (13,26): 15.5 >= -5 (nums +10 5v3, hp +5, power +5, emblems +6, dist -2.0) | centre, zone 1
//     own score: retreat ...           (a follower whose own sight disagrees with the shared mode)
//     - side decision (NOTE)
// Wording rules: say why, briefly; no health, gold, item or champion stats; tiles and slots are fine.
#include "bot.h"
#include "trace.h"
#include "seen.h"

void Bot::branch(const std::string& name, const std::string& why) {
    branch_name = name;
    branch_why = why;
}

void Bot::note(const std::string& text) {
    notes.push_back(text);
}

std::string Bot::tile_flags(vec2 tile) const {
    std::string flags;
    if (bc::map::spawn_region(Team::enemy).is_set(tile)) flags += " enemy-spawn";
    if (bc::map::spawn_region(Team::ally).is_set(tile)) flags += " ally-spawn";
    if (spawn_zone.is_set(tile)) flags += " chase-ring";
    return flags;
}

std::string Bot::slot_text() const {
    if (route.empty()) return "s?";
    const int s = slot % static_cast<int>(route.size());
    return format("s%d ", s) + at(centers[s]) + tile_flags(centers[s]);
}

std::string Bot::shared_tags() const {
    // Only what is on: an aggressive route, an escort, an open claim, the v86 phase and the target's zone.
    std::string tags;
    auto add = [&](const std::string& tag) { tags += (tags.empty() ? "" : ", ") + tag; };
    if (aggressive) add("aggressive");
    if (carrier) add(format("escort c%d", carrier - 1));
    if (capture_slot != NO_CAPTURE) add(format("claim s%d by c%d", capture_slot, collector - 1));
    if (zones_known) {
        std::string ph = PHASE_NAMES[phase & 3];
        if (phase == PHASE_SPLIT && target2 != NO_TARGET2) ph += format(" (pair s%d)", target2);
        add(ph);
        if (!route.empty()) add(format("zone %d", zone(slot % static_cast<int>(route.size()))));
    }
    return tags;
}

void Bot::begin_trace() {
    trace::reset();
    LOG(shared_why.clear(); local_why.clear(); branch_name.clear(); branch_why.clear(); notes.clear(); score_why.clear(););
}

bool Bot::in_spawn_probe(const bc::champion& me) const {
    if (!me.is_alive() || !ready) return false;
    const auto pos = me.position();
    return bc::map::spawn_region(Team::enemy).is_set(pos) || spawn_zone.is_set(pos);
}

std::string Bot::where_text(const bc::champion& me) const {
    // The champion first, so a reader can follow one champion through the interleaved turns.
    const std::string who = format("c%d ", champ_id(me.champion_name()));
    if (!me.is_alive()) return who + "dead";
    const auto pos = me.position();
    std::string text = who + at(pos);
    if (bc::map::spawn_region(Team::enemy).is_set(pos)) text += " ON-ENEMY-SPAWN";
    if (ready && spawn_zone.is_set(pos)) text += " IN-CHASE-RING";
    return text;
}

std::vector<std::string> Bot::outcome_chunks(const bc::champion&) const {
    // What the turn did, or why it did nothing: the acts, plus why a step failed when one was tried.
    const auto& acts = trace::acts;
    std::string joined;
    bool moved = false;
    for (const auto& kind : acts) {
        joined += (joined.empty() ? "" : ",") + kind;
        if (kind == "move" || kind == "dash") moved = true;
    }
    if (!joined.empty()) {
        if (!moved && !trace::stall.empty()) joined += " (no step: " + trace::stall + ")";
        return {joined};
    }
    if (!trace::stall.empty()) return {"idle: " + trace::stall};
    if (!trace::attack.empty()) return {"idle: " + trace::attack};
    return {"idle"};
}

void Bot::log_turn(const bc::champion& me) {
    const auto prefix = format("r%u ", bc::round_number());
    std::string line = where_text(me) + " " + (branch_name.empty() ? "unset" : branch_name) + ": "
        + (branch_why.empty() ? "no reason recorded" : branch_why);
    if (me.is_alive())
        for (const auto& chunk : outcome_chunks(me)) line += " | " + chunk;
    say(prefix, line);
    // v135a audit: health, target and claim, every awake camp whose reach holds us (kind, tile, who is nearest: us, an
    // ally c#, an enemy E; = marks a tie), and the sturdy allies (max health 1000+, over the recall bar) inside reach.
    if (me.is_alive()) {
        const auto pos = me.position();
        std::string a = format("@s hp %d/%d mode %d slot %d cap %d/c%d", static_cast<int>(me.health()), static_cast<int>(me.max_health()),
                               mode, route.empty() ? -1 : slot % static_cast<int>(route.size()), capture_slot, collector - 1);
        if (job_slot >= 0) a += format(" job s%d", job_slot);  // v202 (BUSY_JOBS)
        std::string camps, tanks;
        for (const auto& c : seen::camps()) {
            const int reach = camp_kind_reach(c.kind());
            if (reach <= 0) continue;
            const auto spot = c.spawn_position();
            const int gap = distance(pos, spot);
            if (gap > reach) continue;
            std::uint32_t s = 0;
            if (bc::detail::sdk_camp_state(static_cast<std::uint32_t>(static_cast<bc::handle_camp_t>(c)), &s) != 0 || s != 1) continue;
            int best = gap;
            std::string who = "me", ties;
            bool tie = false;
            for (const auto& x : seen::champions(Team::ally)) {
                if (x == me) continue;
                const int d = distance(x.position(), spot);
                if (d > reach) continue;
                if (x.max_health() >= 1000 && x.health() >= x.max_health() * RECALL_HEALTH
                    && tanks.find(format("c%d", cached_ident(x) - 1)) == std::string::npos)
                    tanks += format(" c%d", cached_ident(x) - 1);
                if (d < best) best = d, who = format("c%d", cached_ident(x) - 1), tie = false, ties.clear();
                else if (d == best) tie = true, ties += format("c%d", cached_ident(x) - 1);
            }
            for (const auto& x : seen::champions(Team::enemy)) {
                const int d = distance(x.position(), spot);
                if (d <= reach && d < best) best = d, who = "E", tie = false, ties.clear();
            }
            const char* k = c.kind() == bc::camp_type::wolf ? "w" : c.kind() == bc::camp_type::elk ? "e" : c.kind() == bc::camp_type::golem ? "g" : "M";
            camps += format(" %s(%d,%d)>%s%s", k, spot.x, spot.y, who.c_str(), tie ? ("=" + ties).c_str() : "");
        }
        if (!camps.empty()) a += " awake" + camps;
        if (!tanks.empty()) a += " tanks" + tanks;
        say("  ", a);
    }
    for (const auto& text : trace::audit) say("  ", text);
    if (!shared_why.empty()) say("  ", shared_why);
    if (!local_why.empty()) say("  ", local_why);
    for (const auto& text : notes) say("  - ", text);
}
