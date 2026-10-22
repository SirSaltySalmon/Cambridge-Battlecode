"""Per-team gem pace in ladder summaries: collected / carried / banked every 150 rounds.

usage: python ladder_pace.py SUMMARY.json...  (file names <map>-<a|b>-<win|loss>-<id>.json, as saved from cambc matches summary)
"""
import json
import os
import sys

STEP = 150


def series(timeline, end):
    out, seen, value = [], 0, 0
    runs = iter(timeline["runs"])
    for r in range(STEP, end + STEP, STEP):
        while seen < r:
            run = next(runs, None)
            if run is None:
                seen = r
                break
            value = float(run["value"])
            seen += run["duration"]
        out.append(value)
    return out


def team_series(d, team, metric):
    end = d["match"]["roundsCompleted"]
    total = None
    for t in d["championTimelines"]:
        if t["metric"] == metric and t["owner"]["team"] == team:
            s = series(t, end)
            total = s if total is None else [a + b for a, b in zip(total, s)]
    return total or []


def main():
    for f in sys.argv[1:]:
        side = os.path.basename(f).split("-")[-3]
        us = "teamA" if side == "a" else "teamB"
        them = "teamB" if us == "teamA" else "teamA"
        d = json.load(open(f))
        names = {r["id"]: r["name"] for r in d["roster"]["champions"]}
        print("== %s (r%d) us: %s | them: %s" % (os.path.basename(f)[:-5], d["match"]["roundsCompleted"],
              ",".join(n for i, n in names.items() if any(r["id"] == i and r["team"] == us for r in d["roster"]["champions"])),
              ",".join(n for i, n in names.items() if any(r["id"] == i and r["team"] == them for r in d["roster"]["champions"]))))
        for team, label in ((us, "us"), (them, "them")):
            for metric in ("collectedGems", "gems", "submittedGems"):
                print("  %-4s %-13s %s" % (label, metric, " ".join("%4.0f" % v for v in team_series(d, team, metric))))


if __name__ == "__main__":
    main()
