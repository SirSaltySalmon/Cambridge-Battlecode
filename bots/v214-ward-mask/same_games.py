"""Base-rule check: compare two replay dirs game by game (same map and side).

  python -I bots/v214-ward-mask/same_games.py DIR_A DIR_B

Prints how many games have the same winner and the same gems for both teams, and lists the ones that differ.
"""
import glob
import json
import os
import sys


def load(d):
    out = {}
    for f in glob.glob(os.path.join(d, "*.json")):
        j = json.load(open(f))
        gems = tuple(sorted((x["team"], int(x["submittedGems"])) for x in j["teams"]))
        out[os.path.basename(f)] = (j["match"]["outcome"], gems, j["match"].get("rounds", j["match"].get("round")))
    return out


a, b = load(sys.argv[1]), load(sys.argv[2])
common = sorted(set(a) & set(b))
same = [k for k in common if a[k][:2] == b[k][:2]]
winner = [k for k in common if a[k][0] == b[k][0]]
print("common", len(common), "same winner", len(winner), "same winner and gems", len(same))
for k in common:
    if a[k][:2] != b[k][:2]:
        print("  differ", k, a[k], b[k])
