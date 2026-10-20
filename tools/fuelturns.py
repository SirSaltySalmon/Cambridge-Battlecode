"""Fuel per turn matched to the decision log: python tools/fuelturns.py DIR [LIMIT]

The .fuel values after the header (8 budget values + one per draft pick) are one per champion turn, in the same order
as the turn blocks of the decision log (debug builds only). Prints, for turns at or over LIMIT (default 100000), the
count by branch and by round 1 vs later, and the 10 most expensive turns.
"""
import collections
import glob
import json
import os
import struct
import sys
import zipfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import logparse


def main():
    d0 = sys.argv[1]
    limit = int(sys.argv[2]) if len(sys.argv) > 2 else 100000
    by_branch = collections.Counter()
    total_by_branch = collections.Counter()
    fuel_by_branch = collections.Counter()
    worst = []
    mism = 0
    for f in sorted(glob.glob(os.path.join(d0, "*.repl26a"))):
        side = "a" if f.endswith("-a.repl26a") else "b"
        z = zipfile.ZipFile(f)
        ts = logparse.turns(z.read("team-%s.stdout" % side).decode(errors="replace").replace("\x00", ""))
        raw = z.read("team-%s.fuel" % side)
        vals = [v[0] for v in struct.iter_unpack("<Q", raw[13:13 + (len(raw) - 13) // 8 * 8])]
        js = f[:-8] + ".json"
        picks = 5
        if os.path.exists(js):
            d = json.load(open(js))
            us = "team" + side.upper()
            ours = [c for c in d["champions"] if c["team"] == us]
            names = {c["name"] for c in ours}
            picks = len(ours) - (1 if "Cassi" in names and "Issac" in names else 0)
        vals = vals[8 + picks:]
        if len(vals) != len(ts):
            mism += 1
        for t, v in zip(ts, vals):
            b = "round1" if t["round"] == 1 else (t["branch"] or "?")
            total_by_branch[b] += 1
            fuel_by_branch[b] += v
            if v >= limit and t["round"] > 1:
                by_branch[b] += 1
                worst.append((v, os.path.basename(f), t["round"], t["champ"], b, t["shared"][:60]))
    print("files with turn/fuel count mismatch: %d" % mism)
    print("turns >= %d by branch (round 1 excluded):" % limit)
    for b, n in by_branch.most_common(25):
        print("  %-28s %5d of %6d turns, mean %.1fk" % (b, n, total_by_branch[b], fuel_by_branch[b] / total_by_branch[b] / 1000))
    worst.sort(reverse=True)
    for w in worst[:10]:
        print("  %7d %s r%d c%d %s | %s" % w)


if __name__ == "__main__":
    main()
