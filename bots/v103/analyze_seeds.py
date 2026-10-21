"""Per-map record and mean gem margin of seeded bench runs (see README: bench noise), old seeds against new seeds.

usage: python analyze_seeds.py "OLD_DIR[,OLD_DIR...]" "NEW_DIR[,NEW_DIR...]"  (each DIR holds <map>-a/-b.json, the bot
under test being team A in -a)
"""
import collections
import glob
import json
import os
import sys


def runs(dirs):
    r = collections.defaultdict(lambda: [0, 0, 0])
    for d in dirs:
        for f in glob.glob(os.path.join(d, "*.json")):
            j = json.load(open(f))
            us = "teamA" if f.endswith("-a.json") else "teamB"
            them = "teamB" if us == "teamA" else "teamA"
            b = {t["team"]: int(t["submittedGems"]) for t in j["teams"]}
            m = os.path.basename(f)[:-7]
            r[m][0] += j["match"]["outcome"] == us
            r[m][1] += 1
            r[m][2] += b[us] - b[them]
    return r


def main():
    old, new = runs(sys.argv[1].split(",")), runs(sys.argv[2].split(","))
    print("%-24s %14s %14s" % ("map", "old W/G margin", "new W/G margin"))
    tot = [0, 0, 0, 0]
    for m in sorted(old):
        o, n = old[m], new.get(m, [0, 0, 0])
        tot = [tot[0] + o[0], tot[1] + o[1], tot[2] + n[0], tot[3] + n[1]]
        flag = "<<" if n[1] and abs(n[0] / n[1] - o[0] / o[1]) >= 0.34 else ""
        print("%-24s %3d/%-3d %+6.0f %3d/%-3d %+6.0f %s" % (m, o[0], o[1], o[2] / o[1], n[0], n[1], n[2] / max(1, n[1]), flag))
    print("total old %d/%d, new %d/%d" % tuple(tot))


if __name__ == "__main__":
    main()
