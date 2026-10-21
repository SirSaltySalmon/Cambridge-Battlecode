"""v101 bench summary: record, gems, kills, deaths and gold of each bot under test against v100, split by team size,
plus the slot champion's own line (Heart of Ruin, or Stellar / Varg / Ren in the baseline).

usage: python analyze_hor.py BENCH_DIR TAG[,TAG...] [TAG[,TAG...] ...]   (each TAG a folder of <map>-a/-b.json, the bot
under test being team A in -a; one row per comma-separated group)
"""
import collections
import glob
import json
import os
import sys


def game(path):
    j = json.load(open(path))
    us = "teamA" if path.endswith("-a.json") else "teamB"
    teams = {t["team"]: t for t in j["teams"]}
    champs = collections.defaultdict(list)
    for c in j["champions"]:
        champs[c["team"]].append(c)
    def side(team):
        cs = champs[team]
        m = lambda k: sum(float(c["metrics"].get(k, 0)) for c in cs)
        return {"gems": int(teams[team]["submittedGems"]), "kills": m("kills"), "deaths": m("deaths"),
                "gold": m("earnedGold"), "names": [c["name"] for c in cs], "champs": cs}
    them = "teamB" if us == "teamA" else "teamA"
    a, b = side(us), side(them)
    key = lambda s: (s["gems"], s["gold"], s["kills"])
    win = key(a) > key(b) or (key(a) == key(b) and us == "teamB")
    return a, b, win, len(a["names"])


def row(label, games):
    n = max(1, len(games))
    w = sum(g[2] for g in games)
    avg = lambda f: sum(f(g) for g in games) / n
    return "%-22s %3d/%-3d gems %6.0f-%-6.0f margin %+6.0f kills %5.1f deaths %5.1f K-D %+5.1f gold %6.0f-%-6.0f" % (
        label, w, len(games), avg(lambda g: g[0]["gems"]), avg(lambda g: g[1]["gems"]),
        avg(lambda g: g[0]["gems"] - g[1]["gems"]), avg(lambda g: g[0]["kills"]), avg(lambda g: g[0]["deaths"]),
        avg(lambda g: g[0]["kills"] - g[0]["deaths"]), avg(lambda g: g[0]["gold"]), avg(lambda g: g[1]["gold"]))


def slot_line(label, games, names):
    stats = collections.defaultdict(list)
    for g in games:
        for c in g[0]["champs"]:
            if c["name"] in names:
                stats[c["name"]].append(c["metrics"])
    out = []
    for name, ms in sorted(stats.items()):
        n = len(ms)
        f = lambda k: sum(float(m.get(k, 0)) for m in ms) / n
        out.append("    %-14s %-14s games %3d kills %4.1f deaths %4.1f champ dmg %6.0f taken %6.0f shield %5.0f gems %5.0f gold %5.0f" % (
            label, name, n, f("kills"), f("deaths"), f("championDamageDone"), f("championDamageTaken"),
            f("shieldingDone"), f("submittedGems"), f("earnedGold")))
    return out


def main():
    bench = sys.argv[1]
    for group in sys.argv[2:]:
        games = []
        for tag in group.split(","):
            for f in sorted(glob.glob(os.path.join(bench, tag, "*.json"))):
                games.append(game(f))
        big = [g for g in games if g[3] == 5]
        small = [g for g in games if g[3] < 5]
        print(row(group[:22], games))
        print(row("  5-pick maps", big))
        print(row("  3/4-pick maps", small))
        for line in slot_line(group[:14], games, {"Heart of Ruin", "Stellar", "Varg", "Ren"}):
            print(line)


if __name__ == "__main__":
    main()
