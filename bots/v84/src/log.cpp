// The decision log printed at the end of each turn.
#include "bot.h"
#include "trace.h"
#include <cstdio>

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

void Bot::log_compact(const bc::champion& me) {
    // v83 (LOG_RELEASE): one short line per turn for platform (release) builds, where the full log is skipped and stdout
    // is capped at ~1 MB. Built in a stack buffer with no string allocations and written with one fwrite:
    //   r<round> c<id> <x>,<y> h<health> g<gems> <mode><slot>[*=led] <branch> [<acts>] <start of the reason, debug only>
    char line[LOG_LINE];
    std::size_t n = 0;
    auto put = [&](const char* text, std::size_t len) {
        if (len > sizeof line - 1 - n) len = sizeof line - 1 - n;
        for (std::size_t i = 0; i < len; ++i) line[n++] = text[i];
    };
    auto chr = [&](char c) {
        if (n < sizeof line - 1) line[n++] = c;
    };
    auto num = [&](long long v) {
        char digits[24];
        int d = 0;
        const bool neg = v < 0;
        unsigned long long u = neg ? static_cast<unsigned long long>(-v) : static_cast<unsigned long long>(v);
        do {
            digits[d++] = static_cast<char>('0' + u % 10);
            u /= 10;
        } while (u && d < 23);
        if (neg) chr('-');
        while (d) chr(digits[--d]);
    };
    chr('r');
    num(bc::round_number());
    put(" c", 2);
    num(champ_id(me.champion_name()));
    chr(' ');
    if (!me.is_alive()) {
        put("dead", 4);
    } else {
        const auto pos = me.position();
        num(pos.x);
        chr(',');
        num(pos.y);
        put(" h", 2);
        num(static_cast<long long>(me.health()));
        put(" g", 2);
        num(me.gem_count());
    }
    chr(' ');
    chr("FfrT"[mode & 3]);  // farm, fight, retreat, tribute
    num(slot);
    if (led) chr('*');
    chr(' ');
    put(branch_name.data(), branch_name.size());
    if (!trace::acts.empty()) {
        put(" [", 2);
        for (std::size_t i = 0; i < trace::acts.size(); ++i) {
            if (i) chr(',');
            put(trace::acts[i].data(), trace::acts[i].size());
        }
        chr(']');
    }
    if (!branch_why.empty()) {
        chr(' ');
        put(branch_why.data(), branch_why.size() < LOG_WHY ? branch_why.size() : LOG_WHY);
    }
    line[n++] = '\n';
    std::fwrite(line, 1, n, stdout);
}
