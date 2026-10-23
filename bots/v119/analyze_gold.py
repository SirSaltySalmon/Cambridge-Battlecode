"""Summarise a gold-weight bench: per variant dir, record vs the baseline and farming totals for both sides.

usage: python analyze_gold.py DIR [DIR...]  (each DIR holds <map>-a.json / <map>-b.json; -a = the variant is team A)
"""
import glob
import json
import os
import sys


CHECKPOINTS = (300, 600, 900)


def banked_at(d, team, round_):
    # Tributed gems after round_ rounds (the last value when the game ended earlier).
    for t in d["teamTimelines"]:
        if t["metric"] == "submittedGems" and t["owner"] == team:
            seen, value = 0, 0
            for run in t["runs"]:
                if seen >= round_:
                    break
                value = int(run["value"])
                seen += run["duration"]
            return value
    return 0


def side_stats(d, team):
    ids = {r["id"] for r in d["roster"]["champions"] if r["team"] == team}
    m = [c["metrics"] for c in d["champions"] if c["id"] in ids]
    total = lambda k: sum(float(x[k]) for x in m)
    return {
        "banked": next(int(t["submittedGems"]) for t in d["teams"] if t["team"] == team),
        "collected": total("collectedGems"),
        "gold": total("earnedGold"),
        "kills": total("kills"),
        "deaths": total("deaths"),
        **{"r%d" % r: banked_at(d, team, r) for r in CHECKPOINTS},
    }


def main():
    for dir_ in sys.argv[1:]:
        rows = []
        for f in sorted(glob.glob(os.path.join(dir_, "*.json"))):
            d = json.load(open(f))
            us = "teamA" if f.endswith("-a.json") else "teamB"
            them = "teamB" if us == "teamA" else "teamA"
            rows.append((os.path.basename(f)[:-5], d["match"]["outcome"] == us, d["match"]["roundsCompleted"],
                         side_stats(d, us), side_stats(d, them)))
        if not rows:
            continue
        wins = sum(r[1] for r in rows)
        s = lambda i, k: sum(r[i][k] for r in rows)
        print("== %s: %d-%d" % (os.path.basename(dir_.rstrip("/\\")), wins, len(rows) - wins))
        for k in ("banked", "collected", "gold", "kills", "deaths") + tuple("r%d" % r for r in CHECKPOINTS):
            a, b = s(3, k), s(4, k)
            print("  %-9s %8.0f vs %8.0f (%+.1f%%)" % (k, a, b, 100.0 * (a - b) / max(1, b)))
        print("  lost: " + ", ".join("%s %d-%d" % (r[0], r[3]["banked"], r[4]["banked"]) for r in rows if not r[1]))


if __name__ == "__main__":
    main()
