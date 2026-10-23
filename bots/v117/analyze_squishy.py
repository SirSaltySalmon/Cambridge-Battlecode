"""Camp damage taken and deaths of the squishy casters (Cassi, Stellar, Willow), per bot, plus record and gems.

usage: python analyze_squishy.py NEW OLD DIR [DIR...]  (NEW is team A in <map>-a and team B in <map>-b)
"""
import collections
import glob
import json
import os
import sys

SQUISHY = ("Cassi", "Stellar", "Willow")


def main():
    new, old = sys.argv[1], sys.argv[2]
    files = sorted(f for d in sys.argv[3:] for f in glob.glob(os.path.join(d, "*.json")))
    total = collections.defaultdict(collections.Counter)
    per_name = collections.defaultdict(collections.Counter)
    for f in files:
        d = json.load(open(f))
        game = os.path.basename(f)[:-5]
        bot_of = {"teamA": new, "teamB": old} if game.endswith("-a") else {"teamA": old, "teamB": new}
        names = {r["id"]: r for r in d["roster"]["champions"]}
        gems = {t["team"]: int(t["submittedGems"]) for t in d["teams"]}
        for team, bot in bot_of.items():
            total[bot].update(games=1, wins=d["match"]["outcome"] == team, gems=gems[team])
        for c in d["champions"]:
            r = names[c["id"]]
            bot = bot_of[r["team"]]
            m = c["metrics"]
            total[bot].update(deaths=int(m["deaths"]), camp_taken=float(m["monsterDamageTaken"]))
            if r["name"] in SQUISHY:
                total[bot].update(sq_deaths=int(m["deaths"]), sq_camp_taken=float(m["monsterDamageTaken"]),
                                  sq_camp_done=float(m["monsterDamageDone"]))
                per_name[(bot, r["name"])].update(games=1, deaths=int(m["deaths"]), camp_taken=float(m["monsterDamageTaken"]))
    for bot in (new, old):
        t = total[bot]
        g = max(1, t["games"])
        print("%s: %d-%d, gems %d; all deaths %d, camp damage taken %.0f/game" % (
            bot, t["wins"], t["games"] - t["wins"], t["gems"], t["deaths"], t["camp_taken"] / g))
        print("  casters (Cassi, Stellar, Willow): deaths %d, camp damage taken %.0f/game, camp damage done %.0f/game" % (
            t["sq_deaths"], t["sq_camp_taken"] / g, t["sq_camp_done"] / g))
        for name in SQUISHY:
            p = per_name[(bot, name)]
            if p["games"]:
                print("    %-8s %2d games: deaths %3d, camp damage taken %.0f/game" % (name, p["games"], p["deaths"],
                                                                                    p["camp_taken"] / p["games"]))


if __name__ == "__main__":
    main()
