"""Profile copy: inclusive fuel and calls per function per turn, one 'PROF' line a turn on stdout.

usage: python profpatch.py BOT_DIR   (a throwaway copy)
"""
import glob
import os
import re
import sys

SKIP = {"format", "say", "at", "gap", "allowed", "kind_rank", "tile_flags", "cached_ident", "slot_text", "where_text", "note", "branch",
        "log_turn", "begin_trace", "play_turn", "Travel&"}
FREE = ["go", "hop_route", "few_hop_holes", "dash_along", "dash_ranked", "blocked_tiles_into", "thin_holes",
        "hop_steps", "landings_by_field", "aoe_aim", "cast_aoe", "spell_places", "threat_zone", "recall_zone",
        "channel_threat", "ability_of", "move_speed", "close_in", "exit_turn_steps", "goal_cover", "camps", "champions",
        "roster", "visible_camps_into", "visible_living_into", "tiles_around", "sighted", "within", "alive",
        "attack_if_in_range", "cast_with", "cast", "cast_self", "observe_vision", "is_mage", "format", "say", "at",
        "spawn_zone_of", "home_island", "around_into", "copy_into", "not_into", "and_into"]

d = sys.argv[1]
src = os.path.join(d, "src")
names = set(FREE)
for path in glob.glob(os.path.join(src, "*.cpp")):
    for m in re.finditer(r"^[A-Za-z][^;\n(=]*\bBot::([a-z_]+)\(", open(path, encoding="utf-8").read(), re.M):
        names.add(m.group(1))
names = sorted(n for n in names if n not in SKIP)
idx = {n: i for i, n in enumerate(names)}
done = set()
for path in glob.glob(os.path.join(src, "*.cpp")):
    s = open(path, encoding="utf-8").read()
    orig = s
    pat = re.compile(r"^([A-Za-z][^;\n(=]*?\b(?:Bot::|seen::|fast::)?)([a-z_]+)\(", re.M)
    out, pos = [], 0
    for m in pat.finditer(s):
        name = m.group(2)
        if name not in idx:
            continue
        head = m.group(1)
        if head.strip().startswith(("return", "else", "if", "for", "while", "#", "using", "static_assert")):
            continue
        # The definition's body brace: the first '{' after the matching ')' with no ';' in between.
        depth, i = 0, m.end() - 1
        while i < len(s):
            if s[i] == "(":
                depth += 1
            elif s[i] == ")":
                depth -= 1
                if depth == 0:
                    break
            i += 1
        rest = s[i + 1:i + 200]
        j = rest.find("{")
        k = rest.find(";")
        if j < 0 or (0 <= k < j) or rest[:j].strip() not in ("", "const", "const noexcept", "noexcept"):
            continue
        brace = i + 1 + j
        out.append(s[pos:brace + 1])
        out.append(" prof::Scope prof_scope(%d, \"%s\");" % (idx[name], name))
        pos = brace + 1
        done.add(name)
    out.append(s[pos:])
    s = "".join(out)
    if s != orig:
        s = '#include "prof.h"\n' + s
        open(path, "w", encoding="utf-8").write(s)

open(os.path.join(src, "prof.h"), "w").write("""// Profiling only: inclusive fuel per function per turn.
#pragma once
#include <cambc.h>
#include <cstdint>
namespace prof {
inline constexpr int N = %d;
inline std::uint64_t acc[N];
inline std::uint32_t calls[N], depth[N];
inline const char* names[N];
inline std::uint64_t own[N];
struct Scope;
inline Scope* top = nullptr;
inline void reset() {
    for (int i = 0; i < N; ++i) acc[i] = calls[i] = own[i] = 0;
    top = nullptr;
}
struct Scope {
    int i;
    std::uint64_t f, child = 0;
    bool outer;
    Scope* up;
    Scope(int i, const char* name) : i(i), f(cambc::fuel_remaining()), outer(depth[i]++ == 0), up(top) {
        names[i] = name;
        ++calls[i];
        top = this;
    }
    ~Scope() {
        --depth[i];
        const auto now = cambc::fuel_remaining();
        const auto spent = f >= now ? f - now : 0;
        if (outer) acc[i] += spent;
        own[i] += spent >= child ? spent - child : 0;
        top = up;
        if (up) up->child += spent;
    }
};
}  // namespace prof
""" % max(1, len(names)))

turn = os.path.join(src, "turn.cpp")
s = open(turn, encoding="utf-8").read()
old = """void Bot::play_turn() {
    auto& me = bc::self();"""
assert old in s
s = s.replace(old, """void Bot::play_turn() {
    prof::reset();
    struct ProfOut {
        std::uint64_t fuel;
        std::uint32_t round;
        int who;
        ~ProfOut() {
            if (bc::round_number() != round) {
                say(format("PROF r%u c%d EXHAUSTED", round, who));
                return;
            }
            std::string line = format("PROF r%u c%d total %llu", round, who, static_cast<unsigned long long>(fuel - bc::fuel_remaining()));
            for (int i = 0; i < prof::N; ++i)
                if (prof::calls[i]) line += format(" %s=%llu/%llu/%u", prof::names[i], static_cast<unsigned long long>(prof::acc[i]), static_cast<unsigned long long>(prof::own[i]), prof::calls[i]);
            say(line);
        }
    } prof_out{bc::fuel_remaining(), static_cast<std::uint32_t>(bc::round_number()), static_cast<int>(bc::self().champion_name())};
    auto& me = bc::self();""")
open(turn, "w", encoding="utf-8").write(s)
print("scoped %d of %d" % (len(done), len(names)))
print("missing:", sorted(set(names) - done))
