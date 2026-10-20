"""Farming efficiency from bench statistics JSON (BOT's side vs the opponent's, same games).

usage:
  python tools/farm.py rows DIR [DIR...]        one row per bench folder (replays/bench/BOT-vs-OPP-<kind>)
  python tools/farm.py table KIND BOT [BOT...]  each BOT vs v82 and vs v83 in replays/bench (KIND normal|unlimited)

Columns (totals over the folder's games, BOT's side first, opponent's second):
  W-L       record
  trib      submitted gems (the score)
  col/100r  gems collected per 100 rounds (camps + kills)
  camp/100r camp captures per 100 rounds (gemCollection from a camp group; one per camp and round per team)
  gem/camp  camp gems per capture
  kill%     share of collected gems taken from enemy champions
  lost      gems lost on death
  bank%     submitted / collected
"""
import glob
import json
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def team_stats(d, team):
    s = dict(trib=0, col=0, camp_gems=0, kill_gems=0, camps=0, lost=0)
    captures = set()
    for t in d["teams"]:
        if t["team"] == team:
            s["trib"] = int(t["submittedGems"])
    for c in d["champions"]:
        if c["team"] == team:
            s["col"] += int(c["metrics"]["collectedGems"])
    for e in d["events"]:
        if e["owner"].get("team") != team:
            continue
        if e["kind"] == "gemCollection":
            for x in e["entries"]:
                if x["context"] == "campGroup":
                    s["camp_gems"] += int(x["value"])
                    captures.add((x["timestamp"]["round"], x["target"]["id"]))
                else:
                    s["kill_gems"] += int(x["value"])
        elif e["kind"] == "gemLoss":
            s["lost"] += sum(int(x["value"]) for x in e["entries"])
    s["camps"] = len(captures)
    return s


def folder(path):
    us_t = dict(trib=0, col=0, camp_gems=0, kill_gems=0, camps=0, lost=0)
    them_t = dict(us_t)
    wins = games = rounds = 0
    for f in sorted(glob.glob(os.path.join(path, "*.json"))):
        if not re.search(r"-[ab]\.json$", f):
            continue
        try:
            d = json.load(open(f))
        except Exception:
            continue
        us = "teamA" if f.endswith("-a.json") else "teamB"
        them = "teamB" if us == "teamA" else "teamA"
        for tot, t in ((us_t, us), (them_t, them)):
            for k, v in team_stats(d, t).items():
                tot[k] += v
        games += 1
        wins += d["match"]["outcome"] == us
        rounds += d["match"]["roundsCompleted"]
    return dict(games=games, wins=wins, rounds=rounds, us=us_t, them=them_t)


def fmt(r):
    if not r or not r["games"]:
        return "%-58s" % "  (missing)"
    us, th, n = r["us"], r["them"], max(1, r["rounds"]) / 100.0

    def pair(a, b, f="%5.1f"):
        return (f + "/" + f) % (a, b)
    return "%2d-%-2d %5d/%-5d %s %s %s %s %5d/%-5d %s" % (
        r["wins"], r["games"] - r["wins"], us["trib"], th["trib"],
        pair(us["col"] / n, th["col"] / n, "%4.0f"), pair(us["camps"] / n, th["camps"] / n, "%4.2f"),
        pair(us["camp_gems"] / max(1, us["camps"]), th["camp_gems"] / max(1, th["camps"]), "%4.1f"),
        pair(100.0 * us["kill_gems"] / max(1, us["col"]), 100.0 * th["kill_gems"] / max(1, th["col"]), "%2.0f"),
        us["lost"], th["lost"],
        pair(100.0 * us["trib"] / max(1, us["col"]), 100.0 * th["trib"] / max(1, th["col"]), "%2.0f"))


HEAD = " W-L   trib us/them  col/100r  camp/100r  gem/camp   kill%  lost us/them bank%"


def rows(argv):
    print("%-40s %s" % ("folder", HEAD))
    for p in argv:
        print("%-40s %s" % (os.path.basename(os.path.normpath(p)), fmt(folder(p))))


def table(argv):
    kind, bots = argv[0], argv[1:]
    bench = os.path.join(ROOT, "replays", "bench")
    for opp in ("v82", "v83"):
        print("\nvs %s (%s)\n%-12s %s" % (opp, kind, "bot", HEAD))
        for b in bots:
            p = os.path.join(bench, "%s-vs-%s-%s" % (b, opp, kind))
            print("%-12s %s" % (b, fmt(folder(p) if os.path.isdir(p) else None)))


if __name__ == "__main__":
    {"rows": rows, "table": table}[sys.argv[1]](sys.argv[2:])
