"""Camp deaths and camp damage per game for the bench side: python analyze_camps.py DIR [DIR...]

-a.json = the bot under test is team A. Prints per map: camp deaths (ours / theirs), champion deaths, camp damage
taken, first camp death round, then totals per DIR.
"""
import glob, json, os, sys


def side(d, team):
    ids = {r["id"] for r in d["roster"]["champions"] if r["team"] == team}
    camp = champ = 0
    first = None
    for e in d["events"]:
        if e["kind"] != "death" or e["owner"].get("id") not in ids or e["owner"].get("team") != team:
            continue
        for x in e["entries"]:
            if x["context"] == "camp":
                camp += 1
                r = x["timestamp"]["round"]
                first = r if first is None else min(first, r)
            else:
                champ += 1
    dmg = sum(float(c["metrics"]["monsterDamageTaken"]) for c in d["champions"] if c["id"] in ids)
    banked = next(int(t["submittedGems"]) for t in d["teams"] if t["team"] == team)
    return camp, champ, dmg, first, banked


for dir_ in sys.argv[1:]:
    tot = [0, 0, 0, 0, 0.0, 0, 0]
    rows = []
    for f in sorted(glob.glob(os.path.join(dir_, "*.json"))):
        d = json.load(open(f))
        us = "teamA" if f.endswith("-a.json") else "teamB"
        them = "teamB" if us == "teamA" else "teamA"
        a, b = side(d, us), side(d, them)
        win = d["match"]["outcome"] == us
        rows.append((os.path.basename(f)[:-5], win, a, b))
        tot[0] += a[0]; tot[1] += b[0]; tot[2] += a[1]; tot[3] += b[1]; tot[4] += a[2]; tot[5] += win; tot[6] += 1
    print("== %s: %d-%d, camp deaths %d vs %d, champion deaths %d vs %d, camp damage taken %.0f" % (
        os.path.basename(dir_.rstrip("/\\")), tot[5], tot[6] - tot[5], tot[0], tot[1], tot[2], tot[3], tot[4]))
    if "-v" in os.environ.get("CAMPS", ""):
        for n, w, a, b in rows:
            print("  %-26s %s campdeaths %2d/%2d first %4s dmg %6.0f banked %4d-%4d" % (n, "W" if w else "L", a[0], b[0], a[3], a[2], a[4], b[4]))
