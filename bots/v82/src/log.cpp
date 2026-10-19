// The decision log printed at the end of each turn.
#include "bot.h"
#include "trace.h"

void Bot::branch(const std::string& name, const std::string& why) {
    branch_name = name;
    branch_why = why;
}

void Bot::note(const std::string& text) {
    notes.push_back(text);
}

std::string Bot::tile_flags(vec2 tile) const {
    std::string flags;
    auto add = [&](const char* flag) { flags += (flags.empty() ? "" : ",") + std::string(flag); };
    if (bc::map::spawn_region(Team::enemy).is_set(tile)) add("enemy-spawn");
    if (bc::map::spawn_region(Team::ally).is_set(tile)) add("ally-spawn");
    if (spawn_zone.is_set(tile)) add("chase-ring");
    return flags.empty() ? "open" : flags;
}

std::string Bot::slot_text() const {
    if (route.empty()) return "s?";
    const int s = slot % static_cast<int>(route.size());
    const auto center = centers[s];
    return format("s%d", s) + at(center) + " " + tile_flags(center);
}

std::string Bot::shared_tags() const {
    std::string tags = aggressive ? "aggr" : "calm";
    tags += carrier ? format(" car %d", carrier - 1) : " car -";
    if (capture_slot != NO_CAPTURE) tags += format(" cap s%d->c%d", capture_slot, collector - 1);
    if (rejoining) tags += " rejoining";
    return tags;
}

void Bot::begin_trace() {
    trace::reset();
    shared_why.clear();
    local_why.clear();
    branch_name.clear();
    branch_why.clear();
    notes.clear();
    score_why.clear();
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
    if (bc::map::spawn_region(Team::ally).is_set(pos)) text += " on-ally-spawn";
    if (ready && spawn_zone.is_set(pos)) text += " IN-CHASE-RING";
    if (spawn_gap) {
        text += format(" gap %d", *spawn_gap);
        if (*spawn_gap <= 2) text += " SPAWNS-CLOSE";
    }
    if (ally_in_ring) text += format(" ally-in-ring %d/%zu", ally_in_ring, ally_spawn_list.size());
    return text;
}

std::vector<std::string> Bot::outcome_chunks(const bc::champion& me) const {
    const auto& acts = trace::acts;
    bool moved = false;
    for (const auto& kind : acts)
        if (kind == "move" || kind == "dash") moved = true;
    std::vector<std::string> chunks;
    if (!acts.empty()) {
        std::string joined;
        for (std::size_t i = 0; i < acts.size(); ++i) joined += (i ? "," : "") + acts[i];
        chunks.push_back("ACT " + joined);
    }
    if (!moved) {
        const auto ends_with = [](const std::string& s, const std::string& tail) {
            return s.size() >= tail.size() && s.compare(s.size() - tail.size(), tail.size(), tail) == 0;
        };
        const bool holding = ends_with(branch_name, ".hold") || branch_name.rfind("channel.", 0) == 0
            || branch_name == "spawn.heal";
        std::string reason;
        if (!trace::stall.empty())
            reason = trace::stall;
        else if (!trace::attack.empty())
            reason = trace::attack;
        else if (holding)
            reason = "chose to wait";
        else if (!acts.empty())
            reason = "no step";
        else
            reason = "no action attempted";
        chunks.push_back("IDLE " + reason);
    }
    if (me.is_alive() && (!moved || in_spawn_probe(me))) {
        const auto pos = me.position();
        chunks.push_back(step_room(pos));
        auto nearest = [&](const std::vector<vec2>& tiles) {
            std::optional<int> best;
            for (const auto t : tiles)
                if (!best || distance(pos, t) < *best) best = distance(pos, t);
            return best ? std::to_string(*best) : std::string("None");
        };
        chunks.push_back("dA" + nearest(ally_spawn_list) + " dE" + nearest(enemy_spawn_list));
    }
    if (chunks.empty()) chunks.push_back("IDLE no action attempted");
    return chunks;
}

void Bot::log_turn(const bc::champion& me) {
    const auto prefix = format("r%u ", bc::round_number());
    std::vector<std::string> lines{
        where_text(me),
        shared_why.empty() ? "shared ?" : shared_why,
        (branch_name.empty() ? "unset" : branch_name) + ": " + (branch_why.empty() ? "no branch recorded" : branch_why),
    };
    for (auto& chunk : outcome_chunks(me)) lines.push_back(std::move(chunk));
    if (!local_why.empty()) lines.push_back(local_why);
    for (const auto& text : notes) lines.push_back("note: " + text);
    for (const auto& line : lines) say(prefix, line);
}
