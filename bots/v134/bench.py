"""v133 bench helpers (run from the repo root).

  python bots/v133t/bench.py seed BOT N        copy bots/BOT to bots/BOT-sN with N*300 fuel burnt at turn end
  python bots/v133t/bench.py variant BASE NAME FLAG=VALUE...   copy bots/BASE to bots/NAME with those constants.h values
  python bots/v133t/bench.py manifest OUT OLD BOT[,BOT...]   rows for every maps/0*.map26a, both sides, into
                                                replays/OUT/<bot>/<map>-a|b (team A of -a is the bot under test)
  python bots/v133t/bench.py summary DIR[,DIR...] [DIR[,DIR...] ...]   one line per comma group: record, banked,
                                                kills / deaths, camp deaths, out-of-fuel turns (ours vs theirs)
"""
import glob
import json
import os
import re
import shutil
import sys


def seed(bot, n):
    src, dst = os.path.join("bots", bot), os.path.join("bots", "%s-s%d" % (bot, n))
    if os.path.exists(dst):
        shutil.rmtree(dst)
    shutil.copytree(src, dst, ignore=shutil.ignore_patterns(".cambc", "*.py"))
    main = os.path.join(dst, "src", "main.cpp")
    text = open(main, encoding="utf-8").read()
    burn = "        { volatile int sink = 0; for (int i = 0; i < %d; ++i) sink += i; }\n        cambc::end_turn();" % (300 * n)
    assert text.count("        cambc::end_turn();") == 1
    open(main, "w", encoding="utf-8").write(text.replace("        cambc::end_turn();", burn))


def variant(base, name, sets):
    src, dst = os.path.join("bots", base), os.path.join("bots", name)
    if os.path.exists(dst):
        shutil.rmtree(dst)
    shutil.copytree(src, dst, ignore=shutil.ignore_patterns(".cambc", "*.py"))
    path = os.path.join(dst, "src", "constants.h")
    text = open(path, encoding="utf-8").read()
    for kv in sets:
        k, v = kv.split("=")
        text, n = re.subn(r"(inline constexpr \w+ %s = )[^;]+;" % k, r"\g<1>%s;" % v, text)
        assert n == 1, k
    open(path, "w", encoding="utf-8").write(text)


def manifest(out, old, bots):
    maps = sorted(glob.glob("maps/0*.map26a"))
    rows = []
    for bot in bots:
        d = "replays/b%s/%s" % (out, bot)
        os.makedirs(d, exist_ok=True)
        for m in maps:
            name = os.path.basename(m)[: -len(".map26a")]
            m = m.replace("\\", "/")
            rows.append("bots/%s bots/%s %s %s/%s-a.repl26a --statistics %s/%s-a.json --json" % (bot, old, m, d, name, d, name))
            rows.append("bots/%s bots/%s %s %s/%s-b.repl26a --statistics %s/%s-b.json --json" % (old, bot, m, d, name, d, name))
    path = "b%s.txt" % out
    open(path, "w").write("\n".join(rows) + "\n")
    print(path, len(rows), "games")


def tally(dirs):
    t = dict(w=0, l=0, d=0, us=0, them=0, k=0, kd=0, ok=0, okd=0, cd=0, ocd=0, f=0, of=0, n=0)
    for d in dirs:
        for f in glob.glob(os.path.join(d, "*.json")):
            j = json.load(open(f))
            us = "teamA" if f.endswith("-a.json") else "teamB"
            gems = {x["team"]: int(x["submittedGems"]) for x in j["teams"]}
            them = "teamB" if us == "teamA" else "teamA"
            out = j["match"]["outcome"]
            t["n"] += 1
            # Equal gems count as a tie (the rules hand those to team B by gold, then kills).
            t["d" if gems[us] == gems[them] else "w" if out == us else "l"] += 1
            t["us"] += gems[us]
            t["them"] += gems[them]
            for e in j["events"]:
                mine = e["owner"]["team"] == us
                n = len(e["entries"])
                if e["kind"] == "kill":
                    t["k" if mine else "ok"] += n
                elif e["kind"] == "death":
                    t["kd" if mine else "okd"] += n
                    c = sum(1 for x in e["entries"] if x.get("context") == "camp")
                    t["cd" if mine else "ocd"] += c
                elif e["kind"] == "fuelExhausted":
                    t["f" if mine else "of"] += n
    return t


def summary(groups):
    for g in groups:
        t = tally(g.split(","))
        print("%-40s %3d games %3d-%3d-%-3d banked %+6.1f%%  K/D %5d/%-5d vs %5d/%-5d  camp deaths %4d vs %-4d  out of fuel %5d vs %d"
              % (g[-40:], t["n"], t["w"], t["l"], t["d"], 100.0 * (t["us"] / max(1, t["them"]) - 1), t["k"], t["kd"], t["ok"],
                 t["okd"], t["cd"], t["ocd"], t["f"], t["of"]))


if __name__ == "__main__":
    cmd = sys.argv[1]
    if cmd == "seed":
        seed(sys.argv[2], int(sys.argv[3]))
    elif cmd == "variant":
        variant(sys.argv[2], sys.argv[3], sys.argv[4:])
    elif cmd == "manifest":
        manifest(sys.argv[2], sys.argv[3], sys.argv[4].split(","))
    elif cmd == "summary":
        summary(sys.argv[2:])
