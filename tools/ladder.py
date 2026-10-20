"""Summarise downloaded ladder matches (cambc matches summary --json): python tools/ladder.py DIR [TEAM_ID_PREFIX]

Files are named <map>-<a|b>-<id8>.json, where a/b is our side. Per match and in total, for us and the opponent:
result, tributed gems, captures (gems from camp groups), kills, deaths (by champions / by camps), gems lost on death,
gems stolen from kills, tributes (count, first round), wards placed, consumables bought.
"""
import collections
import glob
import json
import os
import sys


def team_stats(d, team):
    s = collections.Counter()
    firsts = []
    cons = collections.Counter()
    for e in d["events"]:
        if e["owner"].get("team") != team:
            continue
        k = e["kind"]
        for x in e["entries"]:
            r = x["timestamp"]["round"]
            v = int(x["value"]) if str(x["value"]).lstrip("-").isdigit() else 0
            if k == "death":
                s["deaths"] += 1
                s["deaths_camp" if x["context"] == "camp" else "deaths_champ"] += 1
            elif k == "kill":
                s["kills"] += 1
            elif k == "gemLoss":
                s["gems_lost"] += v
            elif k == "gemCollection":
                if x["context"] == "campGroup":
                    s["captured"] += v
                    s["captures"] += 1
                else:
                    s["stolen"] += v
            elif k == "gemSubmission":
                s["tributes"] += 1
                s["tributed"] += v
                firsts.append(r)
            elif k == "wardPlacement":
                s["wards"] += 1
            elif k == "buyConsumable":
                cons[x["target"].get("name", "?")] += v or 1
    s["first_tribute"] = min(firsts) if firsts else 0
    return s, cons


def main():
    d0 = sys.argv[1]
    tot = {"us": collections.Counter(), "them": collections.Counter()}
    cons_tot = {"us": collections.Counter(), "them": collections.Counter()}
    wins = games = 0
    by_map = collections.defaultdict(lambda: [0, 0])
    print("%-24s %-22s R  gems    opp | deaths(champ/camp) lost stolen | trib first | opp deaths lost trib first | opp consumables"
          % ("match", "opponent"))
    for f in sorted(glob.glob(os.path.join(d0, "*.json"))):
        d = json.load(open(f))
        name = os.path.basename(f)[:-5]
        side = name.rsplit("-", 2)[-2]
        us = "team" + side.upper()
        them = "teamB" if us == "teamA" else "teamA"
        gems = {t["team"]: int(t["submittedGems"]) for t in d["teams"]}
        a, ca = team_stats(d, us)
        b, cb = team_stats(d, them)
        win = d["match"]["outcome"] == us
        games += 1
        wins += win
        m = name.rsplit("-", 2)[0]
        by_map[m][0 if win else 1] += 1
        tot["us"].update(a)
        tot["them"].update(b)
        cons_tot["us"].update(ca)
        cons_tot["them"].update(cb)
        opp = "?"
        for c in d["roster"]["champions"]:
            pass
        print("%-24s %-22s %s %5d %5d | %3d (%2d/%2d) %4d %4d | %3d %4d | %3d %4d %3d %4d | %s" % (
            name, "", "W" if win else "L", gems[us], gems[them], a["deaths"], a["deaths_champ"], a["deaths_camp"],
            a["gems_lost"], a["stolen"], a["tributes"], a["first_tribute"], b["deaths"], b["gems_lost"], b["tributes"],
            b["first_tribute"], dict(cb)))
    print("\n%d-%d" % (wins, games - wins))
    for who in ("us", "them"):
        t = tot[who]
        g = max(1, games)
        print("%-4s per game: tributed %.0f, captured %.0f (%.1f captures), kills %.1f, deaths %.1f (champ %.1f, camp %.1f), "
              "gems lost %.0f, stolen %.0f, tributes %.1f, wards %.0f; consumables %s" % (
                  who, t["tributed"] / g, t["captured"] / g, t["captures"] / g, t["kills"] / g, t["deaths"] / g,
                  t["deaths_champ"] / g, t["deaths_camp"] / g, t["gems_lost"] / g, t["stolen"] / g, t["tributes"] / g,
                  t["wards"] / g, dict(cons_tot[who])))
    print("by map:", {k: "%d-%d" % tuple(v) for k, v in sorted(by_map.items())})


if __name__ == "__main__":
    main()
