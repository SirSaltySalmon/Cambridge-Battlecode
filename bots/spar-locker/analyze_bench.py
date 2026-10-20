"""Summarise v81 benchmark games: results, mage damage from camps, monolith captures, mage branch counts.

usage: python analyze.py BOT DIR [DIR...]  (every *.json with a matching *.repl26a in the dirs)
"""
import collections
import glob
import json
import os
import re
import sys
import zipfile

MAGES = {"Cassi", "Stellar"}
BRANCH = re.compile(r"^r\d+ ([a-z][\w.]*): ")


def team_of(d, bot_side):
    return "teamA" if bot_side == "A" else "teamB"


def group_captures(d, team_ids):
    # Sum gems per (round, camp group) for our champions: a monolith pays 200 split among the collectors.
    per = collections.Counter()
    for e in d["events"]:
        if e["kind"] != "gemCollection" or e["owner"]["id"] not in team_ids:
            continue
        for x in e["entries"]:
            if x["target"]["kind"] == "campGroup":
                per[(x["timestamp"]["round"], x["target"]["id"])] += int(x["value"])
    return per


def main():
    bot = sys.argv[1]
    files = sorted(f for dir_ in sys.argv[2:] for f in glob.glob(os.path.join(dir_, "*.json")))
    total = collections.Counter()
    branches = collections.Counter()
    rows = []
    for f in files:
        d = json.load(open(f))
        names = {r["id"]: r for r in d["roster"]["champions"]}
        repl = f[:-5] + ".repl26a"
        side = None
        stdout = ""
        if os.path.exists(repl):
            z = zipfile.ZipFile(repl)
            # The side running BOT is named in the file name: <map>-<a|b>, a = BOT is team A.
            side = "A" if f.endswith("-a.json") else "B"
            stdout = z.read("team-%s.stdout" % side.lower()).decode(errors="replace")
        us = team_of(d, side)
        them = "teamB" if us == "teamA" else "teamA"
        ours = {i for i, r in names.items() if r["team"] == us}
        gems = {t["team"]: int(t["submittedGems"]) for t in d["teams"]}
        metrics = {c["id"]: c["metrics"] for c in d["champions"]}
        kills = sum(int(metrics[i]["kills"]) for i in ours)
        deaths = sum(int(metrics[i]["deaths"]) for i in ours)
        mage_ids = [i for i in ours if names[i]["name"] in MAGES]
        mage_camp = sum(float(metrics[i]["monsterDamageTaken"]) for i in mage_ids)
        mage_deaths = sum(int(metrics[i]["deaths"]) for i in mage_ids)
        caps = group_captures(d, ours)
        monos = sum(1 for v in caps.values() if v >= 150)
        win = d["match"]["outcome"] == us
        line = os.path.basename(f)[:-5]
        rows.append((line, "W" if win else "L", d["match"]["roundsCompleted"], gems[us], gems[them], kills, deaths,
                     mage_deaths, int(mage_camp), monos))
        total.update(games=1, wins=win, gems=gems[us], opp=gems[them], kills=kills, deaths=deaths,
                     mage_deaths=mage_deaths, mage_camp=mage_camp, monos=monos)
        for raw in stdout.splitlines():
            m = BRANCH.match(raw.lstrip("\x00 "))
            if m and (m.group(1).startswith(("mono.", "farm.camp.", "retreat.camp.", "fight.hit.", "farm.guard"))):
                branches[m.group(1)] += 1
    print("%-28s %s %5s %6s %6s %5s %6s %6s %8s %5s" % ("game", "R", "round", "gems", "opp", "kill", "death", "mDeath",
                                                       "mCampDmg", "mono"))
    for r in rows:
        print("%-28s %s %5d %6d %6d %5d %6d %6d %8d %5d" % r)
    g = max(1, total["games"])
    print("\n%s: %d-%d, gems %d vs %d (%+.1f%%), kills %d deaths %d, mage deaths %.2f/game, mage camp damage %.0f/game, "
          "monolith captures %d" % (bot, total["wins"], total["games"] - total["wins"], total["gems"], total["opp"],
                                    100.0 * (total["gems"] - total["opp"]) / max(1, total["opp"]), total["kills"],
                                    total["deaths"], total["mage_deaths"] / g, total["mage_camp"] / g, total["monos"]))
    if branches:
        print("\nmage/monolith branches (our side):")
        for k, v in branches.most_common():
            print("  %-28s %d" % (k, v))


if __name__ == "__main__":
    main()
