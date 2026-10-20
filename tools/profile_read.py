"""Read a profiling copy's PROF lines: python tools/profile_read.py DIR [MIN_TOTAL]

For every turn (or only turns whose total is at least MIN_TOTAL), the mean inclusive fuel per turn of each function,
largest first, plus the branch the turn took (from the decision log line of the same champion and round).
"""
import collections
import glob
import os
import re
import sys
import zipfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import logparse

PROF = re.compile(r"^PROF r(\d+) c(\d+) (\d+)(.*)$")


def main():
    d0 = sys.argv[1]
    lo = int(sys.argv[2]) if len(sys.argv) > 2 else 0
    fuel = collections.Counter()
    calls = collections.Counter()
    branches = collections.Counter()
    n = 0
    total = 0
    for f in sorted(glob.glob(os.path.join(d0, "*.repl26a"))):
        side = "a" if f.endswith("-a.repl26a") else "b"
        text = zipfile.ZipFile(f).read("team-%s.stdout" % side).decode(errors="replace").replace("\x00", "")
        branch = {(t["round"], t["champ"]): t["branch"] for t in logparse.turns(text)}
        for raw in text.splitlines():
            m = PROF.match(raw.strip())
            if not m:
                continue
            r, c, tot = int(m.group(1)), int(m.group(2)), int(m.group(3))
            if tot < lo or r == 1:
                continue
            n += 1
            total += tot
            branches[branch.get((r, c), "?")] += 1
            for item in m.group(4).split():
                name, rest = item.split("=")
                fu, ca = rest.split("/")
                fuel[name] += int(fu)
                calls[name] += int(ca)
    print("%d turns, mean total %.1fk" % (n, total / max(1, n) / 1000))
    print("branches:", branches.most_common(8))
    for name, fu in fuel.most_common(40):
        print("  %-28s %8.1fk per turn  %6.2f calls per turn" % (name, fu / n / 1000, calls[name] / n))


if __name__ == "__main__":
    main()
