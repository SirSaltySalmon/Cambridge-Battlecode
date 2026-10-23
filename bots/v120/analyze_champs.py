"""Deaths and kills per champion for the bench side: python analyze_champs.py DIR [DIR...]

-a.json = the bot under test is team A. Per champion name: deaths to champions, deaths to camps, kills, over all games.
"""
import glob, json, os, sys
from collections import Counter

for dir_ in sys.argv[1:]:
    deaths, camp, kills, games = Counter(), Counter(), Counter(), Counter()
    for f in glob.glob(os.path.join(dir_, "*.json")):
        d = json.load(open(f))
        us = "teamA" if f.endswith("-a.json") else "teamB"
        ids = {r["id"]: r["name"] for r in d["roster"]["champions"] if r["team"] == us}
        for name in ids.values():
            games[name] += 1
        for e in d["events"]:
            owner = e["owner"].get("id")
            if owner not in ids or e["owner"].get("team") != us:
                continue
            for x in e["entries"]:
                if e["kind"] == "death":
                    (camp if x["context"] == "camp" else deaths)[ids[owner]] += 1
                elif e["kind"] == "kill":
                    kills[ids[owner]] += 1
    print("== " + os.path.basename(dir_.rstrip("/\\")))
    for name in sorted(games):
        print("  %-14s games %2d  deaths %4d (camp %3d)  kills %4d" % (name, games[name], deaths[name], camp[name], kills[name]))
