"""Banked by round 300 / 600, us vs them, over non-Prism games only (our team did not draft Prism).

  python bots/v214-ward-mask/gold_noprism.py DIR[,DIR...] [DIR[,DIR...] ...]
Uses bots/v114/analyze_gold.py's banked_at.
"""
import glob
import json
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "v114"))
from analyze_gold import banked_at  # noqa: E402

for group in sys.argv[1:]:
    n, s = 0, {300: [0, 0], 600: [0, 0]}
    for d in group.split(","):
        for f in glob.glob(os.path.join(d, "*.json")):
            j = json.load(open(f))
            us = "teamA" if f.endswith("-a.json") else "teamB"
            them = "teamB" if us == "teamA" else "teamA"
            if "Prism" in {c["name"] for c in j["roster"]["champions"] if c["team"] == us}:
                continue
            n += 1
            for r in s:
                s[r][0] += banked_at(j, us, r)
                s[r][1] += banked_at(j, them, r)
    print("%-50s non-prism %d games  by r300 %+.1f%%  by r600 %+.1f%%" % (
        group[-50:], n, *(100.0 * (s[r][0] / max(1, s[r][1]) - 1) for r in (300, 600))))
