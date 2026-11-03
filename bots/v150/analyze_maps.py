"""v150 bench summary by map set (run from the repo root).

  python bots/v150/analyze_maps.py DIR[,DIR...] [DIR[,DIR...] ...]

DIRs hold <map>-a.json / <map>-b.json stats (bench.py manifest layout: the bot under test is team A in -a, B in -b).
One block per comma group; one line per map set: all, bad (the 14 ladder maps we lose most), best (our 4 best),
three / four / five champions, one altar. Columns, ours vs theirs where paired:
  record, banked % (ours / theirs - 1), K/D, camp deaths, out-of-fuel turns,
  per game: tributes, gems banked by r300, gems lost on deaths (to champions + camps), camp groups cleared by r100 and
  r300, gold earned by our non-Vesper champions, Heart of Ruin deaths, altar locks bought.
"""
import glob
import json
import os
import sys

# Map facts from bots/zz-facts (2026-10-29): champions per team, altars, walking steps spawn to spawn.
FACTS = {"00ambush": (5, 4, 123), "00anvil": (3, 1, 28), "00breadcrumbs": (3, 1, 48), "00collision": (3, 2, 34), "00contest": (5, 2, 39), "00corners": (5, 5, 40), "00crater": (5, 1, 52), "00dunes": (5, 2, 95), "00eight": (5, 1, 46), "00elkring": (5, 2, 31), "00expanse": (5, 10, 85), "00fragment": (3, 2, 23), "00galaxy": (5, 2, 112), "00gem-fields": (5, 4, 41), "00gemfight": (4, 2, 46), "00great-wall": (5, 4, 85), "00interlock": (4, 4, 41), "00lanes": (5, 18, 81), "00lost-world": (5, 3, 26), "00monolith": (5, 6, 49), "00oasis": (4, 2, 24), "00packs": (3, 6, 26), "00pilgrimage": (5, 4, 123), "00quadrants": (5, 4, 50), "00quarry": (4, 2, 40), "00reverse-spiral": (4, 2, 1), "00ribs": (5, 2, 122), "00road-of-riches": (5, 2, 243), "00rows": (4, 2, 73), "00rushbait-v2": (4, 2, 1), "00split": (5, 3, 84), "00stampede": (5, 2, 55), "00swirl": (4, 2, 13), "00the-line": (4, 1, 26), "00vault": (4, 2, 30), "01big-world": (5, 6, 139), "01boss-battle": (5, 1, 38), "01broken-curve": (3, 4, 35), "01concentrated-wealth": (5, 2, 177), "01corridors": (5, 2, 35), "01elk-arena": (5, 2, 19), "01fracture": (5, 3, 160), "01gold-rush": (3, 4, 56), "01hooks": (4, 1, 34), "01islands": (5, 10, 63), "01shortcut": (4, 2, 101), "01starfish": (5, 2, 41), "01starvation": (5, 2, 183), "01the-farm": (3, 6, 41), "01yellow-brick-road": (5, 10, 29), "02arena": (3, 2, 15), "02crossing": (5, 3, 38), "02cuts": (5, 4, 165), "02famine": (5, 4, 87), "02fields": (5, 3, 47), "02geodes": (5, 5, 72), "02gift-wrapping": (5, 4, 40), "02guardians": (4, 1, 36), "02harvest": (5, 6, 87), "02stalactites": (4, 2, 23), "02tangle": (5, 4, 71), "02the-pack": (3, 1, 22), "02the-pond": (3, 2, 38), "02treasure-box": (5, 1, 4), "02uncharted-lands": (5, 4, 42), "adgato-gemcombat1": (3, 1, 26), "adgato-gemcombat2": (4, 1, 46), "adgato-gemcombat3": (4, 1, 48), "adgato-gemcombat4": (5, 1, 60), "adgato-gemcombat5": (5, 1, 18)}
BAD = set("00breadcrumbs 01broken-curve 00reverse-spiral adgato-gemcombat1 adgato-gemcombat2 adgato-gemcombat3 "
          "adgato-gemcombat4 00anvil 01big-world 02famine 00monolith 02the-pack 01hooks 02arena".split())
