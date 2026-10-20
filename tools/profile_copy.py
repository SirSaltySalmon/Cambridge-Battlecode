"""Make a fuel-profiling copy of a bot: python tools/profile_copy.py SRC_BOT DEST_BOT

Every function defined at column 0 in DEST/src/*.cpp gets PROF_SCOPE("name") as its first line (inclusive fuel and
calls, from bc::fuel_remaining()). At the end of each turn the copy prints, inside debug_block (free in debug builds),
one line `PROF r<round> c<champ> <total> name=fuel/calls ...` for the functions that ran. Run it with unlimited fuel
(tools/bench.py --unlimited) and read it with tools/profile_read.py. The copy is a throwaway: delete it afterwards.
"""
import os
import re
import shutil
import sys

src, dest = sys.argv[1], sys.argv[2]
root = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "bots")
if os.path.exists(os.path.join(root, dest)):
    shutil.rmtree(os.path.join(root, dest))
shutil.copytree(os.path.join(root, src), os.path.join(root, dest), ignore=shutil.ignore_patterns(".cambc"))
d = os.path.join(root, dest, "src")
open(os.path.join(d, "prof.h"), "w").write('''// Throwaway fuel profiler (tools/profile_copy.py).
#pragma once
#include <cambc.h>
#include <cstdint>
#include <cstdio>
namespace prof {
struct Entry { const char* name; std::uint64_t fuel; std::uint32_t calls; };
inline Entry entries[512];
inline int count = 0;
inline Entry* reg(const char* n) { entries[count] = {n, 0, 0}; return &entries[count++ % 512]; }
struct Scope {
    Entry* e; std::uint64_t start;
    explicit Scope(Entry* entry) : e(entry), start(cambc::fuel_remaining()) { ++e->calls; }
    ~Scope() { e->fuel += start - cambc::fuel_remaining(); }
};
inline void dump(unsigned round, int champ, std::uint64_t total) {
    std::printf("PROF r%u c%d %llu", round, champ, static_cast<unsigned long long>(total));
    for (int i = 0; i < count; ++i)
        if (entries[i].calls) {
            std::printf(" %s=%llu/%u", entries[i].name, static_cast<unsigned long long>(entries[i].fuel), entries[i].calls);
            entries[i].fuel = 0;
            entries[i].calls = 0;
        }
    std::printf("\\n");
}
}  // namespace prof
#define PROF_SCOPE(n) static prof::Entry* _prof_entry = prof::reg(n); prof::Scope _prof_scope(_prof_entry)
''')
head = re.compile(r"^[A-Za-z][\w:<>,\s\*&]*?\b((?:Bot::)?~?\w+)\(")
for name in os.listdir(d):
    if not name.endswith(".cpp"):
        continue
    path = os.path.join(d, name)
    lines = open(path).read().split("\n")
    out = ['#include "prof.h"']
    i = 0
    while i < len(lines):
        line = lines[i]
        m = head.match(line)
        if m and not line.rstrip().endswith(";") and not line.startswith(("namespace", "struct", "class", "using", "#", "inline constexpr", "constexpr", "static const", "const ")):
            j = i
            while j < len(lines) and not lines[j].rstrip().endswith("{") and not lines[j].rstrip().endswith(";"):
                j += 1
            if j < len(lines) and lines[j].rstrip().endswith("{") and "=" not in lines[i].split("(")[0]:
                out.extend(lines[i:j + 1])
                fn = m.group(1).replace("Bot::", "")
                out.append('    PROF_SCOPE("%s");' % fn)
                i = j + 1
                continue
        out.append(line)
        i += 1
    open(path, "w").write("\n".join(out))
# Dump each turn: total turn fuel from the top of play_turn.
p = os.path.join(d, "turn.cpp")
s = open(p).read()
s = s.replace('''void Bot::play_turn() {
    PROF_SCOPE("play_turn");''', '''void Bot::play_turn() {
    const auto prof_start = bc::fuel_remaining();''')
s = s.replace('''    if (failure) std::rethrow_exception(failure);
}''', '''    debug_block({ prof::dump(bc::round_number(), champ_id(me.champion_name()), prof_start - bc::fuel_remaining()); });
    if (failure) std::rethrow_exception(failure);
}''', 1)
open(p, "w").write(s)
print("profiling copy at bots/" + dest)
