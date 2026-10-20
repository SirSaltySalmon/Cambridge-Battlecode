"""Fuel per champion turn of the bot under test across bench folders (<map>-a: it is team A, <map>-b: team B).

usage: python fuel_bench.py DIR[,DIR...] [DIR[,DIR...] ...]   (one summary line per comma-separated group)
A turn of exactly 100000 ran out of fuel; entries over 100000 (draft and first turns, bigger budgets) are left out.
"""
import glob
import os
import sys
import zipfile

from fuel_compare import LIMIT, turn_fuel


def main():
    for group in sys.argv[1:]:
        used, games, worst = [], 0, []
        for d in group.split(","):
            for f in sorted(glob.glob(os.path.join(d, "*.repl26a"))):
                side = "a" if f.endswith("-a.repl26a") else "b"
                u = [x for x in turn_fuel(zipfile.ZipFile(f).read("team-%s.fuel" % side)) if x <= LIMIT]
                used.extend(u)
                games += 1
                out = sum(1 for x in u if x == LIMIT)
                if out:
                    worst.append((out, os.path.basename(f)[:-8]))
        used.sort()
        n = max(1, len(used))
        print("%-40s games %3d turns %7d mean %6.0f median %6d p95 %6d p99 %6d >=90k %4d out-of-fuel %4d" % (
            group[-40:], games, len(used), sum(used) / n, used[n // 2], used[int(.95 * n)], used[int(.99 * n)],
            sum(1 for x in used if x >= 90000), sum(1 for x in used if x == LIMIT)))
        worst.sort(reverse=True)
        if worst:
            print("    most out-of-fuel turns: " + ", ".join("%s %d" % (m, c) for c, m in worst[:6]))


if __name__ == "__main__":
    main()