BEST = set("01concentrated-wealth 02cuts 00galaxy 00dunes".split())
SETS = [("all", lambda m: True), ("bad", lambda m: m in BAD), ("best", lambda m: m in BEST),
        ("three", lambda m: FACTS[m][0] == 3), ("four", lambda m: FACTS[m][0] == 4), ("five", lambda m: FACTS[m][0] == 5),
        ("one altar", lambda m: FACTS[m][1] == 1)]
KEYS = "n w l d gems ogems k ok cd ocd f of trib otrib b300 ob300 lost olost c100 oc100 c300 oc300 gold ogold hor ohor locks olocks".split()


def game(path):
    j = json.load(open(path))
    us = "teamA" if path.endswith("-a.json") else "teamB"
    them = "teamB" if us == "teamA" else "teamA"
    t = dict.fromkeys(KEYS, 0)
    t["n"] = 1
    gems = {x["team"]: int(x["submittedGems"]) for x in j["teams"]}
    t["gems"], t["ogems"] = gems[us], gems[them]
    t["d" if gems[us] == gems[them] else "w" if j["match"]["outcome"] == us else "l"] = 1
    for e in j["events"]:
        o = "" if e["owner"]["team"] == us else "o"
        en = e["entries"]
        kind = e["kind"]
        if kind == "kill":
            t[o + "k"] += len(en)
        elif kind == "death":
            t[o + "cd"] += sum(1 for x in en if x.get("context") == "camp")
            if e["owner"].get("name") == "Heart of Ruin":
                t[o + "hor"] += len(en)
        elif kind == "fuelExhausted":
            t[o + "f"] += len(en)
        elif kind == "gemSubmission":
            t[o + "trib"] += len(en)
            t[o + "b300"] += sum(int(x["value"]) for x in en if x["timestamp"]["round"] <= 300)
        elif kind == "gemLoss":
            t[o + "lost"] += sum(int(x["value"]) for x in en)
        elif kind == "campClear":
            t[o + "c100"] += sum(1 for x in en if x["timestamp"]["round"] <= 100)
            t[o + "c300"] += sum(1 for x in en if x["timestamp"]["round"] <= 300)
        elif kind == "buyConsumable":
            t[o + "locks"] += sum(int(x.get("value", 1)) for x in en if "lock" in json.dumps(x.get("target", "")).lower())
    for c in j["champions"]:
        if c["name"] != "Vesper Vale":
            t[("" if c["team"] == us else "o") + "gold"] += int(c["metrics"]["earnedGold"])
    return t


def line(name, t):
    n = max(1, t["n"])
    pg = lambda a, b: "%5.1f/%-5.1f" % (t[a] / n, t[b] / n)
    return ("%-9s %3d %3d-%3d-%-2d banked %+6.1f%% K/D %4d/%-4d camp deaths %3d/%-3d oof %5d/%-5d | trib %s b300 %s "
            "lost %s clear100 %s clear300 %s gold %s hor deaths %s locks %s"
            % (name, t["n"], t["w"], t["l"], t["d"], 100.0 * (t["gems"] / max(1, t["ogems"]) - 1), t["k"], t["ok"],
               t["cd"], t["ocd"], t["f"], t["of"], pg("trib", "otrib"), pg("b300", "ob300"), pg("lost", "olost"),
               pg("c100", "oc100"), pg("c300", "oc300"), "%5.0f/%-5.0f" % (t["gold"] / n, t["ogold"] / n),
               pg("hor", "ohor"), pg("locks", "olocks")))


def main():
    for group in sys.argv[1:]:
        games = []
        for d in group.split(","):
            for f in sorted(glob.glob(os.path.join(d, "*.json"))):
                m = os.path.basename(f)[: -len("-a.json")]
                if m in FACTS:
                    games.append((m, game(f)))
        print("==", group[-70:])
        for name, keep in SETS:
            total = dict.fromkeys(KEYS, 0)
            for m, t in games:
                if keep(m):
                    for k in KEYS:
                        total[k] += t[k]
            if total["n"]:
                print(line(name, total))


if __name__ == "__main__":
    main()
