"""v215 (TP) bench summary: record, gems, deaths, crystals, fuel, for groups of bench folders.

usage: python analyze_tp.py VALUES NAME=DIR[,DIR...] [NAME=DIR[,DIR...] ...]
Each DIR holds <map>-a/-b .json + .repl26a pairs; the bot under test is team A in -a files and team B in -b files.
VALUES is a file of "<map> <crystal value>" lines (from the draft log) used to split crystal maps from the rest;
pass - to skip the split. Turns of exactly 100000 fuel ran out; entries over 100000 (draft, first turns) are left out.
"""
import collections
import glob
import json
import os
import struct
import sys
import zipfile

LIMIT = 100000


def fuel_lists(raw):
    values = [struct.unpack_from("<Q", raw, 13 + 8 * i)[0] for i in range((len(raw) - 13) // 8)]
    rest = values[8:]
    start = rest.index(2 * LIMIT) if 2 * LIMIT in rest else 0
    draft = rest[:start]
    while start < len(rest) and rest[start] == 2 * LIMIT:
        start += 1
    return draft, [x for x in rest[start:] if x <= LIMIT]


def game(path):
    side = "a" if path.endswith("-a.json") else "b"
    team = "teamA" if side == "a" else "teamB"
    d = json.load(open(path))
    gems = {t["team"]: int(t["submittedGems"]) for t in d["teams"]}
    other = "teamB" if team == "teamA" else "teamA"
    outcome = d["match"].get("outcome")
    deaths = kills = faults = oof = 0
    for e in d["events"]:
        owner = e.get("owner", {})
        n = len(e.get("entries", []))
        if owner.get("team") != team:
            continue
        if e["kind"] == "death":
            deaths += n
        elif e["kind"] == "kill":
            kills += n
        elif e["kind"] == "botFault":
            faults += n
        elif e["kind"] == "fuelExhausted":
            oof += n
    bought = 0
    for c in d["champions"]:
        if c.get("team") != team:
            continue
        for b in c.get("consumablesBought", []):
            if b["consumable"] == "Teleport Crystal":
                bought += int(b["quantity"])
    repl = path[:-5] + ".repl26a"
    draft, turns = ([], [])
    if os.path.exists(repl):
        draft, turns = fuel_lists(zipfile.ZipFile(repl).read("team-%s.fuel" % side))
    name = os.path.basename(path)[:-7]
    return dict(map=name, side=side, win=outcome == team, loss=outcome == other, ours=gems.get(team, 0),
                theirs=gems.get(other, 0), deaths=deaths, kills=kills, faults=faults, oof_events=oof, bought=bought,
                rounds=d["match"].get("roundsCompleted", 0), draft=max(draft) if draft else 0, turns=turns)


def summarise(name, games, values, tag=""):
    if not games:
        return
    w = sum(g["win"] for g in games)
    l = sum(g["loss"] for g in games)
    ours = sum(g["ours"] for g in games)
    theirs = sum(g["theirs"] for g in games)
    deaths = sum(g["deaths"] for g in games)
    kills = sum(g["kills"] for g in games)
    turns = [x for g in games for x in g["turns"]]
    n = max(1, len(turns))
    oof = sum(1 for x in turns if x == LIMIT)
    bought = sum(g["bought"] for g in games)
    faults = sum(g["faults"] for g in games)
    print("%-22s %-10s games %4d  W-L %4d-%-4d (%.1f%%)  gems %7d vs %7d  deaths %5d kills %5d  crystals %5d  "
          "fuel mean %6.0f  oof %5d  faults %d  draft max %d" % (
              name, tag, len(games), w, l, 100.0 * w / len(games), ours, theirs, deaths, kills, bought,
              sum(turns) / n, oof, faults, max(g["draft"] for g in games)))


def main():
    values = {}
    if sys.argv[1] != "-":
        for line in open(sys.argv[1]):
            parts = line.split()
            if len(parts) >= 2:
                values[parts[0]] = int(parts[1])
    groups = collections.OrderedDict()
    for arg in sys.argv[2:]:
        name, dirs = arg.split("=", 1)
        gs = []
        for dname in dirs.split(","):
            for f in sorted(glob.glob(os.path.join(dname, "*.json"))):
                gs.append(game(f))
        groups[name] = gs
    for name, gs in groups.items():
        summarise(name, gs, values, "all")
        if values:
            summarise(name, [g for g in gs if values.get(g["map"], 0) >= 10], values, "value>=10")
            summarise(name, [g for g in gs if 1 <= values.get(g["map"], 0) < 10], values, "value1-9")
            summarise(name, [g for g in gs if values.get(g["map"], 0) == 0], values, "value0")
    # Per map: wins per group, for the first two groups.
    names = list(groups)
    if len(names) >= 2:
        a, b = names[0], names[1]
        per = collections.defaultdict(lambda: [0, 0, 0, 0, 0, 0])
        for g in groups[a]:
            per[g["map"]][0] += g["win"]
            per[g["map"]][1] += 1
            per[g["map"]][4] += g["ours"]
        for g in groups[b]:
            per[g["map"]][2] += g["win"]
            per[g["map"]][3] += 1
            per[g["map"]][5] += g["ours"]
        rows = sorted(per.items(), key=lambda kv: (kv[1][0] / max(1, kv[1][1]) - kv[1][2] / max(1, kv[1][3])))
        print("\nper map (%s vs %s): wins/games, banked gems; value" % (a, b))
        for m, (wa, na, wb, nb, ga, gb) in rows:
            if wa * nb != wb * na:
                print("  %-24s %2d/%-2d vs %2d/%-2d  gems %6d vs %6d  value %s" % (m, wa, na, wb, nb, ga, gb, values.get(m, "?")))


if __name__ == "__main__":
    main()
