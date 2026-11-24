"""F3 step 1: score the fight sim's logged predictions (FIGHT_SIM 1, debug games) against what happened.

usage: python3 bots/v216-sim/simeval.py DIR [DIR...] [--window 10] [--radius 10]

DIR holds evaluate.py --debug --keep output (<opp>-<map>-<a|b>.repl26a + .json; our bot is team A in -a games, B in
-b), or `cambc run` output (a .repl26a bundle and stats.json; our bot is team A).
Each `sim:` note belongs to the champion line above it. One sample per (game, round): the first note of the round
(usually the leader's shared decision). Outcome: champion deaths of each side within WINDOW rounds after the round,
dying within RADIUS tiles (Chebyshev) of the noting champion.
Reports AUC of: v213 score margin (score - bar), sim fight value, sim fight - flee, sim predicted death edge; against
(a) any death of ours, (b) the exchange (their deaths > ours vs ours > theirs, ties dropped). Plus calibration of the
sim's predicted deaths and, for the old score's FIGHT/RETREAT choice, outcomes by choice.
"""
import glob
import json
import os
import re
import sys
import zipfile

LINE = re.compile(r"^r(\d+) c(\d+) \((\d+),(\d+)\) (\S+):")
SIM = re.compile(r"^\s+- sim: f (\S+) r (\S+) od (\S+) td (\S+) odr (\S+) t (\d+) n (\d+) (\d+) (\d+) near (\d+) \| old (\w+) (\S+) (-?\d+) cur (\d+)(?: \| g4 (\S+) (\S+) (\S+))?")


def auc(scores, labels):
    pos = [s for s, l in zip(scores, labels) if l]
    neg = [s for s, l in zip(scores, labels) if not l]
    if not pos or not neg:
        return float("nan")
    allv = sorted([(s, 1) for s in pos] + [(s, 0) for s in neg])
    # rank-sum with ties averaged
    ranks, i = {}, 0
    rank_sum = 0.0
    while i < len(allv):
        j = i
        while j < len(allv) and allv[j][0] == allv[i][0]:
            j += 1
        r = (i + j + 1) / 2.0
        for k in range(i, j):
            if allv[k][1]:
                rank_sum += r
        i = j
    return (rank_sum - len(pos) * (len(pos) + 1) / 2.0) / (len(pos) * len(neg))


def games(dirs):
    for d in dirs:
        for rep in sorted(glob.glob(os.path.join(d, "*.repl26a"))):
            base = rep[: -len(".repl26a")]
            if os.path.exists(base + ".json"):
                side = "a" if base.endswith("-a") else "b"
                yield rep, base + ".json", side
            elif os.path.exists(os.path.join(d, "stats.json")):
                yield rep, os.path.join(d, "stats.json"), "a"


def samples(rep, stats, side, window, radius):
    us = "team" + side.upper()
    st = json.load(open(stats))
    deaths = []
    for e in st["events"]:
        if e["kind"] == "death":
            for x in e["entries"]:
                if x["context"] == "champion":
                    deaths.append((x["timestamp"]["round"], e["owner"]["team"] == us, x.get("x"), x.get("y")))
    with zipfile.ZipFile(rep) as z:
        text = z.read("team-%s.stdout" % side).decode("utf-8", "replace").replace(chr(0), "")
    last, seen = None, set()
    out = []
    for line in text.splitlines():
        m = LINE.match(line)
        if m:
            last = (int(m.group(1)), int(m.group(3)), int(m.group(4)))
            continue
        s = SIM.match(line)
        if not s or last is None or last[0] in seen:
            continue
        r, x, y = last
        seen.add(r)
        od = td = 0
        for dr, ours, dx, dy in deaths:
            if r < dr <= r + window and dx is not None and max(abs(dx - x), abs(dy - y)) <= radius:
                if ours:
                    od += 1
                else:
                    td += 1
        f, rr, pod, ptd = (float(s.group(i)) for i in (1, 2, 3, 4))
        out.append(dict(r=r, f=f, fr=f - rr, pod=pod, ptd=ptd, old=float(s.group(12)) - int(s.group(13)),
                        old_fight=s.group(11) == "fight", n_ours=int(s.group(7)), n_theirs=int(s.group(8)),
                        od=od, td=td, game=os.path.basename(rep),
                        g4=(float(s.group(16)) - float(s.group(15))) if s.group(15) else 0.0,
                        g4die=-float(s.group(17)) if s.group(17) else 0.0))
    return out


def main():
    args = sys.argv[1:]
    window, radius = 10, 10
    if "--window" in args:
        i = args.index("--window"); window = int(args[i + 1]); del args[i:i + 2]
    if "--radius" in args:
        i = args.index("--radius"); radius = int(args[i + 1]); del args[i:i + 2]
    rows = []
    for rep, stats, side in games(args):
        rows += samples(rep, stats, side, window, radius)
    print("samples %d (one per game round with a sim note), games %d, window %d, radius %d"
          % (len(rows), len({r["game"] for r in rows}), window, radius))
    if not rows:
        return
    died = [r["od"] > 0 for r in rows]
    print("rounds followed by a death of ours: %d, of theirs: %d" % (sum(died), sum(r["td"] > 0 for r in rows)))
    preds = [("v213 margin (score-bar)", "old"), ("sim fight", "f"), ("sim fight-flee", "fr"),
             ("sim death edge (td-od)", None), ("g4 p_won-p_lost", "g4"), ("g4 -p_die", "g4die")]
    print("\nAUC (higher = predicts the good outcome; 0.5 = none)")
    print("%-26s %14s %14s" % ("predictor", "no death ours", "won exchange"))
    ex = [r for r in rows if r["td"] != r["od"]]
    for name, key in preds:
        val = (lambda r: r["ptd"] - r["pod"]) if key is None else (lambda r, k=key: r[k])
        a1 = auc([val(r) for r in rows], [not d for d in died])
        a2 = auc([val(r) for r in ex], [r["td"] > r["od"] for r in ex])
        print("%-26s %14.3f %14.3f" % (name, a1, a2))
    print("(exchange samples: %d)" % len(ex))
    print("\ncalibration: sim predicted our deaths (fighting) vs actual deaths of ours in the window")
    for lo, hi in ((0, 0.01), (0.01, 0.5), (0.5, 1.01), (1.01, 99)):
        b = [r for r in rows if lo <= r["pod"] < hi]
        if b:
            print("  pred od [%.2f,%.2f): n %5d  actual ours %.3f  theirs %.3f  pred theirs %.2f"
                  % (lo, hi, len(b), sum(r["od"] for r in b) / len(b), sum(r["td"] for r in b) / len(b),
                     sum(r["ptd"] for r in b) / len(b)))
    print("\nby choice (v213 / sim sign of fight-flee):")
    for name, pick in (("v213 fight", lambda r: r["old_fight"]), ("v213 retreat", lambda r: not r["old_fight"]),
                       ("sim fight", lambda r: r["fr"] >= 0), ("sim retreat", lambda r: r["fr"] < 0),
                       ("both fight", lambda r: r["old_fight"] and r["fr"] >= 0),
                       ("v213 fight, sim retreat", lambda r: r["old_fight"] and r["fr"] < 0),
                       ("v213 retreat, sim fight", lambda r: not r["old_fight"] and r["fr"] >= 0)):
        b = [r for r in rows if pick(r)]
        if b:
            print("  %-26s n %5d  ours died %.3f  theirs died %.3f" % (
                name, len(b), sum(r["od"] for r in b) / len(b), sum(r["td"] for r in b) / len(b)))


if __name__ == "__main__":
    main()
