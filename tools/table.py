"""Results table from results/results-BOT-vs-OPP-KIND.txt summary lines: python tools/table.py BASE BOT [BOT...]

One row per bot: record and gem margin against each opponent, the total, and the win difference from BASE against the
same opponents (BASE vs itself counts as an even 17-17 per 34 games).
"""
import glob
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
LINE = re.compile(r"^(\S+): (\d+)-(\d+), gems (\d+) vs (\d+) \(([+-][\d.]+)%\).*fuel mean ([\d.]+)k p99 ([\d.]+)k, turns >=100k (\d+)")
OPPS = [("v82", "normal"), ("v82", "unlimited"), ("spar-brawler", "normal"), ("v80-5", "normal"), ("spar-locker", "normal")]


def read(bot, opp, kind):
    p = os.path.join(ROOT, "results", "results-%s-vs-%s-%s.txt" % (bot, opp, kind))
    if not os.path.exists(p):
        return None
    for line in open(p):
        m = LINE.match(line.strip())
        if m:
            return int(m.group(2)), int(m.group(3)), float(m.group(6)), float(m.group(7)), int(m.group(9))
    return None


def main():
    base = sys.argv[1]
    bots = sys.argv[2:]
    head = "%-12s" % "bot" + "".join("%-22s" % ("%s %s" % (o.replace("spar-", ""), k[:3])) for o, k in OPPS) + "total   vs base"
    print(head)
    for bot in [base] + bots:
        cells, w, l, dw = [], 0, 0, 0
        for o, k in OPPS:
            r = read(bot, o, k) if not (bot == base and o == base) else (17, 17, 0.0, 0, 0)
            b = read(base, o, k) if o != base else (17, 17, 0.0, 0, 0)
            if r is None:
                cells.append("%-22s" % "-")
                continue
            w += r[0]
            l += r[1]
            if b:
                dw += r[0] - b[0]
            cells.append("%-22s" % ("%d-%d %+.1f%%" % (r[0], r[1], r[2])))
        print("%-12s" % bot + "".join(cells) + "%d-%d  %+d" % (w, l, dw))


if __name__ == "__main__":
    main()
