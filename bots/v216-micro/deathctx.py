"""F5 step 0: where and how one of our champions dies to enemy champions, from debug bench replays.

usage: tuner/work/replay-venv/bin/python bots/v216-micro/deathctx.py DIR [DIR...] [--champ "Vesper Vale"] [--out FILE]

DIR = tuner/evaluate.py --debug --keep output (needs the <tag>.rounds.json caches fightstats.py writes, or the venv to
build them). For every death of CHAMP (ours) to a champion it reads:
  - CHAMP's branch from the debug log in the K rounds before (the log's c<id> line),
  - health share at death-5 / -3 / -1 (replay snapshots),
  - at death-1: Chebyshev distance to the nearest living enemy, the nearest enemy melee, enemies within 3 and 4,
    allies within 3 and 4, whether she shot (log outcome has 'attack') or moved in the last 3 rounds.
Then tables: branch family at death-1, health band at death-5, nearest-melee distance, allies near.
"""
import collections
import glob
import json
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "tools", "fights"))
import fightstats as fs  # noqa: E402

MELEE = {"Ryth", "Vendetta", "Issac", "Marcella", "Ren", "Varg", "Heart of Ruin"}
K = 5


def branch_rows(replay, side):
    import zipfile
    try:
        with zipfile.ZipFile(replay) as z:
            text = z.read("team-%s.stdout" % side).decode("utf-8", "replace").replace(chr(0), " ")
    except KeyError:
        return None
    by = collections.defaultdict(dict)
    for line in text.splitlines():
        m = fs.LINE.match(line)
        if m:
            by[int(m.group(1))][int(m.group(2))] = (m.group(5), m.group(7) or "", m.group(6))
    return by


def one(replay, stats_path, side, champ):
    us = 0 if side == "a" else 1
    us_name = "team" + ("A" if us == 0 else "B")
    st = json.load(open(stats_path))
    snap = {r["r"]: r["champs"] for r in fs.decode(replay)}
    by = branch_rows(replay, side)
    cid = fs.CHAMPS.index(champ)
    out = []
    for e in st["events"]:
        if e["kind"] != "death" or e["owner"]["team"] != us_name or e["owner"]["name"] != champ:
            continue
        for x in e["entries"]:
            if x["context"] != "champion":
                continue
            r = x["timestamp"]["round"]
            row = {"game": os.path.basename(replay)[: -len(".repl26a")], "r": r}
            me = lambda rr: next((c for c in snap.get(rr, []) if c["team"] == us and c["type"] == champ), None)
            for k in (5, 3, 1):
                c = me(r - k)
                row["hp%d" % k] = c["hp"] / c["max"] if c and c["max"] else None
            c = me(r - 1)
            if c:
                p = (c["x"], c["y"])
                foes = [d for d in snap.get(r - 1, []) if d["team"] != us and d["alive"]]
                pals = [d for d in snap.get(r - 1, []) if d["team"] == us and d["alive"] and d["type"] != champ]
                dist = [fs.cheb(p, (d["x"], d["y"])) for d in foes]
                mdist = [fs.cheb(p, (d["x"], d["y"])) for d in foes if d["type"] in MELEE]
                adist = [fs.cheb(p, (d["x"], d["y"])) for d in pals]
                row.update(near=min(dist) if dist else None, melee=min(mdist) if mdist else None,
                           foes3=sum(1 for v in dist if v <= 3), foes4=sum(1 for v in dist if v <= 4),
                           pals3=sum(1 for v in adist if v <= 3), pals4=sum(1 for v in adist if v <= 4),
                           emb_me=c["emblems"])
            if by is not None:
                seq = [by.get(rr, {}).get(cid) for rr in range(r - K, r)]
                row["branches"] = [s[0] if s else None for s in seq]
                row["outcomes"] = [s[1] if s else None for s in seq]
            out.append(row)
    return out


def band(v, cuts):
    if v is None:
        return "?"
    for lo, name in cuts:
        if v >= lo:
            return name
    return cuts[-1][1]


def main():
    args = sys.argv[1:]
    champ, out_path = "Vesper Vale", None
    if "--champ" in args:
        i = args.index("--champ"); champ = args[i + 1]; del args[i:i + 2]
    if "--out" in args:
        i = args.index("--out"); out_path = args[i + 1]; del args[i:i + 2]
    rows = []
    games = 0
    for d in args:
        for replay in sorted(glob.glob(os.path.join(d, "*.repl26a"))):
            tag = replay[: -len(".repl26a")]
            stats = tag + ".json"
            if not os.path.exists(stats):
                continue
            games += 1
            rows += one(replay, stats, tag[-1], champ)
    L = ["%s deaths to champions: %d in %d games (%s)" % (champ, len(rows), games, ", ".join(args))]

    def table(title, key):
        cnt = collections.Counter(key(r) for r in rows)
        L.append(title)
        for k, v in sorted(cnt.items(), key=lambda kv: -kv[1]):
            L.append("  %-40s %4d  %4.0f%%" % (k, v, 100.0 * v / max(1, len(rows))))

    fam = lambda b: (fs.family(b) or "busy") if b else "none"
    table("branch family at death-1:", lambda r: fam((r.get("branches") or [None])[-1]))
    table("branch at death-1 (top):", lambda r: ((r.get("branches") or [None])[-1] or "none"))
    table("any engage family in last 5 rounds -> last family:",
          lambda r: ("engaged" if any(b and fam(b) == "engage" for b in r.get("branches", [])) else "never engaged")
          + " / " + fam((r.get("branches") or [None])[-1]))
    hp = [(0.8, ">=80%"), (0.6, "60-80%"), (0.35, "35-60%"), (0.0, "<35%")]
    table("health at death-5:", lambda r: band(r.get("hp5"), hp))
    table("health at death-3:", lambda r: band(r.get("hp3"), hp))
    table("nearest enemy melee at death-1 (tiles):", lambda r: "none" if r.get("melee") is None else str(min(r["melee"], 9)))
    table("nearest enemy at death-1:", lambda r: "?" if r.get("near") is None else str(min(r["near"], 9)))
    table("enemies within 4 at death-1:", lambda r: str(r.get("foes4")))
    table("allies within 3 at death-1:", lambda r: str(r.get("pals3")))
    table("shot in last 3 rounds / moved in last 3:", lambda r: "shot=%s moved=%s" % (
        any("attack" in (o or "") for o in r.get("outcomes", [])[-3:]),
        any(("move" in (o or "") or "dash" in (o or "")) for o in r.get("outcomes", [])[-3:])))
    # Deaths from high health with an enemy melee close and no ally near: the brief's 'positioning failure'.
    pos = [r for r in rows if (r.get("hp5") or 0) >= 0.6 and r.get("melee") is not None and r["melee"] <= 2]
    L.append("from >=60%% health at death-5 with an enemy melee <=2 at death-1: %d (%.0f%%), of them allies within 3 = 0: %d"
             % (len(pos), 100.0 * len(pos) / max(1, len(rows)), sum(1 for r in pos if r.get("pals3") == 0)))
    L.append("examples (game r branches hp5/hp1 melee foes4 pals3):")
    for r in rows[:40]:
        L.append("  %s r%d %s %.2f/%.2f m%s f%s a%s" % (r["game"], r["r"], "|".join((b or "-") for b in r.get("branches", [])),
                                                    r.get("hp5") or -1, r.get("hp1") or -1, r.get("melee"), r.get("foes4"), r.get("pals3")))
    text = "\n".join(L)
    print(text)
    if out_path:
        open(out_path, "w").write(text + "\n")


if __name__ == "__main__":
    main()
