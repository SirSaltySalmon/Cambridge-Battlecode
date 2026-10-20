"""Compare a bench against a mirror baseline game by game: which results the new bot flips, and its gem margin.

usage: python analyze_flips.py MIRROR_DIR BENCH_DIR  (MIRROR_DIR holds <map>.json from OLD vs OLD; BENCH_DIR holds
<map>-a.json / <map>-b.json from NEW vs OLD, -a = NEW is team A)
"""
import glob
import json
import os
import sys


def bank(d):
    return {t["team"]: int(t["submittedGems"]) for t in d["teams"]}


def main():
    base = {}
    for f in glob.glob(os.path.join(sys.argv[1], "*.json")):
        d = json.load(open(f))
        base[os.path.basename(f)[:-5]] = (d["match"]["outcome"], bank(d))
    gains = losses = 0
    margin = margin_base = 0
    for f in sorted(glob.glob(os.path.join(sys.argv[2], "*.json"))):
        name = os.path.basename(f)[:-5]
        m, side = name.rsplit("-", 1)
        if m not in base:
            continue
        d = json.load(open(f))
        us = "teamA" if side == "a" else "teamB"
        them = "teamB" if us == "teamA" else "teamA"
        won, bwon = d["match"]["outcome"] == us, base[m][0] == us
        b, bb = bank(d), base[m][1]
        margin += b[us] - b[them]
        margin_base += bb[us] - bb[them]
        flag = ""
        if won != bwon:
            flag = "<< gained" if won else "<< lost"
            gains += won
            losses += bwon
        print("%-26s new %s %4d-%4d | mirror %s %4d-%4d %s" % (name, "W" if won else "L", b[us], b[them],
                                                           "W" if bwon else "L", bb[us], bb[them], flag))
    print("flips: %d gained, %d lost; summed gem margin %+d (mirror %+d)" % (gains, losses, margin, margin_base))


if __name__ == "__main__":
    main()
