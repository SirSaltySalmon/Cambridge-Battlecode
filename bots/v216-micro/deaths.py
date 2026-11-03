"""Our deaths and kills per champion from evaluate.py --keep stats JSONs (no replay decode; release runs are fine).

usage: python3 bots/v216-micro/deaths.py DIR [DIR...]
The bot under test is team A of "-a" games and team B of "-b" games. Prints, per champion of ours: deaths to champions,
deaths to camps, kills, champion damage done/taken (sums of the championDamageDone/Taken timelines), games present.
"""
import collections
import glob
import json
import os
import sys


def main():
    for d in sys.argv[1:]:
        rows = collections.defaultdict(lambda: collections.Counter())
        games = 0
        for p in sorted(glob.glob(os.path.join(d, "*.json"))):
            if p.endswith(".rounds.json"):
                continue
            tag = os.path.basename(p)[:-5]
            if tag[-2:] not in ("-a", "-b"):
                continue
            us = "team" + ("A" if tag.endswith("-a") else "B")
            st = json.load(open(p))
            games += 1
            present = set()
            for e in st["events"]:
                if e["owner"]["team"] != us:
                    continue
                name = e["owner"]["name"]
                present.add(name)
                if e["kind"] == "death":
                    for x in e["entries"]:
                        rows[name]["deaths_" + x["context"]] += 1
            for t in st["championTimelines"]:
                if t["owner"]["team"] != us:
                    continue
                name = t["owner"]["name"]
                if t["metric"] in ("championDamageDone", "championDamageTaken", "kills"):
                    # cumulative timelines: the last value is the game total
                    total = float(t["runs"][-1]["value"]) if t["runs"] else 0
                    rows[name][t["metric"]] += total
                    present.add(name)
            for name in present:
                rows[name]["games"] += 1
        print("%s: %d games" % (d, games))
        print("  %-14s %5s %7s %6s %6s %10s %10s" % ("champion", "games", "d_champ", "d_camp", "kills", "dmg_done", "dmg_taken"))
        for name, c in sorted(rows.items(), key=lambda kv: -kv[1]["deaths_champion"]):
            print("  %-14s %5d %7d %6d %6d %10.0f %10.0f" % (name[:14], c["games"], c["deaths_champion"], c["deaths_camp"],
                                                         c["kills"], c["championDamageDone"], c["championDamageTaken"]))


if __name__ == "__main__":
    main()
