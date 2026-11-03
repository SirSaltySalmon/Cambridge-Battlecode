"""v150: each variant against the seed control, seed by seed (run from the repo root).

  python bots/v150/compare_seeds.py ROOT CONTROL VARIANT [VARIANT ...]

ROOT holds <bot>, <bot>-s1, <bot>-s2 result dirs. CONTROL seed 0 is the base itself (a mirror: equal gems), so its dir
may be missing. Prints, per map set, our banked gems vs theirs as a % (ours / theirs - 1) per seed and the 3-seed
total, minus the control's on the same seed and set, plus wins.
"""
import os
import sys

sys.path.insert(0, os.path.dirname(__file__))
from analyze_maps import FACTS, SETS, game  # noqa: E402
import glob  # noqa: E402


def load(d):
    out = []
    for f in sorted(glob.glob(os.path.join(d, "*.json"))):
        m = os.path.basename(f)[: -len("-a.json")]
        if m in FACTS:
            out.append((m, game(f)))
    return out


def totals(games, keep):
    us = sum(t["gems"] for m, t in games if keep(m))
    them = sum(t["ogems"] for m, t in games if keep(m))
    wins = sum(t["w"] for m, t in games if keep(m))
    n = sum(1 for m, t in games if keep(m))
    return us, them, wins, n


def main():
    root, control = sys.argv[1], sys.argv[2]
    seeds = ["", "-s1", "-s2"]
    ctl = [load(os.path.join(root, control + s)) if os.path.isdir(os.path.join(root, control + s)) else None for s in seeds]
    names = [n for n, _ in SETS]
    print("%-12s " % "variant" + " ".join("%-22s" % n for n in names))
    for v in sys.argv[3:]:
        runs = [load(os.path.join(root, v + s)) for s in seeds]
        cells = []
        for name, keep in SETS:
            dv = []
            tu = tt = cu = ct = w = n = 0
            for i, r in enumerate(runs):
                us, them, wins, k = totals(r, keep)
                if ctl[i] is None:
                    cus, cthem = 1, 1  # mirror: equal gems
                else:
                    cus, cthem, _, _ = totals(ctl[i], keep)
                dv.append(100.0 * (us / max(1, them) - cus / max(1, cthem)))
                tu += us; tt += them; w += wins; n += k
            cells.append("%+5.1f [%s] %3d/%-3d" % (sum(dv) / len(dv), " ".join("%+.0f" % x for x in dv), w, n))
        print("%-12s " % v + " ".join("%-22s" % c for c in cells))


if __name__ == "__main__":
    main()
