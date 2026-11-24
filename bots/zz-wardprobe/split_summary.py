"""Bench summary split by whether OUR team drafted Prism (her team holds big bags on purpose, so banked runs late).

usage: python bots/zz-wardprobe/split_summary.py DIR[,DIR...] [DIR[,DIR...] ...]   (one block per comma group)
Reads the stats JSON next to each replay (<map>-a.json: we are team A, <map>-b.json: team B, as bench.py lays out).
Per split: record and win %, banked % (submitted gems us/them - 1), collected % (gemCollection us/them - 1: gems
picked up from captures and kills, banked or not), gems we lost to enemy champions and gems they lost to us
(gemLoss with context champion), and kills / deaths.
"""
import glob
import json
import os
import sys


def blank():
    return dict(n=0, w=0, l=0, d=0, bank=0, obank=0, col=0, ocol=0, lost=0, olost=0, k=0, kd=0)


def tally(dirs):
    out = {"prism": blank(), "no-prism": blank()}
    for d in dirs:
        for f in glob.glob(os.path.join(d, "*.json")):
            j = json.load(open(f))
            us = "teamA" if f.endswith("-a.json") else "teamB"
            them = "teamB" if us == "teamA" else "teamA"
            ours = {c["name"] for c in j["roster"]["champions"] if c["team"] == us}
            t = out["prism" if "Prism" in ours else "no-prism"]
            gems = {x["team"]: int(x["submittedGems"]) for x in j["teams"]}
            t["n"] += 1
            t["d" if gems[us] == gems[them] else "w" if j["match"]["outcome"] == us else "l"] += 1
            t["bank"] += gems[us]
            t["obank"] += gems[them]
            for e in j["events"]:
                mine = e["owner"]["team"] == us
                if e["kind"] == "gemCollection":
                    v = sum(int(x.get("value", 0)) for x in e["entries"])
                    t["col" if mine else "ocol"] += v
                elif e["kind"] == "gemLoss":
                    v = sum(int(x.get("value", 0)) for x in e["entries"] if x.get("context") == "champion")
                    t["lost" if mine else "olost"] += v
                elif e["kind"] == "kill":
                    t["k"] += len(e["entries"]) if mine else 0
                elif e["kind"] == "death":
                    t["kd"] += len(e["entries"]) if mine else 0
    return out


def pct(a, b):
    return 100.0 * (a / max(1, b) - 1)


if __name__ == "__main__":
    for group in sys.argv[1:]:
        print(group[-60:])
        for name, t in tally(group.split(",")).items():
            if not t["n"]:
                continue
            print("  %-8s %4d games %4d-%4d-%-3d win %5.1f%%  banked %+6.1f%%  collected %+6.1f%%  gems lost to kills %6d vs %-6d"
                  "  K/D %5d/%-5d" % (name, t["n"], t["w"], t["l"], t["d"], 100.0 * t["w"] / t["n"], pct(t["bank"], t["obank"]),
                                     pct(t["col"], t["ocol"]), t["lost"], t["olost"], t["k"], t["kd"]))
